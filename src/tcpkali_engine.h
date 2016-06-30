/*
 * Copyright (c) 2014, 2015  Machine Zone, Inc.
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
#ifndef TCPKALI_ENGINE_H
#define TCPKALI_ENGINE_H

#include <StreamBoyerMooreHorspool.h>
#include <hdr_histogram.h>

#include "tcpkali_traffic_stats.h"
#include "tcpkali_transport.h"
#include "tcpkali_logging.h"
#include "tcpkali_atomic.h"
#include "tcpkali_rate.h"
#include "tcpkali_expr.h"
#include "tcpkali_dns.h"

long number_of_cpus();
struct randomMessage_t {
	bool isRandomiseMsgLength;
	bool isRandomiseInitMsgLength;
	bool randomizeMsgContent;
	bool randomizeInitMsgContent;
	int randomMinSize;
	int randomMaxSize;
	int randomMinInitSize;
	int randomMaxInitSize;
};

struct engine;

struct engine_params {
	struct randomMessage_t randomMessageParams;
    struct addresses remote_addresses;
    struct addresses listen_addresses;
    struct addresses source_addresses;
    size_t requested_workers;             /* Number of threads to start */
    rate_spec_t channel_send_rate;        /* --channel-upstream */
    rate_spec_t channel_recv_rate;        /* --channel-downstream */
    enum verbosity_level verbosity_level; /* Default verbosity level is 1 */
    enum {
        NSET_UNSET = -1,
        NSET_NODELAY_OFF = 0, /* Enable Nagle */
        NSET_NODELAY_ON = 1,  /* Disable Nagle */
    } nagle_setting;
    enum {
        WRCOMB_OFF = 0, /* Disable write coalescing */
        WRCOMB_ON = 1,  /* Enable write coalescing (default) */
    } write_combine;
    enum {
        LMODE_DEFAULT = 0x00, /* Do not send data, ignore received data */
        LMODE_ACTIVE = 0x01,  /* Actively send messages */
        _LMODE_RCV_MASK = 0xf0,
        _LMODE_SND_MASK = 0x0f,
    } listen_mode;
    uint32_t sock_rcvbuf_size; /* SO_RCVBUF setting */
    uint32_t sock_sndbuf_size; /* SO_SNDBUF setting */
    double connect_timeout;
    double channel_lifetime;
    double epoch;
    int websocket_enable; /* Enable Websocket responder on (-l) */
    /* Pre-computed message data template */
    struct message_collection message_collection;  /* A descr. what to send */
    struct transport_data_spec *data_templates[2]; /* client, server tmpls */
    enum {
        DS_DUMP_ONE_IN = 1,
        DS_DUMP_ONE_OUT = 2,
        DS_DUMP_ONE = 3, /* 2|1 */
        DS_DUMP_ALL_IN = 4,
        DS_DUMP_ALL_OUT = 8,
        DS_DUMP_ALL = 12 /* 8|4 */
    } dump_setting;
    enum {
        LMEASURE_CONNECT = (1 << 0),
        LMEASURE_FIRSTBYTE = (1 << 1),
        LMEASURE_MARKER = (1 << 2)
    } latency_setting;
    tk_expr_t *latency_marker; /* --latency-marker */
    int latency_marker_skip;   /* --latency-marker-skip <N> */

    struct StreamBMH_Occ sbmh_shared_occ; /* Streaming Boyer-Moore-Horspool */
};

struct array_of_doubles {
    size_t size;
    double *doubles;
};

struct engine *engine_start(struct engine_params);
const struct engine_params *engine_params(struct engine *);
void engine_update_message_send_rate(struct engine *, double msg_rate);


/*
 * Report the number of opened connections by categories.
 */
void engine_get_connection_stats(struct engine *, size_t *connecting,
                                 size_t *incoming, size_t *outgoing,
                                 size_t *counter);

/*
 * Snapshot of the current latency.
 */
struct latency_snapshot {
    struct hdr_histogram *connect_histogram;
    struct hdr_histogram *firstbyte_histogram;
    struct hdr_histogram *marker_histogram;
};
void engine_prepare_latency_snapshot(struct engine *);
struct latency_snapshot *engine_collect_latency_snapshot(struct engine *);
void engine_free_latency_snapshot(struct latency_snapshot *);

size_t engine_initiate_new_connections(struct engine *, size_t n);

non_atomic_traffic_stats engine_traffic(struct engine *);

void engine_terminate(struct engine *, double epoch_start,
                      /* Traffic observed during ramp-up phase */
                      non_atomic_traffic_stats initial_traffic,
                      /* Report latencies at specified %'iles */
                      struct array_of_doubles want_latency_percentiles);

#endif /* TCPKALI_ENGINE_H */
