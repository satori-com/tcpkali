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

#include <ev.h>
#include <statsd.h>

#include "tcpkali.h"
#include "tcpkali_mavg.h"
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
    { "debug", 1, 0, 'd' },
    { "connections", 0, 0, 'c' },
    { "connect-rate", 1, 0, 'r' },
    { "duration", 1, 0, 'T' },
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
    { "listen-port", 1, 0, 'l' },
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
struct addresses detect_listen_addresses(int listen_port);
static void print_connections_line(int conns, int max_conns, int conns_counter);

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
        .debug_level = DBG_ERROR,
        .connect_timeout = 1.0,
        .channel_lifetime = INFINITY
    };

    while(1) {
        char *option = argv[optind];
        int c;
        c = getopt_long(argc, argv, "hc:m:f:l:r:w:T:", cli_long_options, NULL);
        if(c == -1)
            break;
        switch(c) {
        case 'h':
            usage(argv[0], &default_config);
            exit(EX_USAGE);
        case 'd':
            engine_params.debug_level = atoi(optarg);
            switch(engine_params.debug_level) {
            case 0: case 1: case 2: break;
            default:
                fprintf(stderr, "Expecting --debug=[0..2]\n");
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
                fprintf(stderr, "--message: Message is already specified.\n");
                exit(EX_USAGE);
            }
            conf.message_data = strdup(optarg);
            conf.message_size = strlen(optarg);
            break;
            }
        case '1': {
            if(conf.first_message_data) {
                fprintf(stderr, "--message: Message is already specified.\n");
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
        case 'M': {
            int rate = parse_with_multipliers(option, optarg,
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
            fprintf(stderr, "%d connections can not be opened "
                    "at a rate %g within test duration %g.\n"
                    "Decrease --connections=%d or increase --duration=%g.\n",
                conf.max_connections,
                conf.connect_rate,
                conf.test_duration, conf.max_connections, conf.test_duration);
            exit(EX_USAGE);
        } else {
            fprintf(stderr, "WARNING: %d connections might not be opened "
                    "at a rate %g within test duration %g.\n"
                    "Decrease --connections=%d or increase --duration=%g.\n",
                conf.max_connections,
                conf.connect_rate,
                conf.test_duration, conf.max_connections, conf.test_duration);
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
        engine_params.listen_addresses = detect_listen_addresses(conf.listen_port);

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
     * Traffic in/out moving average, smoothing period is 3 seconds.
     */
    mavg traffic_mavgs[2];
    mavg_init(&traffic_mavgs[0], ev_now(EV_DEFAULT), 3.0);
    mavg_init(&traffic_mavgs[1], ev_now(EV_DEFAULT), 3.0);
    struct stats_checkpoint checkpoint = { 0, 0, 0, 0, 0, 0 };

    /*
     * Ramp up to the specified number of connections by opening them at a
     * specifed --connect-rate.
     */
    if(conf.max_connections) {
        double epoch_end = ev_now(EV_DEFAULT) + conf.test_duration;
        if(open_connections_until_maxed_out(eng, conf.connect_rate,
                                            conf.max_connections, epoch_end,
                                            &checkpoint, traffic_mavgs, statsd,
                                            &term_flag,
                                            PHASE_ESTABLISHING_CONNECTIONS,
                                            print_stats) == 0) {
            printf("%s", clear_line);
            printf("Ramped up to %d connections.\n", conf.max_connections);
        } else {
            fprintf(stderr, "Could not create %d connection%s"
                            " in allotted time (%gs)\n",
                            conf.max_connections,
                            conf.max_connections==1?"":"s",
                            conf.test_duration);
            exit(1);
        }
    }

    /*
     * When did we start measuring the steady-state performance,
     * as opposed to waiting for the connections to be established.
     */
    checkpoint.epoch_start = ev_now(EV_DEFAULT),
    engine_traffic(eng, &checkpoint.initial_data_sent,
                        &checkpoint.initial_data_received);

    /* Reset the test duration after ramp-up. */
    for(double epoch_end = ev_now(EV_DEFAULT) + conf.test_duration;;) {
        if(open_connections_until_maxed_out(eng, conf.connect_rate,
                                            conf.max_connections, epoch_end,
                                            &checkpoint, traffic_mavgs,
                                            statsd, &term_flag,
                                            PHASE_STEADY_STATE,
                                            print_stats) == -1)
            break;
    }

    printf("%s", clear_line);
    engine_terminate(eng);

    return 0;
}

static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, struct stats_checkpoint *checkpoint, mavg traffic_mavgs[2], Statsd *statsd, int *term_flag, enum work_phase phase, int print_stats) {
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

    ssize_t conn_deficit = 1; /* Assume connections still have to be est. */

    while(now < epoch_end && !*term_flag
            /* ...until we have all connections established or
             * we're in a steady state. */
            && (phase == PHASE_STEADY_STATE || conn_deficit > 0)) {
        usleep(timeout_us);
        ev_now_update(EV_DEFAULT);
        now = ev_now(EV_DEFAULT);
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

        if(statsd) {
            statsd_count(statsd, "connections.opened", to_start, 1);
            statsd_gauge(statsd, "connections.total", conns_in + conns_out, 1);
            statsd_gauge(statsd, "connections.total.in", conns_in, 1);
            statsd_gauge(statsd, "connections.total.out", conns_out, 1);
            if(phase == PHASE_STEADY_STATE) {
                statsd_gauge(statsd, "traffic.bitrate", bps_in + bps_out, 1);
                statsd_gauge(statsd, "traffic.bitrate.in", bps_in, 1);
                statsd_gauge(statsd, "traffic.bitrate.out", bps_out, 1);
                statsd_count(statsd, "traffic.data", rcvd + sent, 1);
                statsd_count(statsd, "traffic.data.rcvd", rcvd, 1);
                statsd_count(statsd, "traffic.data.sent", sent, 1);
            }
        }

        if(print_stats) {
            if(phase == PHASE_ESTABLISHING_CONNECTIONS) {
                print_connections_line(conns_out, max_connections,
                                       conns_counter);
            } else {
                printf("  Traffic %.3f↓, %.3f↑ Mbps "
                        "(conns %ld↓ %ld↑ %ld⇡; seen %ld)     \r",
                    bps_in/1000000.0, bps_out/1000000.0,
                    (long)conns_in, (long)conns_out,
                    (long)connecting, (long)conns_counter
                );
            }
        }

    }

    return (now >= epoch_end || *term_flag) ? -1 : 0;
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
    printf("%s  \r", buf);
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
    "  --debug <level=1>           Debug level [0..2].\n"
    "  -c, --connections <N=%d>     Connections to keep open to the destinations\n"
    "  -r, --connect-rate <R=%g>  Limit number of new connections per second\n"
    "  --connect-timeout <T=1s>    Limit time spent in a connection attempt\n"
    "  --channel-lifetime <T>      Shut down each connection after T seconds\n"
    "  --channel-bandwidth <Bw>    Limit single connection bandwidth\n"
    "  --message-first <string>    Send this message first, once\n"
    "  -m, --message <string>      Message to repeatedly send to the remote\n"
    "  -f, --message-file <name>   Read message to send from a file\n"
    "  --message-rate <R>          Messages per second per connection to send\n"
    "  -l, --listen-port <port>    Listen on the specified port\n"
    "  -w, --workers <N=%ld>%s        Number of parallel threads to use\n"
    "  -T, --duration <T=10s>      Load test for the specified amount of time\n"
    "  --statsd                    Enable StatsD output (default %s)\n"
    "  --statsd-host <host>        StatsD host to send data (default is localhost)\n"
    "  --statsd-port <port>        StatsD port to use (default is %d)\n"
    "  --statsd-namespace <string> Metric namespace (default is \"%s\")\n"
    //"  --total-bandwidth <Bw>    Limit total bandwidth (see multipliers below)\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, Mbps (bits per second), kBps, MBps (bytes per second)\n"
    "  <T>:  ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)\n",
    conf->max_connections,
    conf->connect_rate,
    number_of_cpus(), number_of_cpus() < 10 ? " " : "",
    conf->statsd_enable ? "enabled" : "disabled",
    conf->statsd_port,
    conf->statsd_namespace
    );
}
