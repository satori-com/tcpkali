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

#include "config.h"

#ifdef  HAVE_CURSES_H
#include <curses.h>
#endif
#ifdef  HAVE_TERM_H
#include <term.h>
#endif

#include "tcpkali_terminfo.h"

static char *str_clear_eol = ""; // ANSI terminal code: "\033[K";

const char *tcpkali_clear_eol() {
    return str_clear_eol;
}

#ifdef  HAVE_LIBNCURSES

static void enable_cursor(void) {
    printf("%s", tgetstr("ve", 0)); /* cursor_normal */
}

void
tcpkali_init_terminal(void) {
    /* Nicer visuals */
    tgetent(0, getenv("TERM"));
    str_clear_eol = tgetstr("ce", 0) ? : "";

    /* Disable cursor */
    printf("%s", tgetstr("vi", 0));
    atexit(enable_cursor);
}

#else   /* !HAVE_LIBNCURSES */

void tcpkali_init_terminal(void) { return; }

#endif  /* HAVE_LIBNCURSES */
