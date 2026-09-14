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

static void operand_text(const Value *v, char *buf, size_t cap, const char **text, size_t *len) {
    if (v->tag == TAG_STRING) {
        *text = v->data.str.data;
        *len = v->data.str.len;
        return;
    }

    if (v->tag == TAG_FLOAT) {
        snprintf(buf, cap, "%g", v->data.float_val);
    } else {
        snprintf(buf, cap, "%lld", (long long)v->data.int_val);
    }

    *text = buf;
    *len = strlen(buf);
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

    if (a->tag == TAG_STRING || b->tag == TAG_STRING) {
        const Value *other = a->tag == TAG_STRING ? b : a;
        if (other->tag != TAG_STRING && !is_number(other)) {
            vm->last_error = VM_ERR_TYPE;
            return;
        }

        char a_buf[64];
        char b_buf[64];
        const char *sa;
        const char *sb;
        size_t sa_len;
        size_t sb_len;

        operand_text(a, a_buf, sizeof(a_buf), &sa, &sa_len);
        operand_text(b, b_buf, sizeof(b_buf), &sb, &sb_len);

        char *joined = malloc(sa_len + sb_len + 1);
        if (!joined) {
            vm->last_error = VM_ERR_OOM;
            return;
        }

        memcpy(joined, sa, sa_len);
        memcpy(joined + sa_len, sb, sb_len);
        joined[sa_len + sb_len] = '\0';

        Value *result = value_new_string_len(joined, sa_len + sb_len);
        free(joined);
        vm_push_owned(vm, result);
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

    vm->last_error = VM_ERR_TYPE;
}

static void binary_div(VM *vm, const Value *a, const Value *b) {
    if (int_like(a) && int_like(b)) {
        if (b->data.int_val == 0) {
            vm->last_error = VM_ERR_DIV_ZERO;
        } else if (a->data.int_val == INT64_MIN && b->data.int_val == -1) {
            vm->last_error = VM_ERR_OVERFLOW;
        } else {
            vm_push_owned(vm, value_new_int(a->data.int_val / b->data.int_val));
        }
        return;
    }

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

void op_binary(VM *vm, uint8_t op) {
    Value *b = vm_pop(vm);
    Value *a = vm_pop(vm);

    if (!a || !b) {
        if (a) value_release(a);
        if (b) value_release(b);
        vm->last_error = VM_ERR_STACK;
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
