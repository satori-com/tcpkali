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
#ifndef TCPKALI_IFACE_H
#define TCPKALI_IFACE_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tcpkali_common.h"

/*
 * A list of IPv4/IPv6 addresses.
 */
struct addresses {
    struct sockaddr_storage *addrs;
    size_t n_addrs;
};

/* Note: sizeof(struct sockaddr_in6) > sizeof(struct sockaddr *)! */
static inline socklen_t UNUSED
sockaddr_len(struct sockaddr_storage *ss) {
    switch(ss->ss_family) {
    case AF_INET:
        return sizeof(struct sockaddr_in);
    case AF_INET6:
        return sizeof(struct sockaddr_in6);
    }
    assert(!"Not IPv4 and not IPv6");
    return 0;
}

/*
 * Print the IP addresses into the specified stdio channel.
 */
void fprint_addresses(FILE *, char *prefix, char *separator, char *suffix,
                      struct addresses);

void address_add(struct addresses *, struct sockaddr *sa);

/*
 * Return the string representing the socket address.
 * The size should be at least INET6_ADDRSTRLEN+64.
 */
const char *format_sockaddr(struct sockaddr_storage *, char *, size_t);

/*
 * Detect IPs on which we can listen.
 */
struct addresses detect_listen_addresses(const char *local_hostname, int listen_port);

/*
 * Parse the specified IP as it were a source IP, and add it to the list.
 */
int add_source_ip(struct addresses *addresses, const char *str);

/*
 * Given a list of destination addresses, populate the list of source
 * addresses with compatible source (local) IPs.
 */
int detect_source_ips(struct addresses *dsts, struct addresses *srcs);

#endif /* TCPKALI_IFACE_H */
