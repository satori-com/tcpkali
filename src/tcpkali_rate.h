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
#ifndef TCPKALI_RATE_H
#define TCPKALI_RATE_H

#include "tcpkali_common.h"

/*
 * Rate specification, as taken from the user's input (CLI).
 */
typedef struct rate_spec {
    double value;
    enum {
        RS_UNLIMITED,
        RS_BYTES_PER_SECOND,
        RS_MESSAGES_PER_SECOND,
    } value_base;
} rate_spec_t;

#define RATE_BPS(rate) \
    ((rate_spec_t){.value_base = RS_BYTES_PER_SECOND, .value = rate})

#define RATE_MPS(rate) \
    ((rate_spec_t){.value_base = RS_MESSAGES_PER_SECOND, .value = rate})

/*
 * Bandwidth limit description structure,
 * figured out from the rate specification once we know the message size.
 */
typedef struct bandwidth_limit {
    /* floating point OK */
    double bytes_per_second;
    /*
     * --channel-bandwidth intentionally disregards the message
     * boundary, unlike --message-rate, which attempts to preserve it.
     * Thus minimal_move_size is 1 for --channel-bandwidth and is equal to
     * message size if not.
     */
    uint32_t minimal_move_size;
} bandwidth_limit_t;

/*
 * Compute the bandwidth limit based on the rate specification.
 */
static inline UNUSED bandwidth_limit_t
compute_bandwidth_limit(rate_spec_t rspec) {
    bandwidth_limit_t lim = {
        .bytes_per_second = -1.0, /* Simulate "not set" -> no limit. */
        .minimal_move_size = 1460 /* ~MTU */
    };

    if(rspec.value_base == RS_BYTES_PER_SECOND) {
        lim.bytes_per_second = rspec.value;
        lim.minimal_move_size = 1;
    }

    return lim;
}

/*
 * Compute the bandwidth limit based on the rate specification and the message
 * size.
 */
static inline UNUSED bandwidth_limit_t
compute_bandwidth_limit_by_message_size(rate_spec_t rspec,
                                        size_t message_size) {
    bandwidth_limit_t lim = {0, 0};

    /*
     * The message_size varies according to the \{expressions} which might
     * be present in a message. Therefore we compute the bandwidth limit
     * independently for every connection made.
     */
    switch(rspec.value_base) {
    case RS_UNLIMITED:
        lim.bytes_per_second = -1.0;  /* Simulate "not set" -> no limit. */
        lim.minimal_move_size = 1460; /* ~MTU */
        break;
    case RS_BYTES_PER_SECOND:
        lim.bytes_per_second = rspec.value;
        lim.minimal_move_size = 1;
        break;
    case RS_MESSAGES_PER_SECOND:
        lim.bytes_per_second = message_size * rspec.value;
        lim.minimal_move_size = message_size < 1460 ? message_size : 1460;
        break;
    }

    return lim;
}

#endif /* TCPKALI_RATE_H */
