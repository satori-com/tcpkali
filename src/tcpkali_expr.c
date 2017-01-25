/*
 * Copyright (c) 2015, 2016  Machine Zone, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>

#include "tcpkali_transport.h"
#include "tcpkali_websocket.h"
#include "tcpkali_terminfo.h"
#include "tcpkali_data.h"
#include "tcpkali_expr.h"

int yyparse(void **param);
void *yy_scan_bytes(const char *, int len);
void *yy_delete_buffer(void *);
void *yyrestart(FILE *);

#define DEBUG(fmt, args...)                                         \
    do {                                                            \
        if(debug) {                                                 \
            fprintf(stderr, "%s" fmt, tcpkali_clear_eol(), ##args); \
        }                                                           \
    } while(0)


void
free_expression(tk_expr_t *expr) {
    if(expr) {
        switch(expr->type) {
        case EXPR_DATA:
            free((void *)expr->u.data.data);
            break;
        case EXPR_RAW:
            free_expression((void *)expr->u.raw.expr);
            break;
        case EXPR_WS_FRAME:
            free((void *)expr->u.ws_frame.data);
            break;
        case EXPR_MODULO:
            free_expression((void *)expr->u.modulo.expr);
            break;
        case EXPR_CONCAT:
            free_expression(expr->u.concat.expr[0]);
            free_expression(expr->u.concat.expr[1]);
            break;
        case EXPR_CONNECTION_PTR:
        case EXPR_CONNECTION_UID:
        case EXPR_MESSAGE_MARKER:
            break;
        case EXPR_REGEX:
            tregex_free(expr->u.regex.re);
            break;
        }
    }
}

int
parse_expression(tk_expr_t **expr_p, const char *buf, size_t size, int debug) {
    void *ybuf;
    ybuf = yy_scan_bytes(buf, size);
    if(!ybuf) {
        assert(ybuf);
        return -1;
    }

    if(expr_p) *expr_p = 0;

    tk_expr_t *expr = 0;

    int ret = yyparse((void *)&expr);

    yy_delete_buffer(ybuf);

    if(ret == 0) {
        assert(expr);
        if(expr_p)
            *expr_p = expr;
        else
            free_expression(expr);
        return 0;
    } else {
        char tmp[PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(size)];
        DEBUG("Failed to parse \"%s\"\n",
              printable_data(tmp, sizeof(tmp), buf, size, 1));
        free_expression(expr);
        return -1;
    }
}

ssize_t
eval_expression(char **buf_p, size_t size, tk_expr_t *expr, expr_callback_f cb,
                void *key, long *value, int client_mode, pcg32_random_t *rng) {
    char *buf;

    if(!*buf_p) {
        size = size ? size : expr->estimate_size;
        buf = malloc(size + 1);
        *buf_p = buf;
    } else {
        buf = *buf_p;
    }

    switch(expr->type) {
    case EXPR_DATA:
        if(size < expr->u.data.size) return -1;
        memcpy(buf, expr->u.data.data, expr->u.data.size);
        return expr->u.data.size;
    case EXPR_RAW:
        return eval_expression(buf_p, size, expr->u.raw.expr, cb, key, value,
                               client_mode, rng);
    case EXPR_WS_FRAME:
        if(size < expr->estimate_size) {
            return -1;
        } else {
            size_t hdr_size = websocket_frame_header(
                (uint8_t *)buf, size,
                client_mode ? WS_SIDE_CLIENT : WS_SIDE_SERVER,
                expr->u.ws_frame.opcode, expr->u.ws_frame.rsvs,
                expr->u.ws_frame.fin, expr->u.ws_frame.size);
            memcpy(buf + hdr_size, expr->u.ws_frame.data,
                   expr->u.ws_frame.size);
            return (hdr_size + expr->u.ws_frame.size);
        }
    case EXPR_MODULO: {
        long v = 0;
        (void)eval_expression(&buf, size, expr->u.modulo.expr, cb, key, &v,
                              client_mode, rng);
        v %= expr->u.modulo.modulo_value;
        ssize_t s = snprintf(buf, size, "%ld", v);
        if(s < 0 || s > (ssize_t)size) return -1;
        if(value) *value = v;
        return s;
    } break;
    case EXPR_CONCAT: {
        if((expr->u.concat.expr[0]->estimate_size
            + expr->u.concat.expr[1]->estimate_size)
           > size)
            return -1;
        ssize_t size1 = eval_expression(&buf, size, expr->u.concat.expr[0], cb,
                                        key, value, client_mode, rng);
        if(size1 < 0) return -1;
        char *buf2 = buf + size1;
        ssize_t size2 =
            eval_expression(&buf2, size - size1, expr->u.concat.expr[1], cb,
                            key, value, client_mode, rng);
        if(size1 < 0) return -1;
        return size1 + size2;
    }
    case EXPR_CONNECTION_PTR:
    case EXPR_CONNECTION_UID:
    case EXPR_MESSAGE_MARKER:
        return cb(buf, size, expr, key, value);
    case EXPR_REGEX:
        return tregex_eval_rng(expr->u.regex.re, buf, size, rng);
    }

    return -1;
}

tk_expr_t *
concat_expressions(tk_expr_t *expr1, tk_expr_t *expr2) {
    if(expr1 && expr2) {
        tk_expr_t *expr = calloc(1, sizeof(*expr));
        expr->type = EXPR_CONCAT;
        expr->u.concat.expr[0] = expr1;
        expr->u.concat.expr[1] = expr2;
        expr->estimate_size = expr1->estimate_size + expr2->estimate_size;
        expr->dynamic_scope = expr1->dynamic_scope > expr2->dynamic_scope
                                  ? expr1->dynamic_scope
                                  : expr2->dynamic_scope;
        return expr;
    } else if(expr1) {
        return expr1;
    } else if(expr2) {
        return expr2;
    } else {
        return NULL;
    }
}

/*
 * Split result into parts that may be WS-framed, the WS frame itself,
 * and a remainder.
 */
struct esw_result
expression_split_by_websocket_frame(tk_expr_t *expr) {
    struct esw_result result = {0, 0, 0};

    if(!expr) return result;

    switch(expr->type) {
    case EXPR_DATA:
    case EXPR_MODULO:
    case EXPR_CONNECTION_PTR:
    case EXPR_CONNECTION_UID:
    case EXPR_REGEX:
    case EXPR_MESSAGE_MARKER:
        result.esw_prefix = expr;
        return result;
    case EXPR_WS_FRAME:
    case EXPR_RAW:
        result.esw_websocket_frame = expr;
        return result;
    case EXPR_CONCAT: {
        struct esw_result result0 =
            expression_split_by_websocket_frame(expr->u.concat.expr[0]);
        struct esw_result result1 =
            expression_split_by_websocket_frame(expr->u.concat.expr[1]);
        /* No websocket frames in both branches of concatenation */
        if(result0.esw_websocket_frame == NULL
           && result1.esw_websocket_frame == NULL) {
            result.esw_prefix = expr;
            return result;
        }
        expr->u.concat.expr[0] = NULL;
        expr->u.concat.expr[1] = NULL;
        free_expression(expr);
        expr = NULL;
        if(result0.esw_websocket_frame) {
            result0.esw_remainder = concat_expressions(
                concat_expressions(result0.esw_remainder, result1.esw_prefix),
                concat_expressions(result1.esw_websocket_frame,
                                   result1.esw_remainder));
            return result0;
        }
        assert(result0.esw_prefix != NULL);
        assert(result0.esw_websocket_frame == NULL);
        assert(result0.esw_remainder == NULL);
        result1.esw_prefix =
            concat_expressions(result0.esw_prefix, result1.esw_prefix);
        return result1;
    }
    }

    return result;
}

int has_subexpression(const tk_expr_t *expr, enum tk_expr_type t) {
    if(!expr) return 0;

    if(expr->type == t)
        return 1;

    switch(expr->type) {
    case EXPR_RAW:
        return has_subexpression(expr->u.raw.expr, t);
    case EXPR_CONCAT:
        return has_subexpression(expr->u.concat.expr[0], t)
                || has_subexpression(expr->u.concat.expr[1], t);
    case EXPR_MODULO:
        return has_subexpression(expr->u.modulo.expr, t);
    default:
        return 0;
    }
}

void
unescape_expression(tk_expr_t *expr) {
    switch(expr->type) {
    case EXPR_DATA:
        unescape_data((char *)expr->u.data.data, &expr->u.data.size);
        expr->estimate_size = expr->u.data.size;
        return;
    case EXPR_RAW:
        unescape_expression(expr->u.raw.expr);
        return;
    case EXPR_CONCAT:
        unescape_expression(expr->u.concat.expr[0]);
        unescape_expression(expr->u.concat.expr[1]);
        expr->estimate_size = expr->u.concat.expr[0]->estimate_size
                              + expr->u.concat.expr[1]->estimate_size;
        return;
    case EXPR_MODULO:
    case EXPR_CONNECTION_PTR:
    case EXPR_CONNECTION_UID:
    case EXPR_REGEX:
    case EXPR_MESSAGE_MARKER:
        return;
    case EXPR_WS_FRAME: {
        size_t overhead = expr->estimate_size - expr->u.ws_frame.size;
        unescape_data((char *)expr->u.ws_frame.data, &expr->u.ws_frame.size);
        expr->estimate_size = expr->u.ws_frame.size + overhead;
        return;
    }
    }
}
