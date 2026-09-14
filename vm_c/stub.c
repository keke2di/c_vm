#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "platform.h"
#include "vm.h"

#define TRAILER_SIZE 16

int main(void) {
    size_t exe_size = 0;
    uint8_t *exe_data = platform_read_executable(&exe_size);
    if (!exe_data) {
        fprintf(stderr, "Failed to read executable\n");
        return 1;
    }

    if (exe_size < TRAILER_SIZE) {
        free(exe_data);
        fprintf(stderr, "Executable too small\n");
        return 1;
    }

    const uint8_t *trailer = exe_data + exe_size - TRAILER_SIZE;
    uint64_t blob_offset;
    uint32_t blob_len;
    memcpy(&blob_offset, trailer, 8);
    memcpy(&blob_len, trailer + 8, 4);

    if (memcmp(trailer + 12, "CVMT", 4) != 0) {
        free(exe_data);
        fprintf(stderr, "Invalid trailer magic\n");
        return 1;
    }

    uint64_t data_end = exe_size - TRAILER_SIZE;
    if (blob_offset > data_end || blob_len > data_end - blob_offset) {
        free(exe_data);
        fprintf(stderr, "Invalid blob offset/length\n");
        return 1;
    }

    VM vm;
    int err = vm_load_memory(&vm, exe_data + blob_offset, (size_t)blob_len);
    free(exe_data);

    if (err != VM_ERR_OK) {
        fprintf(stderr, "VM load error: %s\n", vm_error_string(&vm));
        return 1;
    }

    err = vm_run(&vm);
    if (err != VM_ERR_OK) {
        fprintf(stderr, "VM run error: %s\n", vm_error_string(&vm));
        vm_free(&vm);
        return 1;
    }

    vm_free(&vm);
    return 0;
}
