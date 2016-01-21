/*
 * Copyright (c) 2014  Machine Zone, Inc.
 *
 * Original author: Lev Walkin <lwalkin@machinezone.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef TCPKALI_ATOMIC_H
#define TCPKALI_ATOMIC_H

#include "tcpkali_common.h"

#define PRIan PRIu32 /* printf formatting argument for narrow type */

/*
 * We introduce two atomic integer types, narrow (32-bit) and wide, which is
 * hopefully 64-bit, if the system permits. We also introduce non-atomic
 * types, to allow collecting data in the data type compatible with the
 * atomic, but not provide atomicity guarantees. This two-tier typing is
 * good for documentation and correctness.
 */
#if defined(__clang__)                                           \
    || (defined(__GNUC__) && (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 \
                              || __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8))

#if defined(__clang__) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
#define PRIaw PRIu64
typedef uint64_t non_atomic_wide_t;
typedef uint32_t non_atomic_narrow_t;
#else
#define PRIaw PRIu32
typedef uint32_t non_atomic_wide_t;
typedef uint32_t non_atomic_narrow_t;
#warning \
    "This compiler does not have 64-bit compare_and_swap, results might be broken"
#endif

typedef struct { non_atomic_wide_t _atomic_val; } atomic_wide_t;
typedef struct { non_atomic_narrow_t _atomic_val; } atomic_narrow_t;

static inline void UNUSED
atomic_add(atomic_wide_t *i, non_atomic_wide_t v) {
    __sync_add_and_fetch(&i->_atomic_val, v);
}

static inline void UNUSED
atomic_increment(atomic_narrow_t *i) {
    __sync_add_and_fetch(&i->_atomic_val, 1);
}

static inline void UNUSED
atomic_decrement(atomic_narrow_t *i) {
    __sync_add_and_fetch(&i->_atomic_val, -1);
}

static inline non_atomic_narrow_t UNUSED
atomic_inc_and_get(atomic_narrow_t *i) {
    return __sync_add_and_fetch(&i->_atomic_val, 1);
}

static inline non_atomic_narrow_t UNUSED
atomic_get(const atomic_narrow_t *i) {
    return __sync_add_and_fetch(&((atomic_narrow_t *)i)->_atomic_val, 0);
}

static non_atomic_wide_t UNUSED
atomic_wide_get(const atomic_wide_t *i) {
    return __sync_add_and_fetch(&((atomic_wide_t *)i)->_atomic_val, 0);
}

#else /* No builtin atomics, emulate */

#if SIZEOF_SIZE_T == 4
#define PRIaw PRIu32
typedef uint32_t non_atomic_wide_t;
typedef uint32_t non_atomic_narrow_t;
#elif SIZEOF_SIZE_T == 8
#define PRIaw PRIu64
typedef uint64_t non_atomic_wide_t;
typedef uint32_t non_atomic_narrow_t;
#endif /* SIZEOF_SIZE_T */

typedef struct { non_atomic_wide_t _atomic_val; } atomic_wide_t;
typedef struct { non_atomic_narrow_t _atomic_val; } atomic_narrow_t;

static inline void UNUSED
atomic_add(atomic_wide_t *i, uint64_t v) {
#if SIZEOF_SIZE_T == 4
    asm volatile("lock addl %1, %0" : "+m"(i->_atomic_val) : "r"(v));
#elif SIZEOF_SIZE_T == 8
    asm volatile("lock addq %1, %0" : "+m"(i->_atomic_val) : "r"(v));
#else
#error "Weird platform, aborting"
#endif /* SIZEOF_SIZE_T */
}

static inline non_atomic_narrow_t UNUSED
atomic_get(atomic_narrow_t *i) {
    return i->_atomic_val;
}

static inline non_atomic_wide_t UNUSED
atomic_wide_get(atomic_wide_t *i) {
    return i->_atomic_val;
}

static inline void UNUSED
atomic_increment(atomic_narrow_t *i) {
    asm volatile("lock incl %0" : "+m"(i->_atomic_val));
}

static inline void UNUSED
atomic_decrement(atomic_narrow_t *i) {
    asm volatile("lock decl %0" : "+m"(i->_atomic_val));
}

static inline non_atomic_narrow_t UNUSED
atomic_inc_and_get(atomic_narrow_t *i) {
    non_atomic_narrow_t prev = 1;
    asm volatile("lock xaddl %1, %0" : "+m"(i->_atomic_val), "+r"(prev));
    return 1 + prev;
}

#endif /* Builtin atomics */

#endif /* TCPKALI_ATOMIC_H */
