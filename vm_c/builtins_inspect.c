#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int match_type(const Value *x, const Value *type) {
    if (value_type_of(x) == type) return 1;
    if (x->tag == TAG_BOOL && strcmp(value_type_name(type), "int") == 0) return 1;
    return 0;
}

Value *builtin_isinstance(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 2) != 0) return NULL;

    Value *x = args[0];
    Value *classinfo = args[1];

    if (classinfo->tag == TAG_TYPE) {
        return value_bool(match_type(x, classinfo));
    }

    if (classinfo->tag == TAG_TUPLE) {
        for (uint32_t i = 0; i < classinfo->data.tuple.len; i++) {
            Value *entry = classinfo->data.tuple.items[i];
            if (entry->tag != TAG_TYPE) return vm_fail(vm, VM_ERR_TYPE);
            if (match_type(x, entry)) return value_bool(1);
        }
        return value_bool(0);
    }

    return vm_fail(vm, VM_ERR_TYPE);
}

Value *builtin_callable(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    ValueTag tag = args[0]->tag;
    return value_bool(tag == TAG_FUNCTION || tag == TAG_TYPE);
}

Value *builtin_id(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    return value_new_int((int64_t)(intptr_t)args[0]);
}

Value *builtin_ord(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *x = args[0];

    if (x->tag == TAG_STRING) {
        if (utf8_length(x->data.str.data, x->data.str.len) != 1) return vm_fail(vm, VM_ERR_TYPE);
        size_t offset = 0;
        return value_new_int((int64_t)utf8_decode(x->data.str.data, x->data.str.len, &offset));
    }

    if (x->tag == TAG_BYTES) {
        if (x->data.bytes.len != 1) return vm_fail(vm, VM_ERR_TYPE);
        return value_new_int(x->data.bytes.data[0]);
    }

    return vm_fail(vm, VM_ERR_TYPE);
}

Value *builtin_chr(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (!int_like(args[0])) return vm_fail(vm, VM_ERR_TYPE);

    int64_t cp = args[0]->data.int_val;
    if (cp < 0 || cp > 0x10FFFF) return vm_fail(vm, VM_ERR_VALUE);

    char buf[4];
    size_t len = utf8_encode((uint32_t)cp, buf);
    return value_new_string_len(buf, len);
}

static Value *format_with_spec(VM *vm, const Value *v, const char *spec, size_t spec_len) {
    size_t out_len = 0;
    int err = 0;
    char *s = format_value(v, spec, spec_len, 0, &out_len, &err);
    if (err || !s) {
        free(s);
        return vm_fail(vm, VM_ERR_VALUE);
    }
    Value *result = value_new_string_len(s, out_len);
    free(s);
    return result ? result : vm_fail(vm, VM_ERR_OOM);
}

static Value *radix(VM *vm, Value **args, uint32_t nargs, const Value *kwnames, char code) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (!int_like(args[0])) return vm_fail(vm, VM_ERR_TYPE);
    char spec[2] = { '#', code };
    return format_with_spec(vm, args[0], spec, 2);
}

Value *builtin_bin(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return radix(vm, args, nargs, kwnames, 'b');
}

Value *builtin_oct(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return radix(vm, args, nargs, kwnames, 'o');
}

Value *builtin_hex(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return radix(vm, args, nargs, kwnames, 'x');
}

Value *builtin_format(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;

    if (nargs == 1) {
        size_t len = 0;
        char *s = value_to_string_sized(args[0], &len);
        if (!s) return vm_fail(vm, VM_ERR_OOM);
        Value *result = value_new_string_len(s, len);
        free(s);
        return result ? result : vm_fail(vm, VM_ERR_OOM);
    }

    if (args[1]->tag != TAG_STRING) return vm_fail(vm, VM_ERR_TYPE);
    return format_with_spec(vm, args[0], args[1]->data.str.data, args[1]->data.str.len);
}

Value *builtin_ascii(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    char *s = value_to_ascii(args[0]);
    if (!s) return vm_fail(vm, VM_ERR_OOM);

    Value *result = value_new_string_len(s, strlen(s));
    free(s);
    return result ? result : vm_fail(vm, VM_ERR_OOM);
}
