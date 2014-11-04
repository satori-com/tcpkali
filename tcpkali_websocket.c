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
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "tcpkali_websocket.h"

/*
 * Write out a frame header to prefix a payload of given size.
 */
size_t websocket_frame_header(size_t payload_size, uint8_t *buf, size_t size) {
    uint8_t *orig_buf_ptr = buf;

    /* Buf should be able to contain the largest frame header. */
    assert(2 + 8 <= size);

    if(!payload_size) return 0;

    struct ws_frame {
        enum {
            OP_CONTINUATION = 0x0,
            OP_TEXT_FRAME   = 0x1,
            OP_BINARY_FRAME = 0x2,
            OP_CLOSE        = 0x8,
            OP_PING         = 0x9,
            OP_PONG         = 0xA
        } opcode:4;
        unsigned int rsvs:3;
        unsigned int fin:1;
    } first_byte = {
        .opcode = OP_TEXT_FRAME,
        .fin = 1
    };
    assert(sizeof(first_byte) == 1);

    *buf++ = *(uint8_t *)&first_byte;

    if(payload_size <= 125) {
        *buf++ = payload_size;
    } else if(payload_size <= 65535) {
        *buf++ = 126;
        uint16_t network_order_size = htonl(payload_size);
        memcpy(buf, &network_order_size, 2);
        buf += 2;
    } else if(sizeof(payload_size) <= sizeof(uint32_t)) {
        *buf++ = 127;
        memset(buf, 0, 4);
        buf += 4;
        uint32_t network_order_size = htonl(payload_size);
        memcpy(buf, &network_order_size, 4);
        buf += 4;
    } else {
        /* (>>32) won't work if payload_size is uint32. */
        *buf++ = 127;
        uint32_t hi = htonl(payload_size >> 32);
        memcpy(buf, &hi, 4);
        buf += 4;
        uint32_t lo = htonl(payload_size & 0xffffffff);
        memcpy(buf, &lo, 4);
        buf += 4;
    }

    return buf - orig_buf_ptr;
}

