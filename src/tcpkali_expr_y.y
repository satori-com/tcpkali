%{

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tcpkali_expr.h"

int yylex(void);
int yyerror(const char *);

#define YYPARSE_PARAM   param
#define YYERROR_VERBOSE

%}

%union  {
    tk_expr_t   *tv_expr;
    struct {
        char  *buf;
        size_t len;
    } tv_string;
    char  tv_char;
};

%token              TOK_connection   "connection"
%token              TOK_ptr          " ptr"
%token              TOK_uid          "uid"
%token              END 0            "end of expression"
%token  <tv_string> arbitrary_string

%type   <tv_expr>   Expr    "expression"
%type   <tv_expr>   DataOrExpr "some string or \\{expression}"
%type   <tv_expr>   DataAndExpressions "data and expressions"

%%

Grammar:
    END {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        *(tk_expr_t **)param = expr;
        return 0;
    }
    | DataAndExpressions END {
        *(tk_expr_t **)param = $1;
        return 0;
    }

DataAndExpressions:
    DataOrExpr {
        $$ = $1;
    }
    | DataOrExpr DataAndExpressions {
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        if($1->type == EXPR_DATA && $2->type == EXPR_DATA) {
            /* Concatenate two strings for simplicity */
            expr->type = EXPR_DATA;
            expr->u.data.size = $1->u.data.size + $2->u.data.size;
            expr->u.data.data = malloc(expr->u.data.size + 1);
            memcpy((char *)expr->u.data.data, $1->u.data.data, $1->u.data.size);
            memcpy((char *)expr->u.data.data + $1->u.data.size, $2->u.data.data,
                                                        $2->u.data.size);
            ((char *)expr->u.data.data)[expr->u.data.size] = '\0';
            free((void *)$1->u.data.data);
            free((void *)$2->u.data.data);
        } else {
            expr->type = EXPR_CONCAT;
            expr->u.concat.expr[0] = $1;
            expr->u.concat.expr[1] = $2;
            expr->estimate_size = $1->estimate_size + $2->estimate_size;
        }
        $$ = expr;
    }

DataOrExpr:
    arbitrary_string {
        /* If there's nothing to parse, don't return anything */
        tk_expr_t *expr = calloc(1, sizeof(tk_expr_t));
        expr->type = EXPR_DATA;
        expr->u.data.data = ($1).buf;
        expr->u.data.size = ($1).len;
        expr->estimate_size = ($1).len;
        $$ = expr;
    }
    | '{' Expr '}' {
        $$ = $2;
    }

Expr:
    TOK_connection '.' TOK_ptr {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_CONNECTION_PTR;
        $$->estimate_size = sizeof("100000000000000");
    }
    | TOK_connection '.' TOK_uid {
        $$ = calloc(1, sizeof(*($$)));
        $$->type = EXPR_CONNECTION_UID;
        $$->estimate_size = sizeof("100000000000000");
    }

%%

int
yyerror(const char *msg) {
        extern char *yytext;
        fprintf(stderr, "Parse error near token \"%s\": %s\n", yytext, msg);
        return -1;
}

