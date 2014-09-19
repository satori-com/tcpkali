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

/*
 * Describe the command line options.
 */
static struct option cli_long_options[] = {
    { "help", 0, 0, 'h' }
};

/*
 * A list of IPv4/IPv6 addresses.
 */
struct addresses {
    struct sockaddr *addrs;
    int n_addrs;
};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage(char *argv0);
static struct addresses *pickup_remote_addresses(char **hostports, int n);
static void fprint_addresses(FILE *, char *prefix, char *separator, char *suffix, struct addresses *);

/*
 * Parse command line options and kick off the engine.
 */
int main(int argc, char **argv) {
    int opt_index;

    while(1) {
        int c;
        c = getopt_long(argc, argv, "h", cli_long_options, &opt_index);
        if(c == -1)
            break;
        switch(c) {
        case 'h':
            usage(argv[0]);
            exit(EX_USAGE);
        }
    }

    if(optind == argc) {
        fprintf(stderr, "Expecting target <host:port> as an argument. See --help.\n");
        exit(EX_USAGE);
    }

    /*
     * Pick multiple destinations from the command line.
     */
    struct addresses *addresses;
    addresses = pickup_remote_addresses(&argv[optind], argc-optind);
    if(addresses->n_addrs == 0) {
        errx(EX_NOHOST, "DNS did not return usable addresses for given host(s)\n");
    }

    fprint_addresses(stderr, "Destination: ",
                         "\nDestination: ", "\n", addresses);

    return 0;
}

/*
 * Convert the given host:port strings into a sequence of all
 * socket addresses corresponding to the ip:port combinations.
 * Note: the number of socket addresses can be greater or less than
 * the number of host:port pairs specified due to aliasing (several
 * hostnames resolving to the same IP address) or multiple-IP response.
 */
static struct addresses *pickup_remote_addresses(char **hostports, int nhostports) {
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

    struct addresses *addresses = malloc(sizeof(*addresses));
    assert(addresses);
    addresses->addrs = addrs;
    addresses->n_addrs = n_addrs;

    return addresses;
}
    

/*
 * Display destination addresses with a given prefix, separator and suffix.
 */
static void fprint_addresses(FILE *fp, char *prefix, char *separator, char *suffix, struct addresses *addresses) {
    for(int n = 0; n < addresses->n_addrs; n++) {
        void *in_addr;
        uint16_t nport;
        char buf[INET6_ADDRSTRLEN];
        switch(addresses->addrs[n].sa_family) {
        case AF_INET:
            in_addr = &((struct sockaddr_in *)&addresses->addrs[n])->sin_addr;
            nport = ((struct sockaddr_in *)&addresses->addrs[n])->sin_port;
            break;
        case AF_INET6:
            in_addr = &((struct sockaddr_in6 *)&addresses->addrs[n])->sin6_addr;
            nport = ((struct sockaddr_in6 *)&addresses->addrs[n])->sin6_port;
            break;
        default:
            assert(!"ipv4 or ipv6 expected");
        }
        const char *ip = inet_ntop(addresses->addrs[n].sa_family, in_addr,
                                   buf, sizeof(buf));
        if(n == 0) {
            fprintf(fp, "%s", prefix);
        } else {
            fprintf(fp, "%s", separator);
        }
        fprintf(stderr, "[%s]:%d", ip, ntohs(nport));
        if(n == addresses->n_addrs - 1) {
            fprintf(fp, "%s", suffix);
        }
    }
}

/*
 * Display the Usage screen.
 */
static void usage(char *argv0) {
    fprintf(stderr, "Usage: %s [OPTIONS] <host:port>\n", basename(argv0));
    fprintf(stderr,
    "Where OPTIONS are:\n"
    "  -h, --help                  Display this help screen\n"
    "  -c, --connections <N>       Number of channels to open to the destinations\n"
    "  --connect-rate <R>          Number of new connections per second\n"
    "  --message-rate <N>          Generate N messages per second per channel\n"
    "  --channel-bandwidth  <Bw>   Limit channel bandwidth (see multipliers below)\n"
    "  --total-bandwidth  <Bw>     Limit total bandwidth (see multipliers below)\n"
    "  --time <T>                  Load test for the specified amount of time\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, mbps (bits per second), kBps, mBps (megabytes per second)\n"
    "  <T>:  s, m, h (seconds, minutes, hours)\n"
    );
}
