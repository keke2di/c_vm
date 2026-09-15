#include "vm_internal.h"

static const char *const CODEC_PARAMS[] = { "encoding", "errors" };

static Value *str_encode(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, CODEC_PARAMS, 2, 2, 0, slots) != 0) return NULL;
    return codec_encode_value(vm, self, slots[0], slots[1]);
}

const MethodEntry STR_METHODS[] = {
    {"encode", str_encode},
};

const uint32_t STR_METHOD_COUNT = (uint32_t)(sizeof(STR_METHODS) / sizeof(STR_METHODS[0]));
