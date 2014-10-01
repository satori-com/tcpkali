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
char *format_sockaddr(struct sockaddr *, char *, size_t);

#endif  /* TCPKALI_DNS_H */
