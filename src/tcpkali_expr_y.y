%{

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "tcpkali_websocket.h"
#include "tcpkali_expr.h"

int yylex(void);
int yyerror(tk_expr_t **, const char *);

#define YYPARSE_PARAM   param
#define YYERROR_VERBOSE

%}

%parse-param {tk_expr_t **param}

%union  {
    tk_expr_t   *tv_expr;
    tregex      *tv_regex;
    long         tv_long;
    struct {
        char  *buf;
        size_t len;
    } tv_string;
    struct {
        unsigned char from;
        unsigned char to;
    } tv_class_range;
    struct {
        unsigned char from;
        unsigned char to;
    } tv_repeat_range;
    enum ws_frame_opcode tv_opcode;
    char  tv_char;
};

%token              TOK_ws           "ws"
%token              TOK_raw          "raw"
%token <tv_opcode>  TOK_ws_opcode    "text, binary, close, ping, pong, continuation"
%token <tv_long>    TOK_ws_reserved_flag "rsv1, rsv2, rsv3"
%token              TOK_global       "global"
%token              TOK_connection   "connection"
%token              TOK_message      "message"
%token              TOK_ptr          " ptr"
%token              TOK_uid          "uid"
%token              TOK_regex        "re"
%token              TOK_marker       "marker"
%token              TOK_ellipsis     "..."
%token              END 0            "end of expression"
%token  <tv_string> string_token     "arbitrary string"
%token  <tv_class_range>  class_range_token      "regex character class range"
%token  <tv_repeat_range>  repeat_range_token      "regex repeat spec"
%token  <tv_string> quoted_string    "quoted string"
%token  <tv_string> filename         "file name"
%token  <tv_long>   integer

%left   '|'

%type   <tv_string> String File FileOrQuoted
%type   <tv_expr>   NumericExpr
%type   <tv_regex>  CompleteRegex RepeatedRegex RegexPiece RegexClasses RegexClass RegexAlternatives RegexSequence
%type   <tv_expr>   WSBasicFrame WSFrameWithData WSFrameFinalized
%type   <tv_expr>   ByteSequenceOrExpr          "some string or \\{expression}"
%type   <tv_expr>   ByteSequencesAndExpressions "data and expressions"
%type   <tv_expr>   NonWSExpression      "connection, global, re, or <filename.ext>"
%type   <tv_expr>   WSExpression      "ws"

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
    | '{' WSExpression '}' {
        $$ = $2;
    }
    | '{' NonWSExpression '}' {
        $$ = $2;
    }

WSExpression: WSFrameFinalized

NonWSExpression:
    File {    /* \{<filename.txt>} */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ($1).buf;
        expr->u.data.size = ($1).len;
        expr->estimate_size = ($1).len;
        $$ = expr;
    }
    | NumericExpr {
        $$ = $1;
    }
    | TOK_raw FileOrQuoted {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ($2).buf;
        expr->u.data.size = ($2).len;
        expr->estimate_size = ($2).len;
        $$ = calloc(1, sizeof(tk_expr_t));
        $$->type = EXPR_RAW;
        $$->u.raw.expr = expr;
        $$->estimate_size = expr->estimate_size;
    }
    | TOK_raw '{' NonWSExpression '}' {
        $$ = calloc(1, sizeof(tk_expr_t));
        $$->type = EXPR_RAW;
        $$->u.raw.expr = $3;
        $$->estimate_size = $3->estimate_size;
        $$->dynamic_scope = $3->dynamic_scope;
    }
    | TOK_global '.' TOK_regex CompleteRegex {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        char *data = malloc(tregex_max_size($4) + 1);
        assert(data);
        expr->u.data.data = data;
        expr->u.data.size = tregex_eval($4, data, tregex_max_size($4)+ 1);
        expr->estimate_size = expr->u.data.size;
        tregex_free($4);
        $$ = expr;
    }
    | TOK_connection '.' TOK_regex CompleteRegex {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_REGEX;
        expr->u.regex.re = $4;
        expr->estimate_size = tregex_max_size($4);
        expr->dynamic_scope = DS_PER_CONNECTION;
        $$ = expr;
    }
    | TOK_regex CompleteRegex {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_REGEX;
        expr->u.regex.re = $2;
        expr->estimate_size = tregex_max_size($2);
        expr->dynamic_scope = DS_PER_MESSAGE;
        $$ = expr;
    }

NumericExpr:
    NumericExpr '%' integer {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_MODULO;
        $$->u.modulo.expr = $1;
        $$->u.modulo.modulo_value = $3;
        $$->estimate_size = $1->estimate_size;
        $$->dynamic_scope = $1->dynamic_scope;
    }
    | TOK_connection '.' TOK_ptr {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_CONNECTION_PTR;
        $$->estimate_size = sizeof("100000000000000");
        $$->dynamic_scope = DS_PER_CONNECTION;
    }
    | TOK_connection '.' TOK_uid {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_CONNECTION_UID;
        $$->estimate_size = sizeof("100000000000000");
        $$->dynamic_scope = DS_PER_CONNECTION;
    }
    | TOK_message '.' TOK_marker {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_MESSAGE_MARKER;
        $$->estimate_size = sizeof("1000000000000" "1000000000000000" "!") - 1;
        $$->dynamic_scope = DS_PER_MESSAGE;
    }

WSFrameFinalized:
    WSFrameWithData
    /* \{ws.ping ...} (yes, tree dots!) */
    | WSFrameFinalized TOK_ellipsis {
        $$ = $1;
        $$->u.ws_frame.fin = 0; /* Expect continuation. */
    }
    /* \{ws.ping rsv1} */
    | WSFrameFinalized TOK_ws_reserved_flag {
        $$ = $1;
        $$->u.ws_frame.rsvs |= $2;
    }

WSFrameWithData:
    WSBasicFrame
    /* \{ws.ping "Ping payload"} or \{ws.binary </dev/null>} */
    | WSFrameWithData FileOrQuoted {
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

FileOrQuoted: quoted_string | File

File:
    '<' filename '>' {
        const char *name = $2.buf;
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
    
CompleteRegex:
    RegexAlternatives

RegexAlternatives:
    RegexSequence {
        $$ = tregex_alternative($1);
    }
    | RegexAlternatives '|' RegexSequence {
        $$ = tregex_alternative_add($1, $3);
    }

RegexSequence:
    RepeatedRegex
    | RegexSequence RepeatedRegex {
        $$ = tregex_join($1, $2);
    }

RepeatedRegex:
    RegexPiece
    | RegexPiece '?' { $$ = tregex_repeat($1, 0, 1); }
    | RegexPiece '+' { $$ = tregex_repeat($1, 1, 16); }
    | RegexPiece '*' { $$ = tregex_repeat($1, 0, 16); }
    | RegexPiece '{' integer '}' { $$ = tregex_repeat($1, $3, $3); }
    | RegexPiece '{' integer ',' integer '}' { $$ = tregex_repeat($1, $3, $5); }

RegexPiece:
    string_token {
        $$ = tregex_string($1.buf, $1.len);
    }
    | '[' RegexClasses ']' {
        $$ = $2;
    }
    | '(' CompleteRegex ')' {
        $$ = $2;
    }

RegexClasses:
    RegexClass
    | RegexClasses RegexClass {
        $$ = tregex_union_ranges($1, $2);
    }

RegexClass:
    String {
        $$ = tregex_range_from_string($1.buf, $1.len);
    }
    | class_range_token {
        $$ = tregex_range($1.from, $1.to);
    }

%%

int
yyerror(tk_expr_t **param, const char *msg) {
    (void)param;
    extern char *yytext;
    fprintf(stderr, "Parse error near token \"%s\": %s\n", yytext, msg);
    return -1;
}

