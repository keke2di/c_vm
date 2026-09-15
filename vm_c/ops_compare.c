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
        case OP_COMPARE_LT: return a < b;
        case OP_COMPARE_LE: return a <= b;
        case OP_COMPARE_GT: return a > b;
        case OP_COMPARE_GE: return a >= b;
        default: return 0;
    }
}

static int compare_doubles(uint8_t op, double a, double b) {
    switch (op) {
        case OP_COMPARE_LT: return a < b;
        case OP_COMPARE_LE: return a <= b;
        case OP_COMPARE_GT: return a > b;
        case OP_COMPARE_GE: return a >= b;
        default: return 0;
    }
}

static int compare_ordering(uint8_t op, int cmp) {
    switch (op) {
        case OP_COMPARE_LT: return cmp < 0;
        case OP_COMPARE_LE: return cmp <= 0;
        case OP_COMPARE_GT: return cmp > 0;
        case OP_COMPARE_GE: return cmp >= 0;
        default: return 0;
    }
}

static int items_equal(Value **a, uint32_t alen, Value **b, uint32_t blen) {
    if (alen != blen) return 0;
    for (uint32_t i = 0; i < alen; i++) {
        if (!value_equal(a[i], b[i])) return 0;
    }
    return 1;
}

static int dicts_equal(const Value *a, const Value *b) {
    if (a->data.dict.len != b->data.dict.len) return 0;
    for (uint32_t i = 0; i < a->data.dict.len; i++) {
        Value *bval = value_dict_get(b, a->data.dict.entries[i].key);
        if (!bval) return 0;
        if (!value_equal(a->data.dict.entries[i].value, bval)) return 0;
    }
    return 1;
}

static int dict_view_contains(const Value *view, Value *item) {
    Value *dict = view->data.view.dict;

    switch (view->data.view.kind) {
        case VIEW_KEYS:
            if (!value_is_hashable(item)) return -1;
            return value_dict_get(dict, item) != NULL;

        case VIEW_VALUES:
            for (uint32_t i = 0; i < dict->data.dict.len; i++) {
                if (value_equal(dict->data.dict.entries[i].value, item)) return 1;
            }
            return 0;

        default: {
            if (item->tag != TAG_TUPLE || item->data.tuple.len != 2) return 0;
            Value *key = item->data.tuple.items[0];
            if (!value_is_hashable(key)) return -1;
            Value *found = value_dict_get(dict, key);
            return found && value_equal(found, item->data.tuple.items[1]);
        }
    }
}

static int is_setlike(const Value *v) {
    return v->tag == TAG_SET || v->tag == TAG_FROZENSET ||
           (v->tag == TAG_DICT_VIEW && v->data.view.kind != VIEW_VALUES);
}

static int setlike_contains(const Value *container, Value *item) {
    if (container->tag == TAG_DICT_VIEW) {
        return dict_view_contains(container, item) == 1;
    }
    return value_is_hashable(item) && value_set_contains(container, item);
}

static int setlike_equal(const Value *a, const Value *b) {
    int64_t len = value_length(a);
    if (len != value_length(b)) return 0;

    for (int64_t i = 0; i < len; i++) {
        Value *item = value_item_at(a, i);
        int found = item && setlike_contains(b, item);
        value_release(item);
        if (!found) return 0;
    }

    return 1;
}

int value_equal(const Value *a, const Value *b) {
    if (a == b) return 1;

    if (is_number(a) && is_number(b)) {
        if (int_like(a) && int_like(b)) {
            return a->data.int_val == b->data.int_val;
        }
        return as_double(a) == as_double(b);
    }

    if (is_setlike(a) && is_setlike(b)) {
        return setlike_equal(a, b);
    }

    if (a->tag != b->tag) return 0;

    switch (a->tag) {
        case TAG_NONE:
            return 1;

        case TAG_STRING:
            return a->data.str.len == b->data.str.len &&
                   (a->data.str.len == 0 ||
                    memcmp(a->data.str.data, b->data.str.data, a->data.str.len) == 0);

        case TAG_BYTES:
            return a->data.bytes.len == b->data.bytes.len &&
                   (a->data.bytes.len == 0 ||
                    memcmp(a->data.bytes.data, b->data.bytes.data, a->data.bytes.len) == 0);

        case TAG_LIST:
            return items_equal(a->data.list.items, a->data.list.len,
                               b->data.list.items, b->data.list.len);

        case TAG_TUPLE:
            return items_equal(a->data.tuple.items, a->data.tuple.len,
                               b->data.tuple.items, b->data.tuple.len);

        case TAG_DICT:
            return dicts_equal(a, b);

        case TAG_RANGE: {
            int64_t la = value_range_len(a->data.range.start, a->data.range.stop, a->data.range.step);
            int64_t lb = value_range_len(b->data.range.start, b->data.range.stop, b->data.range.step);
            if (la != lb) return 0;
            if (la == 0) return 1;
            if (a->data.range.start != b->data.range.start) return 0;
            if (la == 1) return 1;
            return a->data.range.step == b->data.range.step;
        }

        default:
            return 0;
    }
}

static int order_compare(const Value *a, const Value *b, int *ok);

static int order_items(Value **a, uint32_t alen, Value **b, uint32_t blen, int *ok) {
    uint32_t n = alen < blen ? alen : blen;
    for (uint32_t i = 0; i < n; i++) {
        if (value_equal(a[i], b[i])) continue;
        int cmp = order_compare(a[i], b[i], ok);
        if (!*ok) return 0;
        return cmp;
    }
    return alen < blen ? -1 : alen > blen ? 1 : 0;
}

static int order_compare(const Value *a, const Value *b, int *ok) {
    *ok = 1;

    if (is_number(a) && is_number(b)) {
        if (int_like(a) && int_like(b)) {
            int64_t x = a->data.int_val;
            int64_t y = b->data.int_val;
            return x < y ? -1 : x > y ? 1 : 0;
        }
        double x = as_double(a);
        double y = as_double(b);
        return x < y ? -1 : x > y ? 1 : 0;
    }

    if (a->tag != b->tag) {
        *ok = 0;
        return 0;
    }

    switch (a->tag) {
        case TAG_STRING:
        case TAG_BYTES: {
            int cmp = value_compare(a, b);
            return cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
        }

        case TAG_LIST:
            return order_items(a->data.list.items, a->data.list.len,
                               b->data.list.items, b->data.list.len, ok);

        case TAG_TUPLE:
            return order_items(a->data.tuple.items, a->data.tuple.len,
                               b->data.tuple.items, b->data.tuple.len, ok);

        default:
            *ok = 0;
            return 0;
    }
}

int value_order(const Value *a, const Value *b, int *ok) {
    return order_compare(a, b, ok);
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

    if (op == OP_COMPARE_EQ) {
        result = value_equal(a, b);
    } else if (op == OP_COMPARE_NE) {
        result = !value_equal(a, b);
    } else if (int_like(a) && int_like(b)) {
        result = compare_ints(op, a->data.int_val, b->data.int_val);
    } else if (is_number(a) && is_number(b)) {
        result = compare_doubles(op, as_double(a), as_double(b));
    } else if ((a->tag == TAG_STRING && b->tag == TAG_STRING) ||
               (a->tag == TAG_BYTES && b->tag == TAG_BYTES)) {
        result = compare_ordering(op, value_compare(a, b));
    } else if (a->tag == TAG_LIST && b->tag == TAG_LIST) {
        int ok = 1;
        int cmp = order_items(a->data.list.items, a->data.list.len,
                              b->data.list.items, b->data.list.len, &ok);
        if (ok) {
            result = compare_ordering(op, cmp);
        } else {
            vm->last_error = VM_ERR_TYPE;
        }
    } else if (a->tag == TAG_TUPLE && b->tag == TAG_TUPLE) {
        int ok = 1;
        int cmp = order_items(a->data.tuple.items, a->data.tuple.len,
                              b->data.tuple.items, b->data.tuple.len, &ok);
        if (ok) {
            result = compare_ordering(op, cmp);
        } else {
            vm->last_error = VM_ERR_TYPE;
        }
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
                found = value_equal(container->data.list.items[i], item);
            }
            break;

        case TAG_TUPLE:
            for (uint32_t i = 0; i < container->data.tuple.len && !found; i++) {
                found = value_equal(container->data.tuple.items[i], item);
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
            if (!value_is_hashable(item)) {
                vm->last_error = VM_ERR_TYPE;
            } else {
                found = value_dict_get(container, item) != NULL;
            }
            break;

        case TAG_SET:
        case TAG_FROZENSET:
            if (!value_is_hashable(item) && item->tag != TAG_SET) {
                vm->last_error = VM_ERR_TYPE;
            } else {
                found = value_set_contains(container, item);
            }
            break;

        case TAG_DICT_VIEW:
            found = dict_view_contains(container, item);
            if (found < 0) {
                found = 0;
                vm->last_error = VM_ERR_TYPE;
            }
            break;

        case TAG_RANGE: {
            int64_t x = 0;
            int have = 0;
            if (int_like(item)) {
                x = item->data.int_val;
                have = 1;
            } else if (item->tag == TAG_FLOAT) {
                double d = item->data.float_val;
                if (d == (double)(int64_t)d) {
                    x = (int64_t)d;
                    have = 1;
                }
            }
            if (have) {
                int64_t start = container->data.range.start;
                int64_t stop = container->data.range.stop;
                int64_t step = container->data.range.step;
                if (step > 0) {
                    found = x >= start && x < stop && (x - start) % step == 0;
                } else {
                    found = x <= start && x > stop && (x - start) % step == 0;
                }
            }
            break;
        }

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
