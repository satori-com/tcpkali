/*
 * Copyright (c) 2015, 2016, 2017  Machine Zone, Inc.
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
#ifndef TCPKALI_COMMON_H
#define TCPKALI_COMMON_H

#include <config.h>
#include <inttypes.h>
#include <assert.h>
#include <stddef.h>

#define UNUSED __attribute__((unused))
#define PRINTFLIKE(n, m) __attribute__((format(printf, n, m)))

/*
 * Requested types of latency measurement.
 */
typedef enum {
    SLT_CONNECT = (1 << 0),
    SLT_FIRSTBYTE = (1 << 1),
    SLT_MARKER = (1 << 2)
} statsd_report_latency_types;

#define MESSAGE_MARKER_TOKEN "TCPKaliMsgTS-"

/*
 * Snapshot of the current latency.
 */
struct latency_snapshot {
    struct hdr_histogram *connect_histogram;
    struct hdr_histogram *firstbyte_histogram;
    struct hdr_histogram *marker_histogram;
};

/*
 * Array of doubles used in e.g. overriding reported latency percentiles.
 */
struct percentile_values {
    size_t size;
    struct percentile_value {
        double value_d;
        char value_s[sizeof("100.123")];
    } *values;
};

#endif /* TCPKALI_COMMON_H */
