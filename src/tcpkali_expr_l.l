%{

#include <stdio.h>
#include <stdlib.h>

#include "tcpkali_expr.h"
#include "tcpkali_expr_y.h"

int transport_expr_lex(void);

#define yyterminate()   return END;

%}

%option never-interactive
%option noinput
%option noyywrap stack

%x in_expression

WSP [\t\r\v\f\n ]

%%

<INITIAL>{
        "\\{"       { yy_push_state(in_expression); return '{'; }

        [^\\]+  {
            yylval.tv_string.buf = malloc(yyleng + 1);
            yylval.tv_string.len = yyleng;
            memcpy(yylval.tv_string.buf, yytext, yyleng);
            yylval.tv_string.buf[yyleng] = '\0';
            return arbitrary_string;
        }

    }

<INITIAL>.              return yytext[0];
<INITIAL><<EOF>>        yyterminate();


<in_expression>{
    "}"             { yy_pop_state(); return '}'; }
    {WSP}+          /* Ignore whitespace */
    "connection"    return TOK_connection;
    "ptr"           return TOK_ptr;
    "."             return '.';
    .     {
                    fprintf(stderr,
                        "Unexpected token in message expression: %s\n",
                        yytext);
                    yy_top_state(); /* Just to use this function */
                    (void)yyunput;  /* Just to use this symbol */
                    return -1;
            }

    [^} .]+     {
                    fprintf(stderr,
                        "Unexpected token in message expression: %s\n",
                        yytext);
                    yy_top_state(); /* Just to use this function */
                    (void)yyunput;  /* Just to use this symbol */
                    return -1;
                }
    }

%%
