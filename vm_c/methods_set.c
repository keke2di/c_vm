#include "vm_internal.h"

static int frozen_of(const Value *self) {
    return self->tag == TAG_FROZENSET;
}

static Value *to_set(VM *vm, const Value *other) {
    if (is_setlike(other)) return value_retain((Value *)other);
    Value *s = value_new_set();
    if (!s) return vm_fail(vm, VM_ERR_OOM);
    if (value_set_update(vm, s, other) != 0) {
        value_release(s);
        return NULL;
    }
    return s;
}

static int reject_kwargs(VM *vm, const Value *kwnames) {
    if (kw_count(kwnames) != 0) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }
    return 0;
}

static Value *set_union(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (reject_kwargs(vm, kwnames) != 0) return NULL;
    return setlike_union(vm, self, nargs, args, frozen_of(self));
}

static Value *set_intersection(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (reject_kwargs(vm, kwnames) != 0) return NULL;
    return setlike_intersection(vm, self, nargs, args, frozen_of(self));
}

static Value *set_difference(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (reject_kwargs(vm, kwnames) != 0) return NULL;
    return setlike_difference(vm, self, nargs, args, frozen_of(self));
}

static Value *set_symmetric_difference(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    return setlike_symdiff(vm, self, args[0], frozen_of(self));
}

static Value *set_issubset(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    Value *other = to_set(vm, args[0]);
    if (!other) return NULL;
    int result = setlike_subset(self, other);
    value_release(other);
    return value_bool(result);
}

static Value *set_issuperset(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    Value *other = to_set(vm, args[0]);
    if (!other) return NULL;
    int result = setlike_subset(other, self);
    value_release(other);
    return value_bool(result);
}

static Value *set_isdisjoint(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    Value *other = to_set(vm, args[0]);
    if (!other) return NULL;
    int disjoint = 1;
    for (uint32_t i = 0; i < self->data.set.len; i++) {
        if (value_set_contains(other, self->data.set.items[i])) {
            disjoint = 0;
            break;
        }
    }
    value_release(other);
    return value_bool(disjoint);
}

static Value *set_copy(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    Value *others = NULL;
    return setlike_union(vm, self, 0, &others, frozen_of(self));
}

static Value *set_add(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    int rc = value_set_add(self, args[0]);
    if (rc != 0) return vm_fail(vm, insert_error(rc));
    return value_new_none();
}

static Value *set_remove(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (!value_is_hashable(args[0])) return vm_fail(vm, VM_ERR_TYPE);
    if (!value_set_discard(self, args[0])) return vm_fail(vm, VM_ERR_KEY);
    return value_new_none();
}

static Value *set_discard(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (!value_is_hashable(args[0])) return vm_fail(vm, VM_ERR_TYPE);
    value_set_discard(self, args[0]);
    return value_new_none();
}

static Value *set_pop(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    if (self->data.set.len == 0) return vm_fail(vm, VM_ERR_KEY);
    self->data.set.len--;
    return self->data.set.items[self->data.set.len];
}

static Value *set_clear(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)args;
    if (check_positional(vm, nargs, kwnames, 0, 0) != 0) return NULL;
    for (uint32_t i = 0; i < self->data.set.len; i++) {
        value_release(self->data.set.items[i]);
    }
    self->data.set.len = 0;
    return value_new_none();
}

static Value *set_update(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (reject_kwargs(vm, kwnames) != 0) return NULL;
    for (uint32_t i = 0; i < nargs; i++) {
        if (value_set_update(vm, self, args[i]) != 0) return NULL;
    }
    return value_new_none();
}

static Value *replace_contents(VM *vm, Value *self, Value *computed) {
    if (!computed) return NULL;

    for (uint32_t i = 0; i < self->data.set.len; i++) {
        value_release(self->data.set.items[i]);
    }
    self->data.set.len = 0;

    for (uint32_t i = 0; i < computed->data.set.len; i++) {
        if (value_set_add(self, computed->data.set.items[i]) != 0) {
            value_release(computed);
            return vm_fail(vm, VM_ERR_OOM);
        }
    }

    value_release(computed);
    return value_new_none();
}

static Value *set_intersection_update(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (reject_kwargs(vm, kwnames) != 0) return NULL;
    return replace_contents(vm, self, setlike_intersection(vm, self, nargs, args, 0));
}

static Value *set_difference_update(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (reject_kwargs(vm, kwnames) != 0) return NULL;
    for (uint32_t i = 0; i < nargs; i++) {
        Value *iter = value_make_iter(vm, args[i]);
        if (!iter) return NULL;
        for (;;) {
            Value *item = iterator_next(vm, iter);
            if (!item) break;
            value_set_discard(self, item);
            value_release(item);
        }
        value_release(iter);
        if (vm->last_error != VM_ERR_OK) return NULL;
    }
    return value_new_none();
}

static Value *set_symmetric_difference_update(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    return replace_contents(vm, self, setlike_symdiff(vm, self, args[0], 0));
}

const MethodEntry SET_METHODS[] = {
    {"add", set_add},
    {"remove", set_remove},
    {"discard", set_discard},
    {"pop", set_pop},
    {"clear", set_clear},
    {"copy", set_copy},
    {"update", set_update},
    {"union", set_union},
    {"intersection", set_intersection},
    {"difference", set_difference},
    {"symmetric_difference", set_symmetric_difference},
    {"intersection_update", set_intersection_update},
    {"difference_update", set_difference_update},
    {"symmetric_difference_update", set_symmetric_difference_update},
    {"issubset", set_issubset},
    {"issuperset", set_issuperset},
    {"isdisjoint", set_isdisjoint},
};

const uint32_t SET_METHOD_COUNT = (uint32_t)(sizeof(SET_METHODS) / sizeof(SET_METHODS[0]));

const MethodEntry FROZENSET_METHODS[] = {
    {"copy", set_copy},
    {"union", set_union},
    {"intersection", set_intersection},
    {"difference", set_difference},
    {"symmetric_difference", set_symmetric_difference},
    {"issubset", set_issubset},
    {"issuperset", set_issuperset},
    {"isdisjoint", set_isdisjoint},
};

const uint32_t FROZENSET_METHOD_COUNT = (uint32_t)(sizeof(FROZENSET_METHODS) / sizeof(FROZENSET_METHODS[0]));
