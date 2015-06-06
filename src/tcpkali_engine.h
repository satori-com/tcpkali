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

#include "tcpkali_transport.h"
#include "tcpkali_atomic.h"
#include "tcpkali_rate.h"
#include "tcpkali_expr.h"

long number_of_cpus();

struct engine;

struct engine_params {
    struct addresses remote_addresses;
    struct addresses listen_addresses;
    size_t requested_workers;       /* Number of threads to start */
    rate_spec_t channel_send_rate;  /* --channel-bandwidth or --message-rate */
    enum {
        DBG_ALWAYS  = 0,
        DBG_ERROR   = 0,
        DBG_NORMAL  = 1,    /* Default verbosity level */
        DBG_WARNING = 1,    /* Deliberately the same as "normal" */
        DBG_DETAIL  = 2,    /* Increased verbosity */
        DBG_DATA    = 3,    /* Dump incoming and outgoing data as well */
        _DBG_MAX
    } verbosity_level;      /* Default verbosity level is 1 */
    enum {
        NSET_UNSET = -1,
        NSET_NODELAY_OFF = 0,  /* Enable Nagle */
        NSET_NODELAY_ON = 1,   /* Disable Nagle */
    } nagle_setting;
    enum {
        LMODE_DEFAULT   = 0x00,   /* Do not send data, ignore received data */
        LMODE_ACTIVE    = 0x01,   /* Actively send messages */
        _LMODE_RCV_MASK = 0xf0,
        _LMODE_SND_MASK = 0x0f,
    } listen_mode;
    uint32_t sock_rcvbuf_size;        /* SO_RCVBUF setting */
    uint32_t sock_sndbuf_size;        /* SO_SNDBUF setting */
    double connect_timeout;
    double channel_lifetime;
    double epoch;
    int    websocket_enable;        /* Enable Websocket responder on (-l) */
    /* Pre-computed message data template */
    struct message_collection    message_collection; /* A descr. what to send */
    struct transport_data_spec  *data_templates[2]; /* client, server tmpls */
    tk_expr_t *latency_marker;      /* --latency-marker */
    int        latency_marker_skip;    /* --latency-marker-skip <N> */
    struct StreamBMH_Occ sbmh_shared_occ;  /* Streaming Boyer-Moore-Horspool */
};

struct engine *engine_start(struct engine_params);


/*
 * Report the number of opened connections by categories.
 */
void engine_get_connection_stats(struct engine *,
    size_t *connecting, size_t *incoming, size_t *outgoing, size_t *counter);
struct hdr_histogram *engine_get_latency_stats(struct engine *);
void engine_traffic(struct engine *, non_atomic_wide_t *sent, non_atomic_wide_t *received);


size_t engine_initiate_new_connections(struct engine *, size_t n);

void engine_terminate(struct engine *, double epoch_start,
    non_atomic_wide_t initial_data_sent,    /* Data sent during ramp-up */
    non_atomic_wide_t initial_data_received /* Data received during ramp-up */
    );

#endif  /* TCPKALI_ENGINE_H */
