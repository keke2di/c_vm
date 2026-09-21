#include <stdlib.h>
#include "vm_internal.h"

#define SMALL_MAP_ARGS 8

Value *value_make_iter(VM *vm, const Value *iterable) {
    if (!iterable) return vm_fail(vm, VM_ERR_STACK);

    if (iterable->tag == TAG_ITERATOR) {
        return value_retain((Value *)iterable);
    }

    int64_t length = value_length(iterable);
    if (length < 0) return vm_fail(vm, VM_ERR_TYPE);

    Value *iter = value_new_iterator(ITER_SEQ);
    if (!iter) return vm_fail(vm, VM_ERR_OOM);

    iter->data.iter->source = value_retain((Value *)iterable);
    iter->data.iter->length = length;
    return iter;
}

static Value *exhausted(IterObject *it) {
    it->done = 1;
    return NULL;
}

static Value *seq_next(VM *vm, IterObject *it) {
    Value *src = it->source;
    Value *item;

    if (it->done) return NULL;

    switch (src->tag) {
        case TAG_STRING: {
            size_t offset = (size_t)it->index;
            if (offset >= src->data.str.len) return exhausted(it);
            size_t size = utf8_char_size(src->data.str.data, src->data.str.len, offset);
            it->index += (int64_t)size;
            item = value_new_string_len(src->data.str.data + offset, size);
            break;
        }

        case TAG_LIST:
            if (it->index >= (int64_t)src->data.list.len) return exhausted(it);
            return value_retain(src->data.list.items[it->index++]);

        case TAG_DICT:
        case TAG_SET:
        case TAG_DICT_VIEW:
            if (value_length(src) != it->length) return vm_fail(vm, VM_ERR_RUNTIME);
            if (it->index >= it->length) return exhausted(it);
            item = value_item_at(src, it->index++);
            break;

        default:
            if (it->index >= it->length) return exhausted(it);
            item = value_item_at(src, it->index++);
            break;
    }

    return item ? item : vm_fail(vm, VM_ERR_OOM);
}

static Value *reversed_next(VM *vm, IterObject *it) {
    Value *src = it->source;
    Value *item;

    if (it->done) return NULL;

    if (src->tag == TAG_STRING) {
        size_t end = (size_t)it->index;
        if (end == 0) return exhausted(it);
        size_t start = end - 1;
        while (start > 0 && ((unsigned char)src->data.str.data[start] & 0xC0) == 0x80) {
            start--;
        }
        it->index = (int64_t)start;
        item = value_new_string_len(src->data.str.data + start, end - start);
        return item ? item : vm_fail(vm, VM_ERR_OOM);
    }

    if ((src->tag == TAG_DICT || src->tag == TAG_DICT_VIEW) && value_length(src) != it->length) {
        return vm_fail(vm, VM_ERR_RUNTIME);
    }

    if (it->index < 0 || it->index >= value_length(src)) return exhausted(it);

    item = value_item_at(src, it->index--);
    return item ? item : vm_fail(vm, VM_ERR_OOM);
}

static void strict_exhausted(VM *vm, IterObject *it, uint32_t index) {
    if (index > 0) {
        vm->last_error = VM_ERR_VALUE;
        return;
    }

    for (uint32_t i = 1; i < it->nsubs; i++) {
        Value *item = iterator_next(vm, it->subs[i]);
        if (item) {
            value_release(item);
            vm->last_error = VM_ERR_VALUE;
            return;
        }
        if (vm->last_error != VM_ERR_OK) return;
    }
}

static Value *enumerate_next(VM *vm, IterObject *it) {
    Value *item = iterator_next(vm, it->source);
    if (!item) return NULL;

    Value *tuple = value_new_tuple(2);
    Value *index = value_new_int(it->counter);
    if (!tuple || !index) {
        value_release(item);
        value_release(index);
        value_release(tuple);
        return vm_fail(vm, VM_ERR_OOM);
    }

    tuple->data.tuple.items[0] = index;
    tuple->data.tuple.items[1] = item;
    it->counter++;
    return tuple;
}

static Value *zip_next(VM *vm, IterObject *it) {
    if (it->nsubs == 0) return NULL;

    Value *tuple = value_new_tuple(it->nsubs);
    if (!tuple) return vm_fail(vm, VM_ERR_OOM);

    for (uint32_t i = 0; i < it->nsubs; i++) {
        Value *item = iterator_next(vm, it->subs[i]);
        if (!item) {
            value_release(tuple);
            if (vm->last_error == VM_ERR_OK && it->strict) {
                strict_exhausted(vm, it, i);
            }
            return NULL;
        }
        tuple->data.tuple.items[i] = item;
    }

    return tuple;
}

static Value *map_next(VM *vm, IterObject *it) {
    Value *small[SMALL_MAP_ARGS];
    Value **margs = small;

    if (it->nsubs > SMALL_MAP_ARGS) {
        margs = malloc(it->nsubs * sizeof(Value *));
        if (!margs) return vm_fail(vm, VM_ERR_OOM);
    }

    uint32_t got = 0;
    while (got < it->nsubs) {
        Value *item = iterator_next(vm, it->subs[got]);
        if (!item) break;
        margs[got++] = item;
    }

    Value *result = NULL;

    if (got == it->nsubs) {
        vm_call_sync(vm, it->func, margs, got, &result);
    } else if (vm->last_error == VM_ERR_OK && it->strict) {
        strict_exhausted(vm, it, got);
    }

    for (uint32_t j = 0; j < got; j++) {
        value_release(margs[j]);
    }
    if (margs != small) free(margs);

    return result;
}

static Value *filter_next(VM *vm, IterObject *it) {
    for (;;) {
        Value *item = iterator_next(vm, it->source);
        if (!item) return NULL;

        int keep;
        if (it->func->tag == TAG_NONE) {
            keep = value_truthy(item);
        } else {
            Value *call_args[1] = { item };
            Value *res = NULL;
            if (vm_call_sync(vm, it->func, call_args, 1, &res) != 0) {
                value_release(item);
                return NULL;
            }
            keep = value_truthy(res);
            value_release(res);
        }

        if (keep) return item;
        value_release(item);
    }
}

static Value *callable_next(VM *vm, IterObject *it) {
    if (it->done) return NULL;

    Value *result = NULL;
    if (vm_call_sync(vm, it->func, NULL, 0, &result) != 0) return NULL;

    if (value_equal(result, it->source)) {
        value_release(result);
        return exhausted(it);
    }

    return result;
}

Value *iterator_next(VM *vm, Value *iter) {
    if (!iter || iter->tag != TAG_ITERATOR) return vm_fail(vm, VM_ERR_TYPE);

    IterObject *it = iter->data.iter;

    switch (it->kind) {
        case ITER_SEQ: return seq_next(vm, it);
        case ITER_REVERSED: return reversed_next(vm, it);
        case ITER_ENUMERATE: return enumerate_next(vm, it);
        case ITER_ZIP: return zip_next(vm, it);
        case ITER_MAP: return map_next(vm, it);
        case ITER_FILTER: return filter_next(vm, it);
        case ITER_CALLABLE: return callable_next(vm, it);
        default: return vm_fail(vm, VM_ERR_TYPE);
    }
}

int value_list_extend(VM *vm, Value *list, const Value *iterable) {
    Value *iter = value_make_iter(vm, iterable);
    if (!iter) return -1;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        int rc = value_list_append(list, item);
        value_release(item);

        if (rc != 0) {
            value_release(iter);
            vm->last_error = VM_ERR_OOM;
            return -1;
        }
    }

    value_release(iter);
    return vm->last_error == VM_ERR_OK ? 0 : -1;
}

void op_list_extend(VM *vm) {
    Value *iterable = vm_pop(vm);
    Value *list = vm_pop(vm);

    if (!iterable || !list) {
        if (iterable) value_release(iterable);
        if (list) value_release(list);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (list->tag != TAG_LIST) {
        vm->last_error = VM_ERR_TYPE;
    } else {
        value_list_extend(vm, list, iterable);
    }

    value_release(iterable);
    value_release(list);
}

void op_dict_merge(VM *vm) {
    Value *other = vm_pop(vm);
    Value *target = vm_pop(vm);

    if (!other || !target) {
        if (other) value_release(other);
        if (target) value_release(target);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (target->tag != TAG_DICT || other->tag != TAG_DICT) {
        vm->last_error = VM_ERR_TYPE;
    } else {
        for (uint32_t i = 0; i < other->data.dict.len; i++) {
            Value *key = other->data.dict.entries[i].key;
            Value *value = other->data.dict.entries[i].value;

            if (value_dict_get(target, key) != NULL) {
                vm->last_error = VM_ERR_TYPE;
                break;
            }

            int rc = value_dict_set(target, key, value);
            if (rc != 0) {
                vm->last_error = insert_error(rc);
                break;
            }
        }
    }

    value_release(other);
    value_release(target);
}

Value *value_list_from_iterable(VM *vm, const Value *iterable) {
    Value *iter = value_make_iter(vm, iterable);
    if (!iter) return NULL;

    Value *list = value_new_list();
    if (!list) {
        value_release(iter);
        return vm_fail(vm, VM_ERR_OOM);
    }

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;
        int rc = value_list_append(list, item);
        value_release(item);
        if (rc != 0) {
            vm->last_error = VM_ERR_OOM;
            break;
        }
    }

    value_release(iter);

    if (vm->last_error != VM_ERR_OK) {
        value_release(list);
        return NULL;
    }

    return list;
}

int value_set_update(VM *vm, Value *set, const Value *iterable) {
    Value *iter = value_make_iter(vm, iterable);
    if (!iter) return -1;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;
        int rc = value_set_add(set, item);
        value_release(item);
        if (rc != 0) {
            vm->last_error = insert_error(rc);
            break;
        }
    }

    value_release(iter);
    return vm->last_error == VM_ERR_OK ? 0 : -1;
}

int value_dict_update(VM *vm, Value *dict, const Value *source) {
    if (source->tag == TAG_DICT) {
        for (uint32_t i = 0; i < source->data.dict.len; i++) {
            int rc = value_dict_set(dict, source->data.dict.entries[i].key,
                                    source->data.dict.entries[i].value);
            if (rc != 0) {
                vm->last_error = insert_error(rc);
                return -1;
            }
        }
        return 0;
    }

    Value *iter = value_make_iter(vm, source);
    if (!iter) return -1;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        Value *pair = value_list_from_iterable(vm, item);
        value_release(item);
        if (!pair) break;

        if (pair->data.list.len != 2) {
            vm->last_error = VM_ERR_VALUE;
        } else {
            int rc = value_dict_set(dict, pair->data.list.items[0], pair->data.list.items[1]);
            if (rc != 0) vm->last_error = insert_error(rc);
        }

        value_release(pair);
        if (vm->last_error != VM_ERR_OK) break;
    }

    value_release(iter);
    return vm->last_error == VM_ERR_OK ? 0 : -1;
}

void op_get_iter(VM *vm) {
    Value *v = vm_pop(vm);
    if (!v) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *iter = value_make_iter(vm, v);
    value_release(v);

    if (iter) {
        vm_push_owned(vm, iter);
    }
}

void op_unpack_sequence(VM *vm, uint32_t n) {
    Value *iterable = vm_pop(vm);
    if (!iterable) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *iter = value_make_iter(vm, iterable);
    value_release(iterable);
    if (!iter) return;

    Value **items = n ? malloc(n * sizeof(Value *)) : NULL;
    if (n && !items) {
        value_release(iter);
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t got = 0;
    int err = 0;
    for (uint32_t i = 0; i < n; i++) {
        Value *item = iterator_next(vm, iter);
        if (vm->last_error != VM_ERR_OK) {
            err = 1;
            break;
        }
        if (!item) {
            vm->last_error = VM_ERR_VALUE;
            err = 1;
            break;
        }
        items[got++] = item;
    }

    if (!err) {
        Value *extra = iterator_next(vm, iter);
        if (vm->last_error != VM_ERR_OK) {
            err = 1;
        } else if (extra) {
            value_release(extra);
            vm->last_error = VM_ERR_VALUE;
            err = 1;
        }
    }

    value_release(iter);

    if (err) {
        for (uint32_t i = 0; i < got; i++) value_release(items[i]);
        free(items);
        return;
    }

    for (uint32_t i = 0; i < n; i++) {
        vm_push_owned(vm, items[n - 1 - i]);
    }
    free(items);
}

void op_unpack_ex(VM *vm, uint32_t before, uint32_t after) {
    Value *iterable = vm_pop(vm);
    if (!iterable) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *iter = value_make_iter(vm, iterable);
    value_release(iterable);
    if (!iter) return;

    Value **items = NULL;
    uint32_t count = 0;
    uint32_t cap = 0;
    int err = 0;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (vm->last_error != VM_ERR_OK) {
            err = 1;
            break;
        }
        if (!item) break;
        if (count == cap) {
            uint32_t newcap = cap ? cap * 2 : 8;
            Value **grown = realloc(items, newcap * sizeof(Value *));
            if (!grown) {
                value_release(item);
                vm->last_error = VM_ERR_OOM;
                err = 1;
                break;
            }
            items = grown;
            cap = newcap;
        }
        items[count++] = item;
    }

    value_release(iter);

    if (!err && count < (uint64_t)before + after) {
        vm->last_error = VM_ERR_VALUE;
        err = 1;
    }

    if (err) {
        for (uint32_t i = 0; i < count; i++) value_release(items[i]);
        free(items);
        return;
    }

    uint32_t middle = count - before - after;
    Value *mid_list = value_new_list();
    if (!mid_list) {
        for (uint32_t i = 0; i < count; i++) value_release(items[i]);
        free(items);
        vm->last_error = VM_ERR_OOM;
        return;
    }

    for (uint32_t i = 0; i < middle; i++) {
        if (value_list_append(mid_list, items[before + i]) != 0) {
            value_release(mid_list);
            for (uint32_t j = 0; j < count; j++) value_release(items[j]);
            free(items);
            vm->last_error = VM_ERR_OOM;
            return;
        }
    }

    for (uint32_t i = 0; i < middle; i++) {
        value_release(items[before + i]);
    }

    for (uint32_t i = count; i > before + middle; i--) {
        vm_push_owned(vm, items[i - 1]);
    }
    vm_push_owned(vm, mid_list);
    for (uint32_t i = before; i > 0; i--) {
        vm_push_owned(vm, items[i - 1]);
    }

    free(items);
}
