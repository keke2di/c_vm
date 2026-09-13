#include "value.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *value_strdup(const char *s) {
    if (!s) return NULL;

    size_t len = strlen(s) + 1;
    char *copy = malloc(len);

    if (!copy) return NULL;

    memcpy(copy, s, len);
    return copy;
}

static char *value_strdup_len(const char *data, size_t len) {
    if (len > UINT32_MAX) return NULL;
    if (len > 0 && !data) return NULL;

    char *copy = malloc(len + 1);

    if (!copy) return NULL;

    if (len > 0) {
        memcpy(copy, data, len);
    }

    copy[len] = '\0';
    return copy;
}

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

Value *value_new_none(void) {
    return value_alloc(TAG_NONE);
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

Value *value_retain(Value *v) {
    if (v) {
        v->refcount++;
    }

    return v;
}

void value_release(Value *v) {
    if (!v) return;

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
            if (v->data.set.items) {
                for (uint32_t i = 0; i < v->data.set.len; i++) {
                    value_release(v->data.set.items[i]);
                }

                free(v->data.set.items);
            }
            break;

        case TAG_FUNCTION:
            free(v->data.func);
            break;

        default:
            break;
    }

    free(v);
}

int64_t value_as_int(const Value *v) {
    if (!v || v->tag != TAG_INT) return 0;

    return v->data.int_val;
}

double value_as_float(const Value *v) {
    if (!v || v->tag != TAG_FLOAT) return 0.0;

    return v->data.float_val;
}

const char *value_as_string(const Value *v) {
    if (!v || v->tag != TAG_STRING) return NULL;

    return v->data.str.data;
}

size_t value_string_len(const Value *v) {
    if (!v || v->tag != TAG_STRING) return 0;

    return v->data.str.len;
}

size_t value_bytes_len(const Value *v) {
    if (!v || v->tag != TAG_BYTES) return 0;

    return v->data.bytes.len;
}

const unsigned char *value_bytes_data(const Value *v) {
    if (!v || v->tag != TAG_BYTES) return NULL;

    return v->data.bytes.data;
}

size_t value_list_len(const Value *v) {
    if (!v || v->tag != TAG_LIST) return 0;

    return v->data.list.len;
}

Value *value_list_get(const Value *v, size_t idx) {
    if (!v || v->tag != TAG_LIST) return NULL;
    if (idx >= v->data.list.len) return NULL;

    return v->data.list.items[idx];
}

void value_list_set(Value *v, size_t idx, Value *item) {
    if (!v || v->tag != TAG_LIST) return;
    if (idx >= v->data.list.len) return;

    value_release(v->data.list.items[idx]);
    v->data.list.items[idx] = value_retain(item);
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

size_t value_tuple_len(const Value *v) {
    if (!v || v->tag != TAG_TUPLE) return 0;

    return v->data.tuple.len;
}

Value *value_tuple_get(const Value *v, size_t idx) {
    if (!v || v->tag != TAG_TUPLE) return NULL;
    if (idx >= v->data.tuple.len) return NULL;

    return v->data.tuple.items[idx];
}

int value_dict_set(Value *dict, Value *key, Value *value) {
    if (!dict || dict->tag != TAG_DICT) return -1;

    for (uint32_t i = 0; i < dict->data.dict.len; i++) {
        if (value_compare(dict->data.dict.entries[i].key, key) == 0) {
            value_release(dict->data.dict.entries[i].value);
            dict->data.dict.entries[i].value = value_retain(value);
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
        if (value_compare(
                dict->data.dict.entries[i].key,
                key
            ) == 0) {
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

size_t value_dict_len(const Value *dict) {
    if (!dict || dict->tag != TAG_DICT) return 0;

    return dict->data.dict.len;
}

int value_set_add(Value *set, Value *item) {
    if (!set || set->tag != TAG_SET) return -1;

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
    if (!set || set->tag != TAG_SET) return 0;

    for (uint32_t i = 0; i < set->data.set.len; i++) {
        if (value_compare(set->data.set.items[i], item) == 0) {
            return 1;
        }
    }

    return 0;
}

size_t value_set_len(const Value *set) {
    if (!set || set->tag != TAG_SET) return 0;

    return set->data.set.len;
}

int value_compare(const Value *a, const Value *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;

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

char *value_to_string(const Value *v) {
    if (!v) return value_strdup("nil");

    char buf[256];

    switch (v->tag) {
        case TAG_INT:
            snprintf(
                buf,
                sizeof(buf),
                "%lld",
                (long long)v->data.int_val
            );
            break;

        case TAG_FLOAT:
            snprintf(
                buf,
                sizeof(buf),
                "%g",
                v->data.float_val
            );
            break;

        case TAG_STRING:
            return value_strdup_len(
                v->data.str.data,
                v->data.str.len
            );

        case TAG_BYTES:
            snprintf(
                buf,
                sizeof(buf),
                "[bytes len=%u]",
                v->data.bytes.len
            );
            break;

        case TAG_TUPLE:
            snprintf(
                buf,
                sizeof(buf),
                "(tuple len=%u)",
                v->data.tuple.len
            );
            break;

        case TAG_LIST:
            snprintf(
                buf,
                sizeof(buf),
                "[list len=%u]",
                v->data.list.len
            );
            break;

        case TAG_DICT:
            snprintf(
                buf,
                sizeof(buf),
                "{dict len=%u}",
                v->data.dict.len
            );
            break;

        case TAG_SET:
            snprintf(
                buf,
                sizeof(buf),
                "{set len=%u}",
                v->data.set.len
            );
            break;

        case TAG_FUNCTION:
            snprintf(
                buf,
                sizeof(buf),
                "<%sfunction %s>",
                v->data.func && v->data.func->kind == FUNC_BUILTIN ? "built-in " : "",
                v->data.func && v->data.func->name ? v->data.func->name : "?"
            );
            break;

        case TAG_NONE:
            return value_strdup("None");

        default:
            snprintf(
                buf,
                sizeof(buf),
                "unknown tag %d",
                v->tag
            );
            break;
    }

    return value_strdup(buf);
}
