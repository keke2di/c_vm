#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opcodes.h"
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int is_number(const Value *v) {
    return int_like(v) || v->tag == TAG_FLOAT;
}

static int is_sequence(const Value *v) {
    return v->tag == TAG_STRING || v->tag == TAG_LIST ||
           v->tag == TAG_TUPLE || v->tag == TAG_BYTES;
}

static double as_double(const Value *v) {
    return v->tag == TAG_FLOAT ? v->data.float_val : (double)v->data.int_val;
}

static int mul_overflows(int64_t a, int64_t b) {
    if (a == 0 || b == 0) return 0;
    if (a == INT64_MIN) return b != 1;
    if (b == INT64_MIN) return a != 1;
    if (a > 0) return b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a;
    return b > 0 ? a < INT64_MIN / b : a < INT64_MAX / b;
}

static void concat_string(VM *vm, const Value *a, const Value *b) {
    uint64_t total = (uint64_t)a->data.str.len + b->data.str.len;
    if (total > UINT32_MAX) {
        vm->last_error = VM_ERR_OVERFLOW;
        return;
    }

    char *joined = malloc((size_t)total + 1);
    if (!joined) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    memcpy(joined, a->data.str.data, a->data.str.len);
    memcpy(joined + a->data.str.len, b->data.str.data, b->data.str.len);
    joined[total] = '\0';

    Value *result = value_new_string_len(joined, (size_t)total);
    free(joined);

    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    vm_push_owned(vm, result);
}

static void concat_bytes(VM *vm, const Value *a, const Value *b) {
    uint64_t total = (uint64_t)a->data.bytes.len + b->data.bytes.len;
    if (total > UINT32_MAX) {
        vm->last_error = VM_ERR_OVERFLOW;
        return;
    }

    unsigned char *buf = NULL;
    if (total > 0) {
        buf = malloc((size_t)total);
        if (!buf) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        memcpy(buf, a->data.bytes.data, a->data.bytes.len);
        memcpy(buf + a->data.bytes.len, b->data.bytes.data, b->data.bytes.len);
    }

    Value *result = value_new_bytes(buf, (size_t)total);
    free(buf);

    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    vm_push_owned(vm, result);
}

static void concat_list(VM *vm, const Value *a, const Value *b) {
    Value *result = value_new_list();
    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    for (uint32_t i = 0; i < a->data.list.len; i++) {
        if (value_list_append(result, a->data.list.items[i]) != 0) {
            value_release(result);
            vm->last_error = VM_ERR_OOM;
            return;
        }
    }

    for (uint32_t i = 0; i < b->data.list.len; i++) {
        if (value_list_append(result, b->data.list.items[i]) != 0) {
            value_release(result);
            vm->last_error = VM_ERR_OOM;
            return;
        }
    }

    vm_push_owned(vm, result);
}

static void concat_tuple(VM *vm, const Value *a, const Value *b) {
    uint64_t total = (uint64_t)a->data.tuple.len + b->data.tuple.len;
    if (total > UINT32_MAX) {
        vm->last_error = VM_ERR_OVERFLOW;
        return;
    }

    Value *result = value_new_tuple((size_t)total);
    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t k = 0;
    for (uint32_t i = 0; i < a->data.tuple.len; i++) {
        result->data.tuple.items[k++] = value_retain(a->data.tuple.items[i]);
    }
    for (uint32_t i = 0; i < b->data.tuple.len; i++) {
        result->data.tuple.items[k++] = value_retain(b->data.tuple.items[i]);
    }

    vm_push_owned(vm, result);
}

static int repeat_overflows(int64_t count, uint32_t unit, uint64_t *out_total, VM *vm) {
    if (unit != 0 && (uint64_t)count > (uint64_t)UINT32_MAX / unit) {
        vm->last_error = VM_ERR_OVERFLOW;
        return 1;
    }
    *out_total = (uint64_t)unit * (uint64_t)count;
    return 0;
}

static void repeat_string(VM *vm, const Value *s, int64_t count) {
    if (count < 0) count = 0;
    uint64_t total;
    if (repeat_overflows(count, s->data.str.len, &total, vm)) return;

    char *buf = NULL;
    if (total > 0) {
        buf = malloc((size_t)total);
        if (!buf) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        for (int64_t i = 0; i < count; i++) {
            memcpy(buf + (size_t)i * s->data.str.len, s->data.str.data, s->data.str.len);
        }
    }

    Value *result = value_new_string_len(buf ? buf : "", (size_t)total);
    free(buf);

    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    vm_push_owned(vm, result);
}

static void repeat_bytes(VM *vm, const Value *s, int64_t count) {
    if (count < 0) count = 0;
    uint64_t total;
    if (repeat_overflows(count, s->data.bytes.len, &total, vm)) return;

    unsigned char *buf = NULL;
    if (total > 0) {
        buf = malloc((size_t)total);
        if (!buf) {
            vm->last_error = VM_ERR_OOM;
            return;
        }
        for (int64_t i = 0; i < count; i++) {
            memcpy(buf + (size_t)i * s->data.bytes.len, s->data.bytes.data, s->data.bytes.len);
        }
    }

    Value *result = value_new_bytes(buf, (size_t)total);
    free(buf);

    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    vm_push_owned(vm, result);
}

static void repeat_list(VM *vm, const Value *s, int64_t count) {
    if (count < 0) count = 0;
    uint64_t total;
    if (repeat_overflows(count, s->data.list.len, &total, vm)) return;

    Value *result = value_new_list();
    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    for (int64_t c = 0; c < count; c++) {
        for (uint32_t i = 0; i < s->data.list.len; i++) {
            if (value_list_append(result, s->data.list.items[i]) != 0) {
                value_release(result);
                vm->last_error = VM_ERR_OOM;
                return;
            }
        }
    }

    vm_push_owned(vm, result);
}

static void repeat_tuple(VM *vm, const Value *s, int64_t count) {
    if (count < 0) count = 0;
    uint64_t total;
    if (repeat_overflows(count, s->data.tuple.len, &total, vm)) return;

    Value *result = value_new_tuple((size_t)total);
    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t k = 0;
    for (int64_t c = 0; c < count; c++) {
        for (uint32_t i = 0; i < s->data.tuple.len; i++) {
            result->data.tuple.items[k++] = value_retain(s->data.tuple.items[i]);
        }
    }

    vm_push_owned(vm, result);
}

static void repeat_sequence(VM *vm, const Value *seq, int64_t count) {
    switch (seq->tag) {
        case TAG_STRING: repeat_string(vm, seq, count); break;
        case TAG_BYTES: repeat_bytes(vm, seq, count); break;
        case TAG_LIST: repeat_list(vm, seq, count); break;
        case TAG_TUPLE: repeat_tuple(vm, seq, count); break;
        default: vm->last_error = VM_ERR_TYPE; break;
    }
}

static void binary_add(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b)) {
        int64_t ai = a->data.int_val;
        int64_t bi = b->data.int_val;

        if ((bi > 0 && ai > INT64_MAX - bi) || (bi < 0 && ai < INT64_MIN - bi)) {
            vm->last_error = VM_ERR_OVERFLOW;
            return;
        }

        vm_push_owned(vm, value_new_int(ai + bi));
        return;
    }

    if (is_number(a) && is_number(b)) {
        vm_push_owned(vm, value_new_float(as_double(a) + as_double(b)));
        return;
    }

    if (a->tag == TAG_STRING && b->tag == TAG_STRING) {
        concat_string(vm, a, b);
        return;
    }

    if (a->tag == TAG_LIST && b->tag == TAG_LIST) {
        concat_list(vm, a, b);
        return;
    }

    if (a->tag == TAG_TUPLE && b->tag == TAG_TUPLE) {
        concat_tuple(vm, a, b);
        return;
    }

    if (a->tag == TAG_BYTES && b->tag == TAG_BYTES) {
        concat_bytes(vm, a, b);
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_sub(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b)) {
        int64_t ai = a->data.int_val;
        int64_t bi = b->data.int_val;

        if ((bi < 0 && ai > INT64_MAX + bi) || (bi > 0 && ai < INT64_MIN + bi)) {
            vm->last_error = VM_ERR_OVERFLOW;
            return;
        }

        vm_push_owned(vm, value_new_int(ai - bi));
        return;
    }

    if (is_number(a) && is_number(b)) {
        vm_push_owned(vm, value_new_float(as_double(a) - as_double(b)));
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_mul(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b)) {
        if (mul_overflows(a->data.int_val, b->data.int_val)) {
            vm->last_error = VM_ERR_OVERFLOW;
            return;
        }

        vm_push_owned(vm, value_new_int(a->data.int_val * b->data.int_val));
        return;
    }

    if (is_number(a) && is_number(b)) {
        vm_push_owned(vm, value_new_float(as_double(a) * as_double(b)));
        return;
    }

    if (is_sequence(a) && int_like(b)) {
        repeat_sequence(vm, a, b->data.int_val);
        return;
    }

    if (int_like(a) && is_sequence(b)) {
        repeat_sequence(vm, b, a->data.int_val);
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_div(VM *vm, const Value *a, const Value *b) {
    if (is_number(a) && is_number(b)) {
        double bf = as_double(b);
        if (bf == 0.0) {
            vm->last_error = VM_ERR_DIV_ZERO;
        } else {
            vm_push_owned(vm, value_new_float(as_double(a) / bf));
        }
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_mod(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b)) {
        int64_t ai = a->data.int_val;
        int64_t bi = b->data.int_val;

        if (bi == 0) {
            vm->last_error = VM_ERR_DIV_ZERO;
        } else if (ai == INT64_MIN && bi == -1) {
            vm->last_error = VM_ERR_OVERFLOW;
        } else {
            int64_t remainder = ai % bi;
            if (remainder != 0 && (remainder < 0) != (bi < 0)) {
                remainder += bi;
            }
            vm_push_owned(vm, value_new_int(remainder));
        }
        return;
    }

    if (is_number(a) && is_number(b)) {
        double af = as_double(a);
        double bf = as_double(b);

        if (bf == 0.0) {
            vm->last_error = VM_ERR_DIV_ZERO;
            return;
        }

        double remainder = fmod(af, bf);
        if (remainder == 0.0) {
            remainder = copysign(0.0, bf);
        } else if ((remainder < 0.0) != (bf < 0.0)) {
            remainder += bf;
        }
        vm_push_owned(vm, value_new_float(remainder));
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_floordiv(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b)) {
        int64_t ai = a->data.int_val;
        int64_t bi = b->data.int_val;

        if (bi == 0) {
            vm->last_error = VM_ERR_DIV_ZERO;
        } else if (ai == INT64_MIN && bi == -1) {
            vm->last_error = VM_ERR_OVERFLOW;
        } else {
            int64_t quotient = ai / bi;
            int64_t remainder = ai % bi;
            if (remainder != 0 && (remainder > 0) != (bi > 0)) {
                quotient--;
            }
            vm_push_owned(vm, value_new_int(quotient));
        }
        return;
    }

    if (is_number(a) && is_number(b)) {
        double bf = as_double(b);
        if (bf == 0.0) {
            vm->last_error = VM_ERR_DIV_ZERO;
        } else {
            vm_push_owned(vm, value_new_float(floor(as_double(a) / bf)));
        }
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_pow(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b) && b->data.int_val >= 0) {
        int64_t base = a->data.int_val;
        int64_t exp = b->data.int_val;
        int64_t result = 1;

        while (exp > 0) {
            if (exp & 1) {
                if (mul_overflows(result, base)) {
                    vm->last_error = VM_ERR_OVERFLOW;
                    return;
                }
                result *= base;
            }

            exp >>= 1;

            if (exp > 0) {
                if (mul_overflows(base, base)) {
                    vm->last_error = VM_ERR_OVERFLOW;
                    return;
                }
                base *= base;
            }
        }

        vm_push_owned(vm, value_new_int(result));
        return;
    }

    if (is_number(a) && is_number(b)) {
        vm_push_owned(vm, value_new_float(pow(as_double(a), as_double(b))));
        return;
    }

    vm->last_error = VM_ERR_TYPE;
}

static void binary_bitop(VM *vm, uint8_t op, const Value *a, const Value *b) {
    if (!int_like(a) || !int_like(b)) {
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    int64_t ai = a->data.int_val;
    int64_t bi = b->data.int_val;
    int both_bool = a->tag == TAG_BOOL && b->tag == TAG_BOOL;

    switch (op) {
        case OP_BINARY_AND:
            vm_push_owned(vm, both_bool ? value_bool((ai & bi) != 0) : value_new_int(ai & bi));
            return;

        case OP_BINARY_OR:
            vm_push_owned(vm, both_bool ? value_bool((ai | bi) != 0) : value_new_int(ai | bi));
            return;

        case OP_BINARY_XOR:
            vm_push_owned(vm, both_bool ? value_bool((ai ^ bi) != 0) : value_new_int(ai ^ bi));
            return;

        case OP_BINARY_LSHIFT:
            if (bi < 0) {
                vm->last_error = VM_ERR_VALUE;
                return;
            }
            if (bi >= 64) {
                if (ai != 0) {
                    vm->last_error = VM_ERR_OVERFLOW;
                    return;
                }
                vm_push_owned(vm, value_new_int(0));
                return;
            }
            {
                int64_t shifted = (int64_t)((uint64_t)ai << bi);
                if ((shifted >> bi) != ai) {
                    vm->last_error = VM_ERR_OVERFLOW;
                    return;
                }
                vm_push_owned(vm, value_new_int(shifted));
            }
            return;

        case OP_BINARY_RSHIFT:
            if (bi < 0) {
                vm->last_error = VM_ERR_VALUE;
                return;
            }
            if (bi >= 64) {
                vm_push_owned(vm, value_new_int(ai < 0 ? -1 : 0));
                return;
            }
            vm_push_owned(vm, value_new_int(ai >> bi));
            return;

        default:
            vm->last_error = VM_ERR_INVALID_OP;
            return;
    }
}

static int is_set_operator(uint8_t op) {
    return op == OP_BINARY_OR || op == OP_BINARY_AND ||
           op == OP_BINARY_XOR || op == OP_BINARY_SUB;
}

void op_binary(VM *vm, uint8_t op) {
    Value *b = vm_pop(vm);
    Value *a = vm_pop(vm);

    if (!a || !b) {
        if (a) value_release(a);
        if (b) value_release(b);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (is_set_operator(op) && is_setlike(a) && is_setlike(b)) {
        Value *result = set_binary_op(vm, op, a, b);
        if (result) vm_push_owned(vm, result);
        value_release(a);
        value_release(b);
        return;
    }

    if (op == OP_BINARY_OR && a->tag == TAG_DICT && b->tag == TAG_DICT) {
        Value *result = value_dict_union(vm, a, b);
        if (result) vm_push_owned(vm, result);
        value_release(a);
        value_release(b);
        return;
    }

    switch (op) {
        case OP_BINARY_ADD: binary_add(vm, a, b); break;
        case OP_BINARY_SUB: binary_sub(vm, a, b); break;
        case OP_BINARY_MUL: binary_mul(vm, a, b); break;
        case OP_BINARY_DIV: binary_div(vm, a, b); break;
        case OP_BINARY_MOD: binary_mod(vm, a, b); break;
        case OP_BINARY_FLOORDIV: binary_floordiv(vm, a, b); break;
        case OP_BINARY_POW: binary_pow(vm, a, b); break;
        case OP_BINARY_AND:
        case OP_BINARY_OR:
        case OP_BINARY_XOR:
        case OP_BINARY_LSHIFT:
        case OP_BINARY_RSHIFT: binary_bitop(vm, op, a, b); break;
        default: vm->last_error = VM_ERR_INVALID_OP; break;
    }

    value_release(a);
    value_release(b);
}

void op_unary(VM *vm, uint8_t op) {
    Value *v = vm_pop(vm);
    if (!v) return;

    switch (op) {
        case OP_UNARY_POS:
            if (int_like(v)) {
                vm_push_owned(vm, value_new_int(v->data.int_val));
            } else if (v->tag == TAG_FLOAT) {
                vm_push_owned(vm, value_new_float(v->data.float_val));
            } else {
                vm->last_error = VM_ERR_TYPE;
            }
            break;

        case OP_UNARY_NEG:
            if (int_like(v)) {
                if (v->data.int_val == INT64_MIN) {
                    vm->last_error = VM_ERR_OVERFLOW;
                } else {
                    vm_push_owned(vm, value_new_int(-v->data.int_val));
                }
            } else if (v->tag == TAG_FLOAT) {
                vm_push_owned(vm, value_new_float(-v->data.float_val));
            } else {
                vm->last_error = VM_ERR_TYPE;
            }
            break;

        case OP_UNARY_INVERT:
            if (int_like(v)) {
                vm_push_owned(vm, value_new_int(~v->data.int_val));
            } else {
                vm->last_error = VM_ERR_TYPE;
            }
            break;

        case OP_UNARY_NOT:
            vm_push_owned(vm, value_bool(!value_truthy(v)));
            break;

        default:
            vm->last_error = VM_ERR_INVALID_OP;
            break;
    }

    value_release(v);
}
