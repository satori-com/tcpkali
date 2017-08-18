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
#include <stdio.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>

#include "tcpkali_websocket.h"
#include "libcows_base64.h"
#include "sha-1.c"


/*
 * RFC 6455, 5.5:
 * Control frames are identified by opcodes where the most significant
 * bit of the opcode is 1.
 */
#define IS_WS_CONTROL_FRAME(op) ((((unsigned)(op)) & 0x8) ? 1 : 0)

/*
 * Write out a frame header to prefix a payload of given size.
 */
size_t
websocket_frame_header(uint8_t *buf, size_t size, enum websocket_side side,
                       enum ws_frame_opcode opcode, int reserved, int fin,
                       size_t payload_size) {
    uint8_t tmpbuf[WEBSOCKET_MAX_FRAME_HDR_SIZE];
    uint8_t *orig_buf_ptr;

    /* Return the frame header size if there's no buffer to write to. */
    if(buf) {
        /* Buf should be able to contain the largest frame header. */
        assert(size >= WEBSOCKET_MAX_FRAME_HDR_SIZE);
        orig_buf_ptr = buf;
    } else {
        orig_buf_ptr = buf = tmpbuf;
        size = sizeof(tmpbuf);
    }

    struct ws_frame {
        enum ws_frame_opcode opcode : 4;
        unsigned int rsvs : 3;
        unsigned int fin : 1;
    } first_byte = {.opcode = opcode, .rsvs = reserved, .fin = (fin != 0)};

    *buf++ = *(uint8_t *)&first_byte;

    /* Mask MUST be present in C->S (RFC) */
    const unsigned char mask_flag = (side == WS_SIDE_CLIENT) ? 0x80 : 0;

    if(payload_size <= 125) {
        *buf++ = mask_flag | payload_size;
    } else if(payload_size <= 65535) {
        *buf++ = mask_flag | 126;
        uint16_t network_order_size = htons(payload_size);
        memcpy(buf, &network_order_size, 2);
        buf += 2;
    } else if(sizeof(payload_size) <= sizeof(uint32_t)) {
        *buf++ = mask_flag | 127;
        memset(buf, 0, 4);
        buf += 4;
        uint32_t network_order_size = htonl(payload_size);
        memcpy(buf, &network_order_size, 4);
        buf += 4;
    } else {
        /* (>>32) won't work if payload_size is uint32. */
        *buf++ = mask_flag | 127;
        uint32_t hi = htonl(payload_size >> 32);
        memcpy(buf, &hi, 4);
        buf += 4;
        uint32_t lo = htonl(payload_size & 0xffffffff);
        memcpy(buf, &lo, 4);
        buf += 4;
    }

    /* Add 4-byte 0-valued XOR mask (for debugging) */
    if(mask_flag) {
        memset(buf, 0, 4);
        buf += 4;
    }

    return buf - orig_buf_ptr;
}


/*
 * Detect WebSocket handshake.
 * A high performance, but extremely naive and broken implementation.
 * Normally a well-behaving client (or proxy) will attempt to send a whole HTTP
 * request in a single TCP fragment. In some cases, a header-per-fragment
 * is also something that naive clients do.
 * This function does not support HTTP headers split between frames arbitrarily,
 * which is not too common in the high performance code anyway.
 * TODO: replace with an efficient but more correct parser.
 */
http_detect_websocket_rval
http_detect_websocket(const char *buf, size_t size, char *out_buf,
                      size_t out_buf_sz, size_t *response_size) {
    const char *keyhdr = "sec-websocket-key:";
    size_t keyhdr_size = sizeof("sec-websocket-key:") - 1;

    /*
     * Ignore the GET/Version completely, search right for Sec-WebSocket-Key.
     */
    for(const char *bend = buf + size; buf < bend; buf++) {
        if(*buf != '\n') {
            continue;
        } else {
            buf++;
            size_t buf_remainder = (bend - buf);
            if(buf_remainder < keyhdr_size) {
                return HDW_TRUNCATED_INPUT;
            }

            if(strncasecmp(buf, keyhdr, keyhdr_size) == 0) {
                const char *keyvalue = buf + keyhdr_size;
                int extent = bend - keyvalue;
                if(extent > 30) extent = 30;
                const char *kvend = memchr(keyvalue, '\n', extent);
                if(kvend == 0) {
                    return HDW_TRUNCATED_INPUT;
                }
                /* ltrim */
                for(; keyvalue < kvend && *keyvalue == ' '; keyvalue++)
                    ;
                /* rtrim */
                for(; kvend > keyvalue && (kvend[-1] == ' ' || kvend[-1] == '\r'
                                           || kvend[-1] == ' ');
                    kvend--)
                    ;
                int kvsize = kvend - keyvalue;
                if(kvsize < 1 || kvsize > 26 /* 26 is in RFC6455 */)
                    return HDW_UNEXPECTED_ERROR;

                char new_key[64];
                char sha1_buf[20];
                char base64_output[32];
                size_t new_key_size;
#define MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
                memcpy(new_key, keyvalue, kvsize);
                strcpy(new_key + kvsize, MAGIC);
                new_key_size = kvsize + sizeof(MAGIC) - 1;

                SHA1((void *)new_key, new_key_size, (unsigned char *)sha1_buf);
                size_t base64_output_size = sizeof(base64_output);
                *response_size = snprintf(
                    out_buf, out_buf_sz,
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: %s\r\n"
                    "\r\n",
                    libcows_base64_encode(sha1_buf, sizeof(sha1_buf),
                                          base64_output, &base64_output_size));
                assert(*response_size < out_buf_sz);

                /* Write out WebSocket response */
                /*
                if(write(fd, out_buf, response_size) != response_size)
                    return HDW_UNEXPECTED_ERROR;
                */

                return HDW_WEBSOCKET_DETECTED;
            }
        }
    }

    return HDW_NOT_ENOUGH_DATA;
}
