#include <math.h>
#include <stdint.h>
#include <string.h>
#include "vm_internal.h"

#define HASH_MODULUS 2305843009213693951ull
#define HASH_XXPRIME_1 11400714785074694791ull
#define HASH_XXPRIME_2 14029467366897019727ull
#define HASH_XXPRIME_5 2870177450012600261ull
#define HASH_INF 314159ll
#define HASH_NONE 4238894112ll
#define HASH_EMPTY_RANGE 2676694398852732306ll
#define HASH_SET_ZERO_LANE 0x345678ull
#define HASH_TUPLE_NONE 1546275796ll
#define HASH_SET_NONE 590923713ll

static uint64_t rot_left(uint64_t x, unsigned bits) {
    return (x << bits) | (x >> (64 - bits));
}

static uint64_t shuffle(uint64_t x) {
    return rot_left(x, 31);
}

static int64_t from_unsigned(uint64_t x) {
    int64_t v = (int64_t)x;
    if (v == -1) v = -2;
    return v;
}

static int64_t hash_int_value(int64_t value) {
    uint64_t magnitude = (uint64_t)value;
    if (value < 0) {
        magnitude = (uint64_t)0 - (uint64_t)value;
    }

    int64_t h = (int64_t)(magnitude % HASH_MODULUS);
    if (value < 0) {
        h = -h;
    }
    if (h == -1) {
        h = -2;
    }
    return h;
}

static int64_t hash_pointer(const void *p) {
    uint64_t x = (uint64_t)(uintptr_t)p;
    return from_unsigned((x >> 4) | (x << 60));
}

static int64_t hash_double_value(double value) {
    if (value == 0.0) return 0;
    if (isinf(value)) return value > 0 ? HASH_INF : -HASH_INF;

    int exponent = 0;
    double mantissa = frexp(value, &exponent);
    int sign = 1;

    if (mantissa < 0) {
        sign = -1;
        mantissa = -mantissa;
    }

    uint64_t x = 0;

    while (mantissa != 0.0) {
        x = ((x << 28) & HASH_MODULUS) | (x >> (61 - 28));
        mantissa *= 268435456.0;
        exponent -= 28;

        double chunk = floor(mantissa);
        mantissa -= chunk;
        x += (uint64_t)chunk;

        if (x >= HASH_MODULUS) x -= HASH_MODULUS;
    }

    if (exponent >= 0) {
        exponent %= 61;
    } else {
        exponent = 60 - ((-exponent - 1) % 61);
    }

    x = ((x << exponent) & HASH_MODULUS) | (x >> (61 - exponent));

    int64_t h = (int64_t)x * sign;
    if (h == -1) {
        h = -2;
    }
    return h;
}

typedef struct {
    uint64_t v0;
    uint64_t v1;
    uint64_t v2;
    uint64_t v3;
    uint64_t tail;
    uint64_t total;
    unsigned pending;
} SipState;

static void sip_round(SipState *s) {
    s->v0 += s->v1;
    s->v1 = rot_left(s->v1, 13);
    s->v1 ^= s->v0;
    s->v0 = rot_left(s->v0, 32);
    s->v2 += s->v3;
    s->v3 = rot_left(s->v3, 16);
    s->v3 ^= s->v2;
    s->v0 += s->v3;
    s->v3 = rot_left(s->v3, 21);
    s->v3 ^= s->v0;
    s->v2 += s->v1;
    s->v1 = rot_left(s->v1, 17);
    s->v1 ^= s->v2;
    s->v2 = rot_left(s->v2, 32);
}

static void sip_init(SipState *s) {
    s->v0 = 0x736f6d6570736575ull;
    s->v1 = 0x646f72616e646f6dull;
    s->v2 = 0x6c7967656e657261ull;
    s->v3 = 0x7465646279746573ull;
    s->tail = 0;
    s->total = 0;
    s->pending = 0;
}

static void sip_block(SipState *s, uint64_t block) {
    s->v3 ^= block;
    sip_round(s);
    s->v0 ^= block;
}

static void sip_update(SipState *s, const unsigned char *data, size_t len) {
    s->total += len;

    for (size_t i = 0; i < len; i++) {
        s->tail |= (uint64_t)data[i] << (8 * s->pending);
        s->pending++;

        if (s->pending == 8) {
            sip_block(s, s->tail);
            s->tail = 0;
            s->pending = 0;
        }
    }
}

static uint64_t sip_finish(SipState *s) {
    uint64_t last = s->tail | ((s->total & 0xff) << 56);

    sip_block(s, last);
    s->v2 ^= 0xff;

    sip_round(s);
    sip_round(s);
    sip_round(s);

    return s->v0 ^ s->v1 ^ s->v2 ^ s->v3;
}

static int64_t hash_text(const char *data, size_t len) {
    if (len == 0) return 0;

    uint32_t widest = 0;
    size_t offset = 0;

    while (offset < len) {
        size_t before = offset;
        uint32_t cp = utf8_decode(data, len, &offset);
        if (offset == before) break;
        if (cp > widest) widest = cp;
    }

    unsigned width = widest < 0x100 ? 1 : (widest < 0x10000 ? 2 : 4);
    SipState state;
    sip_init(&state);
    offset = 0;

    while (offset < len) {
        size_t before = offset;
        uint32_t cp = utf8_decode(data, len, &offset);
        if (offset == before) break;

        unsigned char bytes[4];
        for (unsigned i = 0; i < width; i++) {
            bytes[i] = (unsigned char)((cp >> (8 * i)) & 0xff);
        }
        sip_update(&state, bytes, width);
    }

    return from_unsigned(sip_finish(&state));
}

static int64_t hash_bytes(const unsigned char *data, size_t len) {
    if (len == 0) return 0;

    SipState state;
    sip_init(&state);
    sip_update(&state, data, len);
    return from_unsigned(sip_finish(&state));
}

static uint64_t tuple_mix(uint64_t acc, int64_t lane) {
    acc += (uint64_t)lane * HASH_XXPRIME_2;
    acc = shuffle(acc);
    return acc * HASH_XXPRIME_1;
}

static int64_t tuple_finish(uint64_t acc, uint64_t count) {
    acc += count ^ (HASH_XXPRIME_5 ^ 3527539ull);
    if (acc == UINT64_MAX) return HASH_TUPLE_NONE;
    return from_unsigned(acc);
}

static int64_t hash_tuple(const Value *v, int *err) {
    uint64_t acc = HASH_XXPRIME_5;

    for (uint32_t i = 0; i < v->data.tuple.len; i++) {
        int item_error = 0;
        int64_t lane = value_hash(v->data.tuple.items[i], &item_error);
        if (item_error) {
            *err = 1;
            return 0;
        }
        acc = tuple_mix(acc, lane);
    }

    return tuple_finish(acc, v->data.tuple.len);
}

static int64_t hash_setlike(const Value *v, int *err) {
    uint64_t acc = 0;

    for (uint32_t i = 0; i < v->data.set.len; i++) {
        int item_error = 0;
        int64_t lane = value_hash(v->data.set.items[i], &item_error);
        if (item_error) {
            *err = 1;
            return 0;
        }
        uint64_t bits = (uint64_t)lane;
        if (bits == 0) bits = HASH_SET_ZERO_LANE;
        acc ^= shuffle(bits);
    }

    acc ^= (uint64_t)(v->data.set.len + 1);
    acc *= HASH_XXPRIME_5;

    if (acc == UINT64_MAX) return HASH_SET_NONE;
    return from_unsigned(acc);
}

static int64_t hash_range(const Value *v) {
    int64_t count = value_range_len(
        v->data.range.start,
        v->data.range.stop,
        v->data.range.step);

    if (count == 0) return HASH_EMPTY_RANGE;

    uint64_t acc = HASH_XXPRIME_5;
    acc = tuple_mix(acc, hash_int_value(count));
    acc = tuple_mix(acc, hash_int_value(v->data.range.start));
    acc = tuple_mix(acc, hash_int_value(v->data.range.step));
    return tuple_finish(acc, 3);
}

int64_t value_hash(const Value *v, int *err) {
    if (err) *err = 0;
    if (!v) return 0;

    switch (v->tag) {
        case TAG_NONE:
            return HASH_NONE;

        case TAG_BOOL:
        case TAG_INT:
            return hash_int_value(v->data.int_val);

        case TAG_FLOAT:
            if (isnan(v->data.float_val)) return hash_pointer(v);
            return hash_double_value(v->data.float_val);

        case TAG_STRING:
            return hash_text(v->data.str.data, v->data.str.len);

        case TAG_BYTES:
            return hash_bytes(v->data.bytes.data, v->data.bytes.len);

        case TAG_TUPLE:
            return hash_tuple(v, err);

        case TAG_FROZENSET:
            return hash_setlike(v, err);

        case TAG_RANGE:
            return hash_range(v);

        case TAG_LIST:
        case TAG_DICT:
        case TAG_SET:
        case TAG_DICT_VIEW:
            *err = 1;
            return 0;

        default:
            return hash_pointer(v);
    }
}
