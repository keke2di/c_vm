#ifndef CVM_VM_INTERNAL_H
#define CVM_VM_INTERNAL_H

#include <stdint.h>
#include <stdio.h>
#include "vm.h"

#define STACK_INIT_CAP 64
#define MAX_STACK_DEPTH 1024
#define MAX_CALL_DEPTH 256

#define SPACE_ASCII 0
#define SPACE_NUMERIC 1
#define SPACE_UNICODE 2

#ifdef CVM_DEBUG
#define VM_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VM_DEBUG(...) ((void)0)
#endif

typedef Value *(*NativeFn)(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
typedef Value *(*MethodFn)(VM *vm, Value *self, Value **args, uint32_t nargs, const Value *kwnames);

typedef struct {
    const char *name;
    MethodFn fn;
} MethodEntry;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} StrBuf;

void vm_push(VM *vm, Value *v);
void vm_push_owned(VM *vm, Value *v);
Value *vm_pop(VM *vm);

Value *vm_fail(VM *vm, int error);
int insert_error(int rc);
uint32_t kw_count(const Value *kwnames);
int check_positional(VM *vm, uint32_t nargs, const Value *kwnames, uint32_t min, uint32_t max);
int bind_keywords(VM *vm, Value **kwvalues, const Value *kwnames,
                  const char *const *names, uint32_t nparams, Value **slots);
int bind_args(VM *vm, Value **args, uint32_t nargs, const Value *kwnames,
              const char *const *names, uint32_t nparams, uint32_t max_positional,
              uint32_t required, Value **slots);

void strbuf_append(StrBuf *b, const char *text, size_t len);
void strbuf_free(StrBuf *b);
Value *strbuf_finish_string(VM *vm, StrBuf *b);
Value *strbuf_finish_bytes(VM *vm, StrBuf *b);

Value *codec_encode_value(VM *vm, const Value *text, const Value *encoding, const Value *errors);
Value *codec_decode_value(VM *vm, const unsigned char *data, size_t len,
                          const Value *encoding, const Value *errors);

uint32_t builtin_count(void);
const char *builtin_name(uint32_t index);
NativeFn builtin_function(uint32_t index);

Value *builtin_abs(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_divmod(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_pow(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_round(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_sum(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_min(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_max(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_sorted(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_any(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_all(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_isinstance(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_callable(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_id(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_ord(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_chr(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_bin(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_oct(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_hex(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_format(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *builtin_ascii(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);

Value *construct_int(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_float(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_str(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_bool(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_list(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_range(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_type(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_tuple(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_set(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_frozenset(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_dict(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);
Value *construct_bytes(VM *vm, Value **args, uint32_t nargs, const Value *kwnames);

int vm_install_type_globals(VM *vm);
NativeFn type_constructor(int type_id);

extern const MethodEntry LIST_METHODS[];
extern const uint32_t LIST_METHOD_COUNT;
extern const MethodEntry DICT_METHODS[];
extern const uint32_t DICT_METHOD_COUNT;
extern const MethodEntry SET_METHODS[];
extern const uint32_t SET_METHOD_COUNT;
extern const MethodEntry STR_METHODS[];
extern const uint32_t STR_METHOD_COUNT;
extern const MethodEntry BYTES_METHODS[];
extern const uint32_t BYTES_METHOD_COUNT;
MethodFn method_lookup(const Value *self, const Value *name);

int vm_init_callable_globals(VM *vm);
void vm_call_value(VM *vm, uint32_t nargs, const Value *kwnames);
void vm_call_method(VM *vm, uint32_t nargs, int has_kwnames);
int vm_step(VM *vm);
int vm_call_sync(VM *vm, Value *callable, Value **args, uint32_t nargs, Value **out_result);

void op_binary(VM *vm, uint8_t op);
void op_unary(VM *vm, uint8_t op);
void op_compare(VM *vm, uint8_t op);
void op_is(VM *vm, int negate);

size_t utf8_char_size(const char *s, size_t byte_len, size_t offset);
size_t utf8_length(const char *s, size_t byte_len);
size_t utf8_offset(const char *s, size_t byte_len, size_t index);
uint32_t utf8_decode(const char *s, size_t byte_len, size_t *offset);
size_t utf8_encode(uint32_t cp, char *out);
int unicode_is_space(uint32_t cp);
int ascii_is_space(uint32_t c);
void text_space_bounds(const char *s, size_t len, int mode, size_t *begin, size_t *end);
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
void op_delete_index(VM *vm);
char *format_value(const Value *v, const char *spec, size_t spec_len, int conv, size_t *out_len, int *err);
void op_format_value(VM *vm, uint32_t conv);
void op_build_string(VM *vm, uint32_t n);
void op_get_iter_item(VM *vm);
void op_get_slice(VM *vm);
void op_len(VM *vm);

void op_get_iter(VM *vm);
Value *value_make_iter(VM *vm, const Value *iterable);
Value *iterator_next(VM *vm, Value *iter);
Value *value_list_from_iterable(VM *vm, const Value *iterable);
int value_set_update(VM *vm, Value *set, const Value *iterable);
int value_dict_update(VM *vm, Value *dict, const Value *source);
void op_unpack_sequence(VM *vm, uint32_t n);
void op_unpack_ex(VM *vm, uint32_t before, uint32_t after);

#endif
