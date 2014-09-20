#ifndef TCPKALI_DNS_H
#define TCPKALI_DNS_H

#include <stdio.h>

/*
 * A list of IPv4/IPv6 addresses.
 */
struct addresses {
    struct sockaddr *addrs;
    int n_addrs;
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

/*
 * Return the string representing the socket address.
 * The size should be at least INET6_ADDRSTRLEN+64.
 */
char *format_sockaddr(struct sockaddr *, char *, size_t);

#endif  /* TCPKALI_DNS_H */
