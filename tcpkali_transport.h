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
#ifndef TCPKALI_TRANSPORT_H
#define TCPKALI_TRANSPORT_H

/*
 * Our send buffer is pre-computed in advance and shared between
 * the instances of engine. The buffer contains both headers and
 * payload. The data_header_size is used to determine the end of
 * the headers and start of the payload.
 */
struct transport_data_spec {
    void  *ptr;
    size_t header_size; /* Part of data to send just once. */
    size_t total_size;
};


/*
 * Create the data specification by adding transport specific framing.
 */
struct transport_data_spec add_transport_framing(struct iovec *iovs,
        size_t iovs_header, size_t iovs_total,
        int websocket_enable);

/*
 * To be able to efficiently transfer small payloads, we replicate
 * the payload data several times to send more data in a single call.
 * The function replaces the given (data) contents.
 */
void replicate_payoad(struct transport_data_spec *data, size_t target_payload_size);

#endif  /* TCPKALI_TRANSPORT_H */
