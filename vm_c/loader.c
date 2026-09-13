#include "vm.h"
#include "opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


static int parse_constant(
    const uint8_t *buf,
    size_t *pos,
    size_t end,
    Value **out
) {
    if (!buf || !pos || !out) return VM_ERR_BOUNDS;

    *out = NULL;

    if (*pos >= end) return VM_ERR_BOUNDS;

    uint8_t typ = buf[(*pos)++];

    switch (typ) {
        case 0x00: {
            Value *v = malloc(sizeof(Value));
            if (!v) return VM_ERR_OOM;

            memset(v, 0, sizeof(Value));
            v->tag = TAG_NONE;
            v->refcount = 1;

            *out = v;
            return VM_ERR_OK;
        }

        case 0x01:
            *out = value_new_int(0);
            return *out ? VM_ERR_OK : VM_ERR_OOM;

        case 0x02:
            *out = value_new_int(1);
            return *out ? VM_ERR_OK : VM_ERR_OOM;

        case 0x03: {
            if (end - *pos < 8) return VM_ERR_BOUNDS;

            int64_t val;
            memcpy(&val, buf + *pos, 8);
            *pos += 8;

            *out = value_new_int(val);
            return *out ? VM_ERR_OK : VM_ERR_OOM;
        }

        case 0x04: {
            if (end - *pos < 8) return VM_ERR_BOUNDS;

            double val;
            memcpy(&val, buf + *pos, 8);
            *pos += 8;

            *out = value_new_float(val);
            return *out ? VM_ERR_OK : VM_ERR_OOM;
        }

        case 0x05: {
            if (end - *pos < 4) return VM_ERR_BOUNDS;

            uint32_t len;
            memcpy(&len, buf + *pos, 4);
            *pos += 4;

            if ((size_t)len > end - *pos) return VM_ERR_BOUNDS;

            char *s = malloc((size_t)len + 1);
            if (!s) return VM_ERR_OOM;

            memcpy(s, buf + *pos, len);
            s[len] = '\0';
            *pos += len;

            *out = value_new_string(s);
            free(s);

            return *out ? VM_ERR_OK : VM_ERR_OOM;
        }

        case 0x06: {
            if (end - *pos < 4) return VM_ERR_BOUNDS;

            uint32_t len;
            memcpy(&len, buf + *pos, 4);
            *pos += 4;

            if ((size_t)len > end - *pos) return VM_ERR_BOUNDS;

            *out = value_new_bytes(buf + *pos, len);
            if (!*out) return VM_ERR_OOM;

            *pos += len;

            return VM_ERR_OK;
        }

        case 0x07: {
            if (end - *pos < 4) return VM_ERR_BOUNDS;

            uint32_t count;
            memcpy(&count, buf + *pos, 4);
            *pos += 4;

            if (count > 65536) return VM_ERR_BOUNDS;

            Value *tuple = value_new_tuple(count);
            if (!tuple) return VM_ERR_OOM;

            for (uint32_t i = 0; i < count; i++) {
                Value *item = NULL;

                int err = parse_constant(
                    buf,
                    pos,
                    end,
                    &item
                );

                if (err != VM_ERR_OK) {
                    value_release(tuple);
                    return err;
                }

                tuple->data.tuple.items[i] = item;
            }

            *out = tuple;
            return VM_ERR_OK;
        }

        default:
            return VM_ERR_BOUNDS;
    }
}

typedef struct {
    uint32_t name_idx;
    uint16_t num_locals;
    uint16_t num_params;
    uint16_t num_defaults;
    size_t param_names_pos;
    size_t defaults_pos;
    uint32_t code_len;
    size_t code_pos;
} FunctionRecord;

static int parse_function_record(
    const VM *vm,
    const uint8_t *data,
    size_t *pos,
    size_t end,
    FunctionRecord *rec
) {
    size_t p = *pos;

    if (p > end || end - p < 4 + 2 + 2 + 2) {
        return VM_ERR_BOUNDS;
    }

    memcpy(&rec->name_idx, data + p, 4);
    p += 4;

    memcpy(&rec->num_locals, data + p, 2);
    p += 2;

    memcpy(&rec->num_params, data + p, 2);
    p += 2;

    memcpy(&rec->num_defaults, data + p, 2);
    p += 2;

    if (
        rec->name_idx >= vm->num_names ||
        rec->num_params > rec->num_locals ||
        rec->num_defaults > rec->num_params
    ) {
        return VM_ERR_BOUNDS;
    }

    size_t tables_len = ((size_t)rec->num_params + rec->num_defaults) * 4;

    if (end - p < tables_len + 4) {
        return VM_ERR_BOUNDS;
    }

    rec->param_names_pos = p;

    for (uint32_t i = 0; i < rec->num_params; i++) {
        uint32_t name_idx;
        memcpy(&name_idx, data + p, 4);
        p += 4;

        if (name_idx >= vm->num_names) {
            return VM_ERR_BOUNDS;
        }
    }

    rec->defaults_pos = p;

    for (uint32_t i = 0; i < rec->num_defaults; i++) {
        uint32_t const_idx;
        memcpy(&const_idx, data + p, 4);
        p += 4;

        if (const_idx >= vm->num_constants) {
            return VM_ERR_BOUNDS;
        }
    }

    memcpy(&rec->code_len, data + p, 4);
    p += 4;

    if ((size_t)rec->code_len > end - p) {
        return VM_ERR_BOUNDS;
    }

    rec->code_pos = p;
    *pos = p + rec->code_len;

    return VM_ERR_OK;
}

int vm_load(VM *vm, const char *filename) {
    if (!vm || !filename) return VM_ERR_LOAD;

    memset(vm, 0, sizeof(VM));

    uint8_t *buf = NULL;
    uint8_t *plain = NULL;
    int err = VM_ERR_OK;

    FILE *f = fopen(filename, "rb");
    if (!f) {
        vm->last_error = VM_ERR_LOAD;
        return vm->last_error;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        vm->last_error = VM_ERR_LOAD;
        return vm->last_error;
    }

    long fsize_long = ftell(f);

    if (
        fsize_long < 0 ||
        (unsigned long long)fsize_long >
            (unsigned long long)SIZE_MAX
    ) {
        fclose(f);
        vm->last_error = VM_ERR_LOAD;
        return vm->last_error;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        vm->last_error = VM_ERR_LOAD;
        return vm->last_error;
    }

    size_t fsize = (size_t)fsize_long;

    if (
        fsize <
        4 + 1 + 1 + 2 + 4 + 4 + 4 + 4 + 16 + 12
    ) {
        fclose(f);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    buf = malloc(fsize);

    if (!buf) {
        fclose(f);
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }

    if (fread(buf, 1, fsize, f) != fsize) {
        free(buf);
        fclose(f);
        vm->last_error = VM_ERR_LOAD;
        return vm->last_error;
    }

    fclose(f);

    size_t pos = 0;

    if (memcmp(buf + pos, "CVM2", 4) != 0) {
        free(buf);
        vm->last_error = VM_ERR_BAD_MAGIC;
        return vm->last_error;
    }

    pos += 4;

    uint8_t version = buf[pos++];

    if (version != 3) {
        free(buf);
        vm->last_error = VM_ERR_VERSION;
        return vm->last_error;
    }

    uint8_t pepper_id = buf[pos++];
    uint16_t flags;

    memcpy(&flags, buf + pos, 2);
    pos += 2;

    if (pepper_id != 1 || flags != 0) {
        free(buf);
        vm->last_error = VM_ERR_VERSION;
        return vm->last_error;
    }

    uint32_t plain_len;
    uint32_t ct_len;
    uint32_t salt_len;
    uint32_t nonce_len;

    memcpy(&plain_len, buf + pos, 4);
    pos += 4;

    memcpy(&ct_len, buf + pos, 4);
    pos += 4;

    memcpy(&salt_len, buf + pos, 4);
    pos += 4;

    memcpy(&nonce_len, buf + pos, 4);
    pos += 4;

    if (salt_len != 16 || nonce_len != 12) {
        free(buf);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    if (plain_len != ct_len) {
        free(buf);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    if (
        salt_len > fsize - pos ||
        nonce_len > fsize - pos - salt_len
    ) {
        free(buf);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    pos += salt_len;
    pos += nonce_len;

    if (ct_len > fsize - pos) {
        free(buf);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    if ((size_t)ct_len != fsize - pos) {
        free(buf);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    uint8_t *ct = buf + pos;

    plain = malloc(ct_len);

    if (!plain) {
        free(buf);
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }

    memcpy(plain, ct, ct_len);

    if (plain_len != ct_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    size_t ppos = 0;

    if (ppos + 4 > plain_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    uint32_t entry_idx;
    memcpy(&entry_idx, plain + ppos, 4);
    ppos += 4;

    if (ppos + 5 > plain_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    uint8_t sec_tag = plain[ppos++];

    uint32_t sec_len;
    memcpy(&sec_len, plain + ppos, 4);
    ppos += 4;

    if (
        sec_tag != 0x01 ||
        (size_t)sec_len > plain_len - ppos
    ) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    size_t const_end = ppos + sec_len;

    if (ppos + 4 > const_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    uint32_t num_constants;
    memcpy(&num_constants, plain + ppos, 4);
    ppos += 4;

    if (num_constants > 65536) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    vm->num_constants = num_constants;

    vm->constants = calloc(
        num_constants,
        sizeof(Value *)
    );

    if (!vm->constants && num_constants > 0) {
        err = VM_ERR_OOM;
        goto fail;
    }

    for (uint32_t i = 0; i < num_constants; i++) {
        err = parse_constant(
            plain,
            &ppos,
            const_end,
            &vm->constants[i]
        );

        if (err != VM_ERR_OK) {
            goto fail;
        }
    }

    if (ppos != const_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    if (ppos + 5 > plain_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    sec_tag = plain[ppos++];

    memcpy(&sec_len, plain + ppos, 4);
    ppos += 4;

    if (
        sec_tag != 0x02 ||
        (size_t)sec_len > plain_len - ppos
    ) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    size_t names_end = ppos + sec_len;

    if (ppos + 4 > names_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    uint32_t num_names;
    memcpy(&num_names, plain + ppos, 4);
    ppos += 4;

    if (num_names > 65535) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    vm->num_names = num_names;

    vm->names = calloc(
        num_names,
        sizeof(char *)
    );

    if (!vm->names && num_names > 0) {
        err = VM_ERR_OOM;
        goto fail;
    }

    for (uint32_t i = 0; i < num_names; i++) {
        if (ppos + 1 > names_end) {
            err = VM_ERR_BOUNDS;
            goto fail;
        }

        uint8_t name_len = plain[ppos++];

        if ((size_t)name_len > names_end - ppos) {
            err = VM_ERR_BOUNDS;
            goto fail;
        }

        char *name = malloc((size_t)name_len + 1);

        if (!name) {
            err = VM_ERR_OOM;
            goto fail;
        }

        memcpy(name, plain + ppos, name_len);
        name[name_len] = '\0';
        ppos += name_len;

        vm->names[i] = name;
    }

    if (ppos != names_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    if (ppos + 5 > plain_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    sec_tag = plain[ppos++];

    memcpy(&sec_len, plain + ppos, 4);
    ppos += 4;

    if (
        sec_tag != 0x03 ||
        (size_t)sec_len > plain_len - ppos
    ) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    size_t funcs_end = ppos + sec_len;

    if (ppos + 4 > funcs_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    uint32_t num_functions;
    memcpy(&num_functions, plain + ppos, 4);
    ppos += 4;

    if (
        num_functions == 0 ||
        num_functions > 4096
    ) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    vm->num_functions = num_functions;

    vm->functions = calloc(
        num_functions,
        sizeof(FuncEntry)
    );

    if (!vm->functions) {
        err = VM_ERR_OOM;
        goto fail;
    }

    size_t total_code = 0;
    size_t temp_pos = ppos;
    FunctionRecord rec;

    for (uint32_t i = 0; i < num_functions; i++) {
        err = parse_function_record(vm, plain, &temp_pos, funcs_end, &rec);

        if (err != VM_ERR_OK) {
            goto fail;
        }

        if ((size_t)rec.code_len > SIZE_MAX - total_code) {
            err = VM_ERR_BOUNDS;
            goto fail;
        }

        total_code += rec.code_len;
    }

    if (temp_pos != funcs_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    vm->bytecode_len = total_code;

    vm->bytecode = malloc(total_code);

    if (!vm->bytecode && total_code > 0) {
        err = VM_ERR_OOM;
        goto fail;
    }

    temp_pos = ppos;
    uint32_t code_offset = 0;

    for (uint32_t i = 0; i < num_functions; i++) {
        err = parse_function_record(vm, plain, &temp_pos, funcs_end, &rec);

        if (err != VM_ERR_OK) {
            goto fail;
        }

        memcpy(
            vm->bytecode + code_offset,
            plain + rec.code_pos,
            rec.code_len
        );

        FuncEntry *entry = &vm->functions[i];

        entry->name_index = rec.name_idx;
        entry->code_offset = code_offset;
        entry->locals_count = rec.num_locals;
        entry->params_count = rec.num_params;
        entry->defaults_count = rec.num_defaults;

        if (rec.num_params > 0) {
            entry->param_names = malloc((size_t)rec.num_params * sizeof(uint32_t));

            if (!entry->param_names) {
                err = VM_ERR_OOM;
                goto fail;
            }

            memcpy(
                entry->param_names,
                plain + rec.param_names_pos,
                (size_t)rec.num_params * sizeof(uint32_t)
            );
        }

        if (rec.num_defaults > 0) {
            entry->default_consts = malloc((size_t)rec.num_defaults * sizeof(uint32_t));

            if (!entry->default_consts) {
                err = VM_ERR_OOM;
                goto fail;
            }

            memcpy(
                entry->default_consts,
                plain + rec.defaults_pos,
                (size_t)rec.num_defaults * sizeof(uint32_t)
            );
        }

        code_offset += rec.code_len;
    }

    if (temp_pos != funcs_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    ppos = funcs_end;

    if (ppos != plain_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    if (entry_idx >= num_functions) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    vm->entry_func_index = entry_idx;

    free(plain);
    free(buf);

    vm->last_error = VM_ERR_OK;
    return vm->last_error;

fail:
    free(plain);
    free(buf);
    vm_free(vm);
    vm->last_error = err;
    return err;
}