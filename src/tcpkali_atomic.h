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

#ifndef UNUSED
#define UNUSED  __attribute__((unused))
#endif

#if defined(__GNUC__) && (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 || __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)

typedef uint32_t atomic_t;
#ifdef  __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
typedef uint64_t atomic_wide_t;
#else
#warning "This compiler does not have 64-bit compare_and_swap, results might be broken"
typedef uint32_t atomic_wide_t;
#endif

static inline void UNUSED
atomic_add(atomic_wide_t *i, uint64_t v) { __sync_add_and_fetch(i, v); }

static atomic_wide_t UNUSED
atomic_wide_get(atomic_wide_t *i) { return __sync_add_and_fetch(i, 0); }

static inline void UNUSED
atomic_increment(atomic_t *i) { __sync_add_and_fetch(i, 1); }

static inline void UNUSED
atomic_decrement(atomic_t *i) { __sync_add_and_fetch(i, -1); }

static inline atomic_t UNUSED
atomic_get(atomic_t *i) { return __sync_add_and_fetch(i, 0); }

#else   /* No builtin atomics, emulate */

#if SIZEOF_SIZE_T == 4
typedef uint32_t atomic_t;
typedef uint32_t atomic_wide_t;
static inline void UNUSED atomic_add(atomic_wide_t *i, uint64_t v) {
    asm volatile("lock addl %1, %0" : "+m" (*i) : "r" (v));
}
static atomic_wide_t UNUSED atomic_wide_get(atomic_wide_t *i) { return *i; }
static atomic_t UNUSED atomic_get(atomic_t *i) { return *i; }
#elif SIZEOF_SIZE_T == 8
typedef uint32_t atomic_t;
typedef uint64_t atomic_wide_t;
static inline void UNUSED atomic_add(atomic_wide_t *i, uint64_t v) {
    asm volatile("lock addq %1, %0" : "+m" (*i) : "r" (v));
}
static atomic_wide_t UNUSED atomic_wide_get(atomic_wide_t *i) { return *i; }
static atomic_t UNUSED atomic_get(atomic_t *i) { return *i; }
#endif  /* SIZEOF_SIZE_T */

static inline void UNUSED
atomic_increment(atomic_t *i) { asm volatile("lock incl %0" : "+m" (*i)); }

static inline void UNUSED
atomic_decrement(atomic_t *i) { asm volatile("lock decl %0" : "+m" (*i)); }

#endif  /* Builtin atomics */

#endif  /* TCPKALI_ATOMIC_H */
