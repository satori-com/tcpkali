/*
    tcpkali: fast multi-core TCP load generator.

    Original author: Lev Walkin <lwalkin@machinezone.com>

    Copyright (C) 2014  Machine Zone, Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#define _POSIX_SOURCE
#define _BSD_SOURCE
#include <stdio.h>
#include <string.h>
#include <signal.h>

static int *flagvar;
static void signal_handler(int __attribute__((unused)) sig) {
    fprintf(stderr, "Ctrl+C pressed, finishing up...\n");
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
flagify_term_signals(int *flag) {
    sigset_t set;
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

