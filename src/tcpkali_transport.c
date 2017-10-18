/*
 * Copyright (c) 2014, 2015, 2016  Machine Zone, Inc.
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
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "tcpkali_data.h"
#include "tcpkali_expr.h"
#include "tcpkali_websocket.h"
#include "tcpkali_transport.h"

/*
 * Helper function to sort headers first, messages last.
 */
static int
snippet_compare_cb(const void *ap, const void *bp) {
    const struct message_collection_snippet *a = ap;
    const struct message_collection_snippet *b = bp;
    int ka = MSK_PURPOSE(a);
    int kb = MSK_PURPOSE(b);

    if(ka < kb) return -1;
    if(ka > kb) return 1;

    if(a->sort_index < b->sort_index) return -1;
    if(a->sort_index > b->sort_index) return 1;

    return 0;
}

void
message_collection_finalize(struct message_collection *mc, int as_websocket,
                            const char *hostport, const char *path, const char *headers) {
    const char ws_http_headers_fmt[] =
        "GET /%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "%s\r\n";

    assert(mc->state == MC_EMBRYONIC);

    if(as_websocket) {
        if(!hostport) hostport = "";
        if(!path) path = "";
        if(!headers) headers = "";

        ssize_t estimated_size = sizeof(ws_http_headers_fmt) + strlen(hostport)
                                 + strlen(path) + strlen(headers);
        char http_headers[estimated_size];
        ssize_t h_size = snprintf(http_headers, estimated_size,
                                  ws_http_headers_fmt, path, hostport, headers);
        assert(h_size < estimated_size);

        const int NO_UNESCAPE = 0;
        const int PERFORM_EXPR_PARSING = 1;
        message_collection_add(mc, MSK_PURPOSE_HTTP_HEADER, http_headers,
                               h_size, NO_UNESCAPE, PERFORM_EXPR_PARSING);

        mc->state = MC_FINALIZED_WEBSOCKET;
    } else {
        mc->state = MC_FINALIZED_PLAIN_TCP;
    }

    /* Order hdr > first_msg > msg. */
    qsort(mc->snippets, mc->snippets_count, sizeof(mc->snippets[0]),
          snippet_compare_cb);
}

/*
 * If the payload is less then target_size,
 * replicate it several times so the total buffer exceeds target_size.
 */
void
replicate_payload(struct transport_data_spec *data, size_t target_size) {
    size_t payload_size = data->total_size - data->once_size;

    assert(!(data->flags & TDS_FLAG_REPLICATED));
    assert(data->marker_token_ptr == 0);    /* Can't be replicated */

    if(!payload_size) {
        /* Can't blow up an empty buffer. */
    } else if(payload_size >= target_size) {
        /* Data is large enough to avoid blowing up. */
    } else {
        /* The optimum target_size is size(L2)/k */
        size_t n = ceil(((double)target_size) / payload_size);
        size_t new_payload_size = n * payload_size;
        size_t once_offset = data->once_size;
        size_t allocated = once_offset + new_payload_size + 1;
        char *p = realloc(data->ptr, allocated);
        void *msg_data = p + once_offset;
        assert(p);
        for(size_t i = 1; i < n; i++) {
            memcpy(&p[once_offset + i * payload_size], msg_data, payload_size);
        }
        p[once_offset + new_payload_size] = '\0';
        data->ptr = p;
        data->total_size = once_offset + new_payload_size;
        data->allocated_size = allocated;
    }

    /*
     * Always mark as replicated, even if we have not increased the size
     * At least, replication procedure was applied.
     */
    data->flags |= TDS_FLAG_REPLICATED;
}

void
message_collection_replicate(struct message_collection *mc_from, struct message_collection *mc_to) {
    mc_to->snippets = malloc(sizeof(mc_from->snippets[0])*mc_from->snippets_size);
    for(size_t i = 0; i < mc_from->snippets_count; i++) {
        struct message_collection_snippet *snip = &mc_from->snippets[i];
        mc_to->snippets[i].data = snip->data;
        mc_to->snippets[i].size = snip->size;
        mc_to->snippets[i].expr = replicate_expression(snip->expr);
        mc_to->snippets[i].flags = snip->flags;
        mc_to->snippets[i].sort_index =  snip->sort_index;
    }
    mc_to->snippets_size = mc_from->snippets_size;
    mc_to->snippets_count = mc_from->snippets_count;
    mc_to->most_dynamic_expression = mc_from->most_dynamic_expression;
    mc_to->state = mc_from->state;
}

void
message_collection_free(struct message_collection *mc) {
    for(size_t i = 0; i < mc->snippets_count; i++) {
        struct message_collection_snippet *snip = &mc->snippets[i];
        if(snip->flags & MSK_EXPRESSION_FOUND) {
            free_expression(snip->expr, 0);
        }
    }
    free((void *)mc->snippets);
}

static void
message_collection_ensure_space(struct message_collection *mc, size_t need) {
    /* Reallocate snippets array, if needed. */
    while(mc->snippets_count + need > mc->snippets_size) {
        mc->snippets_size = 2 * (mc->snippets_size ? mc->snippets_size : 8);
        struct message_collection_snippet *ptr =
            realloc(mc->snippets, mc->snippets_size * sizeof(mc->snippets[0]));
        if(!ptr) {
            fprintf(stderr,
                    "Too many --message "
                    "or --first-message arguments\n");
            exit(1);
        }
        memset(&ptr[mc->snippets_count], 0,
               (mc->snippets_size - mc->snippets_count) * sizeof(ptr[0]));
        mc->snippets = ptr;
    }
}

void
message_collection_add(struct message_collection *mc, enum mc_snippet_kind kind,
                       void *data, size_t size, int unescape,
                       int parse_expressions) {
    assert(mc->state == MC_EMBRYONIC);

    /* Verify that messages are properly kinded. */
    enum mc_snippet_kind adjusted_kind = kind;
    switch(adjusted_kind) {
    case MSK_PURPOSE_HTTP_HEADER:
        break;
    case MSK_PURPOSE_FIRST_MSG:
    case MSK_PURPOSE_MESSAGE:
        adjusted_kind |= MSK_FRAMING_REQUESTED;
        break;
    default:
        assert(!"Cannot add message with non-MSK_PURPOSE_ kind");
        return; /* Unreachable */
    }

    message_collection_ensure_space(mc, 1);

    char *p = malloc(size + 1);
    assert(p);
    memcpy(p, data, size);
    p[size] = 0;

    struct message_collection_snippet *snip;
    snip = &mc->snippets[mc->snippets_count];
    snip->data = p;
    snip->size = size;
    snip->expr = 0;
    snip->flags = adjusted_kind;
    snip->sort_index = mc->snippets_count;

    if(parse_expressions) {
        const int ENABLE_DEBUG = 1;
        tk_expr_t *expr = 0;
        if(parse_expression(&expr, snip->data, snip->size, ENABLE_DEBUG)
           == -1) {
            /* parse_expression() has already printed the failure reason */
            exit(1);
        }
        if(unescape) unescape_expression(expr);
        if(expr->type == EXPR_DATA) {
            /*
             * Trivial expression which does not change.
             * Absorbing it into collection element body.
             */
            free(snip->data);
            snip->data = (char *)expr->u.data.data;
            snip->size = expr->u.data.size;
            expr->u.data.data = 0;
            free_expression(expr, 0);
            /* Just use the snip->data instead. */
            mc->snippets_count++;
        } else {
            message_collection_add_expr(mc, kind, expr);
        }
    } else {
        if(unescape) unescape_data(snip->data, &snip->size);
        mc->snippets_count++;
    }
}

void
message_collection_add_expr(struct message_collection *mc,
                            enum mc_snippet_kind kind, struct tk_expr *expr) {
    while(expr) {
        message_collection_ensure_space(mc, 2);

        struct esw_result result = expression_split_by_websocket_frame(expr);

        struct message_collection_snippet *snip;
        snip = &mc->snippets[mc->snippets_count];
        snip->data = 0;
        snip->size = 0;
        snip->expr = 0;
        snip->flags = kind;
        snip->sort_index = mc->snippets_count;

        if(result.esw_prefix) {
            snip->expr = result.esw_prefix;
            snip->flags = kind;
            if(!(snip->flags & MSK_PURPOSE_HTTP_HEADER))
                snip->flags |= MSK_FRAMING_REQUESTED;
            snip->flags |= MSK_EXPRESSION_FOUND;
            if(mc->most_dynamic_expression < snip->expr->dynamic_scope) {
                mc->most_dynamic_expression = snip->expr->dynamic_scope;
            }
            mc->snippets_count++;

            snip = &mc->snippets[mc->snippets_count];
            snip->data = 0;
            snip->size = 0;
            snip->expr = 0;
            snip->sort_index = mc->snippets_count;
        }

        if(result.esw_websocket_frame) {
            snip->expr = result.esw_websocket_frame;
            if(snip->expr->type == EXPR_RAW
               && snip->expr->u.raw.expr->type != EXPR_DATA) {
                snip->flags |= MSK_EXPRESSION_FOUND;
            }
            if(mc->most_dynamic_expression < snip->expr->dynamic_scope) {
                mc->most_dynamic_expression = snip->expr->dynamic_scope;
            }
            /* Disallow framing of websocket frames. */
            snip->flags = kind;
            snip->flags &= (~MSK_FRAMING_REQUESTED);
            snip->flags |= MSK_FRAMING_ASSERTED;
            snip->flags |= MSK_EXPRESSION_FOUND;
            mc->snippets_count++;
        }

        /*
         * Figure out what to do with the rest of the expression.
         * It might contain more websocket frames.
         */
        expr = result.esw_remainder;
    }
}

size_t
ws_header_size_estimate(enum mc_snippet_estimate mce,
                        enum websocket_side ws_side,
                        size_t data_size) {
    switch (mce) {
    case MCE_MAXIMUM_SIZE: {
        size_t client_size = websocket_frame_header(NULL, 0, WS_SIDE_CLIENT,
                                       WS_OP_TEXT_FRAME, 0, 1, data_size);
        size_t server_size = websocket_frame_header(NULL, 0, WS_SIDE_SERVER,
                                       WS_OP_TEXT_FRAME, 0, 1, data_size);
        return client_size > server_size ? client_size : server_size;
    };
    case MCE_AVERAGE_SIZE: {
        return websocket_frame_header(NULL, 0, ws_side,
                                       WS_OP_TEXT_FRAME, 0, 1, data_size);
    };
    case MCE_MINIMUM_SIZE: {
        size_t client_size = websocket_frame_header(NULL, 0, WS_SIDE_CLIENT,
                                       WS_OP_TEXT_FRAME, 0, 1, data_size);
        size_t server_size = websocket_frame_header(NULL, 0, WS_SIDE_SERVER,
                                       WS_OP_TEXT_FRAME, 0, 1, data_size);
        return client_size < server_size ? client_size : server_size;
    };
    }
}

/*
 * Give the largest size the message can possibly occupy.
 */
size_t
message_collection_estimate_size(struct message_collection *mc,
                                 enum mc_snippet_kind kind_and,
                                 enum mc_snippet_kind kind_equal,
                                 enum mc_snippet_estimate mce,
                                 enum websocket_side ws_side,
                                 int ws_enable) {
    size_t total_size = 0;
    size_t i;

    assert(mc->state != MC_EMBRYONIC);

    for(i = 0; i < mc->snippets_count; i++) {
        size_t snippet_size = 0;
        struct message_collection_snippet *snip = &mc->snippets[i];

        /* Match pattern */
        if((snip->flags & kind_and) != kind_equal) continue;

        if(snip->flags & MSK_EXPRESSION_FOUND) {
            if(mce == MCE_AVERAGE_SIZE) {
                snippet_size += average_size(snip->expr);
            } else if(snip->expr->type == EXPR_REGEX && mce == MCE_MINIMUM_SIZE) {
                snippet_size += tregex_min_size(snip->expr->u.regex.re);
            } else {
                snippet_size += snip->expr->estimate_size;
            }
        } else {
            snippet_size += snip->size;
        }
        snippet_size += ws_enable ?
                        ws_header_size_estimate(mce, ws_side, snippet_size) : 0;
        total_size += snippet_size;
    }
    return total_size;
}

int
message_collection_has(const struct message_collection *mc, enum tk_expr_type t) {

    for(size_t i = 0; i < mc->snippets_count; i++) {
        struct message_collection_snippet *snip = &mc->snippets[i];
        if(has_subexpression(snip->expr, t)) {
            return 1;
        }
    }

    return 0;
}

typedef struct {
    expr_callback_f *original_callback;
    void *original_key;
    void **marker_ptr_ptr;
    int multiple_message_markers;
} callback_wrapper_key_t;
static ssize_t
callback_wrapper(char *buf, size_t size, tk_expr_t *expr, void *key,
                 long *output_value) {
    callback_wrapper_key_t *wkey = key;

    if(expr->type == EXPR_MESSAGE_MARKER) {
        if(!*wkey->marker_ptr_ptr && !wkey->multiple_message_markers) {
            *wkey->marker_ptr_ptr = buf;
        } else {
            /* Use more generic (slow) scanning algorithm later */
            wkey->multiple_message_markers = 1;
            *wkey->marker_ptr_ptr = 0;
        }
    }

    return wkey->original_callback(buf, size, expr, wkey->original_key,
                                  output_value);
}


struct transport_data_spec *
transport_spec_from_message_collection(struct transport_data_spec *out_spec,
                                       struct message_collection *mc,
                                       expr_callback_f optional_cb,
                                       void *expr_cb_key,
                                       enum transport_websocket_side tws_side,
                                       enum transport_conversion tconv,
                                       pcg32_random_t *rng) {
    /*
     * If expressions found we can not create a transport data specification
     * from this collection directly. Need to go through expression evaluator.
     */
    if(mc->most_dynamic_expression != DS_GLOBAL_FIXED) {
        if(!optional_cb) return NULL;
    }

    struct transport_data_spec *data_spec;
    /* out_spec is expected to be 0-filled, if given. */
    data_spec = out_spec ? out_spec : calloc(1, sizeof(*data_spec));
    assert(data_spec);

    enum websocket_side ws_side =
        (tws_side == TWS_SIDE_CLIENT) ? WS_SIDE_CLIENT : WS_SIDE_SERVER;

    if(tconv == TS_CONVERSION_INITIAL) {
        size_t estimate_size =
            message_collection_estimate_size(mc, 0, 0, MCE_MAXIMUM_SIZE, ws_side, 1);
        if(estimate_size < REPLICATE_MAX_SIZE)
            estimate_size = REPLICATE_MAX_SIZE;
        data_spec->ptr = malloc(estimate_size + 1);
        data_spec->allocated_size = estimate_size;
        assert(data_spec->ptr);
    } else {
        assert(data_spec);
        assert(data_spec->ptr);
        data_spec->total_size = data_spec->once_size;
    }

    callback_wrapper_key_t callback_key = {.original_callback = optional_cb,
                                           .original_key = expr_cb_key};

    int place_multiple_messages = 0;

    do { /* while(place_multiple_messages) */

        if(tconv == TS_CONVERSION_OVERRIDE_MESSAGES) {
            /* We'll rebuild the message anew */
            data_spec->single_message_size = 0;
        }

        size_t i;
        for(i = 0; i < mc->snippets_count; i++) {
            struct message_collection_snippet *snip = &mc->snippets[i];

            void *data = snip->data;
            size_t size = snip->size;

            if(tconv == TS_CONVERSION_OVERRIDE_MESSAGES) {
                if(MSK_PURPOSE(snip) == MSK_PURPOSE_MESSAGE)
                    place_multiple_messages = 1;
                else
                    continue;
            }

            size_t estimate_ws_frame_size = 0;
            void *marker_ptr = 0;

            if(snip->flags & MSK_EXPRESSION_FOUND) {
                ssize_t reified_size;
                uint8_t *tptr = data_spec->ptr + data_spec->total_size;
                if(data_spec->total_size + WEBSOCKET_MAX_FRAME_HDR_SIZE
                       + snip->expr->estimate_size
                   > data_spec->allocated_size) {
                    assert(tconv == TS_CONVERSION_OVERRIDE_MESSAGES);
                    place_multiple_messages = 0;
                    break;
                }
                if(mc->state == MC_FINALIZED_WEBSOCKET
                   && snip->flags & MSK_FRAMING_REQUESTED) {
                    estimate_ws_frame_size = websocket_frame_header(
                        tptr, data_spec->allocated_size - data_spec->total_size,
                        ws_side, WS_OP_TEXT_FRAME, 0, 1,
                        snip->expr->estimate_size);
                    tptr += estimate_ws_frame_size;
                }
                callback_key.marker_ptr_ptr = &marker_ptr;
                reified_size = eval_expression(
                    (char **)&tptr,
                    data_spec->allocated_size
                        - (data_spec->total_size + estimate_ws_frame_size),
                    snip->expr, callback_wrapper, &callback_key, 0,
                    (tws_side == TWS_SIDE_CLIENT), rng);
                if(callback_key.multiple_message_markers) {
                    data_spec->marker_token_ptr = 0;
                    marker_ptr = 0;
                } else if(marker_ptr) {
                    if(data_spec->marker_token_ptr) {
                        callback_key.multiple_message_markers = 1;
                        data_spec->marker_token_ptr = 0;
                        marker_ptr = 0;
                    } else {
                        data_spec->marker_token_ptr = marker_ptr;
                    }
                }
                assert(reified_size >= 0);
                data = 0;
                size = reified_size;
            } else {
                if(data_spec->total_size + snip->size
                   > data_spec->allocated_size) {
                    assert(tconv == TS_CONVERSION_OVERRIDE_MESSAGES);
                    place_multiple_messages = 0;
                    break;
                }
            }

            size_t ws_frame_size = 0;
            if(mc->state == MC_FINALIZED_WEBSOCKET) {
                /* Do not construct WebSocket/HTTP header. */
                if((ws_side == WS_SIDE_SERVER)
                   && (snip->flags & MSK_PURPOSE_HTTP_HEADER))
                    continue;

                if(snip->flags & MSK_FRAMING_REQUESTED) {
                    if(snip->flags & MSK_EXPRESSION_FOUND) {
                        uint8_t *tptr = data_spec->ptr + data_spec->total_size;
                        /* Save the websocket frame elsewhere temporarily */
                        ws_frame_size = websocket_frame_header(
                            tptr,
                            data_spec->allocated_size - data_spec->total_size,
                            ws_side, WS_OP_TEXT_FRAME, 0, 1, size);
                        /*
                         * Most of the time websocket frame will have the
                         * same length as the estimated one
                         */
                        if(ws_frame_size != estimate_ws_frame_size) {
                            assert(ws_frame_size < estimate_ws_frame_size);
                            /*
                             * Move the data to the left to adjust for lower
                             * space used by the framing.
                             */
                            memmove((char *)data_spec->ptr
                                        + data_spec->total_size + ws_frame_size,
                                    (char *)data_spec->ptr
                                        + data_spec->total_size
                                        + estimate_ws_frame_size,
                                    size);
                            if(marker_ptr) {
                                marker_ptr -= estimate_ws_frame_size - ws_frame_size;
                                data_spec->marker_token_ptr = marker_ptr;
                            }
                        }
                    } else {
                        ws_frame_size = websocket_frame_header(
                            (uint8_t *)data_spec->ptr + data_spec->total_size,
                            data_spec->allocated_size - data_spec->total_size,
                            ws_side, WS_OP_TEXT_FRAME, 0, 1, size);
                    }
                }
            }

            /*
             * We only add data if it has not already been added.
             */
            size_t framed_snippet_size = ws_frame_size + size;
            if(data) { /* Data is not there if expression is used. */
                memcpy((char *)data_spec->ptr + data_spec->total_size
                           + ws_frame_size,
                       data, size);
            }
            data_spec->total_size += framed_snippet_size;

            switch(MSK_PURPOSE(snip)) {
            case MSK_PURPOSE_HTTP_HEADER:
                data_spec->ws_hdr_size += framed_snippet_size;
                data_spec->once_size += framed_snippet_size;
                break;
            case MSK_PURPOSE_FIRST_MSG:
                data_spec->once_size += framed_snippet_size;
                break;
            case MSK_PURPOSE_MESSAGE:
                data_spec->single_message_size += framed_snippet_size;
                break;
            default:
                assert(!"No recognized snippet purpose");
                return NULL;
            }
        }

    } while(place_multiple_messages);

    assert(data_spec->total_size <= data_spec->allocated_size);
    ((char *)data_spec->ptr)[data_spec->total_size] = '\0';

    return data_spec;
}
