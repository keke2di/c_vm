#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

static int int_like(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static void release_stack_range(VM *vm, uint32_t start, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (vm->stack[start + i]) {
            value_release(vm->stack[start + i]);
            vm->stack[start + i] = NULL;
        }
    }
    vm->stack_top -= count;
}

void op_build_list(VM *vm, uint32_t count) {
    if (count > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *list = value_new_list();
    if (!list) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t start = vm->stack_top - count;

    for (uint32_t i = 0; i < count; i++) {
        Value *item = vm->stack[start + i];

        if (value_list_append(list, item) != 0) {
            release_stack_range(vm, start, count);
            value_release(list);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        value_release(item);
        vm->stack[start + i] = NULL;
    }

    vm->stack_top -= count;
    vm_push_owned(vm, list);
}

void op_build_tuple(VM *vm, uint32_t count) {
    if (count > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *tuple = value_new_tuple(count);
    if (!tuple) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t start = vm->stack_top - count;

    for (uint32_t i = 0; i < count; i++) {
        tuple->data.tuple.items[i] = vm->stack[start + i];
        vm->stack[start + i] = NULL;
    }

    vm->stack_top -= count;
    vm_push_owned(vm, tuple);
}

void op_build_map(VM *vm, uint32_t count) {
    if (count > UINT32_MAX / 2 || 2 * count > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *dict = value_new_dict();
    if (!dict) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t total = 2 * count;
    uint32_t start = vm->stack_top - total;

    for (uint32_t i = 0; i < count; i++) {
        Value *key = vm->stack[start + 2 * i];
        Value *value = vm->stack[start + 2 * i + 1];

        if (value_dict_set(dict, key, value) != 0) {
            release_stack_range(vm, start, total);
            value_release(dict);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        value_release(key);
        value_release(value);
        vm->stack[start + 2 * i] = NULL;
        vm->stack[start + 2 * i + 1] = NULL;
    }

    vm->stack_top -= total;
    vm_push_owned(vm, dict);
}

void op_build_set(VM *vm, uint32_t count) {
    if (count > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *set = value_new_set();
    if (!set) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    uint32_t start = vm->stack_top - count;

    for (uint32_t i = 0; i < count; i++) {
        Value *item = vm->stack[start + i];

        if (value_set_add(set, item) != 0) {
            release_stack_range(vm, start, count);
            value_release(set);
            vm->last_error = VM_ERR_OOM;
            return;
        }

        value_release(item);
        vm->stack[start + i] = NULL;
    }

    vm->stack_top -= count;
    vm_push_owned(vm, set);
}

void op_list_append(VM *vm) {
    if (vm->stack_top < 2) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *value = vm_pop(vm);
    Value *list = vm_pop(vm);

    if (list->tag != TAG_LIST) {
        vm->last_error = VM_ERR_TYPE;
    } else if (value_list_append(list, value) != 0) {
        vm->last_error = VM_ERR_OOM;
    }

    value_release(value);
    value_release(list);
}

void op_set_add(VM *vm) {
    if (vm->stack_top < 2) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *value = vm_pop(vm);
    Value *set = vm_pop(vm);

    if (set->tag != TAG_SET) {
        vm->last_error = VM_ERR_TYPE;
    } else if (value_set_add(set, value) != 0) {
        vm->last_error = VM_ERR_OOM;
    }

    value_release(value);
    value_release(set);
}

void op_map_add(VM *vm) {
    if (vm->stack_top < 3) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *value = vm_pop(vm);
    Value *key = vm_pop(vm);
    Value *dict = vm_pop(vm);

    if (dict->tag != TAG_DICT) {
        vm->last_error = VM_ERR_TYPE;
    } else if (value_dict_set(dict, key, value) != 0) {
        vm->last_error = VM_ERR_OOM;
    }

    value_release(value);
    value_release(key);
    value_release(dict);
}

static int normalize_index(int64_t *idx, int64_t len) {
    if (*idx < 0) *idx += len;
    return *idx >= 0 && *idx < len;
}

void op_get_index(VM *vm) {
    Value *idx_val = vm_pop(vm);
    Value *container = vm_pop(vm);

    if (!idx_val || !container) {
        if (idx_val) value_release(idx_val);
        if (container) value_release(container);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (container->tag == TAG_DICT) {
        Value *found = value_dict_get(container, idx_val);
        if (found) {
            vm_push(vm, found);
        } else {
            vm_push_owned(vm, value_new_int(0));
        }
    } else if (!int_like(idx_val)) {
        vm->last_error = VM_ERR_TYPE;
    } else {
        int64_t idx = idx_val->data.int_val;

        switch (container->tag) {
            case TAG_LIST:
                if (!normalize_index(&idx, container->data.list.len)) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push(vm, container->data.list.items[idx]);
                }
                break;

            case TAG_TUPLE:
                if (!normalize_index(&idx, container->data.tuple.len)) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push(vm, container->data.tuple.items[idx]);
                }
                break;

            case TAG_BYTES:
                if (!normalize_index(&idx, container->data.bytes.len)) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push_owned(vm, value_new_int(container->data.bytes.data[idx]));
                }
                break;

            case TAG_STRING:
                if (!normalize_index(&idx, container->data.str.len)) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push_owned(vm, value_new_string_len(container->data.str.data + idx, 1));
                }
                break;

            default:
                vm->last_error = VM_ERR_TYPE;
                break;
        }
    }

    value_release(idx_val);
    value_release(container);
}

void op_get_iter_item(VM *vm) {
    Value *idx_val = vm_pop(vm);
    Value *container = vm_pop(vm);

    if (!idx_val || !container) {
        if (idx_val) value_release(idx_val);
        if (container) value_release(container);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (!int_like(idx_val)) {
        vm->last_error = VM_ERR_TYPE;
    } else if (idx_val->data.int_val < 0) {
        vm->last_error = VM_ERR_BOUNDS;
    } else {
        size_t idx = (size_t)idx_val->data.int_val;
        Value *item = NULL;

        switch (container->tag) {
            case TAG_DICT:
                item = value_dict_key_at(container, idx);
                if (!item) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push(vm, item);
                }
                break;

            case TAG_LIST:
                item = value_list_get(container, idx);
                if (!item) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push(vm, item);
                }
                break;

            case TAG_TUPLE:
                item = value_tuple_get(container, idx);
                if (!item) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push(vm, item);
                }
                break;

            case TAG_STRING:
                if (idx >= container->data.str.len) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push_owned(vm, value_new_string_len(container->data.str.data + idx, 1));
                }
                break;

            case TAG_BYTES:
                if (idx >= container->data.bytes.len) {
                    vm->last_error = VM_ERR_BOUNDS;
                } else {
                    vm_push_owned(vm, value_new_int(container->data.bytes.data[idx]));
                }
                break;

            default:
                vm->last_error = VM_ERR_TYPE;
                break;
        }
    }

    value_release(idx_val);
    value_release(container);
}

static void adjust_slice(
    const Value *start_val,
    const Value *stop_val,
    int64_t step,
    int64_t len,
    int64_t *start_out,
    int64_t *count_out
) {
    int64_t start = int_like(start_val)
        ? start_val->data.int_val
        : (step < 0 ? INT64_MAX : 0);
    int64_t stop = int_like(stop_val)
        ? stop_val->data.int_val
        : (step < 0 ? INT64_MIN : INT64_MAX);

    if (start < 0) {
        start += len;
        if (start < 0) start = step < 0 ? -1 : 0;
    } else if (start >= len) {
        start = step < 0 ? len - 1 : len;
    }

    if (stop < 0) {
        stop += len;
        if (stop < 0) stop = step < 0 ? -1 : 0;
    } else if (stop >= len) {
        stop = step < 0 ? len - 1 : len;
    }

    int64_t count = 0;

    if (step < 0) {
        if (stop < start) count = (start - stop - 1) / (-step) + 1;
    } else if (start < stop) {
        count = (stop - start - 1) / step + 1;
    }

    *start_out = start;
    *count_out = count;
}

void op_get_slice(VM *vm) {
    Value *step_val = vm_pop(vm);
    Value *stop_val = vm_pop(vm);
    Value *start_val = vm_pop(vm);
    Value *container = vm_pop(vm);

    if (!step_val || !stop_val || !start_val || !container) {
        if (step_val) value_release(step_val);
        if (stop_val) value_release(stop_val);
        if (start_val) value_release(start_val);
        if (container) value_release(container);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    int valid_container =
        container->tag == TAG_LIST ||
        container->tag == TAG_TUPLE ||
        container->tag == TAG_STRING ||
        container->tag == TAG_BYTES;

    int valid_bounds =
        (start_val->tag == TAG_NONE || int_like(start_val)) &&
        (stop_val->tag == TAG_NONE || int_like(stop_val)) &&
        (step_val->tag == TAG_NONE || int_like(step_val));

    int64_t step = int_like(step_val) ? step_val->data.int_val : 1;

    if (!valid_container || !valid_bounds || step == 0) {
        vm->last_error = VM_ERR_TYPE;
    } else {
        if (step == INT64_MIN) step = -INT64_MAX;

        int64_t len;
        switch (container->tag) {
            case TAG_LIST: len = container->data.list.len; break;
            case TAG_TUPLE: len = container->data.tuple.len; break;
            case TAG_STRING: len = container->data.str.len; break;
            default: len = container->data.bytes.len; break;
        }

        int64_t start;
        int64_t count;
        adjust_slice(start_val, stop_val, step, len, &start, &count);

        Value *result = NULL;

        if (container->tag == TAG_LIST) {
            result = value_new_list();
            for (int64_t k = 0; result && k < count; k++) {
                if (value_list_append(result, container->data.list.items[start + k * step]) != 0) {
                    value_release(result);
                    result = NULL;
                }
            }
        } else if (container->tag == TAG_TUPLE) {
            result = value_new_tuple((size_t)count);
            for (int64_t k = 0; result && k < count; k++) {
                result->data.tuple.items[k] = value_retain(container->data.tuple.items[start + k * step]);
            }
        } else {
            const char *source = container->tag == TAG_STRING
                ? container->data.str.data
                : (const char *)container->data.bytes.data;
            char *buffer = malloc((size_t)count + 1);
            if (buffer) {
                for (int64_t k = 0; k < count; k++) {
                    buffer[k] = source[start + k * step];
                }
                buffer[count] = '\0';
                result = container->tag == TAG_STRING
                    ? value_new_string_len(buffer, (size_t)count)
                    : value_new_bytes((const unsigned char *)buffer, (size_t)count);
                free(buffer);
            }
        }

        vm_push_owned(vm, result);
    }

    value_release(step_val);
    value_release(stop_val);
    value_release(start_val);
    value_release(container);
}

void op_set_index(VM *vm) {
    Value *val = vm_pop(vm);
    Value *idx_val = vm_pop(vm);
    Value *container = vm_pop(vm);

    if (!val || !idx_val || !container) {
        if (val) value_release(val);
        if (idx_val) value_release(idx_val);
        if (container) value_release(container);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (container->tag == TAG_LIST && int_like(idx_val)) {
        int64_t idx = idx_val->data.int_val;
        if (!normalize_index(&idx, container->data.list.len)) {
            vm->last_error = VM_ERR_BOUNDS;
        } else {
            value_list_set(container, (size_t)idx, val);
        }
    } else if (container->tag == TAG_DICT && idx_val->tag == TAG_STRING) {
        if (value_dict_set(container, idx_val, val) != 0) {
            vm->last_error = VM_ERR_OOM;
        }
    } else {
        vm->last_error = VM_ERR_TYPE;
    }

    value_release(val);
    value_release(idx_val);
    value_release(container);
}

void op_len(VM *vm) {
    Value *v = vm_pop(vm);
    if (!v) return;

    int64_t len_val = -1;

    switch (v->tag) {
        case TAG_STRING: len_val = v->data.str.len; break;
        case TAG_BYTES: len_val = v->data.bytes.len; break;
        case TAG_LIST: len_val = v->data.list.len; break;
        case TAG_TUPLE: len_val = v->data.tuple.len; break;
        case TAG_DICT: len_val = v->data.dict.len; break;
        case TAG_SET: len_val = v->data.set.len; break;
        default: vm->last_error = VM_ERR_TYPE; break;
    }

    if (vm->last_error == VM_ERR_OK) {
        vm_push_owned(vm, value_new_int(len_val));
    }

    value_release(v);
}
