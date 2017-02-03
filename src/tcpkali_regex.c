#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <pcg_basic.h>

#include "tcpkali_common.h"
#include "tcpkali_regex.h"

struct tregex {
#define TREGEX_ELEMENTS 16
    enum {
        TRegexChars,
        TRegexClass,
        TRegexSequence,
        TRegexAlternative,
        TRegexRepeat
    } kind;
    struct {
        size_t bits;
    } randomness;
    size_t min_size;
    size_t max_size;
    union {
        struct {
            char *data;
            size_t size;
        } chars;
        struct {
            unsigned char size;
            unsigned char table[256];
        } oneof;
        struct {
            tregex *piece[TREGEX_ELEMENTS];
            unsigned pieces;
        } sequence;
        struct {
            tregex *branch[TREGEX_ELEMENTS];
            unsigned branches;
        } alternative;
        struct {
            tregex *what;
            unsigned minimum;
            unsigned range;
        } repeat;
    };
};

static UNUSED void
tregex_debug_print(tregex *re) {
    switch(re->kind) {
    case TRegexChars:
        fwrite(re->chars.data, 1, re->chars.size, stdout);
        break;
    case TRegexClass:
        printf("[");
        fwrite(re->oneof.table, 1, re->oneof.size, stdout);
        printf("]");
        break;
    case TRegexSequence:
        for(size_t i = 0; i < re->sequence.pieces; i++)
            tregex_debug_print(re->sequence.piece[i]);
        break;
    case TRegexAlternative:
        printf("(");
        for(size_t i = 0; i < re->alternative.branches; i++) {
            if(i) printf("|");
            tregex_debug_print(re->alternative.branch[i]);
        }
        printf(")");
        break;
    case TRegexRepeat:
        if(re->repeat.minimum == 0 && re->repeat.range == 1) {
            printf("?");
        } else {
            printf("{%u,%u}", re->repeat.minimum,
                   re->repeat.minimum + re->repeat.range);
        }
        break;
    }
}

tregex *
tregex_string(const char *str, ssize_t len) {
    if(len < 0) len = strlen(str);
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexChars;
    re->randomness.bits = 0;
    re->min_size = len;
    re->max_size = len;
    re->chars.data = calloc(1, len + 1);
    assert(re->chars.data);
    memcpy(re->chars.data, str, len);
    re->chars.size = len;
    return re;
}

tregex *
tregex_join(tregex *re, tregex *rhs) {
    if(re->kind == TRegexSequence && rhs->kind == TRegexSequence) {
        if(re->sequence.pieces + rhs->sequence.pieces < TREGEX_ELEMENTS) {
            memcpy(&re->sequence.piece[re->sequence.pieces],
                   &rhs->sequence.piece[0],
                   rhs->sequence.pieces * sizeof(re->sequence.piece[0]));
            re->randomness.bits += rhs->randomness.bits;
            rhs->sequence.pieces = 0;
            re->min_size += rhs->min_size;
            re->max_size += rhs->max_size;
            return re;
        } else {
            assert(!"Too many regex elements");
            return NULL;
        }
    }

    if(re->kind == TRegexSequence && rhs->kind != TRegexSequence) {
        if(re->sequence.pieces < TREGEX_ELEMENTS) {
            re->sequence.piece[re->sequence.pieces++] = rhs;
            re->randomness.bits += rhs->randomness.bits;
            re->min_size += rhs->min_size;
            re->max_size += rhs->max_size;
            return re;
        } else {
            assert(!"Too many regex elements");
            return NULL;
        }
    }

    if(re->kind != TRegexSequence) {
        tregex *r = calloc(1, sizeof(*r));
        r->kind = TRegexSequence;
        r = tregex_join(r, re);
        r = tregex_join(r, rhs);
        return r;
    }

    assert(!"Unreachable");
}

static size_t bits_in(size_t n) {
    if(n) return floor(log2(n) + 1);
    return 0;
}

tregex *
tregex_range(unsigned char from, unsigned char to) {
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexClass;
    re->randomness.bits = bits_in(to-from);
    re->min_size = 1;
    re->max_size = 1;
    for(unsigned i = from; i <= to; i++) {
        re->oneof.table[re->oneof.size++] = i;
    }
    return re;
}

tregex *
tregex_range_from_string(const char *str, ssize_t len) {
    if(len < 0) len = strlen(str);
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexClass;
    re->min_size = 1;
    re->max_size = 1;
    uint8_t used[256];
    memset(used, 0, sizeof(used));
    for(size_t i = 0; i < (size_t)len; i++) {
        /* Remove duplicates */
        unsigned char c = str[i];
        if(!used[c]) {
            used[c] = 1;
            re->oneof.table[re->oneof.size++] = c;
        }
    }
    re->randomness.bits = bits_in(re->oneof.size);
    return re;
}

tregex *
tregex_union_ranges(tregex *re, tregex *rhs) {
    assert(re->kind == TRegexClass);
    assert(rhs->kind == TRegexClass);
    uint8_t used[256];
    memset(used, 0, sizeof(used));
    for(size_t i = 0; i < re->oneof.size; i++) {
        used[re->oneof.table[i]] = 1;
    }
    for(size_t i = 0; i < rhs->oneof.size; i++) {
        unsigned char c = rhs->oneof.table[i];
        if(!used[c]) {
            re->oneof.table[re->oneof.size++] = c;
        }
    }
    return re;
}

tregex *
tregex_alternative(tregex *rhs) {
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexAlternative;
    re->randomness.bits = rhs->randomness.bits;
    re->min_size = rhs->min_size;
    re->max_size = rhs->max_size;
    re->alternative.branch[0] = rhs;
    re->alternative.branches = 1;
    return re;
}

tregex *
tregex_alternative_add(tregex *re, tregex *rhs) {
    assert(re->kind == TRegexAlternative);
    if(re->alternative.branches < TREGEX_ELEMENTS) {
        re->randomness.bits -= ceil(log2(re->alternative.branches));
        re->alternative.branch[re->alternative.branches++] = rhs;
        re->randomness.bits += ceil(log2(re->alternative.branches));
        re->randomness.bits += rhs->randomness.bits;
    } else {
        assert(!"FIXME: Too many alternatives");
        return NULL;
    }
    if(re->min_size > rhs->min_size) re->min_size = rhs->min_size;
    if(re->max_size < rhs->max_size) re->max_size = rhs->max_size;
    return re;
}

tregex *
tregex_repeat(tregex *what, unsigned start, unsigned stop) {
    if(stop < start) {
        unsigned tmp = stop;
        stop = start;
        start = tmp;
    }
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexRepeat;
    re->randomness.bits = ceil(log2(1 + stop - start));
    re->randomness.bits += what->randomness.bits * stop;
    re->repeat.what = what;
    re->repeat.minimum = start;
    re->repeat.range = 1 + stop - start;
    re->min_size = what->min_size * start;
    re->max_size = what->max_size * stop;
    return re;
}

void
tregex_free(tregex *re) {
    if(re) {
        switch(re->kind) {
        case TRegexChars:
            free(re->chars.data);
            break;
        case TRegexClass:
            break;
        case TRegexRepeat:
            tregex_free(re->repeat.what);
            break;
        case TRegexSequence:
            for(size_t i = 0; i < re->sequence.pieces; i++)
                tregex_free(re->sequence.piece[i]);
            break;
        case TRegexAlternative:
            for(size_t i = 0; i < re->alternative.branches; i++)
                tregex_free(re->alternative.branch[i]);
            break;
        }
    }
}

struct randomness_source {
    uint8_t *randomness;
    size_t used;
    size_t bits;
};

/*
 * Get some randomness bits.
 */
static size_t rs_get(struct randomness_source *rs, size_t bound) {
    assert(bound >= 1);
    if(bound == 1) return 0;
    size_t bits = ceil(log2(bound));
    //printf("rs_get(bound=%zu); bits = %zu\n", bound, bits);
    assert(rs->bits - rs->used >= bits);
    size_t shift = (rs->used & 7);
    uint32_t r = (*(uint32_t *)(&rs->randomness[rs->used/8])) << shift;
    rs->used += bits;
    return (r % bound);
}

static ssize_t
tregex_eval_rs(tregex *re, char *buf, size_t size, struct randomness_source *rs) {
    const char *bold = buf;
    const char *bend = buf + size;

    if(re->max_size > size) return -1;

    switch(re->kind) {
    case TRegexChars:
        if((size_t)(bend - buf) >= re->chars.size) {
            memcpy(buf, re->chars.data, re->chars.size);
            buf += re->chars.size;
        }
        break;
    case TRegexClass:
        assert(re->oneof.size >= 1);
        if(bend - buf) {
            *buf++ = re->oneof.table[rs_get(rs, re->oneof.size)];
        }
        break;
    case TRegexRepeat: {
        size_t cycles = re->repeat.minimum
                        + (re->repeat.range ? rs_get(rs, re->repeat.range) : 0);
        for(unsigned i = 0; i < cycles; i++) {
            buf += tregex_eval_rs(re->repeat.what, buf, bend - buf, rs);
        }
    } break;
    case TRegexSequence:
        for(size_t i = 0; i < re->sequence.pieces; i++) {
            buf += tregex_eval_rs(re->sequence.piece[i], buf, bend - buf, rs);
        }
        break;
    case TRegexAlternative:
        buf += tregex_eval_rs(
            re->alternative.branch[rs_get(rs, re->alternative.branches)], buf, bend - buf, rs);
        break;
    }

    assert(buf <= bend);
    if(bold > buf) *buf = '\0';

    return (buf - bold);
}

ssize_t
tregex_eval_rng(tregex *re, char *buf, size_t size, pcg32_random_t *rng) {
    size_t bytes = ((re->randomness.bits | 63) + 1) / 8;
    assert((bytes & 3) == 0);   /* Aligned to 8 bytes */
    uint8_t randomness_buffer[bytes];
    for(size_t n = 0; n < sizeof(randomness_buffer); n += 4) {
        *(uint32_t *)(&randomness_buffer[n]) = pcg32_random_r(rng);
    }

    struct randomness_source rs;
    rs.randomness = randomness_buffer;
    rs.bits = re->randomness.bits;
    rs.used = 0;
    //printf("Evaluating with randomness bits %zu (%zu bytes)\n", rs.bits, bytes);
    return tregex_eval_rs(re, buf, size, &rs);
}

/* Slow-ish. Avoid using in performance-critical code. */
ssize_t
tregex_eval(tregex *re, char *buf, size_t size) {
    pcg32_random_t rng;
    pcg32_srandom_r(&rng, random(), 0);
    return tregex_eval_rng(re, buf, size, &rng);
}

size_t
tregex_min_size(tregex *re) {
    assert(re);
    return re->min_size;
}

size_t
tregex_max_size(tregex *re) {
    assert(re);
    return re->max_size;
}

#ifdef TCPKALI_REGEX_UNIT_TEST

int
main() {
    char buf[128];
    tregex *re;
    ssize_t n;

    /* [a-z] */
    re = tregex_range('a', 'z');
    n = tregex_eval(re, buf, sizeof(buf));
    assert(n == 1);
    assert(buf[0] >= 'a' && buf[0] <= 'z');
    tregex_free(re);

    /* [a-z]? */
    re = tregex_repeat(tregex_range('a', 'z'), 0, 1);
    n = tregex_eval(re, buf, sizeof(buf));
    assert(n == 0 || n == 1);
    assert(!n || (buf[0] >= 'a' && buf[0] <= 'z'));
    tregex_free(re);

    /* [a-z]+ */
    re = tregex_repeat(tregex_range('a', 'z'), 1, 8);
    n = tregex_eval(re, buf, sizeof(buf));
    assert(n >= 1 && n <= 8);
    assert(buf[0] >= 'a' && buf[0] <= 'z');
    tregex_free(re);

    /* a|b */
    re = tregex_alternative(tregex_range('a', 'a'));
    re = tregex_alternative_add(re, tregex_range('b', 'b'));
    n = tregex_eval(re, buf, sizeof(buf));
    assert(n == 1);
    assert(buf[0] == 'a' || buf[0] == 'b');
    tregex_free(re);

    /* a|b */
    re = tregex_alternative(tregex_string("a", -1));
    re = tregex_alternative_add(re, tregex_string("b", -1));
    n = tregex_eval(re, buf, sizeof(buf));
    assert(n == 1);
    assert(buf[0] == 'a' || buf[0] == 'b');
    tregex_free(re);
}

#endif /* TCPKALI_REGEX_UNIT_TEST */
