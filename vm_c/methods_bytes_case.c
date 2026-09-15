#include <string.h>
#include "vm_internal.h"

#define BCASE_UP 0
#define BCASE_LOW 1
#define BCASE_TITLE 2
#define BCASE_CAPITALIZE 3
#define BCASE_SWAP 4

static int ascii_lower_p(unsigned char c) { return c >= 'a' && c <= 'z'; }
static int ascii_upper_p(unsigned char c) { return c >= 'A' && c <= 'Z'; }
static int ascii_alpha_p(unsigned char c) { return ascii_lower_p(c) || ascii_upper_p(c); }
static int ascii_digit_p(unsigned char c) { return c >= '0' && c <= '9'; }

static unsigned char to_lower(unsigned char c) {
    return ascii_upper_p(c) ? (unsigned char)(c + 32) : c;
}

static unsigned char to_upper(unsigned char c) {
    return ascii_lower_p(c) ? (unsigned char)(c - 32) : c;
}

static Value *convert_case(VM *vm, Value *self, uint32_t nargs, const Value *kwnames, int op) {
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;

    const unsigned char *s = self->data.bytes.data;
    size_t len = self->data.bytes.len;

    StrBuf out = { NULL, 0, 0, 0 };
    int prev_cased = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = s[i];
        unsigned char mapped;

        switch (op) {
            case BCASE_UP: mapped = to_upper(c); break;
            case BCASE_LOW: mapped = to_lower(c); break;
            case BCASE_TITLE: mapped = prev_cased ? to_lower(c) : to_upper(c); break;
            case BCASE_CAPITALIZE: mapped = i == 0 ? to_upper(c) : to_lower(c); break;
            default: mapped = ascii_upper_p(c) ? to_lower(c) : to_upper(c); break;
        }

        char byte = (char)mapped;
        strbuf_append(&out, &byte, 1);
        prev_cased = ascii_alpha_p(c);
    }

    return strbuf_finish_bytes(vm, &out);
}

Value *bytes_upper(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return convert_case(vm, self, nargs, kwnames, BCASE_UP);
}

Value *bytes_lower(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return convert_case(vm, self, nargs, kwnames, BCASE_LOW);
}

Value *bytes_title(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return convert_case(vm, self, nargs, kwnames, BCASE_TITLE);
}

Value *bytes_capitalize(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return convert_case(vm, self, nargs, kwnames, BCASE_CAPITALIZE);
}

Value *bytes_swapcase(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return convert_case(vm, self, nargs, kwnames, BCASE_SWAP);
}

#define BPRED_ALPHA 0
#define BPRED_DIGIT 1
#define BPRED_ALNUM 2
#define BPRED_SPACE 3
#define BPRED_LOWER 4
#define BPRED_UPPER 5
#define BPRED_TITLE 6
#define BPRED_ASCII 7

static int check_predicate(const unsigned char *s, size_t len, int kind) {
    if (kind == BPRED_ASCII) {
        for (size_t i = 0; i < len; i++) {
            if (s[i] >= 0x80) return 0;
        }
        return 1;
    }

    if (len == 0) return 0;

    int seen_cased = 0;
    int prev_cased = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = s[i];

        switch (kind) {
            case BPRED_ALPHA:
                if (!ascii_alpha_p(c)) return 0;
                break;

            case BPRED_DIGIT:
                if (!ascii_digit_p(c)) return 0;
                break;

            case BPRED_ALNUM:
                if (!ascii_alpha_p(c) && !ascii_digit_p(c)) return 0;
                break;

            case BPRED_SPACE:
                if (!ascii_is_space(c)) return 0;
                break;

            case BPRED_LOWER:
                if (ascii_upper_p(c)) return 0;
                if (ascii_lower_p(c)) seen_cased = 1;
                break;

            case BPRED_UPPER:
                if (ascii_lower_p(c)) return 0;
                if (ascii_upper_p(c)) seen_cased = 1;
                break;

            default:
                if (ascii_alpha_p(c)) {
                    if (prev_cased ? !ascii_lower_p(c) : !ascii_upper_p(c)) return 0;
                    seen_cased = 1;
                }
                prev_cased = ascii_alpha_p(c);
                break;
        }
    }

    if (kind == BPRED_LOWER || kind == BPRED_UPPER || kind == BPRED_TITLE) return seen_cased;
    return 1;
}

static Value *predicate_method(VM *vm, Value *self, uint32_t nargs, const Value *kwnames, int kind) {
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    return value_bool(check_predicate(self->data.bytes.data, self->data.bytes.len, kind));
}

#define DEFINE_BPREDICATE(name, kind)                                                      \
    Value *name(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) { \
        (void)args;                                                                        \
        return predicate_method(vm, self, nargs, kwnames, kind);                           \
    }

DEFINE_BPREDICATE(bytes_isalpha, BPRED_ALPHA)
DEFINE_BPREDICATE(bytes_isdigit, BPRED_DIGIT)
DEFINE_BPREDICATE(bytes_isalnum, BPRED_ALNUM)
DEFINE_BPREDICATE(bytes_isspace, BPRED_SPACE)
DEFINE_BPREDICATE(bytes_islower, BPRED_LOWER)
DEFINE_BPREDICATE(bytes_isupper, BPRED_UPPER)
DEFINE_BPREDICATE(bytes_istitle, BPRED_TITLE)
DEFINE_BPREDICATE(bytes_isascii, BPRED_ASCII)

#define BPAD_LEFT 0
#define BPAD_RIGHT 1
#define BPAD_CENTER 2

static Value *pad_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                         const Value *kwnames, int mode) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (args[0]->tag != TAG_INT && args[0]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
    char fill = ' ';

    if (nargs == 2) {
        const char *text;
        size_t flen;
        if (bytes_value(vm, args[1], &text, &flen) != 0) return NULL;
        if (flen != 1) return vm_fail(vm, VM_ERR_TYPE);
        fill = text[0];
    }

    int64_t width = args[0]->data.int_val;

    if (width <= (int64_t)len) return value_new_bytes((const unsigned char *)s, len);

    int64_t pad = width - (int64_t)len;
    int64_t left;

    if (mode == BPAD_LEFT) {
        left = 0;
    } else if (mode == BPAD_RIGHT) {
        left = pad;
    } else {
        left = pad / 2 + (pad & width & 1);
    }

    StrBuf out = { NULL, 0, 0, 0 };

    for (int64_t i = 0; i < left; i++) strbuf_append(&out, &fill, 1);
    strbuf_append(&out, s, len);
    for (int64_t i = left; i < pad; i++) strbuf_append(&out, &fill, 1);

    return strbuf_finish_bytes(vm, &out);
}

Value *bytes_ljust(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return pad_common(vm, self, args, nargs, kwnames, BPAD_LEFT);
}

Value *bytes_rjust(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return pad_common(vm, self, args, nargs, kwnames, BPAD_RIGHT);
}

Value *bytes_center(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return pad_common(vm, self, args, nargs, kwnames, BPAD_CENTER);
}

Value *bytes_zfill(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (args[0]->tag != TAG_INT && args[0]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
    int64_t width = args[0]->data.int_val;

    if (width <= (int64_t)len) return value_new_bytes((const unsigned char *)s, len);

    size_t sign = (len > 0 && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
    StrBuf out = { NULL, 0, 0, 0 };

    if (sign) strbuf_append(&out, s, 1);
    for (int64_t i = 0; i < width - (int64_t)len; i++) strbuf_append(&out, "0", 1);
    strbuf_append(&out, s + sign, len - sign);

    return strbuf_finish_bytes(vm, &out);
}

static const char *const TABSIZE_PARAMS[] = { "tabsize" };

Value *bytes_expandtabs(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[1];
    if (bind_args(vm, args, nargs, kwnames, TABSIZE_PARAMS, 1, 1, 0, slots) != 0) return NULL;

    int64_t tabsize = 8;

    if (slots[0]) {
        if (slots[0]->tag != TAG_INT && slots[0]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);
        tabsize = slots[0]->data.int_val;
    }

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;

    StrBuf out = { NULL, 0, 0, 0 };
    int64_t column = 0;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];

        if (c == '\t') {
            if (tabsize > 0) {
                int64_t spaces = tabsize - (column % tabsize);
                for (int64_t k = 0; k < spaces; k++) strbuf_append(&out, " ", 1);
                column += spaces;
            }
            continue;
        }

        strbuf_append(&out, &c, 1);
        column = (c == '\n' || c == '\r') ? 0 : column + 1;
    }

    return strbuf_finish_bytes(vm, &out);
}

static Value *remove_affix(VM *vm, Value *self, Value **args, uint32_t nargs,
                           const Value *kwnames, int suffix) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    const char *affix;
    size_t alen;
    if (bytes_value(vm, args[0], &affix, &alen) != 0) return NULL;

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;

    if (alen > 0 && alen <= len) {
        size_t at = suffix ? len - alen : 0;

        if (memcmp(s + at, affix, alen) == 0) {
            const unsigned char *data = (const unsigned char *)(suffix ? s : s + alen);
            return value_new_bytes(data, len - alen);
        }
    }

    return value_new_bytes((const unsigned char *)s, len);
}

static const char *const TRANSLATE_PARAMS[] = { NULL, "delete" };

Value *bytes_translate(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, TRANSLATE_PARAMS, 2, 2, 1, slots) != 0) return NULL;

    const char *table = NULL;

    if (slots[0]->tag != TAG_NONE) {
        size_t tlen;
        if (bytes_value(vm, slots[0], &table, &tlen) != 0) return NULL;
        if (tlen != 256) return vm_fail(vm, VM_ERR_VALUE);
    }

    const char *drop = NULL;
    size_t droplen = 0;

    if (slots[1] && slots[1]->tag != TAG_NONE) {
        if (bytes_value(vm, slots[1], &drop, &droplen) != 0) return NULL;
    }

    const unsigned char *s = self->data.bytes.data;
    size_t len = self->data.bytes.len;

    StrBuf out = { NULL, 0, 0, 0 };

    for (size_t i = 0; i < len; i++) {
        unsigned char c = s[i];

        if (drop && memchr(drop, c, droplen)) continue;

        char mapped = table ? table[c] : (char)c;
        strbuf_append(&out, &mapped, 1);
    }

    return strbuf_finish_bytes(vm, &out);
}

Value *bytes_removeprefix(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return remove_affix(vm, self, args, nargs, kwnames, 0);
}

Value *bytes_removesuffix(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return remove_affix(vm, self, args, nargs, kwnames, 1);
}
