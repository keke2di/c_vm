#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
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

void builtin_int(VM *vm, uint32_t nargs) {
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
    if (int_like(arg)) {
        result = arg->data.int_val;
    } else if (arg->tag == TAG_STRING) {
        char *end;
        long long parsed = strtoll(arg->data.str.data, &end, 10);
        if (*end == '\0') {
            result = parsed;
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

void builtin_str(VM *vm, uint32_t nargs) {
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

    size_t str_len = arg->tag == TAG_STRING ? value_string_len(arg) : strlen(s);
    value_release(arg);

    Value *str_val = value_new_string_len(s, str_len);
    free(s);

    vm_push_owned(vm, str_val);
}

void builtin_float(VM *vm, uint32_t nargs) {
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
    } else if (int_like(arg)) {
        result = (double)arg->data.int_val;
    } else if (arg->tag == TAG_STRING) {
        char *end;
        double parsed = strtod(arg->data.str.data, &end);
        if (*end == '\0') {
            result = parsed;
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

void builtin_list(VM *vm, uint32_t nargs) {
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

void builtin_bool(VM *vm, uint32_t nargs) {
    if (nargs == 0) {
        vm_push_owned(vm, value_bool(0));
        return;
    }
    if (nargs != 1) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    Value *arg = vm_pop(vm);
    if (!arg) return;

    int truth = value_truthy(arg);
    value_release(arg);
    vm_push_owned(vm, value_bool(truth));
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
        Value *pair = value_new_list();

        if (!index || !pair ||
            value_list_append(pair, index) != 0 ||
            value_list_append(pair, iterable->data.list.items[i]) != 0 ||
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
    {"enumerate", builtin_enumerate},
};

#define BUILTIN_COUNT ((uint32_t)(sizeof(BUILTINS) / sizeof(BUILTINS[0])))

uint32_t builtin_count(void) {
    return BUILTIN_COUNT;
}

const char *builtin_name(uint32_t index) {
    return index < BUILTIN_COUNT ? BUILTINS[index].name : NULL;
}

void builtin_invoke(VM *vm, uint32_t index, uint32_t nargs) {
    if (index >= BUILTIN_COUNT) {
        vm->last_error = VM_ERR_FUNC_NOT_FOUND;
        return;
    }
    BUILTINS[index].handler(vm, nargs);
}
