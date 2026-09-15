#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>
#include "value.h"

typedef struct VM VM;
typedef struct Frame Frame;
typedef struct FuncEntry FuncEntry;

struct FuncEntry {
    uint32_t name_index;
    uint32_t code_offset;
    uint32_t locals_count;
    uint32_t params_count;
    uint32_t defaults_count;
    uint32_t *param_names;
    uint32_t *default_consts;
};

struct Frame {
    Frame *prev;
    uint8_t *return_ip;
    Value **locals;
    uint32_t locals_cap;
    FuncEntry *func;
    uint32_t stack_base;
};

struct VM {
    uint8_t *bytecode;
    size_t bytecode_len;

    Value **constants;
    uint32_t num_constants;

    char **names;
    uint32_t num_names;

    Value **globals;

    FuncEntry *functions;
    uint32_t num_functions;

    uint32_t entry_func_index;

    uint8_t *ip;
    Value **stack;
    uint32_t stack_cap;
    uint32_t stack_top;

    Frame *current_frame;

    Value *result;

    int last_error;
    char error_msg[256];
};

#define VM_ERR_OK           0
#define VM_ERR_LOAD        -1
#define VM_ERR_OOM         -2
#define VM_ERR_BAD_MAGIC   -3
#define VM_ERR_VERSION     -4
#define VM_ERR_BOUNDS      -5
#define VM_ERR_TYPE        -6
#define VM_ERR_STACK       -7
#define VM_ERR_DIV_ZERO    -8
#define VM_ERR_FUNC_NOT_FOUND -9
#define VM_ERR_INVALID_OP  -10
#define VM_ERR_OVERFLOW  -11
#define VM_ERR_VALUE     -12
#define VM_ERR_STOP      -13
#define VM_ERR_KEY       -14
#define VM_ERR_ATTR      -15
#define VM_ERR_RUNTIME   -16
#define VM_ERR_LOOKUP    -17
#define VM_ERR_UNICODE   -18

int vm_load_memory(VM *vm, const uint8_t *data, size_t len);

int vm_run(VM *vm);

void vm_free(VM *vm);

const char *vm_error_string(VM *vm);

#endif
