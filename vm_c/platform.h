#ifndef CVM_PLATFORM_H
#define CVM_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

uint8_t *platform_read_executable(size_t *out_len);

#endif
