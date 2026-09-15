#include <string.h>
#include "vm_internal.h"

static const char *const CODEC_PARAMS[] = { "encoding", "errors" };

static Value *bytes_decode(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, CODEC_PARAMS, 2, 2, 0, slots) != 0) return NULL;
    return codec_decode_value(vm, self->data.bytes.data, self->data.bytes.len, slots[0], slots[1]);
}

static Value *make_bytes(VM *vm, const char *s, size_t begin, size_t end) {
    Value *v = value_new_bytes((const unsigned char *)s + begin, end - begin);
    return v ? v : vm_fail(vm, VM_ERR_OOM);
}

static int needle_value(VM *vm, const Value *v, const char **data, size_t *len, char *scratch) {
    if (v && (v->tag == TAG_INT || v->tag == TAG_BOOL)) {
        int64_t n = v->data.int_val;

        if (n < 0 || n > 255) {
            vm->last_error = VM_ERR_VALUE;
            return -1;
        }

        scratch[0] = (char)n;
        *data = scratch;
        *len = 1;
        return 0;
    }

    return bytes_value(vm, v, data, len);
}

static int append_bytes(VM *vm, Value *list, const char *s, size_t begin, size_t end) {
    Value *piece = value_new_bytes((const unsigned char *)s + begin, end - begin);

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

static Value *find_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                          const Value *kwnames, int reverse, int raising) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    char scratch[1];
    const char *needle;
    size_t nlen;
    if (needle_value(vm, args[0], &needle, &nlen, scratch) != 0) return NULL;

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
    size_t begin;
    size_t end;

    int rc = bytes_bounds(vm, len, nargs > 1 ? args[1] : NULL,
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

    return value_new_int((int64_t)found);
}

static Value *bytes_find(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 0, 0);
}

static Value *bytes_rfind(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 1, 0);
}

static Value *bytes_index(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 0, 1);
}

static Value *bytes_rindex(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return find_common(vm, self, args, nargs, kwnames, 1, 1);
}

static Value *bytes_count(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    char scratch[1];
    const char *needle;
    size_t nlen;
    if (needle_value(vm, args[0], &needle, &nlen, scratch) != 0) return NULL;

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
    size_t begin;
    size_t end;

    int rc = bytes_bounds(vm, len, nargs > 1 ? args[1] : NULL,
                          nargs > 2 ? args[2] : NULL, &begin, &end);
    if (rc < 0) return NULL;
    if (rc > 0) return value_new_int(0);

    if (nlen == 0) return value_new_int((int64_t)(end - begin) + 1);

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

    if (bytes_value(vm, needle, &text, &nlen) != 0) return -1;

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

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
    size_t begin;
    size_t end;

    int rc = bytes_bounds(vm, len, nargs > 1 ? args[1] : NULL,
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

static Value *bytes_startswith(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return affix_common(vm, self, args, nargs, kwnames, 0);
}

static Value *bytes_endswith(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return affix_common(vm, self, args, nargs, kwnames, 1);
}

static Value *bytes_replace(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 3) != 0) return NULL;

    const char *needle;
    size_t nlen;
    const char *repl;
    size_t rlen;

    if (bytes_value(vm, args[0], &needle, &nlen) != 0) return NULL;
    if (bytes_value(vm, args[1], &repl, &rlen) != 0) return NULL;

    int64_t budget = -1;

    if (nargs == 3) {
        if (args[2]->tag != TAG_INT && args[2]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);
        budget = args[2]->data.int_val;
    }

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;

    if (budget == 0) return make_bytes(vm, s, 0, len);

    StrBuf out = { NULL, 0, 0, 0 };

    if (nlen == 0) {
        int64_t used = 0;

        for (size_t i = 0; i < len; i++) {
            if (budget < 0 || used < budget) {
                strbuf_append(&out, repl, rlen);
                used++;
            }
            strbuf_append(&out, s + i, 1);
        }

        if (budget < 0 || used < budget) strbuf_append(&out, repl, rlen);
        return strbuf_finish_bytes(vm, &out);
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
    return strbuf_finish_bytes(vm, &out);
}

static Value *partition_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                               const Value *kwnames, int reverse) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    const char *needle;
    size_t nlen;
    if (bytes_value(vm, args[0], &needle, &nlen) != 0) return NULL;
    if (nlen == 0) return vm_fail(vm, VM_ERR_VALUE);

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
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
        Value *piece = make_bytes(vm, s, parts[i][0], parts[i][1]);

        if (!piece) {
            value_release(result);
            return NULL;
        }

        result->data.tuple.items[i] = piece;
    }

    return result;
}

static Value *bytes_partition(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return partition_common(vm, self, args, nargs, kwnames, 0);
}

static Value *bytes_rpartition(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return partition_common(vm, self, args, nargs, kwnames, 1);
}

static int split_by_space(VM *vm, Value *list, const char *s, size_t len,
                          int64_t budget, int reverse) {
    size_t begin = 0;
    size_t end = len;

    while (begin < end && ascii_is_space((unsigned char)s[begin])) begin++;
    while (end > begin && ascii_is_space((unsigned char)s[end - 1])) end--;

    if (!reverse) {
        size_t at = begin;
        int64_t used = 0;

        while (at < end && (budget < 0 || used < budget)) {
            size_t token = at;
            while (token < end && !ascii_is_space((unsigned char)s[token])) token++;
            if (append_bytes(vm, list, s, at, token) != 0) return -1;
            used++;
            while (token < end && ascii_is_space((unsigned char)s[token])) token++;
            at = token;
        }

        if (at < end && append_bytes(vm, list, s, at, len) != 0) return -1;
        return 0;
    }

    size_t at = end;
    int64_t used = 0;

    while (at > begin && (budget < 0 || used < budget)) {
        size_t token = at;
        while (token > begin && !ascii_is_space((unsigned char)s[token - 1])) token--;
        if (append_bytes(vm, list, s, token, at) != 0) return -1;
        used++;
        while (token > begin && ascii_is_space((unsigned char)s[token - 1])) token--;
        at = token;
    }

    if (at > begin && append_bytes(vm, list, s, 0, at) != 0) return -1;
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

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;

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

    if (bytes_value(vm, slots[0], &sep, &seplen) != 0) {
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
            if (append_bytes(vm, list, s, at, found) != 0) { failed = 1; break; }
            at = found + seplen;
            used++;
        }

        if (!failed && append_bytes(vm, list, s, at, len) != 0) failed = 1;
    } else {
        size_t at = len;

        while (budget < 0 || used < budget) {
            size_t found = text_rfind(s, 0, at, sep, seplen);
            if (found == TEXT_NOT_FOUND) break;
            if (append_bytes(vm, list, s, found + seplen, at) != 0) { failed = 1; break; }
            at = found;
            used++;
        }

        if (!failed && append_bytes(vm, list, s, 0, at) != 0) failed = 1;
        if (!failed) reverse_list(list);
    }

    if (failed) {
        value_release(list);
        return NULL;
    }

    return list;
}

static Value *bytes_split(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return split_common(vm, self, args, nargs, kwnames, 0);
}

static Value *bytes_rsplit(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return split_common(vm, self, args, nargs, kwnames, 1);
}

static const char *const SPLITLINES_PARAMS[] = { "keepends" };

static Value *bytes_splitlines(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[1];
    if (bind_args(vm, args, nargs, kwnames, SPLITLINES_PARAMS, 1, 1, 0, slots) != 0) return NULL;

    int keepends = slots[0] ? value_truthy(slots[0]) : 0;

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;

    Value *list = value_new_list();
    if (!list) return vm_fail(vm, VM_ERR_OOM);

    size_t start = 0;
    size_t i = 0;

    while (i < len) {
        char c = s[i];

        if (c != '\n' && c != '\r') {
            i++;
            continue;
        }

        size_t term = i + 1;
        if (c == '\r' && term < len && s[term] == '\n') term++;

        if (append_bytes(vm, list, s, start, keepends ? term : i) != 0) {
            value_release(list);
            return NULL;
        }

        start = term;
        i = term;
    }

    if (start < len && append_bytes(vm, list, s, start, len) != 0) {
        value_release(list);
        return NULL;
    }

    return list;
}

static Value *bytes_join(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *iter = value_make_iter(vm, args[0]);
    if (!iter) return NULL;

    StrBuf out = { NULL, 0, 0, 0 };
    int first = 1;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        if (item->tag != TAG_BYTES) {
            value_release(item);
            vm->last_error = VM_ERR_TYPE;
            break;
        }

        if (!first) {
            strbuf_append(&out, (const char *)self->data.bytes.data, self->data.bytes.len);
        }

        strbuf_append(&out, (const char *)item->data.bytes.data, item->data.bytes.len);
        first = 0;
        value_release(item);
    }

    value_release(iter);

    if (vm->last_error != VM_ERR_OK) {
        strbuf_free(&out);
        return NULL;
    }

    return strbuf_finish_bytes(vm, &out);
}

static Value *strip_common(VM *vm, Value *self, Value **args, uint32_t nargs,
                           const Value *kwnames, int left, int right) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;

    const char *s = (const char *)self->data.bytes.data;
    size_t len = self->data.bytes.len;
    const char *set = NULL;
    size_t setlen = 0;

    if (nargs == 1 && args[0]->tag != TAG_NONE) {
        if (bytes_value(vm, args[0], &set, &setlen) != 0) return NULL;
    }

    size_t begin = 0;
    size_t end = len;

    if (left) {
        while (begin < end) {
            unsigned char c = (unsigned char)s[begin];
            int strip = set ? memchr(set, c, setlen) != NULL : ascii_is_space(c);
            if (!strip) break;
            begin++;
        }
    }

    if (right) {
        while (end > begin) {
            unsigned char c = (unsigned char)s[end - 1];
            int strip = set ? memchr(set, c, setlen) != NULL : ascii_is_space(c);
            if (!strip) break;
            end--;
        }
    }

    return make_bytes(vm, s, begin, end);
}

static Value *bytes_strip(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return strip_common(vm, self, args, nargs, kwnames, 1, 1);
}

static Value *bytes_lstrip(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return strip_common(vm, self, args, nargs, kwnames, 1, 0);
}

static Value *bytes_rstrip(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    return strip_common(vm, self, args, nargs, kwnames, 0, 1);
}

static const char HEX_DIGITS[] = "0123456789abcdef";

static Value *bytes_hex(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 2) != 0) return NULL;

    const char *sep = NULL;
    size_t seplen = 0;
    int64_t group = 1;

    if (nargs >= 1) {
        if (text_value(vm, args[0], &sep, &seplen) != 0) return NULL;
    }

    if (nargs == 2) {
        if (args[1]->tag != TAG_INT && args[1]->tag != TAG_BOOL) return vm_fail(vm, VM_ERR_TYPE);
        group = args[1]->data.int_val;
        if (group == 0) return vm_fail(vm, VM_ERR_VALUE);
    }

    const unsigned char *data = self->data.bytes.data;
    size_t len = self->data.bytes.len;
    size_t span = group < 0 ? (size_t)(-group) : (size_t)group;

    StrBuf out = { NULL, 0, 0, 0 };

    for (size_t i = 0; i < len; i++) {
        if (sep && i > 0) {
            int boundary = group < 0 ? (i % span) == 0 : ((len - i) % span) == 0;
            if (boundary) strbuf_append(&out, sep, seplen);
        }

        char pair[2];
        pair[0] = HEX_DIGITS[data[i] >> 4];
        pair[1] = HEX_DIGITS[data[i] & 0x0F];
        strbuf_append(&out, pair, 2);
    }

    return strbuf_finish_string(vm, &out);
}

const MethodEntry BYTES_METHODS[] = {
    {"decode", bytes_decode},
    {"find", bytes_find},
    {"rfind", bytes_rfind},
    {"index", bytes_index},
    {"rindex", bytes_rindex},
    {"count", bytes_count},
    {"startswith", bytes_startswith},
    {"endswith", bytes_endswith},
    {"replace", bytes_replace},
    {"partition", bytes_partition},
    {"rpartition", bytes_rpartition},
    {"split", bytes_split},
    {"rsplit", bytes_rsplit},
    {"splitlines", bytes_splitlines},
    {"join", bytes_join},
    {"strip", bytes_strip},
    {"lstrip", bytes_lstrip},
    {"rstrip", bytes_rstrip},
    {"hex", bytes_hex},
    {"upper", bytes_upper},
    {"lower", bytes_lower},
    {"title", bytes_title},
    {"capitalize", bytes_capitalize},
    {"swapcase", bytes_swapcase},
    {"center", bytes_center},
    {"ljust", bytes_ljust},
    {"rjust", bytes_rjust},
    {"zfill", bytes_zfill},
    {"expandtabs", bytes_expandtabs},
    {"removeprefix", bytes_removeprefix},
    {"removesuffix", bytes_removesuffix},
    {"isalpha", bytes_isalpha},
    {"isdigit", bytes_isdigit},
    {"isalnum", bytes_isalnum},
    {"isspace", bytes_isspace},
    {"islower", bytes_islower},
    {"isupper", bytes_isupper},
    {"istitle", bytes_istitle},
    {"isascii", bytes_isascii},
    {"translate", bytes_translate},
};

const uint32_t BYTES_METHOD_COUNT = (uint32_t)(sizeof(BYTES_METHODS) / sizeof(BYTES_METHODS[0]));
