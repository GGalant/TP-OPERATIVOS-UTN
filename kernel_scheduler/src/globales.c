#include "kernel_scheduler.h"

// Planificación - nuevas estructuras
t_cola_nivel* array_colas_ready;
int cantidad_niveles;
sem_t sem_procesos_ready;
t_algoritmo_planificacion algoritmo_global;
bool queue_preemption;

// Colas de estados
t_list *cola_block;
t_list *cola_exec;
t_list *cola_exit;
t_list *lista_cpu_conectadas;
t_list *lista_io_conectadas;
t_list *cola_espera_sleep;
t_list *cola_espera_stdout;
t_list *cola_espera_stdin;
t_list *cola_susp_block;
t_list *cola_susp_ready;
t_list *lista_mutex;
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
char* buffer_global_contenido_km = NULL;
char* buffer_respuesta_creacion_segmento = NULL;
uint32_t buffer_global_tamanio_contenido_km = 0;
const int PRIORIDAD_MAXIMA = 0;
int *conexion_km_ptr;
int id_io_global = -1;
pid_t id_proceso_global = 1;
int server_ks;
int conexion_km;
int log_level;
int suspension_timeout;
int rr_quantum;
bool bsod_activo = false; //avisa a los hilos que tienen q terminar

pthread_mutex_t mutex_cola_block;
pthread_mutex_t mutex_cola_exec;
pthread_mutex_t mutex_cola_exit;
pthread_mutex_t mutex_lista_cpu_conectadas;
pthread_mutex_t mutex_lista_io_conectadas;
pthread_mutex_t mutex_generador_id_io;
pthread_mutex_t mutex_generador_pid;
pthread_mutex_t mutex_sleep;
pthread_mutex_t mutex_stdout;
pthread_mutex_t mutex_stdin;
pthread_mutex_t mutex_cola_susp_block;
pthread_mutex_t mutex_cola_susp_ready;
pthread_mutex_t mutex_lista_mutex;
pthread_mutex_t mutex_solicitud_km;
pthread_mutex_t mutex_mem_alloc;
pthread_cond_t cond_cola_block;
pthread_cond_t cond_lista_cpu_conectadas;
pthread_cond_t cond_sleep;
pthread_cond_t cond_stdout;
pthread_cond_t cond_stdin;
pthread_cond_t cond_cola_susp_ready;
pthread_cond_t cond_cola_susp_block;
sem_t sem_inicializar_proceso;
sem_t sem_respuesta_contenido_km;
sem_t sem_creacion_segmento;
sem_t sem_confirmacion_escritura;
sem_t sem_compactacion_ok;
sem_t sem_desuspension_proceso;
sem_t sem_solicitud_mem_free;
bool compactacion_exitosa;
bool desuspension_exitosa;
bool mem_free_exitoso;

void wait_mutex(t_mutex *mutex, pid_t pid_proceso)
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
        pid_t* pid_en_espera = malloc(sizeof(pid_t));
        *pid_en_espera = pid_proceso;
        list_add(mutex->cola_procesos, pid_en_espera);

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
        int* pid_ptr = list_remove(mutex->cola_procesos, 0);
        pid_t proximo_pid = *pid_ptr;
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

/**
 * agregar_proceso_a_ready: Función productora genérica para agregar procesos a ready.
 * Implementa la regla: CMN usa prioridad como índice, FIFO/RR siempre usa índice 0.
 * Si CMN y queue_preemption es true, invoca chequear_desalojo antes de post.
 */
void agregar_proceso_a_ready(t_proceso_pcb* proceso)
{
    if (proceso == NULL || array_colas_ready == NULL) {
        log_error(log_ks, "agregar_proceso_a_ready: proceso o array_colas_ready es NULL");
        return;
    }

    // Determina el índice según el algoritmo global
    int indice_cola;
    if (algoritmo_global == CMN) {
        indice_cola = proceso->prioridad;
        // Validar que el índice esté dentro de rango
        if (indice_cola < 0 || indice_cola >= cantidad_niveles) {
            log_error(log_ks, "agregar_proceso_a_ready: prioridad %d fuera de rango [0, %d]", 
                      indice_cola, cantidad_niveles-1);
            indice_cola = 0; // fallback a la primera cola
        }
    } else {
        // FIFO o RR: siempre índice 0
        indice_cola = 0;
    }

    // Lock del mutex de la cola específica
    pthread_mutex_lock(&array_colas_ready[indice_cola].mutex);
    list_add(array_colas_ready[indice_cola].cola, proceso);
    cambiar_estado_proceso(proceso, READY);
    pthread_mutex_unlock(&array_colas_ready[indice_cola].mutex);

    log_debug(log_debug_ks, "Proceso PID<%d> agregado a READY en cola nivel %d", 
              proceso->pid, indice_cola);

    // Si CMN y queue_preemption está habilitado, chequear desalojo
    if (algoritmo_global == CMN && queue_preemption) {
        chequear_desalojo(proceso);
    }

    // Signal al semáforo contador
    sem_post(&sem_procesos_ready);
}

/**
 * agregar_proceso_a_ready_al_inicio: Función productora para agregar procesos al inicio de la cola de ready.
 * Se utiliza para los procesos desalojados por compactación, para que tengan prioridad.
 */
void agregar_proceso_a_ready_al_inicio(t_proceso_pcb* proceso)
{
    if (proceso == NULL || array_colas_ready == NULL) {
        log_error(log_ks, "agregar_proceso_a_ready_al_inicio: proceso o array_colas_ready es NULL");
        return;
    }

    // Determina el índice según el algoritmo global
    int indice_cola;
    if (algoritmo_global == CMN) {
        indice_cola = proceso->prioridad;
        // Validar que el índice esté dentro de rango
        if (indice_cola < 0 || indice_cola >= cantidad_niveles) {
            log_error(log_ks, "agregar_proceso_a_ready_al_inicio: prioridad %d fuera de rango [0, %d]", 
                      indice_cola, cantidad_niveles-1);
            indice_cola = 0; // fallback a la primera cola
        }
    } else {
        // FIFO o RR: siempre índice 0
        indice_cola = 0;
    }

    // Lock del mutex de la cola específica
    pthread_mutex_lock(&array_colas_ready[indice_cola].mutex);
    list_add_in_index(array_colas_ready[indice_cola].cola, 0, proceso);
    cambiar_estado_proceso(proceso, READY);
    pthread_mutex_unlock(&array_colas_ready[indice_cola].mutex);

    log_debug(log_debug_ks, "Proceso PID<%d> agregado al INICIO de READY en cola nivel %d", proceso->pid, indice_cola);

    // Signal al semáforo contador
    sem_post(&sem_procesos_ready);
}

/**
 * chequear_desalojo: Placeholder para lógica de preemption en CMN.
 */
void chequear_desalojo(t_proceso_pcb* proceso_nuevo)
{
    // TODO: Implementar lógica de interrupción/desalojo
    log_debug(log_debug_ks, "chequear_desalojo: PID<%d> - preemption check (placeholder)", 
              proceso_nuevo->pid);
}

bool comparar_procesos_para_desuspender(void* a, void* b) {
    t_proceso_pcb* pcb_a = (t_proceso_pcb*)a;
    t_proceso_pcb* pcb_b = (t_proceso_pcb*)b;

    if (pcb_a->prioridad < pcb_b->prioridad) {
        return true;
    }
    if (pcb_a->prioridad > pcb_b->prioridad) {
        return false;
    }
    // Misma prioridad, va primero el que lleva mas tiempo suspendido
    return pcb_a->tiempo_en_suspension < pcb_b->tiempo_en_suspension;
}

void desuspender_procesos() {
    pthread_mutex_lock(&mutex_cola_susp_ready);
    if (list_is_empty(cola_susp_ready)) {
        pthread_mutex_unlock(&mutex_cola_susp_ready);
        return;
    }

    list_sort(cola_susp_ready, comparar_procesos_para_desuspender);

    bool seguir_desuspendiendo = true;
    while (seguir_desuspendiendo && !list_is_empty(cola_susp_ready)) {
        t_proceso_pcb* proceso = list_get(cola_susp_ready, 0);

        log_info(log_ks, "Intentando desuspender proceso PID <%d> con prioridad <%d>", proceso->pid, proceso->prioridad);

        op_code op = DESUSPENDER_PROCESO;
        send(conexion_km, &op, sizeof(op_code), 0);
        send(conexion_km, &(proceso->pid), sizeof(pid_t), 0);

        sem_wait(&sem_desuspension_proceso);

        if (desuspension_exitosa) {
            proceso = list_remove(cola_susp_ready, 0);
            log_info(log_ks, "Proceso PID <%d> desuspendido exitosamente. Moviendo a READY.", proceso->pid);
            agregar_proceso_a_ready(proceso);
        } else {
            log_warning(log_ks, "No hay memoria suficiente para desuspender PID <%d>. Deteniendo proceso de desuspensión.", proceso->pid);
            seguir_desuspendiendo = false;
        }
    }
    pthread_mutex_unlock(&mutex_cola_susp_ready);
}