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

/*
 * Reference: http://tools.ietf.org/html/rfc6455
 */
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "buffers.h"
#include "sha-1.c"
#include "libcows_base64.h"
#include "libcows_common.h"
#include "libcows_frame.h"

#include "libcows.h"

/*
 * Kick off writing of the output chain.
 * May return LCERR_OK or LCERR_BROKEN_CONNECTION. In the latter case,
 * the requester shall destroy the context.
 */
static libcows_send_error Write(libcows_ctx *);

/*
 * Perform unmasking of the given buffer, with state across split unmasking.
 */
typedef struct {
    unsigned char *mask4;
    unsigned char state;
} libcows_unmask_key;
static void libcows_unmask(void *data, size_t, void *key);

struct libcows_ctx {
    enum {
        LCTX_EMBRYONIC,
        LCTX_HEALTHY,
        LCTX_BROKEN,    /* The connection is broken, waiting to GC */
        LCTX_ZOMBIE,    /* Deallocated already. */
    } state;
    struct libcows_callbacks *callbacks;
    void *opaque_key;
    /* I/O buffers */
    struct {
        cbuf_chain *payload;    /* Unfinished payload */
        cbuf_chain *inbuf;
        int payload_limit;      /* Max unfinished payload length. */
    } in_state;
    struct {
        int out_limit;          /* Max output buffers length. */
        cbuf_chain *outbuf;
    } out_state;
};

/*
 * Check payload length.
 */
int libcows_set_max_payload_size(libcows_ctx *ctx, int limit) {
    int old_limit = ctx->in_state.payload_limit;
    if(limit >= 0) ctx->in_state.payload_limit = limit;
    return old_limit;
}

/*
 * Do not blow up if receiver is slow.
 */
int libcows_set_outgoing_buffer_limit(libcows_ctx *ctx, int limit) {
    int old_limit = ctx->out_state.out_limit;
    if(limit >= 0) ctx->out_state.out_limit = limit;
    return old_limit;
}

libcows_ctx *
libcows_context_new(const char *sec_websocket_version, const char *sec_websocket_key, const char *more_http_headers, struct libcows_callbacks *callbacks, void *opaque_key) {
    char new_key[64];
    char base64_output[32];
    size_t size, new_key_size;
    libcows_ctx *ctx;
    char *out_buf;

    /*
     * Sec-WebSocket-Version.
     * WebSockets version control.
     */
    if(strncmp(sec_websocket_version, "13", 2) == 0
        && !isdigit(sec_websocket_version[2])) {
        /* We know how to handle this version */
    } else {
        /* ...no, we don't */
        return NULL;
    }

    /*
     * Sec-WebSocket-Key.
     * Trim the key by removing potential whitespace on both sides.
     */
    while(isspace(*sec_websocket_key))
        sec_websocket_key++;
    for(size = strlen(sec_websocket_key);
            size > 0 && isspace(sec_websocket_key[size-1]); size--);
    /* rfc6455#4.1, 7: 16-byte value base64-encoded, makes it 24 bytes. */
    if(size < 1 || size > 24)
        return NULL;

#define MAGIC   "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    assert(sizeof(new_key) > 24 + sizeof(MAGIC));
    memcpy(new_key, sec_websocket_key, size);
    strcpy(new_key+size, MAGIC);
    new_key_size = size + sizeof(MAGIC)-1;

    ctx = calloc(1, sizeof(*ctx));
    ctx->in_state.inbuf = 0;  /* We do not allocate unless ready to receive */
    ctx->in_state.payload_limit = 65535;    /* Default is 64k */
    ctx->out_state.out_limit = 1024 * 1024;  /* Default is 1M */
    ctx->out_state.outbuf = cbuf_chain_new(4000);
    ctx->state = LCTX_EMBRYONIC;

    out_buf = cbuf_chain_get_write_chunk(ctx->out_state.outbuf, &size, 4000);
    assert(out_buf);
    assert(size == 4000);

    char sha1_buf[20];
    SHA1((void *)new_key, new_key_size, (unsigned char *)sha1_buf);
    size_t base64_output_size = sizeof(base64_output);
    size = snprintf(out_buf, size,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "%s"
        "\r\n",
        libcows_base64_encode(sha1_buf, sizeof(sha1_buf),
            base64_output, &base64_output_size),
        more_http_headers
    );
    cbuf_chain_advance_write_ptr(ctx->out_state.outbuf, size);

    ctx->callbacks = callbacks;
    ctx->opaque_key = opaque_key;

    /*
     * Kick writing off. If the first write isn't valid (connection error),
     * no point of continuing with this WS context.
     */
    switch(Write(ctx)) {
    case LCERR_OK:
        ctx->state = LCTX_HEALTHY;
        return ctx;
    case LCERR_BROKEN_CONNECTION:
        libcows_context_free(ctx);
        return NULL;
    case LCERR_QUOTA_EXCEEDED:
    default:
        assert(!"Unreachable: Write() does not check quota");
    }
}

/*
 * Process the Websocket frames
 */
static void libcows_assemble_frames(libcows_ctx *ctx) {

    while(cbuf_chain_get_buffered_data_size(ctx->in_state.inbuf) > 0) {

        struct libcows_ws_frame frame;
        size_t csize;
        void *bytes;

        /*
         * Get the frame header.
         */
        bytes = cbuf_chain_get_read_chunk(ctx->in_state.inbuf, &csize);
        ssize_t frame_hdr_len;
        do {
            frame_hdr_len = libcows_ws_frame_fill(&frame, bytes, csize);
            if(frame_hdr_len == 0) {
                /*
                 * We might have the frame header split between buffers, join them.
                 */
                if(cbuf_chain_flatten(ctx->in_state.inbuf) == 0) {
                    /* Oops, we didn't gain any more data by joining buffers... */
                    return;
                }
            }
        } while(frame_hdr_len == 0);

        /*
         * Figure out if needed to await the full body.
         */
        if(frame.payload_length > ctx->in_state.payload_limit
            && ctx->in_state.payload_limit) {
            ctx->callbacks->finish(ctx, LFR_PAYLOAD_LIMIT);
            return;
        }

        /*
         * Check if this is a control frame, and if so, verify its correctness
         * against rule in RFC 6455, #5.5.
         */
        if(frame.opcode & 0x8) {
            if(frame.payload_length <= 125 && frame.fin) {
                /* Received proper control frame. */
                cbuf_chain_advance_read_ptr(ctx->in_state.inbuf, frame_hdr_len);
                switch(frame.opcode) {
                case WSOP_CLOSE:
                    ctx->callbacks->finish(ctx, LFR_CLIENT_CLOSE);
                    return;
                case WSOP_PING:
                    ctx->callbacks->got_websocket_control(ctx, LCO_PING);
                    break;
                case WSOP_PONG:
                    ctx->callbacks->got_websocket_control(ctx, LCO_PONG);
                    break;
                default:
                    /* Fall through */
                    break;
                }
                continue;
            } else {
                ctx->callbacks->finish(ctx, LFR_BROKEN_CONTROL);
                return;
            }
        }

        /*
         * If there is less data came than expected payload size,
         * wait for more.
         */
        if(frame_hdr_len + frame.payload_length > cbuf_chain_get_buffered_data_size(ctx->in_state.inbuf)) {
            return;
        }

        /*
         * Separate the raw data into a special in_state.payload chain.
         */
        if(!ctx->in_state.payload) ctx->in_state.payload = cbuf_chain_new(1500);
        cbuf_chain_advance_read_ptr(ctx->in_state.inbuf, frame_hdr_len);
        cbuf_chain_move(ctx->in_state.inbuf, ctx->in_state.payload,
                        frame.payload_length,
                        libcows_unmask,
                        &(libcows_unmask_key){ .mask4 = frame.mask });

        if(frame.fin) {
            size_t tmp, chunk_size;
            void *buffer;

            /* Leave space for courtesy '\0' */
            cbuf_chain_get_write_chunk(ctx->in_state.payload, &tmp, 1);
            cbuf_chain_advance_write_ptr(ctx->in_state.payload, 1);

            cbuf_chain_flatten(ctx->in_state.payload);
            buffer = cbuf_chain_get_read_chunk(ctx->in_state.payload, &chunk_size);
            ((char *)buffer)[chunk_size-1] = '\0';
            ctx->callbacks->got_websocket_frame(ctx, buffer, chunk_size - 1);
            cbuf_chain_advance_read_ptr(ctx->in_state.payload, chunk_size);
        }
    }

    /*
     * Destroy buffers until the next receive.
     */
    cbuf_chain_free(ctx->in_state.inbuf);
    ctx->in_state.inbuf = 0;
    cbuf_chain_free(ctx->in_state.payload);
    ctx->in_state.payload = 0;
}

/*
 * Received an asynchronous I/O event from an event subsystem.
 */
void libcows_io_event(libcows_ctx *ctx, int read_ready, int write_ready) {

    if(write_ready) {
        if(Write(ctx) == LCERR_BROKEN_CONNECTION) {
            ctx->callbacks->finish(ctx, LFR_CONNECTION_ERROR);
            return;
        }
    }

    if(read_ready) {
        size_t avail;
        void *buf;

        if(!ctx->in_state.inbuf) ctx->in_state.inbuf = cbuf_chain_new(1500);

        buf = cbuf_chain_get_write_chunk(ctx->in_state.inbuf, &avail, 1);
        ssize_t rd = ctx->callbacks->low_level_read(ctx, buf, avail);
        switch(rd) {
        case -1:
            /* Retry */
            return;
        case 0:
            ctx->callbacks->finish(ctx, LFR_CLIENT_CLOSE);
            return;
        default:
            cbuf_chain_advance_write_ptr(ctx->in_state.inbuf, rd);
            libcows_assemble_frames(ctx);
            if(ctx->state == LCTX_BROKEN) {
                ctx->state = LCTX_ZOMBIE;
                ctx->callbacks->finish(ctx, LFR_CLIENT_CLOSE);
            }
        }
    }
}

/*
 * Get opaque key, which might be a file descriptor in disguise
 * or a more complex context.
 */
void *libcows_context_get_opaque_key(libcows_ctx *ctx) {
    return ctx->opaque_key;
}

static void libcows_queue_hdr(libcows_ctx *ctx, size_t size) {
    struct libcows_ws_frame frame = { .opcode = WSOP_TEXT, .fin = 1 };
    unsigned char fhdr_buf[2 + 8];

    if(!ctx->out_state.outbuf)
        ctx->out_state.outbuf = cbuf_chain_new(1500);

    if(size < 126) {
        fhdr_buf[0] = *(unsigned char *)&frame;
        fhdr_buf[1] = size;
        cbuf_chain_add_bytes(fhdr_buf, 2, ctx->out_state.outbuf);
    } else if(size <= 65536) {
        fhdr_buf[0] = *(unsigned char *)&frame;
        fhdr_buf[1] = 126;
        fhdr_buf[2] = size >> 8;
        fhdr_buf[3] = size;
        cbuf_chain_add_bytes(fhdr_buf, 4, ctx->out_state.outbuf);
    } else {
        fhdr_buf[0] = *(unsigned char *)&frame;
        fhdr_buf[1] = 127;
        fhdr_buf[2] = size >> 56;
        fhdr_buf[3] = size >> 48;
        fhdr_buf[4] = size >> 40;
        fhdr_buf[5] = size >> 32;
        fhdr_buf[6] = size >> 24;
        fhdr_buf[7] = size >> 16;
        fhdr_buf[8] = size >> 8;
        fhdr_buf[9] = size;
        cbuf_chain_add_bytes(fhdr_buf, 10, ctx->out_state.outbuf);
    }
}

/*
 * Send data to the client.
 */
libcows_send_error libcows_send(libcows_ctx *ctx, void *data, size_t size) {

    /*
     * Check limits to avoid blowing up on slow clients.
     * We don't account for (size) here to allow single frame transfers
     * exceeding the buffer limit.
     */
    if(cbuf_chain_get_buffered_data_size(ctx->out_state.outbuf)
            > ctx->out_state.out_limit && ctx->out_state.out_limit > 0) {
        return LCERR_QUOTA_EXCEEDED;
    }

    libcows_queue_hdr(ctx, size);

    cbuf_chain_add_bytes(data, size, ctx->out_state.outbuf);

    return Write(ctx);  /* Returns _OK or _BROKEN_CONNECTION */
}

libcows_send_error libcows_sendv(libcows_ctx *ctx, struct iovec *iov, int iovcnt) {
    size_t size = 0;
    int i;

    /*
     * Check limits to avoid blowing up on slow clients by accumulating
     * too much data. We don't account for (size) here to allow large
     * single frame transfers exceed the ougoing buffer limit.
     */
    if(cbuf_chain_get_buffered_data_size(ctx->out_state.outbuf)
            > ctx->out_state.out_limit && ctx->out_state.out_limit > 0) {
        return LCERR_QUOTA_EXCEEDED;
    }

    for(i = 0; i < iovcnt; i++)
        size += iov[i].iov_len;

    libcows_queue_hdr(ctx, size);

    for(i = 0; i < iovcnt; i++) {
        cbuf_chain_add_bytes(iov[i].iov_base, iov[i].iov_len,
            ctx->out_state.outbuf);
    }

    return Write(ctx);  /* Returns _OK or _BROKEN_CONNECTION */
}

/*
 * Tear down the context without going through the shutdown sequence.
 */
void libcows_context_free(libcows_ctx *ctx) {
    if(ctx) {
        cbuf_chain_free(ctx->in_state.payload);
        cbuf_chain_free(ctx->in_state.inbuf);
        cbuf_chain_free(ctx->out_state.outbuf);
        ctx->state = LCTX_ZOMBIE;
        free(ctx);
    }
}

/*
 * Internal functions.
 */
static libcows_send_error Write(libcows_ctx *ctx) {

    if(ctx->state == LCTX_BROKEN)
        return LCERR_BROKEN_CONNECTION;

    if(!ctx->out_state.outbuf) {
        ctx->callbacks->register_io_interest(ctx, 1, 0);
        return LCERR_OK;
    }

    for(;;) {
        size_t chunk_size;
        void *buf = cbuf_chain_get_read_chunk(ctx->out_state.outbuf, &chunk_size);
        if(!chunk_size) {
            cbuf_chain_free(ctx->out_state.outbuf);
            ctx->out_state.outbuf = 0;
            ctx->callbacks->register_io_interest(ctx, 1, 0);
            break;
        }

        ssize_t wrote = ctx->callbacks->low_level_write(ctx, buf, chunk_size);
        switch(wrote) {
        case -1:
            ctx->callbacks->register_io_interest(ctx, 0, 0);
            ctx->state = LCTX_BROKEN;
            return LCERR_BROKEN_CONNECTION;
        case 0:
            ctx->callbacks->register_io_interest(ctx, 1, 1);
            return LCERR_OK;
        default:
            cbuf_chain_advance_read_ptr(ctx->out_state.outbuf, wrote);
            continue;
        }
    }

    return LCERR_OK;
}

/*
 * Perform client data unmasking.
 */
static void libcows_unmask(void *data, size_t size, void *keyp) {
    libcows_unmask_key *key = keyp;
    unsigned long mask;
    size_t i;

    /*
     * Optimized path.
     */
    if(size >= sizeof(mask)) {

        size_t left_unaligned_bytes = sizeof(mask)
                            - ((uintptr_t)data & (sizeof(mask)-1));
        if(left_unaligned_bytes < sizeof(mask)) {
            for(i = 0; i < left_unaligned_bytes; i++)
                ((unsigned char *)data)[i] ^= key->mask4[(i + key->state) % 4];
            data += left_unaligned_bytes;
            size -= left_unaligned_bytes;
            key->state = (key->state + left_unaligned_bytes) % 4;
        }

        /* Prepare a long mask. */
        for(i = 0; i < sizeof(mask); i++) {
            ((unsigned char *)&mask)[i] = key->mask4[(key->state + i) % 4];
        }
        /* Prepare a long mask. */

        unsigned long *dlong = data;
        unsigned long *dlend = dlong + size/sizeof(mask);
        for(; dlong < dlend; dlong++) {
            (*dlong) ^= mask;
        }

        data += size - (size%sizeof(mask));
        size %= sizeof(mask);
        /* Since we roll 4 or 8 bytes, key->state%4 remains the same. */
        /* Follow up with a slow path */
    }

    /*
     * Slow path.
     */
    for(i = 0; i < size; i++) {
        ((unsigned char *)data)[i] ^= key->mask4[(i + key->state) % 4];
    }

    key->state = (size + key->state) % 4;
}
