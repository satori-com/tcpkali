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
#define _GNU_SOURCE /* to expose strcasestr(3) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "config.h"

#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#ifdef HAVE_TERM_H
#include <term.h>
#endif

#include "tcpkali_common.h"
#include "tcpkali_terminfo.h"

static int int_utf8 = 0;
static int terminal_width = 80;
static const char *str_clear_eol = "";  // ANSI terminal code: "\033[K";
static char tka_sndbrace[16];
static char tka_rcvbrace[16];
static char tka_warn[16];
static char tka_normal[16];

const char *
tk_attr(enum tk_attribute tka) {
    switch(tka) {
    case TKA_NORMAL:
        return tka_normal;
    case TKA_WARNING:
        return tka_warn;
    case TKA_SndBrace:
        return tka_sndbrace;
    case TKA_RcvBrace:
        return tka_rcvbrace;
    }
    /*
     * Not using the "default:" to prompt warnings
     * if not all enum parameters were used in switch() statement.
     */
    return "";
}

const char *
tcpkali_clear_eol() {
    return str_clear_eol;
}
int
tcpkali_is_utf8() {
    return int_utf8;
}

#ifdef HAVE_LIBNCURSES

static char *
cap(char *cap) {
    return tgetstr(cap, 0) ?: "";
}

static void
enable_cursor(void) {
    printf("%s", cap("ve")); /* cursor_normal */
}

static sig_atomic_t terminal_width_changed = 0;

static void
raise_terminal_width_changed(int _sig UNUSED) {
    terminal_width_changed = 1;
}

int
tcpkali_terminal_width(void) {
    if(terminal_width_changed) {
        terminal_width_changed = 0;
        tcpkali_init_terminal();
    }
    return terminal_width;
}

int
tcpkali_init_terminal(void) {
    int errret = 0;

    if(setupterm(NULL, 1, &errret) == ERR) {
        return -1;
    } else {
        setvbuf(stdout, 0, _IONBF, 0);
    }

    signal(SIGWINCH, raise_terminal_width_changed);
    int n = tgetnum("co");
    if(n > 0) terminal_width = n;

    if(strcasestr(getenv("LANG") ?: "", "utf-8")) int_utf8 = 1;

    /* Obtain the clear end of line string */
    str_clear_eol = cap("ce");

    /* Disable cursor */
    printf("%s", cap("vi"));
    atexit(enable_cursor);

    const char *bold = cap("md");

    snprintf(tka_warn, sizeof(tka_warn),
#if NCURSES_TPARM_VARARGS
             "%s", cap("AF") ? tparm(cap("AF"), COLOR_RED) ?: bold : bold);
#else
             "%s", bold);
#endif

    snprintf(tka_sndbrace, sizeof(tka_sndbrace),
#if NCURSES_TPARM_VARARGS
             "%s", cap("AF") ? tparm(cap("AF"), COLOR_RED) ?: bold : bold);
#else
             "%s", bold);
#endif

    snprintf(tka_rcvbrace, sizeof(tka_rcvbrace),
#if NCURSES_TPARM_VARARGS
             "%s", cap("AF") ? tparm(cap("AF"), COLOR_BLUE) ?: bold : bold);
#else
             "%s", bold);
#endif

    snprintf(tka_normal, sizeof(tka_normal), "%s", cap("me"));

    return 0;
}

#else /* !HAVE_LIBNCURSES */

void
tcpkali_init_terminal(void) {
    return;
}
int
tcpkali_terminal_width(void) {
    return terminal_width;
}

#endif /* HAVE_LIBNCURSES */
