#include "vm_internal.h"

static Value *dict_get(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (!value_is_hashable(args[0])) return vm_fail(vm, VM_ERR_TYPE);

    Value *found = value_dict_get(self, args[0]);
    if (found) return value_retain(found);
    return nargs == 2 ? value_retain(args[1]) : value_new_none();
}

static Value *make_view(VM *vm, Value *self, uint32_t nargs, const Value *kwnames, ViewKind kind) {
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    Value *view = value_new_dict_view(self, kind);
    return view ? view : vm_fail(vm, VM_ERR_OOM);
}

static Value *dict_keys(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return make_view(vm, self, nargs, kwnames, VIEW_KEYS);
}

static Value *dict_values(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return make_view(vm, self, nargs, kwnames, VIEW_VALUES);
}

static Value *dict_items(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    return make_view(vm, self, nargs, kwnames, VIEW_ITEMS);
}

static Value *dict_pop(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (!value_is_hashable(args[0])) return vm_fail(vm, VM_ERR_TYPE);

    Value *found = value_dict_get(self, args[0]);
    if (found) {
        Value *result = value_retain(found);
        value_dict_delete(self, args[0]);
        return result;
    }

    return nargs == 2 ? value_retain(args[1]) : vm_fail(vm, VM_ERR_KEY);
}

static Value *dict_popitem(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    if (self->data.dict.len == 0) return vm_fail(vm, VM_ERR_KEY);

    uint32_t last = self->data.dict.len - 1;
    DictEntry *entry = &self->data.dict.entries[last];

    Value *pair = value_new_tuple(2);
    if (!pair) return vm_fail(vm, VM_ERR_OOM);

    pair->data.tuple.items[0] = entry->key;
    pair->data.tuple.items[1] = entry->value;
    self->data.dict.len--;
    return pair;
}

static Value *dict_setdefault(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (!value_is_hashable(args[0])) return vm_fail(vm, VM_ERR_TYPE);

    Value *found = value_dict_get(self, args[0]);
    if (found) return value_retain(found);

    Value *fallback = nargs == 2 ? args[1] : value_new_none();
    int rc = value_dict_set(self, args[0], fallback);
    if (nargs != 2) value_release(fallback);
    if (rc != 0) return vm_fail(vm, insert_error(rc));

    return nargs == 2 ? value_retain(args[1]) : value_new_none();
}

static Value *dict_update(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t nkw = kw_count(kwnames);
    uint32_t npos = nargs - nkw;
    if (npos > 1) return vm_fail(vm, VM_ERR_TYPE);

    if (npos == 1 && value_dict_update(vm, self, args[0]) != 0) return NULL;

    for (uint32_t k = 0; k < nkw; k++) {
        int rc = value_dict_set(self, kwnames->data.tuple.items[k], args[npos + k]);
        if (rc != 0) return vm_fail(vm, insert_error(rc));
    }

    return value_new_none();
}

static Value *dict_clear(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;

    for (uint32_t i = 0; i < self->data.dict.len; i++) {
        value_release(self->data.dict.entries[i].key);
        value_release(self->data.dict.entries[i].value);
    }
    self->data.dict.len = 0;
    return value_new_none();
}

static Value *dict_copy(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;

    Value *out = value_new_dict();
    if (!out) return vm_fail(vm, VM_ERR_OOM);
    if (value_dict_update(vm, out, self) != 0) {
        value_release(out);
        return NULL;
    }
    return out;
}

const MethodEntry DICT_METHODS[] = {
    {"get", dict_get},
    {"keys", dict_keys},
    {"values", dict_values},
    {"items", dict_items},
    {"pop", dict_pop},
    {"popitem", dict_popitem},
    {"setdefault", dict_setdefault},
    {"update", dict_update},
    {"clear", dict_clear},
    {"copy", dict_copy},
};

const uint32_t DICT_METHOD_COUNT = (uint32_t)(sizeof(DICT_METHODS) / sizeof(DICT_METHODS[0]));
