/*
 * Copyright (c) 2014  Machine Zone, Inc.
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

#include "tcpkali_transport.h"

long number_of_cpus();

struct engine;

struct engine_params {
    struct addresses remote_addresses;
    struct addresses listen_addresses;
    size_t requested_workers;       /* Number of threads to start */
    size_t channel_bandwidth_Bps;   /* Single channel bw, bytes per second. */
    size_t minimal_write_size;
    enum {
        DBG_ALWAYS,
        DBG_ERROR,
        DBG_DETAIL,
        DBG_DATA,       /* Dump incoming and outgoing data as well */
        _DBG_MAX
    } verbosity_level;
    double connect_timeout;
    double channel_lifetime;
    double epoch;
    int    websocket_enable;        /* Enable Websocket responder on (-l) */
    /* Pre-computed message data */
    struct transport_data_spec data;
};

struct engine *engine_start(struct engine_params);


/*
 * Report the number of opened connections by categories.
 */
void engine_connections(struct engine *, size_t *connecting, size_t *incoming, size_t *outgoing, size_t *counter);
void engine_traffic(struct engine *, size_t *sent, size_t *received);


size_t engine_initiate_new_connections(struct engine *, size_t n);

void engine_terminate(struct engine *, double epoch_start,
    size_t initial_data_sent,       /* Data sent during ramp-up */
    size_t initial_data_received    /* Data received during ramp-up */
    );

#endif  /* TCPKALI_ENGINE_H */
