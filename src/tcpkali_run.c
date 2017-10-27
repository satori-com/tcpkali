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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

#include "tcpkali_run.h"
#include "tcpkali_mavg.h"
#include "tcpkali_events.h"
#include "tcpkali_engine.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_terminfo.h"

static const char *
time_progress(double start, double now, double stop) {
    const char *clocks[] = {"🕛  ", "🕐  ", "🕑  ", "🕒  ", "🕓  ", "🕔  ",
                            "🕕  ", "🕖  ", "🕗  ", "🕘  ", "🕙  ", "🕚  "};
    if(!tcpkali_is_utf8()) return "";
    double span = (stop - start) / (sizeof(clocks) / sizeof(clocks[0]));
    int pos = (now - start) / span;
    if(pos < 0)
        pos = 0;
    else if(pos > 11)
        pos = 11;
    return clocks[pos];
}

static void
print_connections_line(int conns, int max_conns, int conns_counter) {
    int terminal_width = tcpkali_terminal_width();

    char info[terminal_width + 1];
    ssize_t info_width = snprintf(info, sizeof(info), "| %d of %d (%d)", conns,
                                  max_conns, conns_counter);

    int ribbon_width = terminal_width - info_width - 1;
    if(ribbon_width > 0.6 * terminal_width) ribbon_width = 0.6 * terminal_width;
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
        if(i < at)
            ribbon[i] = '=';
        else if(i > at)
            ribbon[i] = '-';
        else if(i == at)
            ribbon[i] = '>';
    }
    ribbon[ribbon_width] = 0;
    fprintf(stderr, "%s%s%s\r", ribbon, info, tcpkali_clear_eol());
}

static int
format_latency(char *buf, size_t size, const char *prefix,
               struct hdr_histogram *hist) {
    if(hist) {
        if(hist->total_count) {
            return snprintf(buf, size, "%s%.1f ", prefix,
                            hdr_value_at_percentile(hist, 95.0) / 10.0);
        } else {
            return snprintf(buf, size, "%s? ", prefix);
        }
    } else {
        return 0;
    }
}

static void
format_latencies(char *buf, size_t size, struct latency_snapshot *latency) {
    if(latency->connect_histogram || latency->firstbyte_histogram
       || latency->marker_histogram) {
        char *p = buf;
        p += snprintf(p, size, " (");
        p += format_latency(p, size-(p-buf),
                            "c=", latency->connect_histogram);
        p += format_latency(p, size-(p-buf),
                            "fb=", latency->firstbyte_histogram);
        p += format_latency(p, size-(p-buf),
                            "m=", latency->marker_histogram);
        snprintf(p, size - (p - buf), "ms⁹⁵ᵖ)");
    } else {
        buf[0] = '\0';
    }
}

static void
format_message_rate(char *buf, size_t size, const struct oc_args *args, double now) {
    if(engine_params(args->eng)->message_marker) {
        double count_rcvd = mavg_per_second(&args->count_mavgs[0], now);
        double count_sent = mavg_per_second(&args->count_mavgs[1], now);

        snprintf(buf, size, " (%.0f↓ %.0f↑ mps)",
            round(count_rcvd), round(count_sent));
    } else {
        buf[0] = '\0';
    }
}

/*
 * Return non-zero value every time we're more than (delta_time)
 * away from the checkpoint time; updates the checkpoint time.
 */
static int
every(double delta_time, double now, double *checkpoint_time) {
    if((now - *checkpoint_time) < delta_time) {
        return 0;
    } else {
        *checkpoint_time = now;
        return 1;
    }
}

static enum {
    MRR_ONGOING,
    MRR_RATE_SEARCH_SUCCEEDED,
    MRR_RATE_SEARCH_FAILED
} modulate_request_rate(struct engine *eng, double now,
                        struct rate_modulator *rm,
                        struct latency_snapshot *latency) {
    if(rm->mode == RM_UNMODULATED || !latency->marker_histogram)
        return MRR_ONGOING;
    /*
     * Do not measure and modulate latency estimates more frequently than
     * necessary.
     */
    const double short_time = 1.0; /* Every second */
    const double long_time = 10.0;
    if(rm->state == RMS_STATE_INITIAL) {
        const struct engine_params *params = engine_params(eng);
        if(params->channel_send_rate.value_base == RS_MESSAGES_PER_SECOND)
            rm->suggested_rate_value = params->channel_send_rate.value;
        else
            rm->suggested_rate_value = 100;
        rm->binary_search_steps = 15;
        rm->last_update_short = now;
        rm->last_update_long = now;
        rm->state = RMS_RATE_RAMP_UP;
    }

    if(!every(short_time, now, &rm->last_update_short)) return MRR_ONGOING;

    double lat = hdr_value_at_percentile(latency->marker_histogram, 95.0) / 10.0
                 / 1000.0;
    if(every(long_time, now, &rm->last_update_long)) {
        /*
         * Every long time (to make moving averages stabilize a bit)
         * we do assessment and ramp up furtheri or adjust lower and
         * upper bounds if we're doing a binary search for the best rate.
         */
        rm->prev_latency = lat;
        rm->prev_max_latency_exceeded = 0;

        if(rm->state == RMS_RATE_RAMP_UP) {
            if(lat < 1.5 * rm->latency_target) {
                if(lat < 0.95 * rm->latency_target) {
                    rm->rate_min_bound = rm->suggested_rate_value;
                    rm->rate_max_bound = INFINITY;
                }
                rm->suggested_rate_value *= 4;
            } else {
                rm->state = RMS_RATE_BINARY_SEARCH;
                rm->rate_max_bound = rm->suggested_rate_value;
            }
        }
        if(rm->state == RMS_RATE_BINARY_SEARCH) {
            if(rm->binary_search_steps-- <= 0) return MRR_RATE_SEARCH_FAILED;

            if(lat < 0.98 * rm->latency_target) {
                rm->rate_min_bound = rm->suggested_rate_value;
            } else if(lat > 1.01 * rm->latency_target) {
                rm->rate_max_bound = rm->suggested_rate_value;
            } else {
                return MRR_RATE_SEARCH_SUCCEEDED;
            }
            /* If bounds are within 1% of each other, means we've failed */
            if(rm->rate_max_bound > 0
               && 0.001 > ((rm->rate_max_bound - rm->rate_min_bound) / 2)
                              / rm->rate_min_bound) {
                if(lat <= rm->latency_target) return MRR_RATE_SEARCH_SUCCEEDED;
                return MRR_RATE_SEARCH_FAILED;
            }
            rm->suggested_rate_value =
                (rm->rate_max_bound + rm->rate_min_bound) / 2.0;
        }
        engine_set_message_send_rate(eng, rm->suggested_rate_value);
        fprintf(stderr, "Attempting --message-rate %g (in range %g..%g)%s\n",
                rm->suggested_rate_value, rm->rate_min_bound,
                rm->rate_max_bound, tcpkali_clear_eol());
    } else if(rm->state == RMS_RATE_RAMP_UP) {
        if(lat > 2 * rm->latency_target) {
            if(lat > rm->prev_latency) {
                rm->prev_max_latency_exceeded++;
            } else {
                /* If we're fluctuating, disable fast exit from RAMP_UP */
                rm->prev_max_latency_exceeded = -1000;
            }
        }
        rm->prev_latency = lat;
        /*
         * If for the last few seconds we've been consistently increasing
         * latency, quickly exit the ramp-up state.
         */
        if(rm->prev_max_latency_exceeded > 3) {
            rm->state = RMS_RATE_BINARY_SEARCH;
            rm->rate_max_bound = rm->suggested_rate_value;
        }
    }

    return MRR_ONGOING;
}

/* 1.148698 ^ 5 == 2, so 5 key-ups give increase by factor of 2 */
#define UP_FACTOR 1.148698
/* 0.870551 ^ 5 == 0.5, so 5 key-downs give decrease by factor of 2 */
#define DOWN_FACTOR 0.870551

int
process_keyboard_events(struct oc_args *args) {
    switch(tcpkali_kbhit()) {
    case KE_UP_ARROW:
        engine_update_send_rate(args->eng, UP_FACTOR);
        break;
    case KE_DOWN_ARROW:
        engine_update_send_rate(args->eng, DOWN_FACTOR);
        break;
    case KE_Q:
        return 0;
    case KE_ENTER:
        printf("\n");
        break;
    case KE_NOTHING:
        break;
    }
    return 1;
}

enum oc_return_value
open_connections_until_maxed_out(enum work_phase phase, struct oc_args *args) {
    tk_now_update(TK_DEFAULT);
    double now = tk_now(TK_DEFAULT);

    /*
     * It is a little bit better to batch the starts by issuing several
     * start commands per small time tick. Ends up doing less write()
     * operations per batch.
     * Therefore, we round the timeout_ms upwards to the nearest millisecond.
     */
    long timeout_ms = ceil(1000.0 / args->connect_rate);
    if(timeout_ms > 250) timeout_ms = 250;

    struct pacefier keepup_pace;
    pacefier_init(&keepup_pace, args->connect_rate, now);

    ssize_t conn_deficit = 1; /* Assume connections still have to be est. */

    statsd_report_latency_types requested_latency_types =
        engine_params(args->eng)->latency_setting;
    if(requested_latency_types && args->latency_window
       && !args->previous_window_latency) {
        engine_prepare_latency_snapshot(args->eng);
        args->previous_window_latency =
            engine_collect_latency_snapshot(args->eng);
    }

#define STDIN_IDX 0
    struct pollfd poll_fds[1] =
        {{.fd = tcpkali_input_initialized() ? 0 : -1,
          .events = POLLIN}};

    while(now < args->epoch_end && !args->term_flag
          /* ...until we have all connections established or
           * we're in a steady state. */
          && (phase == PHASE_STEADY_STATE || conn_deficit > 0)) {

        switch(poll(poll_fds, 1, timeout_ms)) {
        case 0: /* timeout */
            break;
        case -1: /* error */
            /* EINTR happens when we press Ctrl-C */
            if (errno != EINTR)
                fprintf(stderr, "Poll error %s\n", strerror(errno));
            break;
        default: /* got input in one of the fds */
            /* got something in stdin */
            if(poll_fds[STDIN_IDX].revents & POLLIN) {
                if(!process_keyboard_events(args)) {
                    return OC_INTERRUPT;
                }
            }
        }

        tk_now_update(TK_DEFAULT);
        now = tk_now(TK_DEFAULT);


        size_t connecting, conns_in, conns_out, conns_counter;
        engine_get_connection_stats(args->eng, &connecting, &conns_in, &conns_out,
                                    &conns_counter);
        conn_deficit = args->max_connections - (connecting + conns_out);

        size_t allowed = pacefier_allow(&keepup_pace, now);
        size_t to_start = allowed;
        if(conn_deficit <= 0) {
            to_start = 0;
        }
        if(to_start > (size_t)conn_deficit) {
            to_start = conn_deficit;
        }
        args->connections_opened_tally += engine_initiate_new_connections(args->eng, to_start);
        pacefier_moved(&keepup_pace, allowed, now);

        /* Do not update/print checkpoint stats too often. */
        if(!every(0.25, now, &args->checkpoint.last_update)) continue;

        /*
         * traffic_delta.* contains traffic observed within the last
         * period (now - checkpoint->last_stats_sent).
         */
        non_atomic_traffic_stats _last = args->checkpoint.last_traffic_stats;
        args->checkpoint.last_traffic_stats = engine_traffic(args->eng);
        non_atomic_traffic_stats traffic_delta =
            subtract_traffic_stats(args->checkpoint.last_traffic_stats, _last);

        mavg_add(&args->traffic_mavgs[0], now,
                 (double)traffic_delta.bytes_rcvd);
        mavg_add(&args->traffic_mavgs[1], now,
                 (double)traffic_delta.bytes_sent);
        mavg_add(&args->count_mavgs[0], now,
                (double)traffic_delta.msgs_rcvd);
        mavg_add(&args->count_mavgs[1], now,
                (double)traffic_delta.msgs_sent);

        double bps_in = 8 * mavg_per_second(&args->traffic_mavgs[0], now);
        double bps_out = 8 * mavg_per_second(&args->traffic_mavgs[1], now);

        engine_prepare_latency_snapshot(args->eng);
        struct latency_snapshot *latency = engine_collect_latency_snapshot(args->eng);

        statsd_feedback feedback = {.opened = args->connections_opened_tally,
                                    .conns_in = conns_in,
                                    .conns_out = conns_out,
                                    .bps_in = bps_in,
                                    .bps_out = bps_out,
                                    .traffic_delta = traffic_delta,
                                    .latency = NULL};
        args->connections_opened_tally = 0;

        if(requested_latency_types && args->latency_window) {
            /*
             * Latencies are prepared separately, on a per-window basis,
             * and sent separately.
             */
            report_to_statsd(args->statsd, &feedback, 0, 0);

            if(every(args->latency_window, now,
                     &args->checkpoint.last_latency_window_flush)) {
                struct latency_snapshot *diff =
                    engine_diff_latency_snapshot(args->previous_window_latency, latency);
                engine_free_latency_snapshot(args->previous_window_latency);
                args->previous_window_latency = latency;

                report_latency_to_statsd(args->statsd, diff,
                    requested_latency_types, args->latency_percentiles);

                engine_free_latency_snapshot(diff);
            }
        } else {
            /* Latencies are sent in-line with byte stats, few times a second */
            feedback.latency = latency;
            report_to_statsd(args->statsd, &feedback, requested_latency_types,
                             args->latency_percentiles);
        }

        if(args->print_stats) {
            if(phase == PHASE_ESTABLISHING_CONNECTIONS) {
                print_connections_line(conns_out, args->max_connections,
                                       conns_counter);
            } else {
                char latency_buf[256];
                format_latencies(latency_buf, sizeof(latency_buf), latency);
                char mps_buf[256];
                format_message_rate(mps_buf, sizeof(mps_buf), args, now);

                fprintf(stderr,
                        "%sTraffic %.3f↓, %.3f↑ Mbps "
                        "(%s%ld↓ %ld↑ %ld⇡; %s%ld)%s%s%s\r",
                        time_progress(args->checkpoint.epoch_start, now, args->epoch_end),
                        bps_in / 1000000.0, bps_out / 1000000.0,
                        requested_latency_types ? "" : "conns ",
                        (long)conns_in,
                        (long)conns_out, (long)connecting,
                        requested_latency_types ? "" : "seen ",
                        (long)conns_counter,
                        mps_buf, latency_buf, tcpkali_clear_eol());
            }
        }

        /* Change the request rate according to the modulation rules. */
        if(phase == PHASE_STEADY_STATE) {
            switch(modulate_request_rate(args->eng, now, args->rate_modulator, latency)) {
            case MRR_ONGOING:
                break;
            case MRR_RATE_SEARCH_SUCCEEDED:
                return OC_RATE_GOAL_MET;
            case MRR_RATE_SEARCH_FAILED:
                return OC_RATE_GOAL_FAILED;
            }
        }

        if(latency != args->previous_window_latency)
            engine_free_latency_snapshot(latency);
    }

    if(now >= args->epoch_end) return OC_TIMEOUT;
    if(args->term_flag) return OC_INTERRUPT;
    return OC_CONNECTED;
}

struct orchestration_data
connect_to_orchestration_server(struct orchestration_args args) {
    struct orchestration_data res = {.connected = 0};
    int sockfd;
    for(struct addrinfo *p = args.server_addrs; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        res.socket = sockfd;
        res.connected = 1;
        fprintf(stderr, "Connected to orchestration server at %s\n",
                args.server_addr_str);
        return res;
    }
    return res;
}
