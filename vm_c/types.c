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
    TYPE_BUILTIN_FN,
    TYPE_RANGE,
    TYPE_ITERATOR,
    TYPE_FROZENSET,
    TYPE_DICT_KEYS,
    TYPE_DICT_VALUES,
    TYPE_DICT_ITEMS
};

static Value g_type_int         = { TAG_TYPE, UINT32_MAX, { TYPE_INT } };
static Value g_type_float       = { TAG_TYPE, UINT32_MAX, { TYPE_FLOAT } };
static Value g_type_str         = { TAG_TYPE, UINT32_MAX, { TYPE_STR } };
static Value g_type_bool        = { TAG_TYPE, UINT32_MAX, { TYPE_BOOL } };
static Value g_type_list        = { TAG_TYPE, UINT32_MAX, { TYPE_LIST } };
static Value g_type_tuple       = { TAG_TYPE, UINT32_MAX, { TYPE_TUPLE } };
static Value g_type_dict        = { TAG_TYPE, UINT32_MAX, { TYPE_DICT } };
static Value g_type_set         = { TAG_TYPE, UINT32_MAX, { TYPE_SET } };
static Value g_type_bytes       = { TAG_TYPE, UINT32_MAX, { TYPE_BYTES } };
static Value g_type_none        = { TAG_TYPE, UINT32_MAX, { TYPE_NONE } };
static Value g_type_type        = { TAG_TYPE, UINT32_MAX, { TYPE_TYPE } };
static Value g_type_function    = { TAG_TYPE, UINT32_MAX, { TYPE_FUNCTION } };
static Value g_type_builtin_fn  = { TAG_TYPE, UINT32_MAX, { TYPE_BUILTIN_FN } };
static Value g_type_range       = { TAG_TYPE, UINT32_MAX, { TYPE_RANGE } };
static Value g_type_iterator    = { TAG_TYPE, UINT32_MAX, { TYPE_ITERATOR } };
static Value g_type_frozenset   = { TAG_TYPE, UINT32_MAX, { TYPE_FROZENSET } };
static Value g_type_dict_keys   = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_KEYS } };
static Value g_type_dict_values = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_VALUES } };
static Value g_type_dict_items  = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_ITEMS } };

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
    "builtin_function_or_method",
    "range",
    "iterator",
    "frozenset",
    "dict_keys",
    "dict_values",
    "dict_items"
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
        case TAG_FROZENSET: return &g_type_frozenset;
        case TAG_BYTES: return &g_type_bytes;
        case TAG_TYPE: return &g_type_type;
        case TAG_RANGE: return &g_type_range;
        case TAG_ITERATOR: return &g_type_iterator;
        case TAG_DICT_VIEW:
            switch (v->data.view.kind) {
                case VIEW_KEYS: return &g_type_dict_keys;
                case VIEW_VALUES: return &g_type_dict_values;
                default: return &g_type_dict_items;
            }
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
    {"frozenset", &g_type_frozenset},
    {"bytes", &g_type_bytes},
    {"type", &g_type_type},
    {"range", &g_type_range},
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

NativeFn type_constructor(int type_id) {
    switch (type_id) {
        case TYPE_INT: return construct_int;
        case TYPE_FLOAT: return construct_float;
        case TYPE_STR: return construct_str;
        case TYPE_BOOL: return construct_bool;
        case TYPE_LIST: return construct_list;
        case TYPE_TUPLE: return construct_tuple;
        case TYPE_SET: return construct_set;
        case TYPE_FROZENSET: return construct_frozenset;
        case TYPE_DICT: return construct_dict;
        case TYPE_BYTES: return construct_bytes;
        case TYPE_RANGE: return construct_range;
        case TYPE_TYPE: return construct_type;
        default: return NULL;
    }
}
