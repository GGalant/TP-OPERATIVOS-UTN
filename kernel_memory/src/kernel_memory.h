#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"
#include <pthread.h>

t_log* log_km;
int server_km;
int socket_memory_stick;
int socket_swap;

void* handle_connections(void* arg);
void* handle_ks(void* arg);
void* handle_cpu(void* arg);
