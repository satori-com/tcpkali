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
#ifndef TCPKALI_WEBSOCKET_H
#define TCPKALI_WEBSOCKET_H

#include "tcpkali_common.h"

/*
 * Establish the maximum size of the WebSocket frame header.
 */
#define WEBSOCKET_MAX_FRAME_HDR_SIZE (2 + 8 + 4)

/*
 * Available WebSocket frame opcodes.
 */
enum ws_frame_opcode {
    WS_OP_CONTINUATION = 0x0,
    WS_OP_TEXT_FRAME = 0x1,
    WS_OP_BINARY_FRAME = 0x2,
    WS_OP_CLOSE = 0x8,
    WS_OP_PING = 0x9,
    WS_OP_PONG = 0xA
};

/*
 * Write out a frame header to prefix a payload of given size.
 * RETURN VALUE:
 *   Number of bytes written out to the buffer.
 */
enum websocket_side {
    WS_SIDE_CLIENT,
    WS_SIDE_SERVER,
};
size_t websocket_frame_header(uint8_t *buf, size_t size, enum websocket_side,
                              enum ws_frame_opcode, int reserved, int fin,
                              size_t payload_size);

/*
 * Detect the Websocket handshake in the stream and accept the handshake.
 */
typedef enum {
    HDW_NOT_ENOUGH_DATA,
    HDW_WEBSOCKET_DETECTED,
    HDW_TRUNCATED_INPUT,
    HDW_UNEXPECTED_ERROR,
} http_detect_websocket_rval;
http_detect_websocket_rval http_detect_websocket(const char *, size_t, char *,
                                                 size_t, size_t *);

#endif /* TCPKALI_WEBSOCKET_H */
