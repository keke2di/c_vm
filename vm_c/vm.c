#include <stdlib.h>
#include <string.h>
#include "opcodes.h"
#include "vm_internal.h"

static int read_operand(VM *vm, uint32_t *out) {
    if (vm->ip + 4 > vm->bytecode + vm->bytecode_len) {
        vm->last_error = VM_ERR_BOUNDS;
        return 0;
    }

    memcpy(out, vm->ip, 4);
    vm->ip += 4;
    return 1;
}

static void vm_jump(VM *vm, uint32_t target) {
    if (!vm->current_frame || !vm->current_frame->func) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    size_t absolute_target = vm->current_frame->func->code_offset + (size_t)target;

    if (absolute_target >= vm->bytecode_len) {
        vm->last_error = VM_ERR_BOUNDS;
        return;
    }

    vm->ip = vm->bytecode + absolute_target;
}

static void frame_free(Frame *frame) {
    if (frame->locals) {
        for (uint32_t i = 0; i < frame->locals_cap; i++) {
            if (frame->locals[i]) value_release(frame->locals[i]);
        }
        free(frame->locals);
    }
    free(frame);
}

static int op_return(VM *vm) {
    Frame *old_frame = vm->current_frame;

    if (!old_frame) {
        vm->last_error = VM_ERR_STACK;
        return 0;
    }

    VM_DEBUG(
        "[RETURN] frame=%p stack=%u base=%u prev=%p\n",
        (void *)old_frame,
        vm->stack_top,
        old_frame->stack_base,
        (void *)old_frame->prev);

    Value *retval = NULL;

    if (vm->stack_top > old_frame->stack_base) {
        retval = vm_pop(vm);

        while (vm->stack_top > old_frame->stack_base) {
            Value *v = vm_pop(vm);
            if (v) value_release(v);
        }
    }

    if (!retval) {
        retval = value_new_none();
    }

    vm->current_frame = old_frame->prev;

    if (!retval) {
        vm->last_error = VM_ERR_OOM;
        frame_free(old_frame);
        return 1;
    }

    if (vm->current_frame) {
        vm->ip = old_frame->return_ip;
        vm_push_owned(vm, retval);
        frame_free(old_frame);
        return 0;
    }

    vm->result = retval;
    frame_free(old_frame);
    return 1;
}

static void op_load_fast(VM *vm, uint32_t idx) {
    if (!vm->current_frame) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (idx >= vm->current_frame->locals_cap) {
        vm->last_error = VM_ERR_BOUNDS;
        return;
    }

    Value *local = vm->current_frame->locals[idx];
    if (!local) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    vm_push(vm, local);
}

static void op_store_fast(VM *vm, uint32_t idx) {
    if (!vm->current_frame || idx >= vm->current_frame->locals_cap) {
        vm->last_error = VM_ERR_BOUNDS;
        return;
    }

    Value *v = vm_pop(vm);
    if (!v) return;

    if (vm->current_frame->locals[idx]) {
        value_release(vm->current_frame->locals[idx]);
    }
    vm->current_frame->locals[idx] = v;
}

static void op_load_global(VM *vm, uint32_t idx) {
    if (idx >= vm->num_names) {
        vm->last_error = VM_ERR_BOUNDS;
        return;
    }

    if (!vm->globals[idx]) {
        vm->last_error = VM_ERR_FUNC_NOT_FOUND;
        return;
    }

    vm_push(vm, vm->globals[idx]);
}

static void op_store_global(VM *vm, uint32_t idx) {
    if (idx >= vm->num_names) {
        vm->last_error = VM_ERR_BOUNDS;
        return;
    }

    Value *value = vm_pop(vm);
    if (!value) return;

    if (vm->globals[idx]) {
        value_release(vm->globals[idx]);
    }
    vm->globals[idx] = value;
}

static void op_call_kw(VM *vm, uint32_t nargs) {
    uint32_t frame_base = vm->current_frame ? vm->current_frame->stack_base : 0;
    if (vm->stack_top <= frame_base) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    Value *kwnames = vm_pop(vm);
    if (kwnames->tag != TAG_TUPLE || kwnames->data.tuple.len > nargs) {
        value_release(kwnames);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    vm_call_value(vm, nargs, kwnames);
    value_release(kwnames);
}

static int vm_prepare(VM *vm) {
    FuncEntry *entry_func = &vm->functions[vm->entry_func_index];
    vm->ip = vm->bytecode + entry_func->code_offset;

    vm->globals = calloc(vm->num_names, sizeof(Value*));
    if (vm->num_names > 0 && !vm->globals) {
        vm->last_error = VM_ERR_OOM;
        return 0;
    }

    if (vm_init_callable_globals(vm) != VM_ERR_OK) {
        return 0;
    }

    vm->stack_cap = STACK_INIT_CAP;
    vm->stack = malloc(vm->stack_cap * sizeof(Value*));
    if (!vm->stack) {
        vm->last_error = VM_ERR_OOM;
        return 0;
    }
    vm->stack_top = 0;

    Frame *frame = malloc(sizeof(Frame));
    if (!frame) {
        vm->last_error = VM_ERR_OOM;
        return 0;
    }

    frame->prev = NULL;
    frame->return_ip = NULL;
    frame->func = entry_func;
    frame->locals_cap = entry_func->locals_count;
    frame->locals = NULL;
    frame->stack_base = 0;

    if (frame->locals_cap > 0) {
        frame->locals = calloc(frame->locals_cap, sizeof(Value*));
        if (!frame->locals) {
            free(frame);
            vm->last_error = VM_ERR_OOM;
            return 0;
        }
    }

    vm->current_frame = frame;
    return 1;
}

int vm_step(VM *vm) {
    uint8_t op = *vm->ip++;
    uint32_t operand = 0;

    switch (op) {
        case OP_NOP:
            break;

        case OP_LOAD_CONST:
            if (!read_operand(vm, &operand)) break;
            if (operand >= vm->num_constants) {
                vm->last_error = VM_ERR_BOUNDS;
                break;
            }
            vm_push(vm, vm->constants[operand]);
            break;

        case OP_LOAD_FAST:
            if (read_operand(vm, &operand)) op_load_fast(vm, operand);
            break;

        case OP_STORE_FAST:
            if (read_operand(vm, &operand)) op_store_fast(vm, operand);
            break;

        case OP_LOAD_GLOBAL:
            if (read_operand(vm, &operand)) op_load_global(vm, operand);
            break;

        case OP_STORE_GLOBAL:
            if (read_operand(vm, &operand)) op_store_global(vm, operand);
            break;

        case OP_POP_TOP: {
            Value *v = vm_pop(vm);
            if (v) value_release(v);
            break;
        }

        case OP_DUP_TOP:
            if (vm->stack_top == 0) {
                vm->last_error = VM_ERR_STACK;
                break;
            }
            vm_push(vm, vm->stack[vm->stack_top - 1]);
            break;

        case OP_DUP_TOP_TWO: {
            if (vm->stack_top < 2) {
                vm->last_error = VM_ERR_STACK;
                break;
            }
            Value *second = vm->stack[vm->stack_top - 2];
            Value *first = vm->stack[vm->stack_top - 1];
            vm_push(vm, second);
            vm_push(vm, first);
            break;
        }

        case OP_DELETE_FAST:
            if (read_operand(vm, &operand)) {
                if (!vm->current_frame || operand >= vm->current_frame->locals_cap ||
                    !vm->current_frame->locals[operand]) {
                    vm->last_error = VM_ERR_STACK;
                } else {
                    value_release(vm->current_frame->locals[operand]);
                    vm->current_frame->locals[operand] = NULL;
                }
            }
            break;

        case OP_DELETE_GLOBAL:
            if (read_operand(vm, &operand)) {
                if (operand >= vm->num_names || !vm->globals[operand]) {
                    vm->last_error = VM_ERR_FUNC_NOT_FOUND;
                } else {
                    value_release(vm->globals[operand]);
                    vm->globals[operand] = NULL;
                }
            }
            break;

        case OP_DELETE_SUBSCR:
            op_delete_index(vm);
            break;

        case OP_ROT_TWO: {
            if (vm->stack_top < 2) {
                vm->last_error = VM_ERR_STACK;
                break;
            }
            Value *tmp = vm->stack[vm->stack_top - 1];
            vm->stack[vm->stack_top - 1] = vm->stack[vm->stack_top - 2];
            vm->stack[vm->stack_top - 2] = tmp;
            break;
        }

        case OP_ROT_THREE: {
            if (vm->stack_top < 3) {
                vm->last_error = VM_ERR_STACK;
                break;
            }
            Value *top = vm->stack[vm->stack_top - 1];
            vm->stack[vm->stack_top - 1] = vm->stack[vm->stack_top - 2];
            vm->stack[vm->stack_top - 2] = vm->stack[vm->stack_top - 3];
            vm->stack[vm->stack_top - 3] = top;
            break;
        }

        case OP_BINARY_ADD:
        case OP_BINARY_SUB:
        case OP_BINARY_MUL:
        case OP_BINARY_DIV:
        case OP_BINARY_MOD:
        case OP_BINARY_POW:
        case OP_BINARY_FLOORDIV:
        case OP_BINARY_AND:
        case OP_BINARY_OR:
        case OP_BINARY_XOR:
        case OP_BINARY_LSHIFT:
        case OP_BINARY_RSHIFT:
            op_binary(vm, op);
            break;

        case OP_UNARY_NEG:
        case OP_UNARY_NOT:
        case OP_UNARY_POS:
        case OP_UNARY_INVERT:
            op_unary(vm, op);
            break;

        case OP_COMPARE_EQ:
        case OP_COMPARE_NE:
        case OP_COMPARE_LT:
        case OP_COMPARE_LE:
        case OP_COMPARE_GT:
        case OP_COMPARE_GE:
            op_compare(vm, op);
            break;

        case OP_CONTAINS:
            op_contains(vm);
            break;

        case OP_COMPARE_IS:
            op_is(vm, 0);
            break;

        case OP_COMPARE_IS_NOT:
            op_is(vm, 1);
            break;

        case OP_JUMP:
            if (read_operand(vm, &operand)) vm_jump(vm, operand);
            break;

        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE: {
            if (!read_operand(vm, &operand)) break;
            Value *cond = vm_pop(vm);
            if (!cond) break;
            int truth = value_truthy(cond);
            value_release(cond);
            if (truth == (op == OP_JUMP_IF_TRUE)) {
                vm_jump(vm, operand);
            }
            break;
        }

        case OP_RETURN:
            return op_return(vm);

        case OP_CALL:
            if (read_operand(vm, &operand)) vm_call_value(vm, operand, NULL);
            break;

        case OP_CALL_KW:
            if (read_operand(vm, &operand)) op_call_kw(vm, operand);
            break;

        case OP_CALL_METHOD:
            if (read_operand(vm, &operand)) vm_call_method(vm, operand, 0);
            break;

        case OP_CALL_METHOD_KW:
            if (read_operand(vm, &operand)) vm_call_method(vm, operand, 1);
            break;

        case OP_BUILD_LIST:
            if (read_operand(vm, &operand)) op_build_list(vm, operand);
            break;

        case OP_BUILD_TUPLE:
            if (read_operand(vm, &operand)) op_build_tuple(vm, operand);
            break;

        case OP_BUILD_MAP:
            if (read_operand(vm, &operand)) op_build_map(vm, operand);
            break;

        case OP_BUILD_SET:
            if (read_operand(vm, &operand)) op_build_set(vm, operand);
            break;

        case OP_LIST_APPEND:
            op_list_append(vm);
            break;

        case OP_SET_ADD:
            op_set_add(vm);
            break;

        case OP_MAP_ADD:
            op_map_add(vm);
            break;

        case OP_GET_INDEX:
            op_get_index(vm);
            break;

        case OP_SET_INDEX:
            op_set_index(vm);
            break;

        case OP_GET_ITER_ITEM:
            op_get_iter_item(vm);
            break;

        case OP_GET_ITER:
            op_get_iter(vm);
            break;

        case OP_UNPACK_SEQUENCE:
            if (read_operand(vm, &operand)) op_unpack_sequence(vm, operand);
            break;

        case OP_UNPACK_EX:
            if (read_operand(vm, &operand)) {
                op_unpack_ex(vm, operand & 0xFFFF, operand >> 16);
            }
            break;

        case OP_FOR_ITER: {
            if (!read_operand(vm, &operand)) break;
            Value *iter = vm_pop(vm);
            if (!iter) {
                vm->last_error = VM_ERR_STACK;
                break;
            }
            Value *next = iterator_next(vm, iter);
            value_release(iter);
            if (vm->last_error != VM_ERR_OK) break;
            if (next) {
                vm_push_owned(vm, next);
            } else {
                vm_jump(vm, operand);
            }
            break;
        }

        case OP_GET_SLICE:
            op_get_slice(vm);
            break;

        case OP_LEN:
            op_len(vm);
            break;

        case OP_FORMAT_VALUE:
            if (read_operand(vm, &operand)) op_format_value(vm, operand);
            break;

        case OP_BUILD_STRING:
            if (read_operand(vm, &operand)) op_build_string(vm, operand);
            break;

        default:
            vm->last_error = VM_ERR_INVALID_OP;
            break;
    }

    return 0;
}

int vm_run(VM *vm) {
    if (!vm || vm->entry_func_index >= vm->num_functions) {
        if (vm) vm->last_error = VM_ERR_FUNC_NOT_FOUND;
        return VM_ERR_FUNC_NOT_FOUND;
    }

    if (!vm_prepare(vm)) {
        return vm->last_error;
    }

    while (vm->last_error == VM_ERR_OK) {
        if (vm->ip >= vm->bytecode + vm->bytecode_len) {
            vm->last_error = VM_ERR_BOUNDS;
            break;
        }
        if (vm_step(vm)) {
            break;
        }
    }

    return vm->last_error;
}

void vm_free(VM *vm) {
    if (!vm) return;

    free(vm->bytecode);

    if (vm->constants) {
        for (uint32_t i = 0; i < vm->num_constants; i++) {
            if (vm->constants[i]) value_release(vm->constants[i]);
        }
        free(vm->constants);
    }

    if (vm->globals) {
        for (uint32_t i = 0; i < vm->num_names; i++) {
            if (vm->globals[i]) value_release(vm->globals[i]);
        }
        free(vm->globals);
    }

    if (vm->names) {
        for (uint32_t i = 0; i < vm->num_names; i++) {
            free(vm->names[i]);
        }
        free(vm->names);
    }

    if (vm->functions) {
        for (uint32_t i = 0; i < vm->num_functions; i++) {
            free(vm->functions[i].param_names);
            free(vm->functions[i].default_consts);
        }
        free(vm->functions);
    }

    if (vm->stack) {
        for (uint32_t i = 0; i < vm->stack_top; i++) {
            if (vm->stack[i]) value_release(vm->stack[i]);
        }
        free(vm->stack);
    }

    if (vm->result) value_release(vm->result);

    Frame *frame = vm->current_frame;
    while (frame) {
        Frame *next = frame->prev;
        frame_free(frame);
        frame = next;
    }

    memset(vm, 0, sizeof(VM));
}

const char *vm_error_string(VM *vm) {
    if (!vm) return "NULL VM";

    switch (vm->last_error) {
        case VM_ERR_OK: return "OK";
        case VM_ERR_LOAD: return "Load failed";
        case VM_ERR_OOM: return "Out of memory";
        case VM_ERR_BAD_MAGIC: return "Bad magic";
        case VM_ERR_VERSION: return "Unsupported version";
        case VM_ERR_BOUNDS: return "Bounds error";
        case VM_ERR_TYPE: return "Type error";
        case VM_ERR_STACK: return "Stack error";
        case VM_ERR_DIV_ZERO: return "Division by zero";
        case VM_ERR_FUNC_NOT_FOUND: return "Function not found";
        case VM_ERR_INVALID_OP: return "Invalid opcode";
        case VM_ERR_OVERFLOW: return "Integer overflow";
        case VM_ERR_VALUE: return "Value error";
        case VM_ERR_STOP: return "StopIteration";
        case VM_ERR_KEY: return "Key error";
        case VM_ERR_ATTR: return "Attribute error";
        case VM_ERR_RUNTIME: return "Runtime error";
        case VM_ERR_LOOKUP: return "Lookup error";
        case VM_ERR_UNICODE: return "Unicode error";
        default: return "Unknown error";
    }
}
