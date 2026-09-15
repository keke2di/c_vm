#include "vm_internal.h"

size_t utf8_char_size(const char *s, size_t byte_len, size_t offset) {
    if (offset >= byte_len) return 0;

    unsigned char lead = (unsigned char)s[offset];
    size_t size = 1;

    if (lead >= 0xF0) {
        size = 4;
    } else if (lead >= 0xE0) {
        size = 3;
    } else if (lead >= 0xC0) {
        size = 2;
    }

    if (offset + size > byte_len) {
        size = byte_len - offset;
    }

    return size;
}

size_t utf8_length(const char *s, size_t byte_len) {
    size_t count = 0;

    for (size_t i = 0; i < byte_len; i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {
            count++;
        }
    }

    return count;
}

size_t utf8_offset(const char *s, size_t byte_len, size_t index) {
    size_t offset = 0;

    while (index > 0 && offset < byte_len) {
        offset += utf8_char_size(s, byte_len, offset);
        index--;
    }

    return offset;
}

uint32_t utf8_decode(const char *s, size_t byte_len, size_t *offset) {
    size_t i = *offset;
    size_t size = utf8_char_size(s, byte_len, i);
    const unsigned char *u = (const unsigned char *)s + i;
    uint32_t cp;

    switch (size) {
        case 0:
            return 0;
        case 1:
            cp = u[0];
            break;
        case 2:
            cp = ((uint32_t)(u[0] & 0x1F) << 6) | (uint32_t)(u[1] & 0x3F);
            break;
        case 3:
            cp = ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) |
                 (uint32_t)(u[2] & 0x3F);
            break;
        default:
            cp = ((uint32_t)(u[0] & 0x07) << 18) | ((uint32_t)(u[1] & 0x3F) << 12) |
                 ((uint32_t)(u[2] & 0x3F) << 6) | (uint32_t)(u[3] & 0x3F);
            break;
    }

    *offset = i + size;
    return cp;
}

size_t utf8_encode(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

int ascii_is_space(uint32_t c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0D);
}

int unicode_is_space(uint32_t cp) {
    if (cp < 0x80) {
        return cp == ' ' || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F);
    }
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

static int is_space_in_mode(uint32_t cp, int mode) {
    if (mode == SPACE_ASCII || (mode == SPACE_NUMERIC && cp < 0x80)) {
        return ascii_is_space(cp);
    }
    return unicode_is_space(cp);
}

void text_space_bounds(const char *s, size_t len, int mode, size_t *begin, size_t *end) {
    int decode = mode != SPACE_ASCII;
    size_t b = 0;

    while (b < len) {
        size_t next = b;
        uint32_t cp = decode ? utf8_decode(s, len, &next) : (unsigned char)s[next++];
        if (!is_space_in_mode(cp, mode)) break;
        b = next;
    }

    size_t e = len;

    while (e > b) {
        size_t start = e - 1;
        if (decode) {
            while (start > b && ((unsigned char)s[start] & 0xC0) == 0x80) {
                start--;
            }
        }
        size_t pos = start;
        uint32_t cp = decode ? utf8_decode(s, len, &pos) : (unsigned char)s[start];
        if (!is_space_in_mode(cp, mode)) break;
        e = start;
    }

    *begin = b;
    *end = e;
}
