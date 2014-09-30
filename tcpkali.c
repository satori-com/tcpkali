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
#include <ifaddrs.h>
#include <err.h>
#include <errno.h>
#include <assert.h>

#include <ev.h>
#include <statsd.h>

#include "tcpkali.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_signals.h"
#include "tcpkali_syslimits.h"

/*
 * Describe the command line options.
 */
#define CLI_STATSD_OFFSET   256
#define CLI_CHAN_OFFSET  512
#define CLI_CONN_OFFSET  1024
static struct option cli_long_options[] = {
    { "help", 0, 0, 'h' },
    { "connections", 0, 0, 'c' },
    { "connect-rate", 1, 0, 'r' },
    { "duration", 1, 0, 't' },
    { "message", 1, 0, 'm' },
    { "message-file", 1, 0, 'f' },
    { "message-rate", 1, 0, 'M' },
    { "message-first", 1, 0, '1' },
    { "workers", 1, 0, 'w' },
    { "channel-bandwidth", 1, 0, 'b' },
    { "statsd", 0, 0,           CLI_STATSD_OFFSET + 'e' },
    { "statsd-server", 1, 0,    CLI_STATSD_OFFSET + 's' },
    { "statsd-port", 1, 0,      CLI_STATSD_OFFSET + 'p' },
    { "statsd-namespace", 1, 0, CLI_STATSD_OFFSET + 'n' },
    { "connect-timeout", 1, 0,  CLI_CONN_OFFSET + 't' },
    { "channel-lifetime", 1, 0, CLI_CHAN_OFFSET + 't' },
    { "listen-port", 1, 0, 'l' }
};

static struct tcpkali_config {
    int max_connections;
    double connect_rate;     /* New connects per second. */
    double test_duration;    /* Seconds for the full test. */
    int   statsd_enable;
    char *statsd_server;
    int   statsd_port;
    char *statsd_namespace;
    int   listen_port;      /* Port on which to listen. */
    int   message_rate;     /* Messages per second per channel */
    char  *first_message_data;
    size_t first_message_size;
    char  *message_data;
    size_t message_size;
} default_config = {
        .max_connections = 1,
        .connect_rate = 100.0,
        .test_duration = 10.0,
        .statsd_enable = 0,
        .statsd_server = "127.0.0.1",
        .statsd_port = 8125,
        .statsd_namespace = "tcpkali",
        .listen_port = 0,
        .message_rate = 0
    };

struct stats_checkpoint {
    double epoch_start;   /* Start of current checkpoint epoch */
    size_t initial_data_sent;
    size_t initial_data_received;
};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage(char *argv0, struct tcpkali_config *);
struct multiplier { char *prefix; double mult; };
static double parse_with_multipliers(char *str, struct multiplier *, int n);
static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, struct stats_checkpoint *, Statsd *statsd, int *term_flag, int print_stats);
static int read_in_file(const char *filename, char **data, size_t *size);
struct addresses enumerate_usable_addresses(int listen_port);
static void print_connections_line(int conns, int max_conns, int conns_counter);

static struct multiplier k_multiplier[] = {
    { "k", 1000 }
};
static struct multiplier s_multiplier[] = {
    { "ms", 0.001 },
    { "s", 1 },
    { "second", 1 },
    { "seconds", 1 },
    { "m", 60 },
    { "min", 60 },
    { "minute", 60 },
    { "minutes", 60 }
};
static struct multiplier bw_multiplier[] = {
    { "bps", 1.0/8 },
    { "kbps", 1000/8 },
    { "Mbps", 1000000/8 },
    { "Bps", 1 },
    { "kBps", 1000 },
    { "MBps", 1000000 }
};

/*
 * Parse command line options and kick off the engine.
 */
int main(int argc, char **argv) {
    struct tcpkali_config conf = default_config;
    struct engine_params engine_params;
    memset(&engine_params, 0, sizeof(engine_params));

    while(1) {
        int c;
        c = getopt_long(argc, argv, "hc:m:f:l:w:", cli_long_options, NULL);
        if(c == -1)
            break;
        switch(c) {
        case 'h':
            usage(argv[0], &default_config);
            exit(EX_USAGE);
        case 'c':
            conf.max_connections = atoi(optarg);
            if(conf.max_connections < 0) {
                fprintf(stderr, "Expecting --connections > 0\n");
                exit(EX_USAGE);
            }
            break;
        case 'r':
            conf.connect_rate = parse_with_multipliers(optarg,
                            k_multiplier,
                            sizeof(k_multiplier)/sizeof(k_multiplier[0]));
            if(conf.connect_rate <= 0) {
                fprintf(stderr, "Expected positive --connect-rate=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 't':
            conf.test_duration = parse_with_multipliers(optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(conf.test_duration <= 0) {
                fprintf(stderr, "Expected positive --time=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 'f':
            if(conf.message_data) {
                fprintf(stderr, "--message-file: Message is already specified.\n");
                exit(EX_USAGE);
            } else if(read_in_file(optarg, &conf.message_data,
                                           &conf.message_size) != 0) {
                exit(EX_DATAERR);
            }
            break;
        case 'm': {
            if(conf.message_data) {
                fprintf(stderr, "-m: Message is already specified.\n");
                exit(EX_USAGE);
            }
            conf.message_data = strdup(optarg);
            conf.message_size = strlen(optarg);
            break;
            }
        case '1': {
            if(conf.first_message_data) {
                fprintf(stderr, "-m: Message is already specified.\n");
                exit(EX_USAGE);
            }
            conf.first_message_data = strdup(optarg);
            conf.first_message_size = strlen(optarg);
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
            if(Bps <= 0) {
                fprintf(stderr, "Expecting --channel-bandwidth > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_bandwidth_Bps = Bps;
            break;
            }
        case 'M': {
            int rate = parse_with_multipliers(optarg,
                        k_multiplier,
                        sizeof(k_multiplier)/sizeof(k_multiplier[0]));
            if(rate <= 0) {
                fprintf(stderr, "Expecting --message-rate > 0\n");
                exit(EX_USAGE);
            }
            conf.message_rate = rate;
            break;
            }
        case CLI_STATSD_OFFSET + 'e':
            conf.statsd_enable = 1;
            break;
        case CLI_STATSD_OFFSET+'s':
            conf.statsd_server = strdup(optarg);
            break;
        case CLI_STATSD_OFFSET+'n':
            conf.statsd_namespace = strdup(optarg);
            break;
        case CLI_STATSD_OFFSET+'p':
            conf.statsd_port = atoi(optarg);
            if(conf.statsd_port <= 0 || conf.statsd_port >= 65535) {
                fprintf(stderr, "Port value --statsd-port is out of range\n");
                exit(EX_USAGE);
            }
            break;
        case 'l':
            conf.listen_port = atoi(optarg);
            if(conf.listen_port <= 0 || conf.listen_port >= 65535) {
                fprintf(stderr, "Port value --listen_port is out of range\n");
                exit(EX_USAGE);
            }
            break;
        case CLI_CONN_OFFSET + 't':
            engine_params.connect_timeout = parse_with_multipliers(optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(engine_params.connect_timeout <= 0.0) {
                fprintf(stderr, "Expected positive --connect-timeout=%s\n",
                    optarg);
                exit(EX_USAGE);
            }
            break;
        case CLI_CHAN_OFFSET + 't':
            engine_params.channel_lifetime = parse_with_multipliers(optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(engine_params.channel_lifetime <= 0.0) {
                fprintf(stderr, "Expected positive --channel-lifetime=%s\n",
                    optarg);
                exit(EX_USAGE);
            }
            break;
        default:
            usage(argv[0], &default_config);
            exit(EX_USAGE);
        }
    }

    /*
     * Avoid spawning more threads than connections.
     */
    if(engine_params.requested_workers == 0
        && conf.max_connections < number_of_cpus()
        && conf.listen_port == 0) {
        engine_params.requested_workers = conf.max_connections;
    }
    if(!engine_params.requested_workers)
        engine_params.requested_workers = number_of_cpus();

    /*
     * Check that the system environment is prepared to handle high load.
     */
    if(adjust_system_limits_for_highload(conf.max_connections,
            engine_params.requested_workers) == -1) {
        fprintf(stderr, "System limits will not support the expected load.\n");
        exit(EX_SOFTWARE);
    }
    /* Check system limits and print out if they are not sane. */
    check_system_limits_sanity(conf.max_connections,
                               engine_params.requested_workers);

    /* Default connection timeout is 1 seconds */
    if(engine_params.connect_timeout == 0.0)
        engine_params.connect_timeout = 1.0;

    if(conf.message_size || conf.first_message_size) {
        char *p;
        size_t s;
        s = conf.message_size + conf.first_message_size;
        p = malloc(s + 1);
        assert(p);
        memcpy(p + 0, conf.first_message_data, conf.first_message_size);
        memcpy(p + conf.first_message_size,
                    conf.message_data, conf.message_size);
        p[s] = '\0';
        engine_params.data = p;
        engine_params.data_header_size = conf.first_message_size;
        engine_params.data_size = s;
    }

    /*
     * Make sure we're consistent with the message rate and channel bandwidth.
     */
    if(conf.message_rate) {
        size_t msize = conf.message_size;
        if(msize == 0) {
            fprintf(stderr, "--message-rate parameter has no sense "
                            "without --message or --message-file\n");
            exit(EX_USAGE);
        }
        size_t message_bandwidth = (msize * conf.message_rate) / 1;
        if(engine_params.channel_bandwidth_Bps &&
           engine_params.channel_bandwidth_Bps != message_bandwidth) {
            fprintf(stderr, "--channel-bandwidth=%ld is %s than --message-rate=%ld * %ld (message size)\n",
                (long)engine_params.channel_bandwidth_Bps,
                (engine_params.channel_bandwidth_Bps > message_bandwidth)
                    ? "more" : "less",
                (long)conf.message_rate,
                (long)msize);
            exit(EX_USAGE);
        }
        engine_params.channel_bandwidth_Bps = message_bandwidth;
        /* Write in msize blocks unless they're large, then use default. */
        engine_params.minimal_write_size = msize < 1460 ? msize : 0;
    }

    if(optind == argc && conf.listen_port == 0) {
        fprintf(stderr, "Expecting target <host:port> or --listen-port. See -h or --help.\n");
        usage(argv[0], &default_config);
        exit(EX_USAGE);
    }

    /*
     * Check if the number of connections can be opened in time.
     */
    if(conf.max_connections/conf.connect_rate > conf.test_duration / 10) {
        if(conf.max_connections/conf.connect_rate > conf.test_duration) {
            fprintf(stderr, "%d connections can not be opened at a rate %.1f,"
                            " within test duration %.f.\n"
                            "Decrease --connect-rate or increse --duration.\n",
                conf.max_connections,
                conf.connect_rate,
                conf.test_duration);
            exit(EX_USAGE);
        } else {
            fprintf(stderr, "WARNING: %d connections might not be opened at a rate %.1f,"
                            " within test duration %.f.\n"
                            "Decrease --connect-rate or increse --duration.\n",
                conf.max_connections,
                conf.connect_rate,
                conf.test_duration);
        }
    }

    /*
     * Pick multiple destinations from the command line, resolve them.
     */
    if(argc - optind > 0) {
        engine_params.remote_addresses
            = resolve_remote_addresses(&argv[optind], argc - optind);
        if(engine_params.remote_addresses.n_addrs == 0) {
            errx(EX_NOHOST, "DNS did not return usable addresses for given host(s)");
        } else {
            fprint_addresses(stderr, "Destination: ",
                                   "\nDestination: ", "\n",
                                   engine_params.remote_addresses);
        }
    } else {
        conf.max_connections = 0;
    }
    if(conf.listen_port > 0)
        engine_params.listen_addresses =
            enumerate_usable_addresses(conf.listen_port);

    Statsd *statsd;
    if(conf.statsd_enable) {
        statsd_new(&statsd, conf.statsd_server,
                            conf.statsd_port,
                            conf.statsd_namespace, NULL);
    } else {
        statsd = 0;
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

    int print_stats = isatty(1);
    if(print_stats) {
        setvbuf(stdout, 0, _IONBF, 0);
    }
    char *clear_line = print_stats ? "\033[K" : "";

    /*
     * Ramp up to the specified number of connections by opening them at a
     * specifed --connect-rate.
     */
    if(conf.max_connections) {
        double epoch_end = ev_now(EV_DEFAULT) + conf.test_duration;
        if(open_connections_until_maxed_out(eng, conf.connect_rate,
                                            conf.max_connections, epoch_end,
                                            NULL, statsd, &term_flag,
                                            print_stats) == 0) {
            printf("%s", clear_line);
            printf("Ramped up to %d connections.\n", conf.max_connections);
        } else {
            fprintf(stderr, "Could not create %d connection%s"
                            " in allotted time (%.1fs)\n",
                            conf.max_connections,
                            conf.max_connections==1?"":"s",
                            conf.test_duration);
            exit(1);
        }
    }

    struct stats_checkpoint checkpoint = {
        .epoch_start = ev_now(EV_DEFAULT),
    };
    engine_traffic(eng, &checkpoint.initial_data_sent,
                        &checkpoint.initial_data_received);

    /* Reset the test duration after ramp-up. */
    for(double epoch_end = ev_now(EV_DEFAULT) + conf.test_duration;;) {
        if(open_connections_until_maxed_out(eng, conf.connect_rate,
                                            conf.max_connections, epoch_end,
                                            &checkpoint,
                                            statsd, &term_flag,
                                            print_stats) == -1)
            break;
    }

    printf("%s", clear_line);
    engine_terminate(eng);

    return 0;
}

static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, struct stats_checkpoint *checkpoint, Statsd *statsd, int *term_flag, int print_stats) {
    static double last_stats_sent;

    ev_now_update(EV_DEFAULT);
    double now = ev_now(EV_DEFAULT);

    /*
     * It is a little bit better to batch the starts by issuing several
     * start commands per small time tick. Ends up doing less write()
     * operations per batch.
     * Therefore, we round the timeout_us upwards to the nearest millisecond.
     */
    long timeout_us = 1000 * ceil(1000.0/connect_rate);
    if(timeout_us > 250000)
        timeout_us = 250000;

    struct pacefier keepup_pace;
    pacefier_init(&keepup_pace, now);

    while(now < epoch_end && !*term_flag) {
        usleep(timeout_us);
        ev_now_update(EV_DEFAULT);
        now = ev_now(EV_DEFAULT);
        size_t conns_in, conns_out, conns_counter;
        engine_connections(eng, &conns_in, &conns_out, &conns_counter);
        ssize_t conn_deficit = max_connections - conns_out;

        size_t allowed = pacefier_allow(&keepup_pace, connect_rate, now);
        size_t to_start = allowed;
        if(conn_deficit <= 0) {
            to_start = 0;
        } if(to_start > (size_t)conn_deficit) {
            to_start = conn_deficit;
        }
        engine_initiate_new_connections(eng, to_start);
        pacefier_emitted(&keepup_pace, connect_rate, allowed, now);

        if(now - last_stats_sent > 0.25) {
            size_t sent, rcvd;
            if(statsd) {
                statsd_gauge(statsd, "connections.total.in", conns_in, 1);
                statsd_gauge(statsd, "connections.total.out", conns_out, 1);
                statsd_count(statsd, "connections.opened", to_start, 1);
            }
            last_stats_sent = now;

            if(checkpoint) {
                engine_traffic(eng, &sent, &rcvd);
                double bps_in = (8 * rcvd) / (now - checkpoint->epoch_start);
                double bps_out = (8 * sent) / (now - checkpoint->epoch_start);
                if(statsd) {
                    statsd_gauge(statsd, "traffic.bitrate.total",
                                            bps_in+bps_out, 1);
                    statsd_gauge(statsd, "traffic.bitrate.in", bps_in, 1);
                    statsd_gauge(statsd, "traffic.bitrate.out", bps_out, 1);
                    statsd_count(statsd, "traffic.data.sent",
                                    sent - checkpoint->initial_data_sent, 1);
                    statsd_count(statsd, "traffic.data.rcvd",
                                    rcvd - checkpoint->initial_data_received,1);
                }
                if(print_stats) {
                    printf("  Traffic %.3f↓, %.3f↑ Mbps (conns in %ld; out %ld/%d; seen %ld)     \r",
                        bps_in / 1000000,
                        bps_out / 1000000,
                        (long)conns_in, (long)conns_out, max_connections,
                        (long)conns_counter);
                }
            } else if(print_stats) {
                print_connections_line(conns_out, max_connections, conns_counter);
            }
        }

        if(conn_deficit <= 0) {
            return 0;
        }
    }

    return -1;
}

static void
print_connections_line(int conns, int max_conns, int conns_counter) {
    char buf[80];
    buf[0] = '|';
    const int ribbon_width = 50;
    int at = 1 + ((ribbon_width - 2) * conns) / max_conns;
    for(int i = 1; i < ribbon_width; i++) {
        if (i  < at)     buf[i] = '=';
        else if(i  > at) buf[i] = '-';
        else if(i == at) buf[i] = '>';
    }
    snprintf(buf+ribbon_width, sizeof(buf)-ribbon_width,
        "| %d of %d (%d)", conns, max_conns, conns_counter);
    printf("%s  \r", buf);
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
            break;
        }
    }
    if(*endptr) {
        fprintf(stderr, "Unknown prefix \"%s\" in %s\n", endptr, str);
        return -1;
    }
    return value;
}

static int
read_in_file(const char *filename, char **data, size_t *size) {
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
    data[off] = '\0';    /* Just in case. */
    *size = off;

    fclose(fp);

    return 0;
}

struct addresses enumerate_usable_addresses(int listen_port) {
    struct addresses addresses = { 0, 0 };
    struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
            .ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG };
    char service[32];
    snprintf(service, sizeof(service), "%d", listen_port);

    struct addrinfo *res;
    int err = getaddrinfo(NULL, service, &hints, &res);
    if(err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    /* Move all of the addresses into the separate storage */
    for(struct addrinfo *tmp = res; tmp; tmp = tmp->ai_next) {
        address_add(&addresses, tmp->ai_addr);
    }

    freeaddrinfo(res);

    fprint_addresses(stderr, "Listen on: ",
                               "\nListen on: ", "\n",
                               addresses);

    return addresses;
}

/*
 * Display the Usage screen.
 */
static void
usage(char *argv0, struct tcpkali_config *conf) {
    fprintf(stderr, "Usage: %s [OPTIONS] <host:port> [<host:port>...]\n",
        basename(argv0));
    fprintf(stderr,
    "Where OPTIONS are:\n"
    "  -h, --help                  Display this help screen\n"
    "  -c, --connections <N=%d>     Connections to keep open to the destinations\n"
    "  --connect-rate <R=%.0f>      Limit number of new connections per second\n"
    "  --connect-timeout <T=1s>    Limit time spent in a connection attempt\n"
    "  --channel-lifetime <T>      Shut down each connection after T seconds\n"
    "  --channel-bandwidth <Bw>    Limit single connection bandwidth\n"
    "  --message-first <string>    Send this message first, once\n"
    "  -m, --message <string>      Message to repeatedly send to the remote\n"
    "  -f, --message-file <name>   Read message to send from a file\n"
    "  --message-rate <R>          Messages per second per connection to send\n"
    "  -l, --listen-port <port>    Listen on the specified port\n"
    "  -w, --workers <N=%ld>%s        Number of parallel threads to use\n"
    "  --duration <T=10s>          Load test for the specified amount of time\n"
    "  --statsd                    Enable StatsD output (default %s)\n"
    "  --statsd-host <host>        StatsD host to send data (default is localhost)\n"
    "  --statsd-port <port>        StatsD port to use (default is %d)\n"
    "  --statsd-namespace <string> Metric namespace (default is \"%s\")\n"
    //"  --total-bandwidth <Bw>    Limit total bandwidth (see multipliers below)\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, Mbps (bits per second), kBps, MBps (bytes per second)\n"
    "  <T>:  ms, s, m, h (milliseconds, seconds, minutes, hours)\n",
    conf->max_connections,
    conf->connect_rate,
    number_of_cpus(), number_of_cpus() < 10 ? " " : "",
    conf->statsd_enable ? "enabled" : "disabled",
    conf->statsd_port,
    conf->statsd_namespace
    );
}
