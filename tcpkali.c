#include <getopt.h>
#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* gethostbyname(3) */
#include <libgen.h> /* basename(3) */
#include <err.h>
#include <errno.h>
#include <assert.h>

#include <ev.h>

#include "tcpkali.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_signals.h"

/*
 * Describe the command line options.
 */
static struct option cli_long_options[] = {
    { "help", 0, 0, 'h' },
    { "connections", 0, 0, 'c' },
    { "connect-rate", 1, 0, 'r' },
    { "duration", 1, 0, 't' },
    { "message", 1, 0, 'm' },
    { "message-file", 1, 0, 'f' },
    { "workers", 1, 0, 'w' },
    { "channel-bandwidth", 1, 0, 'b' }
};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage(char *argv0);
struct multiplier { char *prefix; double mult; };
static double parse_with_multipliers(char *str, struct multiplier *, int n);
static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, int *term_flag);
static int read_in_file(const char *filename, void **data, size_t *size);

/*
 * Parse command line options and kick off the engine.
 */
int main(int argc, char **argv) {
    int opt_index;
    int max_connections = 1;
    double connect_rate = 10.0;     /* New connects per second. */
    double test_duration = 10.0;    /* Seconds for the full test. */
    struct engine_params engine_params;
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
    struct multiplier bw_multiplier[] = {
        { "kBps", 1000 },
        { "kbps", 1000/8 },
        { "mBps", 1000000 },
        { "mbps", 1000000/8 }
    };
    memset(&engine_params, 0, sizeof(engine_params));

    while(1) {
        int c;
        c = getopt_long(argc, argv, "hc:m:f:", cli_long_options, &opt_index);
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
            break;
        case 'r':
            connect_rate = parse_with_multipliers(optarg,
                            k_multiplier,
                            sizeof(k_multiplier)/sizeof(k_multiplier[0]));
            if(connect_rate <= 0) {
                fprintf(stderr, "Expected positive --connect-rate=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 't':
            test_duration = parse_with_multipliers(optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(test_duration <= 0) {
                fprintf(stderr, "Expected positive --time=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 'f':
            if(engine_params.message_data) {
                fprintf(stderr, "--message-file: Message is already specified.\n");
                exit(EX_USAGE);
            } else if(read_in_file(optarg, &engine_params.message_data,
                                           &engine_params.message_size) != 0) {
                exit(EX_DATAERR);
            }
            break;
        case 'm': {
            if(engine_params.message_data) {
                fprintf(stderr, "-m: Message is already specified.\n");
                exit(EX_USAGE);
            }
            engine_params.message_data = strdup(optarg);
            engine_params.message_size = strlen(optarg);
            break;
            }
        case 'w': {
            int n = atoi(optarg);
            if(n <= 0) {
                fprintf(stderr, "Expected --workers > 1\n");
                exit(EX_USAGE);
            }
            if(n > 64) {
                fprintf(stderr, "Value --workers=%d is unreasonably large\n",
                    n);
                exit(EX_USAGE);
            }
            engine_params.requested_workers = n;
            break;
            }
        case 'b': {
            int Bps = parse_with_multipliers(optarg,
                        bw_multiplier,
                        sizeof(bw_multiplier)/sizeof(bw_multiplier[0]));
            if(Bps < 0) {
                fprintf(stderr, "Expecting positive --channel-bandwidth\n");
                exit(EX_USAGE);
            }
            engine_params.channel_bandwidth_Bps = Bps;
            break;
            }
        }
    }

    if(optind == argc) {
        fprintf(stderr, "Expecting target <host:port> as an argument. See -h or --help.\n");
        exit(EX_USAGE);
    }

    /*
     * Pick multiple destinations from the command line, resolve them.
     */
    engine_params.addresses = resolve_remote_addresses(&argv[optind],
                                                       argc - optind);
    if(engine_params.addresses.n_addrs == 0) {
        errx(EX_NOHOST, "DNS did not return usable addresses for given host(s)\n");
    } else {
        fprint_addresses(stderr, "Destination: ",
                               "\nDestination: ", "\n",
                               engine_params.addresses);
    }

    /* Block term signals so they're not scheduled in the worker threads. */
    block_term_signals();

    struct engine *eng = engine_start(engine_params);

    /*
     * Convert SIGINT into change of a flag.
     * Has to be run after all other threads are run, otherwise
     * a signal can be delivered to a wrong thread.
     */
    int term_flag = 0;
    flagify_term_signals(&term_flag);

    ev_now_update(EV_DEFAULT);
    double epoch_end = ev_now(EV_DEFAULT) + test_duration;
    if(open_connections_until_maxed_out(eng, connect_rate,
                                        max_connections, epoch_end,
                                        &term_flag) == 0) {
        fprintf(stderr, "Ramped up to %d connections\n", max_connections);
    } else {
        fprintf(stderr, "Could not create %d connection%s"
                        " in allotted time (%0.1fs)\n",
                        max_connections, max_connections==1?"":"s",
                        test_duration);
        exit(1);
    }

    /* Reset the test duration after ramp-up. */
    for(epoch_end = ev_now(EV_DEFAULT) + test_duration;;) {
        if(open_connections_until_maxed_out(eng, connect_rate,
                                            max_connections, epoch_end,
                                            &term_flag) == -1)
            break;
    }

    engine_terminate(eng);

    return 0;
}

static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, int *term_flag) {
    ev_now_update(EV_DEFAULT);
    double now = ev_now(EV_DEFAULT);

    /*
     * It is a little bit better to batch the starts by issuing several
     * start commands per small time tick. Ends up doing less write()
     * operations per batch.
     * Therefore, we round the timeout_us upwards to the nearest millisecond.
     */
    long timeout_us = 1000 * ceil(1000.0/connect_rate);

    struct pacefier keepup_pace;
    pacefier_init(&keepup_pace, now);
    while(now < epoch_end && !*term_flag) {
        usleep(timeout_us);
        ev_now_update(EV_DEFAULT);
        now = ev_now(EV_DEFAULT);
        ssize_t conn_deficit = max_connections - engine_connections(eng);
        if(conn_deficit <= 0) {
            return 0;
        }

        size_t to_start = pacefier_allow(&keepup_pace, connect_rate, now);
        engine_initiate_new_connections(eng,
            to_start < (size_t)conn_deficit ? to_start : (size_t)conn_deficit);
        pacefier_emitted(&keepup_pace, connect_rate, to_start, now);
    }

    return -1;
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

static int
read_in_file(const char *filename, void **data, size_t *size) {
    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long off = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(!off) {
        fprintf(stderr, "%s: Warning: file has no content\n", filename);
    }

    *data = malloc(off + 1);
    size_t r = fread(*data, 1, off, fp);
    assert((long)r == off);
    ((char *)*data)[off] = '\0';    /* Just in case. */
    *size = off;

    fclose(fp);

    return 0;
}

/*
 * Display the Usage screen.
 */
static void
usage(char *argv0) {
    fprintf(stderr, "Usage: %s [OPTIONS] <host:port> [<host:port>...]\n",
        basename(argv0));
    fprintf(stderr,
    "Where OPTIONS are:\n"
    "  -h, --help                  Display this help screen\n"
    "  -c, --connections <N>       Number of channels to open to the destinations\n"
    "  -m, --message <string>      Message to repeatedly send to the remote\n"
    "  -f, --message-file <name>   Read message to send from a file\n"
    "  --connect-rate <R=100>      Number of new connections per second\n"
    "  --workers <N=%ld>%s            Number of parallel threads to use\n"
    "  --channel-bandwidth <Bw>    Limit single channel bandwidth\n"
    //"  --message-rate <N>          Generate N messages per second per channel\n"
    //"  --total-bandwidth <Bw>    Limit total bandwidth (see multipliers below)\n"
    "  --duration <T=10s>          Load test for the specified amount of time\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, mbps (bits per second), kBps, mBps (megabytes per second)\n"
    "  <T>:  s, m, h (seconds, minutes, hours)\n",
    number_of_cpus(), number_of_cpus() < 10 ? " " : ""
    );
}
