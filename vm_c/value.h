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
} ValueTag;

typedef struct Value Value;
typedef struct Function Function;

typedef enum {
    FUNC_USER,
    FUNC_BUILTIN,
} FunctionKind;

typedef struct DictEntry {
    Value *key;
    Value *value;
} DictEntry;

typedef struct DictObject {
    DictEntry *entries;
    uint32_t len;
    uint32_t cap;
} DictObject;

typedef struct SetObject {
    Value **items;
    uint32_t len;
    uint32_t cap;
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
    } data;
};

struct Function {
    FunctionKind kind;
    uint32_t index;
    const char *name;
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
Value *value_new_none(void);
Value *value_true(void);
Value *value_false(void);
Value *value_bool(int b);
Value *value_new_function(FunctionKind kind, uint32_t index, const char *name);

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

int value_set_add(Value *set, Value *item);
int value_set_contains(const Value *set, Value *item);

int value_truthy(const Value *v);
int value_compare(const Value *a, const Value *b);
char *value_to_string(const Value *v);

Value *value_type_of(const Value *v);
const char *value_type_name(const Value *v);

#endif
