#include <string.h>
#include "vm_internal.h"

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
static Value g_type_object      = { TAG_TYPE, UINT32_MAX, { TYPE_OBJECT } };
static Value g_type_enumerate   = { TAG_TYPE, UINT32_MAX, { TYPE_ENUMERATE } };
static Value g_type_zip         = { TAG_TYPE, UINT32_MAX, { TYPE_ZIP } };
static Value g_type_map         = { TAG_TYPE, UINT32_MAX, { TYPE_MAP } };
static Value g_type_filter      = { TAG_TYPE, UINT32_MAX, { TYPE_FILTER } };
static Value g_type_reversed    = { TAG_TYPE, UINT32_MAX, { TYPE_REVERSED } };
static Value g_type_list_iterator      = { TAG_TYPE, UINT32_MAX, { TYPE_LIST_ITERATOR } };
static Value g_type_tuple_iterator     = { TAG_TYPE, UINT32_MAX, { TYPE_TUPLE_ITERATOR } };
static Value g_type_str_iterator       = { TAG_TYPE, UINT32_MAX, { TYPE_STR_ITERATOR } };
static Value g_type_str_ascii_iterator = { TAG_TYPE, UINT32_MAX, { TYPE_STR_ASCII_ITERATOR } };
static Value g_type_bytes_iterator     = { TAG_TYPE, UINT32_MAX, { TYPE_BYTES_ITERATOR } };
static Value g_type_range_iterator     = { TAG_TYPE, UINT32_MAX, { TYPE_RANGE_ITERATOR } };
static Value g_type_dict_keyiterator   = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_KEYITERATOR } };
static Value g_type_dict_valueiterator = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_VALUEITERATOR } };
static Value g_type_dict_itemiterator  = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_ITEMITERATOR } };
static Value g_type_set_iterator       = { TAG_TYPE, UINT32_MAX, { TYPE_SET_ITERATOR } };
static Value g_type_dict_reversekeyiterator   = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_REVERSEKEYITERATOR } };
static Value g_type_dict_reversevalueiterator = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_REVERSEVALUEITERATOR } };
static Value g_type_dict_reverseitemiterator  = { TAG_TYPE, UINT32_MAX, { TYPE_DICT_REVERSEITEMITERATOR } };
static Value g_type_list_reverseiterator      = { TAG_TYPE, UINT32_MAX, { TYPE_LIST_REVERSEITERATOR } };
static Value g_type_callable_iterator         = { TAG_TYPE, UINT32_MAX, { TYPE_CALLABLE_ITERATOR } };

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
    "dict_items",
    "object",
    "enumerate",
    "zip",
    "map",
    "filter",
    "reversed",
    "list_iterator",
    "tuple_iterator",
    "str_iterator",
    "str_ascii_iterator",
    "bytes_iterator",
    "range_iterator",
    "dict_keyiterator",
    "dict_valueiterator",
    "dict_itemiterator",
    "set_iterator",
    "dict_reversekeyiterator",
    "dict_reversevalueiterator",
    "dict_reverseitemiterator",
    "list_reverseiterator",
    "callable_iterator"
};

const char *value_type_name(const Value *v) {
    if (!v || v->tag != TAG_TYPE) return "object";

    int index = exception_index_of((int)v->data.int_val);

    if (index >= 0) return exception_type_name(index);
    return TYPE_NAMES[v->data.int_val];
}

int value_type_is_subtype(const Value *type, const Value *base) {
    if (!type || !base || type->tag != TAG_TYPE || base->tag != TAG_TYPE) return 0;
    if (type == base) return 1;

    int type_id = (int)type->data.int_val;
    int base_id = (int)base->data.int_val;

    if (base_id == TYPE_OBJECT) return 1;
    if (type_id == TYPE_BOOL && base_id == TYPE_INT) return 1;

    if (exception_index_of(type_id) >= 0 && exception_index_of(base_id) >= 0) {
        return exception_type_is_subtype(type_id, base_id);
    }

    return 0;
}

static int text_is_ascii(const Value *v) {
    for (uint32_t i = 0; i < v->data.str.len; i++) {
        if ((unsigned char)v->data.str.data[i] >= 0x80) return 0;
    }
    return 1;
}

static int sequence_iterator_type(const Value *src) {
    switch (src->tag) {
        case TAG_LIST: return TYPE_LIST_ITERATOR;
        case TAG_TUPLE: return TYPE_TUPLE_ITERATOR;
        case TAG_STRING: return text_is_ascii(src) ? TYPE_STR_ASCII_ITERATOR : TYPE_STR_ITERATOR;
        case TAG_BYTES: return TYPE_BYTES_ITERATOR;
        case TAG_RANGE: return TYPE_RANGE_ITERATOR;
        case TAG_DICT: return TYPE_DICT_KEYITERATOR;
        case TAG_SET:
        case TAG_FROZENSET: return TYPE_SET_ITERATOR;
        case TAG_DICT_VIEW:
            switch (src->data.view.kind) {
                case VIEW_KEYS: return TYPE_DICT_KEYITERATOR;
                case VIEW_VALUES: return TYPE_DICT_VALUEITERATOR;
                default: return TYPE_DICT_ITEMITERATOR;
            }
        default: return TYPE_ITERATOR;
    }
}

static int reversed_iterator_type(const Value *src) {
    switch (src->tag) {
        case TAG_LIST: return TYPE_LIST_REVERSEITERATOR;
        case TAG_RANGE: return TYPE_RANGE_ITERATOR;
        case TAG_DICT: return TYPE_DICT_REVERSEKEYITERATOR;
        case TAG_DICT_VIEW:
            switch (src->data.view.kind) {
                case VIEW_VALUES: return TYPE_DICT_REVERSEVALUEITERATOR;
                default: return TYPE_DICT_REVERSEITEMITERATOR;
            }
        default: return TYPE_REVERSED;
    }
}

static int iterator_type(const IterObject *it) {
    switch (it->kind) {
        case ITER_SEQ: return sequence_iterator_type(it->source);
        case ITER_ENUMERATE: return TYPE_ENUMERATE;
        case ITER_ZIP: return TYPE_ZIP;
        case ITER_MAP: return TYPE_MAP;
        case ITER_FILTER: return TYPE_FILTER;
        case ITER_CALLABLE: return TYPE_CALLABLE_ITERATOR;
        case ITER_REVERSED: return reversed_iterator_type(it->source);
        default: return TYPE_ITERATOR;
    }
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
        case TAG_OBJECT: return &g_type_object;
        case TAG_EXCEPTION: {
            int index = exception_index_of((int)v->data.exception.type_id);
            Value *type = exception_type_object(index);
            return type ? type : &g_type_object;
        }
        case TAG_ITERATOR:
            switch (iterator_type(v->data.iter)) {
                case TYPE_LIST_ITERATOR: return &g_type_list_iterator;
                case TYPE_TUPLE_ITERATOR: return &g_type_tuple_iterator;
                case TYPE_STR_ITERATOR: return &g_type_str_iterator;
                case TYPE_STR_ASCII_ITERATOR: return &g_type_str_ascii_iterator;
                case TYPE_BYTES_ITERATOR: return &g_type_bytes_iterator;
                case TYPE_RANGE_ITERATOR: return &g_type_range_iterator;
                case TYPE_DICT_KEYITERATOR: return &g_type_dict_keyiterator;
                case TYPE_DICT_VALUEITERATOR: return &g_type_dict_valueiterator;
                case TYPE_DICT_ITEMITERATOR: return &g_type_dict_itemiterator;
                case TYPE_SET_ITERATOR: return &g_type_set_iterator;
                case TYPE_DICT_REVERSEKEYITERATOR: return &g_type_dict_reversekeyiterator;
                case TYPE_DICT_REVERSEVALUEITERATOR: return &g_type_dict_reversevalueiterator;
                case TYPE_DICT_REVERSEITEMITERATOR: return &g_type_dict_reverseitemiterator;
                case TYPE_LIST_REVERSEITERATOR: return &g_type_list_reverseiterator;
                case TYPE_CALLABLE_ITERATOR: return &g_type_callable_iterator;
                case TYPE_ENUMERATE: return &g_type_enumerate;
                case TYPE_ZIP: return &g_type_zip;
                case TYPE_MAP: return &g_type_map;
                case TYPE_FILTER: return &g_type_filter;
                case TYPE_REVERSED: return &g_type_reversed;
                default: return &g_type_iterator;
            }
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
    {"object", &g_type_object},
    {"enumerate", &g_type_enumerate},
    {"zip", &g_type_zip},
    {"map", &g_type_map},
    {"filter", &g_type_filter},
    {"reversed", &g_type_reversed},
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
        case TYPE_OBJECT: return construct_object;
        case TYPE_ENUMERATE: return builtin_enumerate;
        case TYPE_ZIP: return builtin_zip;
        case TYPE_MAP: return builtin_map;
        case TYPE_FILTER: return builtin_filter;
        case TYPE_REVERSED: return builtin_reversed;
        default: return NULL;
    }
}
