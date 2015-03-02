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
#include <inttypes.h>
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

#include "hdr_histogram.h"

#include "tcpkali.h"
#include "tcpkali_ring.h"
#include "tcpkali_atomic.h"
#include "tcpkali_events.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_websocket.h"

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
    struct pacefier bw_pace;
    off_t write_offset;
    atomic_wide_t data_sent;
    atomic_wide_t data_received;
    float channel_eol_point;    /* End of life time, since epoch */
    enum type {
        CONN_OUTGOING,
        CONN_INCOMING,
        CONN_ACCEPTOR,
    } conn_type:8;
    enum state {
        CSTATE_CONNECTED,
        CSTATE_CONNECTING,
    } conn_state:8;
    int16_t remote_index;  /* \x -> loop_arguments.params.remote_addresses.addrs[x] */
    TAILQ_ENTRY(connection) hook;
    /* Latency */
    struct {
        struct ring_buffer *sent_timestamps;
        struct hdr_histogram *histogram;
        unsigned incomplete_message_bytes_sent;
        /* Number of initial bytes of the marker which match the tail
         * of the incomplete message. */
        unsigned marker_match_prefix;
    } latency;
};

struct loop_arguments {
    struct engine_params params;    /* A copy of engine parameters */
    unsigned int address_offset;    /* An offset into the params.remote_addresses[] */
    struct remote_stats {
        atomic_t connection_attempts;
        atomic_t connection_failures;
    } *remote_stats;    /* Per-thread remote server stats */
    tk_timer stats_timer;
    tk_timer channel_lifetime_timer;
    int global_control_pipe_rd_nbio;    /* Non-blocking pipe anyone could read from. */
    int global_feedback_pipe_wr;    /* Blocking pipe for progress reporting. */
    int private_control_pipe_rd;    /* Private blocking pipe for this worker (read side). */
    int private_control_pipe_wr;    /* Private blocking pipe for this worker (write side). */
    int thread_no;
    /* The following atomic members are accessed outside of worker thread */
    atomic_wide_t worker_data_sent      __attribute__((aligned(sizeof(atomic_wide_t))));
    atomic_wide_t worker_data_received  __attribute__((aligned(sizeof(atomic_wide_t))));
    atomic_t outgoing_connecting;
    atomic_t outgoing_established;
    atomic_t incoming_established;
    atomic_t connections_counter;
    TAILQ_HEAD( , connection) open_conns;  /* Thread-local connections */
    unsigned long worker_connections_initiated;
    unsigned long worker_connections_accepted;
    unsigned long worker_connection_failures;
    unsigned long worker_connection_timeouts;
    struct hdr_histogram *histogram;
    /* Reporting histogram should not be touched unless asked. */
    struct hdr_histogram *reporting_histogram;
    pthread_mutex_t       reporting_histogram_lock;
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
    atomic_wide_t total_data_sent;
    atomic_wide_t total_data_received;
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
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length);
static char *express_bytes(size_t bytes, char *buf, size_t size);
static int limit_channel_lifetime(struct loop_arguments *largs);
static void set_nbio(int fd, int onoff);

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
            fprintf(stderr, fmt, ##args);       \
    } while(0)

struct engine *engine_start(struct engine_params params) {
    int fildes[2];

    /* Check that worker_data_sent is properly aligned for atomicity */
    assert(((long)(&((struct loop_arguments *)0)->worker_data_sent) & 7) == 0);

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
    replicate_payload(&params.data, 64*1024);
    if(params.minimal_move_size == 0)
        params.minimal_move_size = 1460; /* ~MTU */
    params.epoch = tk_now(TK_DEFAULT);  /* Single epoch for all threads */

    struct engine *eng = calloc(1, sizeof(*eng));
    eng->loops = calloc(n_workers, sizeof(eng->loops[0]));
    eng->threads = calloc(n_workers, sizeof(eng->threads[0]));
    eng->n_workers = n_workers;
    eng->global_control_pipe_wr = gtcl_pipe_wr;
    eng->global_feedback_pipe_rd = gfbk_pipe_rd;

    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *largs = &eng->loops[n];
        TAILQ_INIT(&largs->open_conns);
        largs->params = params;
        largs->remote_stats = calloc(params.remote_addresses.n_addrs, sizeof(largs->remote_stats[0]));
        largs->address_offset = n;
        largs->thread_no = n;
        if(params.latency_marker_data) {
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
void engine_terminate(struct engine *eng, double epoch, atomic_wide_t initial_data_sent, atomic_wide_t initial_data_received) {
    size_t connecting, conn_in, conn_out, conn_counter;
    struct hdr_histogram *histogram = 0;

    engine_get_connection_stats(eng, &connecting, &conn_in, &conn_out, &conn_counter);

    /* Terminate all threads. */
    for(int n = 0; n < eng->n_workers; n++) {
        int rc = write(eng->loops[n].private_control_pipe_wr, "T", 1);
        assert(rc == 1);
    }
    for(int n = 0; n < eng->n_workers; n++) {
        void *value;
        pthread_join(eng->threads[n], &value);
        eng->total_data_sent +=
            eng->loops[n].worker_data_sent;
        eng->total_data_received +=
            eng->loops[n].worker_data_received;
        if(eng->loops[n].histogram) {
            if(!histogram) {
                int ret;
                ret = hdr_init(eng->loops[n].histogram->lowest_trackable_value,
                         eng->loops[n].histogram->highest_trackable_value,
                         eng->loops[n].histogram->significant_figures,
                         &histogram);
                assert(ret == 0);
            }
            int64_t nret = hdr_add(histogram, eng->loops[n].histogram);
            assert(nret == 0);
        }
    }
    eng->n_workers = 0;

    /* Data snd/rcv after ramp-up (since epoch) */
    double now = tk_now(TK_DEFAULT);
    double test_duration = now - epoch;
    atomic_wide_t epoch_data_sent     = eng->total_data_sent   - initial_data_sent;
    atomic_wide_t epoch_data_received = eng->total_data_received-initial_data_received;
    atomic_wide_t epoch_data_transmitted = epoch_data_sent + epoch_data_received;

    char buf[64];

    printf("Total data sent:     %s (%" PRIu64 " bytes)\n",
        express_bytes(epoch_data_sent, buf, sizeof(buf)),
        (uint64_t)epoch_data_sent);
    printf("Total data received: %s (%" PRIu64 " bytes)\n",
        express_bytes(epoch_data_received, buf, sizeof(buf)),
        (uint64_t)epoch_data_received);
    long conns = (0 * connecting) + conn_in + conn_out;
    if(!conns) conns = 1; /* Assume a single channel. */
    printf("Bandwidth per channel: %.3f Mbps, %.1f kBps\n",
        8 * ((epoch_data_transmitted / test_duration) / conns) / 1000000.0,
        (epoch_data_transmitted / test_duration) / conns / 1000.0
    );
    printf("Aggregate bandwidth: %.3f↓, %.3f↑ Mbps\n",
        8 * (epoch_data_received / test_duration) / 1000000.0,
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
        c_conn  += eng->loops[n].outgoing_connecting;
        c_out   += eng->loops[n].outgoing_established;
        c_in    += eng->loops[n].incoming_established;
        c_count += eng->loops[n].connections_counter;
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

void engine_traffic(struct engine *eng, atomic_wide_t *sent, atomic_wide_t *received) {
    *sent = 0;
    *received = 0;
    for(int n = 0; n < eng->n_workers; n++) {
        *sent += atomic_wide_get(&eng->loops[n].worker_data_sent);
        *received += atomic_wide_get(&eng->loops[n].worker_data_received);
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
    struct loop_arguments *largs = tk_userdata(TK_A);

    /*
     * Move the connections' stats.data.ptr out into the atomically managed
     * thread-specific aggregate counters.
     */
    struct connection *conn;
    TAILQ_FOREACH(conn, &largs->open_conns, hook) {
        atomic_add(&largs->worker_data_sent, conn->data_sent);
        atomic_add(&largs->worker_data_received, conn->data_received);
        conn->data_sent = 0;
        conn->data_received = 0;
    }
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
            pacefier_init(&conn->bw_pace, tk_now(TK_A));
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
            "  %u↓, %u↑ open connections (%u connecting)\n"
            "  %u connections_counter \n"
            "  ↳ %lu connections_initiated\n"
            "  ↳ %lu connections_accepted\n"
            "  %lu connection_failures\n"
            "  ↳ %lu connection_timeouts\n"
            "  %lu worker_data_sent\n"
            "  %lu worker_data_received\n",
        largs->thread_no,
        largs->incoming_established,
        largs->outgoing_established,
        largs->outgoing_connecting,
        largs->connections_counter,
        largs->worker_connections_initiated,
        largs->worker_connections_accepted,
        largs->worker_connection_failures,
        largs->worker_connection_timeouts,
        (unsigned long)largs->worker_data_sent,
        (unsigned long)largs->worker_data_received);
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
            DEBUG(DBG_ERROR, "Cannot create socket, consider changing ulimit -n and/or kern.maxfiles/fs.file-max sysctls\n");
            exit(1);
        }
        return; /* Come back later */
    } else {
        set_nbio(sockfd, 1);
        int on = ~0;
        int rc = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
        assert(rc != -1);
        if(largs->params.nagle_setting != NSET_UNSET) {
            int v = largs->params.nagle_setting;
            rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
            assert(rc != -1);
        }
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
            if(remote_stats->connection_failures == 1) {
                DEBUG(DBG_ERROR, "Connection to %s is not done: %s\n",
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
    pacefier_init(&conn->bw_pace, now);
    conn->remote_index = remote_index;

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

    if(largs->params.latency_marker_data
    && largs->params.data.single_message_size) {
        memset(&conn->latency, 0, sizeof(conn->latency));
        conn->latency.sent_timestamps = ring_buffer_new(sizeof(double));
        int ret = hdr_init(largs->histogram->lowest_trackable_value,
                         largs->histogram->highest_trackable_value,
                         largs->histogram->significant_figures,
                         &conn->latency.histogram);
        assert(ret == 0);
    }

    int want_write = (largs->params.data.total_size || want_catch_connect);
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
        if(rs->connection_attempts > 10
            && rs->connection_failures == rs->connection_attempts) {
            continue;
        } else {
            break;
        }
    }

    *remote_index = off;
    return (struct sockaddr *)&largs->params.remote_addresses.addrs[off];
}

static void conn_timer_cb(TK_P_ tk_timer *w, int UNUSED revents) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, timer));

    switch(conn->conn_state) {
    case CSTATE_CONNECTED:
        switch(conn->conn_type) {
        case CONN_INCOMING:
            update_io_interest(TK_A_ conn, TK_READ);
            break;
        case CONN_OUTGOING:
            update_io_interest(TK_A_ conn,
                TK_READ | (largs->params.data.total_size ? TK_WRITE : 0));
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
void debug_dump_data(const void *data, size_t size) {
    const int blowup_factor = 4; /* Each character expands by 4, max. */
    char buffer[blowup_factor * size + 1];
    const unsigned char *p = data;
    const unsigned char *pend = p + size;
    char *b;
    for(b = buffer; p < pend; p++) {
        switch(*p) {
        case '\r': *b++ = '\\'; *b++ = 'r'; break;
        case '\n': *b++ = '\\'; *b++ = 'n';
            if(p+1 == pend) break;
            *b++ = '\n'; *b++ = '\t'; break;
        case 32 ... 126: *b++ = *p; break;
        default:
            b += snprintf(b, sizeof(buffer) - (b - buffer), "\\%03o", *p);
            break;
        }
    }
    *b++ = '\0';
    assert((size_t)(b - buffer) <= sizeof(buffer));
    fprintf(stderr, "\033[KData(%ld): ➧%s⬅︎\n", (long)size, buffer);
}

static enum {
    LB_UNLIMITED,   /* Not limiting bandwidth, proceed. */
    LB_PROCEED,     /* Use pacefier_moved() afterwards. */
    LB_GO_SLEEP,    /* Not allowed to move data.        */
} limit_channel_bandwidth(TK_P_ struct connection *conn,
                          size_t *suggested_move_size) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    size_t bw = largs->params.channel_bandwidth_Bps;
    if(!bw) return LB_UNLIMITED;

    size_t smallest_block_to_move = largs->params.minimal_move_size;
    size_t allowed_to_move = pacefier_allow(&conn->bw_pace, bw, tk_now(TK_A));

    if(allowed_to_move < smallest_block_to_move) {
        /*     allowed     smallest_blk
           |------^-------------^-------> */
        double delay = (double)(smallest_block_to_move-allowed_to_move)/bw;
        if(delay > 1.0) delay = 1.0;
        else if(delay < 0.001) delay = 0.001;
        switch(conn->conn_type) {
        case CONN_OUTGOING:
            update_io_interest(TK_A_ conn, TK_READ);
            break;
        case CONN_INCOMING:
            update_io_interest(TK_A_ conn, 0);
            break;
        case CONN_ACCEPTOR:
            assert(!"Unreachable");
        }
#ifdef  USE_LIBUV
        uv_timer_init(TK_A_ &conn->timer);
        uv_timer_start(&conn->timer, conn_timer_cb_uv, 1000 * delay, 0.0);
#else
        ev_timer_init(&conn->timer, conn_timer_cb, delay, 0.0);
        ev_timer_start(TK_A_ &conn->timer);
#endif
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
            case LB_GO_SLEEP: return;
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
                    pacefier_moved(&conn->bw_pace,
                                   largs->params.channel_bandwidth_Bps,
                                   rd, tk_now(TK_A));
                conn->data_received += rd;
                if(largs->params.verbosity_level >= DBG_DATA) {
                    debug_dump_data(buf, rd);
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
                conn->data_received += rd;
                if(largs->params.verbosity_level >= DBG_DATA) {
                    debug_dump_data(buf, rd);
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

static void latency_record_outgoing_ts(TK_P_ struct connection *conn, struct transport_data_spec *data, const void *ptr, size_t wrote) {
    if(!conn->latency.sent_timestamps)
        return;

    void *data_start = data->ptr + data->header_size;
    if(ptr < data_start) {
        /* Ignore the --first-message in our calculations. */
        if(wrote > (size_t)(data_start - ptr)) {
            wrote -= (data_start - ptr);
            ptr = data_start;
        } else {
            return;
        }
    }

    struct loop_arguments *largs = tk_userdata(TK_A);

    size_t sent = wrote + conn->latency.incomplete_message_bytes_sent;
    size_t whole_msgs = (sent / largs->params.data.single_message_size);
    conn->latency.incomplete_message_bytes_sent =
        sent % largs->params.data.single_message_size;
    int ring_grown = 0;
    for(double now = tk_now(TK_A); whole_msgs; whole_msgs--) {
        ring_grown |= ring_buffer_add(conn->latency.sent_timestamps, now);
    }
    if(ring_grown) {
        /*
         * Ring has grown [even more]; check that we aren't recording send
         * timestamps without actually receiving any data back.
         */
        const unsigned MEGABYTE = 1024 * 1024;
        if(conn->latency.sent_timestamps->size > 10 * MEGABYTE) {
            fprintf(stderr,
                "Sending messages too fast, "
                    "not receiving them back fast enough.\n"
                "Check that the --latency-marker data is being received back.\n"
                "Use --verbose 3 to dump received message data.\n");
            exit(1);
        }
    }
}

static void latency_record_incoming_ts(TK_P_ struct connection *conn, char *buf, size_t size) {
    char *p, *bend;

    if(!conn->latency.sent_timestamps)
        return;

    struct loop_arguments *largs = tk_userdata(TK_A);

    uint8_t *lm = largs->params.latency_marker_data;
    size_t lm_size = largs->params.latency_marker_size;
    int num_markers_found = 0;

    if(conn->latency.marker_match_prefix) {
        if(size > lm_size - conn->latency.marker_match_prefix)
            bend = buf + lm_size - conn->latency.marker_match_prefix;
        else
            bend = buf + size;
        for(p = buf; p < bend; p++) {
            if(*p != lm[conn->latency.marker_match_prefix++])
                break;
        }
        if(p == bend) {
            if(conn->latency.marker_match_prefix == lm_size) {
                conn->latency.marker_match_prefix = 0;
                /* Found the full marker */
                num_markers_found++;
            } else {
                /* The message is still incomplete */
                return;
            }
        }
    }

    if(size < lm_size) {
        bend = buf;
    } else {
        bend = buf + (size - lm_size);
    }

    /* Go over they haystack while knowing that haystack's tail is always
     * greater than the latency marker. */
    for(p = buf; p < bend; p++) {
        if(memcmp(p, lm, lm_size) == 0) {
            num_markers_found++;
            p += lm_size - 1;
        }
    }

    /*
     * The last few bytes of the buffer are shorter than the latency marker.
     * Try to see whether it is a prefix of the latency marker.
     */
    size_t buf_tail = size - (p - buf);
    if(buf_tail > 0) {
        if(memcmp(p, lm, buf_tail) == 0) {
            if(buf_tail == lm_size) {
                num_markers_found++;
            } else {
                conn->latency.marker_match_prefix = buf_tail;
            }
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
        if(largs->params.data.total_size == 0) {
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
                    DEBUG(DBG_ERROR, "Closing %s: %s\n",
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
                conn->data_received += rd;
                if(largs->params.verbosity_level >= DBG_DATA) {
                    debug_dump_data(buf, rd);
                }
                latency_record_incoming_ts(TK_A_ conn, buf, rd);
                break;
            }
        }
    }

    if(revents & TK_WRITE) {
        const void *position;
        size_t available_length;
        int record_moved = 0;

        largest_contiguous_chunk(largs, &conn->write_offset, &position, &available_length);
        if(!available_length) {
            /* Only the header was sent. Now, silence. */
            assert(largs->params.data.total_size == largs->params.data.header_size);
            update_io_interest(TK_A_ conn, TK_READ); /* no write interest */
            return;
        }

        /* Adjust (available_length) to avoid sending too much stuff. */
        switch(limit_channel_bandwidth(TK_A_ conn, &available_length)) {
        case LB_UNLIMITED: record_moved = 0; break;
        case LB_PROCEED:   record_moved = 1; break;
        case LB_GO_SLEEP:  return;
        }

        ssize_t wrote = write(tk_fd(w), position, available_length);
        if(wrote == -1) {
            char buf[INET6_ADDRSTRLEN+64];
            switch(errno) {
            case EINTR:
            case EAGAIN:
                break;
            case EPIPE:
            default:
                DEBUG(DBG_ERROR, "Connection reset by %s\n",
                    format_sockaddr(remote, buf, sizeof(buf)));
                close_connection(TK_A_ conn, CCR_REMOTE);
                return;
            }
        } else {
            conn->write_offset += wrote;
            conn->data_sent += wrote;
            if(record_moved)
                pacefier_moved(&conn->bw_pace,
                               largs->params.channel_bandwidth_Bps,
                               wrote, tk_now(TK_A));
            latency_record_outgoing_ts(TK_A_ conn, &largs->params.data, position, wrote);
        }
    }

}

/*
 * Compute the largest amount of data we can send to the channel
 * using a single write() call.
 */
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length) {

    size_t total_size = largs->params.data.total_size;
    size_t available = total_size - *current_offset;
    if(available) {
        *position = largs->params.data.ptr + *current_offset;
        *available_length = available;
    } else {
        size_t off = largs->params.data.header_size;
        *position = largs->params.data.ptr + off;
        *available_length = total_size - off;
        *current_offset = off;
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

static void connections_flush_stats(TK_P) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    struct connection *conn;
    struct connection *tmpconn;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        connection_flush_stats(TK_A_ conn);
    }
}

static void free_connection_by_handle(tk_io *w) {
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));
    free(conn);
}

/*
 * Close connection and update connection and data transfer counters.
 */
static void close_connection(TK_P_ struct connection *conn, enum connection_close_reason reason) {
    char buf[256];
    struct loop_arguments *largs = tk_userdata(TK_A);
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
        DEBUG(DBG_ERROR, "Connection to %s is being closed: %s\n",
                format_sockaddr((struct sockaddr *)&largs->params
                    .remote_addresses.addrs[conn->remote_index],
                    buf, sizeof(buf)),
                strerror(errno));
        largs->worker_connection_failures++;
        largs->worker_connection_timeouts++;
        break;
    }
#ifdef  USE_LIBUV
    uv_timer_stop(&conn->timer);
    uv_poll_stop(&conn->watcher);
#else
    ev_timer_stop(TK_A_ &conn->timer);
    ev_io_stop(TK_A_ &conn->watcher);
#endif

    connection_flush_stats(TK_A_ conn);

    ring_buffer_free(conn->latency.sent_timestamps);
    conn->latency.sent_timestamps = 0;
    if(conn->latency.histogram) {
        int64_t n = hdr_add(largs->histogram, conn->latency.histogram);
        assert(n == 0);
        free(conn->latency.histogram);
    }

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

    tk_close(&conn->watcher, free_connection_by_handle);
}

/*
 * Add whatever data transfer counters we accumulated in a connection
 * back to the worker-wide tally.
 */
static void connection_flush_stats(TK_P_ struct connection *conn) {
    struct loop_arguments *largs = tk_userdata(TK_A);
    atomic_add(&largs->worker_data_sent, conn->data_sent);
    atomic_add(&largs->worker_data_received, conn->data_received);
    conn->data_sent = 0;
    conn->data_received = 0;
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
