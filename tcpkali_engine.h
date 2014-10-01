#ifndef TCPKALI_ENGINE_H
#define TCPKALI_ENGINE_H

long number_of_cpus();

struct engine;

struct engine_params {
    struct addresses remote_addresses;
    struct addresses listen_addresses;
    size_t requested_workers;       /* Number of threads to start */
    size_t channel_bandwidth_Bps;   /* Single channel bw, bytes per second. */
    size_t minimal_write_size;
    enum {
        DBG_ALWAYS,
        DBG_ERROR,
        DBG_DETAIL,
    } debug_level;
    double connect_timeout;
    double channel_lifetime;
    double epoch;
    /* Message data */
    void *data;
    size_t data_header_size;   /* Part of message_data to send once */
    size_t data_size;
};

struct engine *engine_start(struct engine_params);


/*
 * Report the number of opened connections by categories.
 */
void engine_connections(struct engine *, size_t *incoming, size_t *outgoing, size_t *counter);
void engine_traffic(struct engine *, size_t *sent, size_t *received);


size_t engine_initiate_new_connections(struct engine *, size_t n);

void engine_terminate(struct engine *);

#endif  /* TCPKALI_ENGINE_H */
