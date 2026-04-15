#include "utils/serializacion.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h> 
#include <commons/string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

t_log* log_ms;
int server_ms;

void* handle_connections(void* arg);