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
