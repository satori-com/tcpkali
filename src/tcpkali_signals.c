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
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "tcpkali_terminfo.h"

/* Condition on which a printer thread waits to print a "Ctrl+C" line. */
static pthread_cond_t printer_cond;
static pthread_cond_t printer_ack_cond;
static pthread_mutex_t printer_lock;

/*
 * When Ctrl+C is pressed, sometimes we linger and do not terminate
 * fast enough. Therefore we need to print something on the screen.
 * We can't do it in a signal handler (not async-signal-safe),
 * and we don't do it several times. So there's a function that report
 * that we've seen the termination signal (Ctrl+C).
 */
static void *
courtesy_ctrlc_printer(void *fp) {
    pthread_mutex_lock(&printer_lock);
    pthread_cond_wait(&printer_cond, &printer_lock);
    fprintf(fp, "%sCtrl+C pressed, finishing up...\n", tcpkali_clear_eol());
    fflush(fp);
    pthread_cond_signal(&printer_ack_cond);
    pthread_mutex_unlock(&printer_lock);

    return NULL;
}

static void
spawn_courtesy_ctrlc_printer() {
    pthread_cond_init(&printer_cond, NULL);
    pthread_cond_init(&printer_ack_cond, NULL);
    pthread_mutex_init(&printer_lock, NULL);
    pthread_t thr;
    (void)pthread_create(&thr, NULL, courtesy_ctrlc_printer,
                         fdopen(fileno(stderr), "w"));
}

static sig_atomic_t *flagvar;
static void
signal_handler(int __attribute__((unused)) sig) {
    /* Wait until another thread output Ctrl+C notice to standard error. */
    pthread_mutex_lock(&printer_lock);
    pthread_cond_signal(&printer_cond);
    pthread_cond_wait(&printer_ack_cond, &printer_lock);
    pthread_mutex_unlock(&printer_lock);

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
flagify_term_signals(sig_atomic_t *flag) {
    sigset_t set;

    spawn_courtesy_ctrlc_printer();

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
