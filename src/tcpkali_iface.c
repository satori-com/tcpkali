/*
 * Copyright (c) 2014, 2015, 2016  Machine Zone, Inc.
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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> /* gethostbyname(3) */
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "tcpkali_common.h"
#include "tcpkali_logging.h"
#include "tcpkali_iface.h"

/*
 * Note: struct sockaddr_in6 is larger than struct sockaddr, hence
 * the storage should be bigger. However, we shall not dereference
 * the AF_INET (struct sockaddr_in *) as it were a larger structure.
 * Therefore this code is rather complex.
 */
void
address_add(struct addresses *aseq, struct sockaddr *sa) {
    /* Reallocate a bigger list and continue. Don't laugh. */
    aseq->addrs =
        realloc(aseq->addrs, (aseq->n_addrs + 1) * sizeof(aseq->addrs[0]));
    assert(aseq->addrs);
    switch(sa->sa_family) {
    case AF_INET:
        *(struct sockaddr_in *)&aseq->addrs[aseq->n_addrs] =
            *(struct sockaddr_in *)sa;
        aseq->n_addrs++;
        break;
    case AF_INET6:
        *(struct sockaddr_in6 *)&aseq->addrs[aseq->n_addrs] =
            *(struct sockaddr_in6 *)sa;
        aseq->n_addrs++;
        break;
    default:
        assert(!"Not IPv4 and not IPv6");
        break;
    }
}

typedef enum {
    SMATCH_ADDR_ONLY,
    SMATCH_ADDR_PORT,
} sockaddrs_cmp_e;
static int
sockaddrs_match(struct sockaddr *sa, struct sockaddr *sb, sockaddrs_cmp_e cmp) {
    if(sa->sa_family == sb->sa_family) {
        switch(sa->sa_family) {
        case AF_INET: {
            struct sockaddr_in *sia = (struct sockaddr_in *)sa;
            struct sockaddr_in *sib = (struct sockaddr_in *)sb;
            if((cmp != SMATCH_ADDR_PORT || sia->sin_port == sib->sin_port)
               && sia->sin_addr.s_addr == sib->sin_addr.s_addr) {
                return 1;
            }
        } break;
        case AF_INET6: {
            struct sockaddr_in6 *sia = (struct sockaddr_in6 *)sa;
            struct sockaddr_in6 *sib = (struct sockaddr_in6 *)sb;
            if((cmp != SMATCH_ADDR_PORT || sia->sin6_port == sib->sin6_port)
               && 0 == memcmp(&sia->sin6_addr, &sib->sin6_addr,
                              sizeof(struct in6_addr))) {
                return 1;
            }
        } break;
        }
    }
    return 0;
}

/*
 * Return non-zero if such address already exists.
 */
static int
address_is_member(struct addresses *aseq, struct sockaddr *sb) {
    for(size_t i = 0; i < aseq->n_addrs; i++) {
        struct sockaddr *sa = (struct sockaddr *)&aseq->addrs[i];
        if(sockaddrs_match(sa, sb, SMATCH_ADDR_PORT)) return 1;
    }

    return 0;
}

/*
 * Display destination addresses with a given prefix, separator and suffix.
 */
void
fprint_addresses(FILE *fp, char *prefix, char *separator, char *suffix,
                 struct addresses addresses) {
    for(size_t n = 0; n < addresses.n_addrs; n++) {
        if(n == 0) {
            fprintf(fp, "%s", prefix);
        } else {
            fprintf(fp, "%s", separator);
        }
        char buf[INET6_ADDRSTRLEN + 64];
        fprintf(stderr, "%s",
                format_sockaddr(&addresses.addrs[n], buf, sizeof(buf)));
        if(n == addresses.n_addrs - 1) {
            fprintf(fp, "%s", suffix);
        }
    }
}

/*
 * Printable representation of a sockaddr.
 */
const char *
format_sockaddr(struct sockaddr_storage *ss, char *buf, size_t size) {
    void *in_addr;
    uint16_t nport;
    switch(ss->ss_family) {
    case AF_INET:
        in_addr = &((struct sockaddr_in *)ss)->sin_addr;
        nport = ((struct sockaddr_in *)ss)->sin_port;
        break;
    case AF_INET6:
        in_addr = &((struct sockaddr_in6 *)ss)->sin6_addr;
        nport = ((struct sockaddr_in6 *)ss)->sin6_port;
        break;
    default:
        assert(!"ipv4 or ipv6 expected");
        return "<unknown>";
    }
    char ipbuf[INET6_ADDRSTRLEN];
    const char *ip = inet_ntop(ss->ss_family, in_addr, ipbuf, sizeof(ipbuf));
    snprintf(buf, size, "[%s]:%d", ip, ntohs(nport));
    return buf;
}

/*
 * Given a port, detect which addresses we can listen on, using this port.
 */
struct addresses
detect_listen_addresses(const char *local_hostname, int listen_port) {
    struct addresses addresses = {0, 0};
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG};
    char service[32];
    snprintf(service, sizeof(service), "%d", listen_port);

    struct addrinfo *res;
    int err = getaddrinfo(local_hostname, service, &hints, &res);
    if(err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    /* Move all of the addresses into the separate storage */
    for(struct addrinfo *tmp = res; tmp; tmp = tmp->ai_next) {
        address_add(&addresses, tmp->ai_addr);
    }

    freeaddrinfo(res);

    fprint_addresses(stderr, "Listen on: ", "\nListen on: ", "\n", addresses);

    return addresses;
}

/*
 * Check whether we can bind to a specified IP.
 */
static int
check_if_bindable_ip(struct sockaddr_storage *ss) {
    int rc;
    int lsock = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP);
    assert(lsock != -1);
    rc = bind(lsock, (struct sockaddr *)ss, sockaddr_len(ss));
    close(lsock);
    if(rc == -1) {
        char buf[256];
        fprintf(stderr, "%s is not local: %s\n",
                format_sockaddr(ss, buf, sizeof(buf)), strerror(errno));
        return -1;
    }
    return 0;
}

/*
 * Parse the specified IP as it were a source IP, and add it to the list.
 */
int
add_source_ip(struct addresses *addresses, const char *optarg) {
    struct addrinfo hints = {.ai_family = AF_UNSPEC,
                             .ai_socktype = SOCK_STREAM,
                             .ai_protocol = IPPROTO_TCP,
                             .ai_flags = AI_PASSIVE | AI_ADDRCONFIG};

    struct addrinfo *res;
    int err = getaddrinfo(optarg, NULL, &hints, &res);
    if(err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    /* Move all of the addresses into the separate storage */
    for(struct addrinfo *tmp = res; tmp; tmp = tmp->ai_next) {
        address_add(addresses, tmp->ai_addr);
        if(check_if_bindable_ip(&addresses->addrs[addresses->n_addrs - 1])
           < 0) {
            freeaddrinfo(res);
            return -1;
        }
    }

    freeaddrinfo(res);

    return 0;
}

static void
reset_port(struct sockaddr_storage *ss, in_port_t new_port_value) {
    switch(ss->ss_family) {
    case AF_INET: {
        struct sockaddr_in *sin = (struct sockaddr_in *)ss;
        sin->sin_port = new_port_value;
    } break;
    case AF_INET6: {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *)ss;
        sin->sin6_port = new_port_value;
    } break;
    default:
        assert(!"Not IPv4 and not IPv6");
        break;
    }
}

/*
 * Return the name of the interface which contains given IP.
 */
static const char *
interface_by_addr(struct ifaddrs *ifp, struct sockaddr *addr) {
    for(; ifp; ifp = ifp->ifa_next) {
        if(ifp->ifa_addr
           && sockaddrs_match(addr, ifp->ifa_addr, SMATCH_ADDR_ONLY)) {
            return ifp->ifa_name;
        }
    }

    return NULL;
}

static int
detect_local_ip_for_remote(struct ifaddrs *ifp, struct sockaddr_storage *ss,
                           struct sockaddr_storage *r_local_addr) {
    char tmpbuf[128];

    int sockfd = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd == -1) {
        fprintf(stderr, "Could not open %s socket: %s\n",
                ss->ss_family == AF_INET6 ? "IPv6" : "IPv4", strerror(errno));
        return -1;
    }

    /* Enable non-blocking mode. */
    int rc = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    assert(rc != 1);

    /*
     * Try connecting to a destination, and figure out the IP of the local
     * end of the connection while connection is pending. Then close quickly.
     */
    rc = connect(sockfd, (struct sockaddr *)ss, sockaddr_len(ss));
    if(rc == -1 && errno != EINPROGRESS) {
        fprintf(stderr, "Could not pre-check connection to %s: %s\n",
                format_sockaddr(ss, tmpbuf, sizeof(tmpbuf)), strerror(errno));
        close(sockfd);
        return -1;
    } else {
        /*
         * We could not connect to an address. Supposedly this is related
         * to the fact that the system rather early figured we're not
         * supposed to connect there. It can be related to IP filtering,
         * or socket resource exhaustion, or the fact that we're connecting
         * to something very local. A non-blocking connection
         * to a local host may also result in EINPROGRESS, but that's
         * probably not guaranteed. So, we check using a different algorithm
         * if destination IP is local, then just use that interface.
         */
        const char *iface = interface_by_addr(ifp, (struct sockaddr *)ss);
        if(iface) {
            *r_local_addr = *ss;
            reset_port(r_local_addr, 0);
            close(sockfd);
            return 0;
        }
    }

    socklen_t local_addrlen = sizeof(*r_local_addr);
    if(getsockname(sockfd, (struct sockaddr *)r_local_addr, &local_addrlen)
       == -1) {
        fprintf(stderr,
                "Could not get local address when connecting to %s: %s\n",
                format_sockaddr(ss, tmpbuf, sizeof(tmpbuf)), strerror(errno));
        close(sockfd);
        return -1;
    } else {
        /* We're going to bind to that port at some point. Must be zero. */
        reset_port(r_local_addr, 0);
    }

    close(sockfd);
    return 0;
}

static int
compare_ifnames(const char *a, const char *b) {
    for(;; a++, b++) {
        switch(*a) {
        case '\0':
        case ':':
            if(*b == '\0' || *b == ':') break;
            return -1;
        default:
            if(*a == *b) continue;
            return -1;
        }
        break;
    }

    return 0;
}

#ifdef TCPKALI_IFACE_UNIT_TEST
int
main() {
    const char *non_equivalents[][5] = {{"", "a", "b", "ab", "a0"},
                                        {"a", "a0", "a1", "b0", "b1"}};
    const char *equivalents[][5] = {
        {"", ":", ":0", ":1", ":123"},
        {"a", "a:", "a:0", "a:1", "a:123"},
        {"ab", "ab:", "ab:0", "ab:1", "ab:123"},
        {"bond0", "bond0:", "bond0:0", "bond0:1", "bond0:123"}};

    /* Test non-equivalence */
    for(size_t test = 0; test < 2; test++) {
        for(size_t i = 0; i < 5; i++) {
            for(size_t j = 0; j < 5; j++) {
                if(i == j) continue;
                const char *a = non_equivalents[test][i];
                const char *b = non_equivalents[test][j];
                assert(compare_ifnames(a, b) == -1);
            }
        }
    }

    /* Test equivalence */
    for(size_t test = 0; test < 4; test++) {
        for(size_t i = 0; i < 5; i++) {
            for(size_t j = 0; j < 5; j++) {
                if(i == j) continue;
                const char *a = equivalents[test][i];
                const char *b = equivalents[test][j];
                assert(compare_ifnames(a, b) == 0);
            }
        }
    }

    return 0;
}
#endif


static int
collect_interface_addresses(struct ifaddrs *ifp, const char *ifname,
                            sa_family_t family, struct addresses *ss) {
    char tmpbuf[256];
    int found = 0;

    for(; ifp; ifp = ifp->ifa_next) {
        if(ifp->ifa_addr && family == ifp->ifa_addr->sa_family
           && compare_ifnames(ifp->ifa_name, ifname) == 0) {
            /* Add address if it is not already there. */
            if(!address_is_member(ss, ifp->ifa_addr)) {
                fprintf(
                    stderr, "Interface %s address %s\n", ifname,
                    format_sockaddr((struct sockaddr_storage *)ifp->ifa_addr,
                                    tmpbuf, sizeof(tmpbuf)));
                address_add(ss, ifp->ifa_addr);
            }
            found = 1;
        }
    }

    return found ? 0 : -1;
}

/*
 * Given a list of destination addresses, populate the list of source
 * addresses with compatible source (local) IPs.
 */
int
detect_source_ips(struct addresses *dsts, struct addresses *srcs) {
    struct ifaddrs *interfaces = 0;

    int rc = getifaddrs(&interfaces);
    if(rc == -1) {
        /* Can't get interfaces... Won't try to use several source IPs. */
        warning(
            "Can't enumerate interfaces, "
            "won't use multiple source IPs: %s\n",
            strerror(errno));
        return 0;
    }

    /* If we are not supposed to go anywhere, we won't invoke this function. */
    if(dsts->n_addrs == 0) {
        fprintf(stderr,
                "Source IP detection failed: "
                "No destination IPs are given\n");
        freeifaddrs(interfaces);
        return -1;
    }

    sa_family_t common_ss_family = 0;

    for(size_t dst_idx = 0; dst_idx < dsts->n_addrs; dst_idx++) {
        struct sockaddr_storage *ds = &dsts->addrs[dst_idx];
        char tmpbuf[256];

        /*
         * For now, we can reliably detect source ips only
         * when the address families of the destination ips are the same.
         */
        if(common_ss_family == 0) {
            common_ss_family = ds->ss_family;
        } else if(common_ss_family != ds->ss_family) {
            warning(
                "Could not detect local address when connecting to %s:"
                " Multiple incompatible address families in destination.\n",
                format_sockaddr(ds, tmpbuf, sizeof(tmpbuf)));
            warning("Would not open more than 64k connections to %s\n",
                    format_sockaddr(ds, tmpbuf, sizeof(tmpbuf)));
            srcs->n_addrs = 0;
            freeifaddrs(interfaces);
            return 0;
        }

        /*
         * Attempt to create a connection and see what our
         * local address looks like. Then search for that address
         * among the interfaces.
         */
        struct sockaddr_storage local_addr;
        if(detect_local_ip_for_remote(interfaces, ds, &local_addr)) {
            freeifaddrs(interfaces);
            return -1;
        }

        const char *ifname =
            interface_by_addr(interfaces, (struct sockaddr *)&local_addr);
        if(ifname == NULL) {
            warning("Can't determine local interface to connect to %s\n",
                    format_sockaddr(ds, tmpbuf, sizeof(tmpbuf)));
            warning("Would not open more than 64k connections to %s\n",
                    format_sockaddr(ds, tmpbuf, sizeof(tmpbuf)));
            srcs->n_addrs = 0;
            freeifaddrs(interfaces);
            return 0;
        }

        if(collect_interface_addresses(interfaces, ifname, ds->ss_family, srcs)
           == 0) {
            fprintf(stderr, "Using interface %s to connect to %s\n", ifname,
                    format_sockaddr(ds, tmpbuf, sizeof(tmpbuf)));
        } else {
            fprintf(stderr, "Failed to collect IPs from interface %s\n",
                    ifname);
            freeifaddrs(interfaces);
            return -1;
        }
    }

    freeifaddrs(interfaces);
    return 0;
}
