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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

#include "tcpkali.h"
#include "tcpkali_atomic.h"
#include "tcpkali_pacefier.h"

#include <ev.h>

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
    ev_io watcher;
    ev_timer timer;
    struct pacefier bw_pace;
    off_t write_offset;
    size_t data_sent;
    size_t data_received;
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
};

struct loop_arguments {
    struct engine_params params;    /* A copy of engine parameters */
    unsigned int address_offset;    /* An offset into the params.remote_addresses[] */
    struct remote_stats {
        atomic_t connection_attempts;
        atomic_t connection_failures;
    } *remote_stats;    /* Per-thread remote server stats */
    ev_timer stats_timer;
    ev_timer channel_lifetime_timer;
    int global_control_pipe_rd_nbio;    /* Non-blocking pipe anyone could read from. */
    int private_control_pipe_rd;   /* Private blocking pipe for this worker (read side). */
    int private_control_pipe_wr;   /* Private blocking pipe for this worker (write side). */
    int thread_no;
    /* The following atomic members are accessed outside of worker thread */
    atomic64_t worker_data_sent;
    atomic64_t worker_data_received;
    atomic_t outgoing_connecting;
    atomic_t outgoing_established;
    atomic_t incoming_established;
    atomic_t connections_counter;
    TAILQ_HEAD( , connection) open_conns;  /* Thread-local connections */
    unsigned long worker_connections_initiated;
    unsigned long worker_connections_accepted;
    unsigned long worker_connection_failures;
    unsigned long worker_connection_timeouts;
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
    int next_worker_order[_CONTROL_MESSAGES_MAXID];
    int n_workers;
    size_t total_data_sent;
    size_t total_data_received;
};

/*
 * Helper functions defined at the end of the file.
 */
enum connection_close_reason {
    CCR_CLEAN,  /* No failure */
    CCR_LIFETIME, /* Channel lifetime limit (no failure) */
    CCR_TIMEOUT, /* Connection timeout */
    CCR_REMOTE, /* Remote side closed connection */
};
static void *single_engine_loop_thread(void *argp);
static void start_new_connection(EV_P);
static void close_connection(EV_P_ struct connection *conn, enum connection_close_reason reason);
static void connections_flush_stats(EV_P);
static void connection_flush_stats(EV_P_ struct connection *conn);
static void close_all_connections(EV_P_ enum connection_close_reason reason);
static void connection_cb(EV_P_ ev_io *w, int revents);
static void devnull_cb(EV_P_ ev_io *w, int revents);
static void control_cb(EV_P_ ev_io *w, int revents);
static void accept_cb(EV_P_ ev_io *w, int revents);
static void conn_timer(EV_P_ ev_timer *w, int revents); /* Timeout timer */
static void expire_channel_lives(EV_P_ ev_timer *w, int revents);
static void setup_channel_lifetime_timer(EV_P_ double first_timeout);
static void update_io_interest(EV_P_ struct connection *conn, int events);
static struct sockaddr *pick_remote_address(struct loop_arguments *largs, size_t *remote_index);
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length);
static void multiply_data(struct engine_params *);
static char *express_bytes(size_t bytes, char *buf, size_t size);
static int limit_channel_lifetime(struct loop_arguments *largs);
static void set_nbio(int fd, int onoff);

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
        if(largs->params.debug_level >= level)  \
            fprintf(stderr, fmt, ##args);       \
    } while(0)

struct engine *engine_start(struct engine_params params) {
    int fildes[2];

    int rc = pipe(fildes);
    assert(rc == 0);

    int rd_pipe = fildes[0];
    int wr_pipe = fildes[1];
    set_nbio(rd_pipe, 1);

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
    multiply_data(&params);
    if(params.minimal_write_size == 0)
        params.minimal_write_size = 1460; /* ~MTU */
    params.epoch = ev_now(EV_DEFAULT);  /* Single epoch for all threads */

    struct engine *eng = calloc(1, sizeof(*eng));
    eng->loops = calloc(n_workers, sizeof(eng->loops[0]));
    eng->threads = calloc(n_workers, sizeof(eng->threads[0]));
    eng->n_workers = n_workers;
    eng->global_control_pipe_wr = wr_pipe;

    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *loop_args = &eng->loops[n];
        TAILQ_INIT(&loop_args->open_conns);
        loop_args->params = params;
        loop_args->remote_stats = calloc(params.remote_addresses.n_addrs, sizeof(loop_args->remote_stats[0]));
        loop_args->address_offset = n;
        loop_args->thread_no = n;
        int rc;

        int private_pipe[2];
        rc = pipe(private_pipe);
        assert(rc == 0);
        loop_args->private_control_pipe_rd = private_pipe[0];
        loop_args->private_control_pipe_wr = private_pipe[1];
        loop_args->global_control_pipe_rd_nbio = rd_pipe;

        rc = pthread_create(&eng->threads[n], 0,
                                single_engine_loop_thread, loop_args);
        assert(rc == 0);
    }

    return eng;
}

/*
 * Send a signal to finish work and wait for all workers to terminate.
 */
void engine_terminate(struct engine *eng) {
    size_t connecting, conn_in, conn_out, conn_counter;
    engine_connections(eng, &connecting, &conn_in, &conn_out, &conn_counter);

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
    }
    eng->n_workers = 0;
    double epoch = eng->loops[0].params.epoch;   /* same in all threads. */
    double now = ev_now(EV_DEFAULT);
    char buf[64];
    printf("Total data sent:     %s (%ld bytes)\n",
        express_bytes(eng->total_data_sent, buf, sizeof(buf)),
        (long)eng->total_data_sent);
    printf("Total data received: %s (%ld bytes)\n",
        express_bytes(eng->total_data_received, buf, sizeof(buf)),
        (long)eng->total_data_received);
    long conns = (0 * connecting) + conn_in + conn_out;
    if(!conns) conns = 1; /* Assume a single channel. */
    printf("Bandwidth per channel: %.3f Mbps, %.1f kBps\n",
        8 * (((eng->total_data_sent+eng->total_data_received)
                /(now - epoch))/conns) / 1000000.0,
        ((eng->total_data_sent+eng->total_data_received)
                /(now - epoch))/conns/1000.0
    );
    printf("Aggregate bandwidth: %.3f↓, %.3f↑ Mbps\n",
        8 * ((eng->total_data_received) /(now - epoch)) / 1000000.0,
        8 * ((eng->total_data_sent) /(now - epoch)) / 1000000.0);
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
void engine_connections(struct engine *eng, size_t *connecting, size_t *incoming, size_t *outgoing, size_t *counter) {
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

void engine_traffic(struct engine *eng, size_t *sent, size_t *received) {
    *sent = 0;
    *received = 0;
    for(int n = 0; n < eng->n_workers; n++) {
        *sent += eng->loops[n].worker_data_sent;
        *received += eng->loops[n].worker_data_received;
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

static void expire_channel_lives(EV_P_ ev_timer UNUSED *w, int UNUSED revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn;
    struct connection *tmpconn;

    assert(limit_channel_lifetime(largs));
    double delta = ev_now(EV_A) - largs->params.epoch;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        double expires_in = conn->channel_eol_point - delta;
        if(expires_in <= 0.0) {
            close_connection(EV_A_ conn, CCR_CLEAN);
        } else {
            /*
             * Channels are added to the tail of the queue and have the same
             * Expiration timeout. This channel and the others after it
             * are not yet expired. Restart timeout so we'll get to it
             * and the one after it when the time is really due.
             */
            setup_channel_lifetime_timer(EV_A_ expires_in);
            break;
        }
    }

}

static void stats_timer_cb(EV_P_ ev_timer UNUSED *w, int UNUSED revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);

    /*
     * Move the connections' stats data out into the atomically managed
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
    struct ev_loop *loop = ev_loop_new(EVFLAG_AUTO);
    ev_set_userdata(loop, largs);

    ev_io global_control_watcher;
    ev_io private_control_watcher;
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
            pacefier_init(&conn->bw_pace, ev_now(EV_A));
            ev_io_init(&conn->watcher, accept_cb, lsock, EV_READ | EV_WRITE);
            ev_io_start(EV_A_ &conn->watcher);
        }
        if(!opened_listening_sockets) {
            DEBUG(DBG_ALWAYS, "Could not listen on any local sockets!\n");
            exit(EX_UNAVAILABLE);
        }
    }

    if(limit_channel_lifetime(largs)) {
        ev_timer_init(&largs->channel_lifetime_timer,
            expire_channel_lives, 0, 0);
    }

    ev_timer_init(&largs->stats_timer, stats_timer_cb, 0.25, 0.25);
    ev_timer_start(EV_A_ &largs->stats_timer);
    ev_io_init(&global_control_watcher, control_cb, largs->global_control_pipe_rd_nbio, EV_READ);
    ev_io_init(&private_control_watcher, control_cb, largs->private_control_pipe_rd, EV_READ);
    ev_io_start(loop, &global_control_watcher);
    ev_io_start(loop, &private_control_watcher);
    ev_run(loop, 0);
    ev_timer_stop(EV_A_ &largs->stats_timer);
    ev_io_stop(EV_A_ &global_control_watcher);
    ev_io_stop(EV_A_ &private_control_watcher);

    connections_flush_stats(EV_A);

    DEBUG(DBG_ALWAYS, "Exiting worker %d\n"
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

    close_all_connections(EV_A_ CCR_CLEAN);

    return 0;
}

/*
 * Receive a control event from the pipe.
 */
static void control_cb(EV_P_ ev_io *w, int __attribute__((unused)) revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);

    char c;
    int ret = read(w->fd, &c, 1);
    if(ret != 1) {
        if(errno != EAGAIN)
            DEBUG(DBG_ALWAYS, "%d Reading from control channel %d returned: %s\n",
                largs->thread_no, w->fd, strerror(errno));
        return;
    }
    switch(c) {
    case 'c':
        start_new_connection(EV_A);
        break;
    case 'T':
        ev_break(EV_A_ EVBREAK_ALL);
        break;
    default:
        DEBUG(DBG_ALWAYS,
            "Unknown operation '%c' from a control channel %d\n",
            c, w->fd);
    }
}

static void start_new_connection(EV_P) {
    struct loop_arguments *largs = ev_userdata(EV_A);
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

    double now = ev_now(EV_A);

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->conn_type = CONN_OUTGOING;
    conn->conn_state = conn_state;
    if(limit_channel_lifetime(largs)) {
        if(TAILQ_FIRST(&largs->open_conns) == NULL) {
            setup_channel_lifetime_timer(EV_A_ largs->params.channel_lifetime);
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
        ev_timer_init(&conn->timer, conn_timer,
                      largs->params.connect_timeout, 0.0);
        ev_timer_start(EV_A_ &conn->timer);
    }

    int want_write = (largs->params.data_size || want_catch_connect);
    ev_io_init(&conn->watcher, connection_cb, sockfd,
        EV_READ | (want_write ? EV_WRITE : 0));

    ev_io_start(EV_A_ &conn->watcher);
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

static void conn_timer(EV_P_ ev_timer *w, int __attribute__((unused)) revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, timer));

    switch(conn->conn_state) {
    case CSTATE_CONNECTED:
        update_io_interest(EV_A_ conn,
            EV_READ | (largs->params.data_size ? EV_WRITE : 0));
        break;
    case CSTATE_CONNECTING:
        /* Timed out in the connection establishment phase. */
        close_connection(EV_A_ conn, CCR_TIMEOUT);
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

static void setup_channel_lifetime_timer(EV_P_ double first_timeout) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    ev_timer_stop(EV_A_ &largs->channel_lifetime_timer);
    ev_timer_set(&largs->channel_lifetime_timer, first_timeout, 0);
    ev_timer_start(EV_A_ &largs->channel_lifetime_timer);
}

static void accept_cb(EV_P_ ev_io *w, int UNUSED revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);

    int sockfd = accept(w->fd, 0, 0);
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
            setup_channel_lifetime_timer(EV_A_ largs->params.channel_lifetime);
        }
        double now = ev_now(EV_A);
        conn->channel_eol_point =
            (now - largs->params.epoch) + largs->params.channel_lifetime;
    }
    TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook);
    largs->worker_connections_accepted++;

    ev_io_init(&conn->watcher, devnull_cb, sockfd, EV_READ);
    ev_io_start(EV_A_ &conn->watcher);
}

static void devnull_cb(EV_P_ ev_io *w, int revents) {
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));

    if(revents & EV_READ) {
        char buf[16384];
        for(;;) {
            ssize_t rd = read(w->fd, buf, sizeof(buf));
            switch(rd) {
            case -1:
                switch(errno) {
                case EAGAIN:
                    return;
                default:
                    close_connection(EV_A_ conn, CCR_REMOTE);
                    return;
                }
                /* Fall through */
            case 0:
                close_connection(EV_A_ conn, CCR_REMOTE);
                return;
            default:
                conn->data_received += rd;
                break;
            }
        }
    }
}

static void update_io_interest(EV_P_ struct connection *conn, int events) {
    ev_io_stop(EV_A_ &conn->watcher);
    ev_io_set(&conn->watcher, conn->watcher.fd, events);
    ev_io_start(EV_A_ &conn->watcher);
}

static void connection_cb(EV_P_ ev_io *w, int revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));
    struct sockaddr *remote = (struct sockaddr *)&largs->params.remote_addresses.addrs[conn->remote_index];

    if(conn->conn_state == CSTATE_CONNECTING) {
        /*
         * Extended channel lifetimes managed out-of-band, but zero
         * lifetime can be managed here very quickly.
         */
        if(largs->params.channel_lifetime == 0.0) {
            close_connection(EV_A_ conn, CCR_CLEAN);
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
        ev_timer_stop(EV_A_ &conn->timer);
        if(largs->params.data_size == 0) {
            update_io_interest(EV_A_ conn, EV_READ); /* no write interest */
            revents &= ~EV_WRITE;   /* Don't actually write in this loop */
        }
    }

    if(revents & EV_READ) {
        char buf[16384];
        for(;;) {
            ssize_t rd = read(w->fd, buf, sizeof(buf));
            switch(rd) {
            case -1:
                switch(errno) {
                case EAGAIN:
                    return;
                default:
                    DEBUG(DBG_ERROR, "Closing %s: %s\n",
                        format_sockaddr(remote, buf, sizeof(buf)),
                        strerror(errno));
                    close_connection(EV_A_ conn, CCR_REMOTE);
                    return;
                }
                /* Fall through */
            case 0:
                DEBUG(DBG_DETAIL, "Connection half-closed by %s\n",
                    format_sockaddr(remote, buf, sizeof(buf)));
                close_connection(EV_A_ conn, CCR_REMOTE);
                return;
            default:
                conn->data_received += rd;
                break;
            }
        }
    }

    if(revents & EV_WRITE) {
        const void *position;
        size_t available_length;
        largest_contiguous_chunk(largs, &conn->write_offset, &position, &available_length);
        if(!available_length) {
            /* Only the header was sent. Now, silence. */
            assert(largs->params.data_size == largs->params.data_header_size);
            update_io_interest(EV_A_ conn, EV_READ); /* no write interest */
            return;
        }
        size_t bw = largs->params.channel_bandwidth_Bps;
        if(bw != 0) {
            size_t bytes = pacefier_allow(&conn->bw_pace, bw, ev_now(EV_A));
            size_t smallest_block_to_send = largs->params.minimal_write_size;
            if(bytes == 0) {
                double delay = (double)smallest_block_to_send/bw;
                if(delay > 1.0) delay = 1.0;
                else if(delay < 0.001) delay = 0.001;
                update_io_interest(EV_A_ conn, EV_READ); /* no write interest */
                ev_timer_init(&conn->timer, conn_timer, delay, 0.0);
                ev_timer_start(EV_A_ &conn->timer);
                return;
            } else if((size_t)bytes < available_length
                    && available_length > smallest_block_to_send) {
                /* Do not send more than approx 1 MTU. */
                available_length = smallest_block_to_send;
            }
        }
        ssize_t wrote = write(w->fd, position, available_length);
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
                close_connection(EV_A_ conn, CCR_REMOTE);
                return;
            }
        } else {
            conn->write_offset += wrote;
            conn->data_sent += wrote;
            if(bw) pacefier_emitted(&conn->bw_pace, bw, wrote, ev_now(EV_A));
        }
    }

}

/*
 * Compute the largest amount of data we can send to the channel
 * using a single write() call.
 */
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length) {

    size_t total_size = largs->params.data_size;
    size_t available = total_size - *current_offset;
    if(available) {
        *position = largs->params.data + *current_offset;
        *available_length = available;
    } else {
        size_t off = largs->params.data_header_size;
        *position = largs->params.data + off;
        *available_length = total_size - off;
        *current_offset = off;
    }
}

/*
 * Ungracefully close all connections and report accumulated stats
 * back to the central loop structure.
 */
static void close_all_connections(EV_P_ enum connection_close_reason reason) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn;
    struct connection *tmpconn;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        close_connection(EV_A_ conn, reason);
    }
}

static void connections_flush_stats(EV_P) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn;
    struct connection *tmpconn;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        connection_flush_stats(EV_A_ conn);
    }
}

/*
 * Close connection and update connection and data transfer counters.
 */
static void close_connection(EV_P_ struct connection *conn, enum connection_close_reason reason) {
    char buf[256];
    struct loop_arguments *largs = ev_userdata(EV_A);
    switch(reason) {
    case CCR_LIFETIME:
    case CCR_CLEAN:
        break;
    case CCR_REMOTE:
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
    ev_timer_stop(EV_A_ &conn->timer);
    ev_io_stop(EV_A_ &conn->watcher);

    close(conn->watcher.fd);
    conn->watcher.fd = -1;

    connection_flush_stats(EV_A_ conn);

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
    free(conn);
}

static void connection_flush_stats(EV_P_ struct connection *conn) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    atomic_add(&largs->worker_data_sent, conn->data_sent);
    atomic_add(&largs->worker_data_received, conn->data_received);
    conn->data_sent = 0;
    conn->data_received = 0;
}

/*
 * If the datum is less then 65k, make sure we repeat it several times
 * so the total buffer exceeds 65k.
 */
static void multiply_data(struct engine_params *params) {
    size_t msg_size = params->data_size - params->data_header_size;

    if(!msg_size) {
        /* Can't blow up empty buffer. */
    } else if(msg_size >= 65536) {
        /* Data is large enough to avoid blowing up. */
    } else {
        size_t n = ceil((64*1024.0)/msg_size); /* Optimum is size(L2)/k */
        size_t s = n * msg_size;
        size_t hdr_off = params->data_header_size;
        char *p = realloc(params->data, hdr_off + s + 1);
        void *msg_data = p + hdr_off;
        assert(p);
        for(size_t i = 1; i < n; i++) {
            memcpy(&p[hdr_off + i * msg_size], msg_data, msg_size);
        }
        p[hdr_off + s] = '\0';
        params->data = p;
        params->data_size = hdr_off + s;
    }
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
