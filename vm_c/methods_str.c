#include <string.h>
#include "vm_internal.h"

static const char *const CODEC_PARAMS[] = { "encoding", "errors" };

static Value *str_encode(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, CODEC_PARAMS, 2, 2, 0, slots) != 0) return NULL;
    return codec_encode_value(vm, self, slots[0], slots[1]);
}

static Value *make_slice(VM *vm, const char *s, size_t begin, size_t end) {
    Value *v = value_new_string_len(s + begin, end - begin);
    return v ? v : vm_fail(vm, VM_ERR_OOM);
}

static Value *find_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                          const Value *kwnames, int reverse, int raising) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    const char *needle;
    size_t nlen;
    if (text_value(vm, args[0], &needle, &nlen) != 0) return NULL;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    size_t begin;
    size_t end;

    int rc = text_bounds(vm, s, len, nargs > 1 ? args[1] : NULL,
                         nargs > 2 ? args[2] : NULL, &begin, &end);
    if (rc < 0) return NULL;

    size_t found = TEXT_NOT_FOUND;

    if (rc == 0) {
        found = reverse ? text_rfind(s, begin, end, needle, nlen)
                        : text_find(s, begin, end, needle, nlen);
    }

    if (found == TEXT_NOT_FOUND) {
        if (raising) return vm_fail(vm, VM_ERR_VALUE);
        return value_new_int(-1);
    }

    return value_new_int((int64_t)utf8_length(s, found));
}

static Value *str_find(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 0, 0);
}

static Value *str_rfind(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 1, 0);
}

static Value *str_index(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 0, 1);
}

static Value *str_rindex(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 1, 1);
}

static Value *str_count(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    const char *needle;
    size_t nlen;
    if (text_value(vm, args[0], &needle, &nlen) != 0) return NULL;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    size_t begin;
    size_t end;

    int rc = text_bounds(vm, s, len, nargs > 1 ? args[1] : NULL,
                         nargs > 2 ? args[2] : NULL, &begin, &end);
    if (rc < 0) return NULL;
    if (rc > 0) return value_new_int(0);

    if (nlen == 0) {
        int64_t span = (int64_t)utf8_length(s, end) - (int64_t)utf8_length(s, begin);
        return value_new_int(span + 1);
    }

    int64_t total = 0;
    size_t at = begin;

    for (;;) {
        size_t found = text_find(s, at, end, needle, nlen);
        if (found == TEXT_NOT_FOUND) break;
        total++;
        at = found + nlen;
    }

    return value_new_int(total);
}

static int affix_match(VM *vm, const char *s, size_t begin, size_t end,
                       const Value *needle, int at_end, int *result) {
    const char *text;
    size_t nlen;

    if (text_value(vm, needle, &text, &nlen) != 0) return -1;

    if (end - begin < nlen) {
        *result = 0;
        return 0;
    }

    size_t at = at_end ? end - nlen : begin;
    *result = memcmp(s + at, text, nlen) == 0;
    return 0;
}

static Value *affix_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                           const Value *kwnames, int at_end) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    size_t begin;
    size_t end;

    int rc = text_bounds(vm, s, len, nargs > 1 ? args[1] : NULL,
                         nargs > 2 ? args[2] : NULL, &begin, &end);
    if (rc < 0) return NULL;
    if (rc > 0) return value_bool(0);

    if (args[0]->tag == TAG_TUPLE) {
        for (uint32_t i = 0; i < args[0]->data.tuple.len; i++) {
            int matched;
            if (affix_match(vm, s, begin, end, args[0]->data.tuple.items[i], at_end, &matched) != 0) {
                return NULL;
            }
            if (matched) return value_bool(1);
        }
        return value_bool(0);
    }

    int matched;
    if (affix_match(vm, s, begin, end, args[0], at_end, &matched) != 0) return NULL;
    return value_bool(matched);
}

static Value *str_startswith(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return affix_common(vm, self, args, nargs, kwnames, 0);
}

static Value *str_endswith(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return affix_common(vm, self, args, nargs, kwnames, 1);
}

static Value *str_replace(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 3) != 0) return NULL;

    const char *needle;
    size_t nlen;
    const char *repl;
    size_t rlen;

    if (text_value(vm, args[0], &needle, &nlen) != 0) return NULL;
    if (text_value(vm, args[1], &repl, &rlen) != 0) return NULL;

    int64_t budget = -1;

    if (nargs == 3) {
        if (args[2]->tag != TAG_INT && args[2]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);
        budget = args[2]->data.int_val;
    }

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    if (budget == 0) return value_new_string_len(s, len);

    StrBuf out = { NULL, 0, 0, 0 };

    if (nlen == 0) {
        size_t i = 0;
        int64_t used = 0;

        while (i < len) {
            if (budget < 0 || used < budget) {
                strbuf_append(&out, repl, rlen);
                used++;
            }
            size_t next = i;
            utf8_decode(s, len, &next);
            strbuf_append(&out, s + i, next - i);
            i = next;
        }

        if (budget < 0 || used < budget) {
            strbuf_append(&out, repl, rlen);
        }

        return strbuf_finish_string(vm, &out);
    }

    size_t at = 0;
    int64_t used = 0;

    while (budget < 0 || used < budget) {
        size_t found = text_find(s, at, len, needle, nlen);
        if (found == TEXT_NOT_FOUND) break;
        strbuf_append(&out, s + at, found - at);
        strbuf_append(&out, repl, rlen);
        at = found + nlen;
        used++;
    }

    strbuf_append(&out, s + at, len - at);
    return strbuf_finish_string(vm, &out);
}

static Value *partition_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                               const Value *kwnames, int reverse) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    const char *needle;
    size_t nlen;
    if (text_value(vm, args[0], &needle, &nlen) != 0) return NULL;
    if (nlen == 0) return vm_fail(vm, VM_ERR_VALUE);

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    size_t found = reverse ? text_rfind(s, 0, len, needle, nlen)
                           : text_find(s, 0, len, needle, nlen);

    Value *result = value_new_tuple(3);
    if (!result) return vm_fail(vm, VM_ERR_OOM);

    size_t parts[3][2];

    if (found == TEXT_NOT_FOUND) {
        size_t tail = reverse ? 0 : len;
        parts[0][0] = 0;
        parts[0][1] = tail;
        parts[1][0] = tail;
        parts[1][1] = tail;
        parts[2][0] = tail;
        parts[2][1] = len;
    } else {
        parts[0][0] = 0;
        parts[0][1] = found;
        parts[1][0] = found;
        parts[1][1] = found + nlen;
        parts[2][0] = found + nlen;
        parts[2][1] = len;
    }

    for (int i = 0; i < 3; i++) {
        Value *piece = make_slice(vm, s, parts[i][0], parts[i][1]);
        if (!piece) {
            value_release(result);
            return NULL;
        }
        result->data.tuple.items[i] = piece;
    }

    return result;
}

static Value *str_partition(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return partition_common(vm, self, args, nargs, kwnames, 0);
}

static Value *str_rpartition(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return partition_common(vm, self, args, nargs, kwnames, 1);
}

static int append_slice(VM *vm, Value *list, const char *s, size_t begin, size_t end) {
    Value *piece = value_new_string_len(s + begin, end - begin);

    if (!piece) {
        vm->last_error = VM_ERR_OOM;
        return -1;
    }

    int rc = value_list_append(list, piece);
    value_release(piece);

    if (rc != 0) {
        vm->last_error = VM_ERR_OOM;
        return -1;
    }

    return 0;
}

static void reverse_list(Value *list) {
    Value **items = list->data.list.items;
    uint32_t len = list->data.list.len;

    for (uint32_t i = 0; i < len / 2; i++) {
        Value *t = items[i];
        items[i] = items[len - 1 - i];
        items[len - 1 - i] = t;
    }
}

static int split_by_space(VM *vm, Value *list, const char *s, size_t len,
                          int64_t budget, int reverse) {
    size_t begin;
    size_t end;
    text_space_bounds(s, len, SPACE_UNICODE, &begin, &end);

    if (!reverse) {
        size_t at = begin;
        int64_t used = 0;

        while (at < end && (budget < 0 || used < budget)) {
            size_t token = at;

            while (token < end) {
                size_t next = token;
                if (unicode_is_space(utf8_decode(s, len, &next))) break;
                token = next;
            }

            if (append_slice(vm, list, s, at, token) != 0) return -1;
            used++;

            while (token < end) {
                size_t next = token;
                if (!unicode_is_space(utf8_decode(s, len, &next))) break;
                token = next;
            }

            at = token;
        }

        if (at < end && append_slice(vm, list, s, at, len) != 0) return -1;
        return 0;
    }

    size_t at = end;
    int64_t used = 0;

    while (at > begin && (budget < 0 || used < budget)) {
        size_t token = at;

        while (token > begin) {
            size_t start = token - 1;
            while (start > begin && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
            size_t pos = start;
            if (unicode_is_space(utf8_decode(s, len, &pos))) break;
            token = start;
        }

        if (append_slice(vm, list, s, token, at) != 0) return -1;
        used++;

        while (token > begin) {
            size_t start = token - 1;
            while (start > begin && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
            size_t pos = start;
            if (!unicode_is_space(utf8_decode(s, len, &pos))) break;
            token = start;
        }

        at = token;
    }

    if (at > begin && append_slice(vm, list, s, 0, at) != 0) return -1;
    reverse_list(list);
    return 0;
}

static const char *const SPLIT_PARAMS[] = { "sep", "maxsplit" };

static Value *split_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                           const Value *kwnames, int reverse) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, SPLIT_PARAMS, 2, 2, 0, slots) != 0) return NULL;

    int64_t budget = -1;

    if (slots[1]) {
        if (slots[1]->tag != TAG_INT && slots[1]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);
        budget = slots[1]->data.int_val;
    }

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    Value *list = value_new_list();
    if (!list) return vm_fail(vm, VM_ERR_OOM);

    if (!slots[0] || slots[0]->tag == TAG_NONE) {
        if (split_by_space(vm, list, s, len, budget, reverse) != 0) {
            value_release(list);
            return NULL;
        }
        return list;
    }

    const char *sep;
    size_t seplen;

    if (text_value(vm, slots[0], &sep, &seplen) != 0) {
        value_release(list);
        return NULL;
    }

    if (seplen == 0) {
        value_release(list);
        return vm_fail(vm, VM_ERR_VALUE);
    }

    int64_t used = 0;
    int failed = 0;

    if (!reverse) {
        size_t at = 0;

        while (budget < 0 || used < budget) {
            size_t found = text_find(s, at, len, sep, seplen);
            if (found == TEXT_NOT_FOUND) break;
            if (append_slice(vm, list, s, at, found) != 0) { failed = 1; break; }
            at = found + seplen;
            used++;
        }

        if (!failed && append_slice(vm, list, s, at, len) != 0) failed = 1;
    } else {
        size_t at = len;

        while (budget < 0 || used < budget) {
            size_t found = text_rfind(s, 0, at, sep, seplen);
            if (found == TEXT_NOT_FOUND) break;
            if (append_slice(vm, list, s, found + seplen, at) != 0) { failed = 1; break; }
            at = found;
            used++;
        }

        if (!failed && append_slice(vm, list, s, 0, at) != 0) failed = 1;
        if (!failed) reverse_list(list);
    }

    if (failed) {
        value_release(list);
        return NULL;
    }

    return list;
}

static Value *str_split(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return split_common(vm, self, args, nargs, kwnames, 0);
}

static Value *str_rsplit(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return split_common(vm, self, args, nargs, kwnames, 1);
}

static const char *const SPLITLINES_PARAMS[] = { "keepends" };

static Value *str_splitlines(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[1];
    if (bind_args(vm, args, nargs, kwnames, SPLITLINES_PARAMS, 1, 1, 0, slots) != 0) return NULL;

    int keepends = slots[0] ? value_truthy(slots[0]) : 0;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    Value *list = value_new_list();
    if (!list) return vm_fail(vm, VM_ERR_OOM);

    size_t start = 0;
    size_t i = 0;

    while (i < len) {
        size_t next = i;
        uint32_t cp = utf8_decode(s, len, &next);

        if (!text_line_break(cp)) {
            i = next;
            continue;
        }

        size_t term = next;
        if (cp == '\r' && term < len && s[term] == '\n') term++;

        if (append_slice(vm, list, s, start, keepends ? term : i) != 0) {
            value_release(list);
            return NULL;
        }

        start = term;
        i = term;
    }

    if (start < len && append_slice(vm, list, s, start, len) != 0) {
        value_release(list);
        return NULL;
    }

    return list;
}

static Value *str_join(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *iter = value_make_iter(vm, args[0]);
    if (!iter) return NULL;

    StrBuf out = { NULL, 0, 0, 0 };
    int first = 1;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        if (item->tag != TAG_STRING) {
            value_release(item);
            vm->last_error = VM_ERR_TYPE;
            break;
        }

        if (!first) strbuf_append(&out, self->data.str.data, self->data.str.len);
        strbuf_append(&out, item->data.str.data, item->data.str.len);
        first = 0;
        value_release(item);
    }

    value_release(iter);

    if (vm->last_error != VM_ERR_OK) {
        strbuf_free(&out);
        return NULL;
    }

    return strbuf_finish_string(vm, &out);
}

static Value *strip_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                           const Value *kwnames, int left, int right) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;

    const char *s = self->data.str.data;
    size_t len = self->data.str.len;
    const char *set = NULL;
    size_t setlen = 0;

    if (nargs == 1 && args[0]->tag != TAG_NONE) {
        if (text_value(vm, args[0], &set, &setlen) != 0) return NULL;
    }

    size_t begin = 0;
    size_t end = len;

    if (!set) {
        size_t b;
        size_t e;
        text_space_bounds(s, len, SPACE_UNICODE, &b, &e);
        if (left) begin = b;
        if (right) end = e;
    } else {
        if (left) {
            while (begin < end) {
                size_t next = begin;
                uint32_t cp = utf8_decode(s, len, &next);
                if (!text_char_in_set(set, setlen, cp)) break;
                begin = next;
            }
        }

        if (right) {
            while (end > begin) {
                size_t start = end - 1;
                while (start > begin && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
                size_t pos = start;
                uint32_t cp = utf8_decode(s, len, &pos);
                if (!text_char_in_set(set, setlen, cp)) break;
                end = start;
            }
        }
    }

    return make_slice(vm, s, begin, end);
}

static Value *str_strip(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return strip_common(vm, self, args, nargs, kwnames, 1, 1);
}

static Value *str_lstrip(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return strip_common(vm, self, args, nargs, kwnames, 1, 0);
}

static Value *str_rstrip(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return strip_common(vm, self, args, nargs, kwnames, 0, 1);
}

const MethodEntry STR_METHODS[] = {
    {"encode", str_encode},
    {"split", str_split},
    {"rsplit", str_rsplit},
    {"splitlines", str_splitlines},
    {"join", str_join},
    {"strip", str_strip},
    {"lstrip", str_lstrip},
    {"rstrip", str_rstrip},
    {"find", str_find},
    {"rfind", str_rfind},
    {"index", str_index},
    {"rindex", str_rindex},
    {"count", str_count},
    {"startswith", str_startswith},
    {"endswith", str_endswith},
    {"replace", str_replace},
    {"partition", str_partition},
    {"rpartition", str_rpartition},
    {"upper", str_upper},
    {"lower", str_lower},
    {"casefold", str_casefold},
    {"title", str_title},
    {"capitalize", str_capitalize},
    {"swapcase", str_swapcase},
    {"isalpha", str_isalpha},
    {"isdecimal", str_isdecimal},
    {"isdigit", str_isdigit},
    {"isnumeric", str_isnumeric},
    {"isalnum", str_isalnum},
    {"isspace", str_isspace},
    {"islower", str_islower},
    {"isupper", str_isupper},
    {"istitle", str_istitle},
    {"isprintable", str_isprintable},
    {"isidentifier", str_isidentifier},
    {"isascii", str_isascii},
    {"translate", str_translate},
    {"format", str_format},
    {"format_map", str_format_map},
    {"ljust", str_ljust},
    {"rjust", str_rjust},
    {"center", str_center},
    {"zfill", str_zfill},
    {"expandtabs", str_expandtabs},
    {"removeprefix", str_removeprefix},
    {"removesuffix", str_removesuffix},
};

const uint32_t STR_METHOD_COUNT = (uint32_t)(sizeof(STR_METHODS) / sizeof(STR_METHODS[0]));
