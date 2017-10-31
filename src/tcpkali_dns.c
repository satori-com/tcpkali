/*
 * Copyright (c) 2014  Machine Zone, Inc.
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
#define _GNU_SOURCE
#include <getopt.h>
#include <sysexits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>  /* gethostbyname(3) */
#include <libgen.h> /* basename(3) */
#include <err.h>
#include <assert.h>

#include "tcpkali.h"

/*
 * Convert the given host:port strings into a sequence of all
 * socket addresses corresponding to the ip:port combinations.
 * Note: the number of socket addresses can be greater or less than
 * the number of host:port pairs specified due to aliasing (several
 * hostnames resolving to the same IP address) or multiple-IP response.
 */
struct addresses
resolve_remote_addresses(char **hostports, int nhostports) {
    /*
     * Allocate a bunch of address structures.
     */
    struct addresses addresses = {0, 0};

    for(int n = 0; n < nhostports; n++) {
        struct addrinfo *res = 0;

        resolve_address(hostports[n], &res);

        /* Move all of the addresses into the separate storage */
        for(struct addrinfo *tmp = res; tmp; tmp = tmp->ai_next) {
            address_add(&addresses, tmp->ai_addr);
        }

        freeaddrinfo(res);
    }

    return addresses;
}

void
resolve_address(char *address, struct addrinfo **res) {
    char *hostport = strdup(address);
    char *host = hostport;
    char *service_string = strchr(hostport, ':');
    if(service_string) {
        *service_string++ = '\0';
    } else {
        fprintf(stderr, "Expected :port specification. See --help.\n");
        exit(EX_USAGE);
    }

    char *path = strchr(service_string, '/');
    if(path) *path++ = '\0';

    struct addrinfo hints = {
        .ai_family = PF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_flags = AI_ADDRCONFIG, /* Do not return unroutable IPs */
    };
    int error = getaddrinfo(host, service_string, &hints, res);
    if(error) {
        errx(EX_NOHOST, "Resolving %s:%s: %s", host, service_string,
             gai_strerror(error));
    }

    free(hostport);
}

