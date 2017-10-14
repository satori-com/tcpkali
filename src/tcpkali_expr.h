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
#ifndef TCPKALI_EXPR_H
#define TCPKALI_EXPR_H

#include "tcpkali_websocket.h"
#include "tcpkali_regex.h"

typedef struct tk_expr {
    enum tk_expr_type {
        EXPR_DATA,
        EXPR_RAW,      /* 'raw' */
        EXPR_WS_FRAME, /* 'ws.ping', 'ws.pong', etc */
        EXPR_CONCAT,
        EXPR_MODULO,         /* '%' */
        EXPR_CONNECTION_PTR, /* 'connection.ptr' */
        EXPR_CONNECTION_UID, /* 'connection.uid' */
        EXPR_REGEX,
        EXPR_MESSAGE_MARKER, /* 'messager.marker' */
    } type;
    union {
        struct {
            const char *data;
            size_t size;
        } data;
        struct {
            /* Do not add framing around expr: */
            struct tk_expr *expr;
        } raw;
        struct {
            const char *data;
            size_t size;
            enum ws_frame_opcode opcode;
            int rsvs;
            int fin;
        } ws_frame;
        struct {
            struct tk_expr *expr[2];
        } concat;
        struct {
            struct tk_expr *expr; /* Expression */
            long modulo_value;    /* '... % 42' => 42 */
        } modulo;
        struct {
            tregex *re;
        } regex;
    } u;
    size_t estimate_size;
    enum tk_expr_dynamic_scope {
        DS_GLOBAL_FIXED,   /* All connection share data */
        DS_PER_CONNECTION, /* Each connection has its own */
        DS_PER_MESSAGE     /* Each message is different */
    } dynamic_scope;

   /*
    * if it is "per_connection" expr then we evaluate
    * it only once and save res in res_buf
    */
    char *res_buf;
    size_t res_size;
} tk_expr_t;

/*
 * Trivial expression is expression which does not contain any
 * interesting (dynamically computable) values and is basically
 * a constant string.
 */
#define EXPR_IS_TRIVIAL(e) ((e)->type == EXPR_DATA)

/*
 * Parse the expression string of a given length into an expression.
 * Returns -1 on parse error.
 */
int parse_expression(tk_expr_t **, const char *expr_string, size_t size,
                     int debug);

void free_expression(tk_expr_t *expr, int delete_data);

typedef ssize_t(expr_callback_f)(char *buf, size_t size, tk_expr_t *, void *key,
                                 long *output_value);

/*
 * Returns -1 if the expression doesn't fit in the (size) fully.
 * Returns the size of the data placed into *buf_p otherwise.
 */
ssize_t eval_expression(char **buf_p, size_t size, tk_expr_t *, expr_callback_f,
                        void *key, long *output_value, int client_mode, pcg32_random_t *rng);

/*
 * Replicate expression (copy everything except data)
 */
tk_expr_t * replicate_expression(tk_expr_t *expr);

/*
 * Concatenate expressions. One or both expressions can be NULL.
 */
tk_expr_t *concat_expressions(tk_expr_t *, tk_expr_t *);

/*
 * Recursively check whether the given expression has a subexpression
 * of a given type.
 */
int has_subexpression(const tk_expr_t *expr, enum tk_expr_type);

/*
 * Split expression into three parts: prefix, predefined websocket frame, and
 * the remainder.
 * Expression "foo\{ws.ping}bar" will be split to "foo", \{ws.ping} and "bar"
 * expressions.
 * This function destroys the original expression.
 */
struct esw_result {
    tk_expr_t *esw_prefix;
    tk_expr_t *esw_websocket_frame;
    tk_expr_t *esw_remainder;
};
struct esw_result expression_split_by_websocket_frame(tk_expr_t *expr);

/*
 * Recursively go over all of the parts of the expression and unescape them.
 */
void unescape_expression(tk_expr_t *expr);

/*
 * Recursively go over all of the parts of the expression and
 * callculate average msg size.
 */

size_t average_size(tk_expr_t *expr);

#endif /* TCPKALI_EXPR_H */
