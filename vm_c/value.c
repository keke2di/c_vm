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

    v->data.func->kind = kind;
    v->data.func->index = index;
    v->data.func->name = name;
    return v;
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
            break;

        case TAG_SET:
        case TAG_FROZENSET:
            if (v->data.set.items) {
                for (uint32_t i = 0; i < v->data.set.len; i++) {
                    value_release(v->data.set.items[i]);
                }

                free(v->data.set.items);
            }
            break;

        case TAG_DICT_VIEW:
            value_release(v->data.view.dict);
            break;

        case TAG_FUNCTION:
            free(v->data.func);
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

int value_dict_set(Value *dict, Value *key, Value *value) {
    if (!dict || dict->tag != TAG_DICT) return -1;
    if (!value_is_hashable(key)) return -2;

    for (uint32_t i = 0; i < dict->data.dict.len; i++) {
        if (value_equal(dict->data.dict.entries[i].key, key)) {
            Value *old = dict->data.dict.entries[i].value;
            dict->data.dict.entries[i].value = value_retain(value);
            value_release(old);
            return 0;
        }
    }

    if (dict->data.dict.len >= dict->data.dict.cap) {
        uint32_t new_cap =
            (dict->data.dict.cap == 0)
                ? 4
                : dict->data.dict.cap * 2;

        if (new_cap < dict->data.dict.cap) return -1;

        DictEntry *new_entries = realloc(
            dict->data.dict.entries,
            new_cap * sizeof(DictEntry)
        );

        if (!new_entries) return -1;

        dict->data.dict.entries = new_entries;
        dict->data.dict.cap = new_cap;
    }

    dict->data.dict.entries[dict->data.dict.len].key =
        value_retain(key);

    dict->data.dict.entries[dict->data.dict.len].value =
        value_retain(value);

    dict->data.dict.len++;

    return 0;
}

Value *value_dict_get(const Value *dict, Value *key) {
    if (!dict || dict->tag != TAG_DICT) return NULL;

    for (uint32_t i = 0; i < dict->data.dict.len; i++) {
        if (value_equal(dict->data.dict.entries[i].key, key)) {
            return dict->data.dict.entries[i].value;
        }
    }

    return NULL;
}

Value *value_dict_key_at(const Value *dict, size_t index) {
    if (!dict || dict->tag != TAG_DICT) return NULL;
    if (index >= dict->data.dict.len) return NULL;

    return dict->data.dict.entries[index].key;
}

int value_dict_delete(Value *dict, const Value *key) {
    if (!dict || dict->tag != TAG_DICT) return 0;

    DictEntry *entries = dict->data.dict.entries;

    for (uint32_t i = 0; i < dict->data.dict.len; i++) {
        if (value_equal(entries[i].key, key)) {
            Value *old_key = entries[i].key;
            Value *old_value = entries[i].value;
            memmove(&entries[i], &entries[i + 1],
                    (dict->data.dict.len - i - 1) * sizeof(DictEntry));
            dict->data.dict.len--;
            value_release(old_key);
            value_release(old_value);
            return 1;
        }
    }

    return 0;
}

int value_set_add(Value *set, Value *item) {
    if (!set || (set->tag != TAG_SET && set->tag != TAG_FROZENSET)) return -1;
    if (!value_is_hashable(item)) return -2;

    if (value_set_contains(set, item)) return 0;

    if (set->data.set.len >= set->data.set.cap) {
        uint32_t new_cap =
            (set->data.set.cap == 0)
                ? 4
                : set->data.set.cap * 2;

        if (new_cap < set->data.set.cap) return -1;

        Value **new_items = realloc(
            set->data.set.items,
            new_cap * sizeof(Value *)
        );

        if (!new_items) return -1;

        set->data.set.items = new_items;
        set->data.set.cap = new_cap;
    }

    set->data.set.items[set->data.set.len] = value_retain(item);
    set->data.set.len++;

    return 0;
}

int value_set_contains(const Value *set, Value *item) {
    if (!set || (set->tag != TAG_SET && set->tag != TAG_FROZENSET)) return 0;

    for (uint32_t i = 0; i < set->data.set.len; i++) {
        if (value_equal(set->data.set.items[i], item)) {
            return 1;
        }
    }

    return 0;
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
