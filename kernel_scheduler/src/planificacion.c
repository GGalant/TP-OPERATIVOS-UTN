#include "kernel_scheduler.h"
#include "planificacion.h"
#include "cpu.h"
#include <string.h>

typedef struct {
    int cpu_id;
    pid_t pid;
} t_args_quantum;

void cancelar_hilo_quantum_si_existe(t_proceso_pcb* proceso, const char* motivo) {
    if (proceso != NULL && proceso->tiene_hilo_quantum) {
        // No es necesario hacer join, el hilo se cancelará en el usleep
        pthread_cancel(proceso->id_hilo_quantum);
        proceso->tiene_hilo_quantum = false;
        log_debug(log_debug_ks, "Hebra quantum para PID<%d> cancelada. Motivo: %s.", proceso->pid, motivo);
    }
}

/**
 * inicializar_planificador: Inicializa la estructura de colas ready según el algoritmo.
 * - FIFO/RR: 1 cola
 * - CMN: N colas según QUEUES_ALGORITHMS de la config
 */
void inicializar_planificador()
{
    log_debug(log_debug_ks, "Inicializando planificador con algoritmo: %d", algoritmo_global);

    if (algoritmo_global == FIFO || algoritmo_global == RR) {
        cantidad_niveles = 1;
        array_colas_ready = malloc(sizeof(t_cola_nivel));
        if (array_colas_ready == NULL) {
            log_error(log_ks, "Error al asignar memoria para array_colas_ready");
            return;
        }

        array_colas_ready[0].cola = list_create();
        pthread_mutex_init(&array_colas_ready[0].mutex, NULL);
        array_colas_ready[0].algoritmo = algoritmo_global;

        log_info(log_ks, "Planificador inicializado con 1 cola (%s)", 
                 algoritmo_global == FIFO ? "FIFO" : "RR");

    } else if (algoritmo_global == CMN) {
        // Obtener array de strings desde la config
        char** queues_algorithms = config_get_array_value(config, "QUEUES_ALGORITHMS");
        if (queues_algorithms == NULL) {
            log_error(log_ks, "QUEUES_ALGORITHMS no definido en config para CMN");
            cantidad_niveles = 1;
            array_colas_ready = malloc(sizeof(t_cola_nivel));
            array_colas_ready[0].cola = list_create();
            pthread_mutex_init(&array_colas_ready[0].mutex, NULL);
            array_colas_ready[0].algoritmo = FIFO;
            return;
        }

        // Contar cantidad de niveles
        cantidad_niveles = 0;
        for (int i = 0; queues_algorithms[i] != NULL; i++) {
            cantidad_niveles++;
        }

        log_debug(log_debug_ks, "CMN: cantidad_niveles = %d", cantidad_niveles);

        // Asignar memoria para el array de colas
        array_colas_ready = malloc(sizeof(t_cola_nivel) * cantidad_niveles);
        if (array_colas_ready == NULL) {
            log_error(log_ks, "Error al asignar memoria para array_colas_ready (CMN)");
            return;
        }

        // Inicializar cada cola con su algoritmo
        for (int i = 0; i < cantidad_niveles; i++) {
            array_colas_ready[i].cola = list_create();
            pthread_mutex_init(&array_colas_ready[i].mutex, NULL);
            
            t_algoritmo_planificacion algo = string_to_algoritmo(queues_algorithms[i]);
            array_colas_ready[i].algoritmo = algo;

            log_debug(log_debug_ks, "Cola %d inicializada con algoritmo: %d", i, algo);
        }

        log_info(log_ks, "Planificador CMN inicializado con %d niveles", cantidad_niveles);

    } else {
        log_error(log_ks, "Algoritmo de planificación inválido: %d", algoritmo_global);
        cantidad_niveles = 0;
        array_colas_ready = NULL;
    }

    // Inicializar semáforo contador
    sem_init(&sem_procesos_ready, 0, 0);
    log_debug(log_debug_ks, "Semáforo sem_procesos_ready inicializado");
}

/**
 * liberar_planificador: Libera recursos del planificador.
 */
void liberar_planificador()
{
    if (array_colas_ready != NULL) {
        for (int i = 0; i < cantidad_niveles; i++) {
            list_destroy(array_colas_ready[i].cola);
            pthread_mutex_destroy(&array_colas_ready[i].mutex);
        }
        free(array_colas_ready);
        array_colas_ready = NULL;
    }
    
    sem_destroy(&sem_procesos_ready);
    log_debug(log_debug_ks, "Planificador liberado");
}

/**
 * hilo_planificador_principal: Hilo consumidor genérico.
 * Implementa la lógica del patrón Productor-Consumidor:
 * 1. sem_wait(&sem_procesos_ready)
 * 2. Loop por niveles de prioridad
 * 3. Buscar CPU libre
 * 4. Enviar proceso a CPU
 * 5. Repetir
 */
void* hilo_planificador_principal(void* arg)
{
    (void)arg; // unused
    
    log_info(log_ks, "Hilo planificador principal iniciado");

    while (true) {
        // PASO 1: Esperar a que haya un proceso en ready
        sem_wait(&sem_procesos_ready);

        if (bsod_activo) {
            log_debug(log_debug_ks, "BSOD activo. Finalizando hilo planificador principal.");
            break;
        }

        log_debug(log_debug_ks, "Semáforo decrementado, buscando proceso en colas ready...");

        // PASO 2: Buscar proceso respetando prioridad (FIFO sobre CMN)
        t_proceso_pcb* proceso = NULL;
        t_algoritmo_planificacion algoritmo_cola = FIFO;
        int indice_cola_obtenida = -1;

        for (int i = 0; i < cantidad_niveles; i++) {
            pthread_mutex_lock(&array_colas_ready[i].mutex);

            if (!list_is_empty(array_colas_ready[i].cola)) {
                proceso = list_remove(array_colas_ready[i].cola, 0);
                algoritmo_cola = array_colas_ready[i].algoritmo;
                indice_cola_obtenida = i;
                
                log_debug(log_debug_ks, "Proceso PID<%d> extraído de cola nivel %d", 
                         proceso->pid, i);
            }

            pthread_mutex_unlock(&array_colas_ready[i].mutex);

            // Si encontramos un proceso, salimos del loop de búsqueda
            if (proceso != NULL) {
                break;
            }
        }

        // Validar que se extrajo un proceso
        if (proceso == NULL) {
            log_error(log_debug_ks, "ERROR: sem_wait retornó pero no hay proceso en ninguna cola");
            continue; // Reintentar
        }

        // PASO 3: Buscar CPU libre (IMPORTANTE: No mantener mutex de ready)
        pthread_mutex_lock(&mutex_lista_cpu_conectadas);
        //Terminar BSOD
        t_cpu* cpu = NULL;
        while (!bsod_activo && (cpu = buscar_cpu_libre()) == NULL) {
            log_debug(log_debug_ks, "Esperando por CPU libre para PID<%d>...", proceso->pid);
            pthread_cond_wait(&cond_lista_cpu_conectadas, &mutex_lista_cpu_conectadas);
        }

        if (bsod_activo) {
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

            pthread_mutex_lock(&mutex_cola_exit);
            cambiar_estado_proceso(proceso, EXIT);
            list_add(cola_exit, proceso);
            pthread_mutex_unlock(&mutex_cola_exit);

            log_debug(log_debug_ks,
                      "BSOD activo. PID<%d> movido a EXIT y planificador finalizado.",
                      proceso->pid);
            break;
        }

        cpu->estado = CPU_OCUPADA;
        cpu->pid_actual = proceso->pid;
        
        pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

        log_debug(log_debug_ks, "CPU ID<%d> asignada a PID<%d>", cpu->cpu_id, proceso->pid);

        // PASO 4: Enviar proceso a CPU y mover a EXEC
        enviar_proceso_a_cpu(cpu->socket_cpu, proceso->pid);

        pthread_mutex_lock(&mutex_cola_exec);
        cambiar_estado_proceso(proceso, EXEC);
        list_add(cola_exec, proceso);
        pthread_mutex_unlock(&mutex_cola_exec);

        log_info(log_debug_ks, "Proceso PID<%d> enviado a CPU ID<%d>", 
                proceso->pid, cpu->cpu_id);

        // PASO 5: Si es RR, iniciar el quantum
        if (algoritmo_cola == RR) {
            iniciar_hilo_quantum(proceso, cpu->cpu_id);
            log_debug(log_debug_ks, "Quantum iniciado para PID<%d> en CPU<%d>", 
                     proceso->pid, cpu->cpu_id);
        }
    }

    return NULL;
}

/**
 * buscar_cpu_libre: Busca una CPU libre.
 * IMPORTANTE: Debe ser llamada con mutex_lista_cpu_conectadas bloqueado.
 */
t_cpu* buscar_cpu_libre()
{
    log_debug(log_debug_ks, "Buscando CPU libre...");
    
    for (int i = 0; i < list_size(lista_cpu_conectadas); i++) {
        t_cpu* cpu = list_get(lista_cpu_conectadas, i);
        if (cpu != NULL && cpu->estado == CPU_LIBRE) {
            log_debug(log_debug_ks, "¡CPU ID<%d> libre encontrada!", cpu->cpu_id);
            return cpu;
        }
    }
    
    log_debug(log_debug_ks, "No se encontraron CPUs libres");
    return NULL;
}

/**
 * iniciar_hilo_quantum: Inicia un hilo que controla el quantum (RR).
 */
void iniciar_hilo_quantum(t_proceso_pcb* proceso, int cpu_id)
{
    t_args_quantum* args = malloc(sizeof(t_args_quantum));
    if (args == NULL) {
        log_error(log_ks, "No se pudo reservar memoria para quantum del PID<%d>", proceso->pid);
        return;
    }

    args->cpu_id = cpu_id;
    args->pid = proceso->pid;

    // Guardamos el ID del hilo en el PCB para poder cancelarlo si es necesario
    pthread_create(&proceso->id_hilo_quantum, NULL, controlar_quantum, args);
    proceso->tiene_hilo_quantum = true;
    pthread_detach(proceso->id_hilo_quantum);
}

/**
 * enviar_proceso_a_cpu: Envía un proceso a una CPU para ejecución.
 */
void enviar_proceso_a_cpu(int socket_cpu, pid_t pid_proceso)
{
    op_code operation_code = EJECUTAR_PROCESO;
    send(socket_cpu, &operation_code, sizeof(op_code), 0);
    send(socket_cpu, &pid_proceso, sizeof(int), 0);

    log_debug(log_debug_ks, "Operación EJECUTAR_PROCESO enviada a CPU con PID<%d>", 
             pid_proceso);
}

/**
 * string_to_algoritmo: Convierte string a enum de algoritmo.
 */
t_algoritmo_planificacion string_to_algoritmo(const char* algoritmo)
{
    if (algoritmo == NULL) {
        return INVALIDO;
    }

    if (strcmp(algoritmo, "FIFO") == 0) {
        return FIFO;
    } 
    else if (strcmp(algoritmo, "RR") == 0) {
        return RR;
    }
    else if (strcmp(algoritmo, "CMN") == 0) {
        return CMN;
    }
    else {
        return INVALIDO;
    }
}

/**
 * controlar_quantum: Hilo que controla el tiempo de quantum.
 * Envía interrupción de fin de quantum cuando se agota el tiempo.
 */
void* controlar_quantum(void* arg)
{
    t_args_quantum* args = (t_args_quantum*)arg;
    pid_t pid = args->pid;
    int cpu_id = args->cpu_id;
    free(args);

    // Convertir microsegundos a milisegundos
    usleep(rr_quantum * 1000);

    log_debug(log_debug_ks, "Quantum agotado para PID<%d>", pid);

    // Buscar la CPU correspondiente
    pthread_mutex_lock(&mutex_lista_cpu_conectadas);

    t_cpu* cpu = NULL;
    for (int i = 0; i < list_size(lista_cpu_conectadas); i++) {
        t_cpu* cpu_tmp = list_get(lista_cpu_conectadas, i);
        if (cpu_tmp != NULL && cpu_tmp->cpu_id == cpu_id) {
            cpu = cpu_tmp;
            break;
        }
    }

    if (cpu != NULL && cpu->estado == CPU_OCUPADA && cpu->pid_actual == pid) {
        op_code codigo = INTERRUPCION_FIN_QUANTUM;
        send(cpu->socket_cpu, &codigo, sizeof(op_code), 0);
        
        pthread_mutex_lock(&mutex_cola_exec);

        t_proceso_pcb *proceso = remover_proceso_por_pid(cola_exec, pid);

        pthread_mutex_unlock(&mutex_cola_exec);

        if (proceso == NULL)
        {
            log_error(log_ks, "No se encontró el proceso %d en EXEC por fin de quantum", pid);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            return NULL;
        }

        // El hilo cumplió su propósito, reseteamos el flag
        proceso->tiene_hilo_quantum = false;
        agregar_proceso_a_ready(proceso);

        cpu->estado = CPU_LIBRE;
        cpu->pid_actual = -1;
        pthread_cond_signal(&cond_lista_cpu_conectadas);

        log_info(log_ks, "## (%d) - Desalojado por fin de quantum", pid);

        log_debug(log_debug_ks, 
                 "Interrupción de fin de quantum enviada a CPU ID<%d> para PID<%d>",
                 cpu_id, pid);
    } else {
        log_debug(log_debug_ks, 
                 "CPU o proceso no coincide para fin de quantum (PID<%d> esperado en CPU<%d>)",
                 pid, cpu_id);
    }

    pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

    return NULL;
}

/**
 * handle_planificacion: Legacy - Wrapper que delega al hilo principal.
 * Se mantiene por compatibilidad pero simplemente inicia el hilo consumidor.
 */
void *handle_planificacion(void *arg)
{
    (void)arg; // unused
    
    log_debug(log_ks, "handle_planificacion: iniciando hilo planificador principal");
    return hilo_planificador_principal(NULL);
}
