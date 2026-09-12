#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include "vm.h"

static uint8_t* read_file(const char *path, size_t *out_len) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) { CloseHandle(h); return NULL; }
    if (size.QuadPart > (1LL << 30)) { CloseHandle(h); return NULL; }
    uint8_t *buf = malloc((size_t)size.QuadPart);
    if (!buf) { CloseHandle(h); return NULL; }
    DWORD bytes_read;
    if (!ReadFile(h, buf, (DWORD)size.QuadPart, &bytes_read, NULL) || bytes_read != size.QuadPart) {
        free(buf);
        CloseHandle(h);
        return NULL;
    }
    CloseHandle(h);
    *out_len = (size_t)size.QuadPart;
    return buf;
}

static char* write_temp_file(const uint8_t *data, size_t len) {
    char temp_dir[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, temp_dir)) return NULL;
    char temp_file[MAX_PATH];
    if (!GetTempFileNameA(temp_dir, "CVM", 0, temp_file)) return NULL;
    HANDLE h = CreateFileA(temp_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    DWORD written;
    if (!WriteFile(h, data, (DWORD)len, &written, NULL) || written != len) {
        CloseHandle(h);
        DeleteFileA(temp_file);
        return NULL;
    }
    CloseHandle(h);
    char *path = _strdup(temp_file);
    return path;
}

int main(int argc, char **argv) {
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len == 0 || len >= sizeof(exe_path)) {
        fprintf(stderr, "Failed to get EXE path\n");
        return 1;
    }

    size_t exe_size;
    uint8_t *exe_data = read_file(exe_path, &exe_size);
    if (!exe_data) {
        fprintf(stderr, "Failed to read EXE\n");
        return 1;
    }

    if (exe_size < 16) { free(exe_data); fprintf(stderr, "EXE too small\n"); return 1; }
    const uint8_t *trailer = exe_data + exe_size - 16;
    uint64_t blob_offset;
    uint32_t blob_len;
    char magic[5] = {0};
    memcpy(&blob_offset, trailer, 8);
    memcpy(&blob_len, trailer + 8, 4);
    memcpy(magic, trailer + 12, 4);
    if (strcmp(magic, "CVMT") != 0) {
        free(exe_data);
        fprintf(stderr, "Invalid trailer magic\n");
        return 1;
    }
    if (blob_offset >= exe_size || blob_offset + blob_len > exe_size) {
        free(exe_data);
        fprintf(stderr, "Invalid blob offset/length\n");
        return 1;
    }

    uint8_t *blob = exe_data + blob_offset;
    size_t blob_len_sz = blob_len;

    char *temp_path = write_temp_file(blob, blob_len_sz);
    if (!temp_path) {
        free(exe_data);
        fprintf(stderr, "Failed to write temp file\n");
        return 1;
    }

    VM vm;
    int err = vm_load(&vm, temp_path);
    if (err != VM_ERR_OK) {
        fprintf(stderr, "VM load error: %s\n", vm_error_string(&vm));
        DeleteFileA(temp_path);
        free(temp_path);
        free(exe_data);
        return 1;
    }

    err = vm_run(&vm);
    if (err != VM_ERR_OK) {
        fprintf(stderr, "VM run error: %s\n", vm_error_string(&vm));
        vm_free(&vm);
        DeleteFileA(temp_path);
        free(temp_path);
        free(exe_data);
        return 1;
    }

    if (vm.result) {
        char *s = value_to_string(vm.result);
        if (s) {
            printf("%s\n", s);
            free(s);
        }
    }

    vm_free(&vm);
    DeleteFileA(temp_path);
    free(temp_path);
    free(exe_data);

    return 0;
}