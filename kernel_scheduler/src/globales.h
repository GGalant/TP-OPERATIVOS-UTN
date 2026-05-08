#ifndef GLOBALES_H_
#define GLOBALES_H_

#include "kernel_scheduler.h"

// Declaraciones de variables globales
extern t_list* lista_mutex;
extern t_list* cola_ready;
extern t_list* cola_block;
extern t_list* cola_exec;
extern t_list* lista_cpu_conectadas;
extern const int PRIORIDAD_MAXIMA;
extern t_log *log_ks;
extern t_log *log_debug_ks;
extern t_config* config;
extern char* config_path;
extern char* path_proceso_inicial;
extern char* puerto;
extern char* str_log_level;
extern char* puerto_km;
extern char* ip_km;
extern char* algoritmo_planificacion;
extern int* conexion_km_ptr;
extern int server_ks;
extern int conexion_km;
extern int pid_inicial;
extern int log_level;
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_block;
extern pthread_mutex_t mutex_cola_exec;
extern pthread_mutex_t mutex_lista_cpu_conectadas;
extern pthread_cond_t cond_cola_ready;
extern pthread_cond_t cond_cola_block;
extern pthread_cond_t cond_lista_cpu_conectadas;

#endif /* GLOBALES_H_ */
