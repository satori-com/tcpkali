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
#define _ISOC99_SOURCE
#define _BSD_SOURCE
#include <getopt.h>
#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
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
#include <time.h>

#include <statsd.h>

#include "tcpkali.h"
#include "tcpkali_run.h"
#include "tcpkali_mavg.h"
#include "tcpkali_data.h"
#include "tcpkali_events.h"
#include "tcpkali_signals.h"
#include "tcpkali_terminfo.h"
#include "tcpkali_websocket.h"
#include "tcpkali_transport.h"
#include "tcpkali_syslimits.h"
#include "tcpkali_logging.h"
#include "tcpkali_ssl.h"

/*
 * Describe the command line options.
 */
#define CLI_VERBOSE_OFFSET (1 << 8)
#define CLI_STATSD_OFFSET (1 << 9)
#define CLI_CHAN_OFFSET (1 << 10)
#define CLI_CONN_OFFSET (1 << 11)
#define CLI_SOCKET_OPT (1 << 12)
#define CLI_LATENCY (1 << 13)
#define CLI_DUMP (1 << 14)
#define SSL_OPT (1 << 15)
static struct option cli_long_options[] = {
    {"channel-lifetime", 1, 0, CLI_CHAN_OFFSET + 't'},
    {"channel-bandwidth-upstream", 1, 0, 'U'},
    {"channel-bandwidth-downstream", 1, 0, 'D'},
    {"connections", 1, 0, 'c'},
    {"connect-rate", 1, 0, 'R'},
    {"connect-timeout", 1, 0, CLI_CONN_OFFSET + 't'},
    {"delay-send", 1, 0, CLI_CONN_OFFSET + 'z'},
    {"duration", 1, 0, 'T'},
    {"dump-one", 0, 0, CLI_DUMP + '1'},
    {"dump-one-in", 0, 0, CLI_DUMP + 'i'},
    {"dump-one-out", 0, 0, CLI_DUMP + 'o'},
    {"dump-all", 0, 0, CLI_DUMP + 'a'},
    {"dump-all-in", 0, 0, CLI_DUMP + 'I'},
    {"dump-all-out", 0, 0, CLI_DUMP + 'O'},
    {"first-message", 1, 0, '1'},
    {"first-message-file", 1, 0, 'F'},
    {"help", 0, 0, 'E'},
    {"header", 1, 0, 'H'},
    {"latency-connect", 0, 0, CLI_LATENCY + 'c'},
    {"latency-first-byte", 0, 0, CLI_LATENCY + 'f'},
    {"latency-marker", 1, 0, CLI_LATENCY + 'm'},
    {"latency-marker-skip", 1, 0, CLI_LATENCY + 's'},
    {"latency-percentiles", 1, 0, CLI_LATENCY + 'p'},
    {"listen-port", 1, 0, 'l'},
    {"listen-mode", 1, 0, 'L'},
    {"message", 1, 0, 'm'},
    {"message-file", 1, 0, 'f'},
    {"message-rate", 1, 0, 'r'},
    {"message-stop", 1, 0, 's'},
    {"nagle", 1, 0, 'N'},
    {"rcvbuf", 1, 0, CLI_SOCKET_OPT + 'R'},
    {"server", 1, 0, 'S'},
    {"sndbuf", 1, 0, CLI_SOCKET_OPT + 'S'},
    {"source-ip", 1, 0, 'I'},
    {"ssl", 0, 0, SSL_OPT},
    {"ssl-cert", 1, 0, SSL_OPT + 'c'},
    {"ssl-key", 1, 0, SSL_OPT + 'k'},
    {"statsd", 0, 0, CLI_STATSD_OFFSET + 'e'},
    {"statsd-host", 1, 0, CLI_STATSD_OFFSET + 'h'},
    {"statsd-port", 1, 0, CLI_STATSD_OFFSET + 'p'},
    {"statsd-namespace", 1, 0, CLI_STATSD_OFFSET + 'n'},
    {"statsd-latency-window", 1, 0, CLI_STATSD_OFFSET + 'w'},
    {"unescape-message-args", 0, 0, 'e'},
    {"version", 0, 0, 'V'},
    {"verbose", 1, 0, CLI_VERBOSE_OFFSET + 'v'},
    {"workers", 1, 0, 'w'},
    {"write-combine", 1, 0, 'C'},
    {"websocket", 0, 0, 'W'},
    {"ws", 0, 0, 'W'},
    {"message-marker", 0, 0, 'M'},
    {0, 0, 0, 0}};

static struct tcpkali_config {
    int max_connections;
    double connect_rate;  /* New connects per second. */
    double test_duration; /* Seconds for the full test. */
    double latency_window;  /* Seconds */
    int statsd_enable;
    char *statsd_host;
    int statsd_port;
    char *statsd_namespace;
    char *listen_host;    /* Address on which to listen. Can be NULL */
    int listen_port;      /* Port on which to listen. */
    char *first_hostport; /* A single (first) host:port specification */
    char *first_path;     /* A /path specification from the first host */
    struct http_headers {
        size_t offset;
        char buffer[1024];
    } http_headers;
} default_config = {.max_connections = 1,
                    .connect_rate = 100.0,
                    .test_duration = 10.0,
                    .statsd_enable = 0,
                    .statsd_host = "127.0.0.1",
                    .statsd_port = 8125,
                    .statsd_namespace = "tcpkali"};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage_short(char *argv0);
static void usage_long(char *argv0, struct tcpkali_config *);
struct multiplier {
    char *prefix;
    double mult;
};
static double parse_with_multipliers(const char *, char *str,
                                     struct multiplier *, int n);
static int parse_percentile_values(const char *option, char *str,
                                   struct percentile_values *array);

/* clang-format off */
static struct multiplier km_multiplier[] = { { "k", 1000 }, { "m", 1000000 } };
static struct multiplier kb_multiplier[] = { { "k", 1024 }, { "m", 1024*1024 } };
static struct multiplier s_multiplier[] = {
    { "ms", 0.001 }, { "millisecond", 0.001 }, { "milliseconds", 0.001 },
    { "s", 1 }, { "second", 1 }, { "seconds", 1 },
    { "m", 60 }, { "min", 60 }, { "minute", 60 }, { "minutes", 60 },
    { "h", 3600 }, { "hr", 3600 }, { "hour", 3600 }, { "hours", 3600 },
    { "d", 86400 }, { "day", 86400 }, { "days", 86400 },
    { "y", 31536000 }, { "year", 31536000 }, { "years", 31536000 }
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
/* clang-format on */

#ifndef  HAVE_SRANDOMDEV
void portable_srandomdev() {
    FILE *f = fopen("/dev/urandom", "rb");
    if(f) {
        unsigned int r;
        if(fread(&r, sizeof(r), 1, f) == 1) {
            fclose(f);
            srandom(r);
            return;
        }
        fclose(f);
    }
    /* No usable /dev/urandom, too bad. */
    srandom(getpid() ^ time(NULL) ^ atol(getenv("TCPKALI_RANDOM")?:"0"));
}
#endif

/*
 * Parse command line options and kick off the engine.
 */
int
main(int argc, char **argv) {
    struct tcpkali_config conf = default_config;
    struct engine_params engine_params = {.verbosity_level = DBG_ERROR,
                                          .connect_timeout = 1.0,
                                          .channel_lifetime = INFINITY,
                                          .delay_send = 0.0,
                                          .nagle_setting = NSET_UNSET,
                                          .ssl_enable = 0,
                                          .ssl_cert = "cert.pem",
                                          .ssl_key = "key.pem",
                                          .write_combine = WRCOMB_ON};
    struct rate_modulator rate_modulator = {.state = RM_UNMODULATED};
    int unescape_message_data = 0;

    struct orchestration_args orch_args = {.enabled = 0,
                                           .server_addrs = NULL,
                                           .server_addr_str = NULL};

#ifdef HAVE_SRANDOMDEV
    srandomdev();
#else
    portable_srandomdev();
#endif

    struct percentile_values latency_percentiles = {
        .size = 0, .values = NULL,
    };

    while(1) {
        char *option = argv[optind];
        int longindex = -1;
        int c;
        c = getopt_long(argc, argv, "dhc:e1:m:f:r:l:vw:T:H:", cli_long_options,
                        &longindex);
        if(c == -1) break;
        switch(c) {
        case 'V':
            printf(PACKAGE_NAME " version " VERSION
#ifdef USE_LIBUV
                                " (libuv"
#else
                                " (libev"
#endif
#ifdef HAVE_OPENSSL
                                ", ssl"
#endif
                                ")\n");
            exit(0);
        case 'h':
            usage_short(argv[0]);
            exit(EX_USAGE);
        case 'E':
            usage_long(argv[0], &default_config);
            exit(EX_USAGE);
        case 'v': /* -v */
            engine_params.verbosity_level++;
            if(engine_params.verbosity_level >= _DBG_MAX) {
                engine_params.verbosity_level = (_DBG_MAX - 1);
            }
            break;
        case 'H': {
            char *hdrbuf = strdup(optarg);
            size_t hdrlen = strlen(optarg);
            if(unescape_message_data) unescape_data(hdrbuf, &hdrlen);
            char buffer[PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(hdrlen)];
            if(hdrlen == 0) {
                warning("--header=\"\" value is empty, ignored\n");
                break;
            }
            if(!isalpha(hdrbuf[0])) {
                warning(
                    "--header=%s starts with '%c', are you sure?\n",
                    printable_data(buffer, sizeof(buffer), hdrbuf, hdrlen, 1),
                    hdrbuf[0]);
            }
            char *lf = memchr(hdrbuf, '\n', hdrlen);
            if(lf) {
                fprintf(stderr,
                    "--header=%s should not contain '\\n'\n",
                    printable_data(buffer, sizeof(buffer), hdrbuf, hdrlen, 1));
                exit(EX_USAGE);
            }

            if(conf.http_headers.offset + hdrlen + 2
               >= sizeof(conf.http_headers.buffer)) {
                fprintf(stderr, "--header adds too many HTTP headers\n");
                exit(EX_USAGE);
            } else {
                memcpy(conf.http_headers.buffer + conf.http_headers.offset,
                       hdrbuf, hdrlen);
                conf.http_headers.offset += hdrlen;
                memcpy(conf.http_headers.buffer + conf.http_headers.offset,
                       "\r\n", sizeof("\r\n"));
                conf.http_headers.offset += sizeof("\r\n") - 1;
                free(hdrbuf);
            }
            } break;
        case CLI_VERBOSE_OFFSET + 'v': /* --verbose <level> */
            engine_params.verbosity_level = atoi(optarg);
            if((int)engine_params.verbosity_level < 0
               || engine_params.verbosity_level >= _DBG_MAX) {
                fprintf(stderr, "Expecting --verbose=[0..%d]\n", _DBG_MAX - 1);
                exit(EX_USAGE);
            }
            break;
        case 'd': /* -d */
        /* FALL THROUGH */
        case CLI_DUMP + '1': /* --dump-one */
            engine_params.dump_setting |= DS_DUMP_ONE;
            break;
        case CLI_DUMP + 'i': /* --dump-one-in */
            engine_params.dump_setting |= DS_DUMP_ONE_IN;
            break;
        case CLI_DUMP + 'o': /* --dump-one-out */
            engine_params.dump_setting |= DS_DUMP_ONE_OUT;
            break;
        case CLI_DUMP + 'a': /* --dump-all */
            engine_params.dump_setting |= DS_DUMP_ALL;
            break;
        case CLI_DUMP + 'I': /* --dump-all-in */
            engine_params.dump_setting |= DS_DUMP_ALL_IN;
            break;
        case CLI_DUMP + 'O': /* --dump-all-out */
            engine_params.dump_setting |= DS_DUMP_ALL_OUT;
            break;
        case 'c':
            conf.max_connections = parse_with_multipliers(
                option, optarg, km_multiplier,
                sizeof(km_multiplier) / sizeof(km_multiplier[0]));
            if(conf.max_connections < 0) {
                fprintf(stderr, "Expecting --connections > 0\n");
                exit(EX_USAGE);
            }
            break;
        case 'R':
            conf.connect_rate = parse_with_multipliers(
                option, optarg, km_multiplier,
                sizeof(km_multiplier) / sizeof(km_multiplier[0]));
            if(conf.connect_rate <= 0) {
                fprintf(stderr, "Expected positive --connect-rate=%s\n",
                        optarg);
                exit(EX_USAGE);
            }
            break;
        case 'T':
            conf.test_duration = parse_with_multipliers(
                option, optarg, s_multiplier,
                sizeof(s_multiplier) / sizeof(s_multiplier[0]));
            if(conf.test_duration == 0 || conf.test_duration < -1) {
                fprintf(stderr, "Expected positive --duration=%s\n", optarg);
                exit(EX_USAGE);
            }
            if(conf.test_duration == -1) {
                conf.test_duration = INFINITY;
            }
            break;
        case 'e':
            unescape_message_data = 1;
            break;
        case 'M': /* --message-marker */
            if(!engine_params.message_marker
                && (engine_params.latency_setting & SLT_MARKER)) {
                fprintf(stderr,
                        "--message-marker: --latency-marker is already specified, use one or the other\n");
                exit(EX_USAGE);
            }
            engine_params.message_marker = 1;
            break;
        case 'm': /* --message */
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_MESSAGE, optarg, strlen(optarg),
                                   unescape_message_data, 1);
            break;
        case '1': /* --first-message */
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_FIRST_MSG, optarg,
                                   strlen(optarg), unescape_message_data, 1);
            break;
        case 'f': { /* --message-file */
            char *data;
            size_t size;
            if(read_in_file(optarg, &data, &size) != 0) exit(EX_DATAERR);
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_MESSAGE, data, size,
                                   unescape_message_data, 1);
            free(data);
        } break;
        case 'F': { /* --first-message-file */
            char *data;
            size_t size;
            if(read_in_file(optarg, &data, &size) != 0) exit(EX_DATAERR);
            message_collection_add(&engine_params.message_collection,
                                   MSK_PURPOSE_FIRST_MSG, data, size,
                                   unescape_message_data, 1);
        } break;
        case 'w': {
            int n = atoi(optarg);
            if(n <= 0) {
                if(optarg[0] >= '0' && optarg[0] <= '9') {
                    fprintf(stderr, "Expected --workers > 1\n");
                } else if(optarg[0] == 's') {
                    fprintf(stderr,
                            "Expected -w <N> (--workers) or --ws "
                            "(--websocket), but not -ws.\n");
                } else {
                    fprintf(stderr,
                            "Expected --workers <N> (-w <N>), or --websocket "
                            "(--ws)\n");
                }
                exit(EX_USAGE);
            }
            if(n > number_of_cpus()) {
                fprintf(stderr,
                        "Value --workers=%d is unreasonably large,"
                        " only %ld CPU%s detected\n",
                        n, number_of_cpus(), number_of_cpus() == 1 ? "" : "s");
                exit(EX_USAGE);
            }
            engine_params.requested_workers = n;
            break;
        }
        case 'U': { /* --channel-bandwidth-upstream <Bw> */
            double Bps = parse_with_multipliers(
                option, optarg, bw_multiplier,
                sizeof(bw_multiplier) / sizeof(bw_multiplier[0]));
            if(Bps <= 0) {
                fprintf(stderr, "Expecting --channel-bandwidth-upstream > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_send_rate = RATE_BPS(Bps);
            break;
        }
        case 'D': { /* --channel-bandwidth-downstream <Bw> */
            double Bps = parse_with_multipliers(
                option, optarg, bw_multiplier,
                sizeof(bw_multiplier) / sizeof(bw_multiplier[0]));
            if(Bps < 0) {
                fprintf(stderr,
                        "Expecting --channel-bandwidth-downstream > 0\n");
                exit(EX_USAGE);
            }
            engine_params.channel_recv_rate = RATE_BPS(Bps);
            break;
        }
        case 'r': { /* --message-rate <Rate> */
            if(optarg[0] == '@') {
                double latency = parse_with_multipliers(
                    option, optarg + 1, s_multiplier,
                    sizeof(s_multiplier) / sizeof(s_multiplier[0]));
                if(latency <= 0) {
                    fprintf(stderr, "Expecting --message-rate @<Latency>\n");
                    exit(EX_USAGE);
                }

                /* Override -r<Rate> spec. */
                if(rate_modulator.mode == RM_UNMODULATED
                   && engine_params.channel_send_rate.value_base
                          != RS_UNLIMITED) {
                    warning(
                        "--message-rate %s overrides previous fixed "
                        "--message-rate specification.\n",
                        optarg);
                }
                engine_params.channel_send_rate = RATE_MPS(100); /* Initial */
                rate_modulator.mode = RM_MAX_RATE_AT_TARGET_LATENCY;
                rate_modulator.latency_target = latency;
                rate_modulator.latency_target_s = strdup(optarg + 1);
                conf.test_duration = INFINITY;
            } else {
                double rate = parse_with_multipliers(
                    option, optarg, km_multiplier,
                    sizeof(km_multiplier) / sizeof(km_multiplier[0]));
                if(rate <= 0) {
                    fprintf(stderr, "Expecting --message-rate > 0\n");
                    exit(EX_USAGE);
                }
                engine_params.channel_send_rate = RATE_MPS(rate);
                /* Override -r@<Time> spec. */
                if(rate_modulator.mode != RM_UNMODULATED) {
                    warning(
                        "--message-rate %s overrides previous dynamic "
                        "--message-rate specification.\n",
                        optarg);
                    rate_modulator.mode = RM_UNMODULATED;
                }
            }
            break;
        }
        case 's': { /* --message-stop */
            char *data = strdup(optarg);
            size_t size = strlen(optarg);
            if(unescape_message_data) unescape_data(data, &size);
            if(size == 0) {
                fprintf(stderr,
                        "--message-stop: Non-empty message expected\n");
                exit(EX_USAGE);
            }
            if(parse_expression(&engine_params.message_stop_expr, data, size,
                                0)
               == -1) {
                fprintf(stderr,
                        "--message-stop: Failed to parse expression\n");
                exit(EX_USAGE);
            } else if(!EXPR_IS_TRIVIAL(engine_params.message_stop_expr)) {
                fprintf(stderr,
                        "--message-stop: Non-trivial expressions are not "
                        "supported\n");
                exit(EX_USAGE);
            }
        } break;
        case 'N': /* --nagle {on|off} */
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
        case 'C': /* --write-combine {on|off} */
            if(strcmp(optarg, "on") == 0) {
                fprintf(stderr,
                        "NOTE: --write-combine on is a default setting\n");
                engine_params.write_combine = WRCOMB_ON;
            } else if(strcmp(optarg, "off") == 0) {
                engine_params.write_combine = WRCOMB_OFF;
            } else {
                fprintf(stderr, "Expecting --write-combine off\n");
                exit(EX_USAGE);
            }
            break;
        case CLI_SOCKET_OPT + 'R': { /* --rcvbuf */
            long size = parse_with_multipliers(
                option, optarg, kb_multiplier,
                sizeof(kb_multiplier) / sizeof(kb_multiplier[0]));
            if(size <= 0) {
                fprintf(stderr, "Expecting --rcvbuf > 0\n");
                exit(EX_USAGE);
            }
            engine_params.sock_rcvbuf_size = size;
        } break;
        case CLI_SOCKET_OPT + 'S': { /* --sndbuf */
            long size = parse_with_multipliers(
                option, optarg, kb_multiplier,
                sizeof(kb_multiplier) / sizeof(kb_multiplier[0]));
            if(size <= 0) {
                fprintf(stderr, "Expecting --sndbuf > 0\n");
                exit(EX_USAGE);
            }
            engine_params.sock_sndbuf_size = size;
        } break;
        case CLI_STATSD_OFFSET + 'e':
            conf.statsd_enable = 1;
            break;
        case CLI_STATSD_OFFSET + 'h':
            conf.statsd_host = strdup(optarg);
            break;
        case CLI_STATSD_OFFSET + 'n':
            conf.statsd_namespace = strdup(optarg);
            break;
        case CLI_STATSD_OFFSET + 'p':
            conf.statsd_port = atoi(optarg);
            if(conf.statsd_port <= 0 || conf.statsd_port >= 65535) {
                fprintf(stderr, "--statsd-port=%d is not in [1..65535]\n",
                        conf.statsd_port);
                exit(EX_USAGE);
            }
            break;
        case CLI_STATSD_OFFSET + 'w':
            conf.latency_window = parse_with_multipliers(
                option, optarg, s_multiplier,
                sizeof(s_multiplier) / sizeof(s_multiplier[0]));
            if(conf.latency_window <= 0) {
                fprintf(stderr, "Expected positive --statsd-latency-window=%s\n", optarg);
                exit(EX_USAGE);
            }
            break;
        case 'l': {
            const char *port = optarg;
            const char *colon = strchr(optarg, ':');
            if(colon) {
                port = colon + 1;
                free(conf.listen_host);
                conf.listen_host = strdup(optarg);
                assert(conf.listen_host);
                conf.listen_host[colon - optarg] = '\0';
            }
            conf.listen_port = atoi(port);
            if(conf.listen_port <= 0 || conf.listen_port >= 65535) {
                fprintf(stderr, "--listen-port=%d is not in [1..65535]\n",
                        conf.listen_port);
                exit(EX_USAGE);
            }
            }
            break;
        case 'L': /* --listen-mode={silent|active} */
            if(strcmp(optarg, "silent") == 0) {
                engine_params.listen_mode = LMODE_DEFAULT;
            }
            if(strcmp(optarg, "active") == 0) {
                engine_params.listen_mode &= ~_LMODE_SND_MASK;
                engine_params.listen_mode |= LMODE_ACTIVE;
            } else {
                fprintf(stderr,
                        "--listen-mode=%s is not one of {silent|active}\n",
                        optarg);
                exit(EX_USAGE);
            }
            break;
        case CLI_CONN_OFFSET + 't':
            engine_params.connect_timeout = parse_with_multipliers(
                option, optarg, s_multiplier,
                sizeof(s_multiplier) / sizeof(s_multiplier[0]));
            if(engine_params.connect_timeout <= 0.0) {
                fprintf(stderr, "Expected positive --connect-timeout=%s\n",
                        optarg);
                exit(EX_USAGE);
            }
            break;
        case CLI_CONN_OFFSET + 'z': /* --delay-send */
            engine_params.delay_send = parse_with_multipliers(
                option, optarg, s_multiplier,
                sizeof(s_multiplier) / sizeof(s_multiplier[0]));
            break;
        case CLI_CHAN_OFFSET + 't':
            engine_params.channel_lifetime = parse_with_multipliers(
                option, optarg, s_multiplier,
                sizeof(s_multiplier) / sizeof(s_multiplier[0]));
            if(engine_params.channel_lifetime < 0.0) {
                fprintf(stderr, "Expected non-negative --channel-lifetime=%s\n",
                        optarg);
                exit(EX_USAGE);
            }
            break;
        case 'W': /* --websocket: Enable WebSocket framing */
            engine_params.websocket_enable = 1;
            break;
        case SSL_OPT: /* --ssl: Enable TLS */
#ifdef HAVE_OPENSSL
            engine_params.ssl_enable = 1;
#else
            fprintf(stderr, "Compiled without TLS support\n");
            exit(EX_USAGE);
#endif
            break;
        case SSL_OPT + 'c': /* --ssl-cert: X.509 certificate file */
#ifdef HAVE_OPENSSL
            engine_params.ssl_cert = strdup(optarg);
#else
            fprintf(stderr, "Compiled without TLS support\n");
            exit(EX_USAGE);
#endif
            break;
        case SSL_OPT + 'k': /* --ssl-key: SSL private key file */
#ifdef HAVE_OPENSSL
            engine_params.ssl_key = strdup(optarg);
#else
            fprintf(stderr, "Compiled without TLS support\n");
            exit(EX_USAGE);
#endif
            break;
        case CLI_LATENCY + 'c': /* --latency-connect */
            engine_params.latency_setting |= SLT_CONNECT;
            break;
        case CLI_LATENCY + 'f': /* --latency-first-byte */
            engine_params.latency_setting |= SLT_FIRSTBYTE;
            break;
        case CLI_LATENCY + 'm': { /* --latency-marker */
            if(engine_params.message_marker) {
                fprintf(stderr,
                        "--latency-marker: --message-marker is already specified, use one or the other\n");
                exit(EX_USAGE);
            }
            engine_params.latency_setting |= SLT_MARKER;
            char *data = strdup(optarg);
            size_t size = strlen(optarg);
            if(unescape_message_data) unescape_data(data, &size);
            if(size == 0) {
                fprintf(stderr,
                        "--latency-marker: Non-empty marker expected\n");
                exit(EX_USAGE);
            }
            if(parse_expression(&engine_params.latency_marker_expr, data, size,
                                0)
               == -1) {
                fprintf(stderr,
                        "--latency-marker: Failed to parse expression\n");
                exit(EX_USAGE);
            }
        } break;
        case CLI_LATENCY + 's': { /* --latency-marker-skip */
            engine_params.latency_marker_skip = parse_with_multipliers(
                option, optarg, km_multiplier,
                sizeof(km_multiplier) / sizeof(km_multiplier[0]));
            if(engine_params.latency_marker_skip < 0) {
                fprintf(stderr,
                        "--latency-marker-skip: "
                        "Failed to parse or out of range expression\n");
                exit(EX_USAGE);
            }
        } break;
        case CLI_LATENCY + 'p': { /* --latency-percentiles */
            if(parse_percentile_values(cli_long_options[longindex].name,
                                       optarg, &latency_percentiles))
                exit(EX_USAGE);
        } break;
        case 'I': { /* --source-ip */
            if(add_source_ip(&engine_params.source_addresses, optarg) < 0) {
                fprintf(stderr, "--source-ip=%s: local IP address expected\n",
                        optarg);
                exit(EX_USAGE);
            }
        } break;
        case 'S': { /* --server */
            orch_args.enabled = 1;
            orch_args.server_addr_str = strdup(optarg);
            resolve_address(optarg, &orch_args.server_addrs);
        } break;
        default:
            fprintf(stderr, "%s: unknown option\n", option);
            usage_long(argv[0], &default_config);
            exit(EX_USAGE);
        }
    }

    struct orchestration_data orch_state = {.connected = 0};
    if(orch_args.enabled) {
        orch_state = tcpkali_connect_to_orch_server(orch_args);
        if(!orch_state.connected) {
            fprintf(stderr,
                    "Failed to connect to orchestration server at %s\n",
                    orch_args.server_addr_str);
            exit(EX_UNAVAILABLE);
        }
        TcpkaliMessage_t* msg = tcpkali_wait_for_start_command(&orch_state);
        if (!msg) {
            exit(EX_UNAVAILABLE);
        }
        if (msg->present != TcpkaliMessage_PR_start) {
            fprintf(stderr,
                    "Received wrong message type instead of start\n");
            exit(EX_UNAVAILABLE);
        }

        fprintf(stderr, "Received start command from server\n");

        /* Here we should read all the arguments from the start command
         * and override command line arguments if needed but it's
         * not implemented yet. Currently all initial arguments should be
         * specified in command line.
         */
    }

    int print_stats = isatty(1);
    if(print_stats) {
        const char *note = 0;
        if(tcpkali_init_terminal(&note) == -1) {
            warning("Dumb terminal %s, expect unglorified output.\n", note);
            print_stats = 0;
        }
    }

    /* Check that -H,--header is not given without --ws,--websocket */
    if(conf.http_headers.offset > 0 && !engine_params.websocket_enable) {
        fprintf(stderr, "--header option ignored without --websocket\n");
        exit(EX_USAGE);
    }

#ifdef HAVE_OPENSSL
    if(engine_params.ssl_enable) {
            tcpkali_init_ssl();

            /* If server side operation, look into file names */
            if(conf.listen_port != 0) {
                if(access(engine_params.ssl_cert, F_OK) == -1) {
                    fprintf(stderr,
                            "%s: Can not access X.509 certificate file.\n",
                            engine_params.ssl_cert);
                    exit(EX_USAGE);
                }
                if(access(engine_params.ssl_key, F_OK) == -1) {
                    fprintf(stderr, "%s: Can not access SSL private key file\n",
                            engine_params.ssl_key);
                    exit(EX_USAGE);
                }
            }
    }
#endif

    if(rate_modulator.latency_target > 1) {
        char *end = 0;
        strtod(rate_modulator.latency_target_s, &end);
        if(*end == '\0') {
            /* No explicit unit ending, such as "ms" */
            warning(
                "--message-rate @%s: target latency is greater than one "
                "second, which is probably not what you want\n",
                rate_modulator.latency_target_s);
            warning("Suggesting using --message-rate @%gms\n",
                    rate_modulator.latency_target);
        }
    }

    /*
     * Avoid spawning more threads than connections.
     */
    if(engine_params.requested_workers == 0
       && conf.max_connections < number_of_cpus() && conf.listen_port == 0) {
        engine_params.requested_workers = conf.max_connections;
    }
    if(!engine_params.requested_workers)
        engine_params.requested_workers = number_of_cpus();


    /*
     * Check that we'll have a chance to report latency
     */
    if(conf.latency_window) {
        if(conf.latency_window > conf.test_duration) {
            fprintf(stderr, "--statsd-latency-window=%gs exceeds --duration=%gs.\n",
                conf.latency_window, conf.test_duration);
            exit(EX_USAGE);
        }
        if(conf.latency_window >= conf.test_duration / 2) {
            warning("--statsd-latency-window=%gs might result in too few latency reports.\n", conf.latency_window);
        }
        if(conf.latency_window < 0.5) {
            fprintf(stderr, "--statsd-latency-window=%gs is too small. Try 0.5s.\n",
                conf.latency_window);
            exit(EX_USAGE);
        }
    }

    /*
     * Check that the system environment is prepared to handle high load.
     */
    if(adjust_system_limits_for_highload(conf.max_connections,
                                         engine_params.requested_workers)
       == -1) {
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
     * Check whether --rcvbuf and --sndbuf options mean something.
     * Some sysctl variables may make these options ineffectful.
     */
    if(engine_params.sock_rcvbuf_size
       && check_setsockopt_effect(SO_RCVBUF) == 0) {
        /* The check_setsockopt_effect() function yelled already. */
        warning("--rcvbuf option makes no effect.\n");
    }
    if(engine_params.sock_sndbuf_size
       && check_setsockopt_effect(SO_SNDBUF) == 0) {
        /* The check_setsockopt_effect() function yelled already. */
        warning("--sndbuf option makes no effect.\n");
    }

    /*
     * Pick multiple destinations from the command line, resolve them.
     */
    if(argc - optind > 0) {
        engine_params.remote_addresses =
            resolve_remote_addresses(&argv[optind], argc - optind);
        if(engine_params.remote_addresses.n_addrs == 0) {
            errx(EX_NOHOST,
                 "DNS did not return usable addresses for given host(s)");
        } else {
            fprint_addresses(stderr, "Destination: ", "\nDestination: ", "\n",
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
                                 &engine_params.source_addresses)
               < 0) {
                exit(EX_SOFTWARE);
            }
        } else {
            fprint_addresses(stderr, "Source IP: ", "\nSource IP: ", "\n",
                             engine_params.source_addresses);
        }
    } else {
        conf.max_connections = 0;
    }
    if(conf.listen_port > 0) {
        engine_params.listen_addresses =
            detect_listen_addresses(conf.listen_host, conf.listen_port);
    }

    /*
     * Add final touches to the collection:
     * add websocket headers if needed, etc.
     */
    message_collection_finalize(
        &engine_params.message_collection, engine_params.websocket_enable,
        conf.first_hostport, conf.first_path, conf.http_headers.buffer);

    int no_message_to_send =
        (0 == message_collection_estimate_size(
                  &engine_params.message_collection, MSK_PURPOSE_MESSAGE,
                  MSK_PURPOSE_MESSAGE, MCE_MINIMUM_SIZE, WS_SIDE_CLIENT, 0));

    /*
     * Message marker mode can be explicitly enabled via --message-marker,
     * or implicitly via \{message.marker} in the messages to sent.
     */
    engine_params.message_marker |= message_collection_has(&engine_params.message_collection, EXPR_MESSAGE_MARKER);
    if(engine_params.message_marker) {
        engine_params.latency_setting |= SLT_MARKER;
        int res = parse_expression(&engine_params.latency_marker_expr, MESSAGE_MARKER_TOKEN, sizeof(MESSAGE_MARKER_TOKEN) - 1, 0);
        assert(res != -1);
        assert(EXPR_IS_TRIVIAL(engine_params.latency_marker_expr));
    }

    /*
     * Check that we will actually send messages
     * if we are also told to measure latency.
     */
    if(engine_params.latency_marker_expr && !engine_params.message_marker) {
        const char *optname = engine_params.message_marker ?
                            "--message-marker" : "--latency-marker";
        if(no_message_to_send) {
            fprintf(stderr,
                    "%s is given, but no messages "
                    "are supposed to be sent. Specify --message?\n", optname);
            exit(EX_USAGE);
        } else if(argc - optind == 0) {
            fprintf(stderr,
                    "%s is given, but no connections "
                    "are supposed to be initiated. Specify <host:port>?\n",
                    optname);
            exit(EX_USAGE);
        }
    }

    /*
     * If we want to get max rate for specific latency
     * check that we specified the marker for latency measurement
     */
    if(rate_modulator.mode != RM_UNMODULATED
       && !engine_params.latency_marker_expr
       && !engine_params.message_marker) {
        fprintf(stderr,
                "--message-rate @<Latency> requires specifying "
                "--latency-marker or --message-marker as well.\n");
        exit(EX_USAGE);
    }

    /*
     * Make sure the message rate makes sense (e.g. the -m param is there).
     */
    if((engine_params.channel_send_rate.value_base == RS_MESSAGES_PER_SECOND
        || rate_modulator.mode != RM_UNMODULATED)
       && no_message_to_send) {
        if(message_collection_estimate_size(
               &engine_params.message_collection, MSK_PURPOSE_MESSAGE,
               MSK_PURPOSE_MESSAGE, MCE_MAXIMUM_SIZE, WS_SIDE_CLIENT, 1)
           > 0) {
            fprintf(stderr,
                    "--message may resolve "
                    "to zero length, double-check regular expression\n");
        } else {
            fprintf(stderr,
                    "--message-rate parameter makes no sense "
                    "without --message or --message-file\n");
        }
        exit(EX_USAGE);
    }

    /*
     * --write-combine=off makes little sense with Nagle on.
     * Disable Nagle or complain.
     */
    if(engine_params.write_combine == WRCOMB_OFF) {
        switch(engine_params.nagle_setting) {
        case NSET_UNSET:
            fprintf(stderr,
                    "NOTE: --write-combine=off presumes --nagle=off.\n");
            engine_params.nagle_setting = NSET_NODELAY_ON;
            break;
        case NSET_NODELAY_OFF: /* --nagle=on */
            warning(
                "--write-combine=off makes little sense "
                "with --nagle=on.\n");
            break;
        case NSET_NODELAY_ON: /* --nagle=off */
            /* This is the proper setting when --write-combine=off */
            break;
        }
    }

    if(optind == argc && conf.listen_port == 0) {
        fprintf(stderr,
                "Expecting target <host:port> or --listen-port. See -h or "
                "--help.\n");
        usage_short(argv[0]);
        exit(EX_USAGE);
    }

    /*
     * Check if the number of connections can be opened in time.
     */
    if(conf.max_connections / conf.connect_rate > conf.test_duration / 10) {
        if(conf.max_connections / conf.connect_rate > conf.test_duration) {
            fprintf(stderr,
                    "%d connections can not be opened "
                    "at a rate %g within test duration %g.\n"
                    "Decrease --connections=%d, or increase --duration=%g or "
                    "--connect-rate=%g.\n",
                    conf.max_connections, conf.connect_rate, conf.test_duration,
                    conf.max_connections, conf.test_duration,
                    conf.connect_rate);
            exit(EX_USAGE);
        } else {
            warning(
                "%d connections might not be opened "
                "at a rate %g within test duration %g.\n"
                "Decrease --connections=%d, or increase --duration=%g or "
                "--connect-rate=%g.\n",
                conf.max_connections, conf.connect_rate, conf.test_duration,
                conf.max_connections, conf.test_duration, conf.connect_rate);
        }
    }

    /* Which latency types to report to statsd */
    statsd_report_latency_types requested_latency_types = engine_params.latency_setting;

    if(requested_latency_types && !latency_percentiles.size) {
        static struct percentile_value percentile_values[] = {
            { 95, "95" }, { 99, "99" }, { 99.5, "99.5" } };
        static struct percentile_values pvs = {
                .size = sizeof(percentile_values) / sizeof(percentile_values[1]),
                .values = percentile_values
            };
        latency_percentiles = pvs;
    }

    /*
     * Initialize statsd library and push initial (empty) metrics.
     */
    Statsd *statsd;
    if(conf.statsd_enable) {
        statsd_new(&statsd, conf.statsd_host, conf.statsd_port,
                   conf.statsd_namespace, NULL);
        /* Clear up traffic numbers, for better graphing. */
        report_to_statsd(statsd, 0, requested_latency_types, &latency_percentiles);
    } else {
        statsd = 0;
    }

    if(print_stats) {
        /* Stop flashing cursor in the middle of status reporting. */
        tcpkali_disable_cursor();
        /* Enable nonblocking input */
        tcpkali_init_kbdinput();
    }

    /* Block term signals so they're not scheduled in the worker threads. */
    block_term_signals();

    struct engine *eng = engine_start(engine_params);

    /*
     * Traffic in/out moving average, smoothing period is 3 seconds.
     */
    struct oc_args oc_args = {
        .eng = eng,
        .max_connections = conf.max_connections,
        .connect_rate = conf.connect_rate,
        .latency_window = conf.latency_window,
        .statsd = statsd,
        .rate_modulator = &rate_modulator,
        .latency_percentiles = &latency_percentiles,
        .print_stats = print_stats
    };
    mavg_init(&oc_args.traffic_mavgs[0], tk_now(TK_DEFAULT), 1.0 / 8, 3.0);
    mavg_init(&oc_args.traffic_mavgs[1], tk_now(TK_DEFAULT), 1.0 / 8, 3.0);
    mavg_init(&oc_args.count_mavgs[0], tk_now(TK_DEFAULT), 1.0 / 8, 3.0);
    mavg_init(&oc_args.count_mavgs[1], tk_now(TK_DEFAULT), 1.0 / 8, 3.0);

    /*
     * Convert SIGINT into change of a flag.
     * Has to be run after all other threads are run, otherwise
     * a signal can be delivered to a wrong thread.
     */
    flagify_term_signals(&oc_args.term_flag);

    /*
     * Ramp up to the specified number of connections by opening them at a
     * specifed --connect-rate.
     */
    if(conf.max_connections) {
        oc_args.epoch_end = tk_now(TK_DEFAULT) + conf.test_duration;
        if(open_connections_until_maxed_out(PHASE_ESTABLISHING_CONNECTIONS,
                &oc_args, &orch_state) == OC_CONNECTED) {
            fprintf(stderr, "%s", tcpkali_clear_eol());
            fprintf(stderr, "Ramped up to %d connections.\n",
                    conf.max_connections);
        } else {
            fprintf(stderr, "%s", tcpkali_clear_eol());
            fprintf(stderr,
                    "Could not create %d connection%s"
                    " in allotted time (%gs)\n",
                    conf.max_connections, conf.max_connections == 1 ? "" : "s",
                    conf.test_duration);
            /* Level down graphs/charts. */
            report_to_statsd(statsd, 0, requested_latency_types, &latency_percentiles);
            exit(1);
        }
    }

    /*
     * Start measuring the steady-state performance, as opposed to
     * ramping up and waiting for the connections to be established.
     * (initial_traffic_stats) contain traffic numbers accumulated duing
     * ramp-up time.
     */
    oc_args.checkpoint.initial_traffic_stats = engine_traffic(eng);
    oc_args.checkpoint.epoch_start = tk_now(TK_DEFAULT);

    /* Reset the test duration after ramp-up. */
    oc_args.epoch_end = tk_now(TK_DEFAULT) + conf.test_duration;
    enum oc_return_value orv = open_connections_until_maxed_out(
                                    PHASE_STEADY_STATE, &oc_args, &orch_state);

    fprintf(stderr, "%s", tcpkali_clear_eol());
    engine_terminate(eng, oc_args.checkpoint.epoch_start,
                     oc_args.checkpoint.initial_traffic_stats, &latency_percentiles);

    /* Send zeroes, otherwise graphs would continue showing non-zeroes... */
    report_to_statsd(statsd, 0, requested_latency_types, &latency_percentiles);

    switch(orv) {
    case OC_CONNECTED:
        assert(orv != OC_CONNECTED);
    /* Fall through */
    case OC_INTERRUPT:
        exit(EX_USAGE);
        break;
    case OC_TIMEOUT:
        if(rate_modulator.mode != RM_UNMODULATED) {
            fprintf(stderr,
                    "Failed to find the best --message-rate for latency %s in "
                    "-T%g seconds\n",
                    rate_modulator.latency_target_s, conf.test_duration);
            exit(EX_UNAVAILABLE);
        }
        break;
    case OC_RATE_GOAL_MET:
        printf("Best --message-rate for latency %s⁹⁵ᵖ is %g\n",
               rate_modulator.latency_target_s,
               rate_modulator.suggested_rate_value);
        break;
    case OC_RATE_GOAL_FAILED:
        fprintf(stderr,
                "Best --message-rate for latency %s can not be determined in "
                "time due to unstable target system behavior\n",
                rate_modulator.latency_target_s);
        exit(EX_UNAVAILABLE);
        break;
    }

    return 0;
}

static double
parse_with_multipliers(const char *option, char *str, struct multiplier *ms,
                       int n) {
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
parse_percentile_values(const char *option, char *str,
                       struct percentile_values *array) {
    struct percentile_value *values = array->values;
    size_t size = array->size;

    for(char *pos = str; *pos; pos++) {
        const char *start = pos;
        char *endpos;

        double value_d = strtod(start, &endpos);

        if(start == endpos) {
            fprintf(stderr, "--%s: %s: Failed to parse: bad number\n", option, str);
            return -1;
        }
        if(value_d < 0 || !isfinite(value_d) || value_d > 100) {
            fprintf(stderr, "--%s: %s: Failed to parse: bad latency percentile specification"
                            ", expected range is [0..100]\n", option, str);
            return -1;
        }
        for(; pos < endpos; pos++) {
            switch(*pos) {
            case '+': case ' ':
                assert(pos == start);
                start++;
                continue;
            case '.':
                if(endpos - pos == 1) {
                    fprintf(stderr, "--%s: %s: Fractional part is missing\n", option, str);
                    return -1;
                }
                continue;
            case '0' ... '9':
                continue;
            default:
                fprintf(stderr, "--%s: %s: Number should contain only digits\n", option, str);
                return -1;
            }
            break;
        }
        if(endpos - start >= (ssize_t)sizeof(values[0].value_s)) {
            fprintf(stderr, "--%s: %s: Unreasonably precise percentile specification\n", option, str);
            return -1;
        }

        if(*endpos != 0 && *endpos != ',' && *endpos != '/') {
            fprintf(stderr,
                    "--%s: %s: Failed to parse: "
                    "invalid separator, use ',' or '/'\n",
                    option, str);
            return -1;
        }

        values = realloc(values, ++size * sizeof(values[0]));
        assert(values);
        values[size - 1].value_d = value_d;
        memcpy(values[size - 1].value_s, start, endpos-start);
        values[size - 1].value_s[endpos - start] = '\0';

        pos = endpos;
        if(*pos == 0) break;
    }

    array->values = values;
    array->size = size;
    return 0;
}

/*
 * Display the Usage screen.
 */
static void
usage_long(char *argv0, struct tcpkali_config *conf) {
    fprintf(stdout, "Usage: %s [OPTIONS] [-l <port>] [<host:port>...]\n",
            basename(argv0));
    /* clang-format off */
    fprintf(stdout,
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
    "  --sndbuf <SizeBytes>         Set TCP send buffers (set SO_SNDBUF)\n"
    "  --source-ip <IP>             Use the specified IP address to connect\n"
    "  --write-combine off          Disable batching adjacent writes\n"
    "  -w, --workers <N=%ld>%s         Number of parallel threads to use\n"
    "\n"
    "  --ws, --websocket            Use RFC6455 WebSocket transport\n"
    "  --ssl                        Enable TLS\n"
    "  --ssl-cert <filename>        X.509 certificate file (default: cert.pem)\n"
    "  --ssl-key <filename>         Private key file (default: key.pem)\n"
    "  -H, --header <string>        Add HTTP header into WebSocket handshake\n"
    "  -c, --connections <N=%d>      Connections to keep open to the destinations\n"
    "  --connect-rate <Rate=%g>     Limit number of new connections per second\n"
    "  --connect-timeout <Time=1s>  Limit time spent in a connection attempt\n"
    "  --channel-lifetime <Time>    Shut down each connection after Time seconds\n"
    "  --channel-bandwidth-upstream <Bandwidth>     Limit upstream bandwidth\n"
    "  --channel-bandwidth-downstream <Bandwidth>   Limit downstream bandwidth\n"
    "  -l, --listen-port <port>     Listen on the specified port\n"
    "  --listen-mode=<mode>         What to do upon client connect, where <mode> is:\n"
    "               \"silent\"        Do not send data, ignore received data (default)\n"
    "               \"active\"        Actively send messages\n"
    "  -T, --duration <Time=10s>    Exit after the specified amount of time\n"
    "  --delay-send <Time>          Delay sending data by a specified amount of time\n"
    "\n"
    "  -e, --unescape-message-args  Unescape the message data arguments\n"
    "  -1, --first-message <string> Send this message first, once\n"
    "  --first-message-file <name>  Read the first message from a file\n"
    "  -m, --message <string>       Message to repeatedly send to the remote\n"
    "  -f, --message-file <name>    Read message to send from a file\n"
    "  -r, --message-rate <Rate>    Messages per second to send in a connection\n"
    "  -r, --message-rate @<Latency> Measure a message rate at a given latency\n"
    "  --message-stop <string>      Abort if this string is found in received data\n"
    "\n"
    "  --latency-connect            Measure TCP connection establishment latency\n"
    "  --latency-first-byte         Measure time to first byte latency\n"
    "  --latency-marker <string>    Measure latency using a per-message marker\n"
    "  --latency-marker-skip <N>    Ignore the first N occurrences of a marker\n"
    "  --latency-percentiles <list> Report latency at specified percentiles\n"
    "  --message-marker             Parse markers to calculate latency\n"
    "\n"
    "  --statsd                     Enable StatsD output (default %s)\n"
    "  --statsd-host <host>         StatsD host to send data (default is localhost)\n"
    "  --statsd-port <port>         StatsD port to use (default is %d)\n"
    "  --statsd-namespace <string>  Metric namespace (default is \"%s\")\n"
    "  --statsd-latency-window <T>  Aggregate latencies in discrete windows\n"
    "\n"
    "  --server <host:port>         Orchestration server to connect to\n"
    "\n"
    "Variable units and recognized multipliers:\n"
    "  <N>, <Rate>:  k (1000, as in \"5k\" is 5000), m (1000000)\n"
    "  <SizeBytes>:  k (1024, as in \"5k\" is 5120), m (1024*1024)\n"
    "  <Bandwidth>:  kbps, Mbps (bits per second), kBps, MBps (bytes per second)\n"
    "  <Time>, <Latency>:  ms, s, m, h, d (milliseconds, seconds, minutes, etc)\n"
    "  <Rate>, <Time> and <Latency> can be fractional values, such as 0.25.\n",
        /* clang-format on */

        (_DBG_MAX - 1), number_of_cpus(), number_of_cpus() < 10 ? " " : "",
        conf->max_connections, conf->connect_rate,
        conf->statsd_enable ? "enabled" : "disabled", conf->statsd_port,
        conf->statsd_namespace);
}

static void
usage_short(char *argv0) {
    if(isatty(fileno(stdout))) {
        tcpkali_init_terminal(NULL);
    }

    fprintf(stdout, "Usage: %s [OPTIONS] [-l <port>] [<host:port>...]\n",
            basename(argv0));
    fprintf(stdout,
        /* clang-format off */
    "Where some OPTIONS are:\n"
    "  -h                   Print this help screen, then exit\n"
    "  %s--help%s               Print long help screen, then exit\n"
    "  -d                   Dump i/o data for a single connection\n"
    "\n"
    "  -c <N>               Connections to keep open to the destinations\n"
    "  -l <port>            Listen on the specified port\n"
    "  --ws, --websocket    Use RFC6455 WebSocket transport\n"
    "  -T <Time=10s>        Exit after the specified amount of time\n"
    "\n"
    "  -e                   Unescape backslash-escaping in a message string\n"
    "  -1 <string>          Message to send to the remote host once\n"
    "  -m <string>          Message to repeatedly send to the remote\n"
    "  -r <Rate>            Messages per second to send in a connection\n"
    "\n"
    "Variable units and recognized multipliers:\n"
    "  <N>, <Rate>:  k (1000, as in \"5k\" is 5000), m (1000000)\n"
    "  <Time>:       ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)\n"
    "  <Rate> and <Time> can be fractional values, such as 0.25.\n"
    "\n"
    "Use `%s %s--help%s` or `man tcpkali` for a %sfull set%s of supported options.\n",
    tk_attr(TKA_HIGHLIGHT),
    tk_attr(TKA_NORMAL),
    basename(argv0),
    tk_attr(TKA_HIGHLIGHT),
    tk_attr(TKA_NORMAL),
    tk_attr(TKA_HIGHLIGHT),
    tk_attr(TKA_NORMAL)
    );
    /* clang-format on */
}
