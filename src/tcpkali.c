/*
 * Copyright (c) 2014, 2015  Machine Zone, Inc.
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
#include <libgen.h> /* basename(3) */
#include <ifaddrs.h>
#include <err.h>
#include <errno.h>
#include <assert.h>

#include <statsd.h>

#include "tcpkali.h"
#include "tcpkali_mavg.h"
#include "tcpkali_data.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_events.h"
#include "tcpkali_signals.h"
#include "tcpkali_websocket.h"
#include "tcpkali_transport.h"
#include "tcpkali_syslimits.h"
#include "tcpkali_terminfo.h"

/*
 * Describe the command line options.
 */
#define CLI_VERBOSE_OFFSET  (1<<8)
#define CLI_STATSD_OFFSET   (1<<9)
#define CLI_CHAN_OFFSET     (1<<10)
#define CLI_CONN_OFFSET     (1<<11)
#define CLI_SOCKET_OPT      (1<<12)
#define CLI_LATENCY         (1<<13)
#define CLI_DUMP            (1<<14)
static struct option cli_long_options[] = {
    { "channel-lifetime", 1, 0, CLI_CHAN_OFFSET + 't' },
    { "channel-bandwidth-upstream", 1, 0, 'U' },
    { "channel-bandwidth-downstream", 1, 0, 'D' },
    { "connections", 1, 0, 'c' },
    { "connect-rate", 1, 0, 'R' },
    { "connect-timeout", 1, 0,  CLI_CONN_OFFSET + 't' },
    { "duration", 1, 0, 'T' },
    { "dump-one", 0, 0, CLI_DUMP + '1' },
    { "dump-one-in", 0, 0, CLI_DUMP + 'i' },
    { "dump-one-out", 0, 0, CLI_DUMP + 'o' },
    { "dump-all", 0, 0, CLI_DUMP + 'a' },
    { "dump-all-in", 0, 0, CLI_DUMP + 'I' },
    { "dump-all-out", 0, 0, CLI_DUMP + 'O' },
    { "first-message", 1, 0, '1' },
    { "first-message-file", 1, 0, 'F' },
    { "help", 0, 0, 'H' },
    { "latency-connect", 0, 0,      CLI_LATENCY + 'c' },
    { "latency-first-byte", 0, 0,   CLI_LATENCY + 'f' },
    { "latency-marker", 1, 0,       CLI_LATENCY + 'm' },
    { "latency-marker-skip", 1, 0,  CLI_LATENCY + 's' },
    { "latency-percentiles", 1, 0,  CLI_LATENCY + 'p'},
    { "listen-port", 1, 0, 'l' },
    { "listen-mode", 1, 0, 'M' },
    { "message", 1, 0, 'm' },
    { "message-file", 1, 0, 'f' },
    { "message-rate", 1, 0, 'r' },
    { "nagle", 1, 0, 'N' },
    { "rcvbuf", 1, 0,           CLI_SOCKET_OPT + 'R' },
    { "sndbuf", 1, 0,           CLI_SOCKET_OPT + 'S' },
    { "source-ip", 1, 0, 'I' },
    { "statsd", 0, 0,           CLI_STATSD_OFFSET + 'e' },
    { "statsd-host", 1, 0,      CLI_STATSD_OFFSET + 'h' },
    { "statsd-port", 1, 0,      CLI_STATSD_OFFSET + 'p' },
    { "statsd-namespace", 1, 0, CLI_STATSD_OFFSET + 'n' },
    { "unescape-message-args", 0, 0, 'e' },
    { "version", 0, 0, 'V' },
    { "verbose", 1, 0, CLI_VERBOSE_OFFSET + 'v' },
    { "workers", 1, 0, 'w' },
    { "write-combine", 1, 0, 'C' },
    { "websocket", 0, 0, 'W' },
    { "ws", 0, 0, 'W' },
    { 0, 0, 0, 0 }
};

static struct tcpkali_config {
    int max_connections;
    double connect_rate;     /* New connects per second. */
    double test_duration;    /* Seconds for the full test. */
    int   statsd_enable;
    char *statsd_host;
    int   statsd_port;
    char *statsd_namespace;
    int   listen_port;      /* Port on which to listen. */
    char  *first_hostport;      /* A single (first) host:port specification */
    char  *first_path;          /* A /path specification from the first host */
} default_config = {
        .max_connections = 1,
        .connect_rate = 100.0,
        .test_duration = 10.0,
        .statsd_enable = 0,
        .statsd_host = "127.0.0.1",
        .statsd_port = 8125,
        .statsd_namespace = "tcpkali"
    };

struct stats_checkpoint {
    double epoch_start;   /* Start of current checkpoint epoch */
    double last_update;   /* Last we updated the checkpoint structure */
    non_atomic_traffic_stats initial_traffic_stats; /* Ramp-up phase traffic */
    non_atomic_traffic_stats last_traffic_stats;
};

enum work_phase {
    PHASE_ESTABLISHING_CONNECTIONS,
    PHASE_STEADY_STATE
};

/*
 * What we are sending to statsd?
 */
typedef struct {
    size_t opened;
    size_t conns_in;
    size_t conns_out;
    size_t bps_in;
    size_t bps_out;
    non_atomic_traffic_stats traffic_delta;
    struct latency_snapshot *latency;
} statsd_feedback;

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage_short(char *argv0);
static void usage_long(char *argv0, struct tcpkali_config *);
struct multiplier { char *prefix; double mult; };
static double parse_with_multipliers(const char *, char *str, struct multiplier *, int n);
static int parse_array_of_doubles(const char * option, char *str, struct array_of_doubles *array);
static int open_connections_until_maxed_out(struct engine *eng, double connect_rate, int max_connections, double epoch_end, struct stats_checkpoint *, mavg traffic_mavgs[2], Statsd *statsd, int *term_flag, enum work_phase phase, int print_stats);
static void print_connections_line(int conns, int max_conns, int conns_counter);
static void report_to_statsd(Statsd *statsd, statsd_feedback *opt);

static struct multiplier km_multiplier[] = { { "k", 1000 }, { "m", 1000000 } };
static struct multiplier kb_multiplier[] = { { "k", 1024 }, { "m", 1024*1024 } };
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
        .channel_lifetime = INFINITY,
        .nagle_setting = NSET_UNSET,
        .write_combine = WRCOMB_ON
    };
    int unescape_message_data = 0;

    struct array_of_doubles latency_percentiles = {
        .size = 0,
        .doubles = NULL,
    };

    while(1) {
        char *option = argv[optind];
        int c;
        c = getopt_long(argc, argv, "dhc:em:f:r:l:vw:T:", cli_long_options, NULL);
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
            usage_short(argv[0]);
            exit(EX_USAGE);
        case 'H':
            usage_long(argv[0], &default_config);
            exit(EX_USAGE);
        case 'v':   /* -v */
            engine_params.verbosity_level++;
            if(engine_params.verbosity_level >= _DBG_MAX) {
                engine_params.verbosity_level = (_DBG_MAX-1);
            }
            if(engine_params.verbosity_level >= DBG_DATA)
                engine_params.dump_setting = DS_DUMP_ALL;
            break;
        case CLI_VERBOSE_OFFSET + 'v':  /* --verbose <level> */
            engine_params.verbosity_level = atoi(optarg);
            if((int)engine_params.verbosity_level < 0
               || engine_params.verbosity_level >= _DBG_MAX) {
                fprintf(stderr, "Expecting --verbose=[0..%d]\n", _DBG_MAX-1);
                exit(EX_USAGE);
            }
            if(engine_params.verbosity_level >= DBG_DATA)
                engine_params.dump_setting = DS_DUMP_ALL;
            break;
        case 'd':   /* -d */
            /* FALL THROUGH */
        case CLI_DUMP + '1':    /* --dump-one */
            engine_params.dump_setting |= DS_DUMP_ONE;
            break;
        case CLI_DUMP + 'i':    /* --dump-one-in */
            engine_params.dump_setting |= DS_DUMP_ONE_IN;
            break;
        case CLI_DUMP + 'o':    /* --dump-one-out */
            engine_params.dump_setting |= DS_DUMP_ONE_OUT;
            break;
        case CLI_DUMP + 'a':    /* --dump-all */
            engine_params.dump_setting |= DS_DUMP_ALL;
            break;
        case CLI_DUMP + 'I':    /* --dump-all-in */
            engine_params.dump_setting |= DS_DUMP_ALL_IN;
            break;
        case CLI_DUMP + 'O':    /* --dump-all-out */
            engine_params.dump_setting |= DS_DUMP_ALL_OUT;
            break;
        case 'c':
            conf.max_connections = parse_with_multipliers(option, optarg,
                            km_multiplier,
                            sizeof(km_multiplier)/sizeof(km_multiplier[0]));
            if(conf.max_connections < 0) {
                fprintf(stderr, "Expecting --connections > 0\n");
                exit(EX_USAGE);
            }
            break;
        case 'R':
            conf.connect_rate = parse_with_multipliers(option, optarg,
                            km_multiplier,
                            sizeof(km_multiplier)/sizeof(km_multiplier[0]));
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
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_MESSAGE,
                                   optarg, strlen(optarg),
                                   unescape_message_data);
            break;
        case '1':   /* --first-message */
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_FIRST_MSG,
                                   optarg, strlen(optarg),
                                   unescape_message_data);
            break;
        case 'f': { /* --message-file */
            char  *data;
            size_t  size;
            if(read_in_file(optarg, &data, &size) != 0)
                exit(EX_DATAERR);
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_MESSAGE, data, size,
                                   unescape_message_data);
            free(data);
            }
            break;
        case 'F': { /* --first-message-file */
            char  *data;
            size_t  size;
            if(read_in_file(optarg, &data, &size) != 0)
                exit(EX_DATAERR);
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_FIRST_MSG, data, size,
                                   unescape_message_data);
            }
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
        case 'U': { /* --channel-bandwidth-upstream <Bw> */
            double Bps = parse_with_multipliers(option, optarg,
                        bw_multiplier,
                        sizeof(bw_multiplier)/sizeof(bw_multiplier[0]));
            if(Bps <= 0) {
                fprintf(stderr, "Expecting --channel-bandwidth-upstream > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_send_rate = RATE_BPS(Bps);
            break;
            }
        case 'D': { /* --channel-bandwidth-downstream <Bw> */
            double Bps = parse_with_multipliers(option, optarg,
                        bw_multiplier,
                        sizeof(bw_multiplier)/sizeof(bw_multiplier[0]));
            if(Bps < 0) {
                fprintf(stderr, "Expecting --channel-bandwidth-downstream > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_recv_rate = RATE_BPS(Bps);
            break;
            }
        case 'r': { /* --message-rate <Rate> */
            double rate = parse_with_multipliers(option, optarg,
                        km_multiplier,
                        sizeof(km_multiplier)/sizeof(km_multiplier[0]));
            if(rate <= 0) {
                fprintf(stderr, "Expecting --message-rate > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_send_rate = RATE_MPS(rate);
            break;
            }
        case 'N':   /* --nagle {on|off} */
            /* Enabling Nagle toggles off NODELAY */
            if(strcmp(optarg, "on") == 0)
                engine_params.nagle_setting = NSET_NODELAY_OFF;
            else if(strcmp(optarg, "off") == 0)
                engine_params.nagle_setting = NSET_NODELAY_ON;
            else {
                fprintf(stderr, "Expecting --nagle \"on\" or \"off\"\n");
                exit(EX_USAGE);
            }
            break;
        case 'C':   /* --write-combine {on|off} */
            if(strcmp(optarg, "on") == 0) {
                fprintf(stderr, "NOTE: --write-combine on is a default setting\n");
                engine_params.write_combine = WRCOMB_ON;
            } else if(strcmp(optarg, "off") == 0) {
                engine_params.write_combine = WRCOMB_OFF;
            } else {
                fprintf(stderr, "Expecting --write-combine off\n");
                exit(EX_USAGE);
            }
            break;
        case CLI_SOCKET_OPT + 'R': { /* --rcvbuf */
            long size = parse_with_multipliers(option, optarg,
                        kb_multiplier,
                        sizeof(kb_multiplier)/sizeof(kb_multiplier[0]));
            if(size <= 0) {
                fprintf(stderr, "Expecting --rcvbuf > 0\n");
                exit(EX_USAGE);
            }
            engine_params.sock_rcvbuf_size = size;
            }
            break;
        case CLI_SOCKET_OPT + 'S': { /* --sndbuf */
            long size = parse_with_multipliers(option, optarg,
                        kb_multiplier,
                        sizeof(kb_multiplier)/sizeof(kb_multiplier[0]));
            if(size <= 0) {
                fprintf(stderr, "Expecting --sndbuf > 0\n");
                exit(EX_USAGE);
            }
            engine_params.sock_sndbuf_size = size;
            }
            break;
        case CLI_STATSD_OFFSET + 'e':
            conf.statsd_enable = 1;
            break;
        case CLI_STATSD_OFFSET+'h':
            conf.statsd_host = strdup(optarg);
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
        case 'M':   /* --listen-mode={silent|active} */
            if(strcmp(optarg, "silent") == 0) {
                engine_params.listen_mode = LMODE_DEFAULT;
            } if(strcmp(optarg, "active") == 0) {
                engine_params.listen_mode &= ~_LMODE_SND_MASK;
                engine_params.listen_mode |= LMODE_ACTIVE;
            } else {
                fprintf(stderr, "--listen-mode=%s is not one of {silent|active}\n",
                    optarg);
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
            engine_params.websocket_enable = 1;
            break;
        case CLI_LATENCY + 'c': /* --latency-connect */
            engine_params.latency_setting |= LMEASURE_CONNECT;
            break;
        case CLI_LATENCY + 'f': /* --latency-first-byte */
            engine_params.latency_setting |= LMEASURE_FIRSTBYTE;
            break;
        case CLI_LATENCY + 'm': { /* --latency-marker */
            engine_params.latency_setting |= LMEASURE_MARKER;
            char *data  = strdup(optarg);
            size_t size = strlen(optarg);
            if(unescape_message_data)
                unescape_data(data, &size);
            if(size == 0) {
                fprintf(stderr, "--latency-marker: Non-empty marker expected\n");
                exit(EX_USAGE);
            }
            if(parse_expression(&engine_params.latency_marker, data, size, 0)
                    == -1) {
                fprintf(stderr, "--latency-marker: Failed to parse expression\n");
                exit(EX_USAGE);
            }
            }
            break;
        case CLI_LATENCY + 's': { /* --latency-marker-skip */
            engine_params.latency_marker_skip =
                            parse_with_multipliers(option, optarg,
                            km_multiplier,
                            sizeof(km_multiplier)/sizeof(km_multiplier[0]));
            if(engine_params.latency_marker_skip < 0) {
                fprintf(stderr, "--latency-marker-skip: "
                            "Failed to parse or out of range expression\n");
                exit(EX_USAGE);
            }
            }
            break;
        case CLI_LATENCY + 'p': { /* --latency-percentiles */
            if(parse_array_of_doubles(option, optarg, &latency_percentiles))
                exit(EX_USAGE);
            for(size_t i = 0; i < latency_percentiles.size; i++) {
                double number = latency_percentiles.doubles[i];
                if(number < 0.0 || number > 100.0) {
                    fprintf(stderr, "--latency-percentiles: "
                            "Percentile number should be within [0..100] range\n");
                    exit(EX_USAGE);
                }
            }
            }
            break;
        case 'I': { /* --source-ip */
                if(add_source_ip(&engine_params.source_addresses, optarg) < 0) {
                    fprintf(stderr, "--source-ip=%s: local IP address expected\n",
                                    optarg);
                    exit(EX_USAGE);
                }
            }
            break;
        default:
            fprintf(stderr, "%s: unknown option\n", option);
            usage_long(argv[0], &default_config);
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

        /* Figure out source IPs */
        if(engine_params.source_addresses.n_addrs == 0) {
            if(detect_source_ips(&engine_params.remote_addresses,
                                 &engine_params.source_addresses) < 0) {
                exit(EX_SOFTWARE);
            }
        } else {
            fprint_addresses(stderr, "Source IP: ",
                                     "\nSource IP: ", "\n",
                                     engine_params.source_addresses);
        }
    } else {
        conf.max_connections = 0;
    }
    if(conf.listen_port > 0) {
        engine_params.listen_addresses =
            detect_listen_addresses(conf.listen_port);
    }

    /*
     * Add final touches to the collection:
     * add websocket headers if needed, etc.
     */
    message_collection_finalize(&engine_params.message_collection,
                                engine_params.websocket_enable,
                                conf.first_hostport,
                                conf.first_path);

    int no_message_to_send = (0 == message_collection_estimate_size(
                &engine_params.message_collection,
                MSK_PURPOSE_MESSAGE, MSK_PURPOSE_MESSAGE));

    /*
     * Check that we will actually send messages
     * if we are also told to measure latency.
     */
    if(engine_params.latency_marker) {
        if(no_message_to_send || (argc - optind == 0)) {
            fprintf(stderr, "--latency-marker is given, but no messages "
                    "are supposed to be sent. Specify --message?\n");
            exit(EX_USAGE);
        }
    }

    /*
     * Make sure the message rate makes sense (e.g. the -m param is there).
     */
    if(engine_params.channel_send_rate.value_base == RS_MESSAGES_PER_SECOND
       && no_message_to_send) {
        if(engine_params.channel_send_rate.value_base) {
            fprintf(stderr, "--message-rate parameter makes no sense "
                            "without --message or --message-file\n");
            exit(EX_USAGE);
        }
    }

    /*
     * --write-combine=off makes little sense with Nagle on.
     * Disable Nagle or complain.
     */
    if(engine_params.write_combine == WRCOMB_OFF) {
        switch(engine_params.nagle_setting) {
        case NSET_UNSET:
            fprintf(stderr, "NOTE: --write-combine=off presumes --nagle=off.\n");
            engine_params.nagle_setting = NSET_NODELAY_OFF;
            break;
        case NSET_NODELAY_OFF:  /* --nagle=on */
            fprintf(stderr, "WARNING: --write-combine=off makes little sense "
                            "with --nagle=on.\n");
            break;
        case NSET_NODELAY_ON:   /* --nagle=off */
            /* This is the proper setting when --write-combine=off */
            break;
        }
    }

    if(optind == argc && conf.listen_port == 0) {
        fprintf(stderr, "Expecting target <host:port> or --listen-port. See -h or --help.\n");
        usage_short(argv[0]);
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
        statsd_new(&statsd, conf.statsd_host,
                            conf.statsd_port,
                            conf.statsd_namespace, NULL);
        /* Clear up traffic numbers, for better graphing. */
        report_to_statsd(statsd, 0);
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
        tcpkali_init_terminal();
    }

    /*
     * Traffic in/out moving average, smoothing period is 3 seconds.
     */
    mavg traffic_mavgs[2];
    mavg_init(&traffic_mavgs[0], tk_now(TK_DEFAULT), 3.0);
    mavg_init(&traffic_mavgs[1], tk_now(TK_DEFAULT), 3.0);
    struct stats_checkpoint checkpoint = { 0, 0, {0,0,0,0}, {0,0,0,0} };

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
            fprintf(stderr, "%s", tcpkali_clear_eol());
            fprintf(stderr, "Ramped up to %d connections.\n",
                conf.max_connections);
        } else {
            fprintf(stderr, "%s", tcpkali_clear_eol());
            fprintf(stderr, "Could not create %d connection%s"
                            " in allotted time (%gs)\n",
                            conf.max_connections,
                            conf.max_connections==1?"":"s",
                            conf.test_duration);
            /* Level down graphs/charts. */
            report_to_statsd(statsd, 0);
            exit(1);
        }
    }

    /*
     * Start measuring the steady-state performance, as opposed to
     * ramping up and waiting for the connections to be established.
     * (initial_traffic_stats) contain traffic numbers accumulated duing
     * ramp-up time.
     */
    checkpoint.initial_traffic_stats = engine_traffic(eng);
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

    fprintf(stderr, "%s", tcpkali_clear_eol());
    engine_terminate(eng, checkpoint.epoch_start,
                     checkpoint.initial_traffic_stats,
                     latency_percentiles);

    free(latency_percentiles.doubles);

    /* Send zeroes, otherwise graphs would continue showing non-zeroes... */
    report_to_statsd(statsd, 0);

    return 0;
}

static const char *time_progress(double start, double now, double stop) {
    const char *clocks[] = {
        "🕛  ", "🕐  ", "🕑  ", "🕒  ", "🕓  ", "🕔  ",
        "🕕  ", "🕖  ", "🕗  ", "🕘  ", "🕙  ", "🕚  "
    };
    if(!tcpkali_is_utf8()) return "";
    double span = (stop - start) / (sizeof(clocks)/sizeof(clocks[0]));
    int pos = (now - start) / span;
    if(pos < 0) pos = 0;
    else if(pos > 11) pos = 11;
    return clocks[pos];
}

static void format_latencies(char *buf, size_t size, struct latency_snapshot *latency) {
    if(latency->connect_histogram
      || latency->firstbyte_histogram
      || latency->marker_histogram) {
        char *p = buf;
        p += snprintf(p, size, " (");
        if(latency->connect_histogram)
            p += snprintf(p, size-(p-buf),
                    "c=%.1f ", hdr_value_at_percentile(
                    latency->connect_histogram, 95.0) / 10.0);
        if(latency->firstbyte_histogram)
            p += snprintf(p, size-(p-buf),
                    "fb=%.1f ", hdr_value_at_percentile(
                    latency->firstbyte_histogram, 95.0) / 10.0);
        if(latency->marker_histogram)
            p += snprintf(p, size-(p-buf),
                    "m=%.1f ", hdr_value_at_percentile(
                    latency->marker_histogram, 95.0) / 10.0);
        snprintf(p, size-(p-buf), "ms⁹⁵ᵖ)");
    } else {
        buf[0] = '\0';
    }
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
        int update_stats = (now - checkpoint->last_update) >= 0.25;

        size_t connecting, conns_in, conns_out, conns_counter;
        engine_get_connection_stats(eng, &connecting,
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
        pacefier_moved(&keepup_pace, connect_rate, allowed, now);

        /* Do not update/print checkpoint stats too often. */
        if(update_stats) {
            checkpoint->last_update = now;
            /* Fall through and do the chekpoint update. */
        } else {
            continue;
        }

        /*
         * traffic_delta.* contains traffic observed within the last
         * period (now - checkpoint->last_stats_sent).
         */
        non_atomic_traffic_stats _last = checkpoint->last_traffic_stats;
        checkpoint->last_traffic_stats = engine_traffic(eng);
        non_atomic_traffic_stats traffic_delta =
            subtract_traffic_stats(checkpoint->last_traffic_stats, _last);

        mavg_bump(&traffic_mavgs[0], now, (double)traffic_delta.bytes_rcvd);
        mavg_bump(&traffic_mavgs[1], now, (double)traffic_delta.bytes_sent);

        double bps_in = 8 * mavg_per_second(&traffic_mavgs[0], now);
        double bps_out = 8 * mavg_per_second(&traffic_mavgs[1], now);

        engine_prepare_latency_snapshot(eng);
        struct latency_snapshot *latency = engine_collect_latency_snapshot(eng);
        report_to_statsd(statsd,
            &(statsd_feedback){
                .opened = to_start,
                .conns_in = conns_in,
                .conns_out = conns_out,
                .bps_in = bps_in,
                .bps_out = bps_out,
                .traffic_delta = traffic_delta,
                .latency = latency
            });

        if(print_stats) {
            if(phase == PHASE_ESTABLISHING_CONNECTIONS) {
                print_connections_line(conns_out, max_connections,
                                       conns_counter);
            } else {
                char latency_buf[256];
                format_latencies(latency_buf, sizeof(latency_buf), latency);

                fprintf(stderr,
                    "%sTraffic %.3f↓, %.3f↑ Mbps "
                    "(conns %ld↓ %ld↑ %ld⇡; seen %ld)%s%s\r",
                    time_progress(checkpoint->epoch_start, now, epoch_end),
                    bps_in/1000000.0, bps_out/1000000.0,
                    (long)conns_in, (long)conns_out,
                    (long)connecting, (long)conns_counter,
                    latency_buf, tcpkali_clear_eol()
                );
            }
        }

        engine_free_latency_snapshot(latency);
    }

    return (now >= epoch_end || *term_flag) ? -1 : 0;
}

static void report_to_statsd(Statsd *statsd, statsd_feedback *sf) {
    static statsd_feedback empty_feedback;

    if(!statsd)
        return;
    if(!sf) sf = &empty_feedback;

    statsd_resetBatch(statsd);

#define SBATCH(t, str, value) do {                              \
        int ret = statsd_addToBatch(statsd, t, str, value, 1);  \
        if(ret == STATSD_BATCH_FULL) {                          \
            statsd_sendBatch(statsd);                           \
            ret = statsd_addToBatch(statsd, t, str, value, 1);  \
        }                                                       \
        assert(ret == STATSD_SUCCESS);                          \
    } while(0)

    SBATCH(STATSD_COUNT, "connections.opened", sf->opened);
    SBATCH(STATSD_GAUGE, "connections.total", sf->conns_in + sf->conns_out);
    SBATCH(STATSD_GAUGE, "connections.total.in", sf->conns_in);
    SBATCH(STATSD_GAUGE, "connections.total.out", sf->conns_out);
    SBATCH(STATSD_GAUGE, "traffic.bitrate", sf->bps_in + sf->bps_out);
    SBATCH(STATSD_GAUGE, "traffic.bitrate.in", sf->bps_in);
    SBATCH(STATSD_GAUGE, "traffic.bitrate.out", sf->bps_out);
    SBATCH(STATSD_COUNT, "traffic.data", sf->traffic_delta.bytes_rcvd
                                       + sf->traffic_delta.bytes_sent);
    SBATCH(STATSD_COUNT, "traffic.data.rcvd", sf->traffic_delta.bytes_rcvd);
    SBATCH(STATSD_COUNT, "traffic.data.sent", sf->traffic_delta.bytes_sent);
    SBATCH(STATSD_COUNT, "traffic.data.reads", sf->traffic_delta.num_reads);
    SBATCH(STATSD_COUNT, "traffic.data.writes", sf->traffic_delta.num_writes);

    if(sf->latency->marker_histogram || sf == &empty_feedback) {

        struct {
            unsigned p50;
            unsigned p95;
            unsigned p99;
            unsigned p99_5;
            unsigned mean;
            unsigned max;
        } lat;

        if(sf->latency->marker_histogram) {
            struct hdr_histogram *hist = sf->latency->marker_histogram;
            lat.p50 = hdr_value_at_percentile(hist, 50.0) / 10.0;
            lat.p95 = hdr_value_at_percentile(hist, 95.0) / 10.0;
            lat.p99 = hdr_value_at_percentile(hist, 99.0) / 10.0;
            lat.p99_5 = hdr_value_at_percentile(hist, 99.5) / 10.0;
            lat.mean = hdr_mean(hist)/10.0;
            lat.max = hdr_max(hist)/10.0;
            assert(lat.p95 < 1000000);
            assert(lat.mean < 1000000);
            assert(lat.max < 1000000);
        } else {
            memset(&lat, 0, sizeof(lat));
        }

        SBATCH(STATSD_GAUGE, "latency.mean", lat.mean);
        SBATCH(STATSD_GAUGE, "latency.50", lat.p50);
        SBATCH(STATSD_GAUGE, "latency.95", lat.p95);
        SBATCH(STATSD_GAUGE, "latency.99", lat.p99);
        SBATCH(STATSD_GAUGE, "latency.99.5", lat.p99_5);
        SBATCH(STATSD_GAUGE, "latency.max", lat.max);
    }

    statsd_sendBatch(statsd);
}

static void
print_connections_line(int conns, int max_conns, int conns_counter) {
    int terminal_width = tcpkali_terminal_width();

    char info[terminal_width + 1];
    ssize_t info_width = snprintf(info, sizeof(info),
                    "| %d of %d (%d)", conns, max_conns, conns_counter);

    int ribbon_width = terminal_width - info_width - 1;
    if(ribbon_width > 0.6 * terminal_width)
        ribbon_width = 0.6 * terminal_width;
    if(ribbon_width > 50) ribbon_width = 50;

    if(info_width > terminal_width || ribbon_width < 5) {
        /* Can't fit stuff on the screen, make dumb print-outs */
        printf("| %d of %d (%d)\n", conns, max_conns, conns_counter);
        return;
    }

    char ribbon[ribbon_width + 1];
    ribbon[0] = '|';
    int at = 1 + ((ribbon_width - 2) * conns) / max_conns;
    for(int i = 1; i < ribbon_width; i++) {
        if (i < at)      ribbon[i] = '=';
        else if(i  > at) ribbon[i] = '-';
        else if(i == at) ribbon[i] = '>';
    }
    ribbon[ribbon_width] = 0;
    fprintf(stderr, "%s%s%s\r", ribbon, info, tcpkali_clear_eol());
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
parse_array_of_doubles(const char *option, char *str, struct array_of_doubles *array) {
    double *doubles = array->doubles;
    size_t size = array->size;

    for (char *pos = str; *pos; pos++) {
        char *endpos;
        double got = strtod(pos, &endpos);
        if(pos == endpos) {
            fprintf(stderr, "%s: Failed to parse: bad number\n", option);
            return -1;
        }
        if(*endpos != 0 && *endpos != ',' && *endpos != '/') {
            fprintf(stderr, "%s: Failed to parse: "
                    "invalid separator, use ',' or '/'\n", option);
            return -1;
        }
        doubles = realloc(doubles, ++size * sizeof(double));
        doubles[size-1] = got;

        pos = endpos;
        if(*pos == 0)
            break;
    }

    array->doubles = doubles;
    array->size = size;
    return 0;
}

/*
 * Display the Usage screen.
 */
static void
usage_long(char *argv0, struct tcpkali_config *conf) {
    fprintf(stderr, "Usage: %s [OPTIONS] [-l <port>] [<host:port>...]\n",
        basename(argv0));
    fprintf(stderr,
    "Where OPTIONS are:\n"
    "  -h                           Print short help screen, then exit\n"
    "  --help                       Print this help screen, then exit\n"
    "  --version                    Print version number, then exit\n"
    "  -v, --verbose <level=1>      Increase (-v) or set verbosity level [0..%d]\n"
    "  -d, --dump-one               Dump i/o data for a single connection\n"
    "  --dump-one-in                Dump incoming data for a single connection\n"
    "  --dump-one-out               Dump outgoing data for a single connection\n"
    "  --dump-{all,all-in,all-out}  Dump i/o data for all connections\n"
    "  --nagle {on|off}             Control Nagle algorithm (set TCP_NODELAY)\n"
    "  --rcvbuf <SizeBytes>         Set TCP receive buffers (set SO_RCVBUF)\n"
    "  --sndbuf <SizeBytes>         Set TCP rend buffers (set SO_SNDBUF)\n"
    "  --source-ip <IP>             Use the specified IP address to connect\n"
    "  --write-combine off          Disable batching adjacent writes\n"
    "  -w, --workers <N=%ld>%s         Number of parallel threads to use\n"
    "\n"
    "  --ws, --websocket            Use RFC6455 WebSocket transport\n"
    "  -c, --connections <N=%d>      Connections to keep open to the destinations\n"
    "  --connect-rate <Rate=%g>    Limit number of new connections per second\n"
    "  --connect-timeout <Time=1s>  Limit time spent in a connection attempt\n"
    "  --channel-lifetime <Time>    Shut down each connection after Time seconds\n"
    "  --channel-bandwidth-upstream <Bandwidth>     Limit upstream bandwidth\n"
    "  --channel-bandwidth-downstream <Bandwidth>   Limit downstream bandwidth\n"
    "  -l, --listen-port <port>     Listen on the specified port\n"
    "  --listen-mode=<mode>         What to do upon client connect, where <mode> is:\n"
    "               \"silent\"        Do not send data, ignore received data (default)\n"
    "               \"active\"        Actively send messages\n"
    "  -T, --duration <Time=10s>    Exit after the specified amount of time\n"
    "\n"
    "  -e, --unescape-message-args  Unescape the message data arguments\n"
    "  --first-message <string>     Send this message first, once\n"
    "  --first-message-file <name>  Read the first message from a file\n"
    "  -m, --message <string>       Message to repeatedly send to the remote\n"
    "  -f, --message-file <name>    Read message to send from a file\n"
    "  -r, --message-rate <Rate>    Messages per second to send in a connection\n"
    "\n"
    "  --latency-connect            Measure TCP connection establishment latency\n"
    "  --latency-first-byte         Measure time to first byte latency\n"
    "  --latency-marker <string>    Measure latency using a per-message marker\n"
    "  --latency-marker-skip <N>    Ignore the first N occurrences of a marker\n"
    "  --latency-percentiles <list> Report latency at specified percentiles\n"
    "\n"
    "  --statsd                     Enable StatsD output (default %s)\n"
    "  --statsd-host <host>         StatsD host to send data (default is localhost)\n"
    "  --statsd-port <port>         StatsD port to use (default is %d)\n"
    "  --statsd-namespace <string>  Metric namespace (default is \"%s\")\n"
    "\n"
    "Variable units and recognized multipliers:\n"
    "  <N>, <Rate>:  k (1000, as in \"5k\" is 5000), m (1000000)\n"
    "  <SizeBytes>:  k (1024, as in \"5k\" is 5120), m (1024*1024)\n"
    "  <Bandwidth>:  kbps, Mbps (bits per second), kBps, MBps (bytes per second)\n"
    "  <Time>:       ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)\n"
    "  <Rate> and <Time> can be fractional values, such as 0.25.\n",


    (_DBG_MAX - 1),
    number_of_cpus(), number_of_cpus() < 10 ? " " : "",
    conf->max_connections,
    conf->connect_rate,
    conf->statsd_enable ? "enabled" : "disabled",
    conf->statsd_port,
    conf->statsd_namespace
    );
}

static void
usage_short(char *argv0) {
    fprintf(stderr, "Usage: %s [OPTIONS] [-l <port>] [<host:port>...]\n",
        basename(argv0));
    fprintf(stderr,
    "Where some OPTIONS are:\n"
    "  -h                   Print this help screen, then exit\n"
    "  --help               Print long help screen, then exit\n"
    "  -d                   Dump i/o data for a single connection\n"
    "\n"
    "  -c <N>               Connections to keep open to the destinations\n"
    "  -l <port>            Listen on the specified port\n"
    "  --ws, --websocket    Use RFC6455 WebSocket transport\n"
    "  -T <Time=10s>        Exit after the specified amount of time\n"
    "\n"
    "  -e                   Unescape backslash-escaping in a message string\n"
    "  -m <string>          Message to repeatedly send to the remote\n"
    "  -r <Rate>            Messages per second to send in a connection\n"
    "\n"
    "Variable units and recognized multipliers:\n"
    "  <N>, <Rate>:  k (1000, as in \"5k\" is 5000), m (1000000)\n"
    "  <Time>:       ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)\n"
    "  <Rate> and <Time> can be fractional values, such as 0.25.\n"
    "\n"
    "Use `%s --help` or `man tcpkali` for a full set of supported options.\n",
    basename(argv0)

    );

}

