#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int is_callable(const Value *v) {
    return v->tag == TAG_FUNCTION || v->tag == TAG_TYPE;
}

static const char *const PRINT_KEYWORDS[] = { "sep", "end", "file", "flush" };
static const char *const ENUMERATE_PARAMS[] = { "iterable", "start" };
static const char *const STRICT_KEYWORD[] = { "strict" };

static int text_argument(const Value *v, const char *fallback, const char **text, size_t *len) {
    if (!v || v->tag == TAG_NONE) {
        *text = fallback;
        *len = strlen(fallback);
        return 1;
    }
    if (v->tag != TAG_STRING) return 0;
    *text = v->data.str.data;
    *len = v->data.str.len;
    return 1;
}

static Value *builtin_print(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t npos = nargs - kw_count(kwnames);
    Value *kw[4];
    if (bind_keywords(vm, args + npos, kwnames, PRINT_KEYWORDS, 4, kw) != 0) return NULL;

    const char *sep;
    size_t sep_len;
    const char *end;
    size_t end_len;

    if (!text_argument(kw[0], " ", &sep, &sep_len) || !text_argument(kw[1], "\n", &end, &end_len)) {
        return vm_fail(vm, VM_ERR_TYPE);
    }
    if (kw[2] && kw[2]->tag != TAG_NONE) {
        return vm_fail(vm, VM_ERR_ATTR);
    }

    for (uint32_t i = 0; i < npos; i++) {
        size_t text_len = 0;
        char *s = value_to_string_sized(args[i], &text_len);
        if (!s) return vm_fail(vm, VM_ERR_OOM);
        if (i > 0) platform_write_stdout(sep, sep_len);
        platform_write_stdout(s, text_len);
        free(s);
    }

    platform_write_stdout(end, end_len);
    return value_new_none();
}

static Value *builtin_len(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    int64_t len = value_length(args[0]);
    if (len < 0) return vm_fail(vm, VM_ERR_TYPE);
    return value_new_int(len);
}

static Value *builtin_repr(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    char *s = value_to_repr(args[0]);
    if (!s) return vm_fail(vm, VM_ERR_OOM);

    Value *result = value_new_string_len(s, strlen(s));
    free(s);
    return result;
}

Value *builtin_enumerate(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    Value *slots[2];
    if (bind_args(vm, args, nargs, kwnames, ENUMERATE_PARAMS, 2, 2, 1, slots) != 0) return NULL;

    int64_t start = 0;
    if (slots[1]) {
        if (!int_like(slots[1])) return vm_fail(vm, VM_ERR_TYPE);
        start = slots[1]->data.int_val;
    }

    Value *sub = value_make_iter(vm, slots[0]);
    if (!sub) return NULL;

    Value *iter = value_new_iterator(ITER_ENUMERATE);
    if (!iter) {
        value_release(sub);
        return vm_fail(vm, VM_ERR_OOM);
    }

    iter->data.iter->source = sub;
    iter->data.iter->counter = start;
    return iter;
}

static void free_sub_iters(Value **subs, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (subs[i]) value_release(subs[i]);
    }
    free(subs);
}

static int make_sub_iters(VM *vm, Value **iterables, uint32_t count, Value ***out) {
    *out = NULL;
    if (count == 0) return 0;

    Value **subs = calloc(count, sizeof(Value *));
    if (!subs) {
        vm->last_error = VM_ERR_OOM;
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        subs[i] = value_make_iter(vm, iterables[i]);
        if (!subs[i]) {
            free_sub_iters(subs, i);
            return -1;
        }
    }

    *out = subs;
    return 0;
}

Value *builtin_zip(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t npos = nargs - kw_count(kwnames);
    Value *strict;
    if (bind_keywords(vm, args + npos, kwnames, STRICT_KEYWORD, 1, &strict) != 0) return NULL;

    Value **subs;
    if (make_sub_iters(vm, args, npos, &subs) != 0) return NULL;

    Value *iter = value_new_iterator(ITER_ZIP);
    if (!iter) {
        free_sub_iters(subs, npos);
        return vm_fail(vm, VM_ERR_OOM);
    }

    iter->data.iter->subs = subs;
    iter->data.iter->nsubs = npos;
    iter->data.iter->strict = strict ? value_truthy(strict) : 0;
    return iter;
}

Value *builtin_map(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t npos = nargs - kw_count(kwnames);
    Value *strict;
    if (bind_keywords(vm, args + npos, kwnames, STRICT_KEYWORD, 1, &strict) != 0) return NULL;
    if (npos < 2) return vm_fail(vm, VM_ERR_TYPE);

    Value **subs;
    if (make_sub_iters(vm, args + 1, npos - 1, &subs) != 0) return NULL;

    Value *iter = value_new_iterator(ITER_MAP);
    if (!iter) {
        free_sub_iters(subs, npos - 1);
        return vm_fail(vm, VM_ERR_OOM);
    }

    iter->data.iter->func = value_retain(args[0]);
    iter->data.iter->subs = subs;
    iter->data.iter->nsubs = npos - 1;
    iter->data.iter->strict = strict ? value_truthy(strict) : 0;
    return iter;
}

Value *builtin_filter(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 2, 2) != 0) return NULL;

    Value *sub = value_make_iter(vm, args[1]);
    if (!sub) return NULL;

    Value *iter = value_new_iterator(ITER_FILTER);
    if (!iter) {
        value_release(sub);
        return vm_fail(vm, VM_ERR_OOM);
    }

    iter->data.iter->source = sub;
    iter->data.iter->func = value_retain(args[0]);
    return iter;
}

Value *builtin_reversed(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *seq = args[0];

    switch (seq->tag) {
        case TAG_LIST:
        case TAG_TUPLE:
        case TAG_STRING:
        case TAG_BYTES:
        case TAG_RANGE:
        case TAG_DICT:
        case TAG_DICT_VIEW:
            break;
        default:
            return vm_fail(vm, VM_ERR_TYPE);
    }

    Value *iter = value_new_iterator(ITER_REVERSED);
    if (!iter) return vm_fail(vm, VM_ERR_OOM);

    IterObject *it = iter->data.iter;
    it->source = value_retain(seq);
    it->length = value_length(seq);
    it->index = seq->tag == TAG_STRING ? (int64_t)seq->data.str.len : it->length - 1;
    return iter;
}

static Value *builtin_iter(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (nargs == 1) return value_make_iter(vm, args[0]);
    if (!is_callable(args[0])) return vm_fail(vm, VM_ERR_TYPE);

    Value *iter = value_new_iterator(ITER_CALLABLE);
    if (!iter) return vm_fail(vm, VM_ERR_OOM);

    iter->data.iter->func = value_retain(args[0]);
    iter->data.iter->source = value_retain(args[1]);
    return iter;
}

static Value *builtin_next(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 1, 2) != 0) return NULL;
    if (args[0]->tag != TAG_ITERATOR) return vm_fail(vm, VM_ERR_TYPE);

    Value *item = iterator_next(vm, args[0]);
    if (item) return item;
    if (vm->last_error != VM_ERR_OK) return NULL;
    if (nargs == 2) return value_retain(args[1]);
    return vm_fail(vm, VM_ERR_STOP);
}

typedef struct {
    const char *name;
    NativeFn fn;
} BuiltinEntry;

static const BuiltinEntry BUILTINS[] = {
    {"print", builtin_print},
    {"len", builtin_len},
    {"repr", builtin_repr},
    {"iter", builtin_iter},
    {"next", builtin_next},
    {"abs", builtin_abs},
    {"divmod", builtin_divmod},
    {"pow", builtin_pow},
    {"round", builtin_round},
    {"sum", builtin_sum},
    {"min", builtin_min},
    {"max", builtin_max},
    {"sorted", builtin_sorted},
    {"any", builtin_any},
    {"all", builtin_all},
    {"isinstance", builtin_isinstance},
    {"callable", builtin_callable},
    {"id", builtin_id},
    {"ord", builtin_ord},
    {"chr", builtin_chr},
    {"bin", builtin_bin},
    {"oct", builtin_oct},
    {"hex", builtin_hex},
    {"format", builtin_format},
    {"ascii", builtin_ascii},
    {"hash", builtin_hash},
    {"issubclass", builtin_issubclass},
};

#define BUILTIN_COUNT ((uint32_t)(sizeof(BUILTINS) / sizeof(BUILTINS[0])))

uint32_t builtin_count(void) {
    return BUILTIN_COUNT;
}

const char *builtin_name(uint32_t index) {
    return index < BUILTIN_COUNT ? BUILTINS[index].name : NULL;
}

NativeFn builtin_function(uint32_t index) {
    return index < BUILTIN_COUNT ? BUILTINS[index].fn : NULL;
}
