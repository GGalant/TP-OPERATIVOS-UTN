#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"

t_log* log_io;
t_config* config;
int conexion_ks;
char* ip_scheduler;
char* puerto_scheduler;
char* str_log_level;
char* tipo_io_str;

typedef enum {  
  STDIN,
  STDOUT,
  SLEEP
} tipo_io;

void inicializar_io(int argc, char *argv[]);
void enviar_io();
tipo_io obtener_tipo_io(char* tipo);
void handle_io_sleep();
void handle_io_stdin();
void handle_io_stdout();
void liberar_recursos();
void enviar_fin_io(int pid);