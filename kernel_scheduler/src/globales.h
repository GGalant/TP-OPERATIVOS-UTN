#ifndef GLOBALES_H_
#define GLOBALES_H_

typedef struct
{
    char *nombre_mutex;
    int estado; // 0 "libre" o 1 "ocupado"
    int pid_proceso;
    t_list *cola_procesos; // Habria que ver si es solo el pid del proceso o todo el pcb (por ahora asumimos que es el pid)
} t_mutex;

typedef struct
{
    int pid;
    int tiempo_sleep;
} t_peticion_sleep;

typedef struct
{
    int pid;
    char* contenido;
} t_peticion_stdout;

// Declaraciones de variables globales
extern t_list *cola_ready;
extern t_list *cola_block;
extern t_list *cola_exec;
extern t_list *lista_cpu_conectadas;
extern t_list *lista_io_conectadas;
extern t_list *cola_susp_block;
extern t_list *cola_susp_ready;
extern t_list *cola_espera_sleep;
extern t_list *cola_espera_stdout;
extern t_log *log_ks;
extern t_log *log_debug_ks;
extern t_config *config;
extern char *config_path;
extern char *path_proceso_inicial;
extern char *puerto;
extern char *str_log_level;
extern char *puerto_km;
extern char *ip_km;
extern char *algoritmo_planificacion;
extern int *conexion_km_ptr;
extern int server_ks;
extern int conexion_km;
extern const int PRIORIDAD_MAXIMA;
extern int pid_inicial;
extern int log_level;
extern int id_io_global;
extern int rr_quantum;
extern int suspension_timeout;
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_block;
extern pthread_mutex_t mutex_cola_exec;
extern pthread_mutex_t mutex_lista_cpu_conectadas;
extern pthread_mutex_t mutex_lista_io_conectadas;
extern pthread_mutex_t mutex_generador_id_io;
extern pthread_mutex_t mutex_sleep;
extern pthread_mutex_t mutex_stdout;
extern pthread_mutex_t mutex_cola_susp_block;
extern pthread_mutex_t mutex_cola_susp_ready;
extern pthread_cond_t cond_cola_ready;
extern pthread_cond_t cond_cola_block;
extern pthread_cond_t cond_lista_cpu_conectadas;
extern pthread_cond_t cond_sleep;
extern pthread_cond_t cond_stdout;
extern pthread_cond_t cond_cola_susp_ready;
extern pthread_cond_t cond_cola_susp_block;

void wait_mutex(t_mutex *mutex, int pid_proceso);
void signal_mutex(t_mutex *mutex);

#endif /* GLOBALES_H_ */
