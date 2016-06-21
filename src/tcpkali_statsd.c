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

#define SBATCH_INT(t, str, value)                               \
    do {                                                        \
        int ret = statsd_addToBatch(statsd, t, str, value, 1);  \
        if(ret == STATSD_BATCH_FULL) {                          \
            statsd_sendBatch(statsd);                           \
            ret = statsd_addToBatch(statsd, t, str, value, 1);  \
        }                                                       \
        assert(ret == STATSD_SUCCESS);                          \
    } while(0)

#define SBATCH_DBL(t, str, value)                                   \
    do {                                                            \
        int ret = statsd_addToBatch_dbl(statsd, t, str, value, 1);  \
        if(ret == STATSD_BATCH_FULL) {                              \
            statsd_sendBatch(statsd);                               \
            ret = statsd_addToBatch_dbl(statsd, t, str, value, 1);  \
        }                                                           \
        assert(ret == STATSD_SUCCESS);                              \
    } while(0)


static void report_latency(Statsd *statsd, statsd_report_latency_types ltype, struct hdr_histogram *hist, const struct percentile_values *latency_percentiles) {

    if(!hist || hist->total_count == 0)
        return;

#define LENGTH_PREFIXED_STR(s)  {sizeof(s)-1,(s)}
    static struct prefixes {
        const char *min;
        const char *mean;
        const char *max;
        struct {
            size_t size;
            const char *str;
        } latency_kind;
    } prefixes[] =
        {[SLT_CONNECT] = {"latency.connect.min", "latency.connect.mean",
                          "latency.connect.max",
                          LENGTH_PREFIXED_STR("latency.connect.")},
         [SLT_FIRSTBYTE] = {"latency.firstbyte.min", "latency.firstbyte.mean",
                            "latency.firstbyte.max",
                            LENGTH_PREFIXED_STR("latency.firstbyte.")},
         [SLT_MARKER] = {"latency.message.min", "latency.message.mean",
                         "latency.message.max",
                         LENGTH_PREFIXED_STR("latency.message.")}};
    assert(ltype < sizeof(prefixes)/sizeof(prefixes[0]));
    const struct prefixes *pfx = &prefixes[ltype];
    assert(pfx->mean);

    for(size_t i = 0; i < latency_percentiles->size; i++) {
        const struct percentile_value *pv = &latency_percentiles->values[i];
        char name[64];
        memcpy(name, pfx->latency_kind.str, pfx->latency_kind.size);
        strcpy(name+pfx->latency_kind.size, pv->value_s);
        double latency_ms = hdr_value_at_percentile(hist, pv->value_d) / 10.0;
        SBATCH_DBL(STATSD_GAUGE, name, latency_ms);
    }

    SBATCH_DBL(STATSD_GAUGE, pfx->min, hdr_min(hist) / 10.0);
    SBATCH_DBL(STATSD_GAUGE, pfx->mean, hdr_mean(hist) / 10.0);
    SBATCH_DBL(STATSD_GAUGE, pfx->max, hdr_max(hist) / 10.0);
}


void
report_to_statsd(Statsd *statsd, statsd_feedback *sf, statsd_report_latency_types latency_types, const struct percentile_values *latency_percentiles) {
    if(!statsd) return;
    if(!sf) {
        static statsd_feedback empty_feedback;
        sf = &empty_feedback;
    }

    statsd_resetBatch(statsd);

    SBATCH_INT(STATSD_COUNT, "connections.opened", sf->opened);
    SBATCH_INT(STATSD_GAUGE, "connections.total", sf->conns_in + sf->conns_out);
    SBATCH_INT(STATSD_GAUGE, "connections.total.in", sf->conns_in);
    SBATCH_INT(STATSD_GAUGE, "connections.total.out", sf->conns_out);
    SBATCH_INT(STATSD_GAUGE, "traffic.bitrate", sf->bps_in + sf->bps_out);
    SBATCH_INT(STATSD_GAUGE, "traffic.bitrate.in", sf->bps_in);
    SBATCH_INT(STATSD_GAUGE, "traffic.bitrate.out", sf->bps_out);
    SBATCH_INT(STATSD_COUNT, "traffic.data",
           sf->traffic_delta.bytes_rcvd + sf->traffic_delta.bytes_sent);
    SBATCH_INT(STATSD_COUNT, "traffic.data.rcvd", sf->traffic_delta.bytes_rcvd);
    SBATCH_INT(STATSD_COUNT, "traffic.data.sent", sf->traffic_delta.bytes_sent);
    SBATCH_INT(STATSD_COUNT, "traffic.data.reads", sf->traffic_delta.num_reads);
    SBATCH_INT(STATSD_COUNT, "traffic.data.writes", sf->traffic_delta.num_writes);
    SBATCH_INT(STATSD_COUNT, "traffic.msgs.rcvd", sf->traffic_delta.msgs_rcvd);
    SBATCH_INT(STATSD_COUNT, "traffic.msgs.sent", sf->traffic_delta.msgs_sent);

    if(latency_types) {
        if(latency_types & SLT_CONNECT)
            report_latency(statsd, SLT_CONNECT,
                           sf->latency ? sf->latency->connect_histogram : 0,
                           latency_percentiles);
        if(latency_types & SLT_FIRSTBYTE)
            report_latency(statsd, SLT_FIRSTBYTE,
                           sf->latency ? sf->latency->firstbyte_histogram : 0,
                           latency_percentiles);
        if(latency_types & SLT_MARKER)
            report_latency(statsd, SLT_MARKER,
                           sf->latency ? sf->latency->marker_histogram : 0,
                           latency_percentiles);
    }

    statsd_sendBatch(statsd);
}

void
report_latency_to_statsd(Statsd *statsd, struct latency_snapshot *latency, statsd_report_latency_types latency_types, const struct percentile_values *latency_percentiles) {
    if(!statsd) return;

    statsd_resetBatch(statsd);

    if(latency_types & SLT_CONNECT)
        report_latency(statsd, SLT_CONNECT,
                       latency->connect_histogram,
                       latency_percentiles);
    if(latency_types & SLT_FIRSTBYTE)
        report_latency(statsd, SLT_FIRSTBYTE,
                       latency->firstbyte_histogram,
                       latency_percentiles);
    if(latency_types & SLT_MARKER)
        report_latency(statsd, SLT_MARKER,
                       latency->marker_histogram,
                       latency_percentiles);

    statsd_sendBatch(statsd);
}

