%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcpkali_data.h"
#include "tcpkali_expr.h"
#include "tcpkali_expr_y.h"

int transport_expr_lex(void);

#define yyterminate()   return END;

%}

%option never-interactive
%option noinput
%option noyywrap stack

%x in_expression
%x in_filename

WSP [\t\r\v\f\n ]

%%

<INITIAL>{
        "\\{"       { yy_push_state(in_expression); return '{'; }

        "\\"|"\\\\"|[^\\]+  {
            yylval.tv_string.buf = malloc(yyleng + 1);
            yylval.tv_string.len = yyleng;
            memcpy(yylval.tv_string.buf, yytext, yyleng);
            yylval.tv_string.buf[yyleng] = '\0';
            return string_token;
        }
    }

<INITIAL><<EOF>>        yyterminate();

<in_filename>{

    {WSP}+          /* Ignore whitespace */

    /* Any not too brace-y characters within <> brackets parsed as a filename.
     * Backslashes are OK.
     */
    [^]["'{}<>() ]+   {
            yylval.tv_string.buf = strdup(yytext);
            yylval.tv_string.len = strlen(yytext);
            return filename;
        }

    /* A file can be fully enclosed in quotes, if you wish.
     * Backslashes are OK if not before the quotation mark.
     */
    \"([^\\"\r\n]|\\[^"])*\" {
            size_t new_size = yyleng - 2;
            char *new_str = malloc(new_size + 1);
            memcpy(new_str, yytext + 1, new_size);
            new_str[new_size] = '\0';
            yylval.tv_string.buf = new_str;
            yylval.tv_string.len = new_size;
            return filename;
        }

    /* "c:\windows" is ok, "c:\windows\" is not. */
    [^>]+\\\"        {
            fprintf(stderr, "Unexpected filename format: %s ends with a backslashed quote\n", yytext);
            return -1;
        }

    [^> ]+        {
            fprintf(stderr, "Unexpected filename format: %s\n", yytext);
            return -1;
        }

    ">"         yy_pop_state();
}

<in_expression>{
    "<"             { yy_push_state(in_filename); }
    "}"             { yy_pop_state(); return '}'; }
    {WSP}+          /* Ignore whitespace */
    "ws"            return TOK_ws;
    "cont""inuation"?  { yylval.tv_opcode = WS_OP_CONTINUATION;
                      return TOK_ws_opcode; }
    "text"          { yylval.tv_opcode = WS_OP_TEXT_FRAME;
                      return TOK_ws_opcode; }
    "binary"        { yylval.tv_opcode = WS_OP_BINARY_FRAME;
                      return TOK_ws_opcode; }
    "close"         { yylval.tv_opcode = WS_OP_CLOSE;
                      return TOK_ws_opcode; }
    "ping"          { yylval.tv_opcode = WS_OP_PING;
                      return TOK_ws_opcode; }
    "pong"          { yylval.tv_opcode = WS_OP_PONG;
                      return TOK_ws_opcode; }
    "connection"    return TOK_connection;
    "ptr"           return TOK_ptr;
    "uid"           return TOK_uid;
    [0-9]+  {
            yylval.tv_long = atol(yytext);
            return integer;
        }
    "."             return '.';
    "..."           return TOK_ellipsis;
    "%"             return '%';


    \"([^\\"]|\\.)*\" {
                    size_t new_size = yyleng - 2;
                    char *new_str = malloc(new_size + 1);
                    memcpy(new_str, yytext + 1, new_size);
                    new_str[new_size] = '\0';
                    yylval.tv_string.buf = new_str;
                    yylval.tv_string.len = new_size;
                    return quoted_string;
                }

    [^"<} .]+     {
                    fprintf(stderr,
                        "Unexpected token in message expression: %s\n",
                        yytext);
                    yy_top_state(); /* Just to use this function */
                    (void)yyunput;  /* Just to use this symbol */
                    return -1;
                }

    .     {
                    fprintf(stderr,
                        "Unexpected token in message expression: %s\n",
                        yytext);
                    yy_top_state(); /* Just to use this function */
                    (void)yyunput;  /* Just to use this symbol */
                    return -1;
                }

}

%%

