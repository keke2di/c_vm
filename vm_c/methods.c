#include <string.h>
#include "vm_internal.h"

MethodFn method_table_lookup(const MethodEntry *table, uint32_t count, const Value *name) {
    for (uint32_t i = 0; i < count; i++) {
        size_t len = strlen(table[i].name);
        if (len == name->data.str.len && memcmp(table[i].name, name->data.str.data, len) == 0) {
            return table[i].fn;
        }
    }
    return NULL;
}

MethodFn method_lookup(const Value *self, const Value *name) {
    if (!self || !name || name->tag != TAG_STRING) return NULL;

    if (self->tag == TAG_TYPE) {
        return static_method_lookup((int)self->data.int_val, name);
    }

    MethodFn found = NULL;

    switch (self->tag) {
        case TAG_LIST: found = method_table_lookup(LIST_METHODS, LIST_METHOD_COUNT, name); break;
        case TAG_DICT: found = method_table_lookup(DICT_METHODS, DICT_METHOD_COUNT, name); break;
        case TAG_SET: found = method_table_lookup(SET_METHODS, SET_METHOD_COUNT, name); break;
        case TAG_FROZENSET: found = method_table_lookup(FROZENSET_METHODS, FROZENSET_METHOD_COUNT, name); break;
        case TAG_STRING: found = method_table_lookup(STR_METHODS, STR_METHOD_COUNT, name); break;
        case TAG_BYTES: found = method_table_lookup(BYTES_METHODS, BYTES_METHOD_COUNT, name); break;
        default: return NULL;
    }

    if (found) return found;

    return static_method_lookup((int)value_type_of(self)->data.int_val, name);
}
