#ifndef CVM_VM_INTERNAL_H
#define CVM_VM_INTERNAL_H

#include <stdint.h>
#include <stdio.h>
#include "vm.h"

#define STACK_INIT_CAP 64
#define MAX_STACK_DEPTH 1024
#define MAX_CALL_DEPTH 256

#ifdef CVM_DEBUG
#define VM_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VM_DEBUG(...) ((void)0)
#endif

void vm_push(VM *vm, Value *v);
void vm_push_owned(VM *vm, Value *v);
Value *vm_pop(VM *vm);

uint32_t builtin_count(void);
const char *builtin_name(uint32_t index);
void builtin_invoke(VM *vm, uint32_t index, uint32_t nargs);
void builtin_int(VM *vm, uint32_t nargs);
void builtin_float(VM *vm, uint32_t nargs);
void builtin_str(VM *vm, uint32_t nargs);
void builtin_list(VM *vm, uint32_t nargs);
void builtin_bool(VM *vm, uint32_t nargs);

int vm_install_type_globals(VM *vm);
void type_construct(VM *vm, int type_id, uint32_t nargs);

int vm_init_callable_globals(VM *vm);
void vm_call_value(VM *vm, uint32_t nargs, const Value *kwnames);
void vm_call_method(VM *vm, uint32_t nargs);

void op_binary(VM *vm, uint8_t op);
void op_unary(VM *vm, uint8_t op);
void op_compare(VM *vm, uint8_t op);
void op_is(VM *vm, int negate);
void op_contains(VM *vm);

void op_build_list(VM *vm, uint32_t count);
void op_build_tuple(VM *vm, uint32_t count);
void op_build_map(VM *vm, uint32_t count);
void op_build_set(VM *vm, uint32_t count);
void op_list_append(VM *vm);
void op_set_add(VM *vm);
void op_map_add(VM *vm);
void op_get_index(VM *vm);
void op_set_index(VM *vm);
void op_get_iter_item(VM *vm);
void op_get_slice(VM *vm);
void op_len(VM *vm);

#endif
