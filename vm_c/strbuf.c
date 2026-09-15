#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

void strbuf_append(StrBuf *b, const char *text, size_t len) {
    if (b->failed || len == 0) return;

    if (len > SIZE_MAX - b->len - 1) {
        b->failed = 1;
        return;
    }

    if (b->len + len + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->len + len + 1) {
            if (cap > SIZE_MAX / 2) {
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

void strbuf_free(StrBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

Value *strbuf_finish_string(VM *vm, StrBuf *b) {
    Value *result = b->failed ? NULL : value_new_string_len(b->data ? b->data : "", b->len);
    strbuf_free(b);
    return result ? result : vm_fail(vm, VM_ERR_OOM);
}

Value *strbuf_finish_bytes(VM *vm, StrBuf *b) {
    Value *result = b->failed ? NULL : value_new_bytes((const unsigned char *)b->data, b->len);
    strbuf_free(b);
    return result ? result : vm_fail(vm, VM_ERR_OOM);
}
