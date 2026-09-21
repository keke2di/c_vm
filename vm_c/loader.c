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
        case 0x00:
            *out = value_new_none();
            return VM_ERR_OK;

        case 0x01:
            *out = value_false();
            return VM_ERR_OK;

        case 0x02:
            *out = value_true();
            return VM_ERR_OK;

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
    uint16_t posonly_count;
    uint16_t num_defaults;
    uint16_t num_kwonly;
    uint16_t vararg_slot;
    uint16_t kwarg_slot;
    uint16_t num_cells;
    uint16_t num_free;
    size_t param_names_pos;
    size_t kwonly_names_pos;
    size_t kwonly_slots_pos;
    size_t kwonly_flags_pos;
    size_t cell_slots_pos;
    size_t free_names_pos;
    size_t lines_pos;
    uint32_t line_count;
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

    if (p > end || end - p < 4 + 2 * 10) {
        return VM_ERR_BOUNDS;
    }

    memcpy(&rec->name_idx, data + p, 4);
    p += 4;

    memcpy(&rec->num_locals, data + p, 2);
    p += 2;

    memcpy(&rec->num_params, data + p, 2);
    p += 2;

    memcpy(&rec->posonly_count, data + p, 2);
    p += 2;

    memcpy(&rec->num_defaults, data + p, 2);
    p += 2;

    memcpy(&rec->num_kwonly, data + p, 2);
    p += 2;

    memcpy(&rec->vararg_slot, data + p, 2);
    p += 2;

    memcpy(&rec->kwarg_slot, data + p, 2);
    p += 2;

    memcpy(&rec->num_cells, data + p, 2);
    p += 2;

    memcpy(&rec->num_free, data + p, 2);
    p += 2;

    if (
        rec->name_idx >= vm->num_names ||
        rec->num_params > rec->num_locals ||
        rec->num_defaults > rec->num_params ||
        rec->posonly_count > rec->num_params ||
        (rec->vararg_slot != UINT16_MAX && rec->vararg_slot >= rec->num_locals) ||
        (rec->kwarg_slot != UINT16_MAX && rec->kwarg_slot >= rec->num_locals)
    ) {
        return VM_ERR_BOUNDS;
    }

    size_t tables_len =
        (size_t)rec->num_params * 4 +
        (size_t)rec->num_kwonly * 4 +
        (size_t)rec->num_kwonly * 2 +
        (size_t)rec->num_kwonly +
        (size_t)rec->num_cells * 2 +
        (size_t)rec->num_free * 4;

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

    rec->kwonly_names_pos = p;

    for (uint32_t i = 0; i < rec->num_kwonly; i++) {
        uint32_t name_idx;
        memcpy(&name_idx, data + p, 4);
        p += 4;

        if (name_idx >= vm->num_names) {
            return VM_ERR_BOUNDS;
        }
    }

    rec->kwonly_slots_pos = p;

    for (uint32_t i = 0; i < rec->num_kwonly; i++) {
        uint16_t slot;
        memcpy(&slot, data + p, 2);
        p += 2;

        if (slot >= rec->num_locals) {
            return VM_ERR_BOUNDS;
        }
    }

    rec->kwonly_flags_pos = p;
    p += rec->num_kwonly;

    rec->cell_slots_pos = p;

    for (uint32_t i = 0; i < rec->num_cells; i++) {
        uint16_t slot;
        memcpy(&slot, data + p, 2);
        p += 2;

        if (slot >= rec->num_locals) {
            return VM_ERR_BOUNDS;
        }
    }

    rec->free_names_pos = p;

    for (uint32_t i = 0; i < rec->num_free; i++) {
        uint32_t name_idx;
        memcpy(&name_idx, data + p, 4);
        p += 4;

        if (name_idx >= vm->num_names) {
            return VM_ERR_BOUNDS;
        }
    }

    if (end - p < 4) {
        return VM_ERR_BOUNDS;
    }

    memcpy(&rec->line_count, data + p, 4);
    p += 4;

    if (rec->line_count > 1u << 20) {
        return VM_ERR_BOUNDS;
    }

    if ((size_t)rec->line_count * 8 > end - p) {
        return VM_ERR_BOUNDS;
    }

    rec->lines_pos = p;
    p += (size_t)rec->line_count * 8;

    memcpy(&rec->code_len, data + p, 4);
    p += 4;

    if ((size_t)rec->code_len > end - p) {
        return VM_ERR_BOUNDS;
    }

    rec->code_pos = p;
    *pos = p + rec->code_len;

    return VM_ERR_OK;
}

static int load_buffer(VM *vm, uint8_t *buf, size_t fsize) {
    uint8_t *plain = NULL;
    int err = VM_ERR_OK;

    if (
        fsize <
        4 + 1 + 1 + 2 + 4 + 4 + 4 + 4 + 16 + 12
    ) {
        free(buf);
        vm->last_error = VM_ERR_BOUNDS;
        return vm->last_error;
    }

    size_t pos = 0;

    if (memcmp(buf + pos, "CVM2", 4) != 0) {
        free(buf);
        vm->last_error = VM_ERR_BAD_MAGIC;
        return vm->last_error;
    }

    pos += 4;

    uint8_t version = buf[pos++];

    if (version != 5) {
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
        entry->posonly_count = rec.posonly_count;
        entry->defaults_count = rec.num_defaults;
        entry->kwonly_count = rec.num_kwonly;
        entry->vararg_slot = rec.vararg_slot == UINT16_MAX ? UINT32_MAX : rec.vararg_slot;
        entry->kwarg_slot = rec.kwarg_slot == UINT16_MAX ? UINT32_MAX : rec.kwarg_slot;
        entry->cells_count = rec.num_cells;
        entry->frees_count = rec.num_free;

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

        if (rec.num_kwonly > 0) {
            entry->kwonly_names = malloc((size_t)rec.num_kwonly * sizeof(uint32_t));
            entry->kwonly_slots = malloc((size_t)rec.num_kwonly * sizeof(uint16_t));
            entry->kwonly_flags = malloc((size_t)rec.num_kwonly * sizeof(uint8_t));

            if (!entry->kwonly_names || !entry->kwonly_slots || !entry->kwonly_flags) {
                err = VM_ERR_OOM;
                goto fail;
            }

            memcpy(
                entry->kwonly_names,
                plain + rec.kwonly_names_pos,
                (size_t)rec.num_kwonly * sizeof(uint32_t)
            );

            memcpy(
                entry->kwonly_slots,
                plain + rec.kwonly_slots_pos,
                (size_t)rec.num_kwonly * sizeof(uint16_t)
            );

            memcpy(
                entry->kwonly_flags,
                plain + rec.kwonly_flags_pos,
                (size_t)rec.num_kwonly * sizeof(uint8_t)
            );
        }

        if (rec.num_cells > 0) {
            entry->cell_slots = malloc((size_t)rec.num_cells * sizeof(uint16_t));

            if (!entry->cell_slots) {
                err = VM_ERR_OOM;
                goto fail;
            }

            memcpy(
                entry->cell_slots,
                plain + rec.cell_slots_pos,
                (size_t)rec.num_cells * sizeof(uint16_t)
            );
        }

        if (rec.num_free > 0) {
            entry->free_names = malloc((size_t)rec.num_free * sizeof(uint32_t));

            if (!entry->free_names) {
                err = VM_ERR_OOM;
                goto fail;
            }

            memcpy(
                entry->free_names,
                plain + rec.free_names_pos,
                (size_t)rec.num_free * sizeof(uint32_t)
            );
        }

        if (rec.line_count > 0) {
            entry->line_offsets = malloc((size_t)rec.line_count * sizeof(uint32_t));
            entry->line_numbers = malloc((size_t)rec.line_count * sizeof(uint32_t));

            if (!entry->line_offsets || !entry->line_numbers) {
                err = VM_ERR_OOM;
                goto fail;
            }

            for (uint32_t l = 0; l < rec.line_count; l++) {
                memcpy(
                    &entry->line_offsets[l],
                    plain + rec.lines_pos + (size_t)l * 8,
                    sizeof(uint32_t)
                );
                memcpy(
                    &entry->line_numbers[l],
                    plain + rec.lines_pos + (size_t)l * 8 + 4,
                    sizeof(uint32_t)
                );
            }

            entry->line_count = rec.line_count;
        }

        code_offset += rec.code_len;
    }

    if (temp_pos != funcs_end) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    ppos = funcs_end;

    if (ppos == plain_len) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    uint8_t source_tag = plain[ppos];
    uint32_t source_len;

    if (plain_len - ppos < 5) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    memcpy(&source_len, plain + ppos + 1, 4);

    if (source_tag != 0x04 || source_len > plain_len - ppos - 5) {
        err = VM_ERR_BOUNDS;
        goto fail;
    }

    vm->source_name = malloc((size_t)source_len + 1);

    if (!vm->source_name) {
        err = VM_ERR_OOM;
        goto fail;
    }

    memcpy(vm->source_name, plain + ppos + 5, source_len);
    vm->source_name[source_len] = '\0';

    ppos += 5 + source_len;

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

int vm_load_memory(VM *vm, const uint8_t *data, size_t len) {
    if (!vm || !data) return VM_ERR_LOAD;

    memset(vm, 0, sizeof(VM));

    uint8_t *buf = malloc(len ? len : 1);

    if (!buf) {
        vm->last_error = VM_ERR_OOM;
        return vm->last_error;
    }

    memcpy(buf, data, len);

    return load_buffer(vm, buf, len);
}
