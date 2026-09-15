#include <string.h>
#include "vm_internal.h"

static MethodFn find_method(const MethodEntry *table, uint32_t count, const Value *name) {
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

    switch (self->tag) {
        case TAG_LIST: return find_method(LIST_METHODS, LIST_METHOD_COUNT, name);
        case TAG_DICT: return find_method(DICT_METHODS, DICT_METHOD_COUNT, name);
        case TAG_SET: return find_method(SET_METHODS, SET_METHOD_COUNT, name);
        case TAG_STRING: return find_method(STR_METHODS, STR_METHOD_COUNT, name);
        case TAG_BYTES: return find_method(BYTES_METHODS, BYTES_METHOD_COUNT, name);
        default: return NULL;
    }
}
