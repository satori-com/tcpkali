/*
 * Copyright (c) 2014, 2015, 2016  Machine Zone, Inc.
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tcpkali_statsd.h"

void
report_to_statsd(Statsd *statsd, statsd_feedback *sf) {
    static statsd_feedback empty_feedback;

    if(!statsd) return;
    if(!sf) sf = &empty_feedback;

    statsd_resetBatch(statsd);

#define SBATCH(t, str, value)                                  \
    do {                                                       \
        int ret = statsd_addToBatch(statsd, t, str, value, 1); \
        if(ret == STATSD_BATCH_FULL) {                         \
            statsd_sendBatch(statsd);                          \
            ret = statsd_addToBatch(statsd, t, str, value, 1); \
        }                                                      \
        assert(ret == STATSD_SUCCESS);                         \
    } while(0)

    SBATCH(STATSD_COUNT, "connections.opened", sf->opened);
    SBATCH(STATSD_GAUGE, "connections.total", sf->conns_in + sf->conns_out);
    SBATCH(STATSD_GAUGE, "connections.total.in", sf->conns_in);
    SBATCH(STATSD_GAUGE, "connections.total.out", sf->conns_out);
    SBATCH(STATSD_GAUGE, "traffic.bitrate", sf->bps_in + sf->bps_out);
    SBATCH(STATSD_GAUGE, "traffic.bitrate.in", sf->bps_in);
    SBATCH(STATSD_GAUGE, "traffic.bitrate.out", sf->bps_out);
    SBATCH(STATSD_COUNT, "traffic.data",
           sf->traffic_delta.bytes_rcvd + sf->traffic_delta.bytes_sent);
    SBATCH(STATSD_COUNT, "traffic.data.rcvd", sf->traffic_delta.bytes_rcvd);
    SBATCH(STATSD_COUNT, "traffic.data.sent", sf->traffic_delta.bytes_sent);
    SBATCH(STATSD_COUNT, "traffic.data.reads", sf->traffic_delta.num_reads);
    SBATCH(STATSD_COUNT, "traffic.data.writes", sf->traffic_delta.num_writes);

    if((sf->latency && sf->latency->marker_histogram) || sf == &empty_feedback) {
        struct {
            unsigned p50;
            unsigned p95;
            unsigned p99;
            unsigned p99_5;
            unsigned mean;
            unsigned max;
        } lat;

        if(sf->latency && sf->latency->marker_histogram) {
            struct hdr_histogram *hist = sf->latency->marker_histogram;
            lat.p50 = hdr_value_at_percentile(hist, 50.0) / 10.0;
            lat.p95 = hdr_value_at_percentile(hist, 95.0) / 10.0;
            lat.p99 = hdr_value_at_percentile(hist, 99.0) / 10.0;
            lat.p99_5 = hdr_value_at_percentile(hist, 99.5) / 10.0;
            lat.mean = hdr_mean(hist) / 10.0;
            lat.max = hdr_max(hist) / 10.0;
            assert(lat.p95 < 1000000);
            assert(lat.mean < 1000000);
            assert(lat.max < 1000000);
        } else {
            memset(&lat, 0, sizeof(lat));
        }

        SBATCH(STATSD_GAUGE, "latency.mean", lat.mean);
        SBATCH(STATSD_GAUGE, "latency.50", lat.p50);
        SBATCH(STATSD_GAUGE, "latency.95", lat.p95);
        SBATCH(STATSD_GAUGE, "latency.99", lat.p99);
        SBATCH(STATSD_GAUGE, "latency.99.5", lat.p99_5);
        SBATCH(STATSD_GAUGE, "latency.max", lat.max);
    }

    statsd_sendBatch(statsd);
}
