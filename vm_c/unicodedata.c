#include "unicode.h"
#include "unicode_data.h"

#define CASE_LOWER 0
#define CASE_UPPER 1
#define CASE_TITLE 2
#define CASE_FOLD 3

static const UniRecord *uni_record(uint32_t cp) {
    if (cp >= 0x110000u) return &UNI_RECORDS[0];

    uint32_t mid = UNI_STAGE1[cp >> (UNI_SHIFT2 + UNI_SHIFT3)];
    uint32_t sub = UNI_STAGE2[(mid << UNI_SHIFT2) +
                              ((cp >> UNI_SHIFT3) & ((1u << UNI_SHIFT2) - 1u))];
    uint32_t index = UNI_STAGE3[(sub << UNI_SHIFT3) + (cp & ((1u << UNI_SHIFT3) - 1u))];

    return &UNI_RECORDS[index];
}

static int has_flag(uint32_t cp, uint32_t flag) {
    return (uni_record(cp)->flags & flag) != 0;
}

int uni_is_alpha(uint32_t cp) { return has_flag(cp, UNI_ALPHA); }
int uni_is_decimal(uint32_t cp) { return has_flag(cp, UNI_DECIMAL); }
int uni_is_digit(uint32_t cp) { return has_flag(cp, UNI_DIGIT); }
int uni_is_numeric(uint32_t cp) { return has_flag(cp, UNI_NUMERIC); }
int uni_is_printable(uint32_t cp) { return has_flag(cp, UNI_PRINTABLE); }
int uni_is_lower(uint32_t cp) { return has_flag(cp, UNI_LOWER); }
int uni_is_upper(uint32_t cp) { return has_flag(cp, UNI_UPPER); }
int uni_is_title(uint32_t cp) { return has_flag(cp, UNI_TITLE); }
int uni_is_cased(uint32_t cp) { return has_flag(cp, UNI_CASED); }
int uni_is_id_start(uint32_t cp) { return has_flag(cp, UNI_IDSTART); }
int uni_is_id_continue(uint32_t cp) { return has_flag(cp, UNI_IDCONT); }

int uni_decimal_value(uint32_t cp) {
    const UniRecord *record = uni_record(cp);
    return (record->flags & UNI_DECIMAL) ? (int)record->decimal : -1;
}

static size_t map_case(uint32_t cp, int which, uint32_t *out) {
    const UniRecord *record = uni_record(cp);

    if (record->special) {
        const UniSpecial *entry = &UNI_SPECIALS[record->special];
        size_t count = entry->length[which];
        size_t offset = entry->offset[which];

        for (size_t i = 0; i < count; i++) {
            out[i] = UNI_SPECIAL_CPS[offset + i];
        }

        return count;
    }

    int32_t delta;

    switch (which) {
        case CASE_UPPER: delta = record->upper; break;
        case CASE_TITLE: delta = record->title; break;
        case CASE_FOLD: delta = record->casefold; break;
        default: delta = record->lower; break;
    }

    out[0] = (uint32_t)((int32_t)cp + delta);
    return 1;
}

size_t uni_to_lower(uint32_t cp, uint32_t *out) { return map_case(cp, CASE_LOWER, out); }
size_t uni_to_upper(uint32_t cp, uint32_t *out) { return map_case(cp, CASE_UPPER, out); }
size_t uni_to_title(uint32_t cp, uint32_t *out) { return map_case(cp, CASE_TITLE, out); }
size_t uni_to_casefold(uint32_t cp, uint32_t *out) { return map_case(cp, CASE_FOLD, out); }
