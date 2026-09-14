#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

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

void vm_call_value(VM *vm, uint32_t nargs, const Value *kwnames) {
    uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
    if (vm->stack_top < frame_base || vm->stack_top - frame_base <= nargs) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    uint32_t callable_pos = vm->stack_top - nargs - 1;
    Value *callable = vm->stack[callable_pos];

    int is_func = callable->tag == TAG_FUNCTION && callable->data.func;
    int is_type = callable->tag == TAG_TYPE;

    if (!is_func && !is_type) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    FunctionKind kind = FUNC_USER;
    uint32_t index = 0;
    int type_id = 0;

    if (is_func) {
        kind = callable->data.func->kind;
        index = callable->data.func->index;
    } else {
        type_id = (int)callable->data.int_val;
    }

    if (kwnames && (is_type || kind == FUNC_BUILTIN)) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    memmove(
        &vm->stack[callable_pos],
        &vm->stack[callable_pos + 1],
        nargs * sizeof(Value*));
    vm->stack_top--;
    value_release(callable);

    if (is_type) {
        type_construct(vm, type_id, nargs);
        return;
    }

    if (kind == FUNC_BUILTIN) {
        builtin_invoke(vm, index, nargs);
        return;
    }

    vm_call_user(vm, index, nargs, kwnames);
}

static void release_args(Value **args, uint32_t nargs) {
    for (uint32_t i = 0; i < nargs; i++) {
        if (args[i]) value_release(args[i]);
    }
}

static void call_method(VM *vm, Value *obj, const char *method_name, uint32_t nargs, Value **args) {
    if (obj->tag == TAG_LIST && strcmp(method_name, "append") == 0) {
        if (nargs != 1) {
            vm->last_error = VM_ERR_TYPE;
            release_args(args, nargs);
            return;
        }
        if (value_list_append(obj, args[0]) != 0) {
            vm->last_error = VM_ERR_OOM;
        } else {
            vm_push_owned(vm, value_new_int(0));
        }
        value_release(args[0]);
        return;
    }

    if (obj->tag == TAG_DICT && strcmp(method_name, "get") == 0) {
        if (nargs != 1) {
            vm->last_error = VM_ERR_TYPE;
            release_args(args, nargs);
            return;
        }
        Value *found = value_dict_get(obj, args[0]);
        value_release(args[0]);
        if (found) {
            vm_push(vm, found);
        } else {
            vm_push_owned(vm, value_new_int(0));
        }
        return;
    }

    if (obj->tag == TAG_SET && strcmp(method_name, "add") == 0) {
        if (nargs != 1) {
            vm->last_error = VM_ERR_TYPE;
            release_args(args, nargs);
            return;
        }
        if (value_set_add(obj, args[0]) != 0) {
            vm->last_error = VM_ERR_OOM;
        } else {
            vm_push_owned(vm, value_new_int(0));
        }
        value_release(args[0]);
        return;
    }

    vm->last_error = VM_ERR_TYPE;
    release_args(args, nargs);
}

void vm_call_method(VM *vm, uint32_t nargs) {
    if (nargs > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value **args = NULL;

    if (nargs > 0) {
        args = calloc(nargs, sizeof(Value*));
        if (!args) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        for (uint32_t i = nargs; i > 0; i--) {
            args[i - 1] = vm_pop(vm);
            if (!args[i - 1]) {
                vm->last_error = VM_ERR_STACK;
                break;
            }
        }
        if (vm->last_error != VM_ERR_OK) {
            release_args(args, nargs);
            free(args);
            return;
        }
    }

    Value *method_name = vm_pop(vm);
    if (!method_name) {
        vm->last_error = VM_ERR_STACK;
        release_args(args, nargs);
        free(args);
        return;
    }

    if (method_name->tag != TAG_STRING) {
        vm->last_error = VM_ERR_TYPE;
        value_release(method_name);
        release_args(args, nargs);
        free(args);
        return;
    }

    Value *obj = vm_pop(vm);
    if (!obj) {
        vm->last_error = VM_ERR_STACK;
        value_release(method_name);
        release_args(args, nargs);
        free(args);
        return;
    }

    call_method(vm, obj, method_name->data.str.data, nargs, args);

    value_release(method_name);
    value_release(obj);
    free(args);
}
