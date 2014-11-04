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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/uio.h>
#include <assert.h>

#include "tcpkali_websocket.h"
#include "tcpkali_transport.h"

static size_t iovecs_length(struct iovec *iovs, size_t iovl) {
    size_t size = 0;
    for(size_t i = 0; i < iovl; i++)
        size += iovs[i].iov_len;
    return size;
}

/*
 * Add transport specific framing and initialize the engine params members.
 * No framing in case of TCP. HTTP + WebSocket framing in case of websockets.
 */
struct transport_data_spec add_transport_framing(struct iovec *iovs, size_t iovh, size_t iovl, int websocket_enable) {
    assert(iovh <= iovl);

    if(websocket_enable) {
        static const char ws_http_headers[] =
            "GET /ws HTTP/1.1\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        struct iovec ws_iovs[1 + 2 * iovl];
        uint8_t ws_framing_prefixes[WEBSOCKET_MAX_FRAME_HDR_SIZE * iovl];
        uint8_t *wsp = ws_framing_prefixes;
        ws_iovs[0].iov_base = (void *)ws_http_headers;
        ws_iovs[0].iov_len = sizeof(ws_http_headers)-1;
        for(size_t i = 0; i < iovl; i++) {
            uint8_t *old_wsp = wsp;
            wsp += websocket_frame_header(iovs[i].iov_len, wsp,
                                          (size_t)(wsp - ws_framing_prefixes));
            /* WS header */
            ws_iovs[1 + 2*i].iov_base = old_wsp;
            ws_iovs[1 + 2*i].iov_len = wsp - old_wsp;
            /* WS data */
            memcpy(&ws_iovs[1 + 2*i + 1], &iovs[i], sizeof(iovs[0]));
        }
        assert((wsp - ws_framing_prefixes)
            <= (ssize_t)sizeof(ws_framing_prefixes));

        return add_transport_framing(ws_iovs, 1 + 2*iovh, 1 + 2*iovl, 0);
    } else {
        /* Straight plain flat TCP with no framing and back-to-back messages. */
        char *p;
        size_t header_size = iovecs_length(iovs, iovh);
        size_t total_size = iovecs_length(iovs, iovl);
        p = malloc(total_size + 1);
        assert(p);

        struct transport_data_spec data;
        memset(&data, 0, sizeof(data));
        data.ptr = p;
        data.header_size = header_size;
        data.total_size = total_size;

        for(size_t i = 0; i < iovl; i++) {
            memcpy(p, iovs[i].iov_base, iovs[i].iov_len);
            p += iovs[i].iov_len;
        }
        p[0] = '\0';

        return data;
    }
}


/*
 * If the payload is less then target_size,
 * replicate it several times so the total buffer exceeds target_size.
 */
void replicate_payload(struct transport_data_spec *data, size_t target_size) {
    size_t payload_size = data->total_size - data->header_size;

    if(!payload_size) {
        /* Can't blow up an empty buffer. */
    } else if(payload_size >= target_size) {
        /* Data is large enough to avoid blowing up. */
    } else {
        /* The optimum target_size is size(L2)/k */
        size_t n = ceil(((double)target_size)/payload_size);
        size_t new_payload_size = n * payload_size;
        size_t header_offset = data->header_size;
        char *p = realloc(data->ptr, header_offset + new_payload_size + 1);
        void *msg_data = p + header_offset;
        assert(p);
        for(size_t i = 1; i < n; i++) {
            memcpy(&p[header_offset + i * payload_size], msg_data, payload_size);
        }
        p[header_offset + new_payload_size] = '\0';
        data->ptr = p;
        data->total_size = header_offset + new_payload_size;
    }
}

