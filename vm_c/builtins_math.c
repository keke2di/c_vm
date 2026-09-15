#include <math.h>
#include <stdlib.h>
#include <string.h>
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

Value *builtin_abs(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *x = args[0];
    if (int_like(x)) {
        if (x->data.int_val == INT64_MIN) return vm_fail(vm, VM_ERR_OVERFLOW);
        int64_t v = x->data.int_val;
        return value_new_int(v < 0 ? -v : v);
    }
    if (x->tag == TAG_FLOAT) return value_new_float(fabs(x->data.float_val));
    return vm_fail(vm, VM_ERR_TYPE);
}

static void int_floor_divmod(int64_t a, int64_t b, int64_t *q, int64_t *r) {
    int64_t quotient = a / b;
    int64_t remainder = a % b;
    if (remainder != 0 && (remainder < 0) != (b < 0)) {
        quotient--;
        remainder += b;
    }
    *q = quotient;
    *r = remainder;
}

static void float_divmod(double a, double b, double *q, double *r) {
    double mod = fmod(a, b);
    double div = (a - mod) / b;

    if (mod != 0.0) {
        if ((b < 0) != (mod < 0)) {
            mod += b;
            div -= 1.0;
        }
    } else {
        mod = copysign(0.0, b);
    }

    double floordiv;
    if (div != 0.0) {
        floordiv = floor(div);
        if (div - floordiv > 0.5) floordiv += 1.0;
    } else {
        floordiv = copysign(0.0, a / b);
    }

    *q = floordiv;
    *r = mod;
}

static Value *make_pair(VM *vm, Value *first, Value *second) {
    Value *pair = value_new_tuple(2);
    if (!pair || !first || !second) {
        value_release(first);
        value_release(second);
        value_release(pair);
        return vm_fail(vm, VM_ERR_OOM);
    }
    pair->data.tuple.items[0] = first;
    pair->data.tuple.items[1] = second;
    return pair;
}

Value *builtin_divmod(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 2) != 0) return NULL;

    Value *a = args[0];
    Value *b = args[1];

    if (int_like(a) && int_like(b)) {
        if (b->data.int_val == 0) return vm_fail(vm, VM_ERR_DIV_ZERO);
        if (a->data.int_val == INT64_MIN && b->data.int_val == -1) return vm_fail(vm, VM_ERR_OVERFLOW);
        int64_t q, r;
        int_floor_divmod(a->data.int_val, b->data.int_val, &q, &r);
        return make_pair(vm, value_new_int(q), value_new_int(r));
    }

    if (is_number(a) && is_number(b)) {
        double bf = as_double(b);
        if (bf == 0.0) return vm_fail(vm, VM_ERR_DIV_ZERO);
        double q, r;
        float_divmod(as_double(a), bf, &q, &r);
        return make_pair(vm, value_new_float(q), value_new_float(r));
    }

    return vm_fail(vm, VM_ERR_TYPE);
}

static int64_t int_pow(int64_t base, int64_t exp, int *overflow) {
    int64_t result = 1;
    *overflow = 0;
    while (exp > 0) {
        if (exp & 1) {
            if (mul_overflows(result, base)) { *overflow = 1; return 0; }
            result *= base;
        }
        exp >>= 1;
        if (exp > 0) {
            if (mul_overflows(base, base)) { *overflow = 1; return 0; }
            base *= base;
        }
    }
    return result;
}

static uint64_t mulmod(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t result = 0;
    a %= m;
    while (b > 0) {
        if (b & 1) {
            result += a;
            if (result >= m || result < a) result -= m;
        }
        b >>= 1;
        if (b) {
            uint64_t doubled = a + a;
            if (doubled >= m || doubled < a) doubled -= m;
            a = doubled;
        }
    }
    return result;
}

static uint64_t modexp(uint64_t base, uint64_t exp, uint64_t m) {
    uint64_t result = 1 % m;
    base %= m;
    while (exp > 0) {
        if (exp & 1) result = mulmod(result, base, m);
        exp >>= 1;
        if (exp) base = mulmod(base, base, m);
    }
    return result;
}

static int mod_inverse(int64_t base, uint64_t m, uint64_t *out) {
    int64_t old_r = (int64_t)(((base % (int64_t)m) + (int64_t)m) % (int64_t)m);
    int64_t r = (int64_t)m;
    int64_t old_s = 1;
    int64_t s = 0;

    while (r != 0) {
        int64_t quotient = old_r / r;
        int64_t tmp = old_r - quotient * r;
        old_r = r;
        r = tmp;
        tmp = old_s - quotient * s;
        old_s = s;
        s = tmp;
    }

    if (old_r != 1) return -1;

    int64_t inv = old_s % (int64_t)m;
    if (inv < 0) inv += (int64_t)m;
    *out = (uint64_t)inv;
    return 0;
}

static Value *pow_mod(VM *vm, int64_t base, int64_t exp, int64_t mod) {
    if (mod == 0) return vm_fail(vm, VM_ERR_VALUE);

    uint64_t m = mod < 0 ? (uint64_t)(-(mod + 1)) + 1u : (uint64_t)mod;
    if (m == 1) return value_new_int(0);

    uint64_t b;
    if (exp < 0) {
        if (mod_inverse(base, m, &b) != 0) return vm_fail(vm, VM_ERR_VALUE);
        exp = exp == INT64_MIN ? INT64_MAX : -exp;
    } else {
        int64_t reduced = base % (int64_t)m;
        if (reduced < 0) reduced += (int64_t)m;
        b = (uint64_t)reduced;
    }

    uint64_t r = modexp(b, (uint64_t)exp, m);

    int64_t result = (int64_t)r;
    if (mod < 0 && r != 0) result = (int64_t)r - (int64_t)m;
    return value_new_int(result);
}

Value *builtin_pow(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 3) != 0) return NULL;

    Value *base = args[0];
    Value *exp = args[1];

    if (nargs == 3) {
        Value *mod = args[2];
        if (!int_like(base) || !int_like(exp) || !int_like(mod)) return vm_fail(vm, VM_ERR_TYPE);
        return pow_mod(vm, base->data.int_val, exp->data.int_val, mod->data.int_val);
    }

    if (int_like(base) && int_like(exp) && exp->data.int_val >= 0) {
        int overflow;
        int64_t result = int_pow(base->data.int_val, exp->data.int_val, &overflow);
        if (overflow) return vm_fail(vm, VM_ERR_OVERFLOW);
        return value_new_int(result);
    }

    if (is_number(base) && is_number(exp)) {
        return value_new_float(pow(as_double(base), as_double(exp)));
    }

    return vm_fail(vm, VM_ERR_TYPE);
}

static Value *round_int(VM *vm, int64_t value, int64_t ndigits) {
    if (ndigits >= 0) return value_new_int(value);
    if (ndigits < -19) return value_new_int(0);

    int64_t scale = 1;
    for (int64_t i = 0; i < -ndigits; i++) {
        if (mul_overflows(scale, 10)) return value_new_int(0);
        scale *= 10;
    }

    int64_t q = value / scale;
    int64_t r = value % scale;
    int64_t half = scale / 2;
    int64_t ar = r < 0 ? -r : r;

    if (ar > half || (ar == half && (q % 2 != 0))) {
        q += value < 0 ? -1 : 1;
    }

    if (mul_overflows(q, scale)) return vm_fail(vm, VM_ERR_OVERFLOW);
    return value_new_int(q * scale);
}

static Value *round_float(VM *vm, double value, int has_ndigits, int64_t ndigits) {
    if (!has_ndigits) {
        if (isinf(value)) return vm_fail(vm, VM_ERR_OVERFLOW);
        if (isnan(value)) return vm_fail(vm, VM_ERR_VALUE);
        double rounded = nearbyint(value);
        if (rounded >= 9223372036854775808.0 || rounded < -9223372036854775808.0) {
            return vm_fail(vm, VM_ERR_OVERFLOW);
        }
        return value_new_int((int64_t)rounded);
    }

    if (isinf(value) || isnan(value)) return value_new_float(value);

    if (ndigits >= 0) {
        if (ndigits > 323) return value_new_float(value);
        char buf[512];
        snprintf(buf, sizeof(buf), "%.*f", (int)ndigits, value);
        return value_new_float(strtod(buf, NULL));
    }

    if (ndigits < -308) return value_new_float(copysign(0.0, value));

    double scale = pow(10.0, (double)(-ndigits));
    double scaled = value / scale;
    char buf[512];
    snprintf(buf, sizeof(buf), "%.0f", scaled);
    return value_new_float(strtod(buf, NULL) * scale);
}

Value *builtin_round(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;

    Value *x = args[0];
    int has_ndigits = nargs == 2 && args[1]->tag != TAG_NONE;
    int64_t ndigits = 0;

    if (nargs == 2 && args[1]->tag != TAG_NONE) {
        if (!int_like(args[1])) return vm_fail(vm, VM_ERR_TYPE);
        ndigits = args[1]->data.int_val;
    }

    if (int_like(x)) return round_int(vm, x->data.int_val, has_ndigits ? ndigits : 0);
    if (x->tag == TAG_FLOAT) return round_float(vm, x->data.float_val, has_ndigits, ndigits);
    return vm_fail(vm, VM_ERR_TYPE);
}
