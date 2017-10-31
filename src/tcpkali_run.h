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
#include "tcpkali_signals.h"
#include "TcpkaliMessage.h"

struct orchestration_data;

enum work_phase { PHASE_ESTABLISHING_CONNECTIONS, PHASE_STEADY_STATE };

struct rate_modulator {
    enum {
        RM_UNMODULATED, /* Do not modulate request rate */
        RM_MAX_RATE_AT_TARGET_LATENCY
    } mode;
    enum { RMS_STATE_INITIAL, RMS_RATE_RAMP_UP, RMS_RATE_BINARY_SEARCH } state;
    double last_update_long;
    double last_update_short;
    double latency_target; /* In seconds. */
    char *latency_target_s;
    int binary_search_steps;
    /*
     * Runtime parameters.
     */
    /* Previous latency value */
    double prev_latency;
    int prev_max_latency_exceeded;
    /*
     * Rate bounds for binary search.
     */
    double rate_min_bound;
    double rate_max_bound;
    double suggested_rate_value;
};

enum oc_return_value {
    OC_CONNECTED,
    OC_TIMEOUT,
    OC_INTERRUPT,
    OC_RATE_GOAL_MET,
    OC_RATE_GOAL_FAILED
};

struct oc_args {
    struct engine *eng;
    int max_connections;
    double connect_rate;
    double epoch_end;
    double latency_window;
    volatile sig_atomic_t term_flag;
    struct stats_checkpoint {
        double epoch_start; /* Start of current checkpoint epoch */
        double last_update; /* Last we updated the checkpoint structure */
        double last_latency_window_flush;   /* Last time we flushed statsd latencies */
        non_atomic_traffic_stats initial_traffic_stats; /* Ramp-up phase traffic */
        non_atomic_traffic_stats last_traffic_stats;
    } checkpoint;
    struct latency_snapshot *previous_window_latency;
    mavg traffic_mavgs[2];
    mavg count_mavgs[2];    /* --message-marker */
    size_t connections_opened_tally;
    Statsd *statsd;
    struct rate_modulator *rate_modulator;
    struct percentile_values *latency_percentiles;
    int print_stats;
};

enum oc_return_value
open_connections_until_maxed_out(enum work_phase phase,
                                 struct oc_args *,
                                 struct orchestration_data *orch_state);

struct orchestration_args {
    int enabled;
    char *server_addr_str;
    struct addrinfo *server_addrs;
};

struct orchestration_data {
    int connected;
    int sockfd;
    char *buf;
    char *buf_write;
};

struct orchestration_data
tcpkali_connect_to_orch_server(struct orchestration_args arsgs);
TcpkaliMessage_t * tcpkali_wait_for_start_command(struct orchestration_data *state);
TcpkaliMessage_t * read_orch_command(struct orchestration_data *state);
void free_orch_message(TcpkaliMessage_t *msg);

void
free_message(TcpkaliMessage_t *msg);

#endif /* TCPKALI_RUN_H */
