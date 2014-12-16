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
#define _ISOC99_SOURCE
#define _BSD_SOURCE
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

#include <statsd.h>

#include "tcpkali.h"
#include "tcpkali_mavg.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_events.h"
#include "tcpkali_signals.h"
#include "tcpkali_websocket.h"
#include "tcpkali_transport.h"
#include "tcpkali_syslimits.h"

#define ANSI_CLEAR_LINE "\033[K"

/*
 * Describe the command line options.
 */
#define CLI_STATSD_OFFSET   256
#define CLI_CHAN_OFFSET  512
#define CLI_CONN_OFFSET  1024
static struct option cli_long_options[] = {
    { "help", 0, 0, 'h' },
    { "version", 0, 0, 'V' },
    { "verbose", 1, 0, 'v' },
    { "connections", 1, 0, 'c' },
    { "connect-rate", 1, 0, 'r' },
    { "duration", 1, 0, 'T' },
    { "message", 1, 0, 'm' },
    { "message-file", 1, 0, 'f' },
    { "message-rate", 1, 0, 'R' },
    { "first-message", 1, 0, '1' },
    { "first-message-file", 1, 0, 'F' },
    { "workers", 1, 0, 'w' },
    { "channel-bandwidth", 1, 0, 'b' },
    { "statsd", 0, 0,           CLI_STATSD_OFFSET + 'e' },
    { "statsd-server", 1, 0,    CLI_STATSD_OFFSET + 's' },
    { "statsd-port", 1, 0,      CLI_STATSD_OFFSET + 'p' },
    { "statsd-namespace", 1, 0, CLI_STATSD_OFFSET + 'n' },
    { "connect-timeout", 1, 0,  CLI_CONN_OFFSET + 't' },
    { "channel-lifetime", 1, 0, CLI_CHAN_OFFSET + 't' },
    { "listen-port", 1, 0, 'l' },
    { "ws", 0, 0, 'W' }, { "websocket", 0, 0, 'W' },
    { 0, 0, 0, 0 }
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
    double message_rate;    /* Messages per second in channel */
    char  *first_message_data;
    size_t first_message_size;
    char  *message_data;
    size_t message_size;
    int    websocket_enable;    /* Enable WebSocket framing. */
    char  *first_hostport;      /* A single (first) host:port specification */
    char  *first_path;          /* A /path specification from the first host */
} default_config = {
        .max_connections = 1,
        .connect_rate = 100.0,
        .test_duration = 10.0,
        .statsd_enable = 0,
        .statsd_server = "127.0.0.1",
        .statsd_port = 8125,
        .statsd_namespace = "tcpkali"
    };

struct stats_checkpoint {
    double epoch_start;   /* Start of current checkpoint epoch */
    double last_update;   /* Last we updated the checkpoint structure */
    size_t initial_data_sent;
    size_t initial_data_received;
    size_t last_data_sent;
    size_t last_data_received;
};

enum work_phase {
    PHASE_ESTABLISHING_CONNECTIONS,
    PHASE_STEADY_STATE
};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage(char *argv0, struct tcpkali_config *);
struct multiplier { char *prefix; double mult; };
static double parse_with_multipliers(const char *, char *str, struct multiplier *, int n);
static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, struct stats_checkpoint *, mavg traffic_mavgs[2], Statsd *statsd, int *term_flag, enum work_phase phase, int print_stats);
static int read_in_file(const char *filename, char **data, size_t *size);
static int append_data(const char *str, size_t str_size, char **data, size_t *data_size);
struct addresses detect_listen_addresses(int listen_port);
static void print_connections_line(int conns, int max_conns, int conns_counter);
static void report_to_statsd(Statsd *statsd, size_t opened, size_t conns_in, size_t conns_out, size_t bps_in, size_t bps_out, size_t rcvd, size_t sent);
static void unescape(char *data, size_t *initial_data_size);

static struct multiplier k_multiplier[] = {
    { "k", 1000 }
};
static struct multiplier s_multiplier[] = {
    { "ms", 0.001 }, { "millisecond", 0.001 }, { "milliseconds", 0.001 },
    { "s", 1 }, { "second", 1 }, { "seconds", 1 },
    { "m", 60 }, { "min", 60 }, { "minute", 60 }, { "minutes", 60 },
    { "h", 3600 }, { "hr", 3600 }, { "hour", 3600 }, { "hours", 3600 },
    { "d", 86400 }, { "day", 86400 }, { "days", 86400 }
};
static struct multiplier bw_multiplier[] = {
    /* bits per second */
    { "bps", 1.0/8 },
    { "kbps", 1000/8 },
    { "Kbps", 1000/8 },
    { "mbps", 1000000/8 }, { "Mbps", 1000000/8 },
    { "gbps", 1000000000/8 }, { "Gbps", 1000000000/8 },
    /* Bytes per second */
    { "Bps", 1 },
    { "kBps", 1000 }, { "KBps", 1000 },
    { "mBps", 1000000 }, { "MBps", 1000000 },
    { "gBps", 1000000000 }, { "GBps", 1000000000 }
};

/*
 * Parse command line options and kick off the engine.
 */
int main(int argc, char **argv) {
    struct tcpkali_config conf = default_config;
    struct engine_params engine_params = {
        .verbosity_level    = DBG_ERROR,
        .connect_timeout  = 1.0,
        .channel_lifetime = INFINITY
    };
    int unescape_message_data = 0;

    while(1) {
        char *option = argv[optind];
        int c;
        c = getopt_long(argc, argv, "hc:em:f:l:r:w:T:", cli_long_options, NULL);
        if(c == -1)
            break;
        switch(c) {
        case 'V':
            printf(PACKAGE_NAME " version " VERSION
#ifdef  USE_LIBUV
            " (libuv)"
#else
            " (libev)"
#endif
            "\n");
            exit(0);
        case 'h':
            usage(argv[0], &default_config);
            exit(EX_USAGE);
        case 'v':
            engine_params.verbosity_level = atoi(optarg);
            if((int)engine_params.verbosity_level < 0
               || engine_params.verbosity_level >= _DBG_MAX) {
                fprintf(stderr, "Expecting --verbose=[0..%d]\n", _DBG_MAX-1);
                exit(EX_USAGE);
            }
            break;
        case 'c':
            conf.max_connections = atoi(optarg);
            if(conf.max_connections < 0) {
                fprintf(stderr, "Expecting --connections > 0\n");
                exit(EX_USAGE);
            }
            break;
        case 'r':
            conf.connect_rate = parse_with_multipliers(option, optarg,
                            k_multiplier,
                            sizeof(k_multiplier)/sizeof(k_multiplier[0]));
            if(conf.connect_rate <= 0) {
                fprintf(stderr, "Expected positive --connect-rate=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 'T':
            conf.test_duration = parse_with_multipliers(option, optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(conf.test_duration <= 0) {
                fprintf(stderr, "Expected positive --duration=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 'e':
            unescape_message_data = 1;
            break;
        case 'm':   /* --message */
            if(conf.message_data) {
                fprintf(stderr, "--message: Message is already specified.\n");
                exit(EX_USAGE);
            }
            conf.message_data = strdup(optarg);
            conf.message_size = strlen(optarg);
            if(unescape_message_data)
                unescape(conf.message_data, &conf.message_size);
            break;
        case '1':   /* --first-message */
            if(conf.first_message_data) {
                fprintf(stderr, "WARNING: --first-message is already specified;"
                                " appending.\n");
                /* FALL THROUGH */
            }
            char *str = strdup(optarg);
            size_t slen = strlen(optarg);
            if(unescape_message_data)
                unescape(str, &slen);
            if(append_data(str, slen,
                    &conf.first_message_data, &conf.first_message_size) != 0) {
                exit(EX_USAGE);
            }
            free(str);
            break;
        case 'f':   /* --message-file */
            if(conf.message_data) {
                fprintf(stderr, "--message-file: Message is already specified.\n");
                exit(EX_USAGE);
            } else if(read_in_file(optarg, &conf.message_data,
                                           &conf.message_size) != 0) {
                exit(EX_DATAERR);
            }
            if(unescape_message_data)
                unescape(conf.message_data, &conf.message_size);
            break;
        case 'F':
            if(conf.first_message_data) {
                fprintf(stderr, "--first-message-file: Message is already specified.\n");
                exit(EX_USAGE);
            } else if(read_in_file(optarg, &conf.first_message_data,
                                           &conf.first_message_size) != 0) {
                exit(EX_DATAERR);
            }
            if(unescape_message_data)
                unescape(conf.message_data, &conf.message_size);
            break;
        case 'w': {
            int n = atoi(optarg);
            if(n <= 0) {
                fprintf(stderr, "Expected --workers > 1\n");
                exit(EX_USAGE);
            }
            if(n > number_of_cpus()) {
                fprintf(stderr, "Value --workers=%d is unreasonably large,"
                    " only %ld CPU%s detected\n",
                    n, number_of_cpus(), number_of_cpus()==1?"":"s");
                exit(EX_USAGE);
            }
            engine_params.requested_workers = n;
            break;
            }
        case 'b': {
            int Bps = parse_with_multipliers(option, optarg,
                        bw_multiplier,
                        sizeof(bw_multiplier)/sizeof(bw_multiplier[0]));
            if(Bps <= 0) {
                fprintf(stderr, "Expecting --channel-bandwidth > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_bandwidth_Bps = Bps;
            break;
            }
        case 'R': {
            double rate = parse_with_multipliers(option, optarg,
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
                fprintf(stderr, "--statsd-port=%d is not in [1..65535]\n",
                    conf.statsd_port);
                exit(EX_USAGE);
            }
            break;
        case 'l':
            conf.listen_port = atoi(optarg);
            if(conf.listen_port <= 0 || conf.listen_port >= 65535) {
                fprintf(stderr, "--listen-port=%d is not in [1..65535]\n",
                    conf.listen_port);
                exit(EX_USAGE);
            }
            break;
        case CLI_CONN_OFFSET + 't':
            engine_params.connect_timeout = parse_with_multipliers(
                            option, optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(engine_params.connect_timeout <= 0.0) {
                fprintf(stderr, "Expected positive --connect-timeout=%s\n",
                    optarg);
                exit(EX_USAGE);
            }
            break;
        case CLI_CHAN_OFFSET + 't':
            engine_params.channel_lifetime = parse_with_multipliers(
                            option, optarg,
                            s_multiplier,
                            sizeof(s_multiplier)/sizeof(s_multiplier[0]));
            if(engine_params.channel_lifetime < 0.0) {
                fprintf(stderr, "Expected non-negative --channel-lifetime=%s\n",
                    optarg);
                exit(EX_USAGE);
            }
            break;
        case 'W':   /* --websocket: Enable WebSocket framing */
            conf.websocket_enable = 1;
            engine_params.websocket_enable = 1;
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
        /* Print the full set of problems with system limits. */
        check_system_limits_sanity(conf.max_connections,
                                   engine_params.requested_workers);
        fprintf(stderr, "System limits will not support the expected load.\n");
        exit(EX_SOFTWARE);
    } else {
        /* Check other system limits and print out if they might be too low. */
        check_system_limits_sanity(conf.max_connections,
                                   engine_params.requested_workers);
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
        /* Figure out the host and port for HTTP "Host:" header. */
        conf.first_hostport = strdup(argv[optind]);
        conf.first_path = strchr(conf.first_hostport, '/');
        if(conf.first_path) {
            *conf.first_path++ = '\0';
        } else {
            conf.first_path = ""; /* "GET / HTTP/1.1" */
        }
    } else {
        conf.max_connections = 0;
    }
    if(conf.listen_port > 0) {
        engine_params.listen_addresses =
            detect_listen_addresses(conf.listen_port);
    }

    /*
     * Prepare a buffer with data to [repeatedly] send to the other system.
     */
    size_t message_size_with_framing = conf.message_size; /* TCP||WS framing */
    if(conf.message_size || conf.first_message_size || conf.websocket_enable) {
        struct iovec iovs[2] = {
            { .iov_base = conf.first_message_data,
              .iov_len = conf.first_message_size },
            { .iov_base = conf.message_data,
              .iov_len = conf.message_size }
        };
        engine_params.data = add_transport_framing(iovs,
                                1,  /* First message data */
                                2,  /* Subsequent messages data */
                                conf.websocket_enable,
                                conf.first_hostport, conf.first_path);
        message_size_with_framing += conf.websocket_enable
            ? websocket_frame_header(conf.message_size, 0, 0) : 0;
    }

    /*
     * Make sure we're consistent with the message rate and channel bandwidth.
     */
    if(conf.message_rate) {
        size_t msize = message_size_with_framing;
        if(msize == 0) {
            fprintf(stderr, "--message-rate parameter makes no sense "
                            "without --message or --message-file\n");
            exit(EX_USAGE);
        }
        size_t message_bandwidth = (msize * conf.message_rate) / 1;
        if(message_bandwidth == 0) {
            fprintf(stderr, "--message-rate=%g with message size %ld results in a less than a byte per second bandwidth, refusing to proceed\n",
                conf.message_rate, (long)msize);
            exit(EX_USAGE);
        }
        if(engine_params.channel_bandwidth_Bps &&
           engine_params.channel_bandwidth_Bps != message_bandwidth) {
            fprintf(stderr, "--channel-bandwidth=%ld is %s than --message-rate=%g * %ld (message size)\n",
                (long)engine_params.channel_bandwidth_Bps,
                (engine_params.channel_bandwidth_Bps > message_bandwidth)
                    ? "more" : "less",
                conf.message_rate,
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
            fprintf(stderr, "%d connections can not be opened "
                    "at a rate %g within test duration %g.\n"
                    "Decrease --connections=%d, or increase --duration=%g or --connect-rate=%g.\n",
                conf.max_connections,
                conf.connect_rate,
                conf.test_duration, conf.max_connections, conf.test_duration,
                conf.connect_rate);
            exit(EX_USAGE);
        } else {
            fprintf(stderr, "WARNING: %d connections might not be opened "
                    "at a rate %g within test duration %g.\n"
                    "Decrease --connections=%d, or increase --duration=%g or --connect-rate=%g.\n",
                conf.max_connections,
                conf.connect_rate,
                conf.test_duration, conf.max_connections, conf.test_duration,
                conf.connect_rate);
        }
    }

    /*
     * Initialize statsd library and push initial (empty) metrics.
     */
    Statsd *statsd;
    if(conf.statsd_enable) {
        statsd_new(&statsd, conf.statsd_server,
                            conf.statsd_port,
                            conf.statsd_namespace, NULL);
        /* Clear up traffic numbers, for better graphing. */
        report_to_statsd(statsd, 0, 0, 0, 0, 0, 0, 0);
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
    char *clear_line = print_stats ? ANSI_CLEAR_LINE : "";

    /*
     * Traffic in/out moving average, smoothing period is 3 seconds.
     */
    mavg traffic_mavgs[2];
    mavg_init(&traffic_mavgs[0], tk_now(TK_DEFAULT), 3.0);
    mavg_init(&traffic_mavgs[1], tk_now(TK_DEFAULT), 3.0);
    struct stats_checkpoint checkpoint = { 0, 0, 0, 0, 0, 0 };

    /*
     * Ramp up to the specified number of connections by opening them at a
     * specifed --connect-rate.
     */
    if(conf.max_connections) {
        double epoch_end = tk_now(TK_DEFAULT) + conf.test_duration;
        if(open_connections_until_maxed_out(eng, conf.connect_rate,
                                            conf.max_connections, epoch_end,
                                            &checkpoint, traffic_mavgs, statsd,
                                            &term_flag,
                                            PHASE_ESTABLISHING_CONNECTIONS,
                                            print_stats) == 0) {
            fprintf(stderr, "%s", clear_line);
            fprintf(stderr, "Ramped up to %d connections.\n",
                conf.max_connections);
        } else {
            fprintf(stderr, "%s", clear_line);
            fprintf(stderr, "Could not create %d connection%s"
                            " in allotted time (%gs)\n",
                            conf.max_connections,
                            conf.max_connections==1?"":"s",
                            conf.test_duration);
            /* Level down graphs/charts. */
            report_to_statsd(statsd, 0, 0, 0, 0, 0, 0, 0);
            exit(1);
        }
    }

    /*
     * Start measuring the steady-state performance, as opposed to
     * ramping up and waiting for the connections to be established.
     */
    engine_traffic(eng, &checkpoint.initial_data_sent,
                        &checkpoint.initial_data_received);
    checkpoint.epoch_start = tk_now(TK_DEFAULT);

    /* Reset the test duration after ramp-up. */
    for(double epoch_end = tk_now(TK_DEFAULT) + conf.test_duration;;) {
        if(open_connections_until_maxed_out(eng, conf.connect_rate,
                                            conf.max_connections, epoch_end,
                                            &checkpoint, traffic_mavgs,
                                            statsd, &term_flag,
                                            PHASE_STEADY_STATE,
                                            print_stats) == -1)
            break;
    }

    fprintf(stderr, "%s", clear_line);
    engine_terminate(eng, checkpoint.epoch_start,
        checkpoint.initial_data_sent,
        checkpoint.initial_data_received);

    /* Send zeroes, otherwise graphs would continue showing non-zeroes... */
    report_to_statsd(statsd, 0, 0, 0, 0, 0, 0, 0);

    return 0;
}

static const char *time_progress(double start, double now, double stop) {
    const char *clocks[] = {
        "🕛", "🕐", "🕑", "🕒", "🕓", "🕔", "🕕", "🕖", "🕗", "🕘", "🕙", "🕚"
    };
    double span = (stop - start) / (sizeof(clocks)/sizeof(clocks[0]));
    int pos = (now - start) / span;
    if(pos < 0) pos = 0;
    else if(pos > 11) pos = 11;
    return clocks[pos];
}

static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, struct stats_checkpoint *checkpoint, mavg traffic_mavgs[2], Statsd *statsd, int *term_flag, enum work_phase phase, int print_stats) {
    tk_now_update(TK_DEFAULT);
    double now = tk_now(TK_DEFAULT);

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

    ssize_t conn_deficit = 1; /* Assume connections still have to be est. */

    while(now < epoch_end && !*term_flag
            /* ...until we have all connections established or
             * we're in a steady state. */
            && (phase == PHASE_STEADY_STATE || conn_deficit > 0)) {
        usleep(timeout_us);
        tk_now_update(TK_DEFAULT);
        now = tk_now(TK_DEFAULT);
        size_t connecting, conns_in, conns_out, conns_counter;
        engine_connections(eng, &connecting,
                                &conns_in, &conns_out, &conns_counter);
        conn_deficit = max_connections - (connecting + conns_out);

        size_t allowed = pacefier_allow(&keepup_pace, connect_rate, now);
        size_t to_start = allowed;
        if(conn_deficit <= 0) {
            to_start = 0;
        } if(to_start > (size_t)conn_deficit) {
            to_start = conn_deficit;
        }
        engine_initiate_new_connections(eng, to_start);
        pacefier_emitted(&keepup_pace, connect_rate, allowed, now);

        /* Do not update/print checkpoint stats too often. */
        if((now - checkpoint->last_update) < 0.25) {
            continue;
        } else {
            checkpoint->last_update = now;
            /* Fall through and do the chekpoint update. */
        }

        /*
         * sent & rcvd to contain bytes sent/rcvd within the last
         * period (now - checkpoint->last_stats_sent).
         */
        size_t sent = checkpoint->last_data_sent;
        size_t rcvd = checkpoint->last_data_received;
        engine_traffic(eng, &checkpoint->last_data_sent,
                            &checkpoint->last_data_received);
        sent = checkpoint->last_data_sent - sent;
        rcvd = checkpoint->last_data_received - rcvd;

        mavg_bump(&traffic_mavgs[0], now, (double)rcvd);
        mavg_bump(&traffic_mavgs[1], now, (double)sent);

        double bps_in = 8 * mavg_per_second(&traffic_mavgs[0], now);
        double bps_out = 8 * mavg_per_second(&traffic_mavgs[1], now);

        report_to_statsd(statsd,
            to_start, conns_in, conns_out, bps_in, bps_out, rcvd, sent);

        if(print_stats) {
            if(phase == PHASE_ESTABLISHING_CONNECTIONS) {
                print_connections_line(conns_out, max_connections,
                                       conns_counter);
            } else {
                fprintf(stderr,
                    "%s  Traffic %.3f↓, %.3f↑ Mbps "
                    "(conns %ld↓ %ld↑ %ld⇡; seen %ld)" ANSI_CLEAR_LINE "\r",
                    time_progress(checkpoint->epoch_start, now, epoch_end),
                    bps_in/1000000.0, bps_out/1000000.0,
                    (long)conns_in, (long)conns_out,
                    (long)connecting, (long)conns_counter
                );
            }
        }

    }

    return (now >= epoch_end || *term_flag) ? -1 : 0;
}


static void report_to_statsd(Statsd *statsd, size_t opened,
                size_t conns_in, size_t conns_out,
                size_t bps_in, size_t bps_out,
                size_t rcvd, size_t sent) {
    if(statsd) {
        statsd_count(statsd, "connections.opened", opened, 1);
        statsd_gauge(statsd, "connections.total", conns_in + conns_out, 1);
        statsd_gauge(statsd, "connections.total.in", conns_in, 1);
        statsd_gauge(statsd, "connections.total.out", conns_out, 1);
        statsd_gauge(statsd, "traffic.bitrate", bps_in + bps_out, 1);
        statsd_gauge(statsd, "traffic.bitrate.in", bps_in, 1);
        statsd_gauge(statsd, "traffic.bitrate.out", bps_out, 1);
        statsd_count(statsd, "traffic.data", rcvd + sent, 1);
        statsd_count(statsd, "traffic.data.rcvd", rcvd, 1);
        statsd_count(statsd, "traffic.data.sent", sent, 1);
    }
}

static void
print_connections_line(int conns, int max_conns, int conns_counter) {
    char buf[80];
    buf[0] = '|';
    const int ribbon_width = 50;
    int at = 1 + ((ribbon_width - 2) * conns) / max_conns;
    for(int i = 1; i < ribbon_width; i++) {
        if (i < at)      buf[i] = '=';
        else if(i  > at) buf[i] = '-';
        else if(i == at) buf[i] = '>';
    }
    snprintf(buf+ribbon_width, sizeof(buf)-ribbon_width,
        "| %d of %d (%d)", conns, max_conns, conns_counter);
    fprintf(stderr, "%s" ANSI_CLEAR_LINE "\r", buf);
}

static double
parse_with_multipliers(const char *option, char *str, struct multiplier *ms, int n) {
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
    if(!isfinite(value)) {
        fprintf(stderr, "Option %s parses to infinite value\n", option);
        return -1;
    }
    return value;
}

static int
append_data(const char *str, size_t str_size, char **data, size_t *dsize) {
    size_t old_size = (*dsize);
    size_t new_size = old_size + str_size;
    char *p = malloc(new_size + 1);
    assert(p);
    memcpy(p, *data, old_size);
    memcpy(&p[old_size], str, str_size);
    p[new_size] = '\0';
    *data = p;
    *dsize = new_size;
    return 0;
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
    (*data)[off] = '\0';    /* Just in case. */
    *size = off;

    fclose(fp);

    return 0;
}

struct addresses detect_listen_addresses(int listen_port) {
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

static void
unescape(char *data, size_t *initial_data_size) {
    char *r = data;
    char *w = data;
    size_t data_size = initial_data_size ? *initial_data_size : strlen(data);
    char *end = data + data_size;

    for(; r < end; r++, w++) {
        switch(*r) {
        default:
            *w = *r;
            break;
        case '\\':
            r++;
            switch(*r) {
            case 'n': *w = '\n'; break;
            case 'r': *w = '\r'; break;
            case 'f': *w = '\f'; break;
            case 'b': *w = '\b'; break;
            case 'x': {
                /* Do not parse more than 2 symbols (ff) */
                char digits[3];
                char *endptr = (r+3) < end ? (r+3) : end;
                memcpy(digits, r+1, endptr-r-1);    /* Ignore leading 'x' */
                digits[2] = '\0';
                char *digits_end = digits;
                unsigned long l = strtoul(digits, &digits_end, 16);
                if(digits_end == digits) {
                    *w++ = '\\';
                    *w = *r;
                } else {
                    r += (digits_end - digits);
                    *w = (l & 0xff);
                }
                }
                break;
            case '0': {
                char digits[5];
                char *endptr = (r+4) < end ? (r+4) : end;
                memcpy(digits, r, endptr-r);
                digits[4] = '\0';
                char *digits_end = digits;
                unsigned long l = strtoul(digits, &digits_end, 8);
                if(digits_end == digits) {
                    *w = '\0';
                } else {
                    r += (digits_end - digits) - 1;
                    *w = (l & 0xff);
                }
                }
                break;
            default:
                *w++ = '\\';
                *w = *r;
            }
        }
    }
    *w = '\0';

    if(initial_data_size)
        *initial_data_size = (w - data);
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
    "  -h, --help                  Print this help screen, then exit\n"
    "  --version                   Print version number, then exit\n"
    "  --verbose <level=1>         Verbosity level [0..%d]\n"
    "  -c, --connections <N=%d>     Connections to keep open to the destinations\n"
    "  -r, --connect-rate <R=%g>  Limit number of new connections per second\n"
    "  --connect-timeout <T=1s>    Limit time spent in a connection attempt\n"
    "  --channel-lifetime <T>      Shut down each connection after T seconds\n"
    "  --channel-bandwidth <Bw>    Limit single connection bandwidth\n"
    "  -e                          Unescape next {-m|-f|--first-*} arguments\n"
    "  --first-message <string>    Send this message first, once\n"
    "  --first-message-file <name> Read the first message from a file\n"
    "  -m, --message <string>      Message to repeatedly send to the remote\n"
    "  -f, --message-file <name>   Read message to send from a file\n"
    "  --message-rate <R>          Messages per second to send in a connection\n"
    "  -l, --listen-port <port>    Listen on the specified port\n"
    "  -w, --workers <N=%ld>%s        Number of parallel threads to use\n"
    "  -T, --duration <T=10s>      Load test for the specified amount of time\n"
    "  --statsd                    Enable StatsD output (default %s)\n"
    "  --statsd-host <host>        StatsD host to send data (default is localhost)\n"
    "  --statsd-port <port>        StatsD port to use (default is %d)\n"
    "  --statsd-namespace <string> Metric namespace (default is \"%s\")\n"
    "  --ws, --websocket           Use RFC6455 WebSocket transport\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, Mbps (bits per second), kBps, MBps (bytes per second)\n"
    "  <T>:  ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)\n",
    (_DBG_MAX - 1),
    conf->max_connections,
    conf->connect_rate,
    number_of_cpus(), number_of_cpus() < 10 ? " " : "",
    conf->statsd_enable ? "enabled" : "disabled",
    conf->statsd_port,
    conf->statsd_namespace
    );
}
