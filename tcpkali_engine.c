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

#include <config.h>

#ifdef  HAVE_SCHED_H
#include <sched.h>
#endif

#include "tcpkali.h"
#include "tcpkali_atomic.h"

#include <ev.h>

struct loop_arguments {
    struct addresses addresses;
    const void *sample_data;
    size_t      sample_data_size;
    struct remote_stats {
        long connection_attempts;
        long connection_failures;
    } *remote_stats;    /* Per-thread remote server stats */
    unsigned int address_offset;
    int control_pipe;
    int thread_no;
    size_t worker_data_sent;
    size_t worker_data_received;
    enum {
        THREAD_TERMINATING = 1
    } thread_flags;
    atomic_t open_connections;
};

struct connection {
    ev_io watcher;
    off_t write_offset;
    size_t data_sent;
    size_t data_received;
    struct sockaddr *remote_address;
    struct remote_stats *remote_stats;
    struct ev_loop *loop;
};

/*
 * Engine abstracts over workers.
 */
struct engine {
    struct loop_arguments *loop_args;
    pthread_t *threads;
    int wr_pipe;
    int n_workers;
    double epoch;
    size_t total_data_sent;
    size_t total_data_received;
};

/*
 * Helper functions defined at the end of the file.
 */
enum connection_close_reason {
    CCR_CLEAN,
    CCR_REMOTE, /* Remote side closed connection */
};
static void *single_engine_loop_thread(void *argp);
static void start_new_connection(EV_P);
static void close_connection(struct loop_arguments *largs, struct connection *conn, enum connection_close_reason reason);
static void connection_cb(EV_P_ ev_io *w, int revents);
static void control_cb(EV_P_ ev_io *w, int revents);
static struct sockaddr *pick_remote_address(struct loop_arguments *largs, struct remote_stats **remote_stats);
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length);
static void multiply_data(void **data, size_t *size);
static char *express_bytes(size_t bytes, char *buf, size_t size);

struct engine *engine_start(struct addresses addresses, int requested_workers, void *data, size_t data_size) {
    int fildes[2];

    int rc = pipe(fildes);
    assert(rc == 0);

    int rd_pipe = fildes[0];
    int wr_pipe = fildes[1];

    int n_workers = requested_workers;
    if(!n_workers) {
        long n_cpus = number_of_cpus();
        assert(n_cpus >= 1);
        fprintf(stderr, "Using %ld available CPUs\n", n_cpus);
        n_workers = n_cpus;
    }

    struct engine *eng = calloc(1, sizeof(*eng));
    eng->loop_args = calloc(n_workers, sizeof(eng->loop_args[0]));
    eng->threads = calloc(n_workers, sizeof(eng->threads[0]));
    eng->n_workers = n_workers;
    eng->wr_pipe = wr_pipe;
    eng->epoch = ev_now(EV_DEFAULT);

    /*
     * For efficiency, make sure we concatenate a few data items
     * instead of sending short messages one by one.
     */
    multiply_data(&data, &data_size);

    for(int n = 0; n < eng->n_workers; n++) {
        struct loop_arguments *loop_args = &eng->loop_args[n];
        loop_args->sample_data = data;
        loop_args->sample_data_size = data_size;
        loop_args->addresses = addresses;
        loop_args->remote_stats = calloc(addresses.n_addrs, sizeof(loop_args->remote_stats[0]));
        loop_args->control_pipe = rd_pipe;
        loop_args->address_offset = n;
        loop_args->thread_no = n;

        int rc = pthread_create(&eng->threads[n], 0,
                                single_engine_loop_thread, loop_args);
        assert(rc == 0);
    }

    /* Update epoch once more for precision. */
    eng->epoch = ev_now(EV_DEFAULT);

    return eng;
}


/*
 * Send a signal to finish work and wait for all workers to terminate.
 */
void engine_terminate(struct engine *eng) {
    /* Terminate all threads. */
    for(int n = 0; n < eng->n_workers; n++) {
        write(eng->wr_pipe, "T", 1);
    }
    for(int n = 0; n < eng->n_workers; n++) {
        void *value;
        pthread_join(eng->threads[n], &value);
        eng->total_data_sent +=
            eng->loop_args[n].worker_data_sent;
        eng->total_data_received +=
            eng->loop_args[n].worker_data_received;
    }
    eng->n_workers = 0;
    char buf[64];
    fprintf(stderr, "Total data sent:     %s (%ld)\n",
        express_bytes(eng->total_data_sent, buf, sizeof(buf)),
        (long)eng->total_data_sent);
    fprintf(stderr, "Total data received: %s (%ld)\n",
        express_bytes(eng->total_data_received, buf, sizeof(buf)),
        (long)eng->total_data_received);
    fprintf(stderr, "Aggregate bandwidth: %.3f Mbps\n",
        8 * ((eng->total_data_sent+eng->total_data_received)
                /(ev_now(EV_DEFAULT) - eng->epoch))
            / (1000000.0)
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
int engine_connections(struct engine *eng) {
    int connections = 0;
    for(int n = 0; n < eng->n_workers; n++) {
        connections += eng->loop_args[n].open_connections;
    }
    return connections;
}

void engine_initiate_new_connections(struct engine *eng, size_t n) {
    static char buf[1024];  /* This is thread-safe! */
    if(!buf[0]) {
        memset(buf, 'c', sizeof(buf));
    }
    while(n > 0) {
        int current_batch = n < sizeof(buf) ? n : sizeof(buf);
        int wrote = write(eng->wr_pipe, buf, current_batch);
        if(wrote > 0) n -= wrote;
    }
}

static void *single_engine_loop_thread(void *argp) {
    struct loop_arguments *largs = (struct loop_arguments *)argp;
    struct ev_loop *loop = ev_loop_new(EVFLAG_AUTO);
    ev_set_userdata(loop, largs);

    ev_io control_watcher;
    signal(SIGPIPE, SIG_IGN);

    ev_io_init(&control_watcher, control_cb, largs->control_pipe, EV_READ);
    ev_io_start(loop, &control_watcher);

    ev_run(loop, 0);

    fprintf(stderr, "Exiting cpu thread %d\n"
            "  %d open_connections\n"
            "  %ld worker_data_sent\n"
            "  %ld worker_data_received\n",
        largs->thread_no,
        (int)largs->open_connections,
        (long)largs->worker_data_sent,
        (long)largs->worker_data_received);

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
        largs->thread_flags |= THREAD_TERMINATING;
        ev_io_stop(EV_A_ w);
        break;
    default:
        fprintf(stderr, "Unknown operation '%c' from a control channel %d\n",
            c, w->fd);
    }
}

static void start_new_connection(EV_P) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct remote_stats *remote_stats;

    struct sockaddr *sa = pick_remote_address(largs, &remote_stats);

    remote_stats->connection_attempts++;

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

    rc = connect(sockfd, sa, sizeof(*sa));
    if(rc == -1) {
        char buf[INET6_ADDRSTRLEN+64];
        switch(errno) {
        case EINPROGRESS:
            break;
        default:
            remote_stats->connection_failures++;
            if(remote_stats->connection_failures == 1) {
                fprintf(stderr, "Connection to %s is not done: %s\n",
                        format_sockaddr(sa, buf, sizeof(buf)), strerror(errno));
            }
            close(sockfd);
            return;
        }
    }

    struct connection *conn = calloc(1, sizeof(*conn));
    conn->remote_address = sa;
    conn->loop = EV_A;
    conn->remote_stats = remote_stats;
    atomic_increment(&largs->open_connections);

    ev_io_init(&conn->watcher, connection_cb, sockfd,
        EV_READ | (largs->sample_data_size ? EV_WRITE : 0));

    ev_io_start(EV_A_ &conn->watcher);
}

static struct sockaddr *pick_remote_address(struct loop_arguments *largs, struct remote_stats **remote_stats) {

    /*
     * If it is known that a particular destination is broken, choose
     * the working one right away.
     */
    int off = 0;
    for(int attempts = 0; attempts < largs->addresses.n_addrs; attempts++) {
        off = largs->address_offset++ % largs->addresses.n_addrs;
        struct remote_stats *rs = &largs->remote_stats[off];
        if(rs->connection_attempts > 10
            && rs->connection_failures == rs->connection_attempts) {
            continue;
        } else {
            break;
        }
    }

    *remote_stats = &largs->remote_stats[off];
    return &largs->addresses.addrs[off];
}

static void connection_cb(EV_P_ ev_io *w, int revents) {
    struct loop_arguments *largs = ev_userdata(EV_A);
    struct connection *conn = (struct connection *)((char *)w - offsetof(struct connection, watcher));

    if(largs->thread_flags & THREAD_TERMINATING) {
        close_connection(largs, conn, CCR_CLEAN);
        return;
    }

    if(revents & EV_WRITE) {
        const void *position;
        size_t available_length;
        largest_contiguous_chunk(largs, &conn->write_offset, &position, &available_length);
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
                    format_sockaddr(conn->remote_address, buf, sizeof(buf)));
                close_connection(largs, conn, CCR_REMOTE);
                return;
            }
        } else {
            conn->write_offset += wrote;
            conn->data_sent += wrote;
        }
    }

    if(revents & EV_READ) {
        char buf[65536];
        for(;;) {
            ssize_t rd = read(w->fd, buf, sizeof(buf));
            switch(rd) {
            case -1:
                switch(errno) {
                case EAGAIN:
                    return;
                default:
                    fprintf(stderr, "Half-closing %s: %s\n",
                        format_sockaddr(conn->remote_address, buf, sizeof(buf)),
                        strerror(errno));
                }
                /* Fall through */
            case 0:
                fprintf(stderr, "Connection half-closed by %s\n",
                    format_sockaddr(conn->remote_address, buf, sizeof(buf)));
                close_connection(largs, conn, CCR_REMOTE);
                return;
            default:
                conn->data_received += rd;
                break;
            }
        }
    }

}

/*
 * Compute the largest amount of data we can send to the channel
 * using a single write() call.
 */
static void largest_contiguous_chunk(struct loop_arguments *largs, off_t *current_offset, const void **position, size_t *available_length) {

    size_t size = largs->sample_data_size;
    size_t available = size - *current_offset;
    if(available) {
        *position = largs->sample_data + *current_offset;
        *available_length = available;
    } else {
        *position = largs->sample_data;
        *available_length = size;
        *current_offset = 0;
    }
}

static void close_connection(struct loop_arguments *largs, struct connection *conn, enum connection_close_reason reason) {
    if(reason != CCR_CLEAN)
        conn->remote_stats->connection_failures++;
    largs->worker_data_sent += conn->data_sent;
    largs->worker_data_received += conn->data_received;
    atomic_decrement(&largs->open_connections);
    ev_io_stop(conn->loop, &conn->watcher);
    free(conn);
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
