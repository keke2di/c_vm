#include "vm_internal.h"

static const char *const CODEC_PARAMS[] = { "encoding", "errors" };

static Value *bytes_decode(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, CODEC_PARAMS, 2, 2, 0, slots) != 0) return NULL;
    return codec_decode_value(vm, self->data.bytes.data, self->data.bytes.len, slots[0], slots[1]);
}

const MethodEntry BYTES_METHODS[] = {
    {"decode", bytes_decode},
};

const uint32_t BYTES_METHOD_COUNT = (uint32_t)(sizeof(BYTES_METHODS) / sizeof(BYTES_METHODS[0]));
