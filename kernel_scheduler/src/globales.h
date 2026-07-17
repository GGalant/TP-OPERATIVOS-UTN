#ifndef GLOBALES_H_
#define GLOBALES_H_

#include <pthread.h>
#include <commons/collections/list.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    FIFO,
    RR,
    CMN,
    INVALIDO
} t_algoritmo_planificacion;

typedef enum {  
    NEW,
    READY,
    BLOCK,
    EXEC,
    EXIT,
    SUSP_READY,
    SUSP_BLOCK 
} t_estado_proceso;

typedef struct t_proceso_pcb {
    pid_t pid;
    int prioridad;
    int prioridad_original;
    int pc;
    int quantum;
    t_estado_proceso estado;
    char* nombre_mutex;
    pthread_t id_hilo_quantum;
    bool tiene_hilo_quantum;
    time_t tiempo_en_suspension;
} t_proceso_pcb;
typedef struct
{
    char *nombre_mutex;
    int estado; // 0 "libre" o 1 "ocupado"
    pid_t pid_proceso;
    t_list *cola_procesos; // Habria que ver si es solo el pid del proceso o todo el pcb (por ahora asumimos que es el pid)
} t_mutex;

typedef struct
{
    t_list* cola;
    pthread_mutex_t mutex;
    t_algoritmo_planificacion algoritmo;
} t_cola_nivel;

typedef struct
{
    pid_t pid;
    int tiempo_sleep;
} t_peticion_sleep;

typedef struct
{
    pid_t pid;
    char* contenido;
} t_peticion_stdout;

typedef struct 
{
    pid_t pid;
    uint32_t tamanio;
    uint32_t direccion_fisica;
} t_peticion_stdin;

// Declaraciones de variables globales - Planificación
extern t_cola_nivel* array_colas_ready;
extern int cantidad_niveles;
extern sem_t sem_procesos_ready;
extern t_algoritmo_planificacion algoritmo_global;
extern bool queue_preemption;

// Declaraciones de variables globales - Colas de estados obsoletas (excepto cola_ready, mutex_cola_ready, cond_cola_ready que se eliminan)
extern t_list *cola_block;
extern t_list *cola_exec;
extern t_list *cola_exit;
extern t_list *lista_cpu_conectadas;
extern t_list *lista_io_conectadas;
extern t_list *cola_susp_block;
extern t_list *cola_susp_ready;
extern t_list *cola_espera_sleep;
extern t_list *cola_espera_stdout;
extern t_list *cola_espera_stdin;
extern t_list *lista_mutex;
extern t_log *log_ks;
extern t_log *log_ks;
extern t_config *config;
extern char *config_path;
extern char *path_proceso_inicial;
extern char *puerto;
extern char *str_log_level;
extern char *puerto_km;
extern char *ip_km;
extern char *algoritmo_planificacion;
extern char* buffer_global_contenido_km;
extern char* buffer_respuesta_creacion_segmento;
extern uint32_t buffer_global_tamanio_contenido_km;
extern int *conexion_km_ptr;
extern int server_ks;
extern int conexion_km;
extern const int PRIORIDAD_MAXIMA;
extern int log_level;
extern int id_io_global;
extern pid_t id_proceso_global;
extern int rr_quantum;
extern int suspension_timeout;
extern pthread_mutex_t mutex_cola_block;
extern pthread_mutex_t mutex_cola_exec;
extern pthread_mutex_t mutex_cola_exit;
extern pthread_mutex_t mutex_lista_cpu_conectadas;
extern pthread_mutex_t mutex_lista_io_conectadas;
extern pthread_mutex_t mutex_generador_id_io;
extern pthread_mutex_t mutex_generador_pid;
extern pthread_mutex_t mutex_sleep;
extern pthread_mutex_t mutex_stdout;
extern pthread_mutex_t mutex_stdin;
extern pthread_mutex_t mutex_cola_susp_block;
extern pthread_mutex_t mutex_cola_susp_ready;
extern pthread_mutex_t mutex_lista_mutex;
extern pthread_mutex_t mutex_solicitud_km;
extern pthread_mutex_t mutex_mem_alloc;
extern pthread_cond_t cond_cola_block;
extern pthread_cond_t cond_lista_cpu_conectadas;
extern pthread_cond_t cond_sleep;
extern pthread_cond_t cond_stdout;
extern pthread_cond_t cond_stdin;
extern pthread_cond_t cond_cola_susp_ready;
extern pthread_cond_t cond_cola_susp_block;
extern sem_t sem_inicializar_proceso;
extern sem_t sem_respuesta_contenido_km;
extern sem_t sem_creacion_segmento;
extern sem_t sem_confirmacion_escritura;
extern sem_t sem_compactacion_ok;
extern sem_t sem_desuspension_proceso;
extern sem_t sem_solicitud_mem_free;
extern bool bsod_activo;
extern bool compactacion_exitosa;
extern bool desuspension_exitosa;
extern bool mem_free_exitoso;

void wait_mutex(t_mutex *mutex, pid_t pid_proceso);
void signal_mutex(t_mutex *mutex);
void agregar_proceso_a_ready(t_proceso_pcb* proceso);
void chequear_desalojo(t_proceso_pcb* proceso_nuevo);
t_proceso_pcb* remover_proceso_por_pid(t_list* cola, pid_t pid);
t_proceso_pcb* buscar_proceso_por_id(pid_t pid, t_list* cola);
void cancelar_hilo_quantum_si_existe(t_proceso_pcb* proceso, const char* motivo);

void desuspender_procesos();
#endif /* GLOBALES_H_ */
