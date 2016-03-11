%{

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "tcpkali_websocket.h"
#include "tcpkali_expr.h"

int yylex(void);
int yyerror(const char *);

#define YYPARSE_PARAM   param
#define YYERROR_VERBOSE

%}

%union  {
    tk_expr_t   *tv_expr;
    long         tv_long;
    struct {
        char  *buf;
        size_t len;
    } tv_string;
    enum ws_frame_opcode tv_opcode;
    char  tv_char;
};

%token              TOK_ws           "ws"
%token <tv_opcode>  TOK_ws_opcode    "text, binary, close, ping, pong, continuation"
%token              TOK_connection   "connection"
%token              TOK_ptr          " ptr"
%token              TOK_uid          "uid"
%token              TOK_ellipsis     "..."
%token              END 0            "end of expression"
%token  <tv_string> string_token     "arbitrary string"
%token  <tv_string> quoted_string    "quoted string"
%token  <tv_string> filename         "file name"
%token  <tv_long>   integer

%type   <tv_string> String FileOrData
%type   <tv_expr>   NumericExpr
%type   <tv_expr>   WSBasicFrame WSFrameWithData WSFrameFinalized
%type   <tv_expr>   ByteSequenceOrExpr          "some string or \\{expression}"
%type   <tv_expr>   ByteSequencesAndExpressions "data and expressions"

%%

Grammar:
    END {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        *(tk_expr_t **)param = expr;
        return 0;
    }
    | ByteSequencesAndExpressions END {
        *(tk_expr_t **)param = $1;
        return 0;
    }

ByteSequencesAndExpressions:
    ByteSequenceOrExpr {
        $$ = $1;
    }
    | ByteSequenceOrExpr ByteSequencesAndExpressions {
        $$ = concat_expressions($1, $2);
    }

String:
    string_token
    | String string_token {
        size_t len = (($1).len + ($2).len);
        char *p = malloc(len + 1);
        memcpy(p, ($1).buf, ($1).len);
        memcpy(p + ($1).len, ($2).buf, ($2).len);
        p[len] = '\0';
        $$.buf = p;
        $$.len = len;
    }

ByteSequenceOrExpr:
    String {
        /* If there's nothing to parse, don't return anything */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ($1).buf;
        expr->u.data.size = ($1).len;
        expr->estimate_size = ($1).len;
        $$ = expr;
    }
    | '{' NumericExpr '}' {
        $$ = $2;
        $$->dynamic = 1;
    }
    | '{' WSFrameFinalized '}' {
        $$ = $2;
    }

NumericExpr:
    NumericExpr '%' integer {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_MODULO;
        $$->u.modulo.expr = $1;
        $$->u.modulo.modulo_value = $3;
        $$->estimate_size = $1->estimate_size;
    }
    | TOK_connection '.' TOK_ptr {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_CONNECTION_PTR;
        $$->estimate_size = sizeof("100000000000000");
    }
    | TOK_connection '.' TOK_uid {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_CONNECTION_UID;
        $$->estimate_size = sizeof("100000000000000");
    }

WSFrameFinalized:
    WSFrameWithData
    /* \{ws.ping ...} (yes, tree dots!) */
    | WSFrameWithData TOK_ellipsis {
        $$ = $1;
        $$->u.ws_frame.fin = 0; /* Expect continuation. */
    }

WSFrameWithData:
    WSBasicFrame
    /* \{ws.ping "Ping payload"} or \{ws.binary </dev/null>} */
    | WSFrameWithData FileOrData {
        $$ = $1;
        /* Combine old data with new data. */
        size_t total_size = $$->u.ws_frame.size + ($2).len;
        char *p = malloc(total_size + 1);
        assert(p);
        memcpy(p, $$->u.ws_frame.data, $$->u.ws_frame.size);
        memcpy(p + $$->u.ws_frame.size, ($2).buf, ($2).len);
        p[total_size] = '\0';
        free((void *)$$->u.ws_frame.data);
        $$->u.ws_frame.data = p;
        $$->u.ws_frame.size = total_size;
        $$->estimate_size += ($2).len;
    }

WSBasicFrame:
    TOK_ws '.' TOK_ws_opcode {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_WS_FRAME;
        $$->u.ws_frame.opcode = $3;
        $$->u.ws_frame.fin = 1; /* Complete frame */
        $$->estimate_size = WEBSOCKET_MAX_FRAME_HDR_SIZE;
    }

FileOrData:
    quoted_string
    | filename {
        const char *name = $1.buf;
        FILE *fp = fopen(name, "r");
        if(!fp) {
            fprintf(stderr, "Can't open \"%s\": %s\n", name, strerror(errno));
            exit(1);
        }
        fseek(fp, 0, SEEK_END);
        $$.len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        $$.buf = malloc($$.len + 1);
        if($$.buf == NULL || fread($$.buf, 1, $$.len, fp) != $$.len) {
            fprintf(stderr, "Can't read \"%s\": %s\n", name, strerror(errno));
            exit(1);
        }
        fclose(fp);
        $$.buf[$$.len] = '\0';
    }
    

%%

int
yyerror(const char *msg) {
    extern char *yytext;
    fprintf(stderr, "Parse error near token \"%s\": %s\n", yytext, msg);
    return -1;
}

