#include "opcodes.h"
#include "vm_internal.h"

int is_setlike(const Value *v) {
    return v && (v->tag == TAG_SET || v->tag == TAG_FROZENSET);
}

static Value *new_setlike(int frozen) {
    return frozen ? value_new_frozenset() : value_new_set();
}

static Value *copy_setlike(VM *vm, const Value *src, int frozen) {
    Value *out = new_setlike(frozen);
    if (!out) return vm_fail(vm, VM_ERR_OOM);

    for (uint32_t i = 0; i < src->data.set.len; i++) {
        if (value_set_add(out, src->data.set.items[i]) != 0) {
            value_release(out);
            return vm_fail(vm, VM_ERR_OOM);
        }
    }

    return out;
}

Value *setlike_union(VM *vm, const Value *self, uint32_t nothers, Value **others, int frozen) {
    Value *out = copy_setlike(vm, self, frozen);
    if (!out) return NULL;

    for (uint32_t i = 0; i < nothers; i++) {
        if (value_set_update(vm, out, others[i]) != 0) {
            value_release(out);
            return NULL;
        }
    }

    return out;
}

static Value *materialize(VM *vm, const Value *other) {
    if (is_setlike(other)) return value_retain((Value *)other);
    Value *tmp = value_new_set();
    if (!tmp) return vm_fail(vm, VM_ERR_OOM);
    if (value_set_update(vm, tmp, other) != 0) {
        value_release(tmp);
        return NULL;
    }
    return tmp;
}

Value *setlike_intersection(VM *vm, const Value *self, uint32_t nothers, Value **others, int frozen) {
    Value *out = copy_setlike(vm, self, frozen);
    if (!out) return NULL;

    for (uint32_t i = 0; i < nothers; i++) {
        Value *other = materialize(vm, others[i]);
        if (!other) {
            value_release(out);
            return NULL;
        }

        Value *narrowed = new_setlike(frozen);
        if (!narrowed) {
            value_release(other);
            value_release(out);
            return vm_fail(vm, VM_ERR_OOM);
        }

        for (uint32_t j = 0; j < out->data.set.len; j++) {
            if (value_set_contains(other, out->data.set.items[j])) {
                if (value_set_add(narrowed, out->data.set.items[j]) != 0) {
                    value_release(narrowed);
                    value_release(other);
                    value_release(out);
                    return vm_fail(vm, VM_ERR_OOM);
                }
            }
        }

        value_release(other);
        value_release(out);
        out = narrowed;
    }

    return out;
}

Value *setlike_difference(VM *vm, const Value *self, uint32_t nothers, Value **others, int frozen) {
    Value *out = copy_setlike(vm, self, frozen);
    if (!out) return NULL;

    for (uint32_t i = 0; i < nothers; i++) {
        Value *iter = value_make_iter(vm, others[i]);
        if (!iter) {
            value_release(out);
            return NULL;
        }

        for (;;) {
            Value *item = iterator_next(vm, iter);
            if (!item) break;
            value_set_discard(out, item);
            value_release(item);
        }

        value_release(iter);
        if (vm->last_error != VM_ERR_OK) {
            value_release(out);
            return NULL;
        }
    }

    return out;
}

Value *setlike_symdiff(VM *vm, const Value *self, const Value *other_in, int frozen) {
    Value *other = materialize(vm, other_in);
    if (!other) return NULL;

    Value *out = new_setlike(frozen);
    if (!out) {
        value_release(other);
        return vm_fail(vm, VM_ERR_OOM);
    }

    int ok = 1;
    for (uint32_t i = 0; ok && i < self->data.set.len; i++) {
        if (!value_set_contains(other, self->data.set.items[i])) {
            if (value_set_add(out, self->data.set.items[i]) != 0) ok = 0;
        }
    }
    for (uint32_t i = 0; ok && i < other->data.set.len; i++) {
        if (!value_set_contains(self, other->data.set.items[i])) {
            if (value_set_add(out, other->data.set.items[i]) != 0) ok = 0;
        }
    }

    value_release(other);
    if (!ok) {
        value_release(out);
        return vm_fail(vm, VM_ERR_OOM);
    }
    return out;
}

int setlike_subset(const Value *a, const Value *b) {
    for (uint32_t i = 0; i < a->data.set.len; i++) {
        if (!value_set_contains(b, a->data.set.items[i])) return 0;
    }
    return 1;
}

Value *set_binary_op(VM *vm, uint8_t op, const Value *a, const Value *b) {
    int frozen = a->tag == TAG_FROZENSET;
    Value *other = (Value *)b;

    switch (op) {
        case OP_BINARY_OR: return setlike_union(vm, a, 1, &other, frozen);
        case OP_BINARY_AND: return setlike_intersection(vm, a, 1, &other, frozen);
        case OP_BINARY_SUB: return setlike_difference(vm, a, 1, &other, frozen);
        case OP_BINARY_XOR: return setlike_symdiff(vm, a, b, frozen);
        default: return vm_fail(vm, VM_ERR_TYPE);
    }
}

Value *value_dict_union(VM *vm, const Value *a, const Value *b) {
    Value *out = value_new_dict();
    if (!out) return vm_fail(vm, VM_ERR_OOM);

    if (value_dict_update(vm, out, a) != 0 || value_dict_update(vm, out, b) != 0) {
        value_release(out);
        return NULL;
    }

    return out;
}
