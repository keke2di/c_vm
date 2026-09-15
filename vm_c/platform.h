#ifndef CVM_PLATFORM_H
#define CVM_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

uint8_t *platform_read_executable(size_t *out_len);
void platform_write_stdout(const char *data, size_t len);

#endif
