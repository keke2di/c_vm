#include <string.h>
#include "vm_internal.h"

int text_value(VM *vm, const Value *v, const char **data, size_t *len) {
    if (!v || v->tag != TAG_STRING) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }

    *data = v->data.str.data;
    *len = v->data.str.len;
    return 0;
}

static int index_arg(VM *vm, const Value *v, int64_t *out, int64_t fallback) {
    if (!v || v->tag == TAG_NONE) {
        *out = fallback;
        return 0;
    }

    if (v->tag != TAG_INT && v->tag != TAG_BOOL) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }

    *out = v->data.int_val;
    return 0;
}

int bytes_value(VM *vm, const Value *v, const char **data, size_t *len) {
    if (!v || v->tag != TAG_BYTES) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }

    *data = (const char *)v->data.bytes.data;
    *len = v->data.bytes.len;
    return 0;
}

static int resolve_bounds(VM *vm, int64_t len, const Value *start, const Value *stop,
                          int64_t *lo_out, int64_t *hi_out) {
    int64_t lo;
    int64_t hi;

    if (index_arg(vm, start, &lo, 0) != 0) return -1;
    if (index_arg(vm, stop, &hi, len) != 0) return -1;

    if (lo < 0) {
        lo += len;
        if (lo < 0) lo = 0;
    }

    if (hi < 0) {
        hi += len;
        if (hi < 0) hi = 0;
    }

    if (hi > len) hi = len;
    if (lo > len || hi < lo) return 1;

    *lo_out = lo;
    *hi_out = hi;
    return 0;
}

int text_bounds(VM *vm, const char *s, size_t len, const Value *start, const Value *stop,
                size_t *begin, size_t *end) {
    int64_t lo;
    int64_t hi;

    int rc = resolve_bounds(vm, (int64_t)utf8_length(s, len), start, stop, &lo, &hi);
    if (rc != 0) return rc;

    *begin = utf8_offset(s, len, (size_t)lo);
    *end = utf8_offset(s, len, (size_t)hi);
    return 0;
}

int bytes_bounds(VM *vm, size_t len, const Value *start, const Value *stop,
                 size_t *begin, size_t *end) {
    int64_t lo;
    int64_t hi;

    int rc = resolve_bounds(vm, (int64_t)len, start, stop, &lo, &hi);
    if (rc != 0) return rc;

    *begin = (size_t)lo;
    *end = (size_t)hi;
    return 0;
}

int text_char_in_set(const char *set, size_t setlen, uint32_t cp) {
    size_t i = 0;

    while (i < setlen) {
        if (utf8_decode(set, setlen, &i) == cp) return 1;
    }

    return 0;
}

int text_line_break(uint32_t cp) {
    return cp == '\n' || cp == '\v' || cp == '\f' || cp == '\r' ||
           cp == 0x1C || cp == 0x1D || cp == 0x1E || cp == 0x85 ||
           cp == 0x2028 || cp == 0x2029;
}

size_t text_find(const char *hay, size_t begin, size_t end, const char *needle, size_t nlen) {
    if (nlen == 0) return begin;
    if (end < nlen || begin > end - nlen) return TEXT_NOT_FOUND;

    for (size_t i = begin; i + nlen <= end; i++) {
        if (memcmp(hay + i, needle, nlen) == 0) return i;
    }

    return TEXT_NOT_FOUND;
}

size_t text_rfind(const char *hay, size_t begin, size_t end, const char *needle, size_t nlen) {
    if (nlen == 0) return end;
    if (end < nlen || begin > end - nlen) return TEXT_NOT_FOUND;

    for (size_t i = end - nlen + 1; i > begin; i--) {
        if (memcmp(hay + i - 1, needle, nlen) == 0) return i - 1;
    }

    return TEXT_NOT_FOUND;
}
