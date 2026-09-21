#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    TAG_NONE,
    TAG_INT,
    TAG_FLOAT,
    TAG_STRING,
    TAG_LIST,
    TAG_DICT,
    TAG_SET,
    TAG_FUNCTION,
    TAG_BYTES,
    TAG_TUPLE,
    TAG_BOOL,
    TAG_TYPE,
    TAG_RANGE,
    TAG_ITERATOR,
    TAG_FROZENSET,
    TAG_DICT_VIEW,
    TAG_OBJECT,
    TAG_CELL,
    TAG_EXCEPTION,
} ValueTag;

typedef enum {
    VIEW_KEYS,
    VIEW_VALUES,
    VIEW_ITEMS,
} ViewKind;

typedef struct Value Value;
typedef struct Function Function;
typedef struct IterObject IterObject;

typedef enum {
    ITER_SEQ,
    ITER_ENUMERATE,
    ITER_ZIP,
    ITER_MAP,
    ITER_FILTER,
    ITER_REVERSED,
    ITER_CALLABLE,
} IterKind;

struct IterObject {
    IterKind kind;
    Value *source;
    int64_t index;
    int64_t length;
    Value *func;
    Value **subs;
    uint32_t nsubs;
    int64_t counter;
    int done;
    int strict;
};

typedef enum {
    FUNC_USER,
    FUNC_BUILTIN,
} FunctionKind;

typedef struct DictEntry {
    Value *key;
    Value *value;
    int64_t hash;
} DictEntry;

typedef struct DictObject {
    DictEntry *entries;
    uint32_t len;
    uint32_t cap;
    uint32_t *index;
    uint32_t index_cap;
    uint32_t index_used;
} DictObject;

typedef struct SetObject {
    Value **items;
    int64_t *hashes;
    uint32_t len;
    uint32_t cap;
    uint32_t *index;
    uint32_t index_cap;
    uint32_t index_used;
} SetObject;

typedef struct BytesObject {
    unsigned char *data;
    uint32_t len;
} BytesObject;

typedef struct TupleObject {
    Value **items;
    uint32_t len;
} TupleObject;

struct Value {
    ValueTag tag;
    uint32_t refcount;

    union {
        int64_t int_val;
        double float_val;
        void *ptr;

        struct {
            char *data;
            uint32_t len;
        } str;

        BytesObject bytes;

        struct {
            Value **items;
            uint32_t len;
            uint32_t cap;
        } list;

        TupleObject tuple;

        DictObject dict;
        SetObject set;

        Function *func;

        struct {
            int64_t start;
            int64_t stop;
            int64_t step;
        } range;

        IterObject *iter;

        struct {
            Value *dict;
            ViewKind kind;
        } view;

        struct {
            uint32_t type_id;
            Value *args;
        } exception;
    } data;
};

struct Function {
    FunctionKind kind;
    uint32_t index;
    const char *name;
    Value *defaults;
    Value *kwdefaults;
    Value **cells;
    uint32_t ncells;
};

Value *value_new_int(int64_t i);
Value *value_new_float(double f);
Value *value_new_string(const char *s);
Value *value_new_string_len(const char *data, size_t len);
Value *value_new_bytes(const unsigned char *data, size_t len);
Value *value_new_tuple(size_t len);
Value *value_new_list(void);
Value *value_new_dict(void);
Value *value_new_set(void);
Value *value_new_frozenset(void);
Value *value_new_dict_view(Value *dict, ViewKind kind);
Value *value_new_none(void);
Value *value_true(void);
Value *value_false(void);
Value *value_bool(int b);
Value *value_new_function(FunctionKind kind, uint32_t index, const char *name);
Value *value_new_range(int64_t start, int64_t stop, int64_t step);
Value *value_new_iterator(IterKind kind);
Value *value_new_object(void);
Value *value_new_cell(void);
Value *value_cell_get(const Value *cell);
void value_cell_set(Value *cell, Value *value);
int64_t value_range_len(int64_t start, int64_t stop, int64_t step);

Value *value_retain(Value *v);
void value_release(Value *v);


size_t value_string_len(const Value *v);


Value *value_list_get(const Value *v, size_t idx);
void value_list_set(Value *v, size_t idx, Value *item);
int value_list_append(Value *list, Value *item);

Value *value_tuple_get(const Value *v, size_t idx);

int value_dict_set(Value *dict, Value *key, Value *value);
Value *value_dict_get(const Value *dict, Value *key);
Value *value_dict_key_at(const Value *dict, size_t index);
int value_dict_delete(Value *dict, const Value *key);

int value_set_add(Value *set, Value *item);
int value_set_contains(const Value *set, Value *item);
int value_set_discard(Value *set, const Value *item);

int value_list_insert(Value *list, int64_t index, Value *item);
Value *value_list_remove_at(Value *list, int64_t index);

int value_truthy(const Value *v);
int value_compare(const Value *a, const Value *b);
int value_equal(const Value *a, const Value *b);
int value_is_hashable(const Value *v);
int64_t value_hash(const Value *v, int *err);

int64_t value_length(const Value *v);
Value *value_item_at(const Value *v, int64_t idx);
char *value_to_string(const Value *v);
char *value_to_string_sized(const Value *v, size_t *out_len);
char *value_to_repr(const Value *v);
char *value_to_ascii(const Value *v);
int value_order(const Value *a, const Value *b, int *ok);

Value *value_type_of(const Value *v);
const char *value_type_name(const Value *v);

#endif
