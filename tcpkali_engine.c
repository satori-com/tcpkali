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
    enum state {
        CSTATE_CONNECTED,
        CSTATE_CONNECTING,
    } conn_state:8;
    enum type {
        CONN_OUTGOING,
        CONN_INCOMING,
        CONN_ACCEPTOR,
    } conn_type:8;
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
    int control_pipe;
    int thread_no;
    /* The following atomic members are accessed outside of worker thread */
    atomic64_t worker_data_sent;
    atomic64_t worker_data_received;
    atomic_t incoming_connections;
    atomic_t outgoing_connections;
    atomic_t connections_counter;
    TAILQ_HEAD( , connection) open_conns;  /* Thread-local connections */
    unsigned long worker_connections_initiated;
    unsigned long worker_connections_accepted;
    unsigned long worker_connection_failures;
    unsigned long worker_connection_timeouts;
};

/*
 * Engine abstracts over workers.
 */
struct engine {
    struct loop_arguments *loops;
    pthread_t *threads;
    int wr_pipe;
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
static void channel_lifetime(EV_P_ ev_timer *w, int revents);
static void setup_channel_lifetime_timer(EV_P_ double first_timeout);
static struct sockaddr *pick_remote_address(struct loop_arguments *largs, size_t *remote_index);
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length);
static void multiply_data(void **data, size_t *size);
static char *express_bytes(size_t bytes, char *buf, size_t size);

struct engine *engine_start(struct engine_params params) {
    int fildes[2];

    int rc = pipe(fildes);
    assert(rc == 0);

    int rd_pipe = fildes[0];
    int wr_pipe = fildes[1];

    int n_workers = params.requested_workers;
    if(!n_workers) {
        long n_cpus = number_of_cpus();
        assert(n_cpus >= 1);
        fprintf(stderr, "Using %ld available CPUs\n", n_cpus);
        n_workers = n_cpus;
    }

    /*
     * For efficiency, make sure we concatenate a few data items
     * instead of sending short messages one by one.
     */
    multiply_data(&params.message_data, &params.message_size);
    if(params.minimal_write_size == 0)
        params.minimal_write_size = 1460; /* ~MTU */
    params.epoch = ev_now(EV_DEFAULT);  /* Single epoch for all threads */

    struct engine *eng = calloc(1, sizeof(*eng));
    eng->loops = calloc(n_workers, sizeof(eng->loops[0]));
    eng->threads = calloc(n_workers, sizeof(eng->threads[0]));
    eng->n_workers = n_workers;
    eng->wr_pipe = wr_pipe;

    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *loop_args = &eng->loops[n];
        TAILQ_INIT(&loop_args->open_conns);
        loop_args->params = params;
        loop_args->remote_stats = calloc(params.remote_addresses.n_addrs, sizeof(loop_args->remote_stats[0]));
        loop_args->control_pipe = rd_pipe;
        loop_args->address_offset = n;
        loop_args->thread_no = n;

        int rc = pthread_create(&eng->threads[n], 0,
                                single_engine_loop_thread, loop_args);
        assert(rc == 0);
    }

    return eng;
}

/*
 * Send a signal to finish work and wait for all workers to terminate.
 */
void engine_terminate(struct engine *eng) {
    size_t conn_in, conn_out, conn_counter;
    engine_connections(eng, &conn_in, &conn_out, &conn_counter);

    /* Terminate all threads. */
    for(int n = 0; n < eng->n_workers; n++) {
        write(eng->wr_pipe, "T", 1);
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
    fprintf(stderr, "Total data sent:     %s (%ld bytes)\n",
        express_bytes(eng->total_data_sent, buf, sizeof(buf)),
        (long)eng->total_data_sent);
    fprintf(stderr, "Total data received: %s (%ld bytes)\n",
        express_bytes(eng->total_data_received, buf, sizeof(buf)),
        (long)eng->total_data_received);
    fprintf(stderr, "Aggregate bandwidth: %.3f↓, %.3f↑ Mbps\n",
        8 * ((eng->total_data_received) /(now - epoch)) / 1000000.0,
        8 * ((eng->total_data_sent) /(now - epoch)) / 1000000.0);
    long conns = conn_in + conn_out;
    if(!conns) conns = 1; /* Assume a single channel. */
    fprintf(stderr, "Bandwidth per channel: %.3f Mbps, %.1f kBps\n",
        8 * (((eng->total_data_sent+eng->total_data_received)
                /(now - epoch))/conns) / 1000000.0,
        ((eng->total_data_sent+eng->total_data_received)
                /(now - epoch))/conns/1000.0
    );
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
void engine_connections(struct engine *eng, size_t *incoming, size_t *outgoing, size_t *counter) {
    size_t c_in = 0;
    size_t c_out = 0;
    size_t c_count = 0;
    for(int n = 0; n < eng->n_workers; n++) {
        c_in    += eng->loops[n].incoming_connections;
        c_out   += eng->loops[n].outgoing_connections;
        c_count += eng->loops[n].connections_counter;
    }
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

size_t engine_initiate_new_connections(struct engine *eng, size_t n_req) {
    static char buf[1024];  /* This is thread-safe! */
    if(!buf[0]) {
        memset(buf, 'c', sizeof(buf));
    }
    int fd = eng->wr_pipe;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    size_t n = 0;
    while(n < n_req) {
        int current_batch = (n_req-n) < sizeof(buf) ? (n_req-n) : sizeof(buf);
        int wrote = write(eng->wr_pipe, buf, current_batch);
        if(wrote == -1) {
            if(errno == EAGAIN)
                break;
            assert(wrote != -1);
        }
        if(wrote > 0) n += wrote;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    return n;
}

static void channel_lifetime(EV_P_ ev_timer UNUSED *w, int UNUSED revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn;
    struct connection *tmpconn;

    assert(largs->params.channel_lifetime);
    double delta = ev_now(EV_A) - largs->params.epoch;
    TAILQ_FOREACH_SAFE(conn, &largs->open_conns, hook, tmpconn) {
        if(conn->channel_eol_point <= delta) {
            close_connection(EV_A_ conn, CCR_CLEAN);
        } else {
            /*
             * Channels are added to the tail of the queue and have the same
             * Expiration timeout. This channel and the others after it
             * are not yet expired. Restart timeout so we'll get to it
             * and the one after it when the time is really due.
             */
            setup_channel_lifetime_timer(EV_A_
                    (conn->channel_eol_point - delta));
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

    ev_io control_watcher;
    signal(SIGPIPE, SIG_IGN);

    /*
     * Open all listening sockets, if they are specified.
     */
    if(largs->params.listen_addresses.n_addrs) {
        int opened_listening_sockets = 0;
        for(size_t n = 0; n < largs->params.listen_addresses.n_addrs; n++) {
            struct sockaddr *sa = &largs->params.listen_addresses.addrs[n];
            int lsock = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
            assert(lsock != -1);
            int rc = fcntl(lsock, F_SETFL, fcntl(lsock, F_GETFL) | O_NONBLOCK);
            assert(rc != -1);
            int on = ~0;
            rc = setsockopt(lsock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
            assert(rc != -1);
            rc = bind(lsock, sa, sizeof(*sa));
            if(rc == -1) {
                char buf[256];
                fprintf(stderr, "Bind %s is not done: %s\n",
                        format_sockaddr(sa, buf, sizeof(buf)),
                        strerror(errno));
                if(errno == EINVAL) {
                    continue;
                } else {
                    printf("unavailable\n\n");
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
            fprintf(stderr, "Could not listen on any local sockets!\n");
            exit(EX_UNAVAILABLE);
        }
    }


    if(largs->params.channel_lifetime > 0.0) {
        ev_timer_init(&largs->channel_lifetime_timer, channel_lifetime, 1, 0);
        ev_timer_start(EV_A_ &largs->channel_lifetime_timer);
    }

    ev_timer_init(&largs->stats_timer, stats_timer_cb, 0.25, 0.25);
    ev_timer_start(EV_A_ &largs->stats_timer);
    ev_io_init(&control_watcher, control_cb, largs->control_pipe, EV_READ);
    ev_io_start(loop, &control_watcher);
    ev_run(loop, 0);
    ev_timer_stop(EV_A_ &largs->stats_timer);
    ev_io_stop(EV_A_ &control_watcher);

    connections_flush_stats(EV_A);

    fprintf(stderr, "Exiting worker %d\n"
            "  %u↓, %u↑ open connections\n"
            "  %u connections_counter \n"
            "  ↳ %lu connections_initiated\n"
            "  ↳ %lu connections_accepted\n"
            "  %lu connection_failures\n"
            "  ↳ %lu connection_timeouts\n"
            "  %lu worker_data_sent\n"
            "  %lu worker_data_received\n",
        largs->thread_no,
        largs->incoming_connections,
        largs->outgoing_connections,
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

static void control_cb(EV_P_ ev_io *w, int __attribute__((unused)) revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);

    char c;
    if(read(w->fd, &c, 1) != 1) {
        fprintf(stderr, "%d Reading from control channel %d returned: %s\n",
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
        fprintf(stderr, "Unknown operation '%c' from a control channel %d\n",
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
            fprintf(stderr, "System socket table is full (%d), consider changing ulimit -n", max_open_files());
            exit(1);
        }
        return; /* Come back later */
    }
    int rc = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    assert(rc != -1);
    int on = ~0;
    rc = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    assert(rc != -1);

    rc = connect(sockfd, sa, sizeof(*sa));
    if(rc == -1) {
        char buf[INET6_ADDRSTRLEN+64];
        switch(errno) {
        case EINPROGRESS:
            break;
        default:
            atomic_increment(&remote_stats->connection_failures);
            largs->worker_connection_failures++;
            if(remote_stats->connection_failures == 1) {
                fprintf(stderr, "Connection to %s is not done: %s\n",
                        format_sockaddr(sa, buf, sizeof(buf)), strerror(errno));
            }
            close(sockfd);
            return;
        }
    }

    double now = ev_now(EV_A);
    atomic_increment(&largs->outgoing_connections);

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->conn_type = CONN_OUTGOING;
    if(largs->params.channel_lifetime > 0.0) {
        if(TAILQ_FIRST(&largs->open_conns) == NULL) {
            setup_channel_lifetime_timer(EV_A_ largs->params.channel_lifetime);
        }
        conn->channel_eol_point =
            (now - largs->params.epoch) + largs->params.channel_lifetime;
    }
    TAILQ_INSERT_TAIL(&largs->open_conns, conn, hook);
    pacefier_init(&conn->bw_pace, now);
    conn->remote_index = remote_index;

    if(largs->params.connect_timeout > 0.0) {
        conn->conn_state = CSTATE_CONNECTING;
        ev_timer_init(&conn->timer, conn_timer,
                      largs->params.connect_timeout, 0.0);
        ev_timer_start(EV_A_ &conn->timer);
    }

    int want_write = (largs->params.message_size
                        || largs->params.connect_timeout > 0);
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
    return &largs->params.remote_addresses.addrs[off];
}

static void conn_timer(EV_P_ ev_timer *w, int __attribute__((unused)) revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, timer));

    switch(conn->conn_state) {
    case CSTATE_CONNECTED:
        ev_io_set(&conn->watcher, conn->watcher.fd,
            EV_READ | (largs->params.message_size ? EV_WRITE : 0));
        break;
    case CSTATE_CONNECTING:
        close_connection(EV_A_ conn, CCR_TIMEOUT);
        return;
    }
}

static void setup_channel_lifetime_timer(EV_P_ double first_timeout) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    ev_timer_set(&largs->channel_lifetime_timer, first_timeout, 0);
    ev_timer_start(EV_A_ &largs->channel_lifetime_timer);
}

static void accept_cb(EV_P_ ev_io *w, int UNUSED revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    int sockfd = accept(w->fd, 0, 0);
    if(sockfd == -1)
        return;
    int rc = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    assert(rc != -1);

    atomic_increment(&largs->connections_counter);
    atomic_increment(&largs->incoming_connections);

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->conn_type = CONN_INCOMING;
    if(largs->params.channel_lifetime > 0.0) {
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

static void connection_cb(EV_P_ ev_io *w, int revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));
    struct sockaddr *remote = &largs->params.remote_addresses.addrs[conn->remote_index];

    if(conn->conn_state == CSTATE_CONNECTING) {
        conn->conn_state = CSTATE_CONNECTED;
        ev_timer_stop(EV_A_ &conn->timer);
        /*
         * We were asked to produce the WRITE event
         * only to detect successful connection.
         * If there's nothing to write, we remove the write interest.
         */
        if(largs->params.message_size == 0) {
            ev_io_set(&conn->watcher, w->fd, EV_READ);  /* Disable write */
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
                    fprintf(stderr, "Closing %s: %s\n",
                        format_sockaddr(remote, buf, sizeof(buf)),
                        strerror(errno));
                    close_connection(EV_A_ conn, CCR_REMOTE);
                    return;
                }
                /* Fall through */
            case 0:
                fprintf(stderr, "Connection half-closed by %s\n",
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
        size_t bw = largs->params.channel_bandwidth_Bps;
        if(bw != 0) {
            size_t bytes = pacefier_allow(&conn->bw_pace, bw, ev_now(EV_A));
            size_t smallest_block_to_send = largs->params.minimal_write_size;
            if(bytes == 0) {
                double delay = (double)smallest_block_to_send/bw;
                if(delay > 1.0) delay = 1.0;
                else if(delay < 0.001) delay = 0.001;
                ev_io_set(&conn->watcher, w->fd, EV_READ);  /* Disable write */
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
                fprintf(stderr, "Connection reset by %s\n",
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

    size_t size = largs->params.message_size;
    size_t available = size - *current_offset;
    if(available) {
        *position = largs->params.message_data + *current_offset;
        *available_length = available;
    } else {
        *position = largs->params.message_data;
        *available_length = size;
        *current_offset = 0;
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
        fprintf(stderr, "Connection to %s is not done: %s\n",
                format_sockaddr(&largs->params.remote_addresses.addrs[conn->remote_index], buf, sizeof(buf)),
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
    case CONN_INCOMING:
        atomic_decrement(&largs->incoming_connections);
        break;
    case CONN_OUTGOING:
        atomic_decrement(&largs->outgoing_connections);
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
static void multiply_data(void **data, size_t *size) {
    if(!*size) {
        /* Can't blow up empty buffer. */
    } else if(*size >= 65536) {
        /* Data is large enough to avoid blowing up. */
    } else {
        size_t n = 1 + 65536/(*size);
        size_t s = n * (*size);
        char *p = malloc(s);
        assert(p);
        for(size_t i = 0; i < n; i++) {
            memcpy(&p[i * (*size)], *data, *size);
        }
        *data = p;
        *size = s;
    }
}

/*
 * Determine the limit on open files.
 */
int max_open_files() {
    return sysconf(_SC_OPEN_MAX);
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
