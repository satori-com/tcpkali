#define _GNU_SOURCE
#include <getopt.h>
#include <sysexits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
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
struct addresses resolve_remote_addresses(char **hostports, int nhostports) {
    /*
     * Allocate a bunch of address structures.
     */
    int n_addrs = 0;
    int n_addrs_max = 64;
    struct sockaddr *addrs = malloc(n_addrs_max * sizeof(addrs[0]));
    assert(addrs);

    for(int n = 0; n < nhostports; n++) {
        char *hostport = hostports[n];
        char *service_string = strchr(hostport, ':');
        if(service_string) {
            service_string++;
        } else {
            fprintf(stderr, "Expected :port specification. See --help.\n");
            exit(EX_USAGE);
        }

        char *host = strndup(hostport, (service_string - hostport) - 1);

        struct addrinfo hints = {
            .ai_family = PF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
            .ai_flags = AI_ADDRCONFIG, /* Do not return unroutable IPs */
        };
        struct addrinfo *res = 0;
        int error = getaddrinfo(host, service_string, &hints, &res);
        if(error) {
            errx(EX_NOHOST, "Resolving %s:%s: %s",
                host, service_string, gai_strerror(error));
        }

        /* Move all of the addresses into the separate storage */
        for(struct addrinfo *tmp = res; tmp; tmp = tmp->ai_next) {
            if(n_addrs >= n_addrs_max) {
                /* Reallocate a bigger list and continue. */
                n_addrs_max *= 1.5;
                addrs = realloc(addrs, n_addrs_max * sizeof(addrs[0]));
                assert(addrs);
            }
            addrs[n_addrs] = *tmp->ai_addr;
            n_addrs++;
        }

        freeaddrinfo(res);
    }

    struct addresses addresses = { .addrs = addrs, .n_addrs = n_addrs };

    return addresses;
}
    

/*
 * Display destination addresses with a given prefix, separator and suffix.
 */
void fprint_addresses(FILE *fp, char *prefix, char *separator, char *suffix, struct addresses addresses) {
    for(int n = 0; n < addresses.n_addrs; n++) {
        if(n == 0) {
            fprintf(fp, "%s", prefix);
        } else {
            fprintf(fp, "%s", separator);
        }
        char buf[INET6_ADDRSTRLEN+64];
        fprintf(stderr, "%s", format_sockaddr(&addresses.addrs[n],
                              buf, sizeof(buf)));
        if(n == addresses.n_addrs - 1) {
            fprintf(fp, "%s", suffix);
        }
    }
}

/*
 * Printable representation of a sockaddr.
 */
char *format_sockaddr(struct sockaddr *sa, char *buf, size_t size) {
    void *in_addr;
    uint16_t nport;
    switch(sa->sa_family) {
    case AF_INET:
        in_addr = &((struct sockaddr_in *)sa)->sin_addr;
        nport = ((struct sockaddr_in *)sa)->sin_port;
        break;
    case AF_INET6:
        in_addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
        nport = ((struct sockaddr_in6 *)sa)->sin6_port;
        break;
    default:
        assert(!"ipv4 or ipv6 expected");
    }
    char ipbuf[INET6_ADDRSTRLEN];
    const char *ip = inet_ntop(sa->sa_family, in_addr, ipbuf, sizeof(ipbuf));
    snprintf(buf, size, "[%s]:%d", ip, ntohs(nport));
    return buf;
}

