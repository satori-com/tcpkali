/*
 * Copyright (c) 2014, 2016  Machine Zone, Inc.
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
#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200112
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "tcpkali_terminfo.h"

static struct {
    size_t len;
    char buf[64];
} ctrlc_message;

static volatile sig_atomic_t *flagvar;
static void
signal_handler(int __attribute__((unused)) sig) {
    /* Wait until another thread output Ctrl+C notice to standard error. */

    /* If we can't write to stderr atomically when the user is
     * interrupting the program, we have a bigger mess;
     * don't attempt to rectify it here. Partial write is ok.
     */
    (void)write(STDERR_FILENO, ctrlc_message.buf, ctrlc_message.len);

    *flagvar = 1;
}

void
block_term_signals() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, 0);
}

void
flagify_term_signals(volatile sig_atomic_t *flag) {
    sigset_t set;

    /* Pre-create the Ctrl+C message to be async-signal-safe. */
    int len =
        snprintf(ctrlc_message.buf, sizeof(ctrlc_message.buf),
                 "%sCtrl+C pressed, finishing up...\n", tcpkali_clear_eol());
    if(len > 0 && (size_t)len < sizeof(ctrlc_message.buf)) {
        ctrlc_message.len = len;
    }

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_UNBLOCK, &set, 0);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_mask = set;
    act.sa_flags = SA_RESETHAND | SA_RESTART;
    act.sa_handler = signal_handler;
    flagvar = flag;
    sigaction(SIGINT, &act, 0);
}
