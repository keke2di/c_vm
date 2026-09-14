#include <string.h>
#include "opcodes.h"
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int is_number(const Value *v) {
    return int_like(v) || v->tag == TAG_FLOAT;
}

static double as_double(const Value *v) {
    return v->tag == TAG_FLOAT ? v->data.float_val : (double)v->data.int_val;
}

static int compare_ints(uint8_t op, int64_t a, int64_t b) {
    switch (op) {
        case OP_COMPARE_EQ: return a == b;
        case OP_COMPARE_NE: return a != b;
        case OP_COMPARE_LT: return a < b;
        case OP_COMPARE_LE: return a <= b;
        case OP_COMPARE_GT: return a > b;
        case OP_COMPARE_GE: return a >= b;
        default: return 0;
    }
}

static int compare_doubles(uint8_t op, double a, double b) {
    switch (op) {
        case OP_COMPARE_EQ: return a == b;
        case OP_COMPARE_NE: return a != b;
        case OP_COMPARE_LT: return a < b;
        case OP_COMPARE_LE: return a <= b;
        case OP_COMPARE_GT: return a > b;
        case OP_COMPARE_GE: return a >= b;
        default: return 0;
    }
}

static int compare_ordering(uint8_t op, int cmp) {
    return compare_ints(op, cmp, 0);
}

static int compare_bytes(const Value *a, const Value *b) {
    uint32_t a_len = a->data.bytes.len;
    uint32_t b_len = b->data.bytes.len;
    uint32_t min_len = a_len < b_len ? a_len : b_len;

    int cmp = min_len > 0 ? memcmp(a->data.bytes.data, b->data.bytes.data, min_len) : 0;
    if (cmp != 0) return cmp;
    if (a_len < b_len) return -1;
    if (a_len > b_len) return 1;
    return 0;
}

static int tuples_equal(const Value *a, const Value *b) {
    if (a->data.tuple.len != b->data.tuple.len) return 0;

    for (uint32_t i = 0; i < a->data.tuple.len; i++) {
        if (value_compare(a->data.tuple.items[i], b->data.tuple.items[i]) != 0) {
            return 0;
        }
    }

    return 1;
}

void op_compare(VM *vm, uint8_t op) {
    Value *b = vm_pop(vm);
    Value *a = vm_pop(vm);

    if (!a || !b) {
        if (a) value_release(a);
        if (b) value_release(b);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    int result = 0;

    if (int_like(a) && int_like(b)) {
        result = compare_ints(op, a->data.int_val, b->data.int_val);
    } else if (is_number(a) && is_number(b)) {
        result = compare_doubles(op, as_double(a), as_double(b));
    } else if (a->tag == TAG_STRING && b->tag == TAG_STRING) {
        result = compare_ordering(op, value_compare(a, b));
    } else if (a->tag == TAG_BYTES && b->tag == TAG_BYTES) {
        result = compare_ordering(op, compare_bytes(a, b));
    } else if (a->tag == TAG_TUPLE && b->tag == TAG_TUPLE &&
               (op == OP_COMPARE_EQ || op == OP_COMPARE_NE)) {
        int equal = tuples_equal(a, b);
        result = op == OP_COMPARE_EQ ? equal : !equal;
    } else if (a->tag == TAG_TYPE && b->tag == TAG_TYPE &&
               (op == OP_COMPARE_EQ || op == OP_COMPARE_NE)) {
        int equal = a == b;
        result = op == OP_COMPARE_EQ ? equal : !equal;
    } else {
        vm->last_error = VM_ERR_TYPE;
    }

    if (vm->last_error == VM_ERR_OK) {
        vm_push_owned(vm, value_bool(result));
    }

    value_release(a);
    value_release(b);
}

static int contains_substring(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len) {
    if (needle_len == 0) return 1;
    if (needle_len > haystack_len) return 0;

    for (size_t i = 0; i + needle_len <= haystack_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return 1;
        }
    }

    return 0;
}

void op_contains(VM *vm) {
    Value *container = vm_pop(vm);
    Value *item = vm_pop(vm);

    if (!item || !container) {
        vm->last_error = VM_ERR_STACK;
        if (item) value_release(item);
        if (container) value_release(container);
        return;
    }

    int found = 0;

    switch (container->tag) {
        case TAG_LIST:
            for (uint32_t i = 0; i < container->data.list.len && !found; i++) {
                found = value_compare(container->data.list.items[i], item) == 0;
            }
            break;

        case TAG_TUPLE:
            for (uint32_t i = 0; i < container->data.tuple.len && !found; i++) {
                found = value_compare(container->data.tuple.items[i], item) == 0;
            }
            break;

        case TAG_BYTES:
            if (!int_like(item)) {
                vm->last_error = VM_ERR_TYPE;
            } else if (item->data.int_val >= 0 && item->data.int_val <= 255) {
                unsigned char needle = (unsigned char)item->data.int_val;
                for (uint32_t i = 0; i < container->data.bytes.len && !found; i++) {
                    found = container->data.bytes.data[i] == needle;
                }
            }
            break;

        case TAG_STRING:
            if (item->tag != TAG_STRING) {
                vm->last_error = VM_ERR_TYPE;
            } else {
                found = contains_substring(
                    container->data.str.data,
                    container->data.str.len,
                    item->data.str.data,
                    item->data.str.len);
            }
            break;

        case TAG_DICT:
            found = value_dict_get(container, item) != NULL;
            break;

        case TAG_SET:
            found = value_set_contains(container, item);
            break;

        default:
            vm->last_error = VM_ERR_TYPE;
            break;
    }

    value_release(item);
    value_release(container);

    if (vm->last_error == VM_ERR_OK) {
        vm_push_owned(vm, value_bool(found));
    }
}

void op_is(VM *vm, int negate) {
    Value *b = vm_pop(vm);
    Value *a = vm_pop(vm);

    if (!a || !b) {
        if (a) value_release(a);
        if (b) value_release(b);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    int same = a == b;
    vm_push_owned(vm, value_bool(negate ? !same : same));

    value_release(a);
    value_release(b);
}
