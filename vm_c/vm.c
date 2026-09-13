#include "vm.h"
#include "opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#define STACK_INIT_CAP 64
#define MAX_STACK_DEPTH 1024
#define MAX_CALL_DEPTH 256

#ifdef CVM_DEBUG
#define VM_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VM_DEBUG(...) ((void)0)
#endif

static int value_truthy(const Value *v) {
    if (!v) return 0;

    switch (v->tag) {
        case TAG_INT:
            return v->data.int_val != 0;
        case TAG_FLOAT:
            return v->data.float_val != 0.0;
        case TAG_STRING:
            return v->data.str.len != 0;
        case TAG_BYTES:
            return v->data.bytes.len != 0;
        case TAG_LIST:
            return v->data.list.len != 0;
        case TAG_TUPLE:
            return v->data.tuple.len != 0;
        case TAG_DICT:
            return v->data.dict.len != 0;
        case TAG_SET:
            return v->data.set.len != 0;
        case TAG_FUNCTION:
            return 1;
        default:
            return 0;
    }
}

static void vm_push(VM *vm, Value *v) {
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

static void vm_push_owned(VM *vm, Value *v) {
    if (!v) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    vm_push(vm, v);
    value_release(v);
}

static Value *vm_pop(VM *vm) {
    if (vm->stack_top == 0) {
        vm->last_error = VM_ERR_STACK;
        return NULL;
    }
    return vm->stack[--vm->stack_top];
}

static void builtin_print(VM *vm, uint32_t nargs) {
    uint32_t start = vm->stack_top - nargs;

    for (uint32_t i = 0; i < nargs; i++) {
        char *s = value_to_string(vm->stack[start + i]);
        if (!s) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        if (i > 0) printf(" ");
        printf("%s", s);
        free(s);
    }
    printf("\n");

    while (vm->stack_top > start) {
        Value *arg = vm_pop(vm);
        if (arg) value_release(arg);
    }

    vm_push_owned(vm, value_new_none());
}

static void builtin_int(VM *vm, uint32_t nargs) {
    if (nargs == 0) {
        vm_push_owned(vm, value_new_int(0));
        return;
    }
    if (nargs != 1) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }
    Value *arg = vm_pop(vm);
    if (!arg) return;
    int64_t result = 0;
    if (arg->tag == TAG_INT) {
        result = arg->data.int_val;
    } else if (arg->tag == TAG_STRING) {
        char *end;
        long long val = strtoll(arg->data.str.data, &end, 10);
        if (*end == '\0') {
            result = val;
        } else {
            vm->last_error = VM_ERR_TYPE;
        }
    } else if (arg->tag == TAG_FLOAT) {
        result = (int64_t)arg->data.float_val;
    } else {
        vm->last_error = VM_ERR_TYPE;
    }
    value_release(arg);
    if (vm->last_error == VM_ERR_OK) {
        vm_push_owned(vm, value_new_int(result));
    }
}

static void builtin_str(VM *vm, uint32_t nargs) {

    if (nargs != 1) {

        vm->last_error = VM_ERR_TYPE;

        return;

    }

    Value *arg = vm_pop(vm);

    if (!arg) return;

    char *s = value_to_string(arg);

    if (!s) {

        vm->last_error = VM_ERR_OOM;

        value_release(arg);

        return;

    }

    size_t str_len = (arg->tag == TAG_STRING)
        ? value_string_len(arg)
        : strlen(s);

    value_release(arg);

    Value *str_val = value_new_string_len(s, str_len);

    free(s);

    if (!str_val) {

        vm->last_error = VM_ERR_OOM;

        return;

    }

    vm_push_owned(vm, str_val);

}

static void builtin_float(VM *vm, uint32_t nargs) {
    if (nargs == 0) {
        vm_push_owned(vm, value_new_float(0.0));
        return;
    }
    if (nargs != 1) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }
    Value *arg = vm_pop(vm);
    if (!arg) return;
    double result = 0.0;
    if (arg->tag == TAG_FLOAT) {
        result = arg->data.float_val;
    } else if (arg->tag == TAG_INT) {
        result = (double)arg->data.int_val;
    } else if (arg->tag == TAG_STRING) {
        char *end;
        double val = strtod(arg->data.str.data, &end);
        if (*end == '\0') {
            result = val;
        } else {
            vm->last_error = VM_ERR_TYPE;
        }
    } else {
        vm->last_error = VM_ERR_TYPE;
    }
    value_release(arg);
    if (vm->last_error == VM_ERR_OK) {
        vm_push_owned(vm, value_new_float(result));
    }
}
static void builtin_list(VM *vm, uint32_t nargs) {
    if (nargs > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *list = value_new_list();
    if (!list) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t start = vm->stack_top - nargs;

    for (uint32_t i = 0; i < nargs; i++) {
        Value *item = vm->stack[start + i];

        if (value_list_append(list, item) != 0) {
            value_release(list);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        value_release(item);
    }

    vm->stack_top -= nargs;
    vm_push_owned(vm, list);
}


static void builtin_append(VM *vm, uint32_t nargs) {
    if (nargs != 2 || vm->stack_top < 2) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *item = vm_pop(vm);
    Value *list = vm_pop(vm);

    if (!item || !list) {
        if (item) value_release(item);
        if (list) value_release(list);
        return;
    }

    if (list->tag != TAG_LIST) {
        value_release(item);
        value_release(list);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    if (value_list_append(list, item) != 0) {
        value_release(item);
        value_release(list);
        vm->last_error = VM_ERR_OOM;
        return;
    }

    value_release(item);
    vm_push_owned(vm, value_new_int(0));
    value_release(list);
}

static void builtin_enumerate(VM *vm, uint32_t nargs) {
    if (nargs != 1) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *iterable = vm_pop(vm);
    if (!iterable) return;

    if (iterable->tag != TAG_LIST) {
        value_release(iterable);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *result = value_new_list();
    if (!result) {
        value_release(iterable);
        vm->last_error = VM_ERR_OOM;
        return;
    }

    for (uint32_t i = 0; i < iterable->data.list.len; i++) {
        Value *index = value_new_int((int64_t)i);
        Value *item = iterable->data.list.items[i];

        if (!index) {
            value_release(result);
            value_release(iterable);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        Value *pair = value_new_list();
        if (!pair) {
            value_release(index);
            value_release(result);
            value_release(iterable);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        if (value_list_append(pair, index) != 0 ||
            value_list_append(pair, item) != 0 ||
            value_list_append(result, pair) != 0) {
            value_release(index);
            value_release(pair);
            value_release(result);
            value_release(iterable);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        value_release(index);
        value_release(pair);
    }

    value_release(iterable);
    vm_push_owned(vm, result);
}

typedef void (*BuiltinHandler)(VM *vm, uint32_t nargs);

typedef struct {
    const char *name;
    BuiltinHandler handler;
} BuiltinEntry;

static const BuiltinEntry BUILTINS[] = {
    {"print", builtin_print},
    {"list", builtin_list},
    {"append", builtin_append},
    {"enumerate", builtin_enumerate},
    {"int", builtin_int},
    {"str", builtin_str},
    {"float", builtin_float},
};

#define BUILTIN_COUNT ((uint32_t)(sizeof(BUILTINS) / sizeof(BUILTINS[0])))

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

static int vm_init_callable_globals(VM *vm) {
    for (uint32_t name_index = 0; name_index < vm->num_names; name_index++) {
        for (uint32_t b = 0; b < BUILTIN_COUNT; b++) {
            if (strcmp(vm->names[name_index], BUILTINS[b].name) != 0) {
                continue;
            }
            if (vm_install_callable(vm, name_index, FUNC_BUILTIN, b) != VM_ERR_OK) {
                return vm->last_error;
            }
            break;
        }
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

static void vm_call_value(VM *vm, uint32_t nargs, const Value *kwnames) {
    uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
    if (vm->stack_top < frame_base || vm->stack_top - frame_base <= nargs) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    uint32_t callable_pos = vm->stack_top - nargs - 1;
    Value *callable = vm->stack[callable_pos];

    if (callable->tag != TAG_FUNCTION || !callable->data.func) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    FunctionKind kind = callable->data.func->kind;
    uint32_t index = callable->data.func->index;

    if (kind == FUNC_BUILTIN && kwnames) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    memmove(
        &vm->stack[callable_pos],
        &vm->stack[callable_pos + 1],
        nargs * sizeof(Value*));
    vm->stack_top--;
    value_release(callable);

    VM_DEBUG(
        "[CALL] kind=%d index=%u nargs=%u stack=%u\n",
        (int)kind,
        index,
        nargs,
        vm->stack_top);

    if (kind == FUNC_BUILTIN) {
        if (index >= BUILTIN_COUNT) {
            vm->last_error = VM_ERR_FUNC_NOT_FOUND;
            return;
        }
        BUILTINS[index].handler(vm, nargs);
        return;
    }

    vm_call_user(vm, index, nargs, kwnames);
}

static void call_method(VM *vm, Value *obj, Value *method_name_val, uint32_t nargs, Value **args) {
    if (method_name_val->tag != TAG_STRING) {
        vm->last_error = VM_ERR_TYPE;
        for (uint32_t i = 0; i < nargs; i++) {
            if (args[i]) value_release(args[i]);
        }
        return;
    }
    const char *method_name = method_name_val->data.str.data;

    if (obj->tag == TAG_LIST && strcmp(method_name, "append") == 0) {
        if (nargs != 1) {
            vm->last_error = VM_ERR_TYPE;
            for (uint32_t i = 0; i < nargs; i++) {
                if (args[i]) value_release(args[i]);
            }
            return;
        }
        Value *arg = args[0];
       if (value_list_append(obj, arg) != 0) {
            vm->last_error = VM_ERR_OOM;
            value_release(arg);
            return;
        }
        value_release(arg);
        vm_push_owned(vm, value_new_int(0));
        return;
    }
    if (obj->tag == TAG_DICT && strcmp(method_name, "get") == 0) {
        if (nargs != 1) {
            vm->last_error = VM_ERR_TYPE;
            for (uint32_t i = 0; i < nargs; i++) {
                if (args[i]) value_release(args[i]);
            }
            return;
        }
        Value *key = args[0];
        Value *found = value_dict_get(obj, key);
        value_release(key);
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
            for (uint32_t i = 0; i < nargs; i++) {
                if (args[i]) value_release(args[i]);
            }
            return;
        }
        Value *arg = args[0];
        if (value_set_add(obj, arg) != 0) {
            vm->last_error = VM_ERR_OOM;
            value_release(arg);
            return;
        }
        value_release(arg);
        vm_push_owned(vm, value_new_int(0));
        return;
    }
    vm->last_error = VM_ERR_TYPE;
    for (uint32_t i = 0; i < nargs; i++) {
        if (args[i]) value_release(args[i]);
    }
}

int vm_run(VM *vm) {
    if (!vm || vm->entry_func_index >= vm->num_functions) {
        vm->last_error = VM_ERR_FUNC_NOT_FOUND;
        return vm->last_error;
    }

    FuncEntry *entry_func = &vm->functions[vm->entry_func_index];
    vm->ip = vm->bytecode + entry_func->code_offset;

    vm->globals = calloc(vm->num_names, sizeof(Value*));
    if (vm->num_names > 0 && !vm->globals) {
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }

    if (vm_init_callable_globals(vm) != VM_ERR_OK) {
        return vm->last_error;
    }

    vm->stack_cap = STACK_INIT_CAP;
    vm->stack = malloc(vm->stack_cap * sizeof(Value*));
    if (!vm->stack) {
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }
    vm->stack_top = 0;

    Frame *frame = malloc(sizeof(Frame));
    if (!frame) {
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }
    frame->prev = NULL;
    frame->return_ip = NULL;
    frame->func = entry_func;
    frame->locals_cap = entry_func->locals_count;
    frame->locals = NULL;

    if (frame->locals_cap > 0) {
        frame->locals = calloc(frame->locals_cap, sizeof(Value*));
    }

    frame->stack_base = vm->stack_top;

    if (frame->locals_cap > 0 && !frame->locals) {
        free(frame);
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }
    vm->current_frame = frame;

    while (1) {
        if (vm->last_error != VM_ERR_OK) break;

        if (vm->ip >= vm->bytecode + vm->bytecode_len) {
            vm->last_error = VM_ERR_BOUNDS;
            break;
        }

        uint8_t op = *vm->ip++;

        switch (op) {
            case OP_LOAD_CONST: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) { vm->last_error = VM_ERR_BOUNDS; break; }
                uint32_t idx;
                memcpy(&idx, vm->ip, 4); vm->ip += 4;
                if (idx >= vm->num_constants) { vm->last_error = VM_ERR_BOUNDS; break; }
                vm_push(vm, vm->constants[idx]);
                break;
            }
            case OP_LOAD_FAST: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t idx;
                memcpy(&idx, vm->ip, 4);
                vm->ip += 4;

                if (!vm->current_frame) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                if (idx >= vm->current_frame->locals_cap) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                Value *local = vm->current_frame->locals[idx];

                VM_DEBUG(
                    "[LOAD_FAST] frame=%p idx=%u local=%p stack_before=%u\n",
                    (void *)vm->current_frame,
                    idx,
                    (void *)local,
                    vm->stack_top);

                if (!local) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                vm_push(vm, local);

                VM_DEBUG(
                    "[LOAD_FAST] stack_after=%u error=%d\n",
                    vm->stack_top,
                    vm->last_error);

                break;
            }

            case OP_STORE_FAST: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) { vm->last_error = VM_ERR_BOUNDS; break; }
                uint32_t idx;
                memcpy(&idx, vm->ip, 4); vm->ip += 4;
                if (idx >= vm->current_frame->locals_cap) { vm->last_error = VM_ERR_BOUNDS; break; }
                Value *v = vm_pop(vm);
                if (!v) break;
                if (vm->current_frame->locals[idx]) {
                    value_release(vm->current_frame->locals[idx]);
                }
                vm->current_frame->locals[idx] = v;
                break;
            }
            case OP_BINARY_ADD: {
                VM_DEBUG(
                    "[ADD] ENTER stack=%u frame=%p base=%u\n",
                    vm->stack_top,
                    (void *)vm->current_frame,
                    vm->current_frame ? vm->current_frame->stack_base : 0);

                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);

                VM_DEBUG(
                    "[ADD] AFTER POP stack=%u a=%p b=%p\n",
                    vm->stack_top,
                    (void *)a,
                    (void *)b);

                if (!a || !b) {
                    vm->last_error = VM_ERR_STACK;
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }

                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    int64_t ai = a->data.int_val;
                    int64_t bi = b->data.int_val;

                    if ((bi > 0 && ai > INT64_MAX - bi) ||
                        (bi < 0 && ai < INT64_MIN - bi)) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        Value *result = value_new_int(ai + bi);
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);

                            VM_DEBUG(
                                "[ADD] INT %lld + %lld -> stack=%u\n",
                                (long long)ai,
                                (long long)bi,
                                vm->stack_top);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                        (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {

                    double af = (a->tag == TAG_INT)
                        ? (double)a->data.int_val
                        : a->data.float_val;

                 double bf = (b->tag == TAG_INT)
                        ? (double)b->data.int_val
                        : b->data.float_val;

                    Value *result = value_new_float(af + bf);

                    if (!result) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        vm_push_owned(vm, result);

                        VM_DEBUG(
                            "[ADD] FLOAT -> stack=%u\n",
                            vm->stack_top);
                    }
                } else if (a->tag == TAG_STRING || b->tag == TAG_STRING) {
                    const char *sa = (a->tag == TAG_STRING) ? a->data.str.data : NULL;
                    const char *sb = (b->tag == TAG_STRING) ? b->data.str.data : NULL;

                    char a_buf[64], b_buf[64];

                    if (!sa) {
                        if (a->tag == TAG_FLOAT)
                            snprintf(a_buf, sizeof(a_buf), "%g", a->data.float_val);
                        else
                            snprintf(a_buf, sizeof(a_buf), "%lld",
                                (long long)a->data.int_val);
                        sa = a_buf;
                    }

                    if (!sb) {
                        if (b->tag == TAG_FLOAT)
                            snprintf(b_buf, sizeof(b_buf), "%g", b->data.float_val);
                        else
                            snprintf(b_buf, sizeof(b_buf), "%lld",
                                (long long)b->data.int_val);
                        sb = b_buf;
                    }

                    size_t sa_len = (a->tag == TAG_STRING)
                        ? value_string_len(a)
                        : strlen(sa);
                    size_t sb_len = (b->tag == TAG_STRING)
                        ? value_string_len(b)
                        : strlen(sb);

                    char *result = malloc(sa_len + sb_len + 1);

                    if (!result) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        memcpy(result, sa, sa_len);
                        memcpy(result + sa_len, sb, sb_len);
                        result[sa_len + sb_len] = '\0';

                        Value *str_result = value_new_string_len(
                            result,
                            sa_len + sb_len
                        );
                        free(result);

                        if (!str_result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, str_result);
                        }
                    }
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(a);
                value_release(b);
                break;
            }

            case OP_BINARY_SUB: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);
                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }
                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    int64_t ai = a->data.int_val;
                    int64_t bi = b->data.int_val;

                    if ((bi < 0 && ai > INT64_MAX + bi) ||
                        (bi > 0 && ai < INT64_MIN + bi)) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        Value *result = value_new_int(ai - bi);
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT) ? (double)a->data.int_val : a->data.float_val;
                    double bf = (b->tag == TAG_INT) ? (double)b->data.int_val : b->data.float_val;
                    vm_push_owned(vm, value_new_float(af - bf));
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }
                value_release(a);
                value_release(b);
                break;
            }
            case OP_BINARY_MUL: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);
                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }
                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    int64_t ai = a->data.int_val;
                    int64_t bi = b->data.int_val;
                    int overflow = 0;

                    if (ai != 0 && bi != 0) {
                        if (ai == INT64_MIN) {
                            overflow = (bi != 1);
                        } else if (bi == INT64_MIN) {
                            overflow = (ai != 1);
                        } else if (ai > 0) {
                            if (bi > 0) {
                                overflow = ai > INT64_MAX / bi;
                            } else {
                                overflow = bi < INT64_MIN / ai;
                            }
                        } else {
                            if (bi > 0) {
                                overflow = ai < INT64_MIN / bi;
                            } else {
                                overflow = ai < INT64_MAX / bi;
                            }
                        }
                    }

                    if (overflow) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        Value *result = value_new_int(ai * bi);
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT) ? (double)a->data.int_val : a->data.float_val;
                    double bf = (b->tag == TAG_INT) ? (double)b->data.int_val : b->data.float_val;
                    vm_push_owned(vm, value_new_float(af * bf));
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }
                value_release(a);
                value_release(b);
                break;
            }
            case OP_BINARY_DIV: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);
                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }
                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    if (b->data.int_val == 0) {
                        vm->last_error = VM_ERR_DIV_ZERO;
                    } else if (a->data.int_val == INT64_MIN &&
                               b->data.int_val == -1) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        Value *result = value_new_int(
                            a->data.int_val / b->data.int_val
                        );
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT) ? (double)a->data.int_val : a->data.float_val;
                    double bf = (b->tag == TAG_INT) ? (double)b->data.int_val : b->data.float_val;

                    if (bf == 0.0) {
                        vm->last_error = VM_ERR_DIV_ZERO;
                    } else {
                        vm_push_owned(vm, value_new_float(af / bf));
                    }
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(a);
                value_release(b);
                break;
            }

            case OP_BINARY_MOD: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);
                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }

                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    if (b->data.int_val == 0) {
                        vm->last_error = VM_ERR_DIV_ZERO;
                    } else if (a->data.int_val == INT64_MIN &&
                               b->data.int_val == -1) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        int64_t remainder = a->data.int_val % b->data.int_val;
                        if (remainder != 0 && (remainder < 0) != (b->data.int_val < 0)) {
                            remainder += b->data.int_val;
                        }
                        Value *result = value_new_int(remainder);
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT) ? (double)a->data.int_val : a->data.float_val;
                    double bf = (b->tag == TAG_INT) ? (double)b->data.int_val : b->data.float_val;
                    if (bf == 0.0) {
                        vm->last_error = VM_ERR_DIV_ZERO;
                    } else {
                        double remainder = fmod(af, bf);
                        if (remainder == 0.0) {
                            remainder = copysign(0.0, bf);
                        } else if ((remainder < 0.0) != (bf < 0.0)) {
                            remainder += bf;
                        }
                        vm_push_owned(vm, value_new_float(remainder));
                    }
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(a);
                value_release(b);
                break;
            }

            case OP_BINARY_FLOORDIV: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);
                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }

                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    if (b->data.int_val == 0) {
                        vm->last_error = VM_ERR_DIV_ZERO;
                    } else if (a->data.int_val == INT64_MIN &&
                               b->data.int_val == -1) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        int64_t q = a->data.int_val / b->data.int_val;
                        int64_t r = a->data.int_val % b->data.int_val;
                        if (r != 0 && ((r > 0) != (b->data.int_val > 0))) {
                            q--;
                        }

                        Value *result = value_new_int(q);
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT) ? (double)a->data.int_val : a->data.float_val;
                    double bf = (b->tag == TAG_INT) ? (double)b->data.int_val : b->data.float_val;
                    if (bf == 0.0) {
                        vm->last_error = VM_ERR_DIV_ZERO;
                    } else {
                        vm_push_owned(vm, value_new_float(floor(af / bf)));
                    }
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(a);
                value_release(b);
                break;
            }

            case OP_BINARY_POW: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);
                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    break;
                }

                if (a->tag == TAG_INT && b->tag == TAG_INT && b->data.int_val >= 0) {
                    int64_t base = a->data.int_val;
                    int64_t exp = b->data.int_val;
                    int64_t result = 1;
                    int overflow = 0;

                    while (exp > 0 && !overflow) {
                        if (exp & 1) {
                            int64_t ai = result;
                            int64_t bi = base;
                            int product_overflow = 0;

                            if (ai != 0 && bi != 0) {
                                if (ai == INT64_MIN) {
                                    product_overflow = (bi != 1);
                                } else if (bi == INT64_MIN) {
                                    product_overflow = (ai != 1);
                                } else if (ai > 0) {
                                    if (bi > 0) {
                                        product_overflow = ai > INT64_MAX / bi;
                                    } else {
                                        product_overflow = bi < INT64_MIN / ai;
                                    }
                                } else {
                                    if (bi > 0) {
                                        product_overflow = ai < INT64_MIN / bi;
                                    } else {
                                        product_overflow = ai < INT64_MAX / bi;
                                    }
                                }
                            }

                            if (product_overflow) {
                                overflow = 1;
                            } else {
                                result = ai * bi;
                            }
                        }

                        exp >>= 1;

                        if (exp > 0 && !overflow) {
                            if (base == INT64_MIN) {
                                overflow = 1;
                            } else if (base > 0) {
                                if (base > INT64_MAX / base) {
                                    overflow = 1;
                                } else {
                                    base *= base;
                                }
                            } else {
                                if (base < INT64_MAX / base) {
                                    overflow = 1;
                                } else {
                                    base *= base;
                                }
                            }
                        }
                    }

                    if (overflow) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        Value *value = value_new_int(result);
                        if (!value) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, value);
                        }
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT) ? (double)a->data.int_val : a->data.float_val;
                    double bf = (b->tag == TAG_INT) ? (double)b->data.int_val : b->data.float_val;
                    vm_push_owned(vm, value_new_float(pow(af, bf)));
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(a);
                value_release(b);
                break;
            }

            case OP_UNARY_POS: {
                Value *v = vm_pop(vm);
                if (!v) break;

                if (v->tag == TAG_INT) {
                    vm_push_owned(vm, value_new_int(v->data.int_val));
                } else if (v->tag == TAG_FLOAT) {
                    vm_push_owned(vm, value_new_float(v->data.float_val));
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(v);
                break;
            }

            case OP_UNARY_INVERT: {
                Value *v = vm_pop(vm);
                if (!v) break;

                if (v->tag == TAG_INT) {
                    vm_push_owned(vm, value_new_int(~v->data.int_val));
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(v);
                break;
            }

            case OP_UNARY_NEG: {
                Value *v = vm_pop(vm);
                if (!v) break;
                if (v->tag == TAG_INT) {
                    if (v->data.int_val == INT64_MIN) {
                        vm->last_error = VM_ERR_OVERFLOW;
                    } else {
                        Value *result = value_new_int(-v->data.int_val);
                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else if (v->tag == TAG_FLOAT) {
                    vm_push_owned(vm, value_new_float(-v->data.float_val));
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }
                value_release(v);
                break;
            }
            case OP_UNARY_NOT: {
                Value *v = vm_pop(vm);
                if (!v) break;

                int truth = value_truthy(v);
                value_release(v);

                Value *result = value_new_int(truth ? 0 : 1);
                if (!result) {
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                vm_push_owned(vm, result);
                break;
            }

            case OP_COMPARE_EQ:
            case OP_COMPARE_NE:
            case OP_COMPARE_LT:
            case OP_COMPARE_LE:
            case OP_COMPARE_GT:
            case OP_COMPARE_GE: {
                Value *b = vm_pop(vm);
                Value *a = vm_pop(vm);

                if (!a || !b) {
                    if (a) value_release(a);
                    if (b) value_release(b);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                int result = 0;

                if (a->tag == TAG_INT && b->tag == TAG_INT) {
                    int64_t ai = a->data.int_val;
                    int64_t bi = b->data.int_val;

                    switch (op) {
                        case OP_COMPARE_EQ: result = (ai == bi); break;
                        case OP_COMPARE_NE: result = (ai != bi); break;
                        case OP_COMPARE_LT: result = (ai < bi); break;
                        case OP_COMPARE_LE: result = (ai <= bi); break;
                        case OP_COMPARE_GT: result = (ai > bi); break;
                        case OP_COMPARE_GE: result = (ai >= bi); break;
                    }
                } else if ((a->tag == TAG_INT || a->tag == TAG_FLOAT) &&
                           (b->tag == TAG_INT || b->tag == TAG_FLOAT)) {
                    double af = (a->tag == TAG_INT)
                        ? (double)a->data.int_val
                        : a->data.float_val;

                    double bf = (b->tag == TAG_INT)
                        ? (double)b->data.int_val
                        : b->data.float_val;

                    switch (op) {
                        case OP_COMPARE_EQ: result = (af == bf); break;
                        case OP_COMPARE_NE: result = (af != bf); break;
                        case OP_COMPARE_LT: result = (af < bf); break;
                        case OP_COMPARE_LE: result = (af <= bf); break;
                        case OP_COMPARE_GT: result = (af > bf); break;
                        case OP_COMPARE_GE: result = (af >= bf); break;
                    }
                } else if (a->tag == TAG_STRING && b->tag == TAG_STRING) {
                    int cmp = value_compare(a, b);

                    switch (op) {
                        case OP_COMPARE_EQ: result = (cmp == 0); break;
                        case OP_COMPARE_NE: result = (cmp != 0); break;
                        case OP_COMPARE_LT: result = (cmp < 0); break;
                        case OP_COMPARE_LE: result = (cmp <= 0); break;
                        case OP_COMPARE_GT: result = (cmp > 0); break;
                        case OP_COMPARE_GE: result = (cmp >= 0); break;
                    }
                } else if (a->tag == TAG_BYTES && b->tag == TAG_BYTES) {
                    size_t a_len = a->data.bytes.len;
                    size_t b_len = b->data.bytes.len;
                    size_t min_len = a_len < b_len ? a_len : b_len;

                    int cmp = memcmp(
                        a->data.bytes.data,
                        b->data.bytes.data,
                        min_len
                    );

                    if (cmp == 0) {
                        if (a_len < b_len) {
                            cmp = -1;
                        } else if (a_len > b_len) {
                            cmp = 1;
                        }
                    }

                    switch (op) {
                        case OP_COMPARE_EQ: result = (cmp == 0); break;
                        case OP_COMPARE_NE: result = (cmp != 0); break;
                        case OP_COMPARE_LT: result = (cmp < 0); break;
                        case OP_COMPARE_LE: result = (cmp <= 0); break;
                        case OP_COMPARE_GT: result = (cmp > 0); break;
                        case OP_COMPARE_GE: result = (cmp >= 0); break;
                    }
                } else if (a->tag == TAG_TUPLE && b->tag == TAG_TUPLE) {
                    if (op == OP_COMPARE_EQ || op == OP_COMPARE_NE) {
                        if (a->data.tuple.len != b->data.tuple.len) {
                            result = (op == OP_COMPARE_NE);
                        } else {
                            result = 1;

                            for (uint32_t i = 0; i < a->data.tuple.len; i++) {
                                if (value_compare(
                                        a->data.tuple.items[i],
                                        b->data.tuple.items[i]) != 0) {
                                    result = 0;
                                    break;
                                }
                            }

                            if (op == OP_COMPARE_NE) {
                                result = !result;
                            }
                        }
                    } else {
                        vm->last_error = VM_ERR_TYPE;
                    }
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                if (vm->last_error == VM_ERR_OK) {
                    Value *result_val = value_new_int(result);

                    if (!result_val) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        vm_push_owned(vm, result_val);
                    }
                }

                value_release(a);
                value_release(b);
                break;
            }
            case OP_CONTAINS: {
                Value *container = vm_pop(vm);
                Value *item = vm_pop(vm);

                if (!item || !container) {
                    vm->last_error = VM_ERR_STACK;
                    if (item) value_release(item);
                    if (container) value_release(container);
                    break;
                }

                int found = 0;

                if (container->tag == TAG_LIST) {
                    for (uint32_t i = 0; i < container->data.list.len; i++) {
                        Value *elem = container->data.list.items[i];

                        if (value_compare(elem, item) == 0) {
                            found = 1;
                            break;
                        }
                    }
                } else if (container->tag == TAG_TUPLE) {
                    for (uint32_t i = 0; i < container->data.tuple.len; i++) {
                        Value *elem = container->data.tuple.items[i];

                        if (value_compare(elem, item) == 0) {
                            found = 1;
                            break;
                        }
                    }
                } else if (container->tag == TAG_BYTES) {
                    if (item->tag != TAG_INT) {
                        vm->last_error = VM_ERR_TYPE;
                    } else {
                        int64_t needle = item->data.int_val;

                        if (needle >= 0 && needle <= 255) {
                            for (uint32_t i = 0; i < container->data.bytes.len; i++) {
                                if (container->data.bytes.data[i] == (unsigned char)needle) {
                                    found = 1;
                                    break;
                                }
                            }
                        }
                    }
                } else if (container->tag == TAG_STRING) {
                    if (item->tag != TAG_STRING) {
                        vm->last_error = VM_ERR_TYPE;
                    } else {
                        size_t haystack_len = container->data.str.len;
                        size_t needle_len = item->data.str.len;

                        if (needle_len == 0) {
                            found = 1;
                        } else if (needle_len <= haystack_len) {
                            for (size_t i = 0; i + needle_len <= haystack_len; i++) {
                                if (memcmp(
                                        container->data.str.data + i,
                                        item->data.str.data,
                                        needle_len
                                    ) == 0) {
                                    found = 1;
                                    break;
                                }
                            }
                        }
                    }
                } else if (container->tag == TAG_DICT) {
                    Value *found_val = value_dict_get(container, item);

                    if (found_val) {
                        found = 1;
                    }
                } else if (container->tag == TAG_SET) {
                    if (value_set_contains(container, item)) {
                        found = 1;
                    }
                } else {
                    vm->last_error = VM_ERR_TYPE;
                }

                value_release(item);
                value_release(container);

                if (vm->last_error == VM_ERR_OK) {
                    Value *result = value_new_int(found);

                    if (!result) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        vm_push_owned(vm, result);
                    }
                }

                break;
            }
            case OP_JUMP: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t target;
                memcpy(&target, vm->ip, 4);
                vm->ip += 4;

                if (!vm->current_frame || !vm->current_frame->func) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                size_t func_base = vm->current_frame->func->code_offset;
                size_t absolute_target = func_base + target;

                if (absolute_target >= vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                vm->ip = vm->bytecode + absolute_target;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t target;
                memcpy(&target, vm->ip, 4);
                vm->ip += 4;

                Value *cond = vm_pop(vm);
                if (!cond) break;

                int truth = value_truthy(cond);
                value_release(cond);

                if (!truth) {
                    if (!vm->current_frame || !vm->current_frame->func) {
                        vm->last_error = VM_ERR_STACK;
                        break;
                    }

                    size_t func_base = vm->current_frame->func->code_offset;
                    size_t absolute_target = func_base + target;

                    if (absolute_target >= vm->bytecode_len) {
                        vm->last_error = VM_ERR_BOUNDS;
                        break;
                    }

                    vm->ip = vm->bytecode + absolute_target;
                }

                break;
            }
            case OP_JUMP_IF_TRUE: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t target;
                memcpy(&target, vm->ip, 4);
                vm->ip += 4;

                Value *cond = vm_pop(vm);
                if (!cond) break;

                int truth = value_truthy(cond);
                value_release(cond);

                if (truth) {
                    if (!vm->current_frame || !vm->current_frame->func) {
                        vm->last_error = VM_ERR_STACK;
                        break;
                    }

                    size_t func_base = vm->current_frame->func->code_offset;
                    size_t absolute_target = func_base + target;

                    if (absolute_target >= vm->bytecode_len) {
                        vm->last_error = VM_ERR_BOUNDS;
                        break;
                    }

                    vm->ip = vm->bytecode + absolute_target;
                }

                break;
            }
            case OP_POP_TOP: {
                Value *v = vm_pop(vm);
                if (v) value_release(v);
                break;
            }
            case OP_DUP_TOP: {
                if (vm->stack_top == 0) { vm->last_error = VM_ERR_STACK; break; }
                Value *v = vm->stack[vm->stack_top - 1];
                vm_push(vm, v);
                break;
            }
            case OP_RETURN: {
                Frame *old_frame = vm->current_frame;
                Value *retval = NULL;

                if (!old_frame) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                VM_DEBUG(
                    "[RETURN] frame=%p stack=%u base=%u prev=%p\n",
                    (void *)old_frame,
                    vm->stack_top,
                    old_frame ? old_frame->stack_base : 0,
                    old_frame ? (void *)old_frame->prev : NULL);

                if (vm->stack_top > old_frame->stack_base) {
                    retval = vm_pop(vm);

                    while (vm->stack_top > old_frame->stack_base) {
                        Value *v = vm_pop(vm);
                        if (v) {
                            value_release(v);
                        }
                    }
                } else {
                    retval = value_new_int(0);
                }

                if (!retval) {
                    retval = value_new_int(0);
                    if (!retval) {
                        vm->last_error = VM_ERR_OOM;

                        vm->current_frame = old_frame->prev;

                        for (uint32_t i = 0; i < old_frame->locals_cap; i++) {
                            if (old_frame->locals[i]) {
                                value_release(old_frame->locals[i]);
                            }
                        }

                        free(old_frame->locals);
                        free(old_frame);

                        goto done;
                    }
                }

                VM_DEBUG(
                    "[RETURN] retval_tag=%d stack_after_pop=%u base=%u\n",
                    retval->tag,
                    vm->stack_top,
                    old_frame->stack_base);

                vm->current_frame = old_frame->prev;

                if (vm->current_frame) {
                    vm->ip = old_frame->return_ip;

                    VM_DEBUG(
                        "[RETURN] pushing retval to caller stack=%u\n",
                        vm->stack_top);

                    vm_push_owned(vm, retval);

                    VM_DEBUG(
                        "[RETURN] caller stack now=%u error=%d\n",
                        vm->stack_top,
                        vm->last_error);


                    for (uint32_t i = 0; i < old_frame->locals_cap; i++) {
                        if (old_frame->locals[i]) {
                            value_release(old_frame->locals[i]);
                        }
                    }

                    free(old_frame->locals);
                    free(old_frame);
                } else {
                    vm->result = retval;

                    for (uint32_t i = 0; i < old_frame->locals_cap; i++) {
                        if (old_frame->locals[i]) {
                            value_release(old_frame->locals[i]);
                        }
                    }

                    free(old_frame->locals);
                    free(old_frame);

                    goto done;
                }

                break;
            }
            case OP_CALL: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) { vm->last_error = VM_ERR_BOUNDS; break; }
                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4); vm->ip += 4;
                vm_call_value(vm, nargs, NULL);
                break;
            }
            case OP_CALL_KW: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) { vm->last_error = VM_ERR_BOUNDS; break; }
                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4); vm->ip += 4;

                uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
                if (vm->stack_top <= frame_base) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                Value *kwnames = vm_pop(vm);
                if (kwnames->tag != TAG_TUPLE || kwnames->data.tuple.len > nargs) {
                    value_release(kwnames);
                    vm->last_error = VM_ERR_TYPE;
                    break;
                }

                vm_call_value(vm, nargs, kwnames);
                value_release(kwnames);
                break;
            }
            case OP_LOAD_GLOBAL: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t idx;
                memcpy(&idx, vm->ip, 4);
                vm->ip += 4;

                if (idx >= vm->num_names) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                if (!vm->globals[idx]) {
                    vm->last_error = VM_ERR_FUNC_NOT_FOUND;
                    break;
                }

                vm_push(vm, vm->globals[idx]);
                break;
            }

            case OP_STORE_GLOBAL: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t idx;
                memcpy(&idx, vm->ip, 4);
                vm->ip += 4;

                if (idx >= vm->num_names) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                Value *value = vm_pop(vm);
                if (!value) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                if (vm->globals[idx]) {
                    value_release(vm->globals[idx]);
                }

                vm->globals[idx] = value;
                break;
            }

            case OP_BUILD_LIST: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4);
                vm->ip += 4;

                if (nargs > vm->stack_top) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                Value *list = value_new_list();
                if (!list) {
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                uint32_t start = vm->stack_top - nargs;
                int failed = 0;

                for (uint32_t i = 0; i < nargs; i++) {
                    Value *item = vm->stack[start + i];

                    if (value_list_append(list, item) != 0) {
                        failed = 1;
                        break;
                    }

                    value_release(item);
                    vm->stack[start + i] = NULL;
                }

                if (failed) {
                    for (uint32_t i = 0; i < nargs; i++) {
                        if (vm->stack[start + i]) {
                            value_release(vm->stack[start + i]);
                            vm->stack[start + i] = NULL;
                        }
                    }

                    vm->stack_top -= nargs;
                    value_release(list);
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                vm->stack_top -= nargs;
                vm_push_owned(vm, list);

                break;
            }
            case OP_BUILD_TUPLE: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4);
                vm->ip += 4;

                if (nargs > vm->stack_top) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                Value *tuple = value_new_tuple(nargs);
                if (!tuple) {
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                uint32_t start = vm->stack_top - nargs;

                for (uint32_t i = 0; i < nargs; i++) {
                    Value *item = vm->stack[start + i];

                    if (!item) {
                        for (uint32_t j = 0; j < i; j++) {
                            vm->stack[start + j] = NULL;
                        }
                        vm->stack_top -= nargs;
                        value_release(tuple);
                        vm->last_error = VM_ERR_STACK;
                        break;
                    }

                    tuple->data.tuple.items[i] = item;
                    vm->stack[start + i] = NULL;
                }

                if (vm->last_error != VM_ERR_OK) {
                    break;
                }

                vm->stack_top -= nargs;
                vm_push_owned(vm, tuple);

                break;
            }
            case OP_BUILD_MAP: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4);
                vm->ip += 4;

                if (nargs > UINT32_MAX / 2 || 2 * nargs > vm->stack_top) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                Value *dict = value_new_dict();
                if (!dict) {
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                uint32_t count = 2 * nargs;
                uint32_t start = vm->stack_top - count;
                int failed = 0;

                for (uint32_t i = 0; i < nargs; i++) {
                    Value *key = vm->stack[start + 2 * i];
                    Value *value = vm->stack[start + 2 * i + 1];

                    if (value_dict_set(dict, key, value) != 0) {
                        failed = 1;
                        break;
                    }

                    value_release(key);
                    value_release(value);

                    vm->stack[start + 2 * i] = NULL;
                    vm->stack[start + 2 * i + 1] = NULL;
                }

                if (failed) {
                    for (uint32_t i = 0; i < count; i++) {
                        if (vm->stack[start + i]) {
                            value_release(vm->stack[start + i]);
                            vm->stack[start + i] = NULL;
                        }
                    }

                    vm->stack_top -= count;
                    value_release(dict);
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                vm->stack_top -= count;

                vm_push_owned(vm, dict);

                break;
            }
            case OP_BUILD_SET: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
                    vm->last_error = VM_ERR_BOUNDS;
                    break;
                }

                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4);
                vm->ip += 4;

                if (nargs > vm->stack_top) {
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                Value *set = value_new_set();
                if (!set) {
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                uint32_t start = vm->stack_top - nargs;
                int failed = 0;

                for (uint32_t i = 0; i < nargs; i++) {
                    Value *item = vm->stack[start + i];

                    if (value_set_add(set, item) != 0) {
                        failed = 1;
                        break;
                    }

                    value_release(item);
                    vm->stack[start + i] = NULL;
                }

                if (failed) {
                    for (uint32_t i = 0; i < nargs; i++) {
                        if (vm->stack[start + i]) {
                            value_release(vm->stack[start + i]);
                            vm->stack[start + i] = NULL;
                        }
                    }

                    vm->stack_top -= nargs;
                    value_release(set);
                    vm->last_error = VM_ERR_OOM;
                    break;
                }

                vm->stack_top -= nargs;

                vm_push_owned(vm, set);

                break;
            }
            case OP_LIST_APPEND: {
                if (vm->stack_top < 2) { vm->last_error = VM_ERR_STACK; break; }
                Value *value = vm_pop(vm);
                Value *list = vm_pop(vm);
                if (!value || !list) {
                    if (value) value_release(value);
                    if (list) value_release(list);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }
                if (list->tag != TAG_LIST) {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(value);
                    value_release(list);
                    break;
                }
                if (value_list_append(list, value) != 0) {
                    vm->last_error = VM_ERR_OOM;
                }
                value_release(value);
                value_release(list);
                break;
            }
            case OP_SET_ADD: {
                if (vm->stack_top < 2) { vm->last_error = VM_ERR_STACK; break; }
                Value *value = vm_pop(vm);
                Value *set = vm_pop(vm);
                if (!value || !set) {
                    if (value) value_release(value);
                    if (set) value_release(set);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }
                if (set->tag != TAG_SET) {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(value);
                    value_release(set);
                    break;
                }
                if (value_set_add(set, value) != 0) {
                    vm->last_error = VM_ERR_OOM;
                }
                value_release(value);
                value_release(set);
                break;
            }
            case OP_MAP_ADD: {
                if (vm->stack_top < 3) { vm->last_error = VM_ERR_STACK; break; }
                Value *value = vm_pop(vm);
                Value *key = vm_pop(vm);
                Value *dict = vm_pop(vm);
                if (!value || !key || !dict) {
                    if (value) value_release(value);
                    if (key) value_release(key);
                    if (dict) value_release(dict);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }
                if (dict->tag != TAG_DICT) {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(value);
                    value_release(key);
                    value_release(dict);
                    break;
                }
                if (value_dict_set(dict, key, value) != 0) {
                    vm->last_error = VM_ERR_OOM;
                }
                value_release(value);
                value_release(key);
                value_release(dict);
                break;
            }

            case OP_GET_INDEX: {
                Value *idx_val = vm_pop(vm);
                Value *container = vm_pop(vm);

                if (!idx_val || !container) {
                    if (idx_val) value_release(idx_val);
                    if (container) value_release(container);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                if (idx_val->tag != TAG_INT && container->tag != TAG_DICT) {
                    vm->last_error = VM_ERR_TYPE;
                } else {
                    int64_t idx = 0;
                    if (idx_val->tag == TAG_INT) {
                        idx = idx_val->data.int_val;
                    }

                    if (container->tag == TAG_LIST) {
                        int64_t len = (int64_t)container->data.list.len;

                        if (idx < 0) idx += len;

                        if (idx < 0 || idx >= len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            Value *item = value_list_get(container, (size_t)idx);
                            if (!item) {
                                vm->last_error = VM_ERR_BOUNDS;
                            } else {
                                vm_push(vm, item);
                            }
                        }
                    } else if (container->tag == TAG_TUPLE) {
                        int64_t len = (int64_t)container->data.tuple.len;

                        if (idx < 0) idx += len;

                        if (idx < 0 || idx >= len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            Value *item = value_tuple_get(container, (size_t)idx);
                            if (!item) {
                                vm->last_error = VM_ERR_BOUNDS;
                            } else {
                                vm_push(vm, item);
                            }
                        }
                    } else if (container->tag == TAG_BYTES) {
                        int64_t len = (int64_t)container->data.bytes.len;

                        if (idx < 0) idx += len;

                        if (idx < 0 || idx >= len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            Value *result = value_new_int(
                                (int64_t)container->data.bytes.data[idx]
                            );

                            if (!result) {
                                vm->last_error = VM_ERR_OOM;
                            } else {
                                vm_push_owned(vm, result);
                            }
                        }
                    } else if (container->tag == TAG_STRING) {
                        int64_t len = (int64_t)container->data.str.len;

                        if (idx < 0) idx += len;

                        if (idx < 0 || idx >= len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            char ch[2];
                            ch[0] = container->data.str.data[idx];
                            ch[1] = '\0';

                            Value *result = value_new_string_len(ch, 1);
                            if (!result) {
                                vm->last_error = VM_ERR_OOM;
                            } else {
                                vm_push_owned(vm, result);
                            }
                        }
                    } else if (container->tag == TAG_DICT) {
                        Value *found = value_dict_get(container, idx_val);

                        if (found) {
                            vm_push(vm, found);
                        } else {
                            Value *result = value_new_int(0);
                            if (!result) {
                                vm->last_error = VM_ERR_OOM;
                            } else {
                                vm_push_owned(vm, result);
                            }
                        }
                    } else {
                        vm->last_error = VM_ERR_TYPE;
                    }
                }

                value_release(idx_val);
                value_release(container);
                break;
            }

            case OP_GET_ITER_ITEM: {
                Value *idx_val = vm_pop(vm);
                Value *container = vm_pop(vm);

                if (!idx_val || !container) {
                    if (idx_val) value_release(idx_val);
                    if (container) value_release(container);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                if (idx_val->tag != TAG_INT) {
                    vm->last_error = VM_ERR_TYPE;
                } else {
                    int64_t idx = idx_val->data.int_val;

                    if (idx < 0) {
                        vm->last_error = VM_ERR_BOUNDS;
                    } else if (container->tag == TAG_DICT) {
                        Value *key = value_dict_key_at(container, (size_t)idx);

                        if (!key) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            vm_push(vm, key);
                        }
                    } else if (container->tag == TAG_LIST) {
                        Value *item = value_list_get(container, (size_t)idx);

                        if (!item) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            vm_push(vm, item);
                        }
                    } else if (container->tag == TAG_TUPLE) {
                        Value *item = value_tuple_get(container, (size_t)idx);

                        if (!item) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            vm_push(vm, item);
                        }
                    } else if (container->tag == TAG_STRING) {
                        if ((size_t)idx >= container->data.str.len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            char ch[2];
                            ch[0] = container->data.str.data[idx];
                            ch[1] = '\0';

                            Value *item = value_new_string_len(ch, 1);
                            if (!item) {
                                vm->last_error = VM_ERR_OOM;
                            } else {
                                vm_push_owned(vm, item);
                            }
                        }
                    } else if (container->tag == TAG_BYTES) {
                        if ((size_t)idx >= container->data.bytes.len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            Value *item = value_new_int(
                                (int64_t)container->data.bytes.data[idx]
                            );

                            if (!item) {
                                vm->last_error = VM_ERR_OOM;
                            } else {
                                vm_push_owned(vm, item);
                            }
                        }
                    } else {
                        vm->last_error = VM_ERR_TYPE;
                    }
                }

                value_release(idx_val);
                value_release(container);
                break;
            }

            case OP_GET_SLICE: {
                Value *step_val = vm_pop(vm);
                Value *stop_val = vm_pop(vm);
                Value *start_val = vm_pop(vm);
                Value *container = vm_pop(vm);

                if (!step_val || !stop_val || !start_val || !container) {
                    if (step_val) value_release(step_val);
                    if (stop_val) value_release(stop_val);
                    if (start_val) value_release(start_val);
                    if (container) value_release(container);
                    vm->last_error = VM_ERR_STACK;
                    break;
                }

                if ((container->tag != TAG_LIST &&
                     container->tag != TAG_TUPLE &&
                     container->tag != TAG_STRING &&
                     container->tag != TAG_BYTES) ||
                    (start_val->tag != TAG_NONE && start_val->tag != TAG_INT) ||
                    (stop_val->tag != TAG_NONE && stop_val->tag != TAG_INT) ||
                    (step_val->tag != TAG_NONE && step_val->tag != TAG_INT)) {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(step_val);
                    value_release(stop_val);
                    value_release(start_val);
                    value_release(container);
                    break;
                }

                int64_t step = step_val->tag == TAG_NONE ? 1 : step_val->data.int_val;

                if (step == 0) {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(step_val);
                    value_release(stop_val);
                    value_release(start_val);
                    value_release(container);
                    break;
                }

                int64_t len;

                if (container->tag == TAG_LIST) {
                    len = (int64_t)container->data.list.len;
                } else if (container->tag == TAG_TUPLE) {
                    len = (int64_t)container->data.tuple.len;
                } else if (container->tag == TAG_STRING) {
                    len = (int64_t)container->data.str.len;
                } else {
                    len = (int64_t)container->data.bytes.len;
                }

                int64_t start;
                int64_t stop;

                if (step > 0) {
                    start = start_val->tag == TAG_NONE ? 0 : start_val->data.int_val;
                    stop = stop_val->tag == TAG_NONE ? len : stop_val->data.int_val;

                    if (start < 0) start += len;
                    if (stop < 0) stop += len;

                    if (start < 0) start = 0;
                    if (start > len) start = len;
                    if (stop < 0) stop = 0;
                    if (stop > len) stop = len;
                } else {
                    start = start_val->tag == TAG_NONE ? len - 1 : start_val->data.int_val;
                    stop = stop_val->tag == TAG_NONE ? -1 : stop_val->data.int_val;

                    if (start < 0) start += len;
                    if (stop < 0 && stop != -1) stop += len;

                    if (start >= len) start = len - 1;
                    if (start < -1) start = -1;
                    if (stop >= len) stop = len - 1;
                }

                if (container->tag == TAG_LIST) {
                    Value *result = value_new_list();

                    if (!result) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        if (step > 0) {
                            for (int64_t i = start; i < stop; i += step) {
                                Value *item = value_list_get(container, (size_t)i);

                                if (!item || value_list_append(result, item) != 0) {
                                    vm->last_error = VM_ERR_OOM;
                                    break;
                                }
                            }
                        } else {
                            for (int64_t i = start; i > stop; i += step) {
                                if (i < 0 || i >= len) continue;

                                Value *item = value_list_get(container, (size_t)i);

                                if (!item || value_list_append(result, item) != 0) {
                                    vm->last_error = VM_ERR_OOM;
                                    break;
                                }
                            }
                        }

                        if (vm->last_error == VM_ERR_OK) {
                            vm_push_owned(vm, result);
                        } else {
                            value_release(result);
                        }
                    }
                } else if (container->tag == TAG_TUPLE) {
                    int64_t count = 0;

                    if (step > 0) {
                        for (int64_t i = start; i < stop; i += step) {
                            count++;
                        }
                    } else {
                        for (int64_t i = start; i > stop; i += step) {
                            if (i >= 0 && i < len) {
                                count++;
                            }
                        }
                    }

                    Value *result = value_new_tuple((size_t)count);

                    if (!result) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        int64_t out = 0;

                        if (step > 0) {
                            for (int64_t i = start; i < stop; i += step) {
                                Value *item = value_tuple_get(container, (size_t)i);

                                if (!item) {
                                    vm->last_error = VM_ERR_BOUNDS;
                                    break;
                                }

                                result->data.tuple.items[out++] = value_retain(item);
                            }
                        } else {
                            for (int64_t i = start; i > stop; i += step) {
                                if (i < 0 || i >= len) continue;

                                Value *item = value_tuple_get(container, (size_t)i);

                                if (!item) {
                                    vm->last_error = VM_ERR_BOUNDS;
                                    break;
                                }

                                result->data.tuple.items[out++] = value_retain(item);
                            }
                        }

                        if (vm->last_error == VM_ERR_OK) {
                            vm_push_owned(vm, result);
                        } else {
                            value_release(result);
                        }
                    }
                } else if (container->tag == TAG_STRING) {
                    int64_t count = 0;

                    if (step > 0) {
                        for (int64_t i = start; i < stop; i += step) {
                            count++;
                        }
                    } else {
                        for (int64_t i = start; i > stop; i += step) {
                            if (i >= 0 && i < len) {
                                count++;
                            }
                        }
                    }

                    char *buffer = malloc((size_t)count + 1);

                    if (!buffer) {
                        vm->last_error = VM_ERR_OOM;
                    } else {
                        int64_t out = 0;

                        if (step > 0) {
                            for (int64_t i = start; i < stop; i += step) {
                                buffer[out++] = container->data.str.data[i];
                            }
                        } else {
                            for (int64_t i = start; i > stop; i += step) {
                                if (i >= 0 && i < len) {
                                    buffer[out++] = container->data.str.data[i];
                                }
                            }
                        }

                        buffer[out] = '\0';

                        Value *result = value_new_string_len(buffer, (size_t)count);
                        free(buffer);

                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    }
                } else {
                    int64_t count = 0;

                    if (step > 0) {
                        for (int64_t i = start; i < stop; i += step) {
                            count++;
                        }
                    } else {
                        for (int64_t i = start; i > stop; i += step) {
                            if (i >= 0 && i < len) {
                                count++;
                            }
                        }
                    }

                    unsigned char *buffer = NULL;

                    if (count > 0) {
                        buffer = malloc((size_t)count);

                        if (!buffer) {
                            vm->last_error = VM_ERR_OOM;
                        }
                    }

                    if (vm->last_error == VM_ERR_OK) {
                        int64_t out = 0;

                        if (step > 0) {
                            for (int64_t i = start; i < stop; i += step) {
                                buffer[out++] = container->data.bytes.data[i];
                            }
                        } else {
                            for (int64_t i = start; i > stop; i += step) {
                                if (i >= 0 && i < len) {
                                    buffer[out++] = container->data.bytes.data[i];
                                }
                            }
                        }

                        Value *result = value_new_bytes(buffer, (size_t)count);
                        free(buffer);

                        if (!result) {
                            vm->last_error = VM_ERR_OOM;
                        } else {
                            vm_push_owned(vm, result);
                        }
                    } else {
                        free(buffer);
                    }
                }

                value_release(step_val);
                value_release(stop_val);
                value_release(start_val);
                value_release(container);
                break;
            }
            case OP_SET_INDEX: {
                    Value *val = vm_pop(vm);
                    Value *idx_val = vm_pop(vm);
                    Value *container = vm_pop(vm);

                    if (!val || !idx_val || !container) break;

                    if (container->tag == TAG_LIST && idx_val->tag == TAG_INT) {
                        int64_t idx = idx_val->data.int_val;
                        int64_t len = (int64_t)container->data.list.len;

                        if (idx < 0) {
                            idx += len;
                        }

                        if (idx < 0 || idx >= len) {
                            vm->last_error = VM_ERR_BOUNDS;
                        } else {
                            value_list_set(container, (size_t)idx, val);
                        }
                    } else if (container->tag == TAG_DICT && idx_val->tag == TAG_STRING) {
                        if (value_dict_set(container, idx_val, val) != 0) {
                            vm->last_error = VM_ERR_OOM;
                        }
                    } else {
                        vm->last_error = VM_ERR_TYPE;
                    }

                    value_release(val);
                    value_release(idx_val);
                    value_release(container);
                    break;
                }
            case OP_LEN: {
                Value *v = vm_pop(vm);
                if (!v) break;

                int64_t len_val = -1;

                if (v->tag == TAG_STRING) {
                    len_val = (int64_t)v->data.str.len;
                } else if (v->tag == TAG_BYTES) {
                    len_val = (int64_t)v->data.bytes.len;
                } else if (v->tag == TAG_LIST) {
                    len_val = (int64_t)v->data.list.len;
                } else if (v->tag == TAG_TUPLE) {
                    len_val = (int64_t)v->data.tuple.len;
                } else if (v->tag == TAG_DICT) {
                    len_val = (int64_t)v->data.dict.len;
                } else if (v->tag == TAG_SET) {
                    len_val = (int64_t)v->data.set.len;
                } else {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(v);
                    break;
                }

                Value *result = value_new_int(len_val);
                if (!result) {
                    vm->last_error = VM_ERR_OOM;
                } else {
                    vm_push_owned(vm, result);
                }

                value_release(v);
                break;
            }
            case OP_CALL_METHOD: {
                if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) { vm->last_error = VM_ERR_BOUNDS; break; }
                uint32_t nargs;
                memcpy(&nargs, vm->ip, 4); vm->ip += 4;
                if (nargs > vm->stack_top) { vm->last_error = VM_ERR_STACK; break; }
                Value **args = NULL;
                if (nargs > 0) {
                    args = calloc(nargs, sizeof(Value*));
                    if (!args) { vm->last_error = VM_ERR_OOM; break; }
                    for (int i = nargs - 1; i >= 0; i--) {
                        args[i] = vm_pop(vm);
                        if (!args[i]) { vm->last_error = VM_ERR_STACK; break; }
                    }
                    if (vm->last_error != VM_ERR_OK) {
                        for (uint32_t i = 0; i < nargs; i++) {
                            if (args[i]) value_release(args[i]);
                        }
                        free(args);
                        break;
                    }
                }
                Value *method_name = vm_pop(vm);
                if (!method_name) {
                    vm->last_error = VM_ERR_STACK;
                    if (args) {
                        for (uint32_t i = 0; i < nargs; i++) {
                            if (args[i]) value_release(args[i]);
                        }
                        free(args);
                    }
                    break;
                }
                if (method_name->tag != TAG_STRING) {
                    vm->last_error = VM_ERR_TYPE;
                    value_release(method_name);
                    if (args) {
                        for (uint32_t i = 0; i < nargs; i++) {
                            if (args[i]) value_release(args[i]);
                        }
                        free(args);
                    }
                    break;
                }
                Value *obj = vm_pop(vm);
                if (!obj) {
                    vm->last_error = VM_ERR_STACK;
                    value_release(method_name);
                    if (args) { for (uint32_t i = 0; i < nargs; i++) value_release(args[i]); free(args); }
                    break;
                }

                call_method(vm, obj, method_name, nargs, args);

                value_release(method_name);
                value_release(obj);
                if (args) free(args);
                if (vm->last_error != VM_ERR_OK) break;
                break;
            }
            case OP_NOP:
                break;
            default:
                vm->last_error = VM_ERR_INVALID_OP;
                break;
        }
    }

done:
    return vm->last_error;
}

void vm_free(VM *vm) {
    if (!vm) return;
    if (vm->bytecode) free(vm->bytecode);
    if (vm->constants) {
        for (uint32_t i = 0; i < vm->num_constants; i++) {
            if (vm->constants[i]) value_release(vm->constants[i]);
        }
        free(vm->constants);
    }
    if (vm->globals) {
        for (uint32_t i = 0; i < vm->num_names; i++) {
            if (vm->globals[i]) value_release(vm->globals[i]);
        }
        free(vm->globals);
        vm->globals = NULL;
    }

    if (vm->names) {
        for (uint32_t i = 0; i < vm->num_names; i++) {
            if (vm->names[i]) free(vm->names[i]);
        }
        free(vm->names);
    }
    if (vm->functions) {
        for (uint32_t i = 0; i < vm->num_functions; i++) {
            free(vm->functions[i].param_names);
            free(vm->functions[i].default_consts);
        }
        free(vm->functions);
    }
    if (vm->stack) {
        for (uint32_t i = 0; i < vm->stack_top; i++) {
            if (vm->stack[i]) value_release(vm->stack[i]);
        }
        free(vm->stack);
    }
    if (vm->result) value_release(vm->result);
    Frame *frame = vm->current_frame;
    while (frame) {
        Frame *next = frame->prev;
        if (frame->locals) {
            for (uint32_t i = 0; i < frame->locals_cap; i++) {
                if (frame->locals[i]) value_release(frame->locals[i]);
            }
            free(frame->locals);
        }
        free(frame);
        frame = next;
    }
    memset(vm, 0, sizeof(VM));
}

const char *vm_error_string(VM *vm) {
    if (!vm) return "NULL VM";
    switch (vm->last_error) {
        case VM_ERR_OK: return "OK";
        case VM_ERR_LOAD: return "Load failed";
        case VM_ERR_OOM: return "Out of memory";
        case VM_ERR_BAD_MAGIC: return "Bad magic";
        case VM_ERR_VERSION: return "Unsupported version";
        case VM_ERR_BOUNDS: return "Bounds error";
        case VM_ERR_TYPE: return "Type error";
        case VM_ERR_STACK: return "Stack error";
        case VM_ERR_DIV_ZERO: return "Division by zero";
        case VM_ERR_FUNC_NOT_FOUND: return "Function not found";
        case VM_ERR_INVALID_OP: return "Invalid opcode";
        case VM_ERR_OVERFLOW: return "Integer overflow";
        default: return "Unknown error";
    }
}
