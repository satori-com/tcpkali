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

#ifndef LIBCOWS_H
#define LIBCOWS_H

/* Some per-client context */
typedef struct libcows_ctx libcows_ctx;

enum libcows_finish_reason {
    LFR_CLIENT_CLOSE,       /* Client initiated close. */
    LFR_PAYLOAD_LIMIT,      /* Payload greater than global maximum. */
    LFR_BROKEN_CONTROL,     /* Invalid control frame. */
    LFR_CONNECTION_ERROR,   /* Connection is broken. */
};

enum libcows_control_op {
    LCO_PING,
    LCO_PONG,
};

/*
 * The newly formed context needs to be set up with various callbacks
 * to let the caller manage file descriptors, etc.
 * The (low_level_read) and (low_level_write) shall conform to POSIX
 * read(2) and write(2) call semantics.
 * When (finish) callback has been called, the caller's next and last
 * call must be libcows_context_free().
 */
struct libcows_callbacks {
        /* A new websocket frame has been received. */
        void (*got_websocket_frame)(libcows_ctx *, char *data, size_t size);
        /* Received ping or pong control frame. */
        void (*got_websocket_control)(libcows_ctx *, enum libcows_control_op);
        /* These callbacks are called when libcows wants to receive or send */
        ssize_t (*low_level_read)(libcows_ctx *, void *buf, size_t size);
        ssize_t (*low_level_write)(libcows_ctx *, void *buf, size_t size);
        /* libcows can indicate interest in I/O events */
        void (*register_io_interest)(libcows_ctx *, int want_read, int want_write);
        /* Nothing to do but to call libcows_context_free(). */
        void (*finish)(libcows_ctx *, enum libcows_finish_reason);
};

/*
 * Create a new client out of an existing HTTP-handshaked connection.
 * 
 * Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==
 * Sec-WebSocket-Protocol: chat
 * Sec-WebSocket-Version: 13
 *
 * RETURN VALUES:
 *  !0: The library decided to establish the server-side websockets connection.
 *   0: The library refused to establish the connection (protocol mismatch?).
 */
libcows_ctx *libcows_context_new(
                const char *sec_websocket_version,
                const char *sec_websocket_key,
                const char *more_http_headers,
                struct libcows_callbacks *callbacks,
                void *opaque_key
            );

/*
 * Received asynchronous I/O event from an event subsystem.
 */
void libcows_io_event(libcows_ctx *ctx, int read_ready, int write_ready);

/*
 * Get opaque key, which might be a file descriptor in disguise
 * or a more complex context.
 */
void *libcows_context_get_opaque_key(libcows_ctx *);

/*
 * Avoid denial of service by limiting the amount of data. Default is 64k.
 * Returns old limit value.
 */
int libcows_set_max_payload_size(libcows_ctx *, int limit);

/*
 * Avoid overwhelming the receiver by skipping the messages if
 * the receiver is slow and our buffers overfill. Default is no
 * limiting. If limiting is enabled, the libcows_send/sendv will return
 * -1 and no frame is going to be added to the outgoing buffer.
 * Returns old limit value.
 */
int libcows_set_outgoing_buffer_limit(libcows_ctx *, int limit);

/*
 * Send data to the client in a separate frame.
 * RETURN VALUES:
 *  0:  All data was enqueued successfully.
 * -1:  Data was not enqueued, no effect done.
 */
typedef enum {
    LCERR_OK,                   /* Data is queued or sent */
    LCERR_QUOTA_EXCEEDED,       /* Data not queued or sent */
    LCERR_BROKEN_CONNECTION,    /* ctx is gonna be destroyed later. */
} libcows_send_error;
libcows_send_error libcows_send(libcows_ctx *, void *data, size_t size);
libcows_send_error libcows_sendv(libcows_ctx *, struct iovec *iov, int iovcnt);

/*
 * Tear down the context without going through the shutdown sequence.
 */
void libcows_context_free(libcows_ctx *);

#endif  /* LIBCOWS_H */
