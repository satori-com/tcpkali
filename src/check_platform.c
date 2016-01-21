#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include "tcpkali_atomic.h"

struct atomic_holder_1 {
    char c;
    atomic_wide_t atomic;
};

struct atomic_holder_2 {
    char c;
    atomic_wide_t atomic __attribute__((aligned(sizeof(atomic_wide_t))));
};

int
main() {
    /* Check that atomic is properly aligned for atomicity */
    assert(((long)(&((struct atomic_holder_1 *)0)->atomic)
            & (sizeof(atomic_narrow_t) - 1))
           == 0);
    assert(((long)(&((struct atomic_holder_2 *)0)->atomic)
            & (sizeof(atomic_narrow_t) - 1))
           == 0);

    assert((&((struct atomic_holder_1 *)0)->atomic)
           == (&((struct atomic_holder_2 *)0)->atomic));

    return 0;
}
