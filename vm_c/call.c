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

    return VM_ERR_OK;
}

static int name_matches(const char *text, const Value *name) {
    size_t len = strlen(text);

    return name && name->tag == TAG_STRING &&
           name->data.str.len == len &&
           memcmp(text, name->data.str.data, len) == 0;
}

static int32_t keyword_slot(const VM *vm, const FuncEntry *func, const Value *name) {
    for (uint32_t i = func->posonly_count; i < func->params_count; i++) {
        if (name_matches(vm->names[func->param_names[i]], name)) {
            return (int32_t)i;
        }
    }

    return -1;
}

static int32_t kwonly_keyword_slot(const VM *vm, const FuncEntry *func, const Value *name) {
    for (uint32_t i = 0; i < func->kwonly_count; i++) {
        if (name_matches(vm->names[func->kwonly_names[i]], name)) {
            return (int32_t)i;
        }
    }

    return -1;
}

static void release_locals(Value **locals, uint32_t count) {
    if (!locals) return;

    for (uint32_t i = 0; i < count; i++) {
        if (locals[i]) value_release(locals[i]);
    }
    free(locals);
}

static void vm_call_user(VM *vm, Function *fn, Value **args, uint32_t nargs, const Value *kwnames) {
    if (!fn || fn->index >= vm->num_functions) {
        vm->last_error = VM_ERR_FUNC_NOT_FOUND;
        return;
    }

    FuncEntry *func = &vm->functions[fn->index];

    uint32_t call_depth = 0;
    for (Frame *f = vm->current_frame; f != NULL; f = f->prev) {
        call_depth++;
        if (call_depth >= MAX_CALL_DEPTH) {
            vm->last_error = VM_ERR_RECURSION;
            return;
        }
    }

    uint32_t nkw = kwnames ? kwnames->data.tuple.len : 0;

    if (nkw > nargs) {
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
    uint32_t positional = func->params_count;
    uint32_t supplied = npos < positional ? npos : positional;

    for (uint32_t i = 0; i < supplied; i++) {
        locals[i] = value_retain(args[i]);
    }

    if (npos > positional && func->vararg_slot == UINT32_MAX) {
        err = VM_ERR_TYPE;
    }

    if (err == VM_ERR_OK && func->vararg_slot != UINT32_MAX) {
        uint32_t extra_count = npos > positional ? npos - positional : 0;
        Value *rest = value_new_tuple(extra_count);

        if (!rest) {
            err = VM_ERR_OOM;
        } else {
            for (uint32_t i = 0; i < extra_count; i++) {
                rest->data.tuple.items[i] = value_retain(args[positional + i]);
            }
            locals[func->vararg_slot] = rest;
        }
    }

    Value *extra = NULL;

    for (uint32_t k = 0; err == VM_ERR_OK && k < nkw; k++) {
        const Value *name = kwnames->data.tuple.items[k];
        Value *value = args[npos + k];

        if (!name || name->tag != TAG_STRING) {
            err = VM_ERR_TYPE;
            break;
        }

        int32_t slot = keyword_slot(vm, func, name);

        if (slot >= 0) {
            if ((uint32_t)slot < supplied) {
                err = VM_ERR_TYPE;
                break;
            }
            locals[slot] = value_retain(value);
            continue;
        }

        int32_t kslot = kwonly_keyword_slot(vm, func, name);

        if (kslot >= 0) {
            uint32_t target = func->kwonly_slots[kslot];
            if (locals[target]) {
                err = VM_ERR_TYPE;
                break;
            }
            locals[target] = value_retain(value);
            continue;
        }

        if (func->kwarg_slot == UINT32_MAX) {
            err = VM_ERR_TYPE;
            break;
        }

        if (!extra) {
            extra = value_new_dict();
            if (!extra) {
                err = VM_ERR_OOM;
                break;
            }
        }

        if (value_dict_set(extra, (Value *)name, value) != 0) {
            err = VM_ERR_TYPE;
            break;
        }
    }

    uint32_t first_default = positional - func->defaults_count;

    for (uint32_t i = 0; err == VM_ERR_OK && i < positional; i++) {
        if (locals[i]) continue;

        if (i < first_default) {
            err = VM_ERR_TYPE;
            break;
        }

        Value *fallback = value_tuple_get(fn->defaults, i - first_default);
        if (!fallback) {
            err = VM_ERR_TYPE;
            break;
        }
        locals[i] = value_retain(fallback);
    }

    for (uint32_t i = 0; err == VM_ERR_OK && i < func->kwonly_count; i++) {
        uint32_t target = func->kwonly_slots[i];

        if (locals[target]) continue;
        if (!func->kwonly_flags[i]) {
            err = VM_ERR_TYPE;
            break;
        }

        Value key;
        memset(&key, 0, sizeof(key));
        key.tag = TAG_STRING;
        key.refcount = UINT32_MAX;
        key.data.str.data = (char *)vm->names[func->kwonly_names[i]];
        key.data.str.len = (uint32_t)strlen(key.data.str.data);

        Value *stored = fn->kwdefaults ? value_dict_get(fn->kwdefaults, &key) : NULL;
        if (!stored) {
            err = VM_ERR_TYPE;
            break;
        }

        locals[target] = value_retain(stored);
    }

    if (err == VM_ERR_OK && func->kwarg_slot != UINT32_MAX) {
        if (!extra) {
            extra = value_new_dict();
            if (!extra) err = VM_ERR_OOM;
        }

        if (err == VM_ERR_OK) {
            locals[func->kwarg_slot] = extra;
            extra = NULL;
        }
    }

    if (extra) value_release(extra);

    uint32_t total_cells = func->cells_count + func->frees_count;
    Value **cells = NULL;

    if (err == VM_ERR_OK && total_cells > 0) {
        cells = calloc(total_cells, sizeof(Value *));
        if (!cells) {
            err = VM_ERR_OOM;
        }
    }

    for (uint32_t i = 0; err == VM_ERR_OK && i < func->cells_count; i++) {
        cells[i] = value_new_cell();
        if (!cells[i]) err = VM_ERR_OOM;
    }

    for (uint32_t i = 0; err == VM_ERR_OK && i < func->cells_count; i++) {
        uint32_t slot = func->cell_slots[i];

        if (slot >= locals_cap) {
            err = VM_ERR_BOUNDS;
            break;
        }

        if (locals[slot]) {
            value_cell_set(cells[i], locals[slot]);
            locals[slot] = NULL;
        }
    }

    for (uint32_t i = 0; err == VM_ERR_OK && i < func->frees_count; i++) {
        if (!fn->cells || i >= fn->ncells || !fn->cells[i]) {
            err = VM_ERR_TYPE;
            break;
        }
        cells[func->cells_count + i] = value_retain(fn->cells[i]);
    }

    Frame *new_frame = NULL;

    if (err == VM_ERR_OK) {
        new_frame = malloc(sizeof(Frame));
        if (!new_frame) {
            err = VM_ERR_OOM;
        }
    }

    if (err != VM_ERR_OK) {
        if (cells) {
            for (uint32_t i = 0; i < total_cells; i++) {
                if (cells[i]) value_release(cells[i]);
            }
            free(cells);
        }
        release_locals(locals, locals_cap);
        vm->last_error = err;
        return;
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
    new_frame->cells = cells;
    new_frame->cells_cap = total_cells;
    new_frame->handlers = NULL;
    new_frame->handler_count = 0;
    new_frame->handler_cap = 0;
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

static void vm_invoke(VM *vm, Value *callable, Value **args, uint32_t nargs, const Value *kwnames) {
    NativeFn native = NULL;

    if (callable && callable->tag == TAG_FUNCTION && callable->data.func) {
        if (callable->data.func->kind == FUNC_USER) {
            vm_call_user(vm, callable->data.func, args, nargs, kwnames);
            return;
        }
        native = builtin_function(callable->data.func->index);
    } else if (callable && callable->tag == TAG_TYPE) {
        int type_id = (int)callable->data.int_val;

        if (exception_index_of(type_id) >= 0) {
            Value *result = construct_exception(vm, (uint32_t)type_id, args, nargs, kwnames);
            push_result(vm, result);
            return;
        }

        native = type_constructor(type_id);
    }

    if (!native) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *result = native(vm, args, nargs, kwnames);
    push_result(vm, result);
}

void vm_call_value(VM *vm, uint32_t nargs, const Value *kwnames) {
    uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
    if (vm->stack_top < frame_base || vm->stack_top - frame_base <= nargs) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    uint32_t callable_pos = vm->stack_top - nargs - 1;
    Value *callable = vm->stack[callable_pos];

    Value *small[SMALL_ARGS];
    Value **args = NULL;

    if (nargs > 0) {
        args = nargs > SMALL_ARGS ? malloc(nargs * sizeof(Value *)) : small;
        if (!args) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        for (uint32_t i = 0; i < nargs; i++) {
            args[i] = vm->stack[callable_pos + 1 + i];
        }
    }

    vm->stack_top = callable_pos;
    vm_invoke(vm, callable, args, nargs, kwnames);

    drop_args(args ? args : small, nargs, small);
    value_release(callable);
}

void vm_call_ex(VM *vm) {
    Value *kwargs = vm_pop(vm);
    Value *args = vm_pop(vm);
    Value *callable = vm_pop(vm);

    if (!callable) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    uint32_t npos = 0;
    uint32_t nkw = 0;

    if (args && args->tag != TAG_NONE) {
        int64_t len = value_length(args);
        if (len < 0) {
            vm->last_error = VM_ERR_TYPE;
            goto done;
        }
        npos = (uint32_t)len;
    }

    if (kwargs && kwargs->tag != TAG_NONE) {
        if (kwargs->tag != TAG_DICT) {
            vm->last_error = VM_ERR_TYPE;
            goto done;
        }
        nkw = kwargs->data.dict.len;
    }

    uint32_t nargs = npos + nkw;
    Value *small[SMALL_ARGS];
    Value **values = NULL;

    if (nargs > 0) {
        values = nargs > SMALL_ARGS ? malloc(nargs * sizeof(Value *)) : small;
        if (!values) {
            vm->last_error = VM_ERR_OOM;
            goto done;
        }
    }

    for (uint32_t i = 0; i < npos; i++) {
        Value *item = value_item_at(args, (int64_t)i);
        if (!item) {
            vm->last_error = VM_ERR_TYPE;
            drop_args(values, nargs, small);
            goto done;
        }
        values[i] = item;
    }

    Value *kwnames = NULL;

    if (nkw > 0) {
        kwnames = value_new_tuple(nkw);
        if (!kwnames) {
            vm->last_error = VM_ERR_OOM;
            drop_args(values, nargs, small);
            goto done;
        }

        for (uint32_t i = 0; i < nkw; i++) {
            Value *key = value_dict_key_at(kwargs, i);
            kwnames->data.tuple.items[i] = value_retain(key);
            values[npos + i] = value_retain(value_dict_get(kwargs, key));
        }
    }

    vm_invoke(vm, callable, values, nargs, kwnames);

    drop_args(values, nargs, small);
    if (kwnames) value_release(kwnames);

done:
    if (args) value_release(args);
    if (kwargs) value_release(kwargs);
    value_release(callable);
}

void vm_make_function(VM *vm, uint32_t func_index) {
    Value *closure = vm_pop(vm);
    Value *kwdefaults = vm_pop(vm);
    Value *defaults = vm_pop(vm);

    if (!closure || !kwdefaults || !defaults) {
        if (closure) value_release(closure);
        if (kwdefaults) value_release(kwdefaults);
        if (defaults) value_release(defaults);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (func_index >= vm->num_functions ||
        closure->tag != TAG_TUPLE ||
        defaults->tag != TAG_TUPLE ||
        (kwdefaults->tag != TAG_DICT && kwdefaults->tag != TAG_NONE)) {
        value_release(closure);
        value_release(kwdefaults);
        value_release(defaults);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    FuncEntry *func = &vm->functions[func_index];

    if (closure->data.tuple.len != func->frees_count ||
        defaults->data.tuple.len != func->defaults_count) {
        value_release(closure);
        value_release(kwdefaults);
        value_release(defaults);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *fn = value_new_function(FUNC_USER, func_index, vm->names[func->name_index]);

    if (!fn) {
        value_release(closure);
        value_release(kwdefaults);
        value_release(defaults);
        vm->last_error = VM_ERR_OOM;
        return;
    }

    fn->data.func->defaults = value_retain(defaults);
    fn->data.func->kwdefaults =
        kwdefaults->tag == TAG_DICT ? value_retain(kwdefaults) : NULL;

    if (func->frees_count > 0) {
        fn->data.func->cells = malloc(func->frees_count * sizeof(Value *));
        if (!fn->data.func->cells) {
            value_release(fn);
            value_release(closure);
            value_release(kwdefaults);
            value_release(defaults);
            vm->last_error = VM_ERR_OOM;
            return;
        }
        fn->data.func->ncells = func->frees_count;
        for (uint32_t i = 0; i < func->frees_count; i++) {
            fn->data.func->cells[i] = value_retain(closure->data.tuple.items[i]);
        }
    }

    value_release(closure);
    value_release(kwdefaults);
    value_release(defaults);
    vm_push_owned(vm, fn);
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

    while (vm->current_frame != saved_frame) {
        if (vm->last_error != VM_ERR_OK) {
            if (!vm_unwind_to(vm, saved_frame)) {
                vm->ip = saved_ip;
                return -1;
            }
            continue;
        }

        if (vm->ip >= vm->bytecode + vm->bytecode_len) {
            vm->last_error = VM_ERR_BOUNDS;
            continue;
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
