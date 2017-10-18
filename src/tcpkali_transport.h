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
#ifndef TCPKALI_TRANSPORT_H
#define TCPKALI_TRANSPORT_H

#include "tcpkali_expr.h"

/* Forward declarations */
struct tk_expr;
struct transport_data_spec;

/*
 * This is what we get from the CLI options.
 */
struct message_collection {
    struct message_collection_snippet {
        char *data;
        size_t size;
        struct tk_expr *expr;
        enum mc_snippet_kind {
            /* Whether to add WebSocket framing, if needed */
            MSK_PURPOSE_HTTP_HEADER = 0x01, /* HTTP connection upgrade */
            MSK_PURPOSE_FIRST_MSG = 0x02,   /* --first-message, *-file */
            MSK_PURPOSE_MESSAGE = 0x04,     /* --message, *-file */
            MSK_FRAMING_REQUESTED = 0x10,   /* msg needs framing, hdr doesn't */
            MSK_FRAMING_ASSERTED = 0x20,    /* msg will be framed */
            MSK_EXPRESSION_FOUND = 0x40     /* Expression */
#define MSK_PURPOSE(snippet) ((snippet)->flags & 0x0f)
        } flags;
        int sort_index;
    } * snippets;
    /*
     * Number of --first-message, --message, etc
     * options reflected in snippets.
     */
    size_t snippets_count;
    /*
     * Size of the snippets array (>= snippets_count).
     */
    size_t snippets_size;
    /*
     * Whether variable \{expressions} were found in snippets,
     * and exactly how dynamic is the most dynamic expression.
     * Scopes are: global, connection and per message.
     */
    enum tk_expr_dynamic_scope most_dynamic_expression;
    /*
     * A collection must be finalized before use.
     */
    enum {
        MC_EMBRYONIC,           /* We don't know what data is for */
        MC_FINALIZED_PLAIN_TCP, /* Data is for plain TCP connection */
        MC_FINALIZED_WEBSOCKET  /* Data is for WebSocket connection */
    } state;
};

/*
 * Add a new data snippet into the message collection.
 * The function copies data.
 */
void message_collection_add(struct message_collection *mc, enum mc_snippet_kind,
                            void *data, size_t size, int unescape,
                            int parse_expressions);

/*
 * Add a new expression to the message collection.
 * The function takes over the expression pointer.
 */
void message_collection_add_expr(struct message_collection *mc,
                                 enum mc_snippet_kind, struct tk_expr *);

/*
 * Finalize the collection, preventing new data to be added,
 * and adding websocket related messages details.
 */
void message_collection_finalize(struct message_collection *, int as_websocket,
                                 const char *hostport, const char *path,
                                 const char *headers);

/*
 * Recursively figure out if message collection contains the
 * expression of a given type.
 * Returns 0 if the expression type is not found. Otherwise, returns 1.
 */
int message_collection_has(const struct message_collection *, enum tk_expr_type);

/*
 * Estimate the size of the snippets of the specified kind (and mask).
 * Works on a finalized message collection.
 */
enum mc_snippet_estimate { MCE_MINIMUM_SIZE, MCE_MAXIMUM_SIZE, MCE_AVERAGE_SIZE };
size_t message_collection_estimate_size(struct message_collection *mc,
                                        enum mc_snippet_kind kind_and,
                                        enum mc_snippet_kind kind_equal,
                                        enum mc_snippet_estimate,
                                        enum websocket_side ws_side,
                                        int ws_enable);

/*
 * Our send buffer is pre-computed in advance and may be shared
 * between the instances of engine (workers).
 * The buffer contains both headers and payload.
 * The data_header_size is used to determine the end of the headers
 * and start of the payload.
 */
struct transport_data_spec {
    void *ptr;
    size_t ws_hdr_size; /* HTTP header for WebSocket upgrade. */
    size_t once_size;   /* Part of data to send just once. */
    size_t total_size;
    size_t allocated_size;
    size_t single_message_size;
    void *marker_token_ptr;
    enum transport_data_flags {
        TDS_FLAG_NONE = 0x00,
        TDS_FLAG_PTR_SHARED = 0x01, /* Disallow freeing .ptr field */
        TDS_FLAG_REPLICATED = 0x02, /* total_size >= once_ + single_message_ */
    } flags;
};

#define REPLICATE_MAX_SIZE (64 * 1024 - 1) /* Proven to be a sweet spot */

/*
 * Convert message collection into transport data specification, which is
 * friendlier for the high speed sending routine.
 */
enum transport_websocket_side {
    TWS_SIDE_CLIENT,
    TWS_SIDE_SERVER,
};
enum transport_conversion {
    TS_CONVERSION_INITIAL,
    TS_CONVERSION_OVERRIDE_MESSAGES,
};
struct transport_data_spec *transport_spec_from_message_collection(
    struct transport_data_spec *out_spec, struct message_collection *,
    expr_callback_f optional_cb, void *expr_cb_key,
    enum transport_websocket_side, enum transport_conversion,
    pcg32_random_t *rng);

/*
 * To be able to efficiently transfer small payloads, we replicate
 * the payload data several times to send more data in a single call.
 * The function replaces the given (data) contents.
 */
void replicate_payload(struct transport_data_spec *data,
                       size_t target_payload_size);

/*
 * Replicate snippets (need to replicate expressions)
 * it does not copy data
 */
void message_collection_replicate(struct message_collection *mc_from,
                                  struct message_collection *mc_to);

void message_collection_free(struct message_collection *mc);

#endif /* TCPKALI_TRANSPORT_H */
