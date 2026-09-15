#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

typedef struct {
    Value **args;
    uint32_t npos;
    const Value *kwnames;
    Value **kwvalues;
    Value *mapping;
    uint32_t auto_index;
    int used_auto;
    int used_manual;
} FormatCtx;

static int all_digits(const char *s, size_t len) {
    if (len == 0) return 0;

    for (size_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }

    return 1;
}

static int digits_value(const char *s, size_t len, int64_t *out) {
    int64_t value = 0;

    for (size_t i = 0; i < len; i++) {
        if (value > 0x7FFFFFF) return -1;
        value = value * 10 + (s[i] - '0');
    }

    *out = value;
    return 0;
}

static void borrow_string(Value *slot, const char *data, size_t len) {
    slot->tag = TAG_STRING;
    slot->refcount = UINT32_MAX;
    slot->data.str.data = (char *)data;
    slot->data.str.len = (uint32_t)len;
}

static Value *lookup_name(VM *vm, FormatCtx *ctx, const char *name, size_t len) {
    if (len == 0 || all_digits(name, len)) {
        if (ctx->mapping) return vm_fail(vm, VM_ERR_VALUE);

        int64_t index;

        if (len == 0) {
            if (ctx->used_manual) return vm_fail(vm, VM_ERR_VALUE);
            ctx->used_auto = 1;
            index = ctx->auto_index++;
        } else {
            if (ctx->used_auto) return vm_fail(vm, VM_ERR_VALUE);
            ctx->used_manual = 1;
            if (digits_value(name, len, &index) != 0) return vm_fail(vm, VM_ERR_BOUNDS);
        }

        if (index >= (int64_t)ctx->npos) return vm_fail(vm, VM_ERR_BOUNDS);
        return ctx->args[index];
    }

    if (ctx->mapping) {
        Value key;
        borrow_string(&key, name, len);
        Value *found = value_dict_get(ctx->mapping, &key);
        return found ? found : vm_fail(vm, VM_ERR_KEY);
    }

    uint32_t nkw = kw_count(ctx->kwnames);

    for (uint32_t k = 0; k < nkw; k++) {
        const Value *entry = ctx->kwnames->data.tuple.items[k];

        if (entry->tag == TAG_STRING && entry->data.str.len == len &&
            memcmp(entry->data.str.data, name, len) == 0) {
            return ctx->kwvalues[k];
        }
    }

    return vm_fail(vm, VM_ERR_KEY);
}

static Value *apply_accessor(VM *vm, Value *base, const char *key, size_t len) {
    if (all_digits(key, len)) {
        int64_t index;
        if (digits_value(key, len, &index) != 0) return vm_fail(vm, VM_ERR_BOUNDS);

        if (base->tag == TAG_DICT) {
            Value slot;
            slot.tag = TAG_INT;
            slot.refcount = UINT32_MAX;
            slot.data.int_val = index;
            Value *found = value_dict_get(base, &slot);
            return found ? value_retain(found) : vm_fail(vm, VM_ERR_KEY);
        }

        Value *item = value_item_at(base, index);
        return item ? item : vm_fail(vm, VM_ERR_BOUNDS);
    }

    if (base->tag != TAG_DICT) return vm_fail(vm, VM_ERR_TYPE);

    Value slot;
    borrow_string(&slot, key, len);
    Value *found = value_dict_get(base, &slot);
    return found ? value_retain(found) : vm_fail(vm, VM_ERR_KEY);
}

static Value *resolve_field(VM *vm, FormatCtx *ctx, const char *field, size_t len) {
    size_t i = 0;

    while (i < len && field[i] != '[' && field[i] != '.') {
        i++;
    }

    Value *base = lookup_name(vm, ctx, field, i);
    if (!base) return NULL;

    Value *current = value_retain(base);

    while (i < len) {
        if (field[i] != '[') {
            value_release(current);
            return vm_fail(vm, VM_ERR_ATTR);
        }

        i++;
        size_t start = i;

        while (i < len && field[i] != ']') {
            i++;
        }

        if (i >= len || i == start) {
            value_release(current);
            return vm_fail(vm, VM_ERR_VALUE);
        }

        Value *next = apply_accessor(vm, current, field + start, i - start);
        value_release(current);
        if (!next) return NULL;

        current = next;
        i++;
    }

    return current;
}

static int build_nested_spec(VM *vm, FormatCtx *ctx, const char *spec, size_t len, StrBuf *out) {
    size_t i = 0;

    while (i < len) {
        if (spec[i] != '{') {
            size_t start = i;
            while (i < len && spec[i] != '{') i++;
            strbuf_append(out, spec + start, i - start);
            continue;
        }

        i++;
        size_t start = i;

        while (i < len && spec[i] != '}' && spec[i] != '{') {
            i++;
        }

        if (i >= len || spec[i] != '}') {
            vm->last_error = VM_ERR_VALUE;
            return -1;
        }

        Value *value = resolve_field(vm, ctx, spec + start, i - start);
        if (!value) return -1;

        size_t text_len = 0;
        char *text = value_to_string_sized(value, &text_len);
        value_release(value);

        if (!text) {
            vm->last_error = VM_ERR_OOM;
            return -1;
        }

        strbuf_append(out, text, text_len);
        free(text);
        i++;
    }

    return 0;
}

static Value *run_format(VM *vm, Value *self, FormatCtx *ctx) {
    const char *s = self->data.str.data;
    size_t len = self->data.str.len;

    StrBuf out = { NULL, 0, 0, 0 };
    size_t i = 0;

    while (i < len) {
        char c = s[i];

        if (c != '{' && c != '}') {
            size_t start = i;
            while (i < len && s[i] != '{' && s[i] != '}') i++;
            strbuf_append(&out, s + start, i - start);
            continue;
        }

        if (i + 1 < len && s[i + 1] == c) {
            strbuf_append(&out, &c, 1);
            i += 2;
            continue;
        }

        if (c == '}') {
            strbuf_free(&out);
            return vm_fail(vm, VM_ERR_VALUE);
        }

        i++;
        size_t name_start = i;
        int in_bracket = 0;

        while (i < len) {
            char d = s[i];

            if (d == '[') {
                in_bracket = 1;
            } else if (d == ']') {
                in_bracket = 0;
            } else if (!in_bracket && (d == ':' || d == '!' || d == '}')) {
                break;
            }

            i++;
        }

        if (i >= len) {
            strbuf_free(&out);
            return vm_fail(vm, VM_ERR_VALUE);
        }

        size_t name_len = i - name_start;
        int conv = 0;

        if (s[i] == '!') {
            i++;

            char kind = i < len ? s[i] : 0;
            conv = kind == 's' ? 1 : kind == 'r' ? 2 : kind == 'a' ? 3 : 0;

            if (conv == 0) {
                strbuf_free(&out);
                return vm_fail(vm, VM_ERR_VALUE);
            }

            i++;

            if (i >= len || (s[i] != ':' && s[i] != '}')) {
                strbuf_free(&out);
                return vm_fail(vm, VM_ERR_VALUE);
            }
        }

        const char *spec = NULL;
        size_t spec_len = 0;
        size_t spec_start = 0;
        int has_spec = 0;

        if (s[i] == ':') {
            i++;
            spec_start = i;
            int depth = 0;

            while (i < len) {
                if (s[i] == '{') {
                    depth++;
                } else if (s[i] == '}') {
                    if (depth == 0) break;
                    depth--;
                }
                i++;
            }

            if (i >= len) {
                strbuf_free(&out);
                return vm_fail(vm, VM_ERR_VALUE);
            }

            spec = s + spec_start;
            spec_len = i - spec_start;
            has_spec = 1;
        }

        if (i >= len || s[i] != '}') {
            strbuf_free(&out);
            return vm_fail(vm, VM_ERR_VALUE);
        }

        i++;

        Value *value = resolve_field(vm, ctx, s + name_start, name_len);

        if (!value) {
            strbuf_free(&out);
            return NULL;
        }

        StrBuf nested = { NULL, 0, 0, 0 };
        int nested_used = 0;

        if (has_spec && memchr(spec, '{', spec_len)) {
            if (build_nested_spec(vm, ctx, spec, spec_len, &nested) != 0) {
                strbuf_free(&nested);
                value_release(value);
                strbuf_free(&out);
                return NULL;
            }

            spec = nested.data ? nested.data : "";
            spec_len = nested.len;
            nested_used = 1;
        }

        size_t piece_len = 0;
        int err = 0;
        char *piece = format_value(value, spec ? spec : "", spec_len, conv, &piece_len, &err);

        value_release(value);
        if (nested_used) strbuf_free(&nested);

        if (err || !piece) {
            free(piece);
            strbuf_free(&out);
            return vm_fail(vm, VM_ERR_VALUE);
        }

        strbuf_append(&out, piece, piece_len);
        free(piece);
    }

    return strbuf_finish_string(vm, &out);
}

Value *str_format(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t nkw = kw_count(kwnames);
    uint32_t npos = nargs - nkw;

    FormatCtx ctx;
    ctx.args = args;
    ctx.npos = npos;
    ctx.kwnames = kwnames;
    ctx.kwvalues = args + npos;
    ctx.mapping = NULL;
    ctx.auto_index = 0;
    ctx.used_auto = 0;
    ctx.used_manual = 0;

    return run_format(vm, self, &ctx);
}

Value *str_format_map(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;
    if (args[0]->tag != TAG_DICT) return vm_fail(vm, VM_ERR_TYPE);

    FormatCtx ctx;
    ctx.args = NULL;
    ctx.npos = 0;
    ctx.kwnames = NULL;
    ctx.kwvalues = NULL;
    ctx.mapping = args[0];
    ctx.auto_index = 0;
    ctx.used_auto = 0;
    ctx.used_manual = 0;

    return run_format(vm, self, &ctx);
}
