#include <math.h>
#include <stdlib.h>
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int is_number(const Value *v) {
    return int_like(v) || v->tag == TAG_FLOAT;
}

static const char *const START_KEYWORD[] = { "start" };
static const char *const KEY_DEFAULT_KEYWORDS[] = { "key", "default" };
static const char *const SORTED_KEYWORDS[] = { "key", "reverse" };

Value *builtin_sum(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t npos = nargs - kw_count(kwnames);
    if (npos < 1 || npos > 2) return vm_fail(vm, VM_ERR_TYPE);

    Value *start;
    if (npos == 2) {
        start = args[1];
        if (kw_count(kwnames) != 0) return vm_fail(vm, VM_ERR_TYPE);
    } else {
        if (bind_keywords(vm, args + npos, kwnames, START_KEYWORD, 1, &start) != 0) return NULL;
    }

    if (start && (start->tag == TAG_STRING || start->tag == TAG_BYTES)) {
        return vm_fail(vm, VM_ERR_TYPE);
    }

    Value *iter = value_make_iter(vm, args[0]);
    if (!iter) return NULL;

    int is_float = 0;
    int64_t isum = 0;
    double fsum = 0.0;
    double comp = 0.0;

    if (start) {
        if (int_like(start)) {
            isum = start->data.int_val;
        } else if (start->tag == TAG_FLOAT) {
            is_float = 1;
            fsum = start->data.float_val;
        } else {
            value_release(iter);
            return vm_fail(vm, VM_ERR_TYPE);
        }
    }

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;

        if (!is_number(item)) {
            value_release(item);
            vm->last_error = VM_ERR_TYPE;
            break;
        }

        if (!is_float && int_like(item)) {
            int64_t v = item->data.int_val;
            if ((v > 0 && isum > INT64_MAX - v) || (v < 0 && isum < INT64_MIN - v)) {
                value_release(item);
                vm->last_error = VM_ERR_OVERFLOW;
                break;
            }
            isum += v;
        } else {
            if (!is_float) {
                is_float = 1;
                fsum = (double)isum;
                comp = 0.0;
            }
            double x = item->tag == TAG_FLOAT ? item->data.float_val : (double)item->data.int_val;
            double t = fsum + x;
            if (fabs(fsum) >= fabs(x)) {
                comp += (fsum - t) + x;
            } else {
                comp += (x - t) + fsum;
            }
            fsum = t;
        }

        value_release(item);
    }

    value_release(iter);
    if (vm->last_error != VM_ERR_OK) return NULL;

    return is_float ? value_new_float(fsum + comp) : value_new_int(isum);
}

static Value *apply_key(VM *vm, Value *key, Value *item) {
    if (!key || key->tag == TAG_NONE) return value_retain(item);
    Value *args[1] = { item };
    Value *result = NULL;
    if (vm_call_sync(vm, key, args, 1, &result) != 0) return NULL;
    return result;
}

static Value *extreme(VM *vm, Value **args, uint32_t nargs, const Value *kwnames, int want_max) {
    uint32_t npos = nargs - kw_count(kwnames);
    if (npos < 1) return vm_fail(vm, VM_ERR_TYPE);

    Value *slots[2];
    if (bind_keywords(vm, args + npos, kwnames, KEY_DEFAULT_KEYWORDS, 2, slots) != 0) return NULL;
    Value *key = slots[0];
    Value *fallback = slots[1];

    int from_iterable = npos == 1;
    if (!from_iterable && fallback) return vm_fail(vm, VM_ERR_TYPE);

    Value *iter = NULL;
    uint32_t index = 0;

    if (from_iterable) {
        iter = value_make_iter(vm, args[0]);
        if (!iter) return NULL;
    }

    Value *best = NULL;
    Value *best_key = NULL;

    for (;;) {
        Value *item;
        if (from_iterable) {
            item = iterator_next(vm, iter);
            if (!item) break;
        } else {
            if (index >= npos) break;
            item = value_retain(args[index++]);
        }

        Value *k = apply_key(vm, key, item);
        if (!k) {
            value_release(item);
            break;
        }

        if (!best) {
            best = item;
            best_key = k;
            continue;
        }

        int ok = 1;
        int cmp = value_order(k, best_key, &ok);
        if (!ok) {
            value_release(item);
            value_release(k);
            vm->last_error = VM_ERR_TYPE;
            break;
        }

        if (want_max ? cmp > 0 : cmp < 0) {
            value_release(best);
            value_release(best_key);
            best = item;
            best_key = k;
        } else {
            value_release(item);
            value_release(k);
        }
    }

    if (iter) value_release(iter);
    value_release(best_key);

    if (vm->last_error != VM_ERR_OK) {
        value_release(best);
        return NULL;
    }

    if (!best) {
        if (fallback) return value_retain(fallback);
        return vm_fail(vm, VM_ERR_VALUE);
    }

    return best;
}

Value *builtin_min(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return extreme(vm, args, nargs, kwnames, 0);
}

Value *builtin_max(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return extreme(vm, args, nargs, kwnames, 1);
}

typedef struct {
    Value *key;
    Value *val;
} SortItem;

static int merge(SortItem *items, SortItem *tmp, size_t lo, size_t mid, size_t hi, int *ok) {
    size_t i = lo, j = mid, k = lo;

    while (i < mid && j < hi) {
        int cmp = value_order(items[j].key, items[i].key, ok);
        if (!*ok) return -1;
        if (cmp < 0) {
            tmp[k++] = items[j++];
        } else {
            tmp[k++] = items[i++];
        }
    }

    while (i < mid) tmp[k++] = items[i++];
    while (j < hi) tmp[k++] = items[j++];

    for (size_t x = lo; x < hi; x++) items[x] = tmp[x];
    return 0;
}

static int merge_sort(SortItem *items, SortItem *tmp, size_t lo, size_t hi, int *ok) {
    if (hi - lo <= 1) return 0;
    size_t mid = lo + (hi - lo) / 2;
    if (merge_sort(items, tmp, lo, mid, ok) != 0) return -1;
    if (merge_sort(items, tmp, mid, hi, ok) != 0) return -1;
    return merge(items, tmp, lo, mid, hi, ok);
}

Value *builtin_sorted(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    uint32_t npos = nargs - kw_count(kwnames);
    if (npos != 1) return vm_fail(vm, VM_ERR_TYPE);

    Value *slots[2];
    if (bind_keywords(vm, args + npos, kwnames, SORTED_KEYWORDS, 2, slots) != 0) return NULL;
    Value *key = slots[0];
    int reverse = slots[1] ? value_truthy(slots[1]) : 0;

    Value *list = value_list_from_iterable(vm, args[0]);
    if (!list) return NULL;

    size_t n = list->data.list.len;
    if (n <= 1) return list;

    SortItem *items = malloc(n * sizeof(SortItem));
    SortItem *tmp = malloc(n * sizeof(SortItem));
    if (!items || !tmp) {
        free(items);
        free(tmp);
        value_release(list);
        return vm_fail(vm, VM_ERR_OOM);
    }

    int failed = 0;
    for (size_t i = 0; i < n; i++) {
        size_t idx = reverse ? n - 1 - i : i;
        items[i].val = value_retain(list->data.list.items[idx]);
        items[i].key = apply_key(vm, key, items[i].val);
        if (!items[i].key) {
            value_release(items[i].val);
            for (size_t j = 0; j < i; j++) {
                value_release(items[j].key);
                value_release(items[j].val);
            }
            failed = 1;
            break;
        }
    }

    if (!failed) {
        int ok = 1;
        if (merge_sort(items, tmp, 0, n, &ok) != 0 || !ok) {
            if (ok) vm->last_error = VM_ERR_OOM;
            else vm->last_error = VM_ERR_TYPE;
            for (size_t i = 0; i < n; i++) {
                value_release(items[i].key);
                value_release(items[i].val);
            }
            failed = 1;
        }
    }

    free(tmp);

    if (failed) {
        free(items);
        value_release(list);
        return NULL;
    }

    Value *result = value_new_list();
    if (result) {
        for (size_t i = 0; i < n; i++) {
            size_t idx = reverse ? n - 1 - i : i;
            if (value_list_append(result, items[idx].val) != 0) {
                value_release(result);
                result = NULL;
                break;
            }
        }
    }

    for (size_t i = 0; i < n; i++) {
        value_release(items[i].key);
        value_release(items[i].val);
    }
    free(items);
    value_release(list);

    return result ? result : vm_fail(vm, VM_ERR_OOM);
}

static Value *any_all(VM *vm, Value **args, uint32_t nargs, const Value *kwnames, int want_all) {
    if (check_positional(vm, nargs, kwnames, 1, 1) != 0) return NULL;

    Value *iter = value_make_iter(vm, args[0]);
    if (!iter) return NULL;

    int result = want_all;

    for (;;) {
        Value *item = iterator_next(vm, iter);
        if (!item) break;
        int truth = value_truthy(item);
        value_release(item);
        if (want_all ? !truth : truth) {
            result = !want_all;
            break;
        }
    }

    value_release(iter);
    if (vm->last_error != VM_ERR_OK) return NULL;
    return value_bool(result);
}

Value *builtin_any(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return any_all(vm, args, nargs, kwnames, 0);
}

Value *builtin_all(VM *vm, Value **args, uint32_t nargs, const Value *kwnames) {
    return any_all(vm, args, nargs, kwnames, 1);
}
