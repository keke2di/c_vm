#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicode.h"
#include "value.h"

#define REPR_MAX_DEPTH 64

#define MODE_STR 0
#define MODE_REPR 1
#define MODE_ASCII 2

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} Buffer;

static void buffer_append(Buffer *b, const char *text, size_t len) {
    if (b->failed || len == 0) return;

    if (b->len + len + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->len + len + 1) {
            if (cap > (size_t)-1 / 2) {
                b->failed = 1;
                return;
            }
            cap *= 2;
        }

        char *data = realloc(b->data, cap);
        if (!data) {
            b->failed = 1;
            return;
        }

        b->data = data;
        b->cap = cap;
    }

    memcpy(b->data + b->len, text, len);
    b->len += len;
    b->data[b->len] = '\0';
}

static void buffer_text(Buffer *b, const char *text) {
    buffer_append(b, text, strlen(text));
}

static void format_double(Buffer *b, double value) {
    if (isnan(value)) {
        buffer_text(b, "nan");
        return;
    }

    if (isinf(value)) {
        buffer_text(b, value < 0 ? "-inf" : "inf");
        return;
    }

    char scientific[64];
    int precision = 0;

    for (; precision < 17; precision++) {
        snprintf(scientific, sizeof(scientific), "%.*e", precision, value);
        if (strtod(scientific, NULL) == value) break;
    }

    const char *cursor = scientific;
    int negative = 0;

    if (*cursor == '-') {
        negative = 1;
        cursor++;
    }

    char digits[32];
    size_t digit_count = 0;

    while (*cursor && *cursor != 'e' && digit_count + 1 < sizeof(digits)) {
        if (*cursor != '.') {
            digits[digit_count++] = *cursor;
        }
        cursor++;
    }

    digits[digit_count] = '\0';

    while (*cursor && *cursor != 'e') cursor++;
    int exponent = *cursor == 'e' ? atoi(cursor + 1) : 0;

    if (negative) buffer_text(b, "-");

    if (exponent < -4 || exponent >= 16) {
        buffer_append(b, digits, 1);

        if (digit_count > 1) {
            buffer_text(b, ".");
            buffer_append(b, digits + 1, digit_count - 1);
        }

        char tail[16];
        snprintf(
            tail,
            sizeof(tail),
            "e%c%02d",
            exponent < 0 ? '-' : '+',
            exponent < 0 ? -exponent : exponent
        );
        buffer_text(b, tail);
        return;
    }

    if (exponent >= 0) {
        if ((size_t)exponent + 1 >= digit_count) {
            buffer_append(b, digits, digit_count);
            for (size_t i = digit_count; i <= (size_t)exponent; i++) {
                buffer_text(b, "0");
            }
            buffer_text(b, ".0");
        } else {
            buffer_append(b, digits, (size_t)exponent + 1);
            buffer_text(b, ".");
            buffer_append(b, digits + exponent + 1, digit_count - (size_t)exponent - 1);
        }
        return;
    }

    buffer_text(b, "0.");
    for (int i = 0; i < -exponent - 1; i++) {
        buffer_text(b, "0");
    }
    buffer_append(b, digits, digit_count);
}

static void write_escaped_codepoint(Buffer *b, uint32_t cp) {
    char escaped[16];
    int n;

    if (cp < 0x100) {
        n = snprintf(escaped, sizeof(escaped), "\\x%02x", (unsigned)cp);
    } else if (cp < 0x10000) {
        n = snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned)cp);
    } else {
        n = snprintf(escaped, sizeof(escaped), "\\U%08x", (unsigned)cp);
    }

    buffer_append(b, escaped, (size_t)n);
}

static uint32_t decode_codepoint(const char *s, size_t len, size_t *i) {
    unsigned char lead = (unsigned char)s[*i];
    size_t size = 1;

    if (lead >= 0xF0) size = 4;
    else if (lead >= 0xE0) size = 3;
    else if (lead >= 0xC0) size = 2;

    if (*i + size > len) size = len - *i;

    uint32_t cp;
    const unsigned char *u = (const unsigned char *)s + *i;

    if (size == 1) {
        cp = u[0];
    } else if (size == 2) {
        cp = ((uint32_t)(u[0] & 0x1F) << 6) | (uint32_t)(u[1] & 0x3F);
    } else if (size == 3) {
        cp = ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) |
             (uint32_t)(u[2] & 0x3F);
    } else {
        cp = ((uint32_t)(u[0] & 0x07) << 18) | ((uint32_t)(u[1] & 0x3F) << 12) |
             ((uint32_t)(u[2] & 0x3F) << 6) | (uint32_t)(u[3] & 0x3F);
    }

    *i += size;
    return cp;
}

static void write_quoted(Buffer *b, const char *text, size_t len, int is_bytes, int ascii) {
    int has_single = 0;
    int has_double = 0;

    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\'') has_single = 1;
        else if (text[i] == '"') has_double = 1;
    }

    char quote = has_single && !has_double ? '"' : '\'';

    if (is_bytes) buffer_text(b, "b");
    buffer_append(b, &quote, 1);

    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)text[i];

        if (!is_bytes && c >= 0x80) {
            size_t start = i;
            uint32_t cp = decode_codepoint(text, len, &i);

            if (ascii || !uni_is_printable(cp)) {
                write_escaped_codepoint(b, cp);
            } else {
                buffer_append(b, text + start, i - start);
            }
            continue;
        }

        if (c == (unsigned char)quote || c == '\\') {
            char escaped[2] = { '\\', (char)c };
            buffer_append(b, escaped, 2);
        } else if (c == '\n') {
            buffer_text(b, "\\n");
        } else if (c == '\r') {
            buffer_text(b, "\\r");
        } else if (c == '\t') {
            buffer_text(b, "\\t");
        } else if (c < 0x20 || c == 0x7F || (is_bytes && c >= 0x80)) {
            char escaped[8];
            snprintf(escaped, sizeof(escaped), "\\x%02x", c);
            buffer_text(b, escaped);
        } else {
            buffer_append(b, (const char *)&c, 1);
        }
        i++;
    }

    buffer_append(b, &quote, 1);
}

static void write_value(Buffer *b, const Value *v, int mode, const Value **stack, int depth);

static void write_items(
    Buffer *b,
    Value **items,
    uint32_t count,
    int mode,
    const Value **stack,
    int depth
) {
    int child = mode == MODE_ASCII ? MODE_ASCII : MODE_REPR;
    for (uint32_t i = 0; i < count; i++) {
        if (i > 0) buffer_text(b, ", ");
        write_value(b, items[i], child, stack, depth);
    }
}

static void write_value(Buffer *b, const Value *v, int mode, const Value **stack, int depth) {
    if (!v) {
        buffer_text(b, "nil");
        return;
    }

    int child = mode == MODE_ASCII ? MODE_ASCII : MODE_REPR;

    switch (v->tag) {
        case TAG_NONE:
            buffer_text(b, "None");
            return;

        case TAG_BOOL:
            buffer_text(b, v->data.int_val ? "True" : "False");
            return;

        case TAG_INT: {
            char number[32];
            snprintf(number, sizeof(number), "%lld", (long long)v->data.int_val);
            buffer_text(b, number);
            return;
        }

        case TAG_FLOAT:
            format_double(b, v->data.float_val);
            return;

        case TAG_STRING:
            if (mode == MODE_STR) {
                buffer_append(b, v->data.str.data, v->data.str.len);
            } else {
                write_quoted(b, v->data.str.data, v->data.str.len, 0, mode == MODE_ASCII);
            }
            return;

        case TAG_BYTES:
            write_quoted(b, (const char *)v->data.bytes.data, v->data.bytes.len, 1, 0);
            return;

        case TAG_FUNCTION: {
            char text[128];
            snprintf(
                text,
                sizeof(text),
                "<%sfunction %s>",
                v->data.func && v->data.func->kind == FUNC_BUILTIN ? "built-in " : "",
                v->data.func && v->data.func->name ? v->data.func->name : "?"
            );
            buffer_text(b, text);
            return;
        }

        case TAG_TYPE: {
            char text[128];
            snprintf(text, sizeof(text), "<class '%s'>", value_type_name(v));
            buffer_text(b, text);
            return;
        }

        case TAG_RANGE: {
            char text[96];
            if (v->data.range.step == 1) {
                snprintf(
                    text,
                    sizeof(text),
                    "range(%lld, %lld)",
                    (long long)v->data.range.start,
                    (long long)v->data.range.stop);
            } else {
                snprintf(
                    text,
                    sizeof(text),
                    "range(%lld, %lld, %lld)",
                    (long long)v->data.range.start,
                    (long long)v->data.range.stop,
                    (long long)v->data.range.step);
            }
            buffer_text(b, text);
            return;
        }

        case TAG_ITERATOR:
            buffer_text(b, "<iterator object>");
            return;

        default:
            break;
    }

    for (int i = 0; i < depth; i++) {
        if (stack[i] == v) {
            buffer_text(b, "...");
            return;
        }
    }

    if (depth >= REPR_MAX_DEPTH) {
        buffer_text(b, "...");
        return;
    }

    stack[depth] = v;

    switch (v->tag) {
        case TAG_LIST:
            buffer_text(b, "[");
            write_items(b, v->data.list.items, v->data.list.len, mode, stack, depth + 1);
            buffer_text(b, "]");
            return;

        case TAG_TUPLE:
            buffer_text(b, "(");
            write_items(b, v->data.tuple.items, v->data.tuple.len, mode, stack, depth + 1);
            if (v->data.tuple.len == 1) buffer_text(b, ",");
            buffer_text(b, ")");
            return;

        case TAG_SET:
            if (v->data.set.len == 0) {
                buffer_text(b, "set()");
                return;
            }
            buffer_text(b, "{");
            write_items(b, v->data.set.items, v->data.set.len, mode, stack, depth + 1);
            buffer_text(b, "}");
            return;

        case TAG_FROZENSET:
            if (v->data.set.len == 0) {
                buffer_text(b, "frozenset()");
                return;
            }
            buffer_text(b, "frozenset({");
            write_items(b, v->data.set.items, v->data.set.len, mode, stack, depth + 1);
            buffer_text(b, "})");
            return;

        case TAG_DICT:
            buffer_text(b, "{");
            for (uint32_t i = 0; i < v->data.dict.len; i++) {
                if (i > 0) buffer_text(b, ", ");
                write_value(b, v->data.dict.entries[i].key, child, stack, depth + 1);
                buffer_text(b, ": ");
                write_value(b, v->data.dict.entries[i].value, child, stack, depth + 1);
            }
            buffer_text(b, "}");
            return;

        case TAG_DICT_VIEW: {
            static const char *const VIEW_PREFIX[] = { "dict_keys([", "dict_values([", "dict_items([" };
            const Value *dict = v->data.view.dict;
            buffer_text(b, VIEW_PREFIX[v->data.view.kind]);
            for (uint32_t i = 0; i < dict->data.dict.len; i++) {
                const DictEntry *entry = &dict->data.dict.entries[i];
                if (i > 0) buffer_text(b, ", ");
                if (v->data.view.kind == VIEW_ITEMS) {
                    buffer_text(b, "(");
                    write_value(b, entry->key, child, stack, depth + 1);
                    buffer_text(b, ", ");
                    write_value(b, entry->value, child, stack, depth + 1);
                    buffer_text(b, ")");
                } else {
                    write_value(b, v->data.view.kind == VIEW_KEYS ? entry->key : entry->value,
                                child, stack, depth + 1);
                }
            }
            buffer_text(b, "])");
            return;
        }

        default: {
            char text[32];
            snprintf(text, sizeof(text), "unknown tag %d", (int)v->tag);
            buffer_text(b, text);
            return;
        }
    }
}

static char *build_text(const Value *v, int mode, size_t *out_len) {
    Buffer b;
    const Value *stack[REPR_MAX_DEPTH];

    b.data = NULL;
    b.len = 0;
    b.cap = 0;
    b.failed = 0;

    write_value(&b, v, mode, stack, 0);

    if (b.failed) {
        free(b.data);
        return NULL;
    }

    if (!b.data) {
        b.data = malloc(1);
        if (!b.data) return NULL;
        b.data[0] = '\0';
    }

    if (out_len) *out_len = b.len;
    return b.data;
}

char *value_to_string(const Value *v) {
    return build_text(v, MODE_STR, NULL);
}

char *value_to_string_sized(const Value *v, size_t *out_len) {
    return build_text(v, MODE_STR, out_len);
}

char *value_to_repr(const Value *v) {
    return build_text(v, MODE_REPR, NULL);
}

char *value_to_ascii(const Value *v) {
    return build_text(v, MODE_ASCII, NULL);
}
