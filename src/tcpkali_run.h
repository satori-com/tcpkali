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
#ifndef TCPKALI_RUN_H
#define TCPKALI_RUN_H

#include "tcpkali_common.h"
#include "tcpkali_mavg.h"
#include "tcpkali_atomic.h"
#include "tcpkali_engine.h"
#include "tcpkali_statsd.h"

enum work_phase { PHASE_ESTABLISHING_CONNECTIONS, PHASE_STEADY_STATE };

struct stats_checkpoint {
    double epoch_start; /* Start of current checkpoint epoch */
    double last_update; /* Last we updated the checkpoint structure */
    non_atomic_traffic_stats initial_traffic_stats; /* Ramp-up phase traffic */
    non_atomic_traffic_stats last_traffic_stats;
};

int open_connections_until_maxed_out(struct engine *eng, double connect_rate,
                                     int max_connections, double epoch_end,
                                     struct stats_checkpoint *checkpoint,
                                     mavg traffic_mavgs[2], Statsd *statsd,
                                     int *term_flag, enum work_phase phase,
                                     int print_stats);

#endif /* TCPKALI_RUN_H */
