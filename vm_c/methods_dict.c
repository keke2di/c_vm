#include "vm_internal.h"

static Value *dict_get(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (!value_is_hashable(args[0])) return vm_fail(vm, VM_ERR_TYPE);

    Value *found = value_dict_get(self, args[0]);
    if (found) return value_retain(found);
    return nargs == 2 ? value_retain(args[1]) : value_new_none();
}

static Value *make_view(VM *vm, Value *self, uint32_t nargs, const Value *kwnames, ViewKind kind) {
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    Value *view = value_new_dict_view(self, kind);
    return view ? view : vm_fail(vm, VM_ERR_OOM);
}

static Value *dict_keys(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return make_view(vm, self, nargs, kwnames, VIEW_KEYS);
}

static Value *dict_values(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return make_view(vm, self, nargs, kwnames, VIEW_VALUES);
}

static Value *dict_items(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return make_view(vm, self, nargs, kwnames, VIEW_ITEMS);
}

const MethodEntry DICT_METHODS[] = {
    {"get", dict_get},
    {"keys", dict_keys},
    {"values", dict_values},
    {"items", dict_items},
};

const uint32_t DICT_METHOD_COUNT = (uint32_t)(sizeof(DICT_METHODS) / sizeof(DICT_METHODS[0]));
