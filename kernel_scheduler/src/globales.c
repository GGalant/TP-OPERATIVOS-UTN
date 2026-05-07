#include <globales.h>
#include "globales.h"

// Definiciones de variables globales
t_list* lista_mutex;
t_list* cola_ready;
t_list* cola_block;
t_list* cola_exec;
t_list* lista_cpu_conectadas;
const int PRIORIDAD_MAXIMA = 0;
t_log *log_ks;
t_config* config;
char* config_path;
char* path_proceso_inicial;
char* puerto;
char* str_log_level;
char* puerto_km;
char* ip_km;
int server_ks;
int pid_inicial;
int log_level;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_block;
pthread_mutex_t mutex_lista_cpu_conectadas;
pthread_cond_t cond_cola_ready;
pthread_cond_t cond_cola_block;
pthread_cond_t cond_lista_cpu_conectadas;