#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static Value *list_append(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (value_list_append(self, args[0]) != 0) return vm_fail(vm, VM_ERR_OOM);
    return value_new_none();
}

static Value *list_extend(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *iter = value_make_iter(vm, args[0]);
    if (!iter) return NULL;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;
        int rc = value_list_append(self, item);
        value_release(item);
        if (rc != 0) {
            vm->last_error = VM_ERR_OOM;
            break;
        }
    }

    value_release(iter);
    if (vm->last_error != VM_ERR_OK) return NULL;
    return value_new_none();
}

static Value *list_insert(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 2) != 0) return NULL;
    if (!int_like(args[0])) return vm_fail(vm, VM_ERR_TYPE);
    if (value_list_insert(self, args[0]->data.int_val, args[1]) != 0) return vm_fail(vm, VM_ERR_OOM);
    return value_new_none();
}

static Value *list_pop(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, 1) != 0) return NULL;

    int64_t len = self->data.list.len;
    if (len == 0) return vm_fail(vm, VM_ERR_BOUNDS);

    int64_t index = -1;
    if (nargs == 1) {
        if (!int_like(args[0])) return vm_fail(vm, VM_ERR_TYPE);
        index = args[0]->data.int_val;
    }
    if (index < 0) index += len;
    if (index < 0 || index >= len) return vm_fail(vm, VM_ERR_BOUNDS);

    return value_list_remove_at(self, index);
}

static Value *list_remove(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    for (uint32_t i = 0; i < self->data.list.len; i++) {
        if (value_equal(self->data.list.items[i], args[0])) {
            Value *removed = value_list_remove_at(self, i);
            value_release(removed);
            return value_new_none();
        }
    }

    return vm_fail(vm, VM_ERR_VALUE);
}

static Value *list_clear(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    while (self->data.list.len > 0) {
        value_release(self->data.list.items[self->data.list.len - 1]);
        self->data.list.len--;
    }
    return value_new_none();
}

static int clamp_start(int64_t start, int64_t len) {
    if (start < 0) {
        start += len;
        if (start < 0) start = 0;
    }
    return (int)start;
}

static Value *list_index(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    int64_t len = self->data.list.len;
    int64_t start = 0;
    int64_t stop = len;

    if (nargs >= 2) {
        if (!int_like(args[1])) return vm_fail(vm, VM_ERR_TYPE);
        start = clamp_start(args[1]->data.int_val, len);
    }
    if (nargs == 3) {
        if (!int_like(args[2])) return vm_fail(vm, VM_ERR_TYPE);
        stop = args[2]->data.int_val;
        if (stop < 0) stop += len;
        if (stop > len) stop = len;
    }

    for (int64_t i = start; i < stop; i++) {
        if (value_equal(self->data.list.items[i], args[0])) {
            return value_new_int(i);
        }
    }

    return vm_fail(vm, VM_ERR_VALUE);
}

static Value *list_count(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    int64_t count = 0;
    for (uint32_t i = 0; i < self->data.list.len; i++) {
        if (value_equal(self->data.list.items[i], args[0])) count++;
    }
    return value_new_int(count);
}

static Value *list_reverse(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;

    Value **items = self->data.list.items;
    uint32_t len = self->data.list.len;
    for (uint32_t i = 0; i < len / 2; i++) {
        Value *t = items[i];
        items[i] = items[len - 1 - i];
        items[len - 1 - i] = t;
    }
    return value_new_none();
}

static Value *list_copy(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;

    Value *out = value_new_list();
    if (!out) return vm_fail(vm, VM_ERR_OOM);

    for (uint32_t i = 0; i < self->data.list.len; i++) {
        if (value_list_append(out, self->data.list.items[i]) != 0) {
            value_release(out);
            return vm_fail(vm, VM_ERR_OOM);
        }
    }
    return out;
}

static const char *const SORT_KEYWORDS[] = { "key", "reverse" };

static Value *list_sort(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (nargs - kw_count(kwnames) != 0) return vm_fail(vm, VM_ERR_TYPE);

    Value *slots[2];
    if (bind_keywords(vm, args, kwnames, SORT_KEYWORDS, 2, slots) != 0) return NULL;
    int reverse = slots[1] ? value_truthy(slots[1]) : 0;

    if (value_list_sort(vm, self, slots[0], reverse) != 0) return NULL;
    return value_new_none();
}

const MethodEntry LIST_METHODS[] = {
    {"append", list_append},
    {"extend", list_extend},
    {"insert", list_insert},
    {"pop", list_pop},
    {"remove", list_remove},
    {"clear", list_clear},
    {"index", list_index},
    {"count", list_count},
    {"reverse", list_reverse},
    {"copy", list_copy},
    {"sort", list_sort},
};

const uint32_t LIST_METHOD_COUNT = (uint32_t)(sizeof(LIST_METHODS) / sizeof(LIST_METHODS[0]));
