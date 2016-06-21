/*
 * Copyright (c) 2015  Machine Zone, Inc.
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
#ifndef TCPKALI_TRAFFIC_STATS_H
#define TCPKALI_TRAFFIC_STATS_H

#include "tcpkali_atomic.h"

/*
 * Traffic numbers. Non-atomic (non-shared).
 */
typedef struct {
    non_atomic_wide_t bytes_sent;
    non_atomic_wide_t num_writes; /* Number of write(2) calls */
    non_atomic_wide_t bytes_rcvd;
    non_atomic_wide_t num_reads; /* Number of read(2) calls */
    non_atomic_wide_t msgs_sent;
    non_atomic_wide_t msgs_rcvd;
} non_atomic_traffic_stats;

/*
 * Traffic numbers. Shared between threads.
 */
typedef struct {
    atomic_wide_t bytes_sent;
    atomic_wide_t num_writes; /* Number of write(2) calls */
    atomic_wide_t bytes_rcvd;
    atomic_wide_t num_reads; /* Number of read(2) calls */
    atomic_wide_t msgs_sent;
    atomic_wide_t msgs_rcvd;
} atomic_traffic_stats;

/*
 * Add atomic traffic numbers to non-atomic. Mutates the (dst).
 */
static UNUSED void
add_traffic_numbers_AtoN(const atomic_traffic_stats *src,
                         non_atomic_traffic_stats *dst) {
    dst->bytes_sent += atomic_wide_get(&src->bytes_sent);
    dst->num_writes += atomic_wide_get(&src->num_writes);
    dst->bytes_rcvd += atomic_wide_get(&src->bytes_rcvd);
    dst->num_reads += atomic_wide_get(&src->num_reads);
    dst->msgs_sent += atomic_wide_get(&src->msgs_sent);
    dst->msgs_rcvd += atomic_wide_get(&src->msgs_rcvd);
}

static UNUSED void
add_traffic_numbers_NtoA(const non_atomic_traffic_stats *src,
                         atomic_traffic_stats *dst) {
    atomic_add(&dst->bytes_sent, src->bytes_sent);
    atomic_add(&dst->num_writes, src->num_writes);
    atomic_add(&dst->bytes_rcvd, src->bytes_rcvd);
    atomic_add(&dst->num_reads, src->num_reads);
    atomic_add(&dst->msgs_sent, src->msgs_sent);
    atomic_add(&dst->msgs_rcvd, src->msgs_rcvd);
}

/*
 * Add atomic traffic numbers to non-atomic. Returns the (a) - (b).
 */
static UNUSED non_atomic_traffic_stats
subtract_traffic_stats(const non_atomic_traffic_stats a,
                       const non_atomic_traffic_stats b) {
    non_atomic_traffic_stats result;
    result.bytes_sent = a.bytes_sent - b.bytes_sent;
    result.num_writes = a.num_writes - b.num_writes;
    result.bytes_rcvd = a.bytes_rcvd - b.bytes_rcvd;
    result.num_reads = a.num_reads - b.num_reads;
    result.msgs_sent = a.msgs_sent - b.msgs_sent;
    result.msgs_rcvd = a.msgs_rcvd - b.msgs_rcvd;
    return result;
}

#endif /* TCPKALI_TRAFFIC_STATS_H */
