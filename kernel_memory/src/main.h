#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"
#include <pthread.h>

t_log* log_km;
int server_km;

void* handle_connections(void* arg);

