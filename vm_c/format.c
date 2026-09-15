#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_internal.h"

typedef struct {
    char fill;
    char align;
    char sign;
    int alt;
    int zero;
    int width;
    char grouping;
    int has_precision;
    int precision;
    char type;
} FormatSpec;

static int is_align(char c) {
    return c == '<' || c == '>' || c == '^' || c == '=';
}

static int int_like_tag(const Value *v) {
    return v->tag == TAG_INT || v->tag == TAG_BOOL;
}

static int parse_spec(const char *s, size_t len, FormatSpec *fs) {
    memset(fs, 0, sizeof(*fs));
    fs->fill = ' ';

    size_t i = 0;

    if (len - i >= 2 && is_align(s[i + 1])) {
        fs->fill = s[i];
        fs->align = s[i + 1];
        i += 2;
    } else if (len - i >= 1 && is_align(s[i])) {
        fs->align = s[i];
        i += 1;
    }

    if (i < len && (s[i] == '+' || s[i] == '-' || s[i] == ' ')) {
        fs->sign = s[i++];
    }

    if (i < len && s[i] == '#') {
        fs->alt = 1;
        i++;
    }

    if (i < len && s[i] == '0') {
        fs->zero = 1;
        i++;
        if (!fs->align) {
            fs->align = '=';
            fs->fill = '0';
        }
    }

    while (i < len && s[i] >= '0' && s[i] <= '9') {
        fs->width = fs->width * 10 + (s[i] - '0');
        i++;
    }

    if (i < len && (s[i] == ',' || s[i] == '_')) {
        fs->grouping = s[i++];
    }

    if (i < len && s[i] == '.') {
        i++;
        fs->has_precision = 1;
        fs->precision = 0;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            fs->precision = fs->precision * 10 + (s[i] - '0');
            i++;
        }
    }

    if (i < len) {
        fs->type = s[i++];
    }

    return i == len ? 0 : -1;
}

static char *pad_and_finish(const char *body, size_t body_len, size_t prefix_len,
                            const FormatSpec *fs, char default_align, size_t *out_len) {
    char align = fs->align ? fs->align : default_align;
    int pad = fs->width > (int)body_len ? fs->width - (int)body_len : 0;

    size_t total = body_len + (size_t)pad;
    char *out = malloc(total + 1);
    if (!out) return NULL;

    char fill = fs->fill;

    if (pad == 0) {
        memcpy(out, body, body_len);
    } else if (align == '<') {
        memcpy(out, body, body_len);
        memset(out + body_len, fill, pad);
    } else if (align == '>') {
        memset(out, fill, pad);
        memcpy(out + pad, body, body_len);
    } else if (align == '^') {
        int left = pad / 2;
        int right = pad - left;
        memset(out, fill, left);
        memcpy(out + left, body, body_len);
        memset(out + left + body_len, fill, right);
    } else {
        memcpy(out, body, prefix_len);
        memset(out + prefix_len, fill, pad);
        memcpy(out + prefix_len + pad, body + prefix_len, body_len - prefix_len);
    }

    out[total] = '\0';
    *out_len = total;
    return out;
}

static char *group_decimal(const char *digits, size_t len, char sep, size_t *out_len) {
    size_t groups = (len - 1) / 3;
    size_t total = len + groups;
    char *out = malloc(total + 1);
    if (!out) return NULL;

    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) {
            out[o++] = sep;
        }
        out[o++] = digits[i];
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}

static char *format_string_value(const char *text, size_t text_len,
                                 const FormatSpec *fs, size_t *out_len) {
    size_t effective = text_len;
    if (fs->has_precision && (size_t)fs->precision < effective) {
        effective = (size_t)fs->precision;
    }
    return pad_and_finish(text, effective, 0, fs, '<', out_len);
}

static char *format_integer(int64_t value, const FormatSpec *fs, size_t *out_len, int *err) {
    char type = fs->type ? fs->type : 'd';
    int base = 10;
    int upper = 0;
    const char *prefix = "";

    switch (type) {
        case 'd': base = 10; break;
        case 'b': base = 2; prefix = "0b"; break;
        case 'o': base = 8; prefix = "0o"; break;
        case 'x': base = 16; prefix = "0x"; break;
        case 'X': base = 16; upper = 1; prefix = "0X"; break;
        default: *err = 1; return NULL;
    }

    if (fs->grouping && base != 10) {
        *err = 1;
        return NULL;
    }

    uint64_t magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;

    char digits[72];
    size_t dlen = 0;
    if (magnitude == 0) {
        digits[dlen++] = '0';
    } else {
        while (magnitude > 0) {
            int d = (int)(magnitude % (uint64_t)base);
            char c = d < 10 ? (char)('0' + d) : (char)((upper ? 'A' : 'a') + d - 10);
            digits[dlen++] = c;
            magnitude /= (uint64_t)base;
        }
    }
    for (size_t i = 0; i < dlen / 2; i++) {
        char t = digits[i];
        digits[i] = digits[dlen - 1 - i];
        digits[dlen - 1 - i] = t;
    }

    char *grouped = NULL;
    const char *body_digits = digits;
    size_t body_digits_len = dlen;
    if (fs->grouping && base == 10) {
        size_t glen = 0;
        grouped = group_decimal(digits, dlen, fs->grouping, &glen);
        if (!grouped) { *err = 1; return NULL; }
        body_digits = grouped;
        body_digits_len = glen;
    }

    char sign_char = 0;
    if (value < 0) {
        sign_char = '-';
    } else if (fs->sign == '+') {
        sign_char = '+';
    } else if (fs->sign == ' ') {
        sign_char = ' ';
    }

    size_t prefix_len = fs->alt ? strlen(prefix) : 0;
    size_t sign_len = sign_char ? 1 : 0;
    size_t total = sign_len + prefix_len + body_digits_len;

    char *body = malloc(total + 1);
    if (!body) { free(grouped); *err = 1; return NULL; }

    size_t o = 0;
    if (sign_char) body[o++] = sign_char;
    if (prefix_len) { memcpy(body + o, prefix, prefix_len); o += prefix_len; }
    memcpy(body + o, body_digits, body_digits_len);
    o += body_digits_len;
    body[o] = '\0';

    free(grouped);

    char *out = pad_and_finish(body, o, sign_len + prefix_len, fs, '>', out_len);
    free(body);
    if (!out) { *err = 1; return NULL; }
    return out;
}

static char *format_float(double value, const FormatSpec *fs, size_t *out_len, int *err) {
    char type = fs->type;
    int precision = fs->has_precision ? fs->precision : 6;
    double scaled = value;

    char conv[512];
    if (type == 'f' || type == 'F') {
        snprintf(conv, sizeof(conv), "%.*f", precision, value);
    } else if (type == 'e' || type == 'E') {
        snprintf(conv, sizeof(conv), type == 'e' ? "%.*e" : "%.*E", precision, value);
    } else if (type == 'g' || type == 'G') {
        int p = precision == 0 ? 1 : precision;
        snprintf(conv, sizeof(conv), type == 'g' ? "%.*g" : "%.*G", p, value);
    } else if (type == '%') {
        scaled = value * 100.0;
        snprintf(conv, sizeof(conv), "%.*f%%", precision, scaled);
    } else if (type == 0) {
        if (fs->has_precision) {
            int p = precision == 0 ? 1 : precision;
            snprintf(conv, sizeof(conv), "%.*g", p, value);
        } else {
            Value tmp;
            tmp.tag = TAG_FLOAT;
            tmp.refcount = 1;
            tmp.data.float_val = value;
            char *shortest = value_to_string(&tmp);
            if (!shortest) { *err = 1; return NULL; }
            snprintf(conv, sizeof(conv), "%s", shortest);
            free(shortest);
        }
    } else {
        *err = 1;
        return NULL;
    }

    const char *magnitude = conv;
    char sign_char = 0;
    if (conv[0] == '-') {
        sign_char = '-';
        magnitude = conv + 1;
    } else if (fs->sign == '+') {
        sign_char = '+';
    } else if (fs->sign == ' ') {
        sign_char = ' ';
    }

    size_t mag_len = strlen(magnitude);
    size_t sign_len = sign_char ? 1 : 0;
    size_t total = sign_len + mag_len;

    char *body = malloc(total + 1);
    if (!body) { *err = 1; return NULL; }
    size_t o = 0;
    if (sign_char) body[o++] = sign_char;
    memcpy(body + o, magnitude, mag_len);
    o += mag_len;
    body[o] = '\0';

    char *out = pad_and_finish(body, o, sign_len, fs, '>', out_len);
    free(body);
    if (!out) { *err = 1; return NULL; }
    return out;
}

char *format_value(const Value *v, const char *spec, size_t spec_len, int conv,
                   size_t *out_len, int *err) {
    *err = 0;

    char *converted = NULL;
    const Value *target = v;
    Value conv_holder;

    if (conv == 1) {
        converted = value_to_string(v);
    } else if (conv == 2) {
        converted = value_to_repr(v);
    } else if (conv == 3) {
        converted = value_to_ascii(v);
    }

    if (conv != 0) {
        if (!converted) { *err = 1; return NULL; }
        conv_holder.tag = TAG_STRING;
        conv_holder.refcount = 1;
        conv_holder.data.str.data = converted;
        conv_holder.data.str.len = (uint32_t)strlen(converted);
        target = &conv_holder;
    }

    FormatSpec fs;
    if (parse_spec(spec, spec_len, &fs) != 0) {
        free(converted);
        *err = 1;
        return NULL;
    }

    char *result = NULL;

    if (spec_len == 0 && conv == 0) {
        size_t len = 0;
        result = value_to_string_sized(v, &len);
        if (result) *out_len = len;
        else *err = 1;
        return result;
    }

    if (fs.type == 's' || target->tag == TAG_STRING) {
        if (fs.type && fs.type != 's') {
            *err = 1;
        } else if (target->tag != TAG_STRING) {
            *err = 1;
        } else {
            result = format_string_value(target->data.str.data, target->data.str.len, &fs, out_len);
            if (!result) *err = 1;
        }
    } else if (fs.type == 'f' || fs.type == 'F' || fs.type == 'e' || fs.type == 'E' ||
               fs.type == 'g' || fs.type == 'G' || fs.type == '%') {
        double d = target->tag == TAG_FLOAT ? target->data.float_val
                 : int_like_tag(target) ? (double)target->data.int_val : 0.0;
        if (target->tag != TAG_FLOAT && !int_like_tag(target)) {
            *err = 1;
        } else {
            result = format_float(d, &fs, out_len, err);
        }
    } else if (target->tag == TAG_FLOAT) {
        result = format_float(target->data.float_val, &fs, out_len, err);
    } else if (int_like_tag(target)) {
        result = format_integer(target->data.int_val, &fs, out_len, err);
    } else {
        *err = 1;
    }

    free(converted);
    if (*err) {
        free(result);
        return NULL;
    }
    return result;
}

void op_format_value(VM *vm, uint32_t conv) {
    Value *spec = vm_pop(vm);
    Value *value = vm_pop(vm);

    if (!spec || !value) {
        if (spec) value_release(spec);
        if (value) value_release(value);
        vm->last_error = VM_ERR_STACK;
        return;
    }

    if (spec->tag != TAG_STRING) {
        value_release(spec);
        value_release(value);
        vm->last_error = VM_ERR_TYPE;
        return;
    }

    size_t out_len = 0;
    int err = 0;
    char *s = format_value(value, spec->data.str.data, spec->data.str.len,
                           (int)conv, &out_len, &err);
    value_release(spec);
    value_release(value);

    if (err || !s) {
        free(s);
        vm->last_error = VM_ERR_VALUE;
        return;
    }

    Value *result = value_new_string_len(s, out_len);
    free(s);
    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }
    vm_push_owned(vm, result);
}

void op_build_string(VM *vm, uint32_t n) {
    if (n > vm->stack_top) {
        vm->last_error = VM_ERR_STACK;
        return;
    }

    uint32_t start = vm->stack_top - n;
    size_t total = 0;
    for (uint32_t i = 0; i < n; i++) {
        Value *s = vm->stack[start + i];
        if (!s || s->tag != TAG_STRING) {
            vm->last_error = VM_ERR_TYPE;
            return;
        }
        total += s->data.str.len;
    }

    char *buf = malloc(total + 1);
    if (!buf) {
        vm->last_error = VM_ERR_OOM;
        return;
    }

    size_t o = 0;
    for (uint32_t i = 0; i < n; i++) {
        Value *s = vm->stack[start + i];
        memcpy(buf + o, s->data.str.data, s->data.str.len);
        o += s->data.str.len;
    }
    buf[total] = '\0';

    Value *result = value_new_string_len(buf, total);
    free(buf);

    for (uint32_t i = 0; i < n; i++) {
        Value *s = vm_pop(vm);
        if (s) value_release(s);
    }

    if (!result) {
        vm->last_error = VM_ERR_OOM;
        return;
    }
    vm_push_owned(vm, result);
}
