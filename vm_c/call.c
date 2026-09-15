#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

#define SMALL_ARGS 8

static int vm_install_callable(VM *vm, uint32_t name_index, FunctionKind kind, uint32_t index) {
    Value *fn = value_new_function(kind, index, vm->names[name_index]);
    if (!fn) {
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }

    if (vm->globals[name_index]) {
        value_release(vm->globals[name_index]);
    }

    vm->globals[name_index] = fn;
    return VM_ERR_OK;
}

int vm_init_callable_globals(VM *vm) {
    uint32_t count = builtin_count();

    for (uint32_t name_index = 0; name_index < vm->num_names; name_index++) {
        for (uint32_t b = 0; b < count; b++) {
            if (strcmp(vm->names[name_index], builtin_name(b)) != 0) {
                continue;
            }
            if (vm_install_callable(vm, name_index, FUNC_BUILTIN, b) != VM_ERR_OK) {
                return vm->last_error;
            }
            break;
        }
    }

    if (vm_install_type_globals(vm) != VM_ERR_OK) {
        return vm->last_error;
    }

    for (uint32_t i = 0; i < vm->num_functions; i++) {
        if (i == vm->entry_func_index) {
            continue;
        }

        uint32_t name_index = vm->functions[i].name_index;
        if (name_index >= vm->num_names) {
            vm->last_error = VM_ERR_BOUNDS;
            return vm->last_error;
        }

        if (vm_install_callable(vm, name_index, FUNC_USER, i) != VM_ERR_OK) {
            return vm->last_error;
        }
    }

    return VM_ERR_OK;
}

static int32_t vm_param_slot(const VM *vm, const FuncEntry *func, const Value *name) {
    if (!name || name->tag != TAG_STRING) {
        return -1;
    }

    for (uint32_t i = 0; i < func->params_count; i++) {
        const char *param = vm->names[func->param_names[i]];
        size_t param_len = strlen(param);

        if (param_len == name->data.str.len &&
            memcmp(param, name->data.str.data, param_len) == 0) {
            return (int32_t)i;
        }
    }

    return -1;
}

static void vm_call_user(VM *vm, uint32_t func_index, uint32_t nargs, const Value *kwnames) {
    if (func_index >= vm->num_functions) {
        vm->last_error = VM_ERR_FUNC_NOT_FOUND;
        return;
    }

    uint32_t call_depth = 0;
    for (Frame *f = vm->current_frame; f != NULL; f = f->prev) {
        call_depth++;
        if (call_depth >= MAX_CALL_DEPTH) {
            vm->last_error = VM_ERR_STACK;
            return;
        }
    }

    FuncEntry *func = &vm->functions[func_index];
    uint32_t nkw = kwnames ? kwnames->data.tuple.len : 0;

    if (nkw > nargs || nargs - nkw > func->params_count) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    uint32_t npos = nargs - nkw;
    uint32_t locals_cap = func->locals_count;
    Value **locals = NULL;

    if (locals_cap > 0) {
        locals = calloc(locals_cap, sizeof(Value*));
        if (!locals) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
    }

    int err = VM_ERR_OK;
    uint32_t base = vm->stack_top - nargs;

    for (uint32_t i = 0; i < npos; i++) {
        locals[i] = value_retain(vm->stack[base + i]);
    }

    for (uint32_t k = 0; err == VM_ERR_OK && k < nkw; k++) {
        int32_t slot = vm_param_slot(vm, func, kwnames->data.tuple.items[k]);
        if (slot < 0 || locals[slot]) {
            err = VM_ERR_TYPE;
        } else {
            locals[slot] = value_retain(vm->stack[base + npos + k]);
        }
    }

    uint32_t first_default = func->params_count - func->defaults_count;

    for (uint32_t i = 0; err == VM_ERR_OK && i < func->params_count; i++) {
        if (locals[i]) {
            continue;
        }
        if (i < first_default) {
            err = VM_ERR_TYPE;
        } else {
            locals[i] = value_retain(vm->constants[func->default_consts[i - first_default]]);
        }
    }

    Frame *new_frame = NULL;

    if (err == VM_ERR_OK) {
        new_frame = malloc(sizeof(Frame));
        if (!new_frame) {
            err = VM_ERR_OOM;
        }
    }

    if (err != VM_ERR_OK) {
        for (uint32_t i = 0; i < locals_cap; i++) {
            if (locals[i]) value_release(locals[i]);
        }
        free(locals);
        vm->last_error = err;
        return;
    }

    while (vm->stack_top > base) {
        Value *arg = vm_pop(vm);
        if (arg) value_release(arg);
    }

    VM_DEBUG(
        "[CALL] frame for %s nargs=%u kw=%u stack=%u\n",
        func->name_index < vm->num_names ? vm->names[func->name_index] : "?",
        nargs,
        nkw,
        vm->stack_top);

    new_frame->prev = vm->current_frame;
    new_frame->return_ip = vm->ip;
    new_frame->func = func;
    new_frame->locals_cap = locals_cap;
    new_frame->locals = locals;
    new_frame->stack_base = vm->stack_top;
    vm->current_frame = new_frame;
    vm->ip = vm->bytecode + func->code_offset;
}

static Value **take_args(VM *vm, uint32_t nargs, Value **small) {
    Value **args = small;

    if (nargs > SMALL_ARGS) {
        args = malloc(nargs * sizeof(Value *));
        if (!args) {
            vm->last_error = VM_ERR_OOM;
            return NULL;
        }
    }

    for (uint32_t i = nargs; i > 0; i--) {
        args[i - 1] = vm_pop(vm);
    }

    return args;
}

static void drop_args(Value **args, uint32_t nargs, Value **small) {
    for (uint32_t i = 0; i < nargs; i++) {
        if (args[i]) value_release(args[i]);
    }
    if (args != small) free(args);
}

static void push_result(VM *vm, Value *result) {
    if (vm->last_error != VM_ERR_OK) {
        if (result) value_release(result);
        return;
    }
    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }
    vm_push_owned(vm, result);
}

void vm_call_value(VM *vm, uint32_t nargs, const Value *kwnames) {
    uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
    if (vm->stack_top < frame_base || vm->stack_top - frame_base <= nargs) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    uint32_t callable_pos = vm->stack_top - nargs - 1;
    Value *callable = vm->stack[callable_pos];
    NativeFn native = NULL;

    if (callable->tag == TAG_FUNCTION && callable->data.func) {
        if (callable->data.func->kind == FUNC_USER) {
            uint32_t index = callable->data.func->index;
            memmove(
                &vm->stack[callable_pos],
                &vm->stack[callable_pos + 1],
                nargs * sizeof(Value*));
            vm->stack_top--;
            value_release(callable);
            vm_call_user(vm, index, nargs, kwnames);
            return;
        }
        native = builtin_function(callable->data.func->index);
    } else if (callable->tag == TAG_TYPE) {
        native = type_constructor((int)callable->data.int_val);
    }

    if (!native) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *small[SMALL_ARGS];
    Value **args = take_args(vm, nargs, small);
    if (!args) return;

    Value *callee = vm_pop(vm);
    Value *result = native(vm, args, nargs, kwnames);

    drop_args(args, nargs, small);
    value_release(callee);
    push_result(vm, result);
}

void vm_call_method(VM *vm, uint32_t nargs, int has_kwnames) {
    uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
    uint64_t needed = (uint64_t)nargs + 2 + (has_kwnames ? 1 : 0);

    if (vm->stack_top < frame_base || (uint64_t)(vm->stack_top - frame_base) < needed) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *kwnames = has_kwnames ? vm_pop(vm) : NULL;
    if (kwnames && (kwnames->tag != TAG_TUPLE || kwnames->data.tuple.len > nargs)) {
        value_release(kwnames);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *small[SMALL_ARGS];
    Value **args = take_args(vm, nargs, small);
    if (!args) {
        if (kwnames) value_release(kwnames);
        return;
    }

    Value *name = vm_pop(vm);
    Value *self = vm_pop(vm);
    MethodFn method = method_lookup(self, name);
    Value *result = NULL;

    if (method) {
        result = method(vm, self, args, nargs, kwnames);
    } else {
        vm->last_error = name->tag == TAG_STRING ? VM_ERR_ATTR : VM_ERR_TYPE;
    }

    drop_args(args, nargs, small);
    value_release(name);
    value_release(self);
    if (kwnames) value_release(kwnames);
    push_result(vm, result);
}

int vm_call_sync(VM *vm, Value *callable, Value **args, uint32_t nargs, Value **out_result) {
    Frame *saved_frame = vm->current_frame;
    uint8_t *saved_ip = vm->ip;

    vm_push(vm, callable);
    for (uint32_t i = 0; i < nargs; i++) {
        vm_push(vm, args[i]);
    }

    vm_call_value(vm, nargs, NULL);
    if (vm->last_error != VM_ERR_OK) {
        vm->ip = saved_ip;
        return -1;
    }

    while (vm->last_error == VM_ERR_OK && vm->current_frame != saved_frame) {
        if (vm->ip >= vm->bytecode + vm->bytecode_len) {
            vm->last_error = VM_ERR_BOUNDS;
            break;
        }
        vm_step(vm);
    }

    vm->ip = saved_ip;

    if (vm->last_error != VM_ERR_OK) {
        return -1;
    }

    *out_result = vm_pop(vm);
    return *out_result ? 0 : -1;
}
