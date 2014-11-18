/*
 * Copyright (c) 2013 Jacknyfe, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
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

#include <string.h>

#include "libcows_frame.h"

ssize_t
libcows_ws_frame_fill(struct libcows_ws_frame *frame, void *buf, size_t size) {
    const unsigned char *b = buf;

    if(size < 2) return 0;

    *(unsigned char *)frame = b[0];
    int mask_len = (b[1] & 0x80) >> 5;
    int len7 = b[1] & 0x7f;
    int extra_payload_len = ((len7<126)?0:(len7==127?8:2));
    int frame_hdr_len = 2 + extra_payload_len + mask_len;

    if(size < frame_hdr_len) {
        return 0;
    } else {
        switch(extra_payload_len) {
        case 0: frame->payload_length = len7; break;
        case 2: frame->payload_length = (b[2] << 8) | b[3]; break;
        case 4:
            frame->payload_length =
                  ((uint64_t)b[2] << 56) | ((uint64_t)b[3] << 48)
                | ((uint64_t)b[4] << 40) | ((uint64_t)b[5] << 32)
                | (b[6] << 24) | (b[7] << 16) | (b[8] << 8)  | (b[9] << 0);
        }
        if(mask_len)
            memcpy(frame->mask, b + 2 + extra_payload_len, 4);
        else
            memset(frame->mask, 0, 4);

        return frame_hdr_len;
    }
}
