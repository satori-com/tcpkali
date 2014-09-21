#ifndef TCPKALI_ENGINE_H
#define TCPKALI_ENGINE_H

long number_of_cpus();
int max_open_files();

struct engine;

struct engine *engine_start(struct addresses, int requested_workers, void *data, size_t data_size);

int engine_connections(struct engine *);
void engine_initiate_new_connections(struct engine *, size_t n);

void engine_terminate(struct engine *);

#endif  /* TCPKALI_ENGINE_H */
