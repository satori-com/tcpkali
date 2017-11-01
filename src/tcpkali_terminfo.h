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
#ifndef TCPKALI_TERMINFO_H
#define TCPKALI_TERMINFO_H

/*
 * Initialize smart terminal.
 * RETURN VALUES: -1 for error (dumb terminal), 0 for OK.
 * In case of -1, the (*note) will contain an explanation note.
 */
int tcpkali_init_terminal(const char **note);

void tcpkali_init_kbdinput();
int tcpkali_kbdinput_initialized();

void tcpkali_disable_cursor(void);

int tcpkali_terminal_initialized(void);

/*
 * Capability "clr_eol":
 * Return a string which clears the line until the end of it.
 */
const char *tcpkali_clear_eol(void);

/*
 * Terminal is UTF-8 aware
 */
int tcpkali_is_utf8();

/*
 * Width of the terminal output, in columns.
 */
int tcpkali_terminal_width();

/*
 * Get an escape sequence for special terminal output
 * attributes used by tcpkali.
 */
enum tk_attribute { TKA_NORMAL, TKA_WARNING, TKA_HIGHLIGHT, TKA_SndBrace, TKA_RcvBrace };
const char *tk_attr(enum tk_attribute);

enum keyboard_event {
    KE_NOTHING,
    KE_UP_ARROW,
    KE_DOWN_ARROW,
    KE_ENTER,
    KE_Q
};

enum keyboard_event tcpkali_kbdhit(void);

#endif /* TCPKALI_TERMINFO_H */
