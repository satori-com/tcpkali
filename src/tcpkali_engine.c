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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>    /* for TCP_NODELAY */
#include <unistd.h>
#include <stddef.h> /* offsetof(3) */
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <sys/queue.h>
#include <sysexits.h>
#include <math.h>

#include <config.h>

#ifdef  HAVE_SCHED_H
#include <sched.h>
#endif

#include <StreamBoyerMooreHorspool.h>
#include <hdr_histogram.h>

#include "tcpkali.h"
#include "tcpkali_ring.h"
#include "tcpkali_atomic.h"
#include "tcpkali_events.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_websocket.h"
#include "tcpkali_terminfo.h"
#include "tcpkali_expr.h"
#include "tcpkali_data.h"

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)             \
    for ((var) = TAILQ_FIRST((head));                          \
            (var) && ((tvar) = TAILQ_NEXT((var), field), 1);   \
            (var) = (tvar))
#endif

/*
 * A single connection is described by this structure (about 150 bytes).
 */
struct connection {
    tk_io watcher;
    tk_timer timer;
    off_t write_offset;
    struct transport_data_spec data;
    non_atomic_wide_t data_sent;
    non_atomic_wide_t data_rcvd;
    non_atomic_wide_t data_sent_reported;
    non_atomic_wide_t data_rcvd_reported;
    float channel_eol_point;    /* End of life time, since epoch */
    struct pacefier   bandwidth_pace;
    bandwidth_limit_t bandwidth_limit;
    enum type {
        CONN_OUTGOING,
        CONN_INCOMING,
        CONN_ACCEPTOR,
    } conn_type:2;
    enum state {
        CSTATE_CONNECTED,
        CSTATE_CONNECTING,
    } conn_state:1;
    enum {
        WSTATE_SENDING_HTTP_UPGRADE,
        WSTATE_WS_ESTABLISHED,
    } ws_state:1;
    int16_t remote_index;  /* \x -> loop_arguments.params.remote_addresses.addrs[x] */
    non_atomic_narrow_t connection_unique_id; /* connection.uid */
    TAILQ_ENTRY(connection) hook;
    /* Latency */
    struct {
        struct ring_buffer *sent_timestamps;
        struct hdr_histogram *histogram;
        unsigned message_bytes_credit;  /* See (EXPL:1) below. */
        /* Boyer-Moore-Horspool substring search algorithm data */
        struct StreamBMH     *sbmh_ctx;
        /* The following fields might be shared across connections. */
        int                   sbmh_shared;
        struct StreamBMH_Occ *sbmh_occ;
        const uint8_t        *sbmh_data;
        size_t                sbmh_size;
    } latency;
};

struct loop_arguments {
    struct engine_params params;    /* A copy of engine parameters */
    unsigned int address_offset;    /* An offset into the params.remote_addresses[] */
    struct remote_stats {
        atomic_narrow_t connection_attempts;
        atomic_narrow_t connection_failures;
    } *remote_stats;    /* Per-thread remote server stats */
    tk_timer stats_timer;
    tk_timer channel_lifetime_timer;
    int global_control_pipe_rd_nbio;    /* Non-blocking pipe anyone could read from. */
    int global_feedback_pipe_wr;    /* Blocking pipe for progress reporting. */
    int private_control_pipe_rd;    /* Private blocking pipe for this worker (read side). */
    int private_control_pipe_wr;    /* Private blocking pipe for this worker (write side). */
    int thread_no;
    /* The following atomic members are accessed outside of worker thread */
    atomic_wide_t worker_data_sent;
    atomic_wide_t worker_data_rcvd;
    atomic_narrow_t outgoing_connecting;
    atomic_narrow_t outgoing_established;
    atomic_narrow_t incoming_established;
    atomic_narrow_t connections_counter;
    TAILQ_HEAD( , connection) open_conns;  /* Thread-local connections */
    unsigned long worker_connections_initiated;
    unsigned long worker_connections_accepted;
    unsigned long worker_connection_failures;
    unsigned long worker_connection_timeouts;
    struct hdr_histogram *histogram;
    /* Reporting histogram should not be touched unless asked. */
    struct hdr_histogram *reporting_histogram;
    pthread_mutex_t       reporting_histogram_lock;
    /*
     * Connection identifier counter is shared between all connections
     * across all workers. We don't allocate it per worker, so it points
     * to the same memory in the parameters of all workers.
     */
    atomic_narrow_t *connection_unique_id;
};

/*
 * Types of control messages which might require fair ordering between channels.
 */
enum control_message_type_e {
    CONTROL_MESSAGE_CONNECT,
    _CONTROL_MESSAGES_MAXID     /* Do not use. */
};

/*
 * Engine abstracts over workers.
 */
struct engine {
    struct loop_arguments *loops;
    pthread_t *threads;
    int global_control_pipe_wr;
    int global_feedback_pipe_rd;
    int next_worker_order[_CONTROL_MESSAGES_MAXID];
    int n_workers;
    non_atomic_wide_t total_data_sent;
    non_atomic_wide_t total_data_rcvd;
    atomic_narrow_t connection_unique_id; /* == loop->connection_unique_id */
};

/*
 * Helper functions defined at the end of the file.
 */
enum connection_close_reason {
    CCR_CLEAN,  /* No failure */
    CCR_LIFETIME, /* Channel lifetime limit (no failure) */
    CCR_TIMEOUT, /* Connection timeout */
    CCR_REMOTE, /* Remote side closed connection */
    CCR_DATA,   /* Data framing error */
};
static void *single_engine_loop_thread(void *argp);
static void start_new_connection(TK_P);
static void close_connection(TK_P_ struct connection *conn, enum connection_close_reason reason);
static void connections_flush_stats(TK_P);
static void connection_flush_stats(TK_P_ struct connection *conn);
static void close_all_connections(TK_P_ enum connection_close_reason reason);
static void connection_cb(TK_P_ tk_io *w, int revents);
static void devnull_websocket_cb(TK_P_ tk_io *w, int revents);
static void devnull_cb(TK_P_ tk_io *w, int revents);
static void control_cb(TK_P_ tk_io *w, int revents);
static void accept_cb(TK_P_ tk_io *w, int revents);
static void stats_timer_cb(TK_P_ tk_timer UNUSED *w, int UNUSED revents);
static void conn_timer_cb(TK_P_ tk_timer *w, int revents); /* Timeout timer */
static void expire_channel_lives(TK_P_ tk_timer *w, int revents);
static void setup_channel_lifetime_timer(TK_P_ double first_timeout);
static void update_io_interest(TK_P_ struct connection *conn, int events);
static struct sockaddr *pick_remote_address(struct loop_arguments *largs, size_t *remote_index);
static char *express_bytes(size_t bytes, char *buf, size_t size);
static int limit_channel_lifetime(struct loop_arguments *largs);
static void set_nbio(int fd, int onoff);
static void set_socket_options(int fd, struct loop_arguments *largs);

#ifdef  USE_LIBUV
static void expire_channel_lives_uv(tk_timer *w) {
    expire_channel_lives(w->loop, w, 0);
}
static void stats_timer_cb_uv(tk_timer *w) {
    stats_timer_cb(w->loop, w, 0);
}
static void conn_timer_cb_uv(tk_timer *w) {
    conn_timer_cb(w->loop, w, 0);
}
static void devnull_websocket_cb_uv(tk_io *w, int UNUSED status, int revents) {
    devnull_websocket_cb(w->loop, w, revents);
}
static void connection_cb_uv(tk_io *w, int UNUSED status, int revents) {
    connection_cb(w->loop, w, revents);
}
static void devnull_cb_uv(tk_io *w, int UNUSED status, int revents) {
    devnull_cb(w->loop, w, revents);
}
static void accept_cb_uv(tk_io *w, int UNUSED status, int revents) {
    accept_cb(w->loop, w, revents);
}
static void control_cb_uv(tk_io *w, int UNUSED status, int revents) {
    control_cb(w->loop, w, revents);
}
#endif

/* Note: sizeof(struct sockaddr_in6) > sizeof(struct sockaddr *)! */
static socklen_t sockaddr_len(struct sockaddr *sa) {
    switch(sa->sa_family) {
    case AF_INET: return sizeof(struct sockaddr_in);
    case AF_INET6: return sizeof(struct sockaddr_in6);
    }
    assert(!"Not IPv4 and not IPv6");
    return 0;
}

#define DEBUG(level, fmt, args...) do {         \
        if((int)largs->params.verbosity_level >= level)  \
            fprintf(stderr, "%s" fmt, tcpkali_clear_eol(), ##args);       \
    } while(0)

#define REPLICATE_MAX_SIZE  (64*1024)       /* Proven to be a sweet spot */

struct engine *engine_start(struct engine_params params) {
    int fildes[2];

    /* Global control pipe. Engine -> workers. */
    int rc = pipe(fildes);
    assert(rc == 0);
    int gctl_pipe_rd = fildes[0];
    int gtcl_pipe_wr = fildes[1];
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
     * For efficiency, make sure we concatenate a few data items
     * instead of sending short messages one by one.
     */
    switch(parse_payload_data(&params.data_template,
                              params.verbosity_level >= DBG_DETAIL)) {
    case 0:
        replicate_payload(&params.data_template, REPLICATE_MAX_SIZE);
        break;
    case 1:
        assert((params.data_template.flags & TDS_FLAG_EXPRESSION));
        break;
    default:
        exit(EX_UNAVAILABLE);
    }
    params.epoch = tk_now(TK_DEFAULT);  /* Single epoch for all threads */

    struct engine *eng = calloc(1, sizeof(*eng));
    eng->loops = calloc(n_workers, sizeof(eng->loops[0]));
    eng->threads = calloc(n_workers, sizeof(eng->threads[0]));
    eng->n_workers = n_workers;
    eng->global_control_pipe_wr = gtcl_pipe_wr;
    eng->global_feedback_pipe_rd = gfbk_pipe_rd;

    /*
     * Initialize the Boyer-Moore-Horspool occurrences table once,
     * if it can be shared between connections. It can only be shared
     * if it is trivial (does not depend on dynamic \{expressions}).
     */
    if(params.latency_marker && EXPR_IS_TRIVIAL(params.latency_marker)) {
        sbmh_init(NULL, &params.sbmh_shared_occ,
            (void *)params.latency_marker->u.data.data,
                    params.latency_marker->u.data.size);
    }

    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *largs = &eng->loops[n];
        TAILQ_INIT(&largs->open_conns);
        largs->connection_unique_id = &eng->connection_unique_id;
        largs->params = params;
        largs->remote_stats = calloc(params.remote_addresses.n_addrs, sizeof(largs->remote_stats[0]));
        largs->address_offset = n;
        largs->thread_no = n;
        if(params.latency_marker) {
            int decims_in_1s = 10 * 1000; /* decimilliseconds, 1/10 ms */
            int ret = hdr_init(
                1, /* 1/10 milliseconds is the lowest storable value. */
                100 * decims_in_1s,  /* 100 seconds is a max storable value */
                3, &largs->histogram);
            assert(ret == 0);
            DEBUG(DBG_DETAIL,
                "Initialized HdrHistogram with size %ld\n",
                    (long)hdr_get_memory_size(largs->histogram));
        }
        pthread_mutex_init(&largs->reporting_histogram_lock, 0);

        int private_pipe[2];
        int rc = pipe(private_pipe);
        assert(rc == 0);
        largs->private_control_pipe_rd = private_pipe[0];
        largs->private_control_pipe_wr = private_pipe[1];
        largs->global_control_pipe_rd_nbio = gctl_pipe_rd;
        largs->global_feedback_pipe_wr = gfbk_pipe_wr;

        rc = pthread_create(&eng->threads[n], 0,
                            single_engine_loop_thread, largs);
        assert(rc == 0);
    }

    return eng;
}

/*
 * Send a signal to finish work and wait for all workers to terminate.
 */
void engine_terminate(struct engine *eng, double epoch, non_atomic_wide_t initial_data_sent, non_atomic_wide_t initial_data_rcvd) {
    size_t connecting, conn_in, conn_out, conn_counter;
    struct hdr_histogram *histogram = 0;

    engine_get_connection_stats(eng, &connecting, &conn_in, &conn_out, &conn_counter);

    /* Terminate all threads. */
    for(int n = 0; n < eng->n_workers; n++) {
        int rc = write(eng->loops[n].private_control_pipe_wr, "T", 1);
        assert(rc == 1);
    }
    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *largs = &eng->loops[n];
        void *value;
        pthread_join(eng->threads[n], &value);
        eng->total_data_sent += atomic_wide_get(&largs->worker_data_sent);
        eng->total_data_rcvd += atomic_wide_get(&largs->worker_data_rcvd);
        if(largs->histogram) {
            if(!histogram) {
                int ret;
                ret = hdr_init(largs->histogram->lowest_trackable_value,
                         largs->histogram->highest_trackable_value,
                         largs->histogram->significant_figures,
                         &histogram);
                assert(ret == 0);
            }
            int64_t nret = hdr_add(histogram, largs->histogram);
            assert(nret == 0);
        }
    }
    eng->n_workers = 0;

    /* Data snd/rcv after ramp-up (since epoch) */
    double now = tk_now(TK_DEFAULT);
    double test_duration = now - epoch;
    non_atomic_wide_t epoch_data_sent = eng->total_data_sent-initial_data_sent;
    non_atomic_wide_t epoch_data_rcvd = eng->total_data_rcvd-initial_data_rcvd;
    non_atomic_wide_t epoch_data_transmitted = epoch_data_sent+epoch_data_rcvd;

    char buf[64];

    printf("Total data sent:     %s (%" PRIu64 " bytes)\n",
        express_bytes(epoch_data_sent, buf, sizeof(buf)),
        (uint64_t)epoch_data_sent);
    printf("Total data received: %s (%" PRIu64 " bytes)\n",
        express_bytes(epoch_data_rcvd, buf, sizeof(buf)),
        (uint64_t)epoch_data_rcvd);
    long conns = (0 * connecting) + conn_in + conn_out;
    if(!conns) conns = 1; /* Assume a single channel. */
    printf("Bandwidth per channel: %.3f Mbps, %.1f kBps\n",
        8 * ((epoch_data_transmitted / test_duration) / conns) / 1000000.0,
        (epoch_data_transmitted / test_duration) / conns / 1000.0
    );
    printf("Aggregate bandwidth: %.3f↓, %.3f↑ Mbps\n",
        8 * (epoch_data_rcvd / test_duration) / 1000000.0,
        8 * (epoch_data_sent / test_duration) / 1000000.0);
    if(histogram) {
        printf("Latency at percentiles: %.1f/%.1f/%.1f (95/99/99.5%%)\n",
            hdr_value_at_percentile(histogram, 95.0) / 10.0,
            hdr_value_at_percentile(histogram, 99.0) / 10.0,
            hdr_value_at_percentile(histogram, 99.5) / 10.0);
        printf("Mean and max latencies: %.1f/%.1f (mean/max)\n",
            hdr_mean(histogram) / 10.0,
            hdr_max(histogram) / 10.0);
        free(histogram);
    }
    printf("Test duration: %g s.\n", test_duration);
}

static char *express_bytes(size_t bytes, char *buf, size_t size) {
    if(bytes < 2048) {
        snprintf(buf, size, "%ld bytes", (long)bytes);
    } else if(bytes < 512 * 1024) {
        snprintf(buf, size, "%.1f KiB", (bytes/1024.0));
    } else {
        snprintf(buf, size, "%.1f MiB", (bytes/(1024*1024.0)));
    }
    return buf;
}

/*
 * Get number of connections opened by all of the workers.
 */
void engine_get_connection_stats(struct engine *eng, size_t *connecting, size_t *incoming, size_t *outgoing, size_t *counter) {
    size_t c_conn = 0;
    size_t c_in = 0;
    size_t c_out = 0;
    size_t c_count = 0;

    for(int n = 0; n < eng->n_workers; n++) {
        c_conn  += atomic_get(&eng->loops[n].outgoing_connecting);
        c_out   += atomic_get(&eng->loops[n].outgoing_established);
        c_in    += atomic_get(&eng->loops[n].incoming_established);
        c_count += atomic_get(&eng->loops[n].connections_counter);
    }
    *connecting = c_conn;
    *incoming = c_in;
    *outgoing = c_out;
    *counter = c_count;
}

struct hdr_histogram *engine_get_latency_stats(struct engine *eng) {

    if(!eng->loops[0].histogram)
        return 0;

    struct hdr_histogram *histogram;

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

    /* There's going to be no wait or contention here due
     * to the pipe-driven command-response logic. However,
     * we still lock the reporting histogram to pretend that
     * we correctly deal with memory barriers (which we don't
     * have to on x86).
     */
    pthread_mutex_lock(&eng->loops[0].reporting_histogram_lock);
    int ret = hdr_init(
            eng->loops[0].reporting_histogram->lowest_trackable_value,
            eng->loops[0].reporting_histogram->highest_trackable_value,
            eng->loops[0].reporting_histogram->significant_figures,
            &histogram);
    pthread_mutex_unlock(&eng->loops[0].reporting_histogram_lock);
    assert(ret == 0);

    for(int n = 0; n < eng->n_workers; n++) {
        pthread_mutex_lock(&eng->loops[n].reporting_histogram_lock);
        hdr_add(histogram, eng->loops[n].reporting_histogram);
        pthread_mutex_unlock(&eng->loops[n].reporting_histogram_lock);
    }

    return histogram;
}

void engine_traffic(struct engine *eng, non_atomic_wide_t *sent, non_atomic_wide_t *received) {
    *sent = 0;
    *received = 0;
    for(int n = 0; n < eng->n_workers; n++) {
        *sent += atomic_wide_get(&eng->loops[n].worker_data_sent);
        *received += atomic_wide_get(&eng->loops[n].worker_data_rcvd);
    }
}

/*
 * Enable (1) and disable (0) the non-blocking mode on a file descriptor.
 */
static void set_nbio(int fd, int onoff) {
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

#define SET_XXXBUF(fd, opt, value)  do {                    \
    if((value))                                             \
        set_socket_xxxbuf(fd, opt, #opt, value, largs);     \
    } while(0)
static void set_socket_xxxbuf(int fd, int opt, const char *opt_name, size_t value, struct loop_arguments *largs) {
    int rc = setsockopt(fd, SOL_SOCKET, opt, &value, sizeof(value));
    assert(rc != -1);
    if(largs->params.verbosity_level >= DBG_DETAIL) {
        size_t end_value = value;
        socklen_t end_value_size = sizeof(end_value);
        int rc = getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                            &end_value, &end_value_size);
        assert(rc != -1);
        DEBUG(DBG_DETAIL, "setsockopt(%d, %s, %ld) -> %ld\n",
              fd, opt_name, (long)value, (long)end_value);
    }
}

static void set_socket_options(int fd, struct loop_arguments *largs) {
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

size_t engine_initiate_new_connections(struct engine *eng, size_t n_req) {
    static char buf[1024];  /* This is thread-safe! */
    if(!buf[0]) {
        memset(buf, 'c', sizeof(buf));
    }
    size_t n = 0;

    enum { ATTEMPT_FAIR_BALANCE, FIRST_READER_WINS } balance = ATTEMPT_FAIR_BALANCE;
    if(balance == ATTEMPT_FAIR_BALANCE) {
        while(n < n_req) {
            int worker = eng->next_worker_order[CONTROL_MESSAGE_CONNECT]++ % eng->n_workers;
            int fd = eng->loops[worker].private_control_pipe_wr;
            int wrote = write(fd, buf, 1);
            assert(wrote == 1);
            n++;
        }
    } else {
        int fd = eng->global_control_pipe_wr;
        set_nbio(fd, 1);
        while(n < n_req) {
            int current_batch = (n_req-n) < sizeof(buf) ? (n_req-n) : sizeof(buf);
            int wrote = write(fd, buf, current_batch);
            if(wrote == -1) {
                if(errno == EAGAIN)
                    break;
                assert(wrote != -1);
            }
            if(wrote > 0) n += wrote;
        }
        set_nbio(fd, 0);
    }
    return n;
}

static void expire_channel_lives(TK_P_ tk_timer UNUSED *w, int UNUSED revents) {
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

static void stats_timer_cb(TK_P_ tk_timer UNUSED *w, int UNUSED revents) {
    connections_flush_stats(TK_A);
}

static void *single_engine_loop_thread(void *argp) {
    struct loop_arguments *largs = (struct loop_arguments *)argp;
    tk_loop *loop = tk_loop_new();
    tk_set_userdata(loop, largs);

    tk_io global_control_watcher;
    tk_io private_control_watcher;
    signal(SIGPIPE, SIG_IGN);

    /*
     * Open all listening sockets, if they are specified.
     */
    if(largs->params.listen_addresses.n_addrs) {
        int opened_listening_sockets = 0;
        for(size_t n = 0; n < largs->params.listen_addresses.n_addrs; n++) {
            struct sockaddr *sa = (struct sockaddr *)&largs->params.listen_addresses.addrs[n];
            int rc;
            int lsock = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
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
#endif  /* SO_REUSEPORT */
            rc = bind(lsock, sa, sockaddr_len(sa));
            if(rc == -1) {
                char buf[256];
                DEBUG(DBG_ALWAYS, "Bind %s is not done: %s\n",
                        format_sockaddr(sa, buf, sizeof(buf)),
                        strerror(errno));
                switch(errno) {
                case EINVAL: continue;
                case EADDRINUSE:
#ifndef  SO_REUSEPORT
                    DEBUG(DBG_ALWAYS, "A system without SO_REUSEPORT detected."
                        " Use --workers=1 to avoid binding multiple times.\n");
#endif
                default:
                    exit(EX_UNAVAILABLE);
                }
            }
            assert(rc == 0);
            rc = listen(lsock, 256);
            assert(rc == 0);
            opened_listening_sockets++;

            struct connection *conn = calloc(1, sizeof(*conn));
            conn->conn_type = CONN_ACCEPTOR;
            /* avoid TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook); */
            pacefier_init(&conn->bandwidth_pace, tk_now(TK_A));
#ifdef   USE_LIBUV
            uv_poll_init(TK_A_ &conn->watcher, lsock);
            uv_poll_start(&conn->watcher, TK_READ | TK_WRITE, accept_cb_uv);
#else
            ev_io_init(&conn->watcher, accept_cb, lsock, TK_READ | TK_WRITE);
            ev_io_start(TK_A_ &conn->watcher);
#endif
        }
        if(!opened_listening_sockets) {
            DEBUG(DBG_ALWAYS, "Could not listen on any local sockets!\n");
            exit(EX_UNAVAILABLE);
        }
    }

#ifdef  USE_LIBUV
    if(limit_channel_lifetime(largs)) {
        uv_timer_init(TK_A_ &largs->channel_lifetime_timer);
        uv_timer_start(&largs->channel_lifetime_timer,
            expire_channel_lives_uv, 0, 0);
    }
    uv_timer_init(TK_A_ &largs->stats_timer);
    uv_timer_start(&largs->stats_timer, stats_timer_cb_uv, 250, 250);
    uv_poll_init(TK_A_ &global_control_watcher, largs->global_control_pipe_rd_nbio);
    uv_poll_init(TK_A_ &private_control_watcher, largs->private_control_pipe_rd);
    uv_poll_start(&global_control_watcher, TK_READ, control_cb_uv);
    uv_poll_start(&private_control_watcher, TK_READ, control_cb_uv);
    uv_run(TK_A_ UV_RUN_DEFAULT);
    uv_timer_stop(&largs->stats_timer);
    uv_poll_stop(&global_control_watcher);
    uv_poll_stop(&private_control_watcher);
#else
    if(limit_channel_lifetime(largs)) {
        ev_timer_init(&largs->channel_lifetime_timer,
            expire_channel_lives, 0, 0);
    }
    ev_timer_init(&largs->stats_timer, stats_timer_cb, 0.25, 0.25);
    ev_timer_start(TK_A_ &largs->stats_timer);
    ev_io_init(&global_control_watcher, control_cb, largs->global_control_pipe_rd_nbio, TK_READ);
    ev_io_init(&private_control_watcher, control_cb, largs->private_control_pipe_rd, TK_READ);
    ev_io_start(loop, &global_control_watcher);
    ev_io_start(loop, &private_control_watcher);
    ev_run(loop, 0);
    ev_timer_stop(TK_A_ &largs->stats_timer);
    ev_io_stop(TK_A_ &global_control_watcher);
    ev_io_stop(TK_A_ &private_control_watcher);
#endif

    connections_flush_stats(TK_A);

    close_all_connections(TK_A_ CCR_CLEAN);

    DEBUG(DBG_DETAIL, "Exiting worker %d\n"
            "  %"PRIan"↓, %"PRIan"↑ open connections (%"PRIan" connecting)\n"
            "  %"PRIan" connections_counter \n"
            "  ↳ %lu connections_initiated\n"
            "  ↳ %lu connections_accepted\n"
            "  %lu connection_failures\n"
            "  ↳ %lu connection_timeouts\n"
            "  %"PRIaw" worker_data_sent\n"
            "  %"PRIaw" worker_data_rcvd\n",
        largs->thread_no,
        atomic_get(&largs->incoming_established),
        atomic_get(&largs->outgoing_established),
        atomic_get(&largs->outgoing_connecting),
        atomic_get(&largs->connections_counter),
        largs->worker_connections_initiated,
        largs->worker_connections_accepted,
        largs->worker_connection_failures,
        largs->worker_connection_timeouts,
        atomic_wide_get(&largs->worker_data_sent),
        atomic_wide_get(&largs->worker_data_rcvd));
    if(largs->histogram) {
        DEBUG(DBG_DETAIL,
            "  %.1f latency_95_ms\n"
            "  %.1f latency_99_ms\n"
            "  %.1f latency_99_5_ms\n"
            "  %.1f latency_mean_ms\n"
            "  %.1f latency_max_ms\n",
            hdr_value_at_percentile(largs->histogram, 95.0) / 10.0,
            hdr_value_at_percentile(largs->histogram, 99.0) / 10.0,
            hdr_value_at_percentile(largs->histogram, 99.5) / 10.0,
            hdr_mean(largs->histogram) / 10.0,
            hdr_max(largs->histogram) / 10.0);
        if(largs->params.verbosity_level >= DBG_DATA)
            hdr_percentiles_print(largs->histogram, stderr, 5, 10, CLASSIC);
    }

    return 0;
}

/*
 * Each worker maintains two histogram data structures:
 *  1) the working one, which is not protected by mutex and directly
 *     writable by connections, when they die.
 *  2) the reporting_histogram, which is protected by the mutex and
 *     is only updated from time to time. This one is used for reporting
 *     to external observers.
 */
static void worker_update_reporting_histogram(struct loop_arguments *largs) {
    assert(largs->histogram);
    pthread_mutex_lock(&largs->reporting_histogram_lock);
    if(largs->reporting_histogram) {
        hdr_reset(largs->reporting_histogram);
    } else {
        int ret = hdr_init(largs->histogram->lowest_trackable_value,
                         largs->histogram->highest_trackable_value,
                         largs->histogram->significant_figures,
                         &largs->reporting_histogram);
        assert(ret == 0);
    }
    /*
     * 1. Copy accumulated worker-bound histogram data.
     */
    hdr_add(largs->reporting_histogram, largs->histogram);
    /*
     * 2. There are connections with accumulated data,
     *    process a few of them (all would be expensive)
     *    and add their data as well.
     * FYI: 10 hdr_adds() take ~0.2ms.
     */
    struct connection *conn;
    int nmax = 10;
    TAILQ_FOREACH(conn, &largs->open_conns, hook) {
        if(conn->latency.histogram) {
            /* Only active connections might histograms */
            hdr_add(largs->reporting_histogram, conn->latency.histogram);
            if(--nmax == 0) break;
        }
    }
    pthread_mutex_unlock(&largs->reporting_histogram_lock);
}

/*
 * Receive a control event from the pipe.
 */
static void control_cb(TK_P_ tk_io *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    char c;
    int ret = read(tk_fd(w), &c, 1);
    if(ret != 1) {
        if(errno != EAGAIN)
            DEBUG(DBG_ALWAYS, "%d Reading from control channel %d returned: %s\n",
                largs->thread_no, tk_fd(w), strerror(errno));
        return;
    }
    switch(c) {
    case 'c':
        start_new_connection(TK_A);
        break;
    case 'T':
        tk_stop(TK_A);
        break;
    case 'h':
        worker_update_reporting_histogram(largs);
        int wrote = write(largs->global_feedback_pipe_wr, ".", 1);
        assert(wrote == 1);
        break;
    default:
        DEBUG(DBG_ALWAYS,
            "Unknown operation '%c' from a control channel %d\n",
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
    default:
        s = snprintf(buf, size, "?");
        if(v) *v = 0;
        break;
    }

    if(s < 0 || s > (ssize_t)size)
        return -1;
    return s;
}

static struct transport_data_spec
explode_data_template(struct transport_data_spec *data, struct loop_arguments *largs UNUSED, struct connection *conn) {

    /* If no expressions were given, just copy the template */
    if(!(data->flags & TDS_FLAG_EXPRESSION))
        return *data;

    /*
     * We might need a unique ID for a connection, and it is rather expensive
     * to obtain it. We set it here once during connection establishment.
     */
    conn->connection_unique_id = atomic_inc_and_get(largs->connection_unique_id);

    struct transport_data_spec new_data;
    new_data = *data;
    data = &new_data;

    size_t buf_size =
        data->ws_hdr_size
        + (data->expr_head ? data->expr_head->estimate_size
                            : data->once_size - data->ws_hdr_size)
        + (data->expr_body ? data->expr_body->estimate_size
                            : data->total_size - data->once_size)
    ;
    char *buf = malloc(buf_size + 1);
    char *b = buf;
    size_t offset = 0;
    memcpy(b, data->ptr, data->ws_hdr_size);
    b += data->ws_hdr_size;
    offset += data->ws_hdr_size;

    size_t once_size = data->once_size;
    size_t total_size = data->total_size;

    if(data->expr_head) {
        char *tmp = b;
        ssize_t s = eval_expression(&tmp, buf_size - offset, data->expr_head,
                                    expr_callback, conn, 0);
        assert(s >= 0);
        b += s;
        offset += s;
        once_size = data->ws_hdr_size + s;
    } else {
        memcpy(b,
                (char *)data->ptr + data->ws_hdr_size,
                data->once_size - data->ws_hdr_size);
        b += data->once_size - data->ws_hdr_size;
        offset += data->once_size - data->ws_hdr_size;
    }

    if(data->expr_body) {
        char *tmp = b;
        ssize_t s = eval_expression(&tmp, buf_size - offset, data->expr_body,
                                    expr_callback, conn, 0);
        assert(s >= 0);
        b += s;
        offset += s;
        total_size = once_size + s;
    } else {
        memcpy(b,
                (char *)data->ptr + data->once_size,
                data->total_size - data->once_size);
        b += data->total_size - data->once_size;
        offset += data->total_size - data->once_size;
        total_size = once_size + (data->total_size - data->once_size);
    }
    *b = '\0';

    assert(total_size <= buf_size);
    assert((b - buf) == (ssize_t)total_size);

    data->ptr = buf;
    data->once_size = once_size;
    data->total_size = total_size;
    data->single_message_size = total_size - once_size;
    data->expr_head = 0;
    data->expr_body = 0;
    data->flags &= ~TDS_FLAG_EXPRESSION;

    char tmpbuf[PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(total_size)];

    DEBUG(DBG_DATA, "Connection data: %s\n",
            printable_data(tmpbuf, sizeof(tmpbuf),
                           data->ptr, data->total_size, 1));

    replicate_payload(data, REPLICATE_MAX_SIZE);

    return *data;
}

static void
explode_string_expression(char **buf_p, size_t *size, tk_expr_t *expr, struct loop_arguments *largs UNUSED, struct connection *conn) {
    *buf_p = 0;
    ssize_t s = eval_expression(buf_p, 0, expr, expr_callback, conn, 0);
    assert(s >= 0);
    *size = s;
}

static void start_new_connection(TK_P) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct remote_stats *remote_stats;
    size_t remote_index;

    struct sockaddr *sa = pick_remote_address(largs, &remote_index);
    remote_stats = &largs->remote_stats[remote_index];

    atomic_increment(&largs->connections_counter);
    atomic_increment(&remote_stats->connection_attempts);
    largs->worker_connections_initiated++;

    int sockfd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd == -1) {
        switch(errno) {
        case ENFILE:
            DEBUG(DBG_ERROR, "Cannot create socket, "
                             "consider changing ulimit -n and/or "
                             "kern.maxfiles/fs.file-max sysctls\n");
            exit(1);
        }
        return; /* Come back later */
    } else {
        set_nbio(sockfd, 1);
        set_socket_options(sockfd, largs);
    }

    int conn_state;
    int rc = connect(sockfd, sa, sockaddr_len(sa));
    if(rc == -1) {
        char buf[INET6_ADDRSTRLEN+64];
        switch(errno) {
        case EINPROGRESS:
            break;
        default:
            atomic_increment(&remote_stats->connection_failures);
            largs->worker_connection_failures++;
            if(atomic_get(&remote_stats->connection_failures) == 1) {
                DEBUG(DBG_WARNING, "Connection to %s is not done: %s\n",
                        format_sockaddr(sa, buf, sizeof(buf)), strerror(errno));
            }
            close(sockfd);
            return;
        }

        atomic_increment(&largs->outgoing_connecting);
        conn_state = CSTATE_CONNECTING;
    } else { /* This branch is for completeness only. Should not happen. */
        assert(!"Synchronous connection should not happen");
        if(largs->params.channel_lifetime == 0.0) {
            close(sockfd);
            return;
        }
        atomic_increment(&largs->outgoing_established);
        conn_state = CSTATE_CONNECTED;
    }

    /*
     * Print the src/dst for a connection if verbosity level is high enough.
     */
    if(largs->params.verbosity_level >= DBG_DETAIL) {
        char srcaddr_buf[INET6_ADDRSTRLEN+64];
        char dstaddr_buf[INET6_ADDRSTRLEN+64];
        struct sockaddr_storage srcaddr;
        socklen_t addrlen = sizeof(srcaddr);
        if(getsockname(sockfd, (struct sockaddr *)&srcaddr, &addrlen) == 0) {
            DEBUG(DBG_DETAIL, "Connection %s -> %s opened as %d\n",
                      format_sockaddr((struct sockaddr *)&srcaddr,
                            srcaddr_buf, sizeof(srcaddr_buf)),
                      format_sockaddr(sa, dstaddr_buf, sizeof(dstaddr_buf)),
                      sockfd);
        } else {
            DEBUG(DBG_WARNING, "Can't getsockname(%d): %s",
                               sockfd, strerror(errno));
        }
    }

    double now = tk_now(TK_A);

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->conn_type = CONN_OUTGOING;
    conn->conn_state = conn_state;
    if(limit_channel_lifetime(largs)) {
        if(TAILQ_FIRST(&largs->open_conns) == NULL) {
            setup_channel_lifetime_timer(TK_A_ largs->params.channel_lifetime);
        }
        conn->channel_eol_point =
            (now - largs->params.epoch) + largs->params.channel_lifetime;
    }
    TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook);
    pacefier_init(&conn->bandwidth_pace, now);
    conn->remote_index = remote_index;

    conn->data = explode_data_template(&largs->params.data_template, largs, conn);
    conn->bandwidth_limit = compute_bandwidth_limit(
                                largs->params.channel_send_rate,
                                conn->data.single_message_size);

    if(largs->params.latency_marker && conn->data.single_message_size) {
        conn->latency.message_bytes_credit  /* See (EXPL:1) below. */
            = conn->data.single_message_size - 1;

        /*
         * Initialize the Boyer-Moore-Horspool context for substring search.
         */
        struct StreamBMH_Occ *init_occ = NULL;
        if(EXPR_IS_TRIVIAL(largs->params.latency_marker)) {
            /* Shared search table and expression */
            conn->latency.sbmh_shared = 1;
            conn->latency.sbmh_occ  = &largs->params.sbmh_shared_occ;
            conn->latency.sbmh_data = (uint8_t *)largs->params.latency_marker->u.data.data;
            conn->latency.sbmh_size = largs->params.latency_marker->u.data.size;
        } else {
            /* Individual search table. */
            conn->latency.sbmh_shared = 0;
            conn->latency.sbmh_occ = malloc(sizeof(*conn->latency.sbmh_occ));
            assert(conn->latency.sbmh_occ);
            init_occ = conn->latency.sbmh_occ;
            explode_string_expression((char **)&conn->latency.sbmh_data,
                                      &conn->latency.sbmh_size,
                                      largs->params.latency_marker,
                                      largs, conn);
        }
        conn->latency.sbmh_ctx = malloc(SBMH_SIZE(conn->latency.sbmh_size));
        assert(conn->latency.sbmh_ctx);
        sbmh_init(conn->latency.sbmh_ctx, init_occ,
                  conn->latency.sbmh_data, conn->latency.sbmh_size);

        /*
         * Initialize the latency histogram by copying out the template
         * parameter from the loop arguments.
         */
        conn->latency.sent_timestamps = ring_buffer_new(sizeof(double));
        int ret = hdr_init(largs->histogram->lowest_trackable_value,
                         largs->histogram->highest_trackable_value,
                         largs->histogram->significant_figures,
                         &conn->latency.histogram);
        assert(ret == 0);
    }

    int want_catch_connect = (conn_state == CSTATE_CONNECTING
                    && largs->params.connect_timeout > 0.0);
    if(want_catch_connect) {
#ifdef  USE_LIBUV
        uv_timer_init(TK_A_ &conn->timer);
        uint64_t delay = 1000 * largs->params.connect_timeout;
        if(delay == 0) delay = 1;
        uv_timer_start(&conn->timer, conn_timer_cb_uv, delay, 0);
#else
        ev_timer_init(&conn->timer, conn_timer_cb,
                      largs->params.connect_timeout, 0.0);
        ev_timer_start(TK_A_ &conn->timer);
#endif
    }

    int want_write = (conn->data.total_size || want_catch_connect);
    int want_events = TK_READ | (want_write ? TK_WRITE : 0);
#ifdef  USE_LIBUV
    uv_poll_init(TK_A_ &conn->watcher, sockfd);
    uv_poll_start(&conn->watcher, want_events, connection_cb_uv);
#else
    ev_io_init(&conn->watcher, connection_cb, sockfd, want_events);
    ev_io_start(TK_A_ &conn->watcher);
#endif
}

/*
 * Pick an address in a round-robin fashion, skipping certainly broken ones.
 */
static struct sockaddr *pick_remote_address(struct loop_arguments *largs, size_t *remote_index) {

    /*
     * If it is known that a particular destination is broken, choose
     * the working one right away.
     */
    size_t off = 0;
    for(size_t attempts = 0; attempts < largs->params.remote_addresses.n_addrs; attempts++) {
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
    return (struct sockaddr *)&largs->params.remote_addresses.addrs[off];
}

static void conn_timer_cb(TK_P_ tk_timer *w, int UNUSED revents) {
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, timer));

    switch(conn->conn_state) {
    case CSTATE_CONNECTED:
        switch(conn->conn_type) {
        case CONN_INCOMING:
            update_io_interest(TK_A_ conn, TK_READ);
            break;
        case CONN_OUTGOING:
            update_io_interest(TK_A_ conn,
                TK_READ | (conn->data.total_size ? TK_WRITE : 0));
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
static int limit_channel_lifetime(struct loop_arguments *largs) {
    return (largs->params.channel_lifetime != INFINITY
                && largs->params.channel_lifetime > 0.0);
}

static void setup_channel_lifetime_timer(TK_P_ double first_timeout) {
    struct loop_arguments *largs = tk_userdata(TK_A);
#ifdef  USE_LIBUV
    uint64_t delay = 1000 * first_timeout;
    if(delay == 0) delay = 1;
    uv_timer_start(&largs->channel_lifetime_timer,
        expire_channel_lives_uv, delay, 0);
#else
    ev_timer_stop(TK_A_ &largs->channel_lifetime_timer);
    ev_timer_set(&largs->channel_lifetime_timer, first_timeout, 0);
    ev_timer_start(TK_A_ &largs->channel_lifetime_timer);
#endif
}

static void accept_cb(TK_P_ tk_io *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);

    int sockfd = accept(tk_fd(w), 0, 0);
    if(sockfd == -1)
        return;
    set_nbio(sockfd, 1);
    set_socket_options(sockfd, largs);

    atomic_increment(&largs->connections_counter);

    /* If channel lifetime is 0, close it right away. */
    if(largs->params.channel_lifetime == 0.0) {
        largs->worker_connections_accepted++;
        close(sockfd);
        return;
    }

    atomic_increment(&largs->incoming_established);

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->conn_type = CONN_INCOMING;
    if(limit_channel_lifetime(largs)) {
        if(TAILQ_FIRST(&largs->open_conns) == NULL) {
            setup_channel_lifetime_timer(TK_A_ largs->params.channel_lifetime);
        }
        double now = tk_now(TK_A);
        conn->channel_eol_point =
            (now - largs->params.epoch) + largs->params.channel_lifetime;
    }
    TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook);
    largs->worker_connections_accepted++;

#ifdef  USE_LIBUV
    void (*responder_callback)(tk_io *w, int status, int revents);
    if(largs->params.websocket_enable)
        responder_callback = devnull_websocket_cb_uv;
    else
        responder_callback = devnull_cb_uv;
    uv_poll_init(TK_A_ &conn->watcher, sockfd);
    uv_poll_start(&conn->watcher, TK_READ, responder_callback);
#else
    void (*responder_callback)(TK_P_ tk_io *w, int revents);
    if(largs->params.websocket_enable)
        responder_callback = devnull_websocket_cb;
    else
        responder_callback = devnull_cb;
    ev_io_init(&conn->watcher, responder_callback, sockfd, TK_READ);
    ev_io_start(TK_A_ &conn->watcher);
#endif
}

/*
 * Debug data by dumping it in a format escaping all the special
 * characters.
 */
static void debug_dump_data(const char *prefix, const void *data, size_t size) {
    char buffer[PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(size)];
    fprintf(stderr, "%s%s(%ld): ➧%s⬅︎\n",
            tcpkali_clear_eol(), prefix, (long)size,
            printable_data(buffer, sizeof(buffer), data, size, 0));
}

static enum {
    LB_UNLIMITED,   /* Not limiting bandwidth, proceed. */
    LB_PROCEED,     /* Use pacefier_moved() afterwards. */
    LB_GO_SLEEP,    /* Not allowed to move data.        */
} limit_channel_bandwidth(TK_P_ struct connection *conn,
                          size_t *suggested_move_size) {
    double bw = conn->bandwidth_limit.bytes_per_second;
    if(bw == 0.0) {
        return LB_UNLIMITED;    /* Limit not set, don't limit. */
    }

    size_t smallest_block_to_move = conn->bandwidth_limit.minimal_move_size;
    size_t allowed_to_move = pacefier_allow(&conn->bandwidth_pace, bw, tk_now(TK_A));

    if(allowed_to_move < smallest_block_to_move) {
        /*     allowed     smallest_blk
           |------^-------------^-------> */
        double delay = (double)(smallest_block_to_move-allowed_to_move)/bw;
        if(delay < 0.001) delay = 0.001;

        tk_timer_stop(TK_A, &conn->timer);
#ifdef  USE_LIBUV
        uv_timer_init(TK_A_ &conn->timer);
        uv_timer_start(&conn->timer, conn_timer_cb_uv, 1000 * delay, 0.0);
#else
        ev_timer_init(&conn->timer, conn_timer_cb, delay, 0.0);
        ev_timer_start(TK_A_ &conn->timer);
#endif
        *suggested_move_size = 0;
        return LB_GO_SLEEP;
    } else {
        /*   smallest_blk  allowed
           |------^-----------^----> */
        if(*suggested_move_size > allowed_to_move) {
            /*   smallest_blk  allowed   suggested
               |------^-----------^----------^-------> */
            *suggested_move_size = allowed_to_move
                        - (allowed_to_move % smallest_block_to_move);
        }
        return LB_PROCEED;
    }
}

static void devnull_cb(TK_P_ tk_io *w, int revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));

    if(revents & TK_READ) {
        char buf[16384];
        for(;;) {
            size_t recv_size = sizeof(buf);
            int record_moved = 0;
            switch(limit_channel_bandwidth(TK_A_ conn, &recv_size)) {
            case LB_UNLIMITED: record_moved = 0; break;
            case LB_PROCEED:   record_moved = 1; break;
            case LB_GO_SLEEP:
                update_io_interest(TK_A_ conn, 0);
                return;
            }

            ssize_t rd = read(tk_fd(w), buf, recv_size);
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
                if(record_moved)
                    pacefier_moved(&conn->bandwidth_pace,
                                   conn->bandwidth_limit.bytes_per_second,
                                   rd, tk_now(TK_A));
                conn->data_rcvd += rd;
                if(largs->params.verbosity_level >= DBG_DATA) {
                    debug_dump_data("Data", buf, rd);
                }
                break;
            }
        }
    }
}

static void devnull_websocket_cb(TK_P_ tk_io *w, int revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));

    if(revents & TK_READ) {
        char buf[16384];
        for(;;) {
            ssize_t rd = read(tk_fd(w), buf, sizeof(buf));
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
                conn->data_rcvd += rd;
                if(largs->params.verbosity_level >= DBG_DATA) {
                    debug_dump_data("Data", buf, rd);
                }

                /*
                 * Attempt to detect websocket key in HTTP and respond.
                 */
                switch (http_detect_websocket(tk_fd(w), buf, rd)) {
                case HDW_NOT_ENOUGH_DATA: return;
                case HDW_WEBSOCKET_DETECTED: break;
                case HDW_TRUNCATED_INPUT:
                case HDW_UNEXPECTED_ERROR:
                    close_connection(TK_A_ conn, CCR_DATA);
                    return;
                }

                /* Do a proper /dev/null from now on */
#ifdef  USE_LIBUV
                uv_poll_start(w, TK_READ, devnull_cb_uv);
#else
                ev_io_stop(TK_A_ w);
                ev_io_init(w, devnull_cb, tk_fd(w), TK_READ);
                ev_io_start(TK_A_ w);
#endif
                break;
            }
        }
    }
}

static void update_io_interest(TK_P_ struct connection *conn, int events) {
#ifdef  USE_LIBUV
    (void)loop;
    uv_poll_start(&conn->watcher, events, conn->watcher.poll_cb);
#else
    ev_io_stop(TK_A_ &conn->watcher);
    ev_io_set(&conn->watcher, conn->watcher.fd, events);
    ev_io_start(TK_A_ &conn->watcher);
#endif
}

static void latency_record_outgoing_ts(TK_P_ struct connection *conn, size_t wrote) {
    if(!conn->latency.sent_timestamps)
        return;

    struct loop_arguments *largs = tk_userdata(TK_A);

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
            DEBUG(DBG_ERROR,
                "Sending messages too fast, "
                    "not receiving them back fast enough.\n"
                "Check that the --latency-marker data is being received back.\n"
                "Use --verbose 3 to dump received message data.\n");
            exit(1);
        }
    }
}

static void latency_record_incoming_ts(TK_P_ struct connection *conn, char *buf, size_t size) {

    if(!conn->latency.sent_timestamps)
        return;

    const uint8_t *lm = conn->latency.sbmh_data;
    size_t   lm_size  = conn->latency.sbmh_size;
    int num_markers_found = 0;

    for(; size > 0; ) {
        size_t analyzed = sbmh_feed(conn->latency.sbmh_ctx,
                                    conn->latency.sbmh_occ,
                                    lm, lm_size, (unsigned char *)buf, size);
        if(conn->latency.sbmh_ctx->found == sbmh_true) {
            buf += analyzed;
            size -= analyzed;
            num_markers_found++;
            sbmh_reset(conn->latency.sbmh_ctx);
        } else {
            break;
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
            if(hdr_record_value(conn->latency.histogram, latency) == false) {
                fprintf(stderr, "Latency value %g is too large, "
                                "can't record.\n", now - ts);
            }
        } else {
            fprintf(stderr, "More messages received than sent. "
                        "Choose a different --latency-marker.\n"
                        "Use --verbose 3 to dump received message data.\n");
            exit(1);
        }
    }

}

/*
 * Compute the largest amount of data we can send to the channel
 * using a single write() call.
 */
static void largest_contiguous_chunk(struct loop_arguments *largs, struct connection *conn, const void **position, size_t *available_header, size_t *available_body) {
    off_t *current_offset = &conn->write_offset;
    size_t accessible_size = conn->data.total_size;
    size_t available = accessible_size - *current_offset;

    /* The first bunch of bytes sent on the WebSocket connection
     * should be limited by the HTTP upgrade headers.
     * We then wait for the server reply.
     */
    if(conn->data_rcvd == 0
            && conn->data_sent <= conn->data.ws_hdr_size
            && conn->ws_state == WSTATE_SENDING_HTTP_UPGRADE
            && largs->params.websocket_enable) {
        accessible_size = conn->data.ws_hdr_size;
        size_t available = accessible_size - *current_offset;
        *position = conn->data.ptr + *current_offset;
        *available_header = available;
        *available_body = 0;
        return;
    }

    if(conn->data_sent < conn->data.once_size) {
        /* Send header... once per connection lifetime */
        *available_header = conn->data.once_size - conn->data_sent;
        assert(available);
    } else {
        *available_header = 0;    /* Sending body */
    }

    if(available) {
        *position = conn->data.ptr + *current_offset;
        *available_body = available - *available_header;
    } else {
        size_t off = conn->data.once_size;
        *position = conn->data.ptr + off;
        *available_body = accessible_size - off;
        *current_offset = off;
    }
}

static void connection_cb(TK_P_ tk_io *w, int revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));
    struct sockaddr *remote = (struct sockaddr *)&largs->params.remote_addresses.addrs[conn->remote_index];

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

        /*
         * We were asked to produce the WRITE event
         * only to detect successful connection.
         * If there's nothing to write, we remove the write interest.
         */
        tk_timer_stop(TK_A, &conn->timer);
        if(conn->data.total_size == 0) {
            update_io_interest(TK_A_ conn, TK_READ); /* no write interest */
            revents &= ~TK_WRITE;   /* Don't actually write in this loop */
        }
    }

    if(revents & TK_READ) {
        char buf[16384];
        for(;;) {
            ssize_t rd = read(tk_fd(w), buf, sizeof(buf));
            switch(rd) {
            case -1:
                switch(errno) {
                case EAGAIN:
                    return;
                default:
                    DEBUG(DBG_NORMAL, "Closing %s: %s\n",
                        format_sockaddr(remote, buf, sizeof(buf)),
                        strerror(errno));
                    close_connection(TK_A_ conn, CCR_REMOTE);
                    return;
                }
                /* Fall through */
            case 0:
                DEBUG(DBG_DETAIL, "Connection half-closed by %s\n",
                    format_sockaddr(remote, buf, sizeof(buf)));
                close_connection(TK_A_ conn, CCR_REMOTE);
                return;
            default:
                /*
                 * If this is a first packet from the remote server,
                 * and we're in a WebSocket mode where we should wait for
                 * the server response before sending rest, unblock our WRITE
                 * side.
                 */
                if(conn->data_rcvd == 0
                && conn->ws_state == WSTATE_SENDING_HTTP_UPGRADE
                && largs->params.websocket_enable
                && conn->data.ws_hdr_size
                        != conn->data.total_size) {
                    conn->ws_state = WSTATE_WS_ESTABLISHED;
                    update_io_interest(TK_A_ conn, TK_READ | TK_WRITE);
                }
                conn->data_rcvd += rd;
                if(largs->params.verbosity_level >= DBG_DATA) {
                    debug_dump_data("Data", buf, rd);
                }
                latency_record_incoming_ts(TK_A_ conn, buf, rd);
                break;
            }
        }
    }

    if(revents & TK_WRITE) {
        const void *position;
        size_t available_header, available_body;
        int record_moved = 0;

        largest_contiguous_chunk(largs, conn, &position, &available_header, &available_body);
        if(!(available_header + available_body)) {
            /* Only the header was sent. Now, silence. */
            assert(conn->data.total_size == conn->data.once_size
                || largs->params.websocket_enable);
            update_io_interest(TK_A_ conn, TK_READ); /* no write interest */
            return;
        }

        /* Adjust (available_body) to avoid sending too much stuff. */
        switch(limit_channel_bandwidth(TK_A_ conn, &available_body)) {
        case LB_UNLIMITED: record_moved = 0; break;
        case LB_PROCEED:   record_moved = 1; break;
        case LB_GO_SLEEP:
            if(available_header)
                break;
            update_io_interest(TK_A_ conn, TK_READ);
            return;
        }

        ssize_t wrote = write(tk_fd(w), position,
                              available_header + available_body);
        if(wrote == -1) {
            char buf[INET6_ADDRSTRLEN+64];
            switch(errno) {
            case EINTR:
            case EAGAIN:
                break;
            case EPIPE:
            default:
                DEBUG(DBG_WARNING, "Connection reset by %s\n",
                    format_sockaddr(remote, buf, sizeof(buf)));
                close_connection(TK_A_ conn, CCR_REMOTE);
                return;
            }
        } else {
            conn->write_offset += wrote;
            conn->data_sent += wrote;
            if(record_moved)
                pacefier_moved(&conn->bandwidth_pace,
                               conn->bandwidth_limit.bytes_per_second,
                               wrote, tk_now(TK_A));
            wrote -= available_header;
            if(wrote > 0) {
                /* Record latencies for the body only, not headers */
                latency_record_outgoing_ts(TK_A_ conn, wrote);
            }
        }
    }

}

/*
 * Ungracefully close all connections and report accumulated stats
 * back to the central loop structure.
 */
static void close_all_connections(TK_P_ enum connection_close_reason reason) {
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
static void connection_flush_stats(TK_P_ struct connection *conn) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    size_t sent_delta = conn->data_sent - conn->data_sent_reported;
    size_t rcvd_delta = conn->data_rcvd - conn->data_rcvd_reported;
    conn->data_sent_reported = conn->data_sent;
    conn->data_rcvd_reported = conn->data_rcvd;
    atomic_add(&largs->worker_data_sent, sent_delta);
    atomic_add(&largs->worker_data_rcvd, rcvd_delta);
}

static void free_connection_by_handle(tk_io *w) {
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));
    free(conn);
}

/*
 * Free internal structures associated with connection.
 */
static void connection_free_internals(struct connection *conn) {
    /* Remove sent timestamps ring */
    ring_buffer_free(conn->latency.sent_timestamps);

    /* Remove latency histogram data */
    if(conn->latency.histogram)
        free(conn->latency.histogram);

    /* Remove Boyer-Moore-Horspool string search context. */
    if(conn->latency.sbmh_ctx) {
        free(conn->latency.sbmh_ctx);
        if(conn->latency.sbmh_shared == 0) {
            free(conn->latency.sbmh_occ);
            free((void *)conn->latency.sbmh_data);
        }
    }

}

/*
 * Close connection and update connection and data transfer counters.
 */
static void close_connection(TK_P_ struct connection *conn, enum connection_close_reason reason) {
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
            atomic_increment(&largs->remote_stats[conn->remote_index].connection_failures);
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
                format_sockaddr((struct sockaddr *)&largs->params
                    .remote_addresses.addrs[conn->remote_index],
                    buf, sizeof(buf)),
                strerror(errno));
        largs->worker_connection_failures++;
        largs->worker_connection_timeouts++;
        break;
    }

    /* Propagate connection stats back to the worker */
    connection_flush_stats(TK_A_ conn);

    if(conn->latency.histogram) {
        int64_t n = hdr_add(largs->histogram, conn->latency.histogram);
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

    if(conn->data.ptr != largs->params.data_template.ptr)
        free(conn->data.ptr);

    connection_free_internals(conn);

    tk_close(&conn->watcher, free_connection_by_handle);
}

/*
 * Determine the amount of parallelism available in this system.
 */
long number_of_cpus() {
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);

#ifdef   HAVE_SCHED_GETAFFINITY
    cpu_set_t cs;
    CPU_ZERO(&cs);
    sched_getaffinity(0, sizeof(cs), &cs);
    ncpus = CPU_COUNT(&cs);
#endif

    return ncpus;
}
