#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "platform.h"

#define PLATFORM_PATH_CAPACITY 32768
#define PLATFORM_MAX_EXECUTABLE_SIZE (1LL << 30)

uint8_t *platform_read_executable(size_t *out_len) {
    static wchar_t path[PLATFORM_PATH_CAPACITY];

    DWORD path_len = GetModuleFileNameW(NULL, path, PLATFORM_PATH_CAPACITY);
    if (path_len == 0 || path_len >= PLATFORM_PATH_CAPACITY) {
        return NULL;
    }

    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) ||
        size.QuadPart <= 0 ||
        size.QuadPart > PLATFORM_MAX_EXECUTABLE_SIZE) {
        CloseHandle(file);
        return NULL;
    }

    uint8_t *data = malloc((size_t)size.QuadPart);
    if (!data) {
        CloseHandle(file);
        return NULL;
    }

    DWORD read_len = 0;
    if (!ReadFile(file, data, (DWORD)size.QuadPart, &read_len, NULL) ||
        read_len != (DWORD)size.QuadPart) {
        free(data);
        CloseHandle(file);
        return NULL;
    }

    CloseHandle(file);
    *out_len = (size_t)size.QuadPart;
    return data;
}

static HANDLE platform_stdout(int *is_console) {
    static HANDLE handle = NULL;
    static int console = 0;

    if (!handle) {
        DWORD mode = 0;
        handle = GetStdHandle(STD_OUTPUT_HANDLE);
        console = handle != NULL &&
                  handle != INVALID_HANDLE_VALUE &&
                  GetConsoleMode(handle, &mode) != 0;
    }

    *is_console = console;
    return handle;
}

void platform_write_stdout(const char *data, size_t len) {
    if (len == 0) return;

    int is_console = 0;
    HANDLE handle = platform_stdout(&is_console);

    if (!is_console) {
        fwrite(data, 1, len, stdout);
        return;
    }

    if (len > (size_t)INT_MAX) return;

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, data, (int)len, NULL, 0);
    if (wide_len <= 0) {
        fwrite(data, 1, len, stdout);
        return;
    }

    wchar_t *wide = malloc((size_t)wide_len * sizeof(wchar_t));
    if (!wide) return;

    if (MultiByteToWideChar(CP_UTF8, 0, data, (int)len, wide, wide_len) == wide_len) {
        DWORD written = 0;
        WriteConsoleW(handle, wide, (DWORD)wide_len, &written, NULL);
    }

    free(wide);
}
