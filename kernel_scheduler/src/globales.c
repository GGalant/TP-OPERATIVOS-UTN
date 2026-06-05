#include "kernel_scheduler.h"

t_list *cola_ready;
t_list *cola_block;
t_list *cola_exec;
t_list *lista_cpu_conectadas;
t_list *lista_io_conectadas;
t_list *cola_espera_sleep;
t_list *cola_espera_stdout;
t_list *cola_susp_block;
t_list *cola_susp_ready;
t_log *log_ks;
t_log *log_debug_ks;
t_config *config;
char *config_path;
char *path_proceso_inicial;
char *puerto;
char *str_log_level;
char *puerto_km;
char *ip_km;
char *algoritmo_planificacion;
const int PRIORIDAD_MAXIMA = 0;
int *conexion_km_ptr;
int id_io_global = -1;
int server_ks;
int conexion_km;
int pid_inicial;
int log_level;
int suspension_timeout;
int rr_quantum;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_block;
pthread_mutex_t mutex_cola_exec;
pthread_mutex_t mutex_lista_cpu_conectadas;
pthread_mutex_t mutex_lista_io_conectadas;
pthread_mutex_t mutex_generador_id_io;
pthread_mutex_t mutex_sleep;
pthread_mutex_t mutex_stdout;
pthread_mutex_t mutex_cola_susp_block;
pthread_mutex_t mutex_cola_susp_ready;
pthread_cond_t cond_cola_ready;
pthread_cond_t cond_cola_block;
pthread_cond_t cond_lista_cpu_conectadas;
pthread_cond_t cond_sleep;
pthread_cond_t cond_stdout;
pthread_cond_t cond_cola_susp_ready;
pthread_cond_t cond_cola_susp_block;

void wait_mutex(t_mutex *mutex, int pid_proceso)
{
    if (mutex->estado == 0)
    {
        // El recurso está libre, lo toma el proceso actual
        mutex->estado = 1;
        mutex->pid_proceso = pid_proceso;

        log_info(log_ks, "## (%d) Toma el Mutex %s", pid_proceso, mutex->nombre_mutex);
    }
    else
    {
        // El recurso está ocupado, el proceso debe bloquearse
        list_add(mutex->cola_procesos, (void *)(intptr_t)pid_proceso);

        // IMPORTANTE: Aca iria la logica para bloquear ese proceso: EXEC -> BLOCK
        // enviar_a_block(pid);
    }
}

void signal_mutex(t_mutex *mutex)
{
    // Solo el dueño deberia poder liberar el mutex (por logica de proteccion)
    if (!list_is_empty(mutex->cola_procesos))
    {
        // Pasamos el mutex al siguiente proceso en la cola
        int proximo_pid = (int)(intptr_t)list_remove(mutex->cola_procesos, 0);
        mutex->pid_proceso = proximo_pid;

        log_info(log_ks, "## (%d) Toma el Mutex %s", mutex->pid_proceso, mutex->nombre_mutex);

        // Logica de pasar a ready: BLOCK -> READY
        // despertar_proceso(mutex->pid_proceso);
    }
    else
    {
        // No hay nadie esperando, el recurso queda libre
        mutex->estado = 0;
        mutex->pid_proceso = -1;
        log_info(log_ks, "## Proceso libero Mutex %s. Estado: LIBRE", mutex->nombre_mutex);
    }
}
