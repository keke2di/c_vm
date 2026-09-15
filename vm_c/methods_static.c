#include <string.h>
#include "vm_internal.h"

static int codepoint_key(VM *vm, const Value *key, int64_t *out) {
    if (key->tag == TAG_INT || key->tag == TAG_BOOL) {
        *out = key->data.int_val;
        return 0;
    }

    if (key->tag == TAG_STRING) {
        const char *s = key->data.str.data;
        size_t len = key->data.str.len;

        if (utf8_length(s, len) != 1) {
            vm->last_error = VM_ERR_VALUE;
            return -1;
        }

        size_t pos = 0;
        *out = (int64_t)utf8_decode(s, len, &pos);
        return 0;
    }

    vm->last_error = VM_ERR_TYPE;
    return -1;
}

static int put_ordinal(VM *vm, Value *dict, int64_t ordinal, Value *value) {
    Value *key = value_new_int(ordinal);

    if (!key) {
        vm->last_error = VM_ERR_OOM;
        return -1;
    }

    int rc = value_dict_set(dict, key, value);
    value_release(key);

    if (rc != 0) {
        vm->last_error = insert_error(rc);
        return -1;
    }

    return 0;
}

static Value *str_maketrans(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)self;
    if (check_positional(vm, nargs, kwnames, 1, 3) != 0) return NULL;

    Value *out = value_new_dict();
    if (!out) return vm_fail(vm, VM_ERR_OOM);

    if (nargs == 1) {
        if (args[0]->tag != TAG_DICT) {
            value_release(out);
            return vm_fail(vm, VM_ERR_TYPE);
        }

        for (uint32_t i = 0; i < args[0]->data.dict.len; i++) {
            const DictEntry *entry = &args[0]->data.dict.entries[i];
            int64_t ordinal;

            if (codepoint_key(vm, entry->key, &ordinal) != 0 ||
                put_ordinal(vm, out, ordinal, entry->value) != 0) {
                value_release(out);
                return NULL;
            }
        }

        return out;
    }

    const char *from;
    size_t from_len;
    const char *to;
    size_t to_len;

    if (text_value(vm, args[0], &from, &from_len) != 0 ||
        text_value(vm, args[1], &to, &to_len) != 0) {
        value_release(out);
        return NULL;
    }

    if (utf8_length(from, from_len) != utf8_length(to, to_len)) {
        value_release(out);
        return vm_fail(vm, VM_ERR_VALUE);
    }

    size_t fi = 0;
    size_t ti = 0;

    while (fi < from_len) {
        uint32_t source = utf8_decode(from, from_len, &fi);
        uint32_t target = utf8_decode(to, to_len, &ti);

        Value *mapped = value_new_int((int64_t)target);

        if (!mapped) {
            value_release(out);
            return vm_fail(vm, VM_ERR_OOM);
        }

        int rc = put_ordinal(vm, out, (int64_t)source, mapped);
        value_release(mapped);

        if (rc != 0) {
            value_release(out);
            return NULL;
        }
    }

    if (nargs == 3) {
        const char *drop;
        size_t drop_len;

        if (text_value(vm, args[2], &drop, &drop_len) != 0) {
            value_release(out);
            return NULL;
        }

        size_t di = 0;

        while (di < drop_len) {
            uint32_t cp = utf8_decode(drop, drop_len, &di);

            if (put_ordinal(vm, out, (int64_t)cp, value_new_none()) != 0) {
                value_release(out);
                return NULL;
            }
        }
    }

    return out;
}

static Value *bytes_maketrans(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)self;
    if (check_positional(vm, nargs, kwnames, 2, 2) != 0) return NULL;

    const char *from;
    size_t from_len;
    const char *to;
    size_t to_len;

    if (bytes_value(vm, args[0], &from, &from_len) != 0) return NULL;
    if (bytes_value(vm, args[1], &to, &to_len) != 0) return NULL;
    if (from_len != to_len) return vm_fail(vm, VM_ERR_VALUE);

    unsigned char table[256];

    for (int i = 0; i < 256; i++) {
        table[i] = (unsigned char)i;
    }

    for (size_t i = 0; i < from_len; i++) {
        table[(unsigned char)from[i]] = (unsigned char)to[i];
    }

    Value *result = value_new_bytes(table, sizeof(table));
    return result ? result : vm_fail(vm, VM_ERR_OOM);
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static Value *bytes_fromhex(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)self;
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    const char *s;
    size_t len;

    if (args[0]->tag == TAG_BYTES) {
        if (bytes_value(vm, args[0], &s, &len) != 0) return NULL;
    } else if (text_value(vm, args[0], &s, &len) != 0) {
        return NULL;
    }

    StrBuf out = { NULL, 0, 0, 0 };
    size_t i = 0;

    while (i < len) {
        if (ascii_is_space((unsigned char)s[i])) {
            i++;
            continue;
        }

        int high = i + 1 < len ? hex_digit(s[i]) : -1;
        int low = i + 1 < len ? hex_digit(s[i + 1]) : -1;

        if (high < 0 || low < 0) {
            strbuf_free(&out);
            return vm_fail(vm, VM_ERR_VALUE);
        }

        char byte = (char)((high << 4) | low);
        strbuf_append(&out, &byte, 1);
        i += 2;
    }

    return strbuf_finish_bytes(vm, &out);
}

static Value *dict_fromkeys(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    (void)self;
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;

    Value *out = value_new_dict();
    if (!out) return vm_fail(vm, VM_ERR_OOM);

    Value *iter = value_make_iter(vm, args[0]);

    if (!iter) {
        value_release(out);
        return NULL;
    }

    Value *fill = nargs == 2 ? args[1] : value_new_none();

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        int rc = value_dict_set(out, item, fill);
        value_release(item);

        if (rc != 0) {
            vm->last_error = insert_error(rc);
            break;
        }
    }

    value_release(iter);

    if (vm->last_error != VM_ERR_OK) {
        value_release(out);
        return NULL;
    }

    return out;
}

static const MethodEntry STR_STATICS[] = {
    {"maketrans", str_maketrans},
};

static const MethodEntry BYTES_STATICS[] = {
    {"maketrans", bytes_maketrans},
    {"fromhex", bytes_fromhex},
};

static const MethodEntry DICT_STATICS[] = {
    {"fromkeys", dict_fromkeys},
};

#define STATIC_COUNT(table) ((uint32_t)(sizeof(table) / sizeof(table[0])))

MethodFn static_method_lookup(int type_id, const Value *name) {
    switch (type_id) {
        case TYPE_STR: return method_table_lookup(STR_STATICS, STATIC_COUNT(STR_STATICS), name);
        case TYPE_BYTES: return method_table_lookup(BYTES_STATICS, STATIC_COUNT(BYTES_STATICS), name);
        case TYPE_DICT: return method_table_lookup(DICT_STATICS, STATIC_COUNT(DICT_STATICS), name);
        default: return NULL;
    }
}
