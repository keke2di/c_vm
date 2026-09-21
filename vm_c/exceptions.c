#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

typedef struct {
    const char *name;
    const char *base;
} ExceptionSpec;

static const ExceptionSpec EXCEPTION_SPECS[] = {
    { "BaseException", NULL },
    { "BaseExceptionGroup", "BaseException" },
    { "Exception", "BaseException" },
    { "ExceptionGroup", "Exception" },
    { "GeneratorExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "SystemExit", "BaseException" },
    { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" },
    { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" },
    { "AssertionError", "Exception" },
    { "AttributeError", "Exception" },
    { "BufferError", "Exception" },
    { "EOFError", "Exception" },
    { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" },
    { "LookupError", "Exception" },
    { "IndexError", "LookupError" },
    { "KeyError", "LookupError" },
    { "MemoryError", "Exception" },
    { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" },
    { "OSError", "Exception" },
    { "BlockingIOError", "OSError" },
    { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" },
    { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" },
    { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" },
    { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" },
    { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" },
    { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" },
    { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" },
    { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" },
    { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" },
    { "PythonFinalizationError", "RuntimeError" },
    { "StopAsyncIteration", "Exception" },
    { "StopIteration", "Exception" },
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "SystemError", "Exception" },
    { "TypeError", "Exception" },
    { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" },
    { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" },
    { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" },
    { "BytesWarning", "Warning" },
    { "DeprecationWarning", "Warning" },
    { "EncodingWarning", "Warning" },
    { "FutureWarning", "Warning" },
    { "ImportWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" },
    { "ResourceWarning", "Warning" },
    { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" },
    { "UnicodeWarning", "Warning" },
    { "UserWarning", "Warning" },
};

#define EXCEPTION_SPEC_COUNT ((int)(sizeof(EXCEPTION_SPECS) / sizeof(EXCEPTION_SPECS[0])))

static Value g_exception_types[sizeof(EXCEPTION_SPECS) / sizeof(EXCEPTION_SPECS[0])];
static int g_exception_bases[sizeof(EXCEPTION_SPECS) / sizeof(EXCEPTION_SPECS[0])];
static int g_exception_ready = 0;

static const struct {
    const char *alias;
    const char *target;
} EXCEPTION_ALIASES[] = {
    { "EnvironmentError", "OSError" },
    { "IOError", "OSError" },
    { "WindowsError", "OSError" },
};

#define EXCEPTION_ALIAS_COUNT ((int)(sizeof(EXCEPTION_ALIASES) / sizeof(EXCEPTION_ALIASES[0])))

const char *exception_type_name(int index) {
    if (index < 0 || index >= EXCEPTION_SPEC_COUNT) return "BaseException";
    return EXCEPTION_SPECS[index].name;
}

int exception_type_count(void) {
    return EXCEPTION_SPEC_COUNT;
}

int exception_type_at(int index) {
    return TYPE_BASE_EXCEPTION + index;
}

int exception_index_of(int type_id) {
    int index = (int)type_id - TYPE_BASE_EXCEPTION;
    return (index >= 0 && index < EXCEPTION_SPEC_COUNT) ? index : -1;
}

int exception_base_index(int index) {
    if (index < 0 || index >= EXCEPTION_SPEC_COUNT) return -1;
    return g_exception_bases[index];
}

Value *exception_type_object(int index) {
    if (index < 0 || index >= EXCEPTION_SPEC_COUNT) return NULL;
    return &g_exception_types[index];
}

int exception_type_id_for_name(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < EXCEPTION_SPEC_COUNT; i++) {
        if (strcmp(EXCEPTION_SPECS[i].name, name) == 0) {
            return TYPE_BASE_EXCEPTION + i;
        }
    }

    return -1;
}

static void prepare_exceptions(void) {
    if (g_exception_ready) return;

    for (int i = 0; i < EXCEPTION_SPEC_COUNT; i++) {
        g_exception_types[i].tag = TAG_TYPE;
        g_exception_types[i].refcount = UINT32_MAX;
        memset(&g_exception_types[i].data, 0, sizeof(g_exception_types[i].data));
        g_exception_types[i].data.int_val = TYPE_BASE_EXCEPTION + i;
        g_exception_bases[i] = -1;
    }

    for (int i = 0; i < EXCEPTION_SPEC_COUNT; i++) {
        const char *base = EXCEPTION_SPECS[i].base;

        if (!base) continue;

        for (int j = 0; j < EXCEPTION_SPEC_COUNT; j++) {
            if (strcmp(EXCEPTION_SPECS[j].name, base) == 0) {
                g_exception_bases[i] = j;
                break;
            }
        }
    }

    g_exception_ready = 1;
}

int exception_type_is_subtype(int type_id, int base_id) {
    if (type_id == base_id) return 1;

    prepare_exceptions();

    int index = exception_index_of(type_id);

    while (index >= 0) {
        if (TYPE_BASE_EXCEPTION + index == base_id) return 1;
        index = g_exception_bases[index];
    }

    return 0;
}

int vm_init_exceptions(VM *vm) {
    prepare_exceptions();

    for (uint32_t n = 0; n < vm->num_names; n++) {
        for (int i = 0; i < EXCEPTION_SPEC_COUNT; i++) {
            if (strcmp(vm->names[n], EXCEPTION_SPECS[i].name) != 0) continue;

            if (vm->globals[n]) value_release(vm->globals[n]);
            vm->globals[n] = value_retain(&g_exception_types[i]);
            break;
        }

        if (vm->globals[n]) continue;

        for (int i = 0; i < EXCEPTION_ALIAS_COUNT; i++) {
            if (strcmp(vm->names[n], EXCEPTION_ALIASES[i].alias) != 0) continue;

            int target = exception_type_id_for_name(EXCEPTION_ALIASES[i].target);
            if (target < 0) break;

            Value *type = exception_type_object(exception_index_of(target));
            if (!type) break;

            if (vm->globals[n]) value_release(vm->globals[n]);
            vm->globals[n] = value_retain(type);
            break;
        }
    }

    return VM_ERR_OK;
}

Value *value_new_exception(uint32_t type_id, Value *args) {
    Value *v = value_new_object();

    if (!v) return NULL;

    v->tag = TAG_EXCEPTION;
    v->data.exception.type_id = type_id;
    v->data.exception.args = args ? value_retain(args) : NULL;
    return v;
}

Value *construct_exception(VM *vm, uint32_t type_id, Value **args, uint32_t nargs, const Value *kwnames) {
    if (check_positional(vm, nargs, kwnames, 0, UINT32_MAX) != 0) return NULL;

    Value *packed = value_new_tuple(nargs);
    if (!packed) return vm_fail(vm, VM_ERR_OOM);

    for (uint32_t i = 0; i < nargs; i++) {
        packed->data.tuple.items[i] = value_retain(args[i]);
    }

    Value *exception = value_new_exception(type_id, packed);
    value_release(packed);

    return exception ? exception : vm_fail(vm, VM_ERR_OOM);
}

const char *error_type_name(int error) {
    switch (error) {
        case VM_ERR_TYPE: return "TypeError";
        case VM_ERR_VALUE: return "ValueError";
        case VM_ERR_KEY: return "KeyError";
        case VM_ERR_BOUNDS: return "IndexError";
        case VM_ERR_ATTR: return "AttributeError";
        case VM_ERR_RUNTIME: return "RuntimeError";
        case VM_ERR_OVERFLOW: return "OverflowError";
        case VM_ERR_DIV_ZERO: return "ZeroDivisionError";
        case VM_ERR_STOP: return "StopIteration";
        case VM_ERR_LOOKUP: return "LookupError";
        case VM_ERR_UNICODE: return "UnicodeError";
        case VM_ERR_STACK: return "UnboundLocalError";
        case VM_ERR_FUNC_NOT_FOUND: return "NameError";
        case VM_ERR_RECURSION: return "RecursionError";
        case VM_ERR_OOM: return "MemoryError";
        default: return "RuntimeError";
    }
}

const char *error_type_message(int error) {
    switch (error) {
        case VM_ERR_TYPE: return "invalid operation";
        case VM_ERR_VALUE: return "invalid value";
        case VM_ERR_KEY: return "missing key";
        case VM_ERR_BOUNDS: return "index out of range";
        case VM_ERR_ATTR: return "no such method or attribute";
        case VM_ERR_RUNTIME: return "invalid runtime state";
        case VM_ERR_OVERFLOW: return "result out of range";
        case VM_ERR_DIV_ZERO: return "division by zero";
        case VM_ERR_STOP: return "";
        case VM_ERR_LOOKUP: return "unknown lookup";
        case VM_ERR_UNICODE: return "invalid text encoding";
        case VM_ERR_STACK: return "local variable is not bound";
        case VM_ERR_FUNC_NOT_FOUND: return "name is not defined";
        case VM_ERR_RECURSION: return "maximum recursion depth exceeded";
        case VM_ERR_OOM: return "out of memory";
        default: return "runtime failure";
    }
}

Value *vm_exception_from_error(VM *vm, int error) {
    const char *type_name = error_type_name(error);
    const char *message = error_type_message(error);
    int type_id = exception_type_id_for_name(type_name);

    if (type_id < 0) type_id = TYPE_BASE_EXCEPTION;

    Value *text = value_new_string(message);
    if (!text) return NULL;

    Value *args = value_new_tuple(1);

    if (!args) {
        value_release(text);
        return NULL;
    }

    args->data.tuple.items[0] = text;

    Value *exception = value_new_exception((uint32_t)type_id, args);
    value_release(args);
    return exception;
}

int value_is_exception(const Value *v) {
    return v && v->tag == TAG_EXCEPTION;
}

Value *vm_fail_with_value(VM *vm, const char *type_name, Value *value) {
    int type_id = exception_type_id_for_name(type_name);

    if (type_id < 0) return vm_fail(vm, VM_ERR_VALUE);

    Value *args = value_new_tuple(1);

    if (!args) return vm_fail(vm, VM_ERR_OOM);

    args->data.tuple.items[0] = value_retain(value);

    Value *exception = value_new_exception((uint32_t)type_id, args);
    value_release(args);

    if (!exception) return vm_fail(vm, VM_ERR_OOM);

    if (vm->exception) value_release(vm->exception);
    vm->exception = exception;
    vm->last_error = VM_ERR_RAISED;
    return NULL;
}

Value *vm_raise_error(VM *vm, const char *type_name, const char *message) {
    int type_id = exception_type_id_for_name(type_name);

    if (type_id < 0) return NULL;

    Value *text = value_new_string(message ? message : "");
    if (!text) {
        vm->last_error = VM_ERR_OOM;
        return NULL;
    }

    Value *args = value_new_tuple(1);

    if (!args) {
        value_release(text);
        vm->last_error = VM_ERR_OOM;
        return NULL;
    }

    args->data.tuple.items[0] = text;

    Value *exception = value_new_exception((uint32_t)type_id, args);
    value_release(args);

    if (!exception) {
        vm->last_error = VM_ERR_OOM;
        return NULL;
    }

    return exception;
}

Value *vm_make_raised_exception(VM *vm, Value *value) {
    if (value_is_exception(value)) return value_retain(value);

    if (value->tag == TAG_TYPE && exception_index_of((int)value->data.int_val) >= 0) {
        Value *args = value_new_tuple(0);

        if (!args) {
            vm->last_error = VM_ERR_OOM;
            return NULL;
        }

        Value *exception = value_new_exception((uint32_t)value->data.int_val, args);
        value_release(args);

        if (!exception) vm->last_error = VM_ERR_OOM;
        return exception;
    }

    vm->last_error = VM_ERR_RAISED;
    return vm_raise_error(vm, "TypeError", "exceptions must derive from BaseException");
}

int vm_exception_matches(VM *vm, Value *exception, Value *types) {
    (void)vm;

    Value *type = value_type_of(exception);

    if (types->tag == TAG_TYPE) {
        return value_type_is_subtype(type, types);
    }

    if (types->tag == TAG_TUPLE) {
        for (uint32_t i = 0; i < types->data.tuple.len; i++) {
            int result = vm_exception_matches(vm, exception, types->data.tuple.items[i]);
            if (result != 0) return result;
        }
        return 0;
    }

    return -1;
}

Value *vm_take_exception(VM *vm, int error) {
    if (vm->exception) {
        Value *pending = vm->exception;
        vm->exception = NULL;
        return pending;
    }

    Value *created = vm_exception_from_error(vm, error);
    return created;
}

static void write_exception_text(VM *vm, const Value *exception, FILE *out) {
    int index = exception_index_of((int)exception->data.exception.type_id);
    const char *name = index >= 0 ? exception_type_name(index) : "Exception";
    const Value *args = exception->data.exception.args;
    uint32_t count = args && args->tag == TAG_TUPLE ? args->data.tuple.len : 0;

    fprintf(out, "%s", name);

    if (count == 0) {
        fprintf(out, "\n");
        return;
    }

    char *text = NULL;

    if (count == 1) {
        text = value_to_string(args->data.tuple.items[0]);
    } else {
        text = value_to_string(args);
    }

    fprintf(out, ": %s\n", text ? text : "");
    if (text) free(text);
    (void)vm;
}

uint32_t vm_frame_line(const VM *vm, const Frame *frame) {
    if (!frame || !frame->func || frame->func->line_count == 0) return 0;

    const FuncEntry *func = frame->func;
    size_t offset = (size_t)(vm->ip - (vm->bytecode + func->code_offset));
    uint32_t found = 0;

    for (uint32_t i = 0; i < func->line_count; i++) {
        if (func->line_offsets[i] <= offset) {
            found = func->line_numbers[i];
        } else {
            break;
        }
    }

    return found;
}

void vm_write_exception(VM *vm) {
    if (!vm->exception) return;

    const char *source = vm->source_name ? vm->source_name : "<module>";
    Frame *frames[64];
    uint32_t count = 0;

    for (Frame *f = vm->current_frame; f != NULL && count < 64; f = f->prev) {
        frames[count++] = f;
    }

    if (count > 0) {
        fprintf(stderr, "Traceback (most recent call last):\n");

        for (uint32_t i = count; i > 0; i--) {
            Frame *frame = frames[i - 1];
            const char *name = "<module>";

            if (frame->func &&
                frame->func != &vm->functions[vm->entry_func_index] &&
                frame->func->name_index < vm->num_names) {
                name = vm->names[frame->func->name_index];
            }

            fprintf(
                stderr,
                "  File \"%s\", line %u, in %s\n",
                source,
                (unsigned)vm_frame_line(vm, frame),
                name
            );
        }
    }

    write_exception_text(vm, vm->exception, stderr);
}
