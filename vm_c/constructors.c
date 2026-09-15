#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 36;
}

static int parse_int_text(const char *s, size_t len, int space_mode, int base, int64_t *out) {
    size_t i;
    size_t end;
    text_space_bounds(s, len, space_mode, &i, &end);

    int negative = 0;
    if (i < end && (s[i] == '+' || s[i] == '-')) {
        negative = s[i] == '-';
        i++;
    }

    int underscore_ok = 0;
    int check_leading_zero = 0;

    if (end - i >= 2 && s[i] == '0') {
        char marker = (char)(s[i + 1] | 0x20);
        int prefix_base = marker == 'x' ? 16 : marker == 'o' ? 8 : marker == 'b' ? 2 : 0;
        if (prefix_base && (base == 0 || base == prefix_base)) {
            base = prefix_base;
            i += 2;
            underscore_ok = 1;
        }
    }

    if (base == 0) {
        base = 10;
        check_leading_zero = 1;
    }

    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    uint64_t acc = 0;
    int digits = 0;
    int overflow = 0;
    int leading_zero = 0;
    int nonzero = 0;
    int last_underscore = 0;

    for (; i < end; i++) {
        char c = s[i];

        if (c == '_') {
            if (!underscore_ok) return VM_ERR_VALUE;
            underscore_ok = 0;
            last_underscore = 1;
            continue;
        }

        int d = digit_value(c);
        if (d >= base) return VM_ERR_VALUE;

        if (digits == 0 && d == 0) leading_zero = 1;
        if (d != 0) nonzero = 1;

        if (!overflow) {
            if (acc > (limit - (uint64_t)d) / (uint64_t)base) {
                overflow = 1;
            } else {
                acc = acc * (uint64_t)base + (uint64_t)d;
            }
        }

        digits++;
        underscore_ok = 1;
        last_underscore = 0;
    }

    if (digits == 0 || last_underscore) return VM_ERR_VALUE;
    if (check_leading_zero && leading_zero && nonzero) return VM_ERR_VALUE;
    if (overflow) return VM_ERR_OVERFLOW;

    if (!negative || acc == 0) {
        *out = (int64_t)acc;
    } else {
        *out = -(int64_t)(acc - 1) - 1;
    }
    return VM_ERR_OK;
}

static int ascii_equal_nocase(const char *s, size_t len, const char *word) {
    size_t n = strlen(word);
    if (len != n) return 0;

    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != word[i]) return 0;
    }

    return 1;
}

static size_t scan_digits(const char *s, size_t *pos, size_t end, char *buf, size_t *o) {
    size_t i = *pos;
    size_t count = 0;

    while (i < end) {
        if (s[i] >= '0' && s[i] <= '9') {
            buf[(*o)++] = s[i++];
            count++;
        } else if (s[i] == '_' && count > 0 && i + 1 < end && s[i + 1] >= '0' && s[i + 1] <= '9') {
            i++;
        } else {
            break;
        }
    }

    *pos = i;
    return count;
}

static int parse_float_text(const char *s, size_t len, int space_mode, double *out) {
    size_t i;
    size_t end;
    text_space_bounds(s, len, space_mode, &i, &end);

    int negative = 0;
    if (i < end && (s[i] == '+' || s[i] == '-')) {
        negative = s[i] == '-';
        i++;
    }

    if (i >= end) return VM_ERR_VALUE;

    if (ascii_equal_nocase(s + i, end - i, "inf") || ascii_equal_nocase(s + i, end - i, "infinity")) {
        *out = negative ? -HUGE_VAL : HUGE_VAL;
        return VM_ERR_OK;
    }

    if (ascii_equal_nocase(s + i, end - i, "nan")) {
        double nan_value = strtod("nan", NULL);
        *out = negative ? -nan_value : nan_value;
        return VM_ERR_OK;
    }

    char *buf = malloc(end - i + 2);
    if (!buf) return VM_ERR_OOM;

    size_t o = 0;
    if (negative) buf[o++] = '-';

    size_t whole = scan_digits(s, &i, end, buf, &o);
    size_t frac = 0;

    if (i < end && s[i] == '.') {
        buf[o++] = '.';
        i++;
        frac = scan_digits(s, &i, end, buf, &o);
    }

    int ok = whole + frac > 0;

    if (ok && i < end && (s[i] == 'e' || s[i] == 'E')) {
        buf[o++] = 'e';
        i++;
        if (i < end && (s[i] == '+' || s[i] == '-')) {
            buf[o++] = s[i++];
        }
        ok = scan_digits(s, &i, end, buf, &o) > 0;
    }

    ok = ok && i == end;

    if (ok) {
        buf[o] = '\0';
        *out = strtod(buf, NULL);
    }

    free(buf);
    return ok ? VM_ERR_OK : VM_ERR_VALUE;
}

static const char *const INT_PARAMS[] = { NULL, "base" };

Value *construct_int(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, INT_PARAMS, 2, 2, 0, slots) != 0) return NULL;

    Value *x = slots[0];
    int base = 10;

    if (slots[1]) {
        if (!x || !int_like(slots[1])) return vm_fail(vm, VM_ERR_TYPE);
        int64_t requested = slots[1]->data.int_val;
        if (requested != 0 && (requested < 2 || requested > 36)) return vm_fail(vm, VM_ERR_VALUE);
        if (x->tag != TAG_STRING && x->tag != TAG_BYTES) return vm_fail(vm, VM_ERR_TYPE);
        base = (int)requested;
    }

    if (!x) return value_new_int(0);

    int64_t result = 0;
    int err;

    switch (x->tag) {
        case TAG_INT:
        case TAG_BOOL:
            return value_new_int(x->data.int_val);

        case TAG_FLOAT: {
            double d = x->data.float_val;
            if (isnan(d)) return vm_fail(vm, VM_ERR_VALUE);
            if (isinf(d)) return vm_fail(vm, VM_ERR_OVERFLOW);
            d = trunc(d);
            if (d >= 9223372036854775808.0 || d < -9223372036854775808.0) {
                return vm_fail(vm, VM_ERR_OVERFLOW);
            }
            return value_new_int((int64_t)d);
        }

        case TAG_STRING:
            err = parse_int_text(x->data.str.data, x->data.str.len, SPACE_NUMERIC, base, &result);
            break;

        case TAG_BYTES:
            err = parse_int_text((const char *)x->data.bytes.data, x->data.bytes.len, SPACE_ASCII, base, &result);
            break;

        default:
            return vm_fail(vm, VM_ERR_TYPE);
    }

    if (err != VM_ERR_OK) return vm_fail(vm, err);
    return value_new_int(result);
}

Value *construct_float(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;
    if (nargs == 0) return value_new_float(0.0);

    Value *x = args[0];
    double result = 0.0;
    int err;

    switch (x->tag) {
        case TAG_FLOAT:
            return value_new_float(x->data.float_val);

        case TAG_INT:
        case TAG_BOOL:
            return value_new_float((double)x->data.int_val);

        case TAG_STRING:
            err = parse_float_text(x->data.str.data, x->data.str.len, SPACE_NUMERIC, &result);
            break;

        case TAG_BYTES:
            err = parse_float_text((const char *)x->data.bytes.data, x->data.bytes.len, SPACE_ASCII, &result);
            break;

        default:
            return vm_fail(vm, VM_ERR_TYPE);
    }

    if (err != VM_ERR_OK) return vm_fail(vm, err);
    return value_new_float(result);
}

static const char *const STR_PARAMS[] = { "object", "encoding", "errors" };

Value *construct_str(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[3];
    if (bind_args(vm, args, nargs, kwnames, STR_PARAMS, 3, 3, 0, slots) != 0) return NULL;

    Value *object = slots[0];

    if (slots[1] || slots[2]) {
        if ((slots[1] && slots[1]->tag != TAG_STRING) || (slots[2] && slots[2]->tag != TAG_STRING)) {
            return vm_fail(vm, VM_ERR_TYPE);
        }
        if (!object) return value_new_string_len("", 0);
        if (object->tag != TAG_BYTES) return vm_fail(vm, VM_ERR_TYPE);
        return codec_decode_value(vm, object->data.bytes.data, object->data.bytes.len, slots[1], slots[2]);
    }

    if (!object) return value_new_string_len("", 0);

    size_t len = 0;
    char *s = value_to_string_sized(object, &len);
    if (!s) return vm_fail(vm, VM_ERR_OOM);

    Value *result = value_new_string_len(s, len);
    free(s);
    return result;
}

Value *construct_frozenset(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;
    if (nargs == 1 && args[0]->tag == TAG_FROZENSET) return value_retain(args[0]);

    Value *set = value_new_frozenset();
    if (!set) return vm_fail(vm, VM_ERR_OOM);

    if (nargs == 1 && value_set_update(vm, set, args[0]) != 0) {
        value_release(set);
        return NULL;
    }

    return set;
}

static const char *const BYTES_PARAMS[] = { "source", "encoding", "errors" };

Value *construct_bytes(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[3];
    if (bind_args(vm, args, nargs, kwnames, BYTES_PARAMS, 3, 3, 0, slots) != 0) return NULL;

    Value *source = slots[0];
    Value *encoding = slots[1];
    Value *errors = slots[2];

    if ((encoding && encoding->tag != TAG_STRING) || (errors && errors->tag != TAG_STRING)) {
        return vm_fail(vm, VM_ERR_TYPE);
    }

    if (source && source->tag == TAG_STRING) {
        if (!encoding) return vm_fail(vm, VM_ERR_TYPE);
        return codec_encode_value(vm, source, encoding, errors);
    }

    if (encoding || errors) return vm_fail(vm, VM_ERR_TYPE);
    if (!source) return value_new_bytes(NULL, 0);
    if (source->tag == TAG_BYTES) return value_retain(source);

    if (int_like(source)) {
        int64_t count = source->data.int_val;
        if (count < 0) return vm_fail(vm, VM_ERR_VALUE);
        if ((uint64_t)count > UINT32_MAX) return vm_fail(vm, VM_ERR_OOM);

        unsigned char *zeros = calloc((size_t)count + 1, 1);
        if (!zeros) return vm_fail(vm, VM_ERR_OOM);

        Value *result = value_new_bytes(zeros, (size_t)count);
        free(zeros);
        return result;
    }

    Value *iter = value_make_iter(vm, source);
    if (!iter) return NULL;

    StrBuf out = { NULL, 0, 0, 0 };

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        if (!int_like(item)) {
            vm->last_error = VM_ERR_TYPE;
        } else if (item->data.int_val < 0 || item->data.int_val > 255) {
            vm->last_error = VM_ERR_VALUE;
        } else {
            char byte = (char)item->data.int_val;
            strbuf_append(&out, &byte, 1);
        }

        value_release(item);
        if (vm->last_error != VM_ERR_OK) break;
    }

    value_release(iter);

    if (vm->last_error != VM_ERR_OK) {
        strbuf_free(&out);
        return NULL;
    }

    return strbuf_finish_bytes(vm, &out);
}

Value *construct_bool(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;
    return value_bool(nargs == 1 && value_truthy(args[0]));
}

Value *construct_list(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;
    if (nargs == 0) return value_new_list();
    return value_list_from_iterable(vm, args[0]);
}

Value *construct_range(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    int64_t nums[3] = { 0, 0, 1 };

    for (uint32_t i = 0; i < nargs; i++) {
        if (!int_like(args[i])) return vm_fail(vm, VM_ERR_TYPE);
    }

    if (nargs == 1) {
        nums[1] = args[0]->data.int_val;
    } else {
        nums[0] = args[0]->data.int_val;
        nums[1] = args[1]->data.int_val;
        if (nargs == 3) nums[2] = args[2]->data.int_val;
    }

    if (nums[2] == 0) return vm_fail(vm, VM_ERR_VALUE);
    return value_new_range(nums[0], nums[1], nums[2]);
}

Value *construct_type(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    return value_retain(value_type_of(args[0]));
}

Value *construct_tuple(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;
    if (nargs == 0) return value_new_tuple(0);
    if (args[0]->tag == TAG_TUPLE) return value_retain(args[0]);

    Value *list = value_list_from_iterable(vm, args[0]);
    if (!list) return NULL;

    Value *tuple = value_new_tuple(list->data.list.len);
    if (tuple) {
        for (uint32_t i = 0; i < list->data.list.len; i++) {
            tuple->data.tuple.items[i] = value_retain(list->data.list.items[i]);
        }
    }

    value_release(list);
    return tuple ? tuple : vm_fail(vm, VM_ERR_OOM);
}

Value *construct_set(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;

    Value *set = value_new_set();
    if (!set) return vm_fail(vm, VM_ERR_OOM);

    if (nargs == 1 && value_set_update(vm, set, args[0]) != 0) {
        value_release(set);
        return NULL;
    }

    return set;
}

Value *construct_dict(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t nkw = kw_count(kwnames);
    uint32_t npos = nargs - nkw;
    if (npos > 1) return vm_fail(vm, VM_ERR_TYPE);

    Value *dict = value_new_dict();
    if (!dict) return vm_fail(vm, VM_ERR_OOM);

    if (npos == 1 && value_dict_update(vm, dict, args[0]) != 0) {
        value_release(dict);
        return NULL;
    }

    for (uint32_t k = 0; k < nkw; k++) {
        int rc = value_dict_set(dict, kwnames->data.tuple.items[k], args[npos + k]);
        if (rc != 0) {
            value_release(dict);
            return vm_fail(vm, insert_error(rc));
        }
    }

    return dict;
}
