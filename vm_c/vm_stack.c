#include <stdlib.h>
#include "vm_internal.h"

void vm_push(VM *vm, Value *v) {
    if (vm->stack_top >= vm->stack_cap) {
        uint32_t new_cap = vm->stack_cap * 2;
        if (new_cap > MAX_STACK_DEPTH) new_cap = MAX_STACK_DEPTH;
        if (vm->stack_top >= new_cap) {
            vm->last_error = VM_ERR_STACK;
            return;
        }
        Value **new_stack = realloc(vm->stack, new_cap * sizeof(Value*));
        if (!new_stack) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        vm->stack = new_stack;
        vm->stack_cap = new_cap;
    }
    vm->stack[vm->stack_top++] = value_retain(v);
}

void vm_push_owned(VM *vm, Value *v) {
    if (!v) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    vm_push(vm, v);
    value_release(v);
}

Value *vm_pop(VM *vm) {
    if (vm->stack_top == 0) {
        vm->last_error = VM_ERR_STACK;
        return NULL;
    }
    return vm->stack[--vm->stack_top];
}
