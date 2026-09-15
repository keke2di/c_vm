#include <string.h>
#include "unicode.h"
#include "vm_internal.h"

#define CASE_UP 0
#define CASE_LOW 1
#define CASE_FOLD 2
#define CASE_TITLE 3
#define CASE_CAPITALIZE 4
#define CASE_SWAP 5

#define SIGMA 0x03A3u
#define SIGMA_FINAL 0x03C2u
#define SIGMA_SMALL 0x03C3u

static void append_cp(StrBuf *out, uint32_t cp) {
    char buf[4];
    size_t n = utf8_encode(cp, buf);
    strbuf_append(out, buf, n);
}

static void append_mapped(StrBuf *out, const uint32_t *cps, size_t n) {
    for (size_t i = 0; i < n; i++) {
        append_cp(out, cps[i]);
    }
}

static void emit_lower(StrBuf *out, const char *s, size_t len, size_t next,
                       uint32_t cp, int prev_cased) {
    if (cp == SIGMA) {
        int next_cased = 0;

        if (next < len) {
            size_t peek = next;
            next_cased = uni_is_cased(utf8_decode(s, len, &peek));
        }

        append_cp(out, (prev_cased && !next_cased) ? SIGMA_FINAL : SIGMA_SMALL);
        return;
    }

    uint32_t mapped[UNI_CASE_MAX];
    append_mapped(out, mapped, uni_to_lower(cp, mapped));
}

static Value *convert_case(VM *vm, const Value *self, int op) {
    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    StrBuf out = { NULL, 0, 0, 0 };
    uint32_t mapped[UNI_CASE_MAX];
    size_t i = 0;
    int prev_cased = 0;
    int first = 1;

    while (i < len) {
        size_t next = i;
        uint32_t cp = utf8_decode(s, len, &next);

        switch (op) {
            case CASE_UP:
                append_mapped(&out, mapped, uni_to_upper(cp, mapped));
                break;

            case CASE_FOLD:
                append_mapped(&out, mapped, uni_to_casefold(cp, mapped));
                break;

            case CASE_TITLE:
                if (prev_cased) {
                    emit_lower(&out, s, len, next, cp, prev_cased);
                } else {
                    append_mapped(&out, mapped, uni_to_title(cp, mapped));
                }
                break;

            case CASE_CAPITALIZE:
                if (first) {
                    append_mapped(&out, mapped, uni_to_title(cp, mapped));
                } else {
                    emit_lower(&out, s, len, next, cp, prev_cased);
                }
                break;

            case CASE_SWAP:
                if (uni_is_upper(cp)) {
                    emit_lower(&out, s, len, next, cp, prev_cased);
                } else if (uni_is_lower(cp)) {
                    append_mapped(&out, mapped, uni_to_upper(cp, mapped));
                } else {
                    append_cp(&out, cp);
                }
                break;

            default:
                emit_lower(&out, s, len, next, cp, prev_cased);
                break;
        }

        prev_cased = uni_is_cased(cp);
        first = 0;
        i = next;
    }

    return strbuf_finish_string(vm, &out);
}

static Value *case_method(VM *vm, Value *self, uint32_t nargs, const Value *kwnames, int op) {
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    return convert_case(vm, self, op);
}

Value *str_upper(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return case_method(vm, self, nargs, kwnames, CASE_UP);
}

Value *str_lower(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return case_method(vm, self, nargs, kwnames, CASE_LOW);
}

Value *str_casefold(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return case_method(vm, self, nargs, kwnames, CASE_FOLD);
}

Value *str_title(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return case_method(vm, self, nargs, kwnames, CASE_TITLE);
}

Value *str_capitalize(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return case_method(vm, self, nargs, kwnames, CASE_CAPITALIZE);
}

Value *str_swapcase(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return case_method(vm, self, nargs, kwnames, CASE_SWAP);
}

#define PRED_ALPHA 0
#define PRED_DECIMAL 1
#define PRED_DIGIT 2
#define PRED_NUMERIC 3
#define PRED_ALNUM 4
#define PRED_SPACE 5
#define PRED_LOWER 6
#define PRED_UPPER 7
#define PRED_TITLE 8
#define PRED_PRINTABLE 9
#define PRED_IDENTIFIER 10

static int char_matches(uint32_t cp, int kind) {
    switch (kind) {
        case PRED_ALPHA: return uni_is_alpha(cp);
        case PRED_DECIMAL: return uni_is_decimal(cp);
        case PRED_DIGIT: return uni_is_digit(cp);
        case PRED_NUMERIC: return uni_is_numeric(cp);
        case PRED_ALNUM:
            return uni_is_alpha(cp) || uni_is_decimal(cp) || uni_is_digit(cp) || uni_is_numeric(cp);
        case PRED_SPACE: return unicode_is_space(cp);
        default: return uni_is_printable(cp);
    }
}

static int check_predicate(const char *s, size_t len, int kind) {
    if (kind == PRED_PRINTABLE && len == 0) return 1;
    if (len == 0) return 0;

    size_t i = 0;
    int seen_cased = 0;
    int prev_cased = 0;
    int first = 1;

    while (i < len) {
        size_t next = i;
        uint32_t cp = utf8_decode(s, len, &next);

        if (kind == PRED_IDENTIFIER) {
            if (first ? !uni_is_id_start(cp) : !uni_is_id_continue(cp)) return 0;
        } else if (kind == PRED_LOWER || kind == PRED_UPPER) {
            if (uni_is_cased(cp)) {
                int ok = kind == PRED_LOWER ? uni_is_lower(cp) : uni_is_upper(cp);
                if (!ok) return 0;
                seen_cased = 1;
            }
        } else if (kind == PRED_TITLE) {
            if (uni_is_cased(cp)) {
                int ok = prev_cased ? uni_is_lower(cp) : (uni_is_upper(cp) || uni_is_title(cp));
                if (!ok) return 0;
                seen_cased = 1;
            }
            prev_cased = uni_is_cased(cp);
        } else if (!char_matches(cp, kind)) {
            return 0;
        }

        first = 0;
        i = next;
    }

    if (kind == PRED_LOWER || kind == PRED_UPPER || kind == PRED_TITLE) return seen_cased;
    return 1;
}

static Value *predicate_method(VM *vm, Value *self, uint32_t nargs, const Value *kwnames, int kind) {
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    return value_bool(check_predicate(self->data.str.data, self->data.str.len, kind));
}

#define DEFINE_PREDICATE(name, kind)                                                       \
    Value *name(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) { \
        (void)args;                                                                        \
        return predicate_method(vm, self, nargs, kwnames, kind);                           \
    }

DEFINE_PREDICATE(str_isalpha, PRED_ALPHA)
DEFINE_PREDICATE(str_isdecimal, PRED_DECIMAL)
DEFINE_PREDICATE(str_isdigit, PRED_DIGIT)
DEFINE_PREDICATE(str_isnumeric, PRED_NUMERIC)
DEFINE_PREDICATE(str_isalnum, PRED_ALNUM)
DEFINE_PREDICATE(str_isspace, PRED_SPACE)
DEFINE_PREDICATE(str_islower, PRED_LOWER)
DEFINE_PREDICATE(str_isupper, PRED_UPPER)
DEFINE_PREDICATE(str_istitle, PRED_TITLE)
DEFINE_PREDICATE(str_isprintable, PRED_PRINTABLE)
DEFINE_PREDICATE(str_isidentifier, PRED_IDENTIFIER)

Value *str_isascii(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)s[i] >= 0x80) return value_bool(0);
    }

    return value_bool(1);
}

#define PAD_LEFT 0
#define PAD_RIGHT 1
#define PAD_CENTER 2

static Value *pad_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                         const Value *kwnames, int mode) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (args[0]->tag != TAG_INT && args[0]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    uint32_t fill = ' ';

    if (nargs == 2) {
        const char *text;
        size_t flen;
        if (text_value(vm, args[1], &text, &flen) != 0) return NULL;
        size_t pos = 0;
        if (flen == 0 || utf8_length(text, flen) != 1) return vm_fail(vm, VM_ERR_TYPE);
        fill = utf8_decode(text, flen, &pos);
    }

    int64_t width = args[0]->data.int_val;
    int64_t cplen = (int64_t)utf8_length(s, len);

    if (width <= cplen) return value_new_string_len(s, len);

    int64_t pad = width - cplen;
    int64_t left;

    if (mode == PAD_LEFT) {
        left = 0;
    } else if (mode == PAD_RIGHT) {
        left = pad;
    } else {
        left = pad / 2 + (pad & width & 1);
    }

    StrBuf out = { NULL, 0, 0, 0 };

    for (int64_t i = 0; i < left; i++) append_cp(&out, fill);
    strbuf_append(&out, s, len);
    for (int64_t i = left; i < pad; i++) append_cp(&out, fill);

    return strbuf_finish_string(vm, &out);
}

Value *str_ljust(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return pad_common(vm, self, args, nargs, kwnames, PAD_LEFT);
}

Value *str_rjust(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return pad_common(vm, self, args, nargs, kwnames, PAD_RIGHT);
}

Value *str_center(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return pad_common(vm, self, args, nargs, kwnames, PAD_CENTER);
}

Value *str_zfill(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (args[0]->tag != TAG_INT && args[0]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    int64_t width = args[0]->data.int_val;
    int64_t cplen = (int64_t)utf8_length(s, len);

    if (width <= cplen) return value_new_string_len(s, len);

    size_t sign = (len > 0 && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
    StrBuf out = { NULL, 0, 0, 0 };

    if (sign) strbuf_append(&out, s, 1);
    for (int64_t i = 0; i < width - cplen; i++) append_cp(&out, '0');
    strbuf_append(&out, s + sign, len - sign);

    return strbuf_finish_string(vm, &out);
}

static const char *const TABSIZE_PARAMS[] = { "tabsize" };

Value *str_expandtabs(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[1];
    if (bind_args(vm, args, nargs, kwnames, TABSIZE_PARAMS, 1, 1, 0, slots) != 0) return NULL;

    int64_t tabsize = 8;

    if (slots[0]) {
        if (slots[0]->tag != TAG_INT && slots[0]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);
        tabsize = slots[0]->data.int_val;
    }

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    StrBuf out = { NULL, 0, 0, 0 };
    size_t i = 0;
    int64_t column = 0;

    while (i < len) {
        size_t next = i;
        uint32_t cp = utf8_decode(s, len, &next);

        if (cp == '\t') {
            if (tabsize > 0) {
                int64_t spaces = tabsize - (column % tabsize);
                for (int64_t k = 0; k < spaces; k++) append_cp(&out, ' ');
                column += spaces;
            }
        } else {
            strbuf_append(&out, s + i, next - i);
            column = (cp == '\n' || cp == '\r') ? 0 : column + 1;
        }

        i = next;
    }

    return strbuf_finish_string(vm, &out);
}

static Value *remove_affix(VM *vm, Value *self, Value **args, uint32_t nargs,
                           const Value *kwnames, int suffix) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    const char *affix;
    size_t alen;
    if (text_value(vm, args[0], &affix, &alen) != 0) return NULL;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    if (alen > 0 && alen <= len) {
        size_t at = suffix ? len - alen : 0;

        if (memcmp(s + at, affix, alen) == 0) {
            return suffix ? value_new_string_len(s, len - alen)
                          : value_new_string_len(s + alen, len - alen);
        }
    }

    return value_new_string_len(s, len);
}

Value *str_translate(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (args[0]->tag != TAG_DICT) return vm_fail(vm, VM_ERR_TYPE);

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    StrBuf out = { NULL, 0, 0, 0 };
    size_t i = 0;

    while (i < len) {
        size_t next = i;
        uint32_t cp = utf8_decode(s, len, &next);

        Value key;
        key.tag = TAG_INT;
        key.refcount = UINT32_MAX;
        key.data.int_val = (int64_t)cp;

        Value *found = value_dict_get(args[0], &key);

        if (!found) {
            strbuf_append(&out, s + i, next - i);
        } else if (found->tag == TAG_INT || found->tag == TAG_BOOL) {
            int64_t mapped = found->data.int_val;

            if (mapped < 0 || mapped > 0x10FFFF) {
                strbuf_free(&out);
                return vm_fail(vm, VM_ERR_VALUE);
            }

            append_cp(&out, (uint32_t)mapped);
        } else if (found->tag == TAG_STRING) {
            strbuf_append(&out, found->data.str.data, found->data.str.len);
        } else if (found->tag != TAG_NONE) {
            strbuf_free(&out);
            return vm_fail(vm, VM_ERR_TYPE);
        }

        i = next;
    }

    return strbuf_finish_string(vm, &out);
}

Value *str_removeprefix(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return remove_affix(vm, self, args, nargs, kwnames, 0);
}

Value *str_removesuffix(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return remove_affix(vm, self, args, nargs, kwnames, 1);
}
