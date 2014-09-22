#ifndef TCPKALI_ENGINE_H
#define TCPKALI_ENGINE_H

long number_of_cpus();
int max_open_files();

struct engine;

struct engine_params {
    struct addresses addresses;
    size_t requested_workers;       /* Number of threads to start */
    size_t channel_bandwidth_Bps;   /* Single channel bw, bytes per second. */
    /* Message data */
    void *message_data;
    size_t message_size;
};

struct engine *engine_start(struct engine_params);


int engine_connections(struct engine *);
void engine_initiate_new_connections(struct engine *, size_t n);

void engine_terminate(struct engine *);

#endif  /* TCPKALI_ENGINE_H */
