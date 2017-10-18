/*
 * Copyright (c) 2016  Machine Zone, Inc.
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
#ifndef TCPKALI_REGEX_H
#define TCPKALI_REGEX_H

#include <pcg_basic.h>

typedef struct tregex tregex;

/* [a-z] = ('a', 'z') */
tregex *tregex_range(unsigned char from, unsigned char to);

/* [abcdef] = ("abcdef") */
tregex *tregex_range_from_string(const char *str, ssize_t);

/* "abc" = ("abc") */
tregex *tregex_string(const char *str, ssize_t len);

/* [a-zA-Z] = ([a-z], [A-Z]) */
tregex *tregex_union_ranges(tregex *, tregex *);

/* "abcde" = ("ab", "cde") */
tregex *tregex_join(tregex *, tregex *);

/* (ab|cde) = ("ab", "cde") */
tregex *tregex_alternative(tregex *rhs);
tregex *tregex_alternative_add(tregex *base, tregex *rhs);

/* "a?" = ("a", 0, 1) */
tregex *tregex_repeat(tregex *, unsigned from, unsigned to);

size_t tregex_min_size(tregex *);
size_t tregex_avg_size(tregex *);
size_t tregex_max_size(tregex *);

/*
 * Returns -1 if the expression can't fit into the provided buffer in full.
 * Returns the actual size of the data put into the buffer otherwise.
 */
ssize_t tregex_eval(tregex *, char *buf, size_t size);
ssize_t tregex_eval_rng(tregex *, char *buf, size_t size, pcg32_random_t *);

void tregex_free(tregex *);

#endif /* TCPKALI_REGEX_H */
