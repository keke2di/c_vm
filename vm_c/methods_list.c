#include "vm_internal.h"

static Value *list_append(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (value_list_append(self, args[0]) != 0) return vm_fail(vm, VM_ERR_OOM);
    return value_new_none();
}

const MethodEntry LIST_METHODS[] = {
    {"append", list_append},
};

const uint32_t LIST_METHOD_COUNT = (uint32_t)(sizeof(LIST_METHODS) / sizeof(LIST_METHODS[0]));
