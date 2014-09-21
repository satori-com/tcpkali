#include <getopt.h>
#include <sysexits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* gethostbyname(3) */
#include <libgen.h> /* basename(3) */
#include <err.h>
#include <assert.h>

#include <ev.h>

#include "tcpkali.h"
#include "tcpkali_pacefier.h"

/*
 * Describe the command line options.
 */
static struct option cli_long_options[] = {
    { "help", 0, 0, 'h' },
    { "connections", 0, 0, 'c' },
    { "connect-rate", 1, 0, 'r' },
    { "duration", 1, 0, 't' }
};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage(char *argv0);
struct multiplier { char *prefix; double mult; };
static double parse_with_multipliers(char *str, struct multiplier *, int n);
static void keep_connections_open(struct engine *eng, double connect_rate, int max_connections, double test_duration);

/*
 * Parse command line options and kick off the engine.
 */
int main(int argc, char **argv) {
    int opt_index;
    int max_connections = 1;
    double connect_rate = 10;
    double test_duration = 10;
    struct multiplier k_multiplier[] = {
        { "k", 1000 }
    };
    struct multiplier s_multiplier[] = {
        { "ms", 0.001 },
        { "s", 1 },
        { "second", 1 },
        { "seconds", 1 },
        { "m", 60 },
        { "min", 60 },
        { "minute", 60 },
        { "minutes", 60 }
    };

    while(1) {
        int c;
        c = getopt_long(argc, argv, "hc:", cli_long_options, &opt_index);
        if(c == -1)
            break;
        switch(c) {
        case 'h':
            usage(argv[0]);
            exit(EX_USAGE);
        case 'c':
            max_connections = atoi(optarg);
            if(max_connections < 0) {
                fprintf(stderr, "Expecting --connections > 0\n");
                exit(EX_USAGE);
            } else if(max_connections + 10 + number_of_cpus()
                        > max_open_files()) {
                fprintf(stderr, "Number of connections exceeds system limit on open files. Update `ulimit -n`.\n");
                exit(EX_USAGE);
            }
        case 'r':
            connect_rate = parse_with_multipliers(optarg,
                            k_multiplier,
                            sizeof(k_multiplier)/sizeof(k_multiplier[0]));
            if(connect_rate <= 0) {
                fprintf(stderr, "Expected positive --connect-rate=%s\n", optarg);
                exit(EX_USAGE);
            }
        case 't':
            test_duration = parse_with_multipliers(optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(test_duration <= 0) {
                fprintf(stderr, "Expected positive --time=%s\n", optarg);
                exit(EX_USAGE);
            }
        }
    }

    if(optind == argc) {
        fprintf(stderr, "Expecting target <host:port> as an argument. See --help.\n");
        exit(EX_USAGE);
    }

    /*
     * Pick multiple destinations from the command line, resolve them.
     */
    struct addresses addresses;
    addresses = resolve_remote_addresses(&argv[optind], argc-optind);
    if(addresses.n_addrs == 0) {
        errx(EX_NOHOST, "DNS did not return usable addresses for given host(s)\n");
    } else {
        fprint_addresses(stderr, "Destination: ",
                               "\nDestination: ", "\n", addresses);
    }

    struct engine *eng = engine_start(addresses);

    ev_now_update(EV_DEFAULT);
    struct pacefier rampup_pace;
    pacefier_init(&rampup_pace, ev_now(EV_DEFAULT) - 1.0/connect_rate);
    /*
     * It is a little bit better to batch the starts by issuing several
     * start commands per small time tick. Ends up doing less write()
     * operations per batch.
     * Therefore, we round the timeout_us upwards to the nearest millisecond.
     */
    long timeout_us = 1000 * ceil(1000.0/connect_rate);

    do {
        ev_now_update(EV_DEFAULT);
        double now = ev_now(EV_DEFAULT);
        int to_start = pacefier_allow(&rampup_pace, connect_rate, now);
        engine_initiate_new_connections(eng, to_start);
        pacefier_emitted(&rampup_pace, connect_rate, to_start, now);
        usleep(timeout_us);
    } while(engine_connections(eng) < max_connections);

    fprintf(stderr, "Ramped up to %d connections\n", max_connections);

    keep_connections_open(eng, connect_rate, max_connections, test_duration);

    engine_terminate(eng);

    return 0;
}

static void keep_connections_open(struct engine *eng, double connect_rate, int max_connections, double test_duration) {
    ev_now_update(EV_DEFAULT);
    double now = ev_now(EV_DEFAULT);

    double epoch_start = now;
    double epoch_end = now + test_duration;
    long timeout_us = 1000 * ceil(1000.0/connect_rate);

    struct pacefier keepup_pace;
    pacefier_init(&keepup_pace, now - 1.0/connect_rate);
    while(now < epoch_end) {
        usleep(timeout_us);
        ev_now_update(EV_DEFAULT);
        now = ev_now(EV_DEFAULT);
        int conn_deficit = max_connections - engine_connections(eng);
        if(conn_deficit <= 0) {
            pacefier_init(&keepup_pace, now);
            continue;
        }

        int to_start = pacefier_allow(&keepup_pace, connect_rate, now);
        engine_initiate_new_connections(eng,
            to_start < conn_deficit ? to_start : conn_deficit);
        pacefier_emitted(&keepup_pace, connect_rate, to_start, now);
    }
}

static double
parse_with_multipliers(char *str, struct multiplier *ms, int n) {
    char *endptr;
    double value = strtod(str, &endptr);
    if(endptr == str) {
        return -1;
    }
    for(; n > 0; n--, ms++) {
        if(strcmp(endptr, ms->prefix) == 0) {
            value *= ms->mult;
            endptr += strlen(endptr);
        }
    }
    if(*endptr) {
        fprintf(stderr, "Unknown prefix \"%s\" in %s\n", endptr, str);
        return -1;
    }
    return value;
}

/*
 * Display the Usage screen.
 */
static void
usage(char *argv0) {
    fprintf(stderr, "Usage: %s [OPTIONS] <host:port>\n", basename(argv0));
    fprintf(stderr,
    "Where OPTIONS are:\n"
    "  -h, --help                  Display this help screen\n"
    "  -c, --connections <N>       Number of channels to open to the destinations\n"
    "  --connect-rate <R=100>      Number of new connections per second\n"
    //"  --message-rate <N>          Generate N messages per second per channel\n"
    //"  --channel-bandwidth  <Bw>   Limit channel bandwidth (see multipliers below)\n"
    //"  --total-bandwidth  <Bw>     Limit total bandwidth (see multipliers below)\n"
    "  --duration <T=10s>          Load test for the specified amount of time\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, mbps (bits per second), kBps, mBps (megabytes per second)\n"
    "  <T>:  s, m, h (seconds, minutes, hours)\n"
    );
}
