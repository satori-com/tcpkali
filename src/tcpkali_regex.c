#include <sys/types.h>
#include <stdlib.h>
#include "tcpkali_common.h"
#include "tcpkali_regex.h"

struct tregex {
    enum {
        TRegexOneOf,
        TRegexAlternative,
        TRegexRepeat
    } kind;
    size_t max_size;
    union {
        struct {
            unsigned char size;
            unsigned char table[256];
        } oneof;
        struct {
            tregex *branch[6];
            unsigned branches;
        } alternative;
        struct {
            tregex *what;
            unsigned repeat_min;
            unsigned repeat_max;
        } repeat;
    };
};

ssize_t
tregex_execute(tregex *r, char *buf, size_t size) {
    size_t old_size = size;
    for(; size > 0; size--) {
        switch(r->kind) {
        case TRegexOneOf:
            assert(r->oneof.size >= 1);
            *buf = r->oneof.table[random() % r->oneof.size];
            break;
        }
    }
    return (old_size - size);
}


#ifdef  TCPKALI_REGEX_UNIT_TEST

int main() {
    char buf[128];
    tregex *r = tregex_range('a', 'z');

    assert(trex_execute(r, buf, sizeof(buf)) == 1);
    assert(buf[0] >= 'a' && buf[0] <= 'z');

}

#endif  /* TCPKALI_REGEX_UNIT_TEST */
