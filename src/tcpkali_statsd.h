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
#ifndef TCPKALI_STATSD_H
#define TCPKALI_STATSD_H

#include <statsd.h>

#include "tcpkali_common.h"
#include "tcpkali_atomic.h"
#include "tcpkali_engine.h"

/*
 * What we are sending to statsd?
 */
typedef struct {
    size_t opened;
    size_t conns_in;
    size_t conns_out;
    size_t bps_in;
    size_t bps_out;
    non_atomic_traffic_stats traffic_delta;
    struct latency_snapshot *latency;
} statsd_feedback;

void report_to_statsd(Statsd *statsd, statsd_feedback *feedback_optional,
                      statsd_report_latency_types types,
                      const struct percentile_values *latency_percentiles);

void report_latency_to_statsd(Statsd *statsd, struct latency_snapshot *,
                      statsd_report_latency_types types,
                      const struct percentile_values *latency_percentiles);


#endif /* TCPKALI_STATSD_H */
