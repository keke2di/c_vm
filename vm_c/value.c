#include "value.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Value g_none = { TAG_NONE, UINT32_MAX, { 0 } };
static Value g_true = { TAG_BOOL, UINT32_MAX, { 1 } };
static Value g_false = { TAG_BOOL, UINT32_MAX, { 0 } };

static Value *value_alloc(ValueTag tag) {
    Value *v = malloc(sizeof(Value));

    if (!v) return NULL;

    v->tag = tag;
    v->refcount = 1;
    memset(&v->data, 0, sizeof(v->data));

    return v;
}

Value *value_new_int(int64_t i) {
    Value *v = value_alloc(TAG_INT);

    if (v) {
        v->data.int_val = i;
    }

    return v;
}

Value *value_new_float(double f) {
    Value *v = value_alloc(TAG_FLOAT);

    if (v) {
        v->data.float_val = f;
    }

    return v;
}

Value *value_new_string(const char *s) {
    if (!s) return NULL;

    return value_new_string_len(s, strlen(s));
}

Value *value_new_string_len(const char *data, size_t len) {
    if (len > UINT32_MAX) return NULL;
    if (len > 0 && !data) return NULL;

    Value *v = value_alloc(TAG_STRING);

    if (!v) return NULL;

    v->data.str.data = malloc(len + 1);

    if (!v->data.str.data) {
        free(v);
        return NULL;
    }

    if (len > 0) {
        memcpy(v->data.str.data, data, len);
    }

    v->data.str.data[len] = '\0';
    v->data.str.len = (uint32_t)len;

    return v;
}

Value *value_new_bytes(const unsigned char *data, size_t len) {
    if (len > UINT32_MAX) return NULL;
    if (len > 0 && !data) return NULL;

    Value *v = value_alloc(TAG_BYTES);

    if (!v) return NULL;

    v->data.bytes.data = NULL;
    v->data.bytes.len = (uint32_t)len;

    if (len > 0) {
        v->data.bytes.data = malloc(len);

        if (!v->data.bytes.data) {
            free(v);
            return NULL;
        }

        memcpy(v->data.bytes.data, data, len);
    }

    return v;
}

Value *value_new_tuple(size_t len) {
    if (len > UINT32_MAX) return NULL;

    Value *v = value_alloc(TAG_TUPLE);

    if (!v) return NULL;

    v->data.tuple.items = NULL;
    v->data.tuple.len = (uint32_t)len;

    if (len > 0) {
        v->data.tuple.items = calloc(len, sizeof(Value *));

        if (!v->data.tuple.items) {
            free(v);
            return NULL;
        }
    }

    return v;
}

Value *value_new_list(void) {
    Value *v = value_alloc(TAG_LIST);

    if (!v) return NULL;

    v->data.list.items = NULL;
    v->data.list.len = 0;
    v->data.list.cap = 0;

    return v;
}

Value *value_new_dict(void) {
    Value *v = value_alloc(TAG_DICT);

    if (!v) return NULL;

    v->data.dict.entries = NULL;
    v->data.dict.len = 0;
    v->data.dict.cap = 0;

    return v;
}

Value *value_new_set(void) {
    Value *v = value_alloc(TAG_SET);

    if (!v) return NULL;

    v->data.set.items = NULL;
    v->data.set.len = 0;
    v->data.set.cap = 0;

    return v;
}

Value *value_new_frozenset(void) {
    Value *v = value_new_set();

    if (v) {
        v->tag = TAG_FROZENSET;
    }

    return v;
}

Value *value_new_dict_view(Value *dict, ViewKind kind) {
    Value *v = value_alloc(TAG_DICT_VIEW);

    if (!v) return NULL;

    v->data.view.dict = value_retain(dict);
    v->data.view.kind = kind;
    return v;
}

Value *value_new_none(void) {
    return &g_none;
}

Value *value_true(void) {
    return &g_true;
}

Value *value_false(void) {
    return &g_false;
}

Value *value_bool(int b) {
    return b ? &g_true : &g_false;
}

Value *value_new_function(FunctionKind kind, uint32_t index, const char *name) {
    Value *v = value_alloc(TAG_FUNCTION);

    if (!v) return NULL;

    v->data.func = malloc(sizeof(Function));
    if (!v->data.func) {
        free(v);
        return NULL;
    }

    memset(v->data.func, 0, sizeof(Function));
    v->data.func->kind = kind;
    v->data.func->index = index;
    v->data.func->name = name;
    return v;
}

Value *value_new_cell(void) {
    return value_alloc(TAG_CELL);
}

Value *value_cell_get(const Value *cell) {
    if (!cell || cell->tag != TAG_CELL) return NULL;
    return (Value *)cell->data.ptr;
}

void value_cell_set(Value *cell, Value *value) {
    if (!cell || cell->tag != TAG_CELL) return;

    Value *old = (Value *)cell->data.ptr;
    cell->data.ptr = value;

    if (old) value_release(old);
}

int64_t value_range_len(int64_t start, int64_t stop, int64_t step) {
    if (step > 0) {
        if (stop <= start) return 0;
        return ((int64_t)((uint64_t)stop - (uint64_t)start) - 1) / step + 1;
    }
    if (stop >= start) return 0;
    return ((int64_t)((uint64_t)start - (uint64_t)stop) - 1) / (-step) + 1;
}

Value *value_new_range(int64_t start, int64_t stop, int64_t step) {
    Value *v = value_alloc(TAG_RANGE);

    if (!v) return NULL;

    v->data.range.start = start;
    v->data.range.stop = stop;
    v->data.range.step = step;
    return v;
}

Value *value_new_iterator(IterKind kind) {
    Value *v = value_alloc(TAG_ITERATOR);

    if (!v) return NULL;

    v->data.iter = calloc(1, sizeof(IterObject));
    if (!v->data.iter) {
        free(v);
        return NULL;
    }

    v->data.iter->kind = kind;
    return v;
}

Value *value_new_object(void) {
    return value_alloc(TAG_OBJECT);
}

Value *value_retain(Value *v) {
    if (v && v->refcount != UINT32_MAX) {
        v->refcount++;
    }

    return v;
}

void value_release(Value *v) {
    if (!v || v->refcount == UINT32_MAX) return;

    v->refcount--;

    if (v->refcount != 0) return;

    switch (v->tag) {
        case TAG_STRING:
            free(v->data.str.data);
            break;

        case TAG_BYTES:
            free(v->data.bytes.data);
            break;

        case TAG_TUPLE:
            if (v->data.tuple.items) {
                for (uint32_t i = 0; i < v->data.tuple.len; i++) {
                    value_release(v->data.tuple.items[i]);
                }

                free(v->data.tuple.items);
            }
            break;

        case TAG_LIST:
            if (v->data.list.items) {
                for (uint32_t i = 0; i < v->data.list.len; i++) {
                    value_release(v->data.list.items[i]);
                }

                free(v->data.list.items);
            }
            break;

        case TAG_DICT:
            if (v->data.dict.entries) {
                for (uint32_t i = 0; i < v->data.dict.len; i++) {
                    value_release(v->data.dict.entries[i].key);
                    value_release(v->data.dict.entries[i].value);
                }

                free(v->data.dict.entries);
            }
            free(v->data.dict.index);
            break;

        case TAG_SET:
        case TAG_FROZENSET:
            if (v->data.set.items) {
                for (uint32_t i = 0; i < v->data.set.len; i++) {
                    value_release(v->data.set.items[i]);
                }

                free(v->data.set.items);
            }
            free(v->data.set.hashes);
            free(v->data.set.index);
            break;

        case TAG_DICT_VIEW:
            value_release(v->data.view.dict);
            break;

        case TAG_FUNCTION:
            if (v->data.func) {
                if (v->data.func->defaults) value_release(v->data.func->defaults);
                if (v->data.func->kwdefaults) value_release(v->data.func->kwdefaults);
                if (v->data.func->cells) {
                    for (uint32_t i = 0; i < v->data.func->ncells; i++) {
                        value_release(v->data.func->cells[i]);
                    }
                    free(v->data.func->cells);
                }
            }
            free(v->data.func);
            break;

        case TAG_CELL: {
            Value *inner = (Value *)v->data.ptr;
            if (inner) value_release(inner);
            break;
        }

        case TAG_EXCEPTION:
            if (v->data.exception.args) value_release(v->data.exception.args);
            break;

        case TAG_ITERATOR:
            if (v->data.iter) {
                if (v->data.iter->source) value_release(v->data.iter->source);
                if (v->data.iter->func) value_release(v->data.iter->func);
                if (v->data.iter->subs) {
                    for (uint32_t i = 0; i < v->data.iter->nsubs; i++) {
                        value_release(v->data.iter->subs[i]);
                    }
                    free(v->data.iter->subs);
                }
                free(v->data.iter);
            }
            break;

        default:
            break;
    }

    free(v);
}

size_t value_string_len(const Value *v) {
    if (!v || v->tag != TAG_STRING) return 0;

    return v->data.str.len;
}

Value *value_list_get(const Value *v, size_t idx) {
    if (!v || v->tag != TAG_LIST) return NULL;
    if (idx >= v->data.list.len) return NULL;

    return v->data.list.items[idx];
}

void value_list_set(Value *v, size_t idx, Value *item) {
    if (!v || v->tag != TAG_LIST) return;
    if (idx >= v->data.list.len) return;

    Value *old = v->data.list.items[idx];
    v->data.list.items[idx] = value_retain(item);
    value_release(old);
}

int value_list_append(Value *list, Value *item) {
    if (!list || list->tag != TAG_LIST) return -1;

    if (list->data.list.len >= list->data.list.cap) {
        uint32_t new_cap =
            (list->data.list.cap == 0)
                ? 4
                : list->data.list.cap * 2;

        if (new_cap < list->data.list.cap) return -1;

        Value **new_items = realloc(
            list->data.list.items,
            new_cap * sizeof(Value *)
        );

        if (!new_items) return -1;

        list->data.list.items = new_items;
        list->data.list.cap = new_cap;
    }

    list->data.list.items[list->data.list.len] = value_retain(item);
    list->data.list.len++;

    return 0;
}

Value *value_tuple_get(const Value *v, size_t idx) {
    if (!v || v->tag != TAG_TUPLE) return NULL;
    if (idx >= v->data.tuple.len) return NULL;

    return v->data.tuple.items[idx];
}

int value_is_hashable(const Value *v) {
    if (!v) return 0;

    switch (v->tag) {
        case TAG_LIST:
        case TAG_DICT:
        case TAG_SET:
        case TAG_DICT_VIEW:
            return 0;

        case TAG_TUPLE:
            for (uint32_t i = 0; i < v->data.tuple.len; i++) {
                if (!value_is_hashable(v->data.tuple.items[i])) return 0;
            }
            return 1;

        default:
            return 1;
    }
}

#define INDEX_MIN_CAP 8

static uint32_t index_capacity_for(uint32_t count) {
    uint32_t needed = count * 3 / 2 + 1;
    uint32_t cap = INDEX_MIN_CAP;

    while (cap < needed) {
        if (cap > UINT32_MAX / 2) return cap;
        cap *= 2;
    }

    return cap;
}

static uint32_t index_slot(int64_t hash, uint32_t mask) {
    return (uint32_t)((uint64_t)hash & mask);
}

static int dict_index_build(DictObject *dict, uint32_t cap) {
    if (cap == 0) {
        free(dict->index);
        dict->index = NULL;
        dict->index_cap = 0;
        dict->index_used = 0;
        return 0;
    }

    uint32_t *index = calloc(cap, sizeof(uint32_t));
    if (!index) return -1;

    free(dict->index);
    dict->index = index;
    dict->index_cap = cap;
    dict->index_used = 0;

    uint32_t mask = cap - 1;

    for (uint32_t i = 0; i < dict->len; i++) {
        uint32_t slot = index_slot(dict->entries[i].hash, mask);

        while (index[slot]) {
            slot = (slot + 1) & mask;
        }

        index[slot] = i + 1;
        dict->index_used++;
    }

    return 0;
}

static int dict_index_ready(DictObject *dict) {
    if (dict->index && dict->index_used == dict->len) return 0;
    return dict_index_build(dict, dict->len == 0 ? 0 : index_capacity_for(dict->len));
}

static int dict_index_reserve(DictObject *dict) {
    if (dict->index && (dict->index_used + 1) * 3 <= dict->index_cap * 2) return 0;
    return dict_index_build(dict, index_capacity_for(dict->len + 1));
}

static int dict_index_lookup(const DictObject *dict, const Value *key, int64_t hash) {
    if (!dict->index || dict->index_cap == 0) return -1;

    uint32_t mask = dict->index_cap - 1;
    uint32_t slot = index_slot(hash, mask);

    while (dict->index[slot]) {
        uint32_t entry = dict->index[slot] - 1;

        if (dict->entries[entry].hash == hash &&
            value_equal(dict->entries[entry].key, key)) {
            return (int)entry;
        }

        slot = (slot + 1) & mask;
    }

    return -1;
}

static int dict_index_insert(DictObject *dict, uint32_t entry) {
    uint32_t mask = dict->index_cap - 1;
    uint32_t slot = index_slot(dict->entries[entry].hash, mask);

    while (dict->index[slot]) {
        slot = (slot + 1) & mask;
    }

    dict->index[slot] = entry + 1;
    dict->index_used++;
    return 0;
}

static int dict_entries_reserve(DictObject *dict) {
    if (dict->len < dict->cap) return 0;

    uint32_t new_cap = (dict->cap == 0) ? 4 : dict->cap * 2;
    if (new_cap < dict->cap) return -1;

    DictEntry *entries = realloc(dict->entries, new_cap * sizeof(DictEntry));
    if (!entries) return -1;

    dict->entries = entries;
    dict->cap = new_cap;
    return 0;
}

int value_dict_set(Value *dict, Value *key, Value *value) {
    if (!dict || dict->tag != TAG_DICT) return -1;
    if (!value_is_hashable(key)) return -2;

    int error = 0;
    int64_t hash = value_hash(key, &error);
    if (error) return -2;

    DictObject *data = &dict->data.dict;
    if (dict_index_ready(data) != 0) return -1;

    int found = dict_index_lookup(data, key, hash);

    if (found >= 0) {
        Value *old = data->entries[found].value;
        data->entries[found].value = value_retain(value);
        value_release(old);
        return 0;
    }

    if (dict_entries_reserve(data) != 0) return -1;
    if (dict_index_reserve(data) != 0) return -1;

    data->entries[data->len].key = value_retain(key);
    data->entries[data->len].value = value_retain(value);
    data->entries[data->len].hash = hash;
    data->len++;

    return dict_index_insert(data, data->len - 1);
}

Value *value_dict_get(const Value *dict, Value *key) {
    if (!dict || dict->tag != TAG_DICT) return NULL;
    if (!value_is_hashable(key)) return NULL;

    int error = 0;
    int64_t hash = value_hash(key, &error);
    if (error) return NULL;

    DictObject *data = (DictObject *)&dict->data.dict;
    if (dict_index_ready(data) != 0) return NULL;

    int found = dict_index_lookup(data, key, hash);
    if (found < 0) return NULL;

    return data->entries[found].value;
}

Value *value_dict_key_at(const Value *dict, size_t index) {
    if (!dict || dict->tag != TAG_DICT) return NULL;
    if (index >= dict->data.dict.len) return NULL;

    return dict->data.dict.entries[index].key;
}

int value_dict_delete(Value *dict, const Value *key) {
    if (!dict || dict->tag != TAG_DICT) return 0;
    if (!value_is_hashable(key)) return 0;

    int error = 0;
    int64_t hash = value_hash(key, &error);
    if (error) return 0;

    DictObject *data = &dict->data.dict;
    if (dict_index_ready(data) != 0) return 0;

    int found = dict_index_lookup(data, key, hash);
    if (found < 0) return 0;

    Value *old_key = data->entries[found].key;
    Value *old_value = data->entries[found].value;

    memmove(&data->entries[found], &data->entries[found + 1],
            (data->len - (uint32_t)found - 1) * sizeof(DictEntry));
    data->len--;

    value_release(old_key);
    value_release(old_value);

    dict_index_build(data, data->len == 0 ? 0 : index_capacity_for(data->len));
    return 1;
}

static int set_index_build(SetObject *set, uint32_t cap) {
    if (cap == 0) {
        free(set->index);
        set->index = NULL;
        set->index_cap = 0;
        set->index_used = 0;
        return 0;
    }

    uint32_t *index = calloc(cap, sizeof(uint32_t));
    if (!index) return -1;

    free(set->index);
    set->index = index;
    set->index_cap = cap;
    set->index_used = 0;

    uint32_t mask = cap - 1;

    for (uint32_t i = 0; i < set->len; i++) {
        uint32_t slot = index_slot(set->hashes[i], mask);

        while (index[slot]) {
            slot = (slot + 1) & mask;
        }

        index[slot] = i + 1;
        set->index_used++;
    }

    return 0;
}

static int set_index_ready(SetObject *set) {
    if (set->index && set->index_used == set->len) return 0;
    return set_index_build(set, set->len == 0 ? 0 : index_capacity_for(set->len));
}

static int set_index_reserve(SetObject *set) {
    if (set->index && (set->index_used + 1) * 3 <= set->index_cap * 2) return 0;
    return set_index_build(set, index_capacity_for(set->len + 1));
}

static int set_index_lookup(const SetObject *set, const Value *item, int64_t hash) {
    if (!set->index || set->index_cap == 0) return -1;

    uint32_t mask = set->index_cap - 1;
    uint32_t slot = index_slot(hash, mask);

    while (set->index[slot]) {
        uint32_t entry = set->index[slot] - 1;

        if (set->hashes[entry] == hash && value_equal(set->items[entry], item)) {
            return (int)entry;
        }

        slot = (slot + 1) & mask;
    }

    return -1;
}

static int set_index_insert(SetObject *set, uint32_t entry) {
    uint32_t mask = set->index_cap - 1;
    uint32_t slot = index_slot(set->hashes[entry], mask);

    while (set->index[slot]) {
        slot = (slot + 1) & mask;
    }

    set->index[slot] = entry + 1;
    set->index_used++;
    return 0;
}

static int set_entries_reserve(SetObject *set) {
    if (set->len < set->cap) return 0;

    uint32_t new_cap = (set->cap == 0) ? 4 : set->cap * 2;
    if (new_cap < set->cap) return -1;

    Value **items = realloc(set->items, new_cap * sizeof(Value *));
    if (!items) return -1;

    int64_t *hashes = realloc(set->hashes, new_cap * sizeof(int64_t));
    if (!hashes) {
        set->items = items;
        return -1;
    }

    set->items = items;
    set->hashes = hashes;
    set->cap = new_cap;
    return 0;
}

int value_set_add(Value *set, Value *item) {
    if (!set || (set->tag != TAG_SET && set->tag != TAG_FROZENSET)) return -1;
    if (!value_is_hashable(item)) return -2;

    int error = 0;
    int64_t hash = value_hash(item, &error);
    if (error) return -2;

    SetObject *data = &set->data.set;
    if (set_index_ready(data) != 0) return -1;
    if (set_index_lookup(data, item, hash) >= 0) return 0;

    if (set_entries_reserve(data) != 0) return -1;
    if (set_index_reserve(data) != 0) return -1;

    data->items[data->len] = value_retain(item);
    data->hashes[data->len] = hash;
    data->len++;

    return set_index_insert(data, data->len - 1);
}

int value_set_contains(const Value *set, Value *item) {
    if (!set || (set->tag != TAG_SET && set->tag != TAG_FROZENSET)) return 0;

    SetObject *data = (SetObject *)&set->data.set;
    int error = 0;
    int64_t hash = 0;

    if (value_is_hashable(item)) {
        hash = value_hash(item, &error);
    } else {
        error = 1;
    }

    if (error) {
        for (uint32_t i = 0; i < data->len; i++) {
            if (value_equal(data->items[i], item)) return 1;
        }
        return 0;
    }

    if (set_index_ready(data) != 0) return 0;

    return set_index_lookup(data, item, hash) >= 0;
}

int value_set_discard(Value *set, const Value *item) {
    if (!set || (set->tag != TAG_SET && set->tag != TAG_FROZENSET)) return 0;
    if (!value_is_hashable(item)) return 0;

    int error = 0;
    int64_t hash = value_hash(item, &error);
    if (error) return 0;

    SetObject *data = &set->data.set;
    if (set_index_ready(data) != 0) return 0;

    int found = set_index_lookup(data, item, hash);
    if (found < 0) return 0;

    Value *removed = data->items[found];
    uint32_t tail = data->len - (uint32_t)found - 1;

    memmove(&data->items[found], &data->items[found + 1], tail * sizeof(Value *));
    memmove(&data->hashes[found], &data->hashes[found + 1], tail * sizeof(int64_t));
    data->len--;

    value_release(removed);

    set_index_build(data, data->len == 0 ? 0 : index_capacity_for(data->len));
    return 1;
}

int value_list_insert(Value *list, int64_t index, Value *item) {
    if (!list || list->tag != TAG_LIST) return -1;

    int64_t len = list->data.list.len;
    if (index < 0) {
        index += len;
        if (index < 0) index = 0;
    } else if (index > len) {
        index = len;
    }

    if (value_list_append(list, item) != 0) return -1;

    Value *inserted = list->data.list.items[len];
    for (int64_t i = len; i > index; i--) {
        list->data.list.items[i] = list->data.list.items[i - 1];
    }
    list->data.list.items[index] = inserted;
    return 0;
}

Value *value_list_remove_at(Value *list, int64_t index) {
    if (!list || list->tag != TAG_LIST) return NULL;
    if (index < 0 || index >= list->data.list.len) return NULL;

    Value *item = list->data.list.items[index];
    memmove(&list->data.list.items[index], &list->data.list.items[index + 1],
            (list->data.list.len - (size_t)index - 1) * sizeof(Value *));
    list->data.list.len--;
    return item;
}

int value_truthy(const Value *v) {
    if (!v) return 0;

    switch (v->tag) {
        case TAG_INT:
            return v->data.int_val != 0;
        case TAG_BOOL:
            return v->data.int_val != 0;
        case TAG_FLOAT:
            return v->data.float_val != 0.0;
        case TAG_STRING:
            return v->data.str.len != 0;
        case TAG_BYTES:
            return v->data.bytes.len != 0;
        case TAG_LIST:
            return v->data.list.len != 0;
        case TAG_TUPLE:
            return v->data.tuple.len != 0;
        case TAG_DICT:
            return v->data.dict.len != 0;
        case TAG_SET:
        case TAG_FROZENSET:
            return v->data.set.len != 0;
        case TAG_DICT_VIEW:
            return v->data.view.dict->data.dict.len != 0;
        case TAG_FUNCTION:
            return 1;
        case TAG_TYPE:
            return 1;
        case TAG_RANGE:
            return value_range_len(
                v->data.range.start,
                v->data.range.stop,
                v->data.range.step) != 0;
        case TAG_ITERATOR:
            return 1;
        case TAG_OBJECT:
            return 1;
        case TAG_EXCEPTION:
            return 1;
        default:
            return 0;
    }
}

int value_compare(const Value *a, const Value *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    int a_int = a->tag == TAG_INT || a->tag == TAG_BOOL;
    int b_int = b->tag == TAG_INT || b->tag == TAG_BOOL;

    if (a_int && b_int) {
        return
            (a->data.int_val < b->data.int_val) ? -1 :
            (a->data.int_val > b->data.int_val) ? 1 : 0;
    }

    if (a->tag != b->tag) {
        return (int)a->tag - (int)b->tag;
    }

    switch (a->tag) {
        case TAG_NONE:
            return 0;

        case TAG_INT:
            return
                (a->data.int_val < b->data.int_val) ? -1 :
                (a->data.int_val > b->data.int_val) ? 1 : 0;

        case TAG_FLOAT:
            return
                (a->data.float_val < b->data.float_val) ? -1 :
                (a->data.float_val > b->data.float_val) ? 1 : 0;

        case TAG_STRING: {
            size_t a_len = a->data.str.len;
            size_t b_len = b->data.str.len;
            size_t common = a_len < b_len ? a_len : b_len;

            if (common > 0) {
                int result = memcmp(
                    a->data.str.data,
                    b->data.str.data,
                    common
                );

                if (result != 0) return result;
            }

            return
                (a_len < b_len) ? -1 :
                (a_len > b_len) ? 1 : 0;
        }

        case TAG_BYTES: {
            size_t a_len = a->data.bytes.len;
            size_t b_len = b->data.bytes.len;
            size_t common = a_len < b_len ? a_len : b_len;

            if (common > 0) {
                int result = memcmp(
                    a->data.bytes.data,
                    b->data.bytes.data,
                    common
                );

                if (result != 0) return result;
            }

            return
                (a_len < b_len) ? -1 :
                (a_len > b_len) ? 1 : 0;
        }

        case TAG_LIST:
        case TAG_TUPLE:
        case TAG_DICT:
        case TAG_SET:
        case TAG_FUNCTION:
            return (a < b) ? -1 : 1;

        default:
            return (a < b) ? -1 : 1;
    }
}
