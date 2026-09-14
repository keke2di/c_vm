#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"

static char *copy_text(const char *data, size_t len) {
    char *copy = malloc(len + 1);
    if (!copy) return NULL;

    if (len > 0) {
        memcpy(copy, data, len);
    }

    copy[len] = '\0';
    return copy;
}

char *value_to_string(const Value *v) {
    if (!v) return copy_text("nil", 3);

    char buf[256];

    switch (v->tag) {
        case TAG_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)v->data.int_val);
            break;

        case TAG_FLOAT:
            snprintf(buf, sizeof(buf), "%g", v->data.float_val);
            break;

        case TAG_STRING:
            return copy_text(v->data.str.data, v->data.str.len);

        case TAG_BYTES:
            snprintf(buf, sizeof(buf), "[bytes len=%u]", v->data.bytes.len);
            break;

        case TAG_TUPLE:
            snprintf(buf, sizeof(buf), "(tuple len=%u)", v->data.tuple.len);
            break;

        case TAG_LIST:
            snprintf(buf, sizeof(buf), "[list len=%u]", v->data.list.len);
            break;

        case TAG_DICT:
            snprintf(buf, sizeof(buf), "{dict len=%u}", v->data.dict.len);
            break;

        case TAG_SET:
            snprintf(buf, sizeof(buf), "{set len=%u}", v->data.set.len);
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

        case TAG_BOOL:
            return v->data.int_val ? copy_text("True", 4) : copy_text("False", 5);

        case TAG_TYPE:
            snprintf(buf, sizeof(buf), "<class '%s'>", value_type_name(v));
            break;

        case TAG_NONE:
            return copy_text("None", 4);

        default:
            snprintf(buf, sizeof(buf), "unknown tag %d", (int)v->tag);
            break;
    }

    return copy_text(buf, strlen(buf));
}
