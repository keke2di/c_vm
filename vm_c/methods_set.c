#include "vm_internal.h"

static Value *set_add(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    int rc = value_set_add(self, args[0]);
    if (rc != 0) return vm_fail(vm, insert_error(rc));
    return value_new_none();
}

const MethodEntry SET_METHODS[] = {
    {"add", set_add},
};

const uint32_t SET_METHOD_COUNT = (uint32_t)(sizeof(SET_METHODS) / sizeof(SET_METHODS[0]));
