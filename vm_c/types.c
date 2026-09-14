#include <string.h>
#include "vm_internal.h"

enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STR,
    TYPE_BOOL,
    TYPE_LIST,
    TYPE_TUPLE,
    TYPE_DICT,
    TYPE_SET,
    TYPE_BYTES,
    TYPE_NONE,
    TYPE_TYPE,
    TYPE_FUNCTION,
    TYPE_BUILTIN_FN
};

static Value g_type_int        = { TAG_TYPE, UINT32_MAX, { TYPE_INT } };
static Value g_type_float      = { TAG_TYPE, UINT32_MAX, { TYPE_FLOAT } };
static Value g_type_str        = { TAG_TYPE, UINT32_MAX, { TYPE_STR } };
static Value g_type_bool       = { TAG_TYPE, UINT32_MAX, { TYPE_BOOL } };
static Value g_type_list       = { TAG_TYPE, UINT32_MAX, { TYPE_LIST } };
static Value g_type_tuple      = { TAG_TYPE, UINT32_MAX, { TYPE_TUPLE } };
static Value g_type_dict       = { TAG_TYPE, UINT32_MAX, { TYPE_DICT } };
static Value g_type_set        = { TAG_TYPE, UINT32_MAX, { TYPE_SET } };
static Value g_type_bytes      = { TAG_TYPE, UINT32_MAX, { TYPE_BYTES } };
static Value g_type_none       = { TAG_TYPE, UINT32_MAX, { TYPE_NONE } };
static Value g_type_type       = { TAG_TYPE, UINT32_MAX, { TYPE_TYPE } };
static Value g_type_function   = { TAG_TYPE, UINT32_MAX, { TYPE_FUNCTION } };
static Value g_type_builtin_fn = { TAG_TYPE, UINT32_MAX, { TYPE_BUILTIN_FN } };

static const char *TYPE_NAMES[] = {
    "int",
    "float",
    "str",
    "bool",
    "list",
    "tuple",
    "dict",
    "set",
    "bytes",
    "NoneType",
    "type",
    "function",
    "builtin_function_or_method"
};

const char *value_type_name(const Value *v) {
    if (!v || v->tag != TAG_TYPE) return "object";
    return TYPE_NAMES[v->data.int_val];
}

Value *value_type_of(const Value *v) {
    if (!v) return &g_type_none;

    switch (v->tag) {
        case TAG_INT: return &g_type_int;
        case TAG_BOOL: return &g_type_bool;
        case TAG_FLOAT: return &g_type_float;
        case TAG_STRING: return &g_type_str;
        case TAG_LIST: return &g_type_list;
        case TAG_TUPLE: return &g_type_tuple;
        case TAG_DICT: return &g_type_dict;
        case TAG_SET: return &g_type_set;
        case TAG_BYTES: return &g_type_bytes;
        case TAG_TYPE: return &g_type_type;
        case TAG_FUNCTION:
            return (v->data.func && v->data.func->kind == FUNC_BUILTIN)
                ? &g_type_builtin_fn
                : &g_type_function;
        default:
            return &g_type_none;
    }
}

typedef struct {
    const char *name;
    Value *obj;
} TypeGlobal;

static const TypeGlobal TYPE_GLOBALS[] = {
    {"int", &g_type_int},
    {"float", &g_type_float},
    {"str", &g_type_str},
    {"bool", &g_type_bool},
    {"list", &g_type_list},
    {"tuple", &g_type_tuple},
    {"dict", &g_type_dict},
    {"set", &g_type_set},
    {"bytes", &g_type_bytes},
    {"type", &g_type_type},
};

#define TYPE_GLOBAL_COUNT ((uint32_t)(sizeof(TYPE_GLOBALS) / sizeof(TYPE_GLOBALS[0])))

int vm_install_type_globals(VM *vm) {
    for (uint32_t n = 0; n < vm->num_names; n++) {
        for (uint32_t t = 0; t < TYPE_GLOBAL_COUNT; t++) {
            if (strcmp(vm->names[n], TYPE_GLOBALS[t].name) != 0) {
                continue;
            }
            if (vm->globals[n]) {
                value_release(vm->globals[n]);
            }
            vm->globals[n] = value_retain(TYPE_GLOBALS[t].obj);
            break;
        }
    }

    return VM_ERR_OK;
}

void type_construct(VM *vm, int type_id, uint32_t nargs) {
    switch (type_id) {
        case TYPE_INT: builtin_int(vm, nargs); break;
        case TYPE_FLOAT: builtin_float(vm, nargs); break;
        case TYPE_STR: builtin_str(vm, nargs); break;
        case TYPE_LIST: builtin_list(vm, nargs); break;
        case TYPE_BOOL: builtin_bool(vm, nargs); break;

        case TYPE_TYPE:
            if (nargs != 1) {
                vm->last_error = VM_ERR_TYPE;
            } else {
                Value *arg = vm_pop(vm);
                Value *type = value_type_of(arg);
                if (arg) value_release(arg);
                vm_push(vm, type);
            }
            break;

        default:
            vm->last_error = VM_ERR_TYPE;
            break;
    }
}
