#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"

t_log* log_io;
t_config* config;
char* ip_scheduler;
char* puerto_scheduler;
char* str_log_level;
char* tipo_io;

typedef enum {  
  STDIN,
  STDOUT,
  SLEEP
} tipo_io;