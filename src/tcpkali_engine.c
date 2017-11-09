/*
 * Copyright (c) 2014, 2015, 2016, 2017  Machine Zone, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <unistd.h>
#include <stddef.h> /* offsetof(3) */
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <sys/queue.h>
#include <sysexits.h>
#include <math.h>
#include <sys/time.h>

#include <config.h>

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#include <StreamBoyerMooreHorspool.h>
#include <hdr_histogram.h>
#include <pcg_basic.h>

#include "tcpkali.h"
#include "tcpkali_ring.h"
#include "tcpkali_atomic.h"
#include "tcpkali_events.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_websocket.h"
#include "tcpkali_terminfo.h"
#include "tcpkali_logging.h"
#include "tcpkali_expr.h"
#include "tcpkali_data.h"
#include "tcpkali_traffic_stats.h"
#include "tcpkali_connection.h"
#include "tcpkali_ssl.h"

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
    for((var) = TAILQ_FIRST((head));               \
        (var) && ((tvar) = TAILQ_NEXT((var), field), 1); (var) = (tvar))
#endif

struct loop_arguments {
    /**************************
     * NON-SHARED WORKER DATA *
     **************************/
    struct engine_params params; /* A copy of engine parameters */
    unsigned int
        address_offset; /* An offset into the params.remote_addresses[] */

    tk_timer stats_timer;
    tk_timer channel_lifetime_timer;
    int global_control_pipe_rd_nbio; /* Non-blocking pipe anyone could read
                                        from. */
    int global_feedback_pipe_wr;     /* Blocking pipe for progress reporting. */
    int private_control_pipe_rd; /* Private blocking pipe for this worker (read
                                    side). */
    int private_control_pipe_wr; /* Private blocking pipe for this worker (write
                                    side). */
    int thread_no;
    int dump_connect_fd; /* Which connection to dump */

    TAILQ_HEAD(, connection) open_conns; /* Thread-local connections */
    unsigned long worker_connections_initiated;
    unsigned long worker_connections_accepted;
    unsigned long worker_connection_failures;
    unsigned long worker_connection_timeouts;
    struct hdr_histogram *connect_histogram_local;   /* --latency-connect */
    struct hdr_histogram *firstbyte_histogram_local; /* --latency-first-byte */
    struct hdr_histogram *marker_histogram_local;    /* --latency-marker */

    /* Per-worker scratch buffer allows debugging the last received data */
    char scratch_recv_buf[16384];
    size_t scratch_recv_last_size;

    pcg32_random_t rng;

    /*******************************************
     * WORKER DATA SHARED WITH OTHER PROCESSES *
     *******************************************/

    const struct engine_params *shared_eng_params;

    /*
     * Connection identifier counter is shared between all connections
     * across all workers. We don't allocate it per worker, so it points
     * to the same memory in the parameters of all workers.
     */
    atomic_narrow_t *connection_unique_id_atomic;

    /*
     * Reporting histograms should not be touched
     * unless asked through a private control pipe.
     */
    struct hdr_histogram *connect_histogram_shared;
    struct hdr_histogram *firstbyte_histogram_shared;
    struct hdr_histogram *marker_histogram_shared;
    pthread_mutex_t shared_histograms_lock;

    /*
     * Per-remote server stats, pointing to a global table.
     */
    struct remote_stats {
        atomic_narrow_t connection_attempts;
        atomic_narrow_t connection_failures;
    } * remote_stats;

    /* The following atomic members are accessed outside of worker thread */
    atomic_traffic_stats worker_traffic_stats;
    atomic_narrow_t outgoing_connecting;
    atomic_narrow_t outgoing_established;
    atomic_narrow_t incoming_established;
    atomic_narrow_t connections_counter;

    /* Avoid mixing output from several threads when dumping complex state */
    pthread_mutex_t *serialize_output_lock;
};

/*
 * Types of control messages which might require fair ordering between channels.
 */
enum control_message_type_e {
    CONTROL_MESSAGE_CONNECT,
    _CONTROL_MESSAGES_MAXID /* Do not use. */
};

/*
 * Engine abstracts over workers.
 */
struct engine {
    struct engine_params params; /* A copy of engine parameters */
    struct loop_arguments *loops;
    pthread_t *threads;
    int global_control_pipe_wr;
    int global_feedback_pipe_rd;
    int next_worker_order[_CONTROL_MESSAGES_MAXID];
    int n_workers;
    non_atomic_traffic_stats total_traffic_stats;
    atomic_narrow_t connection_unique_id_global;
    pthread_mutex_t serialize_output_lock;
};

const struct engine_params *
engine_params(struct engine *eng) {
    return &eng->params;
}

/*
 * Helper functions defined at the end of the file.
 */
enum connection_close_reason {
    CCR_CLEAN,    /* No failure */
    CCR_LIFETIME, /* Channel lifetime limit (no failure) */
    CCR_TIMEOUT,  /* Connection timeout */
    CCR_REMOTE,   /* Remote side closed connection */
    CCR_DATA,     /* Data framing error */
};
static void *single_engine_loop_thread(void *argp);
static void start_new_connection(TK_P);
static void close_connection(TK_P_ struct connection *conn,
                             enum connection_close_reason reason);
static void connections_flush_stats(TK_P);
static void connection_flush_stats(TK_P_ struct connection *conn);
static void close_all_connections(TK_P_ enum connection_close_reason reason);
static void connection_cb(TK_P_ tk_io *w, int revents);
static void passive_websocket_cb(TK_P_ tk_io *w, int revents);
static void control_cb(TK_P_ tk_io *w, int revents);
static void accept_cb(TK_P_ tk_io *w, int revents);
static void stats_timer_cb(TK_P_ tk_timer UNUSED *w, int UNUSED revents);
static void conn_timer_cb(TK_P_ tk_timer *w, int revents); /* Timeout timer */
static void expire_channel_lives(TK_P_ tk_timer *w, int revents);
static void setup_channel_lifetime_timer(TK_P_ double first_timeout);
static void update_io_interest(TK_P_ struct connection *conn);
static struct sockaddr_storage *pick_remote_address(
    struct loop_arguments *largs, size_t *remote_index);
static char *express_bytes(size_t bytes, char *buf, size_t size);
static int limit_channel_lifetime(struct loop_arguments *largs);
static void set_nbio(int fd, int onoff);
static void set_socket_options(int fd, struct loop_arguments *largs);
static void common_connection_init(TK_P_ struct connection *conn,
                                   enum conn_type conn_type,
                                   enum conn_state conn_state, int sockfd);
static void largest_contiguous_chunk(struct loop_arguments *largs,
                                     struct connection *conn,
                                     const void **position,
                                     size_t *available_header,
                                     size_t *available_body);
static void debug_dump_data(const char *prefix, int fd, const void *data,
                            size_t size, ssize_t limit);
static void debug_dump_data_highlight(const char *prefix, int fd,
                                      const void *data, size_t size,
                                      ssize_t limit, size_t hl_offset,
                                      size_t hl_length);
static void
latency_record_incoming_ts(TK_P_ struct connection *conn, char *buf,
                           size_t size);

#ifdef USE_LIBUV
static void
expire_channel_lives_uv(tk_timer *w) {
    expire_channel_lives(w->loop, w, 0);
}
static void
stats_timer_cb_uv(tk_timer *w) {
    stats_timer_cb(w->loop, w, 0);
}
static void
conn_timer_cb_uv(tk_timer *w) {
    conn_timer_cb(w->loop, w, 0);
}
static void
passive_websocket_cb_uv(tk_io *w, int UNUSED status, int revents) {
    passive_websocket_cb(w->loop, w, revents);
}
static void
connection_cb_uv(tk_io *w, int UNUSED status, int revents) {
    connection_cb(w->loop, w, revents);
}
static void
accept_cb_uv(tk_io *w, int UNUSED status, int revents) {
    accept_cb(w->loop, w, revents);
}
static void
control_cb_uv(tk_io *w, int UNUSED status, int revents) {
    control_cb(w->loop, w, revents);
}
#endif

#define DEBUG(level, fmt, args...)                                        \
    do {                                                                  \
        if((int)largs->params.verbosity_level >= level)                   \
            debug_log(level, largs->params.verbosity_level, fmt, ##args); \
    } while(0)

struct engine *
engine_start(struct engine_params params) {
    int fildes[2];

    /* Global control pipe. Engine -> workers. */
    int rc = pipe(fildes);
    assert(rc == 0);
    int gctl_pipe_rd = fildes[0];
    int gctl_pipe_wr = fildes[1];
    set_nbio(gctl_pipe_rd, 1);

    /* Global feedback pipe. Engine <- workers. */
    rc = pipe(fildes);
    assert(rc == 0);
    int gfbk_pipe_rd = fildes[0];
    int gfbk_pipe_wr = fildes[1];

    /* Figure out number of asynchronous workers to start. */
    int n_workers = params.requested_workers;
    if(!n_workers) {
        long n_cpus = number_of_cpus();
        fprintf(stderr, "Using %ld available CPUs\n", n_cpus);
        assert(n_cpus >= 1);
        n_workers = n_cpus;
    }

    /*
     * The data template creation may fail because the message collection
     * might contain expressions which must be resolved
     * on a per connection or per message basis.
     */
    enum transport_websocket_side tws_side;
    for(tws_side = TWS_SIDE_CLIENT; tws_side <= TWS_SIDE_SERVER; tws_side++) {
        assert(params.data_templates[tws_side] == NULL);
        pcg32_random_t rng;
        pcg32_srandom_r(&rng, random(), tws_side);
        params.data_templates[tws_side] =
            transport_spec_from_message_collection(
                0, &params.message_collection, 0, 0, tws_side,
                TS_CONVERSION_INITIAL, &rng);
        assert(params.data_templates[tws_side]
               || params.message_collection.most_dynamic_expression
                      != DS_GLOBAL_FIXED);
    }

    if(params.data_templates[0])
        replicate_payload(params.data_templates[0], REPLICATE_MAX_SIZE);
    if(params.data_templates[1])
        replicate_payload(params.data_templates[1], REPLICATE_MAX_SIZE);

    struct engine *eng = calloc(1, sizeof(*eng));
    eng->params = params;
    eng->loops = calloc(n_workers, sizeof(eng->loops[0]));
    eng->threads = calloc(n_workers, sizeof(eng->threads[0]));
    eng->n_workers = n_workers;
    eng->global_control_pipe_wr = gctl_pipe_wr;
    eng->global_feedback_pipe_rd = gfbk_pipe_rd;
    if(pthread_mutex_init(&eng->serialize_output_lock, 0) != 0) {
        /* At this stage in the program, no point to continue. */
        assert(!"Should really be unreachable");
        return NULL;
    }

    /*
     * Initialize the Boyer-Moore-Horspool occurrences table once,
     * if it can be shared between connections. It can only be shared
     * if it is trivial (does not depend on dynamic \{expressions}).
     */
    if(params.latency_marker_expr   /* --latency-marker, --message-marker */
       && EXPR_IS_TRIVIAL(params.latency_marker_expr)) {
        sbmh_init(NULL, &params.sbmh_shared_marker_occ,
                  (void *)params.latency_marker_expr->u.data.data,
                  params.latency_marker_expr->u.data.size);
    }
    if(params.message_stop_expr    /* --message-stop */
       && EXPR_IS_TRIVIAL(params.message_stop_expr)) {
        sbmh_init(NULL, &params.sbmh_shared_stop_occ,
                  (void *)params.message_stop_expr->u.data.data,
                  params.message_stop_expr->u.data.size);
    }

    params.epoch = tk_now(TK_DEFAULT); /* Single epoch for all threads */
    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *largs = &eng->loops[n];
        TAILQ_INIT(&largs->open_conns);
        largs->connection_unique_id_atomic = &eng->connection_unique_id_global;
        largs->params = params;
        largs->shared_eng_params = &eng->params;
        largs->remote_stats = calloc(params.remote_addresses.n_addrs,
                                     sizeof(largs->remote_stats[0]));
        largs->address_offset = n;
        largs->thread_no = n;
        largs->serialize_output_lock = &eng->serialize_output_lock;
        const int decims_in_1s = 10 * 1000; /* decimilliseconds, 1/10 ms */
        if(params.latency_setting & SLT_CONNECT) {
            int ret = hdr_init(
                1, /* 1/10 milliseconds is the lowest storable value. */
                100 * decims_in_1s, /* 100 seconds is a max storable value */
                3, &largs->connect_histogram_local);
            assert(ret == 0);
        }
        if(params.latency_setting & SLT_FIRSTBYTE) {
            int ret = hdr_init(
                1, /* 1/10 milliseconds is the lowest storable value. */
                100 * decims_in_1s, /* 100 seconds is a max storable value */
                3, &largs->firstbyte_histogram_local);
            assert(ret == 0);
        }
        if(params.latency_setting & SLT_MARKER) {
            int ret = hdr_init(
                1, /* 1/10 milliseconds is the lowest storable value. */
                100 * decims_in_1s, /* 100 seconds is a max storable value */
                3, &largs->marker_histogram_local);
            assert(ret == 0);
            DEBUG(DBG_DETAIL, "Initialized HdrHistogram with size %ld\n",
                  (long)hdr_get_memory_size(largs->marker_histogram_local));
        }
        if(pthread_mutex_init(&largs->shared_histograms_lock, 0) != 0) {
            /* At this stage in the program, no point to continue. */
            assert(!"Should really be unreachable");
        }

        int private_pipe[2];
        int rc = pipe(private_pipe);
        assert(rc == 0);
        largs->private_control_pipe_rd = private_pipe[0];
        largs->private_control_pipe_wr = private_pipe[1];
        largs->global_control_pipe_rd_nbio = gctl_pipe_rd;
        largs->global_feedback_pipe_wr = gfbk_pipe_wr;
        pcg32_srandom_r(&largs->rng, random(), n);

        rc = pthread_create(&eng->threads[n], 0, single_engine_loop_thread,
                            largs);
        assert(rc == 0);
    }

    return eng;
}

/*
 * Format and print latency snapshot.
 */
static void
print_latency_hdr_histrogram_percentiles(
    const char *title, const struct percentile_values *report_percentiles,
    struct hdr_histogram *histogram) {
    assert(histogram);

    size_t size = report_percentiles->size;

    printf("%s latency at percentiles: ", title);
    for(size_t i = 0; i < size; i++) {
        double per_d = report_percentiles->values[i].value_d;
        printf("%.1f%s", hdr_value_at_percentile(histogram, per_d) / 10.0,
               i == size - 1 ? "" : "/");
    }
    printf(" ms (");
    for(size_t i = 0; i < size; i++) {
        printf("%s%s", report_percentiles->values[i].value_s,
               i == size - 1 ? "" : "/");
    }
    printf("%%)\n");
}

static void
latency_snapshot_print(const struct percentile_values *latency_percentiles,
                       const struct latency_snapshot *latency) {
    if(latency->connect_histogram) {
        print_latency_hdr_histrogram_percentiles(
            "TCP connect", latency_percentiles, latency->connect_histogram);
    }
    if(latency->firstbyte_histogram) {
        print_latency_hdr_histrogram_percentiles("First byte", latency_percentiles,
                                                 latency->firstbyte_histogram);
    }
    if(latency->marker_histogram) {
        print_latency_hdr_histrogram_percentiles("Message", latency_percentiles,
                                                 latency->marker_histogram);
    }
}

/*
 * Estimate packets per second.
 */
static unsigned int
estimate_segments_per_op(non_atomic_wide_t ops, non_atomic_wide_t bytes) {
    const int tcp_mss = 1460; /* TCP Maximum Segment Size, estimate! */
    return ops ? ((bytes / ops) + (tcp_mss - 1)) / tcp_mss : 0;
}
static double
estimate_pps(double duration, non_atomic_wide_t ops, non_atomic_wide_t bytes) {
    unsigned packets_per_op = estimate_segments_per_op(ops, bytes);
    return (packets_per_op * ops) / duration;
}

void
engine_update_workers_send_rate(struct engine *eng, rate_spec_t rate_spec) {
    /*
     * Ask workers to recompute per-connection rates.
     */
    eng->params.channel_send_rate = rate_spec;
    for(int n = 0; n < eng->n_workers; n++) {
        int rc = write(eng->loops[n].private_control_pipe_wr, "r", 1);
        assert(rc == 1);
    }
}

void
engine_set_message_send_rate(struct engine *eng, double msg_rate) {
    engine_update_workers_send_rate(eng, RATE_MPS(msg_rate));
}

void
engine_update_send_rate(struct engine *eng, double multiplier) {
    rate_spec_t new_rate = eng->params.channel_send_rate;
    new_rate.value = new_rate.value * multiplier;
    engine_update_workers_send_rate(eng, new_rate);
}

/*
 * Send a signal to finish work and wait for all workers to terminate.
 */
void
engine_terminate(struct engine *eng, double epoch,
                 non_atomic_traffic_stats initial_traffic_stats,
                 struct percentile_values *latency_percentiles) {
    size_t connecting, conn_in, conn_out, conn_counter;

    engine_get_connection_stats(eng, &connecting, &conn_in, &conn_out,
                                &conn_counter);

    /*
     * Terminate all workers.
     */
    for(int n = 0; n < eng->n_workers; n++) {
        int rc = write(eng->loops[n].private_control_pipe_wr, "T", 1);
        assert(rc == 1);
    }

    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *largs = &eng->loops[n];
        void *value;
        pthread_join(eng->threads[n], &value);
        add_traffic_numbers_AtoN(&largs->worker_traffic_stats,
                                 &eng->total_traffic_stats);
    }

    /*
     * The engine termination (using 'T') will implicitly prepare
     * latency snapshots. We only need to collect it now.
     */
    struct latency_snapshot *latency = engine_collect_latency_snapshot(eng);

    eng->n_workers = 0;

    /* Data snd/rcv after ramp-up (since epoch) */
    double now = tk_now(TK_DEFAULT);
    double test_duration = now - epoch;
    non_atomic_traffic_stats epoch_traffic =
        subtract_traffic_stats(eng->total_traffic_stats, initial_traffic_stats);
    non_atomic_wide_t epoch_data_transmitted =
        epoch_traffic.bytes_sent + epoch_traffic.bytes_rcvd;

    char buf[64];

    printf("Total data sent:     %s (%" PRIu64 " bytes)\n",
           express_bytes(epoch_traffic.bytes_sent, buf, sizeof(buf)),
           (uint64_t)epoch_traffic.bytes_sent);
    printf("Total data received: %s (%" PRIu64 " bytes)\n",
           express_bytes(epoch_traffic.bytes_rcvd, buf, sizeof(buf)),
           (uint64_t)epoch_traffic.bytes_rcvd);
    long conns = (0 * connecting) + conn_in + conn_out;
    if(!conns) conns = 1; /* Assume a single channel. */
    printf("Bandwidth per channel: %.3f⇅ Mbps (%.1f kBps)\n",
           8 * ((epoch_data_transmitted / test_duration) / conns) / 1000000.0,
           (epoch_data_transmitted / test_duration) / conns / 1000.0);
    printf("Aggregate bandwidth: %.3f↓, %.3f↑ Mbps\n",
           8 * (epoch_traffic.bytes_rcvd / test_duration) / 1000000.0,
           8 * (epoch_traffic.bytes_sent / test_duration) / 1000000.0);
    if(eng->params.message_marker) {
        printf("Aggregate message rate: %.3f↓, %.3f↑ mps\n",
               (epoch_traffic.msgs_rcvd / test_duration),
               (epoch_traffic.msgs_sent / test_duration));
    }
    printf("Packet rate estimate: %.1f↓, %.1f↑ (%u↓, %u↑ TCP MSS/op)\n",
           estimate_pps(test_duration, epoch_traffic.num_reads,
                        epoch_traffic.bytes_rcvd),
           estimate_pps(test_duration, epoch_traffic.num_writes,
                        epoch_traffic.bytes_sent),
           estimate_segments_per_op(epoch_traffic.num_reads,
                                    epoch_traffic.bytes_rcvd),
           estimate_segments_per_op(epoch_traffic.num_writes,
                                    epoch_traffic.bytes_sent));
    latency_snapshot_print(latency_percentiles, latency);

    engine_free_latency_snapshot(latency);
    printf("Test duration: %g s.\n", test_duration);
}

static char *
express_bytes(size_t bytes, char *buf, size_t size) {
    if(bytes < 2048) {
        snprintf(buf, size, "%ld bytes", (long)bytes);
    } else if(bytes < 512 * 1024) {
        snprintf(buf, size, "%.1f KiB", (bytes / 1024.0));
    } else {
        snprintf(buf, size, "%.1f MiB", (bytes / (1024 * 1024.0)));
    }
    return buf;
}

/*
 * Get number of connections opened by all of the workers.
 */
void
engine_get_connection_stats(struct engine *eng, size_t *connecting,
                            size_t *incoming, size_t *outgoing,
                            size_t *counter) {
    size_t c_conn = 0;
    size_t c_in = 0;
    size_t c_out = 0;
    size_t c_count = 0;

    for(int n = 0; n < eng->n_workers; n++) {
        c_conn += atomic_get(&eng->loops[n].outgoing_connecting);
        c_out += atomic_get(&eng->loops[n].outgoing_established);
        c_in += atomic_get(&eng->loops[n].incoming_established);
        c_count += atomic_get(&eng->loops[n].connections_counter);
    }
    *connecting = c_conn;
    *incoming = c_in;
    *outgoing = c_out;
    *counter = c_count;
}

void
engine_free_latency_snapshot(struct latency_snapshot *latency) {
    if(latency) {
        free(latency->connect_histogram);
        free(latency->firstbyte_histogram);
        free(latency->marker_histogram);
        free(latency);
    }
}

/*
 * Prepare latency snapshot data.
 */
void
engine_prepare_latency_snapshot(struct engine *eng) {
    if(eng->params.latency_setting != 0) {
        /*
         * If histogram is requested, we first need to ask each worker to
         * assemble that information among its connections.
         */
        for(int n = 0; n < eng->n_workers; n++) {
            int fd = eng->loops[n].private_control_pipe_wr;
            int wrote = write(fd, "h", 1);
            assert(wrote == 1);
        }
        /* Gather feedback. */
        for(int n = 0; n < eng->n_workers; n++) {
            char c;
            int rd = read(eng->global_feedback_pipe_rd, &c, 1);
            assert(rd == 1);
            assert(c == '.');
        }
    }
}

/*
 * Grab the prepared latency snapshot data.
 */
struct latency_snapshot *
engine_collect_latency_snapshot(struct engine *eng) {
    struct latency_snapshot *latency = calloc(1, sizeof(*latency));

    if(eng->params.latency_setting == 0) return latency;

    struct hdr_init_values {
        int64_t lowest_trackable_value;
        int64_t highest_trackable_value;
        int64_t significant_figures;
    } conn_init = {0, 0, 0}, fb_init = {0, 0, 0}, mark_init = {0, 0, 0};

    /* There's going to be no wait or contention here due
     * to the pipe-driven command-response logic. However,
     * we still lock the reporting histogram to pretend that
     * we correctly deal with memory barriers (which we don't
     * have to on x86).
     */
    pthread_mutex_lock(&eng->loops[0].shared_histograms_lock);
    if(eng->loops[0].connect_histogram_shared) {
        conn_init.lowest_trackable_value =
            eng->loops[0].connect_histogram_shared->lowest_trackable_value;
        conn_init.highest_trackable_value =
            eng->loops[0].connect_histogram_shared->highest_trackable_value;
        conn_init.significant_figures =
            eng->loops[0].connect_histogram_shared->significant_figures;
    }
    if(eng->loops[0].firstbyte_histogram_shared) {
        fb_init.lowest_trackable_value =
            eng->loops[0].firstbyte_histogram_shared->lowest_trackable_value;
        fb_init.highest_trackable_value =
            eng->loops[0].firstbyte_histogram_shared->highest_trackable_value;
        fb_init.significant_figures =
            eng->loops[0].firstbyte_histogram_shared->significant_figures;
    }
    if(eng->loops[0].marker_histogram_shared) {
        mark_init.lowest_trackable_value =
            eng->loops[0].marker_histogram_shared->lowest_trackable_value;
        mark_init.highest_trackable_value =
            eng->loops[0].marker_histogram_shared->highest_trackable_value;
        mark_init.significant_figures =
            eng->loops[0].marker_histogram_shared->significant_figures;
    }
    pthread_mutex_unlock(&eng->loops[0].shared_histograms_lock);

    if(conn_init.significant_figures) {
        int ret = hdr_init(
            conn_init.lowest_trackable_value, conn_init.highest_trackable_value,
            conn_init.significant_figures, &latency->connect_histogram);
        assert(ret == 0);
    }
    if(fb_init.significant_figures) {
        int ret = hdr_init(
            fb_init.lowest_trackable_value, fb_init.highest_trackable_value,
            fb_init.significant_figures, &latency->firstbyte_histogram);
        assert(ret == 0);
    }
    if(mark_init.significant_figures) {
        int ret = hdr_init(
            mark_init.lowest_trackable_value, mark_init.highest_trackable_value,
            mark_init.significant_figures, &latency->marker_histogram);
        assert(ret == 0);
    }

    for(int n = 0; n < eng->n_workers; n++) {
        pthread_mutex_lock(&eng->loops[n].shared_histograms_lock);
        if(latency->connect_histogram)
            hdr_add(latency->connect_histogram,
                    eng->loops[n].connect_histogram_shared);
        if(latency->firstbyte_histogram)
            hdr_add(latency->firstbyte_histogram,
                    eng->loops[n].firstbyte_histogram_shared);
        if(latency->marker_histogram)
            hdr_add(latency->marker_histogram,
                    eng->loops[n].marker_histogram_shared);
        pthread_mutex_unlock(&eng->loops[n].shared_histograms_lock);
    }

    return latency;
}

struct latency_snapshot *
engine_diff_latency_snapshot(struct latency_snapshot *base, struct latency_snapshot *update) {

    assert(base);
    assert(update);

    struct latency_snapshot *diff = calloc(1, sizeof(*diff));
    assert(diff);

    if(base->connect_histogram)
        diff->connect_histogram =
            hdr_diff(base->connect_histogram, update->connect_histogram);
    if(base->firstbyte_histogram)
        diff->firstbyte_histogram =
            hdr_diff(base->firstbyte_histogram, update->firstbyte_histogram);
    if(base->marker_histogram)
        diff->marker_histogram =
            hdr_diff(base->marker_histogram, update->marker_histogram);

    return diff;
}

non_atomic_traffic_stats
engine_traffic(struct engine *eng) {
    non_atomic_traffic_stats traffic = {0, 0, 0, 0, 0, 0};
    for(int n = 0; n < eng->n_workers; n++) {
        add_traffic_numbers_AtoN(&eng->loops[n].worker_traffic_stats, &traffic);
    }
    return traffic;
}

/*
 * Enable (1) and disable (0) the non-blocking mode on a file descriptor.
 */
static void
set_nbio(int fd, int onoff) {
    int rc;
    if(onoff) {
        /* Enable non-blocking mode. */
        rc = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    } else {
        /* Enable blocking mode (disable non-blocking). */
        rc = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    }
    assert(rc != -1);
}

#define SET_XXXBUF(fd, opt, value)                                  \
    do {                                                            \
        if((value)) set_socket_xxxbuf(fd, opt, #opt, value, largs); \
    } while(0)
static void
set_socket_xxxbuf(int fd, int opt, const char *opt_name, size_t value,
                  struct loop_arguments *largs) {
    int rc = setsockopt(fd, SOL_SOCKET, opt, &value, sizeof(value));
    assert(rc != -1);
    if(largs->params.verbosity_level >= DBG_DETAIL) {
        size_t end_value = value;
        socklen_t end_value_size = sizeof(end_value);
        int rc =
            getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &end_value, &end_value_size);
        assert(rc != -1);
        DEBUG(DBG_DETAIL, "setsockopt(%d, %s, %ld) -> %ld\n", fd, opt_name,
              (long)value, (long)end_value);
    }
}

static void
set_socket_options(int fd, struct loop_arguments *largs) {
    int on = ~0;
    int rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    assert(rc != -1);
    if(largs->params.nagle_setting != NSET_UNSET) {
        int v = largs->params.nagle_setting;
        rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
        assert(rc != -1);
    }

    SET_XXXBUF(fd, SO_RCVBUF, largs->params.sock_rcvbuf_size);

    SET_XXXBUF(fd, SO_SNDBUF, largs->params.sock_sndbuf_size);
}

size_t
engine_initiate_new_connections(struct engine *eng, size_t n_req) {
    static char buf[1024]; /* This is thread-safe! */
    if(!buf[0]) {
        memset(buf, 'c', sizeof(buf));
    }
    size_t n = 0;

    enum {
        ATTEMPT_FAIR_BALANCE,
        FIRST_READER_WINS
    } balance = ATTEMPT_FAIR_BALANCE;
    if(balance == ATTEMPT_FAIR_BALANCE) {
        while(n < n_req) {
            int worker = eng->next_worker_order[CONTROL_MESSAGE_CONNECT]++
                         % eng->n_workers;
            int fd = eng->loops[worker].private_control_pipe_wr;
            int wrote = write(fd, buf, 1);
            if(wrote == -1 && errno == EINTR) /* Ctrl+C? */
                return n;
            assert(wrote == 1);
            n++;
        }
    } else {
        int fd = eng->global_control_pipe_wr;
        set_nbio(fd, 1);
        while(n < n_req) {
            int current_batch =
                (n_req - n) < sizeof(buf) ? (n_req - n) : sizeof(buf);
            int wrote = write(fd, buf, current_batch);
            if(wrote == -1) {
                if(errno == EAGAIN) break;
                if(errno == EINTR) return n; /* Ctrl+C? */
                assert(wrote != -1);
            }
            if(wrote > 0) n += wrote;
        }
        set_nbio(fd, 0);
    }
    return n;
}

static void
expire_channel_lives(TK_P_ tk_timer UNUSED *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn;
    struct connection *tmpconn;

    assert(limit_channel_lifetime(largs));
    double delta = tk_now(TK_A) - largs->params.epoch;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        double expires_in = conn->channel_eol_point - delta;
        if(expires_in <= 0.0) {
            close_connection(TK_A_ conn, CCR_CLEAN);
        } else {
            /*
             * Channels are added to the tail of the queue and have the same
             * Expiration timeout. This channel and the others after it
             * are not yet expired. Restart timeout so we'll get to it
             * and the one after it when the time is really due.
             */
            setup_channel_lifetime_timer(TK_A_ expires_in);
            break;
        }
    }
}

static void
stats_timer_cb(TK_P_ tk_timer UNUSED *w, int UNUSED revents) {
    connections_flush_stats(TK_A);
}

static void *
single_engine_loop_thread(void *argp) {
    struct loop_arguments *largs = (struct loop_arguments *)argp;
    tk_loop *loop = tk_loop_new();
    tk_set_userdata(loop, largs);

    tk_io global_control_watcher;
    tk_io private_control_watcher;
    const int on_main_thread = (largs->thread_no == 0);

#ifdef SO_REUSEPORT
    const int have_reuseport = 1;
#else
    const int have_reuseport = 0;
#endif

    /*
     * If want to serve connections but don't have SO_REUSEPORT,
     * tell the user upfront.
     */
    if(!have_reuseport && on_main_thread
       && largs->params.listen_addresses.n_addrs) {
        warning(
            "A system without SO_REUSEPORT detected."
            " Using only one core for serving connections.\n");
    }

    signal(SIGPIPE, SIG_IGN);

    tcpkali_ssl_thread_setup();
    /*
     * Open all listening sockets, if they are specified.
     */
    if(largs->params.listen_addresses.n_addrs
       /* Only listen on stuff on other cores when SO_REUSEPORT is available */
       && (have_reuseport || on_main_thread)) {
        int opened_listening_sockets = 0;
        for(size_t n = 0; n < largs->params.listen_addresses.n_addrs; n++) {
            struct sockaddr_storage *ss =
                &largs->params.listen_addresses.addrs[n];
            int rc;
            int lsock = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP);
            assert(lsock != -1);
            set_nbio(lsock, 1);
#ifdef SO_REUSEPORT
            int on = ~0;
            rc = setsockopt(lsock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
            assert(rc != -1);
#else
/*
 * SO_REUSEPORT cannot be used, which means that only a single
 * thread could create their own separate listening socket on
 * a specified address/port. This is bad, but changing.
 * See http://permalink.gmane.org/gmane.linux.network/158320
 */
#endif /* SO_REUSEPORT */
            rc = bind(lsock, (struct sockaddr *)ss, sockaddr_len(ss));
            if(rc == -1) {
                char buf[256];
                DEBUG(DBG_ALWAYS, "Bind %s is not done on thread %d: %s\n",
                      format_sockaddr(ss, buf, sizeof(buf)), largs->thread_no,
                      strerror(errno));
                exit(EX_UNAVAILABLE);
            }
            assert(rc == 0);
            rc = listen(lsock, 256);
            assert(rc == 0);
            opened_listening_sockets++;

            struct connection *conn = calloc(1, sizeof(*conn));
            conn->conn_type = CONN_ACCEPTOR;
            /* avoid TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook); */
            pacefier_init(&conn->send_pace, -1.0, tk_now(TK_A));
            pacefier_init(&conn->recv_pace, -1.0, tk_now(TK_A));
#ifdef USE_LIBUV
            uv_poll_init(TK_A_ & conn->watcher, lsock);
            uv_poll_start(&conn->watcher, TK_READ | TK_WRITE, accept_cb_uv);
#else
            ev_io_init(&conn->watcher, accept_cb, lsock, TK_READ | TK_WRITE);
            ev_io_start(TK_A_ & conn->watcher);
#endif
        }
        if(!opened_listening_sockets) {
            DEBUG(DBG_ALWAYS, "Could not listen on any local sockets!\n");
            exit(EX_UNAVAILABLE);
        }
    }

    const int stats_flush_interval_ms = 42;
#ifdef USE_LIBUV
    if(limit_channel_lifetime(largs)) {
        uv_timer_init(TK_A_ & largs->channel_lifetime_timer);
        uv_timer_start(&largs->channel_lifetime_timer, expire_channel_lives_uv,
                       0, 0);
    }
    uv_timer_init(TK_A_ & largs->stats_timer);
    uv_timer_start(&largs->stats_timer, stats_timer_cb_uv, stats_flush_interval_ms, stats_flush_interval_ms);
    uv_poll_init(TK_A_ & global_control_watcher,
                 largs->global_control_pipe_rd_nbio);
    uv_poll_init(TK_A_ & private_control_watcher,
                 largs->private_control_pipe_rd);
    uv_poll_start(&global_control_watcher, TK_READ, control_cb_uv);
    uv_poll_start(&private_control_watcher, TK_READ, control_cb_uv);
    uv_run(TK_A_ UV_RUN_DEFAULT);
    uv_timer_stop(&largs->stats_timer);
    uv_poll_stop(&global_control_watcher);
    uv_poll_stop(&private_control_watcher);
#else
    if(limit_channel_lifetime(largs)) {
        ev_timer_init(&largs->channel_lifetime_timer, expire_channel_lives, 0,
                      0);
    }
    ev_timer_init(&largs->stats_timer, stats_timer_cb, stats_flush_interval_ms / 1000.0, stats_flush_interval_ms / 1000.0);
    ev_timer_start(TK_A_ & largs->stats_timer);
    ev_io_init(&global_control_watcher, control_cb,
               largs->global_control_pipe_rd_nbio, TK_READ);
    ev_io_init(&private_control_watcher, control_cb,
               largs->private_control_pipe_rd, TK_READ);
    ev_io_start(loop, &global_control_watcher);
    ev_io_start(loop, &private_control_watcher);
    ev_run(loop, 0);
    ev_timer_stop(TK_A_ & largs->stats_timer);
    ev_io_stop(TK_A_ & global_control_watcher);
    ev_io_stop(TK_A_ & private_control_watcher);
#endif

    connections_flush_stats(TK_A);

    close_all_connections(TK_A_ CCR_CLEAN);

    /* Avoid mixing debug output from several threads. */
    pthread_mutex_lock(largs->serialize_output_lock);

    DEBUG(DBG_DETAIL,
          "Exiting worker %d\n"
          "  %" PRIan "↓, %" PRIan "↑ open connections (%" PRIan
          " connecting)\n"
          "  %" PRIan
          " connections_counter \n"
          "  ↳ %lu connections_initiated\n"
          "  ↳ %lu connections_accepted\n"
          "  %lu connection_failures\n"
          "  ↳ %lu connection_timeouts\n"
          "  %" PRIaw
          " worker_data_sent\n"
          "  %" PRIaw
          " worker_data_rcvd\n"
          "  %" PRIaw
          " worker_num_writes\n"
          "  %" PRIaw " worker_num_reads\n",
          largs->thread_no, atomic_get(&largs->incoming_established),
          atomic_get(&largs->outgoing_established),
          atomic_get(&largs->outgoing_connecting),
          atomic_get(&largs->connections_counter),
          largs->worker_connections_initiated,
          largs->worker_connections_accepted, largs->worker_connection_failures,
          largs->worker_connection_timeouts,
          atomic_wide_get(&largs->worker_traffic_stats.bytes_sent),
          atomic_wide_get(&largs->worker_traffic_stats.bytes_rcvd),
          atomic_wide_get(&largs->worker_traffic_stats.num_writes),
          atomic_wide_get(&largs->worker_traffic_stats.num_reads));

    if(largs->connect_histogram_local) {
        struct hdr_histogram *hist = largs->connect_histogram_local;
        DEBUG(DBG_DETAIL,
              "  Connect latency:\n"
              "    %.1f latency_95_ms\n"
              "    %.1f latency_99_ms\n"
              "    %.1f latency_99_5_ms\n"
              "    %.1f latency_mean_ms\n"
              "    %.1f latency_max_ms\n",
              hdr_value_at_percentile(hist, 95.0) / 10.0,
              hdr_value_at_percentile(hist, 99.0) / 10.0,
              hdr_value_at_percentile(hist, 99.5) / 10.0, hdr_mean(hist) / 10.0,
              hdr_max(hist) / 10.0);
        if(largs->params.verbosity_level >= DBG_DEBUG)
            hdr_percentiles_print(hist, stderr, 5, 10, CLASSIC);
    }
    if(largs->marker_histogram_local) {
        struct hdr_histogram *hist = largs->marker_histogram_local;
        DEBUG(DBG_DETAIL,
              "  Marker latency:\n"
              "    %.1f latency_95_ms\n"
              "    %.1f latency_99_ms\n"
              "    %.1f latency_99_5_ms\n"
              "    %.1f latency_mean_ms\n"
              "    %.1f latency_max_ms\n",
              hdr_value_at_percentile(hist, 95.0) / 10.0,
              hdr_value_at_percentile(hist, 99.0) / 10.0,
              hdr_value_at_percentile(hist, 99.5) / 10.0, hdr_mean(hist) / 10.0,
              hdr_max(hist) / 10.0);
        if(largs->params.verbosity_level >= DBG_DEBUG)
            hdr_percentiles_print(hist, stderr, 5, 10, CLASSIC);
    }

    /*
     * Print the scratch buffer to highlight the last thing received.
     */
    if(largs->params.verbosity_level >= DBG_DETAIL) {
        debug_dump_data("Last received bytes ", -1, largs->scratch_recv_buf,
                        largs->scratch_recv_last_size, -1500);
    }

    pthread_mutex_unlock(largs->serialize_output_lock);

    return 0;
}

/*
 * Init HDR Histogram with properties similar to a given one.
 */
static struct hdr_histogram *
hdr_init_similar(struct hdr_histogram *htemplate) {
    if(htemplate) {
        struct hdr_histogram *dst = 0;
        if(hdr_init(htemplate->lowest_trackable_value,
                    htemplate->highest_trackable_value,
                    htemplate->significant_figures, &dst)
           == 0) {
            return dst;
        } else {
            assert(!"Can't create copy of histogram");
        }
    }
    return NULL;
}

/*
 * Copy a histogram by erasing the destination histogram first and adding
 * the source histogram into it.
 */
static void
histogram_data_copy_to_shared(struct hdr_histogram *src,
                              struct hdr_histogram **dst) {
    if(src) {
        if(*dst) {
            hdr_reset(*dst);
        } else {
            *dst = hdr_init_similar(src);
            assert(*dst);
        }
        hdr_add(*dst, src);
    }
}

/*
 * Each worker maintains two sets of histogram data structures:
 *  1) the xxx_histogram_local ones, which are not protected by mutex
 *     and are directly writable by connections when they operate and die.
 *  2) the xxx_histogram_shared, which are protected by the mutex and
 *     are only updated from time to time.
 *     The shared ones are used for reporting to external observers.
 */
static void
worker_update_shared_histograms(struct loop_arguments *largs) {
    if(largs->params.latency_setting == 0) return;

    pthread_mutex_lock(&largs->shared_histograms_lock);

    /* --latency-connect */
    histogram_data_copy_to_shared(largs->connect_histogram_local,
                                  &largs->connect_histogram_shared);

    /* --latency-firstbyte */
    histogram_data_copy_to_shared(largs->firstbyte_histogram_local,
                                  &largs->firstbyte_histogram_shared);

    /* --latency-marker */
    histogram_data_copy_to_shared(largs->marker_histogram_local,
                                  &largs->marker_histogram_shared);

    if(largs->marker_histogram_local) {
        /*
         * 2. There are connections with accumulated data,
         *    process a few of them (all would be expensive)
         *    and add their data as well.
         * FYI: 10 hdr_adds() take ~0.2ms.
         */
        struct connection *conn;
        int nmax = 10;
        TAILQ_FOREACH(conn, &largs->open_conns, hook) {
            if(conn->latency.marker_histogram) {
                /* Only active connections might histograms */
                hdr_add(largs->marker_histogram_shared,
                        conn->latency.marker_histogram);
                if(--nmax == 0) break;
            }
        }
    }


    pthread_mutex_unlock(&largs->shared_histograms_lock);
}

/*
 * Receive a control event from the pipe.
 */
static void
control_cb(TK_P_ tk_io *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    char c;
    int ret = read(tk_fd(w), &c, 1);
    if(ret != 1) {
        if(errno != EAGAIN)
            DEBUG(DBG_ALWAYS,
                  "%d Reading from control channel %d returned: %s\n",
                  largs->thread_no, tk_fd(w), strerror(errno));
        return;
    }
    switch(c) {
    case 'c': /* Initiate a new connection */
        start_new_connection(TK_A);
        break;
    case 'r': /* Recompute message rate on live connections */
        largs->params.channel_send_rate =
            largs->shared_eng_params->channel_send_rate;

        struct connection *conn;
        TAILQ_FOREACH(conn, &largs->open_conns, hook) {
            conn->send_limit = compute_bandwidth_limit_by_message_size(
                largs->params.channel_send_rate,
                conn->avg_message_size);
            double now = tk_now(TK_A);
            if(conn->conn_type == CONN_OUTGOING
                    || (largs->params.listen_mode & _LMODE_SND_MASK)) {
                pacefier_init(&conn->send_pace, conn->send_limit.bytes_per_second, now);
            }
        }
        if(largs->marker_histogram_local && largs->marker_histogram_shared) {
            pthread_mutex_lock(&largs->shared_histograms_lock);
            hdr_reset(largs->marker_histogram_local);
            hdr_reset(largs->marker_histogram_shared);
            pthread_mutex_unlock(&largs->shared_histograms_lock);
            TAILQ_FOREACH(conn, &largs->open_conns, hook) {
                hdr_reset(conn->latency.marker_histogram);
            }
        }
        break;
    case 'T': /* Terminate */
        worker_update_shared_histograms(largs);
        tk_stop(TK_A);
        break;
    case 'h': /* Update historgrams */
        worker_update_shared_histograms(largs);
        int wrote = write(largs->global_feedback_pipe_wr, ".", 1);
        assert(wrote == 1);
        break;
    default:
        DEBUG(DBG_ALWAYS, "Unknown operation '%c' from a control channel %d\n",
              c, tk_fd(w));
    }
}

static ssize_t
expr_callback(char *buf, size_t size, tk_expr_t *expr, void *key, long *v) {
    struct connection *conn = key;
    ssize_t s;

    switch(expr->type) {
    case EXPR_CONNECTION_PTR:
        s = snprintf(buf, size, "%p", conn);
        if(v) *v = (long)conn;
        break;
    case EXPR_CONNECTION_UID:
        s = snprintf(buf, size, "%" PRIan, conn->connection_unique_id);
        if(v) *v = (long)conn->connection_unique_id;
        break;
    case EXPR_MESSAGE_MARKER: {
#define MZEROS   "0000000000000000"
        const size_t tok_size = sizeof(MESSAGE_MARKER_TOKEN MZEROS ".")-1;
        assert(size >= tok_size);
        memcpy(buf, MESSAGE_MARKER_TOKEN MZEROS ".", tok_size);
        s = tok_size;
        if(v) *v = (long)0;
        break;
    }
    default:
        s = snprintf(buf, size, "?");
        if(v) *v = 0;
        break;
    }

    if(s < 0 || s > (ssize_t)size) return -1;
    return s;
}

static void
explode_data_template(struct message_collection *mc,
                      struct transport_data_spec *const data_templates[2],
                      enum transport_websocket_side tws_side,
                      struct transport_data_spec *out_data,
                      struct loop_arguments *largs,
                      struct connection *conn) {
    if(data_templates[tws_side]) {
        assert(mc->most_dynamic_expression == DS_GLOBAL_FIXED);
        /*
         * Return the already once prepared data.
         */
        *out_data = *data_templates[tws_side];
        out_data->flags |= TDS_FLAG_PTR_SHARED;
    } else {
        /*
         * We might need a unique ID for a connection, and it is a bit expensive
         * to obtain it. We set it here once during connection establishment.
         */
        conn->connection_unique_id =
            atomic_inc_and_get(largs->connection_unique_id_atomic);

        struct transport_data_spec *new_data_ptr;
        new_data_ptr = transport_spec_from_message_collection(
            out_data, mc, expr_callback, conn, tws_side, TS_CONVERSION_INITIAL, &largs->rng);
        assert(new_data_ptr == out_data);

        switch(mc->most_dynamic_expression) {
        case DS_GLOBAL_FIXED:
            assert(!"Unreachable");
        case DS_PER_CONNECTION:
            if(largs->params.message_marker == 0) {
                replicate_payload(out_data, REPLICATE_MAX_SIZE);
            }
            break;
        case DS_PER_MESSAGE:
            break;
        }
        assert(out_data->ptr);
    }
}

static void
explode_data_template_override(struct message_collection *mc,
                               enum transport_websocket_side tws_side,
                               struct transport_data_spec *out_data,
                               struct loop_arguments *largs,
                               struct connection *conn) {
    assert(mc->most_dynamic_expression == DS_PER_MESSAGE);

    struct transport_data_spec *new_data_ptr;
    new_data_ptr = transport_spec_from_message_collection(
        out_data, mc, expr_callback, conn, tws_side,
        TS_CONVERSION_OVERRIDE_MESSAGES, &largs->rng);
    assert(new_data_ptr == out_data);
}

static void
explode_string_expression(char **buf_p, size_t *size, tk_expr_t *expr,
                          struct loop_arguments *largs,
                          struct connection *conn) {
    *buf_p = 0;
    ssize_t s = eval_expression(buf_p, 0, expr, expr_callback, conn, 0,
                                conn->conn_type == CONN_OUTGOING, &largs->rng);
    assert(s >= 0);
    *size = s;
}

static void start_new_connection(TK_P) {
    char tmpbuf[INET6_ADDRSTRLEN + 64];
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct remote_stats *remote_stats;
    size_t remote_index;

    struct sockaddr_storage *ss = pick_remote_address(largs, &remote_index);
    remote_stats = &largs->remote_stats[remote_index];

    atomic_increment(&largs->connections_counter);
    atomic_increment(&remote_stats->connection_attempts);
    largs->worker_connections_initiated++;

    int sockfd = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd == -1) {
        switch(errno) {
        case EMFILE:
            DEBUG(DBG_ERROR, "Cannot create socket: %s\n", strerror(errno));
            DEBUG(DBG_ALWAYS,
                  "Increase `ulimit -n` to twice exceed the --connections.\n");
            exit(1);
        case ENFILE:
            DEBUG(DBG_ERROR, "Cannot create socket: %s\n", strerror(errno));
            DEBUG(DBG_ALWAYS, "Increase kern.maxfiles/fs.file-max sysctls\n");
            exit(1);
        }
        DEBUG(DBG_WARNING, "Cannot create socket: %s\n", strerror(errno));
        return; /* Come back later */
    } else {
        set_nbio(sockfd, 1);
        set_socket_options(sockfd, largs);
    }

    /* If --source-ip is specified, bind to the next one. */
    if(largs->params.source_addresses.n_addrs) {
        struct sockaddr_storage *bind_ss =
            &largs->params.source_addresses
                 .addrs[largs->worker_connections_initiated
                        % largs->params.source_addresses.n_addrs];
        int rc =
            bind(sockfd, (struct sockaddr *)bind_ss, sockaddr_len(bind_ss));
        if(rc == -1) {
            atomic_increment(&remote_stats->connection_failures);
            largs->worker_connection_failures++;
            close(sockfd);
            DEBUG(DBG_WARNING, "Connection to %s is not done: %s\n",
                  format_sockaddr(ss, tmpbuf, sizeof(tmpbuf)), strerror(errno));
            return;
        }
    }

    int conn_state;
    int rc = connect(sockfd, (struct sockaddr *)ss, sockaddr_len(ss));
    if(rc == -1) {
        switch(errno) {
        case EINPROGRESS:
            break;
        case EADDRNOTAVAIL: /* Bind failed */
            if(largs->params.source_addresses.n_addrs) {
                /* This is local problem, not remote address problem. */
                largs->worker_connection_failures++;
                DEBUG(DBG_WARNING, "Connection to %s is not done: %s\n",
                      format_sockaddr(ss, tmpbuf, sizeof(tmpbuf)),
                      strerror(errno));
            }
        /* FALL THROUGH */
        default:
            atomic_increment(&remote_stats->connection_failures);
            largs->worker_connection_failures++;
            if(atomic_get(&remote_stats->connection_failures) == 1) {
                DEBUG(DBG_WARNING, "Connection to %s is not done: %s\n",
                      format_sockaddr(ss, tmpbuf, sizeof(tmpbuf)),
                      strerror(errno));
            }
            close(sockfd);
            return;
        }

        atomic_increment(&largs->outgoing_connecting);
        conn_state = CSTATE_CONNECTING;
    } else { /* This branch is for completeness only. Should not happen. */
        if(largs->params.channel_lifetime == 0.0) {
            close(sockfd);
            return;
        }
        atomic_increment(&largs->outgoing_established);
        conn_state = CSTATE_CONNECTED;
        if(largs->connect_histogram_local)
            hdr_record_value(largs->connect_histogram_local, 0);
    }

    /*
     * Print the src/dst for a connection if verbosity level is high enough.
     */
    if(largs->params.verbosity_level >= DBG_DETAIL) {
        char srcaddr_buf[INET6_ADDRSTRLEN + 64];
        char dstaddr_buf[INET6_ADDRSTRLEN + 64];
        struct sockaddr_storage srcaddr;
        socklen_t addrlen = sizeof(srcaddr);
        if(getsockname(sockfd, (struct sockaddr *)&srcaddr, &addrlen) == 0) {
            DEBUG(DBG_DETAIL, "Connection %s -> %s opened as %d\n",
                  format_sockaddr(&srcaddr, srcaddr_buf, sizeof(srcaddr_buf)),
                  format_sockaddr(ss, dstaddr_buf, sizeof(dstaddr_buf)),
                  sockfd);
        } else {
            DEBUG(DBG_WARNING, "Can't getsockname(%d): %s", sockfd,
                  strerror(errno));
        }
    }

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->remote_index = remote_index;
    common_connection_init(TK_A_ conn, CONN_OUTGOING, conn_state, sockfd);
}

/*
 * Pick an address in a round-robin fashion, skipping certainly broken ones.
 */
static struct sockaddr_storage *
pick_remote_address(struct loop_arguments *largs, size_t *remote_index) {
    /*
     * If it is known that a particular destination is broken, choose
     * the working one right away.
     */
    size_t off = 0;
    for(size_t attempts = 0; attempts < largs->params.remote_addresses.n_addrs;
        attempts++) {
        off = largs->address_offset++ % largs->params.remote_addresses.n_addrs;
        struct remote_stats *rs = &largs->remote_stats[off];
        if(atomic_get(&rs->connection_attempts) > 10
           && atomic_get(&rs->connection_failures)
                  == atomic_get(&rs->connection_attempts)) {
            continue;
        } else {
            break;
        }
    }

    *remote_index = off;
    return &largs->params.remote_addresses.addrs[off];
}

static void
conn_timer_cb(TK_P_ tk_timer *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn =
        (struct connection *)((char *)w - offsetof(struct connection, timer));

    switch(conn->conn_state) {
    case CSTATE_CONNECTED:
        switch(conn->conn_type) {
        case CONN_INCOMING:
            if((largs->params.listen_mode & _LMODE_SND_MASK) == 0) {
                conn->conn_wish &=
                    ~(CW_READ_BLOCKED | CW_WRITE_BLOCKED | CW_WRITE_DELAYED);
                update_io_interest(TK_A_ conn);
                break;
            }
        /* Fall through */
        case CONN_OUTGOING:
            if(conn->conn_wish & CW_WRITE_DELAYED) {
                /* Reinitialize the upstream bandwidth limit */
                pacefier_init(&conn->send_pace,
                              conn->send_limit.bytes_per_second, tk_now(TK_A));
            }
            conn->conn_wish &=
                ~(CW_READ_BLOCKED | CW_WRITE_BLOCKED | CW_WRITE_DELAYED);
            update_io_interest(TK_A_ conn);
            break;
        case CONN_ACCEPTOR:
            assert(conn->conn_type != CONN_ACCEPTOR);
            break;
        }
        break;
    case CSTATE_CONNECTING:
        /* Timed out in the connection establishment phase. */
        close_connection(TK_A_ conn, CCR_TIMEOUT);
        return;
    }
}

/*
 * Check whether we have configured an extended (e.g., non-zero), but
 * finite channel lifetime. Zero channel lifetime is managed differently:
 * the channel closes right after we figure out that the connection took place.
 */
static int
limit_channel_lifetime(struct loop_arguments *largs) {
    return (largs->params.channel_lifetime != INFINITY
            && largs->params.channel_lifetime > 0.0);
}

static void
setup_channel_lifetime_timer(TK_P_ double first_timeout) {
    struct loop_arguments *largs = tk_userdata(TK_A);
#ifdef USE_LIBUV
    uint64_t delay = 1000 * first_timeout;
    if(delay == 0) delay = 1;
    uv_timer_start(&largs->channel_lifetime_timer, expire_channel_lives_uv,
                   delay, 0);
#else
    ev_timer_stop(TK_A_ & largs->channel_lifetime_timer);
    ev_timer_set(&largs->channel_lifetime_timer, first_timeout, 0);
    ev_timer_start(TK_A_ & largs->channel_lifetime_timer);
#endif
}

/*
 * If we're not dumping something on a main thread, and we need
 * to keep dumping some connection, enable data dumping for that connection.
 */
static void
maybe_enable_dump(struct loop_arguments *largs, enum conn_type ctype, int fd) {
    /*
     * Enable dumping only on main (first) worker.
     * It spares us from too much coordination over
     * which worker's channel should get its data dumped.
     */
    const int main_thread = (largs->thread_no == 0);

    if(main_thread
       /* DS_DUMP_ALL is handled differently */
       && (largs->params.dump_setting & DS_DUMP_ONE)
       && largs->dump_connect_fd == 0
       /*
        * In case tcpkali acts as both client and listener (-l), we only dump
        * client-side connections.
        */
       && (ctype == CONN_OUTGOING
           || largs->params.remote_addresses.n_addrs == 0)) {
        /*
         * Enable dumping on a chosen file descriptor.
         */
        largs->dump_connect_fd = fd;
    }
}


static void
connection_timer_refresh(TK_P_ struct connection *conn, double delay) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    tk_timer_stop(TK_A, &conn->timer);

    switch(conn->conn_state) {
    case CSTATE_CONNECTED:
        /* Use the supplied delay */
        break;
    case CSTATE_CONNECTING:
        delay = largs->params.connect_timeout;
        break;
    }

    if(delay > 0.0) {
#ifdef USE_LIBUV
        uv_timer_init(TK_A_ & conn->timer);
        uint64_t uint_delay = 1000 * delay;
        if(uint_delay == 0) uint_delay = 1;
        uv_timer_start(&conn->timer, conn_timer_cb_uv, uint_delay, 0);
#else
        ev_timer_init(&conn->timer, conn_timer_cb, delay, 0.0);
        ev_timer_start(TK_A_ & conn->timer);
#endif
    }
}

/*
 * Initialize common connection parameters.
 */
static void
common_connection_init(TK_P_ struct connection *conn, enum conn_type conn_type,
                       enum conn_state conn_state, int sockfd) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    conn->conn_type = conn_type;
    conn->conn_state = conn_state;

    maybe_enable_dump(largs, conn_type, sockfd);

    double now = tk_now(TK_A);

    conn->latency.connection_initiated = now;
    conn->bytes_leftovers = 0;

    if(limit_channel_lifetime(largs)) {
        if(TAILQ_FIRST(&largs->open_conns) == NULL) {
            setup_channel_lifetime_timer(TK_A_ largs->params.channel_lifetime);
        }
        conn->channel_eol_point =
            (now - largs->params.epoch) + largs->params.channel_lifetime;
    }
    TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook);

    /*
     * Set up downstream bandwidth regardless of the type of connection.
     */

    conn->recv_limit = compute_bandwidth_limit(largs->params.channel_recv_rate);
    pacefier_init(&conn->recv_pace, conn->recv_limit.bytes_per_second, now);

    /*
     * If we're going to send data, establish bandwidth control for upstream.
     */

    int active_socket = conn_type == CONN_OUTGOING
                        || (largs->params.listen_mode & _LMODE_SND_MASK);

    if(active_socket) {

        message_collection_replicate(&largs->params.message_collection, &conn->message_collection);
        enum transport_websocket_side tws_side =
            (conn_type == CONN_OUTGOING) ? TWS_SIDE_CLIENT : TWS_SIDE_SERVER;
        explode_data_template(&conn->message_collection,
                              largs->params.data_templates, tws_side,
                              &conn->data, largs, conn);
        enum websocket_side ws_side =
            (tws_side == TWS_SIDE_CLIENT) ? WS_SIDE_CLIENT : WS_SIDE_SERVER;
        conn->avg_message_size = message_collection_estimate_size(
            &conn->message_collection,
            MSK_PURPOSE_MESSAGE, MSK_PURPOSE_MESSAGE,
            MCE_AVERAGE_SIZE, ws_side, largs->params.websocket_enable);
        conn->send_limit = compute_bandwidth_limit_by_message_size(
            largs->params.channel_send_rate, conn->avg_message_size);
        pacefier_init(&conn->send_pace, conn->send_limit.bytes_per_second, now);
        if(largs->params.message_stop_expr) {
            conn->sbmh_stop_ctx = malloc(SBMH_SIZE(largs->params.message_stop_expr->estimate_size));
            assert(conn->sbmh_stop_ctx);
            sbmh_init(conn->sbmh_stop_ctx, NULL, 0, 0);
        }
    }

    if(largs->params.latency_marker_expr && (conn->data.single_message_size || largs->params.message_marker)) {
        if(conn->data.single_message_size) {
            conn->latency.message_bytes_credit /* See (EXPL:1) below. */
                = conn->data.single_message_size - 1;
        }
        /*
         * Figure out how many latency markers to skip
         * before starting to measure latency with them.
         */
        conn->latency.lm_occurrences_skip =
            largs->params.latency_marker_skip;

        /*
         * Initialize the Boyer-Moore-Horspool context for substring search.
         */
        struct StreamBMH_Occ *init_occ = NULL;
        if(EXPR_IS_TRIVIAL(largs->params.latency_marker_expr)) {
            /* Shared search table and expression */
            conn->latency.sbmh_shared = 1;
            conn->latency.sbmh_occ = &largs->params.sbmh_shared_marker_occ;
            conn->latency.sbmh_data =
                (uint8_t *)largs->params.latency_marker_expr->u.data.data;
            conn->latency.sbmh_size =
                largs->params.latency_marker_expr->u.data.size;
        } else {
            /* Individual search table. */
            conn->latency.sbmh_shared = 0;
            conn->latency.sbmh_occ =
                malloc(sizeof(*conn->latency.sbmh_occ));
            assert(conn->latency.sbmh_occ);
            init_occ = conn->latency.sbmh_occ;
            explode_string_expression(
                (char **)&conn->latency.sbmh_data, &conn->latency.sbmh_size,
                largs->params.latency_marker_expr, largs, conn);
        }
        conn->latency.sbmh_marker_ctx =
            malloc(SBMH_SIZE(conn->latency.sbmh_size));
        assert(conn->latency.sbmh_marker_ctx);
        sbmh_init(conn->latency.sbmh_marker_ctx, init_occ,
                  conn->latency.sbmh_data, conn->latency.sbmh_size);

        /*
         * Initialize the latency histogram by copying out the template
         * parameter from the loop arguments.
         */
        conn->latency.sent_timestamps = ring_buffer_new(sizeof(double));
        conn->latency.marker_histogram =
            hdr_init_similar(largs->marker_histogram_local);
    }

    /*
     * Catch connection timeout.
     */
    int want_catch_connect = (conn_state == CSTATE_CONNECTING
                              && largs->params.connect_timeout > 0.0);
    if(want_catch_connect) {
        assert(conn_type == CONN_OUTGOING);
        connection_timer_refresh(TK_A_ conn, 0.0);
    }

    conn->conn_wish =
        CW_READ_INTEREST
        | ((conn->data.total_size || want_catch_connect) ? CW_WRITE_INTEREST
                                                         : 0);

    if(largs->params.websocket_enable) {
        if(conn_type == CONN_OUTGOING) {
            int want_events = TK_READ | TK_WRITE;
#ifdef USE_LIBUV
            uv_poll_init(TK_A_ & conn->watcher, sockfd);
            uv_poll_start(&conn->watcher, want_events, connection_cb_uv);
#else
            ev_io_init(&conn->watcher, connection_cb, sockfd, want_events);
            ev_io_start(TK_A_ & conn->watcher);
#endif
        } else {
#ifdef USE_LIBUV
            uv_poll_init(TK_A_ & conn->watcher, sockfd);
            uv_poll_start(&conn->watcher, TK_READ, passive_websocket_cb_uv);
#else
            ev_io_init(&conn->watcher, passive_websocket_cb, sockfd, TK_READ);
            ev_io_start(TK_A_ & conn->watcher);
#endif
        }
    } else { /* Plain socket */
        int want_write = (conn->data.total_size || want_catch_connect);
        int want_events = TK_READ | (want_write ? TK_WRITE : 0);
#ifdef USE_LIBUV
        uv_poll_init(TK_A_ & conn->watcher, sockfd);
        uv_poll_start(&conn->watcher, want_events, connection_cb_uv);
#else
        ev_io_init(&conn->watcher, connection_cb, sockfd, want_events);
        ev_io_start(TK_A_ & conn->watcher);
#endif
    }
    if(largs->params.ssl_enable != 0) {
        ssl_setup(conn, sockfd, largs->params.ssl_cert, largs->params.ssl_key);
    }
}

static void
accept_cb(TK_P_ tk_io *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    int sockfd = accept(tk_fd(w), 0, 0);
    if(sockfd == -1) {
        switch(errno) {
        case EINTR:
            break;
        case EAGAIN:
            break;
        case EMFILE:
        case ENFILE:
            if(largs->params.remote_addresses.n_addrs == 0) {
                /*
                 * If we are in a purely listen mode (no active connections),
                 * we largely ignore connections that exceeding capacity.
                 * Leave some indication that we're churning through these
                 * connections. It will be visible in the status ribbon
                 * even if verbosity is low.
                 */
                atomic_increment(&largs->connections_counter);
                DEBUG(DBG_DETAIL, "Cannot accept a new connection: %s\n",
                      strerror(errno));
                break;
            }
            DEBUG(DBG_ERROR, "Cannot accept a new connection: %s\n",
                  strerror(errno));
            if(errno == EMFILE) { /* Per-process limit */
                DEBUG(DBG_ALWAYS,
                      "Increase `ulimit -n` to twice exceed the "
                      "--connections.\n");
            } else if(errno == ENFILE) { /* System limit */
                DEBUG(DBG_ALWAYS,
                      "Increase kern.maxfiles/fs.file-max sysctls\n");
            }
            exit(1);
        default:
            DEBUG(DBG_DETAIL, "Cannot accept a new connection: %s\n",
                  strerror(errno));
        }
        return;
    }
    set_nbio(sockfd, 1);
    set_socket_options(sockfd, largs);

    atomic_increment(&largs->connections_counter);
    largs->worker_connections_accepted++;

    /* If channel lifetime is 0, close it right away. */
    if(largs->params.channel_lifetime == 0.0) {
        close(sockfd);
        return;
    }

    struct connection *conn = calloc(1, sizeof(*conn));
    socklen_t addrlen = sizeof(conn->peer_name);
    if(getpeername(sockfd, (struct sockaddr *)&conn->peer_name, &addrlen)
       != 0) {
        DEBUG(DBG_WARNING, "Can't getpeername(%d): %s", sockfd,
              strerror(errno));
        free(conn);
        close(sockfd);
        return;
    }
    atomic_increment(&largs->incoming_established);
    common_connection_init(TK_A_ conn, CONN_INCOMING, CSTATE_CONNECTED, sockfd);
}

/*
 * Debug data by dumping it in a format escaping all the special
 * characters.
 */
static void
debug_dump_data(const char *prefix, int fd, const void *data, size_t size,
                ssize_t limit) {
    debug_dump_data_highlight(prefix, fd, data, size, limit, 0, 0);
}
static void
debug_dump_data_highlight(const char *prefix, int fd, const void *data,
                          size_t size, ssize_t limit, size_t hl_offset,
                          size_t hl_length) {
    /*
     * Do not show more than (limit) first bytes,
     * or more than (-limit) last bytes of the buffer.
     */
    size_t original_size = size;
    size_t preceding = 0;
    size_t following = 0;
    if(limit) {
        if(limit > 0 && (size_t)limit < size) {
            following = size - limit;
            size = limit;
        } else if((size_t)-limit < size) {
            preceding = size + limit;
            size = -limit;
            data += preceding;
        }
    }

    char stack_buffer[4000];
    char *buffer = stack_buffer;
    size_t buf_size = PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(size);

    if(buf_size > sizeof(stack_buffer)) {
        /* This is used only for debugging of an otherwise expensive dataset.
         * We could have cached the malloc result, but that'd be a non-local
         * complication, yielding negligible performance benefit
         * (after all, we're here to print a multi-kilobyte message dump).
         */
        buffer = malloc(buf_size);
        assert(buffer);
    }

    char fdnumbuf[16];
    if(fd >= 0) {
        snprintf(fdnumbuf, sizeof(fdnumbuf), "%d, ", fd);
    } else {
        fdnumbuf[0] = '\0';
    }
    fprintf(stderr, "%s%s(%s%ld): %s%s[%s%s%s]%s%s\n", tcpkali_clear_eol(),
            prefix, fdnumbuf, (long)original_size, preceding ? "..." : "",
            tk_attr(*prefix == 'S' ? TKA_SndBrace : TKA_RcvBrace),
            tk_attr(TKA_NORMAL),
            printable_data_highlight(buffer, buf_size, data, size, 0, hl_offset,
                                     hl_length),
            tk_attr(*prefix == 'S' ? TKA_SndBrace : TKA_RcvBrace),
            tk_attr(TKA_NORMAL), following ? "..." : "");
    if(buffer != stack_buffer) free(buffer);
}

static enum lb_return_value {
    LB_UNLIMITED, /* Not limiting bandwidth, proceed. */
    LB_PROCEED,   /* Use pacefier_moved() afterwards. */
    LB_LOCKSTEP,  /* Proceed but pause right after. */
    LB_GO_SLEEP,  /* Not allowed to move data.        */
} limit_channel_bandwidth(TK_P_ struct connection *conn,
                          size_t *suggested_move_size, int event) {
    struct pacefier *pace = NULL;
    bandwidth_limit_t limit = {0, 0};
    enum lb_return_value rvalue = LB_UNLIMITED;

    if(event & TK_WRITE) {
        limit = conn->send_limit;
        pace = &conn->send_pace;
    } else if(event & TK_READ) {
        limit = conn->recv_limit;
        pace = &conn->recv_pace;
    } else {
        assert(event & (TK_WRITE | TK_READ));
    }

    double bw = limit.bytes_per_second;
    if(bw < 0.0) {
        return LB_UNLIMITED; /* Limit not set, don't limit. */
    }

    size_t smallest_block_to_move = limit.minimal_move_size;
    size_t allowed_to_move = pacefier_allow(pace, tk_now(TK_A));

    if(allowed_to_move < *suggested_move_size) {
        double delay;

        if(event & TK_READ) {
            if(allowed_to_move < smallest_block_to_move) {
                /*   allowed     smallest|suggested
                   |------^-----------^-------^-------> */
                delay = pacefier_when_allowed(pace, tk_now(TK_A), 1460);
                *suggested_move_size = 0;
                rvalue = LB_GO_SLEEP;
            } else {
                /*
                 * If we're reading, there's no gain in reading many
                 * tiny chunks: the packets on the wire will still
                 * very likely be the same, as TCP stacks don't like
                 * sending too small window updates to the sender.
                 * So if the reading allowance is too small, make it
                 * large enough still to batch reads.
                 */
                if(allowed_to_move > 1460)
                    *suggested_move_size = allowed_to_move;
                else
                    *suggested_move_size = 1460;
                return LB_PROCEED;
            }
        } else {
            if(allowed_to_move < smallest_block_to_move) {
                /*   allowed     smallest|suggested
                   |------^-----------^-------^-------> */
                delay = (double)(smallest_block_to_move - allowed_to_move) / bw;
                *suggested_move_size = 0;
                rvalue = LB_GO_SLEEP;
            } else {
                /*   smallest  allowed  suggested
                   |------^--------^-------^-------> */
                size_t excess = (allowed_to_move % smallest_block_to_move);
                delay = (double)(smallest_block_to_move - excess) / bw;
                *suggested_move_size = allowed_to_move - excess;
                rvalue = LB_LOCKSTEP;
            }
        }

        if(delay < 0.001) delay = 0.001;

        connection_timer_refresh(TK_A_ conn, delay);

        return rvalue;
    }

    return LB_PROCEED;
}

static void
passive_websocket_cb(TK_P_ tk_io *w, int revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn =
        (struct connection *)((char *)w - offsetof(struct connection, watcher));
    char out_buf[200];
    size_t response_size = 0;

    if(conn->conn_blocked & CBLOCKED_ON_INIT) {
        if(((conn->conn_blocked & CBLOCKED_ON_READ) && (revents & TK_READ)) ||
           ((conn->conn_blocked & CBLOCKED_ON_WRITE) && (revents & TK_WRITE))) {
            if(conn->conn_blocked & CBLOCKED_ON_READ) {
                revents &= ~TK_READ;
            }
            if(conn->conn_blocked & CBLOCKED_ON_WRITE) {
                revents &= ~TK_WRITE;
            }
            if(ssl_setup(conn, 0, largs->params.ssl_cert, largs->params.ssl_key)) {
                if(conn->conn_blocked & CBLOCKED_ON_INIT) {
                    conn->conn_wish |= CW_READ_INTEREST;
                    conn->conn_wish |= CW_WRITE_INTEREST;
                    update_io_interest(TK_A_ conn);
                    return;
                }
            } else {
                close_connection(TK_A_ conn, CCR_REMOTE);
                return;
            }
        } else {
            return;
        }
    }
    if(revents & TK_WRITE) {
        if(conn->conn_blocked & CBLOCKED_ON_READ) {
            return;
        }
        conn->conn_blocked &= ~CBLOCKED_ON_WRITE;
    }
    if(revents & TK_READ) {
        ssize_t rd = 0;
        if(largs->params.ssl_enable) {
#ifdef HAVE_OPENSSL
            if(conn->conn_blocked & CBLOCKED_ON_WRITE) {
                return;
            }
            conn->conn_blocked &= ~CBLOCKED_ON_READ;
            rd = SSL_read(conn->ssl_fd, largs->scratch_recv_buf,
                          sizeof(largs->scratch_recv_buf));
            switch(SSL_get_error(conn->ssl_fd, rd)) {
            case SSL_ERROR_NONE:
                break;
            case SSL_ERROR_WANT_WRITE:
                conn->conn_blocked |= CBLOCKED_ON_WRITE;
                return;
            case SSL_ERROR_WANT_READ:
                conn->conn_blocked |= CBLOCKED_ON_READ;
                return;
            case SSL_ERROR_ZERO_RETURN:
            default:
                rd = -1;  // Close it
            }
#endif
        } else {
            rd = read(tk_fd(w), largs->scratch_recv_buf,
                      sizeof(largs->scratch_recv_buf));
        }
        switch(rd) {
        case -1:
            switch(errno) {
            case EAGAIN:
                return;
            default:
                close_connection(TK_A_ conn, CCR_REMOTE);
                return;
            }
        /* Fall through */
        case 0:
            close_connection(TK_A_ conn, CCR_REMOTE);
            return;
        default:
            largs->scratch_recv_last_size = rd; /* Only update on >0 data */
            conn->traffic_ongoing.num_reads++;
            conn->traffic_ongoing.bytes_rcvd += rd;
            if(largs->params.dump_setting & DS_DUMP_ALL_IN
               || ((largs->params.dump_setting & DS_DUMP_ONE_IN)
                   && largs->dump_connect_fd == tk_fd(w))) {
                debug_dump_data("Rcv", tk_fd(w), largs->scratch_recv_buf, rd,
                                0);
            }
            latency_record_incoming_ts(TK_A_ conn, largs->scratch_recv_buf, rd);

            /*
             * Attempt to detect websocket key in HTTP and respond.
             */
            switch(http_detect_websocket(largs->scratch_recv_buf, rd, out_buf,
                                         sizeof(out_buf), &response_size)) {
            case HDW_NOT_ENOUGH_DATA:
                return;
            case HDW_WEBSOCKET_DETECTED:
                conn->ws_state = WSTATE_WS_ESTABLISHED;
                break;
            case HDW_TRUNCATED_INPUT:
            case HDW_UNEXPECTED_ERROR:
                close_connection(TK_A_ conn, CCR_DATA);
                return;
            }
            break;
        }
    }
    if((conn->ws_state == WSTATE_WS_ESTABLISHED) && !conn->conn_blocked) {
        if(!response_size) {
            switch(http_detect_websocket(largs->scratch_recv_buf,
                                  largs->scratch_recv_last_size, out_buf,
                                  sizeof(out_buf), &response_size)) {
            case HDW_NOT_ENOUGH_DATA:
            case HDW_WEBSOCKET_DETECTED:
                break;
            case HDW_TRUNCATED_INPUT:
            case HDW_UNEXPECTED_ERROR:
                close_connection(TK_A_ conn, CCR_DATA);
                return;
            }
        }
        if(largs->params.ssl_enable) {
#ifdef HAVE_OPENSSL
            int wrote = SSL_write(conn->ssl_fd, out_buf, response_size);
            switch(SSL_get_error(conn->ssl_fd, wrote)) {
            case SSL_ERROR_NONE:
                break;
            case SSL_ERROR_WANT_WRITE:
                conn->conn_blocked |= CBLOCKED_ON_WRITE;
                return;
            case SSL_ERROR_WANT_READ:
                conn->conn_blocked |= CBLOCKED_ON_READ;
                return;
            case SSL_ERROR_ZERO_RETURN:
            default:
                wrote = -1;  // Close it
            }
            if(wrote != (ssize_t)response_size) {
                close_connection(TK_A_ conn, CCR_DATA);
                return;
            }
#endif
        } else {
            if(write(tk_fd(w), out_buf, response_size)
               != (ssize_t)response_size) {
                close_connection(TK_A_ conn, CCR_DATA);
                return;
            }
        }
        int want_events = TK_READ | TK_WRITE;
#ifdef USE_LIBUV
        uv_poll_start(&conn->watcher, want_events, connection_cb_uv);
#else
        ev_io_stop(TK_A_ w);
        ev_io_init(&conn->watcher, connection_cb, tk_fd(w), want_events);
        ev_io_start(TK_A_ & conn->watcher);
#endif
    }
}

static void
update_io_interest(TK_P_ struct connection *conn) {
    int events = 0;
    /* Remove read or write wish, if we don't want them */
    events |= (conn->conn_wish & CW_READ_INTEREST) ? TK_READ : 0;
    events |= (conn->conn_wish & CW_WRITE_INTEREST) ? TK_WRITE : 0;
    /* Remove read or write wish, if we are blocked on them */
    events &= ~((conn->conn_wish & CW_READ_BLOCKED) ? TK_READ : 0);
    events &= ~((conn->conn_wish & (CW_WRITE_BLOCKED | CW_WRITE_DELAYED))
                ? TK_WRITE : 0);

#ifdef USE_LIBUV
    (void)loop;
    uv_poll_start(&conn->watcher, events, conn->watcher.poll_cb);
#else
    ev_io_stop(TK_A_ & conn->watcher);
    ev_io_set(&conn->watcher, conn->watcher.fd, events);
    ev_io_start(TK_A_ & conn->watcher);
#endif
}

static void
latency_record_outgoing_ts(TK_P_ struct connection *conn, size_t wrote) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    if(largs->params.message_marker) {
            if (conn->avg_message_size > 0) {
                conn->traffic_ongoing.msgs_sent += (conn->bytes_leftovers + wrote) / conn->avg_message_size;
                conn->bytes_leftovers = (conn->bytes_leftovers + wrote) % conn->avg_message_size;
            }
        return;
    }

    if(!conn->latency.sent_timestamps) return;

    /*
     * (EXPL:1)
     * Arguably we should record timestamps only for the whole messages which
     * were sent. However, in some cases (such as with echo servers) the server
     * could start replying earlier than the whole message is received. This
     * confuses the latency estimation code because the reply gets received
     * before the message is fully sent to the remote system. For example,
     *  tcpkali --message Foooooo --latency-marker F <ip>:echo
     * would fail, because F is sometimes received before than the trailing
     * ooo's get to be sent. Therefore we record the timestamp even after
     * only the very first message byte is sent.
     */

    size_t msgsize = conn->data.single_message_size;
    size_t pretend_sent = wrote + conn->latency.message_bytes_credit;
    size_t messages = pretend_sent / msgsize;
    conn->latency.message_bytes_credit =
        pretend_sent % conn->data.single_message_size;
    int ring_grown = 0;
    for(double now = tk_now(TK_A); messages; messages--) {
        ring_grown |= ring_buffer_add(conn->latency.sent_timestamps, now);
    }
    if(ring_grown) {
        /*
         * Ring has grown [even more]; check that we aren't recording send
         * timestamps without actually receiving any data back.
         */
        const unsigned MEGABYTE = 1024 * 1024;
        if(conn->latency.sent_timestamps->size > 10 * MEGABYTE) {
            DEBUG(
                DBG_ERROR,
                "Sending messages too fast, "
                "not receiving them back fast enough.\n"
                "Check that the --latency-marker data is being received back.\n"
                "Use -d option to dump received message data.\n");
            exit(1);
        }
    }
}

static unsigned nibble(const unsigned char c) {
    switch(c) {
    case '0' ... '9': return (c - '0');
    case 'a' ... 'f': return (c - 'a') + 10;
    case 'A' ... 'F': return (c - 'A') + 10;
    default:
        return 0;
    }
}

static void
latency_record_incoming_ts(TK_P_ struct connection *conn, char *buf,
                           size_t size) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    if(!conn->latency.sent_timestamps && !largs->params.message_marker) return;

    const uint8_t *lm = conn->latency.sbmh_data;
    size_t lm_size = conn->latency.sbmh_size;
    unsigned num_markers_found = 0;

    for(; size > 0;) {
        switch(conn->latency.marker_parser.state) {
        case MP_DISENGAGED:
            break;
        case MP_SLURPING_DIGITS:
            if(*buf != '.') {
                conn->latency.marker_parser.collected_digits <<= 4;
                conn->latency.marker_parser.collected_digits |= nibble(*buf);
                buf++;
                size--;
                continue;
            } else {
                conn->latency.marker_parser.state = MP_DISENGAGED;
                conn->traffic_ongoing.msgs_rcvd++;
                struct timeval tp;
                gettimeofday(&tp, NULL);
                int64_t latency = (int64_t)tp.tv_sec * 1000000 + tp.tv_usec
                        - (int64_t)conn->latency.marker_parser.collected_digits;
                latency /= 100; // 1/10 ms
                if(hdr_record_value(conn->latency.marker_histogram, latency)
                        == false) {
                    fprintf(stderr,
                            "Latency value %g is too large, "
                            "can't record.\n",
                            (double) (latency / 10000));
                }
            }
        }
        size_t analyzed =
            sbmh_feed(conn->latency.sbmh_marker_ctx, conn->latency.sbmh_occ, lm,
                      lm_size, (unsigned char *)buf, size);
        if(conn->latency.sbmh_marker_ctx->found == sbmh_true) {
            buf += analyzed;
            size -= analyzed;
            if(largs->params.message_marker) {
                conn->latency.marker_parser.state = MP_SLURPING_DIGITS;
                conn->latency.marker_parser.collected_digits = 0;
            } else {
                num_markers_found++;
            }
            sbmh_reset(conn->latency.sbmh_marker_ctx);
        } else {
            break;
        }
    }

    if(largs->params.message_marker) return;

    /*
     * Skip the necessary numbers of markers.
     */
    if(conn->latency.lm_occurrences_skip) {
        if(num_markers_found <= conn->latency.lm_occurrences_skip) {
            conn->latency.lm_occurrences_skip -= num_markers_found;
            return;
        } else {
            num_markers_found -= conn->latency.lm_occurrences_skip;
            conn->latency.lm_occurrences_skip = 0;
        }
    }

    /*
     * Now, for all found markers extract and use the corresponding
     * end-to-end message latency.
     */
    double now = tk_now(TK_A);
    while(num_markers_found--) {
        double ts;
        int got = ring_buffer_get(conn->latency.sent_timestamps, &ts);
        if(got) {
            int64_t latency = 10000 * (now - ts);
            if(hdr_record_value(conn->latency.marker_histogram, latency)
               == false) {
                fprintf(stderr,
                        "Latency value %g is too large, "
                        "can't record.\n",
                        now - ts);
            }
        } else {
            fprintf(stderr,
                    "More messages received than sent. "
                    "Choose a different --latency-marker.\n"
                    "Use -d option to dump received message data.\n");
            exit(1);
        }
    }
}

static void
scan_incoming_bytes(TK_P_ struct connection *conn, char *buf, size_t size) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    if(conn->sbmh_stop_ctx) {
        size_t needlen = largs->params.message_stop_expr->u.data.size;
        size_t analyzed = sbmh_feed(
            conn->sbmh_stop_ctx, &largs->params.sbmh_shared_stop_occ,
            (unsigned char *)largs->params.message_stop_expr->u.data.data,
            needlen, (unsigned char *)buf, size);
        if(conn->sbmh_stop_ctx->found == sbmh_true) {
            /* Length of --message-stop. */
            size_t needle_tail_in_scope = analyzed > needlen ? needlen : analyzed;
            debug_dump_data_highlight(
                "Last packet", -1, buf, size, 0,
                analyzed > needlen ? analyzed - needlen : 0,
                needle_tail_in_scope);
            char stop_msg[PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(needlen)];
            fprintf(stdout, "Found --message-stop=%s, aborting.\n",
                    printable_data_highlight(
                        stop_msg, sizeof(stop_msg),
                        largs->params.message_stop_expr->u.data.data, needlen,
                        1, 0, needlen));
            exit(2);
        }
        (void)analyzed;
    }
}


static void override_timestamp(char *ptr, size_t size, unsigned long long ts) {
    const size_t mmt_len = sizeof(MESSAGE_MARKER_TOKEN) - 1;
    assert(size >= mmt_len + 16 + 1);
    assert(ptr[0] == MESSAGE_MARKER_TOKEN[0]);
    ptr += mmt_len;
    snprintf(ptr, size - mmt_len, "%016llx", ts);
    ptr[16] = '.';
}

static void update_timestamps(char *ptr, size_t size) {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    unsigned long long ts = (unsigned long long)tp.tv_sec * 1000000 + tp.tv_usec;
    char *end = ptr + size;
    while((ptr = memmem(ptr, end - ptr, MESSAGE_MARKER_TOKEN, sizeof(MESSAGE_MARKER_TOKEN) - 1))) {
        size_t remaining = end - ptr;
        const size_t full_marker = (sizeof(MESSAGE_MARKER_TOKEN)-1) + 16 + 1;
        if(remaining >= full_marker) {
            override_timestamp(ptr, remaining, ts);
            ptr += full_marker;
        } else {
            break;
        }
    }
}

/*
 * Compute the largest amount of data we can send to the channel
 * using a single write() call.
 */
static void
largest_contiguous_chunk(struct loop_arguments *largs, struct connection *conn,
                         const void **position, size_t *available_header,
                         size_t *available_body) {
    off_t *current_offset = &conn->write_offset;
    size_t accessible_size = conn->data.total_size;
    size_t available = accessible_size - *current_offset;

    /* The first bunch of bytes sent on the WebSocket connection
     * should be limited by the HTTP upgrade headers.
     * We then wait for the server reply.
     */
    if(conn->traffic_ongoing.bytes_rcvd == 0
       && conn->traffic_ongoing.bytes_sent <= conn->data.ws_hdr_size
       && conn->ws_state == WSTATE_SENDING_HTTP_UPGRADE
       && largs->params.websocket_enable) {
        accessible_size = conn->data.ws_hdr_size;
        size_t available = accessible_size - *current_offset;
        *position = conn->data.ptr + *current_offset;
        *available_header = available;
        *available_body = 0;
        return;
    }

    if(conn->traffic_ongoing.bytes_sent < conn->data.once_size) {
        /* Send header... once per connection lifetime */
        *available_header =
            conn->data.once_size - conn->traffic_ongoing.bytes_sent;
        assert(available);
    } else {
        *available_header = 0; /* Sending body */
    }

    if(available) {
        *position = conn->data.ptr + *current_offset;
        *available_body = available - *available_header;
    } else {
        /* If we're at the end of the buffer, re-blow it with new messages */
        if(conn->message_collection.most_dynamic_expression
               == DS_PER_MESSAGE
           && (conn->conn_type == CONN_OUTGOING
               || (largs->params.listen_mode & _LMODE_SND_MASK))) {
            explode_data_template_override(&conn->message_collection,
                                           (conn->conn_type == CONN_OUTGOING)
                                               ? TWS_SIDE_CLIENT
                                               : TWS_SIDE_SERVER,
                                           &conn->data, largs, conn);
            accessible_size = conn->data.total_size;
        }

        size_t off = conn->data.once_size;
        *position = conn->data.ptr + off;
        *available_body = accessible_size - off;
        *current_offset = off;
    }

    if(largs->params.message_marker) {
        if(*position < conn->data.marker_token_ptr) {
            /* Short-circquit search: we know where marker is, directly. */
            struct timeval tp;
            gettimeofday(&tp, NULL);
            unsigned long long ts =
                (unsigned long long)tp.tv_sec * 1000000 + tp.tv_usec;
            override_timestamp(conn->data.marker_token_ptr,
                               (*available_body), ts);
        } else {
            /* Do a string search to find our markers and update timestamps */
            update_timestamps((char *)*position, *available_body);
        }
    }
}

static void
connection_cb(TK_P_ tk_io *w, int revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn =
        (struct connection *)((char *)w - offsetof(struct connection, watcher));
    struct sockaddr_storage *remote =
        conn->conn_type == CONN_OUTGOING
            ? &largs->params.remote_addresses.addrs[conn->remote_index]
            : &conn->peer_name;

    if(conn->conn_blocked & CBLOCKED_ON_INIT) {
        if(((conn->conn_blocked & CBLOCKED_ON_READ) && (revents & TK_READ)) ||
           ((conn->conn_blocked & CBLOCKED_ON_WRITE) && (revents & TK_WRITE))) {
            if(conn->conn_blocked & CBLOCKED_ON_READ) {
                revents &= ~TK_READ;
            }
            if(conn->conn_blocked & CBLOCKED_ON_WRITE) {
                revents &= ~TK_WRITE;
            }
            if(ssl_setup(conn, 0, largs->params.ssl_cert, largs->params.ssl_key)) {
                if(conn->conn_blocked & CBLOCKED_ON_INIT) {
                    conn->conn_wish |= CW_READ_INTEREST;
                    conn->conn_wish |= CW_WRITE_INTEREST;
                    update_io_interest(TK_A_ conn);
                    return;
                }
            } else {
                close_connection(TK_A_ conn, CCR_REMOTE);
            }
        } else {
            return;
        }
    }
    if(conn->conn_state == CSTATE_CONNECTING) {
        /*
         * Extended channel lifetimes are managed elsewhere, but zero
         * lifetime can be managed here very quickly.
         */
        if(largs->params.channel_lifetime == 0.0) {
            close_connection(TK_A_ conn, CCR_CLEAN);
            return;
        }

        atomic_decrement(&largs->outgoing_connecting);
        atomic_increment(&largs->outgoing_established);
        conn->conn_state = CSTATE_CONNECTED;
        if(largs->connect_histogram_local) {
            int64_t latency =
                10000 * (tk_now(TK_A) - conn->latency.connection_initiated);
            hdr_record_value(largs->connect_histogram_local, latency);
        }

        /*
         * We were asked to produce the WRITE event
         * only to detect successful connection.
         * If there's nothing to write, we remove the write interest.
         */
        tk_timer_stop(TK_A, &conn->timer);
        if((conn->data.total_size == 0) && !(conn->conn_blocked & CBLOCKED_ON_WRITE)) {
            conn->conn_wish &= ~CW_WRITE_INTEREST; /* Remove write interest */
            update_io_interest(TK_A_ conn);
            revents &= ~TK_WRITE; /* Don't actually write in this loop */
        }
    }

    if(revents & TK_READ) {
        int record_moved_data = 0;
        do {
            size_t read_size = sizeof(largs->scratch_recv_buf);
            if(largs->params.websocket_enable == 0
               || conn->ws_state == WSTATE_WS_ESTABLISHED) {
                switch(
                    limit_channel_bandwidth(TK_A_ conn, &read_size, TK_READ)) {
                case LB_UNLIMITED:
                    break;
                case LB_LOCKSTEP:
                    /* Rate limiter logic does not return it in read mode */
                    assert(!"Unreachable");
                /* Fall through */
                case LB_PROCEED:
                    record_moved_data = 1;
                    break;
                case LB_GO_SLEEP:
                    if(!(conn->conn_blocked & CBLOCKED_ON_READ)) {
                        conn->conn_wish |= CW_READ_BLOCKED;
                        update_io_interest(TK_A_ conn);
                        goto process_WRITE;
                    }
                }
            }

            assert(read_size > 0);
            ssize_t rd = 0;
            if(largs->params.ssl_enable) {
#ifdef HAVE_OPENSSL
                if(conn->conn_blocked & CBLOCKED_ON_WRITE) {
                    goto process_WRITE;
                }
                conn->conn_blocked &= ~CBLOCKED_ON_READ;
                rd = SSL_read(conn->ssl_fd, largs->scratch_recv_buf, read_size);
                switch(SSL_get_error(conn->ssl_fd, rd)) {
                case SSL_ERROR_NONE:
                    break;
                case SSL_ERROR_WANT_WRITE:
                    conn->conn_blocked |= CBLOCKED_ON_WRITE;
                    conn->conn_wish |= CW_WRITE_INTEREST;
                    update_io_interest(TK_A_ conn);
                    goto process_WRITE;
                    break;
                case SSL_ERROR_WANT_READ:
                    conn->conn_blocked |= CBLOCKED_ON_READ;
                    conn->conn_wish |= CW_READ_INTEREST;
                    update_io_interest(TK_A_ conn);
                    return;
                case SSL_ERROR_ZERO_RETURN:
                default:
                    rd = -1;  // Close it
                }
#endif
            } else {
                rd = read(tk_fd(w), largs->scratch_recv_buf, read_size);
            }
            switch(rd) {
            case -1:
                switch(errno) {
                case EINTR:
                case EAGAIN:
                    break;
                default: {
                    char buf[INET6_ADDRSTRLEN + 64];
                    DEBUG(DBG_NORMAL, "Closing %s: %s\n",
                          format_sockaddr(remote, buf, sizeof(buf)),
                          strerror(errno));
                    close_connection(TK_A_ conn, CCR_REMOTE);
                    return;
                }
                }
                break;
            case 0: {
                char buf[INET6_ADDRSTRLEN + 64];
                DEBUG(DBG_DETAIL, "Connection half-closed by %s\n",
                      format_sockaddr(remote, buf, sizeof(buf)));
                close_connection(TK_A_ conn, CCR_REMOTE);
                return;
            }
            default:
                largs->scratch_recv_last_size = rd; /* Only update on >0 data */
                                                    /*
                                                     * If this is a first packet from the remote server,
                                                     * and we're in a WebSocket mode where we should wait for
                                                     * the server response before sending rest, unblock our WRITE
                                                     * side.
                                                     */
                if(conn->traffic_ongoing.bytes_rcvd == 0
                   && conn->ws_state == WSTATE_SENDING_HTTP_UPGRADE
                   && largs->params.websocket_enable
                   && conn->data.ws_hdr_size != conn->data.total_size) {
                    conn->ws_state = WSTATE_WS_ESTABLISHED;
                    conn->conn_wish |= CW_WRITE_INTEREST;
                    update_io_interest(TK_A_ conn);
                }
                if(conn->traffic_ongoing.bytes_rcvd == 0
                   && largs->firstbyte_histogram_local) {
                    int64_t latency =
                        10000
                        * (tk_now(TK_A) - conn->latency.connection_initiated);
                    hdr_record_value(largs->firstbyte_histogram_local, latency);
                }
                conn->traffic_ongoing.num_reads++;
                conn->traffic_ongoing.bytes_rcvd += rd;
                if(largs->params.dump_setting & DS_DUMP_ALL_IN
                   || ((largs->params.dump_setting & DS_DUMP_ONE_IN)
                       && largs->dump_connect_fd == tk_fd(w))) {
                    debug_dump_data("Rcv", tk_fd(w), largs->scratch_recv_buf,
                                    rd, 0);
                }
                latency_record_incoming_ts(TK_A_ conn, largs->scratch_recv_buf,
                                           rd);
                scan_incoming_bytes(TK_A_ conn, largs->scratch_recv_buf, rd);

                if(record_moved_data) {
                    pacefier_moved(&conn->recv_pace, rd, tk_now(TK_A));
                }
                break;
            }
        } while(0);
    }

process_WRITE:
    if(revents & TK_WRITE) {
        const void *position;
        size_t available_header, available_body;
        int record_moved = 0;
        int lockstep = 0;

        largest_contiguous_chunk(largs, conn, &position, &available_header,
                                 &available_body);
        if(!(available_header + available_body) && !(conn->conn_blocked & CBLOCKED_ON_WRITE)) {
            /* Only the header was sent. Now, silence. */
            assert(conn->data.total_size == conn->data.once_size
                   || largs->params.websocket_enable);
            conn->conn_wish &= ~CW_WRITE_INTEREST; /* disable write interest */
            update_io_interest(TK_A_ conn);
            return;
        }

        /* Adjust (available_body) to avoid sending too much stuff. */
        switch(limit_channel_bandwidth(TK_A_ conn, &available_body, TK_WRITE)) {
        case LB_UNLIMITED:
            record_moved = 0;
            break;
        case LB_LOCKSTEP:
            lockstep = 1; /* FALL THROUGH */
        case LB_PROCEED:
            record_moved = 1;
            break;
        case LB_GO_SLEEP:
            if(available_header || (conn->conn_blocked & CBLOCKED_ON_WRITE)) break;
            conn->conn_wish |= CW_WRITE_BLOCKED;
            update_io_interest(TK_A_ conn);
            return;
        }

        if((largs->params.delay_send > 0.0
            && largs->params.delay_send
                   > tk_now(TK_A) - conn->latency.connection_initiated)) {
            conn->conn_wish |= CW_WRITE_DELAYED;
            update_io_interest(TK_A_ conn);
            connection_timer_refresh(TK_A_ conn, largs->params.delay_send);
            return;
        }
        do { /* Write de-coalescing loop */
            size_t available_write =
                available_header
                + (largs->params.write_combine == WRCOMB_ON
                       ? available_body
                       : available_body < conn->send_limit.minimal_move_size
                             ? available_body
                             : conn->send_limit.minimal_move_size);

            ssize_t wrote = 0;
            if(largs->params.ssl_enable) {
#ifdef HAVE_OPENSSL
                if(conn->conn_blocked & CBLOCKED_ON_READ) {
                    return;
                }
                conn->conn_blocked &= ~CBLOCKED_ON_WRITE;
                wrote = SSL_write(conn->ssl_fd, position, available_write);
                switch(SSL_get_error(conn->ssl_fd, wrote)) {
                case SSL_ERROR_NONE:
                    break;
                case SSL_ERROR_WANT_WRITE:
                    conn->conn_blocked |= CBLOCKED_ON_WRITE;
                    conn->conn_wish |= CW_WRITE_INTEREST;
                    update_io_interest(TK_A_ conn);
                    return;
                case SSL_ERROR_WANT_READ:
                    conn->conn_blocked |= CBLOCKED_ON_READ;
                    conn->conn_wish |= CW_READ_INTEREST;
                    update_io_interest(TK_A_ conn);
                    return;
                case SSL_ERROR_ZERO_RETURN:
                default:
                    wrote = -1;  // Close it
                }
#endif
            } else {
                wrote = write(tk_fd(w), position, available_write);
            }
            if(wrote == -1) {
                char buf[INET6_ADDRSTRLEN + 64];
                switch(errno) {
                case EINTR:
                    continue;
                case EAGAIN:
                    /* Undo rate limiting if not all data was sent. */
                    if(lockstep) {
                        tk_timer_stop(TK_A, &conn->timer);
                        lockstep = 0; /* Don't pause I/O later */
                    }
                    break;
                case EPIPE:
                default:
                    DEBUG(DBG_WARNING, "Connection reset by %s\n",
                          format_sockaddr(remote, buf, sizeof(buf)));
                    close_connection(TK_A_ conn, CCR_REMOTE);
                    return;
                }
                break;
            } else {
                conn->write_offset += wrote;
                conn->traffic_ongoing.num_writes++;
                conn->traffic_ongoing.bytes_sent += wrote;
                if(record_moved)
                    pacefier_moved(&conn->send_pace, wrote, tk_now(TK_A));
                if(largs->params.dump_setting & DS_DUMP_ALL_OUT
                   || ((largs->params.dump_setting & DS_DUMP_ONE_OUT)
                       && largs->dump_connect_fd == tk_fd(w))) {
                    debug_dump_data("Snd", tk_fd(w), position, wrote, 0);
                }
                if((size_t)wrote > available_header) {
                    position += wrote;
                    wrote -= available_header;
                    available_header = 0;
                    available_body -= wrote;

                    /* Record latencies for the body only, not headers */
                    latency_record_outgoing_ts(TK_A_ conn, wrote);
                }
            }
        } while(available_body);

        if(lockstep) {
            if(available_body) {
                /* Undo rate limiting if not all was sent. */
                tk_timer_stop(TK_A, &conn->timer);
                /* Will circle back and might set up a new timer */
            } else {
                conn->conn_wish |= CW_WRITE_BLOCKED;
                update_io_interest(TK_A_ conn);
            }
        }

    } /* (events & TK_WRITE) */
}

/*
 * Ungracefully close all connections and report accumulated stats
 * back to the central loop structure.
 */
static void
close_all_connections(TK_P_ enum connection_close_reason reason) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn;
    struct connection *tmpconn;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        close_connection(TK_A_ conn, reason);
    }
}

/*
 * Move the connections' stats.data.ptr out into the atomically managed
 * thread-specific aggregate counters.
 */
static void connections_flush_stats(TK_P) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn;
    TAILQ_FOREACH(conn, &largs->open_conns, hook) {
        connection_flush_stats(TK_A_ conn);
    }
}

/*
 * Add whatever data transfer counters we accumulated in a connection
 * back to the worker-wide tally.
 */
static void
connection_flush_stats(TK_P_ struct connection *conn) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    non_atomic_traffic_stats delta =
        subtract_traffic_stats(conn->traffic_ongoing, conn->traffic_reported);
    conn->traffic_reported = conn->traffic_ongoing;
    add_traffic_numbers_NtoA(&delta, &largs->worker_traffic_stats);
}

static void
free_connection_by_handle(tk_io *w) {
    struct connection *conn =
        (struct connection *)((char *)w - offsetof(struct connection, watcher));
    free(conn);
}

/*
 * Free internal structures associated with connection.
 */
static void
connection_free_internals(struct connection *conn) {
    /* Remove sent timestamps ring */
    ring_buffer_free(conn->latency.sent_timestamps);

    /* Remove latency histogram data */
    if(conn->latency.marker_histogram) free(conn->latency.marker_histogram);

    /* Remove Boyer-Moore-Horspool string search context. */
    if(conn->latency.sbmh_marker_ctx) {
        /* Receive side of --latency-marker or --message-marker */
        free(conn->latency.sbmh_marker_ctx);
        if(conn->latency.sbmh_shared == 0) {
            free(conn->latency.sbmh_occ);
            free((void *)conn->latency.sbmh_data);
        }
    }

    /* Remove --message-stop context. */
    if(conn->sbmh_stop_ctx) {
        free(conn->sbmh_stop_ctx);
    }

    message_collection_free(&conn->message_collection);

#ifdef HAVE_OPENSSL
    if(conn->ssl_ctx) {
        SSL_CTX_free(conn->ssl_ctx);
    }
#endif
}

/*
 * Close connection and update connection and data transfer counters.
 */
static void
close_connection(TK_P_ struct connection *conn,
                 enum connection_close_reason reason) {
    char buf[256];
    struct loop_arguments *largs = tk_userdata(TK_A);

    /* Stop I/O and timer notifications */
    tk_io_stop(TK_A, &conn->watcher);
    tk_timer_stop(TK_A, &conn->timer);

    switch(reason) {
    case CCR_LIFETIME:
    case CCR_CLEAN:
        break;
    case CCR_REMOTE:
    case CCR_DATA:
        switch(conn->conn_type) {
        case CONN_OUTGOING:
            /* Make sure we don't go to this address eventually
             * because it is broken. */
            atomic_increment(
                &largs->remote_stats[conn->remote_index].connection_failures);
        case CONN_INCOMING:
        case CONN_ACCEPTOR:
            /* Do not affect counters. */
            break;
        }
        largs->worker_connection_failures++;
        break;
    case CCR_TIMEOUT:
        assert(conn->conn_type == CONN_OUTGOING);
        errno = ETIMEDOUT;
        DEBUG(DBG_NORMAL, "Connection to %s is being closed: %s\n",
              format_sockaddr(
                  &largs->params.remote_addresses.addrs[conn->remote_index],
                  buf, sizeof(buf)),
              strerror(errno));
        largs->worker_connection_failures++;
        largs->worker_connection_timeouts++;
        break;
    }

    /* Propagate connection stats back to the worker */
    connection_flush_stats(TK_A_ conn);

    if(conn->latency.marker_histogram) {
        int64_t n = hdr_add(largs->marker_histogram_local,
                            conn->latency.marker_histogram);
        assert(n == 0);
    }

    /* Maintain a count of opened/closed connections */
    switch(conn->conn_type) {
    case CONN_OUTGOING:
        if(conn->conn_state == CSTATE_CONNECTING)
            atomic_decrement(&largs->outgoing_connecting);
        else
            atomic_decrement(&largs->outgoing_established);
        break;
    case CONN_INCOMING:
        atomic_decrement(&largs->incoming_established);
        break;
    case CONN_ACCEPTOR:
        break;
    }

    TAILQ_REMOVE(&largs->open_conns, conn, hook);

    if(conn->data.ptr && !(conn->data.flags & TDS_FLAG_PTR_SHARED)) {
        free(conn->data.ptr);
    }

    connection_free_internals(conn);

    /* Stop dumping a given connection, if being dumped. */
    if(largs->dump_connect_fd == tk_fd(&conn->watcher)) {
        largs->dump_connect_fd = 0;
    }

    tk_close(&conn->watcher, free_connection_by_handle);
}

/*
 * Determine the amount of parallelism available in this system.
 */
long
number_of_cpus() {
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);

#ifdef HAVE_SCHED_GETAFFINITY
    cpu_set_t cs;
    CPU_ZERO(&cs);
    sched_getaffinity(0, sizeof(cs), &cs);
    ncpus = CPU_COUNT(&cs);
#endif

    return ncpus;
}
