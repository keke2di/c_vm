#include <string.h>
#include "vm_internal.h"

Value *vm_fail(VM *vm, int error) {
    vm->last_error = error;
    return NULL;
}

int insert_error(int rc) {
    return rc == -2 ? VM_ERR_TYPE : VM_ERR_OOM;
}

uint32_t kw_count(const Value *kwnames) {
    return kwnames ? kwnames->data.tuple.len : 0;
}

int check_positional(VM *vm, uint32_t nargs, const Value *kwnames, uint32_t min, uint32_t max) {
    if (kw_count(kwnames) != 0 || nargs < min || nargs > max) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }
    return 0;
}

static int keyword_is(const Value *key, const char *name) {
    size_t len = strlen(name);
    return key && key->tag == TAG_STRING && key->data.str.len == len &&
           memcmp(key->data.str.data, name, len) == 0;
}

static int bind_named(VM *vm, Value **kwvalues, const Value *kwnames,
                      const char *const *names, uint32_t nparams, Value **slots) {
    uint32_t nkw = kw_count(kwnames);

    for (uint32_t k = 0; k < nkw; k++) {
        const Value *key = kwnames->data.tuple.items[k];
        uint32_t i = 0;

        while (i < nparams && !(names[i] && keyword_is(key, names[i]))) {
            i++;
        }

        if (i == nparams || slots[i]) {
            vm->last_error = VM_ERR_TYPE;
            return -1;
        }

        slots[i] = kwvalues[k];
    }

    return 0;
}

int bind_keywords(VM *vm, Value **kwvalues, const Value *kwnames,
                  const char *const *names, uint32_t nparams, Value **slots) {
    for (uint32_t i = 0; i < nparams; i++) {
        slots[i] = NULL;
    }
    return bind_named(vm, kwvalues, kwnames, names, nparams, slots);
}

int bind_args(VM *vm, Value **args, uint32_t nargs, const Value *kwnames,
              const char *const *names, uint32_t nparams, uint32_t max_positional,
              uint32_t required, Value **slots) {
    uint32_t nkw = kw_count(kwnames);

    if (nkw > nargs || nargs - nkw > max_positional) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }

    uint32_t npos = nargs - nkw;

    for (uint32_t i = 0; i < nparams; i++) {
        slots[i] = i < npos ? args[i] : NULL;
    }

    if (bind_named(vm, args + npos, kwnames, names, nparams, slots) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < required; i++) {
        if (!slots[i]) {
            vm->last_error = VM_ERR_TYPE;
            return -1;
        }
    }

    return 0;
}
