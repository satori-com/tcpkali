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
#ifndef TCPKALI_DNS_H
#define TCPKALI_DNS_H

#include <stdio.h>

/*
 * A list of IPv4/IPv6 addresses.
 */
struct addresses {
    struct sockaddr_storage *addrs;
    size_t n_addrs;
};

/*
 * Given a sequence of host:port strings, return all of the resolved IP
 * addresses.
 */
struct addresses resolve_remote_addresses(char **hostports, int n);

/*
 * Print the IP addresses into the specified stdio channel.
 */
void fprint_addresses(FILE *, char *prefix, char *separator, char *suffix, struct addresses);

void address_add(struct addresses *, struct sockaddr *sa);

/*
 * Return the string representing the socket address.
 * The size should be at least INET6_ADDRSTRLEN+64.
 */
const char *format_sockaddr(struct sockaddr *, char *, size_t);

#endif  /* TCPKALI_DNS_H */
