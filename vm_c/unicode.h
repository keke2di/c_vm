#ifndef CVM_UNICODE_H
#define CVM_UNICODE_H

#include <stdint.h>
#include <stddef.h>

#define UNI_CASE_MAX 4

int uni_is_alpha(uint32_t cp);
int uni_is_decimal(uint32_t cp);
int uni_is_digit(uint32_t cp);
int uni_is_numeric(uint32_t cp);
int uni_is_printable(uint32_t cp);
int uni_is_lower(uint32_t cp);
int uni_is_upper(uint32_t cp);
int uni_is_title(uint32_t cp);
int uni_is_cased(uint32_t cp);
int uni_is_id_start(uint32_t cp);
int uni_is_id_continue(uint32_t cp);
int uni_decimal_value(uint32_t cp);

size_t uni_to_lower(uint32_t cp, uint32_t *out);
size_t uni_to_upper(uint32_t cp, uint32_t *out);
size_t uni_to_title(uint32_t cp, uint32_t *out);
size_t uni_to_casefold(uint32_t cp, uint32_t *out);

#endif
