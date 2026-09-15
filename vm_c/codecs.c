#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

#define NAME_CAP 64
#define COUNT(table) (sizeof(table) / sizeof(table[0]))

typedef enum {
    CODEC_UTF8,
    CODEC_UTF8_SIG,
    CODEC_ASCII,
    CODEC_LATIN1,
} CodecId;

typedef enum {
    ERRORS_STRICT,
    ERRORS_IGNORE,
    ERRORS_REPLACE,
    ERRORS_BACKSLASH,
    ERRORS_XMLCHAR,
    ERRORS_UNSUPPORTED,
    ERRORS_UNKNOWN,
} ErrorMode;

typedef struct {
    const char *name;
    CodecId codec;
} CodecName;

typedef struct {
    const char *name;
    ErrorMode mode;
} ErrorName;

static const CodecName CODEC_MODULES[] = {
    {"utf_8", CODEC_UTF8},
    {"utf_8_sig", CODEC_UTF8_SIG},
    {"latin_1", CODEC_LATIN1},
    {"ascii", CODEC_ASCII},
};

static const CodecName CODEC_ALIASES[] = {
    {"cp65001", CODEC_UTF8},
    {"u8", CODEC_UTF8},
    {"utf", CODEC_UTF8},
    {"utf8", CODEC_UTF8},
    {"utf8_ucs2", CODEC_UTF8},
    {"utf8_ucs4", CODEC_UTF8},
    {"8859", CODEC_LATIN1},
    {"cp819", CODEC_LATIN1},
    {"csisolatin1", CODEC_LATIN1},
    {"ibm819", CODEC_LATIN1},
    {"iso8859", CODEC_LATIN1},
    {"iso8859_1", CODEC_LATIN1},
    {"iso_8859_1", CODEC_LATIN1},
    {"iso_8859_1_1987", CODEC_LATIN1},
    {"iso_ir_100", CODEC_LATIN1},
    {"l1", CODEC_LATIN1},
    {"latin", CODEC_LATIN1},
    {"latin1", CODEC_LATIN1},
    {"646", CODEC_ASCII},
    {"ansi_x3.4_1968", CODEC_ASCII},
    {"ansi_x3.4_1986", CODEC_ASCII},
    {"ansi_x3_4_1968", CODEC_ASCII},
    {"cp367", CODEC_ASCII},
    {"csascii", CODEC_ASCII},
    {"ibm367", CODEC_ASCII},
    {"iso646_us", CODEC_ASCII},
    {"iso_646.irv_1991", CODEC_ASCII},
    {"iso_ir_6", CODEC_ASCII},
    {"us", CODEC_ASCII},
    {"us_ascii", CODEC_ASCII},
};

static const ErrorName ERROR_NAMES[] = {
    {"strict", ERRORS_STRICT},
    {"ignore", ERRORS_IGNORE},
    {"replace", ERRORS_REPLACE},
    {"backslashreplace", ERRORS_BACKSLASH},
    {"xmlcharrefreplace", ERRORS_XMLCHAR},
    {"surrogateescape", ERRORS_UNSUPPORTED},
    {"surrogatepass", ERRORS_UNSUPPORTED},
    {"namereplace", ERRORS_UNSUPPORTED},
};

static int find_codec(const CodecName *table, size_t count, const char *name, CodecId *out) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(table[i].name, name) == 0) {
            *out = table[i].codec;
            return 1;
        }
    }
    return 0;
}

static size_t normalize_encoding(const Value *name, char *out) {
    const char *s = name->data.str.data;
    size_t len = name->data.str.len;
    size_t o = 0;
    int punct = 0;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        int keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '.';

        if (!keep) {
            punct = 1;
            continue;
        }

        if (o + 2 >= NAME_CAP) return NAME_CAP;
        if (punct && o > 0) out[o++] = '_';
        out[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        punct = 0;
    }

    out[o] = '\0';
    return o;
}

static int lookup_codec(VM *vm, const Value *name, CodecId *out) {
    *out = CODEC_UTF8;
    if (!name) return 0;

    char norm[NAME_CAP];
    size_t len = normalize_encoding(name, norm);

    if (len < NAME_CAP) {
        if (find_codec(CODEC_ALIASES, COUNT(CODEC_ALIASES), norm, out)) return 0;

        char dotless[NAME_CAP];
        memcpy(dotless, norm, len + 1);
        for (size_t i = 0; i < len; i++) {
            if (dotless[i] == '.') dotless[i] = '_';
        }
        if (find_codec(CODEC_ALIASES, COUNT(CODEC_ALIASES), dotless, out)) return 0;

        if (!memchr(norm, '.', len) && find_codec(CODEC_MODULES, COUNT(CODEC_MODULES), norm, out)) {
            return 0;
        }
    }

    vm->last_error = VM_ERR_LOOKUP;
    return -1;
}

static ErrorMode lookup_errors(const Value *name) {
    if (!name) return ERRORS_STRICT;

    for (size_t i = 0; i < COUNT(ERROR_NAMES); i++) {
        size_t len = strlen(ERROR_NAMES[i].name);
        if (len == name->data.str.len && memcmp(ERROR_NAMES[i].name, name->data.str.data, len) == 0) {
            return ERROR_NAMES[i].mode;
        }
    }

    return ERRORS_UNKNOWN;
}

static int check_codec_args(VM *vm, const Value *encoding, const Value *errors) {
    if ((encoding && encoding->tag != TAG_STRING) || (errors && errors->tag != TAG_STRING)) {
        vm->last_error = VM_ERR_TYPE;
        return -1;
    }
    return 0;
}

static int handler_failure(VM *vm, ErrorMode mode) {
    vm->last_error = mode == ERRORS_UNKNOWN ? VM_ERR_LOOKUP : VM_ERR_UNICODE;
    return -1;
}

static int utf8_valid_length(const unsigned char *d, size_t len, size_t i) {
    unsigned char lead = d[i];
    if (lead < 0x80) return 1;

    int need;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;

    if (lead >= 0xC2 && lead <= 0xDF) {
        need = 1;
    } else if (lead == 0xE0) {
        need = 2;
        lo = 0xA0;
    } else if (lead == 0xED) {
        need = 2;
        hi = 0x9F;
    } else if (lead >= 0xE1 && lead <= 0xEF) {
        need = 2;
    } else if (lead == 0xF0) {
        need = 3;
        lo = 0x90;
    } else if (lead == 0xF4) {
        need = 3;
        hi = 0x8F;
    } else if (lead >= 0xF1 && lead <= 0xF3) {
        need = 3;
    } else {
        return -1;
    }

    for (int k = 1; k <= need; k++) {
        if (i + (size_t)k >= len) return -k;
        unsigned char c = d[i + (size_t)k];
        unsigned char min = k == 1 ? lo : 0x80;
        unsigned char max = k == 1 ? hi : 0xBF;
        if (c < min || c > max) return -k;
    }

    return need + 1;
}

static int decode_error(VM *vm, StrBuf *out, const unsigned char *bad, size_t span, ErrorMode mode) {
    switch (mode) {
        case ERRORS_IGNORE:
            return 0;

        case ERRORS_REPLACE:
            strbuf_append(out, "\xEF\xBF\xBD", 3);
            return 0;

        case ERRORS_BACKSLASH:
            for (size_t i = 0; i < span; i++) {
                char text[8];
                int n = snprintf(text, sizeof(text), "\\x%02x", bad[i]);
                strbuf_append(out, text, (size_t)n);
            }
            return 0;

        case ERRORS_XMLCHAR:
            vm->last_error = VM_ERR_TYPE;
            return -1;

        default:
            return handler_failure(vm, mode);
    }
}

Value *codec_decode_value(VM *vm, const unsigned char *data, size_t len,
                          const Value *encoding, const Value *errors) {
    if (check_codec_args(vm, encoding, errors) != 0) return NULL;
    if (len == 0) return value_new_string_len("", 0);

    CodecId codec;
    if (lookup_codec(vm, encoding, &codec) != 0) return NULL;
    ErrorMode mode = lookup_errors(errors);

    StrBuf out = { NULL, 0, 0, 0 };
    size_t i = 0;

    if (codec == CODEC_UTF8_SIG && len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        i = 3;
    }

    size_t run = i;

    while (i < len) {
        size_t span;

        if (codec == CODEC_LATIN1) {
            if (data[i] < 0x80) {
                i++;
                continue;
            }
            char encoded[4];
            strbuf_append(&out, (const char *)data + run, i - run);
            strbuf_append(&out, encoded, utf8_encode(data[i], encoded));
            i++;
            run = i;
            continue;
        }

        if (codec == CODEC_ASCII) {
            if (data[i] < 0x80) {
                i++;
                continue;
            }
            span = 1;
        } else {
            int n = utf8_valid_length(data, len, i);
            if (n > 0) {
                i += (size_t)n;
                continue;
            }
            span = (size_t)(-n);
        }

        strbuf_append(&out, (const char *)data + run, i - run);
        if (decode_error(vm, &out, data + i, span, mode) != 0) {
            strbuf_free(&out);
            return NULL;
        }
        i += span;
        run = i;
    }

    strbuf_append(&out, (const char *)data + run, len - run);
    return strbuf_finish_string(vm, &out);
}

static int encode_error(VM *vm, StrBuf *out, uint32_t cp, ErrorMode mode) {
    char text[16];
    int n;

    switch (mode) {
        case ERRORS_IGNORE:
            return 0;

        case ERRORS_REPLACE:
            strbuf_append(out, "?", 1);
            return 0;

        case ERRORS_BACKSLASH:
            if (cp < 0x100) {
                n = snprintf(text, sizeof(text), "\\x%02x", (unsigned)cp);
            } else if (cp < 0x10000) {
                n = snprintf(text, sizeof(text), "\\u%04x", (unsigned)cp);
            } else {
                n = snprintf(text, sizeof(text), "\\U%08x", (unsigned)cp);
            }
            strbuf_append(out, text, (size_t)n);
            return 0;

        case ERRORS_XMLCHAR:
            n = snprintf(text, sizeof(text), "&#%u;", (unsigned)cp);
            strbuf_append(out, text, (size_t)n);
            return 0;

        default:
            return handler_failure(vm, mode);
    }
}

Value *codec_encode_value(VM *vm, const Value *text, const Value *encoding, const Value *errors) {
    if (check_codec_args(vm, encoding, errors) != 0) return NULL;

    CodecId codec;
    if (lookup_codec(vm, encoding, &codec) != 0) return NULL;
    ErrorMode mode = lookup_errors(errors);

    const char *s = text->data.str.data;
    size_t len = text->data.str.len;
    uint32_t limit = codec == CODEC_ASCII ? 0x80 : codec == CODEC_LATIN1 ? 0x100 : 0x110000;
    StrBuf out = { NULL, 0, 0, 0 };

    if (codec == CODEC_UTF8_SIG) {
        strbuf_append(&out, "\xEF\xBB\xBF", 3);
    }

    size_t i = 0;
    size_t run = 0;

    while (i < len) {
        size_t start = i;
        uint32_t cp = utf8_decode(s, len, &i);
        int encodable = cp < limit && (cp < 0xD800 || cp > 0xDFFF);

        if (encodable && (cp < 0x80 || codec != CODEC_LATIN1)) continue;

        strbuf_append(&out, s + run, start - run);

        if (encodable) {
            char byte = (char)cp;
            strbuf_append(&out, &byte, 1);
        } else if (encode_error(vm, &out, cp, mode) != 0) {
            strbuf_free(&out);
            return NULL;
        }

        run = i;
    }

    strbuf_append(&out, s + run, len - run);
    return strbuf_finish_bytes(vm, &out);
}
