/*
 * Copyright (c) 2015  Machine Zone, Inc.
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
#include <sys/uio.h>
#include <assert.h>

#include    "tcpkali_transport.h"
#include    "tcpkali_terminfo.h"
#include    "tcpkali_data.h"
#include    "tcpkali_expr.h"

int yyparse(void **param);
void *yy_scan_bytes(const char *, int len);
void *yy_delete_buffer(void *);
void *yyrestart(FILE *);

#define DEBUG(fmt, args...)  do {                                   \
        if(debug) {                                                 \
            fprintf(stderr, "%s" fmt, tcpkali_clear_eol(), ##args); \
        }                                                           \
    } while(0)


static void free_expr(tk_expr_t *expr) {
    if(expr) {
        switch(expr->type) {
        case EXPR_DATA:
            free((void *)expr->u.data.data);
            break;
        case EXPR_CONCAT:
            free_expr(expr->u.concat.expr[0]);
            free_expr(expr->u.concat.expr[1]);
            break;
        case EXPR_CONNECTION_PTR:
            break;
        }
    }
}

int
parse_payload_data(struct transport_data_spec *data, int debug) {
    assert(!(data->flags & TDS_FLAG_REPLICATED));

    tk_expr_t *expr;

    /*
     * Attempt to find expression in headers.
     */
    switch(parse_expression(&expr, data->ptr, data->once_size, debug)) {
    case 0: break;
    default: return -1;
    case 1:
        data->expr_head = expr;
        data->flags |= TDS_FLAG_EXPRESSION;
        break;
    }

    /*
     * Attempt to find expression in body.
     */
    switch(parse_expression(&expr, (char *)data->ptr + data->once_size,
                                   data->total_size - data->once_size, debug)) {
    case 0: break;
    default: return -1;
    case 1:
        data->expr_body = expr;
        data->flags |= TDS_FLAG_EXPRESSION;
        break;
    }

    return (data->flags & TDS_FLAG_EXPRESSION) ? 1 : 0;
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
        if(expr->type == EXPR_DATA) {
            free_expr(expr);
            return 0;   /* No expression found */
        }
        if(expr_p) *expr_p = expr;
        else free_expr(expr);
        return 1;
    } else {
        char tmp[PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(size)];
        DEBUG("Failed to parse \"%s\"\n",
            printable_data(tmp, sizeof(tmp), buf, size, 1));
        free_expr(expr);
        return -1;
    }
}

ssize_t eval_expression(char **buf_p, size_t size, tk_expr_t *expr, expr_callback_f cb, void *key) {
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
        if(size < expr->u.data.size)
            return -1;
        memcpy(buf, expr->u.data.data, expr->u.data.size);
        return expr->u.data.size;
    case EXPR_CONCAT: {
        assert((expr->u.concat.expr[0]->estimate_size
               + expr->u.concat.expr[1]->estimate_size) <= size);
        ssize_t size1 = eval_expression(&buf, size,
                                        expr->u.concat.expr[0], cb, key);
        if(size1 < 0) return -1;
        char *buf2 = buf + size1;
        ssize_t size2 = eval_expression(&buf2, size - size1,
                                        expr->u.concat.expr[1], cb, key);
        if(size1 < 0) return -1;
        return size1 + size2;
        }
    case EXPR_CONNECTION_PTR: {
        return cb(buf, size, expr, key);
        }
    }

    return -1;
}

