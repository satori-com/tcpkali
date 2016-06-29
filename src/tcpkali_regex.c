#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tcpkali_common.h"
#include "tcpkali_regex.h"

struct tregex {
#define TREGEX_ELEMENTS 16
    enum { TRegexChars, TRegexClass, TRegexSequence, TRegexAlternative, TRegexRepeat } kind;
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
            printf("{%u,%u}", re->repeat.minimum, re->repeat.minimum + re->repeat.range);
        }
        break;
    }
}

tregex *
tregex_string(const char *str, ssize_t len) {
    if(len < 0) len = strlen(str);
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexChars;
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
            memcpy(&re->sequence.piece[re->sequence.pieces], &rhs->sequence.piece[0],
                rhs->sequence.pieces * sizeof(re->sequence.piece[0]));
            re->sequence.pieces += rhs->sequence.pieces;
            rhs->sequence.pieces = 0;
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

tregex *
tregex_range(unsigned char from, unsigned char to) {
    tregex *re = calloc(1, sizeof(*re));
    re->kind = TRegexClass;
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
    re->max_size = rhs->max_size;
    re->alternative.branch[0] = rhs;
    re->alternative.branches = 1;
    return re;
}

tregex *
tregex_alternative_add(tregex *re, tregex *rhs) {
    assert(re->kind == TRegexAlternative);
    if(re->alternative.branches < TREGEX_ELEMENTS) {
        re->alternative.branch[re->alternative.branches++] = rhs;
    } else {
        assert(!"FIXME: Too many alternatives");
        return NULL;
    }
    if(re->max_size < rhs->max_size)
        re->max_size = rhs->max_size;
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
    re->repeat.what = what;
    re->repeat.minimum = start;
    re->repeat.range = stop - start;
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

ssize_t
tregex_eval(tregex *re, char *buf, size_t size) {
    const char *bold = buf;
    const char *bend = buf + size;

    assert(re->max_size <= size);

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
            *buf++ = re->oneof.table[random() % re->oneof.size];
        }
        break;
    case TRegexRepeat: {
        size_t cycles =
            re->repeat.minimum
            + (re->repeat.range ? random() % re->repeat.range : 0);
        for(unsigned i = 0; i < cycles; i++) {
            ssize_t written = tregex_eval(re->repeat.what, buf, bend - buf);
            if(written < 0 || (bend - buf) < written)
                return -1;
            else
                buf += written;
        }
    } break;
    case TRegexSequence:
        for(size_t i = 0; i < re->sequence.pieces; i++) {
            ssize_t written = tregex_eval(re->sequence.piece[i], buf, bend - buf);
            if(written < 0 || (bend - buf) < written)
                return -1;
            else
                buf += written;
        }
        break;
    case TRegexAlternative: {
        ssize_t written = tregex_eval(
            re->alternative.branch[random() % re->alternative.branches],
            buf, bend - buf);
        if(written < 0 || (bend - buf) < written)
            return -1;
        else
            buf += written;
    } break;
    }

    if(bold > buf)
        *buf = '\0';

    return (buf - bold);
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
