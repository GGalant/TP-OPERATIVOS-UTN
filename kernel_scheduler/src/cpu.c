#include "kernel_scheduler.h"
#include "cpu.h"
#include "planificacion.h"
#include "io.h"

void enviar_proceso_a_io_sleep(int pid, int tiempo_sleep);

void *handle_cpu(void *arg)
{
    int socket_cpu = *(int *)arg;
    t_cpu *cpu = NULL; // liberar memoria cuando se desconecte
    free(arg);

    cpu = crear_cpu(socket_cpu);    
    if (cpu == NULL) {
        perror("Error al crear CPU");
        close(socket_cpu);
        free(cpu);
        return NULL;
    }

    while (true)
    {
        // aca iria la logica de operaciones con CPU
        op_code_ks_cpu codigo_operacion = recibir_operacion(socket_cpu);

        log_info(log_ks, "## Proceso ID(<%d>) - Solicitó syscall: <%s>", cpu->pid_actual, operacion_to_string(codigo_operacion));

        switch (codigo_operacion)
        {
        case SYSCALL_EXIT:
            log_info(log_debug_ks, "Finaliza el proceso %d - Motivo: EXIT", cpu->pid_actual);
            // Cpu queda libre, liberar el struct del pcb
            break;
        case SYSCALL_SLEEP: {
            log_info(log_debug_ks, "PID: %d - Bloqueado por: SLEEP", cpu->pid_actual);

            // Aca deberiamos recibir pcb actualizado (o por lo menos pc del proceso) y tiempo sleep
            t_list *paquete_sleep = recibir_paquete(socket_cpu);
            int pid = *(int *)list_get(paquete_sleep, 0);
            int pc_actualizado = *(int *)list_get(paquete_sleep, 1);
            int tiempo_sleep = *(int *)list_get(paquete_sleep, 2);
            list_destroy_and_destroy_elements(paquete_sleep, free);

            // Sacar el proceso de la cola de EXEC
            pthread_mutex_lock(&mutex_cola_exec);
            t_proceso_pcb *proceso = remover_proceso_por_pid(cola_exec, pid);
            pthread_mutex_unlock(&mutex_cola_exec);

            if (proceso == NULL)
            {
                log_error(log_ks, "Error: No se encontró el proceso con PID %d en la cola de EXEC.", pid);
                break;
            }

            // Actualizar PCB, cambiar estado y mover a la cola de BLOCK
            // la cola de block es gestionada por el planificador de mediano plazo
            proceso->pc = pc_actualizado;
            cambiar_estado_proceso(proceso, BLOCK);
            pthread_mutex_lock(&mutex_cola_block);
            list_add(cola_block, proceso);
            pthread_mutex_unlock(&mutex_cola_block);

            // Liberar la CPU
            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            log_debug(log_debug_ks, "## CPU ID<%d> liberada.", cpu->cpu_id);

            enviar_proceso_a_io_sleep(pid, tiempo_sleep);

            break;
        }
        case SYSCALL_STDOUT: {
            log_info(log_ks, "## (%d) - Solicitó syscall: STDOUT", cpu->pid_actual);

            t_list *paquete_stdout = recibir_paquete(socket_cpu);
            uint32_t direccion_logica = *(uint32_t *)list_get(paquete_stdout, 0);
            uint32_t tamanio = *(uint32_t *)list_get(paquete_stdout, 1);
            list_destroy_and_destroy_elements(paquete_stdout, free);

            pthread_mutex_lock(&mutex_cola_exec);
            t_proceso_pcb *proceso = remover_proceso_por_pid(cola_exec, cpu->pid_actual);
            pthread_mutex_unlock(&mutex_cola_exec);

            if (proceso == NULL)
            {
                log_error(log_ks, "Error: No se encontró el proceso con PID %d en la cola de EXEC.", cpu->pid_actual);
                break;
            }

            // proceso->pc = pc_actualizado; // No se recibe el PC actualizado en este syscall
            pthread_mutex_lock(&mutex_cola_block);
            cambiar_estado_proceso(proceso, BLOCK);
            //Aca deberiamos inicar el temporizador para el timeout
            list_add(cola_block, proceso);
            pthread_mutex_unlock(&mutex_cola_block);

            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            log_debug(log_debug_ks, "## CPU ID<%d> liberada.", cpu->cpu_id);

            char *contenido = solicitar_contenido_a_km(conexion_km, proceso->pid, direccion_logica, tamanio);

            // enviar_proceso_a_io_stdout();

            break;
        }
        case INTERRUPCION_FIN_QUANTUM: {
            // La CPU nos avisa el fin de quantum
            t_list *paquete_quantum = recibir_paquete(socket_cpu);

            int pid = *(int *)list_get(paquete_quantum, 0);
            int pc_actualizado = *(int *)list_get(paquete_quantum, 1);

            list_destroy_and_destroy_elements(paquete_quantum, free);

            pthread_mutex_lock(&mutex_cola_exec);

            t_proceso_pcb *proceso = remover_proceso_por_pid(cola_exec, pid);

            pthread_mutex_unlock(&mutex_cola_exec);

            if (proceso == NULL)
            {
                log_error(log_ks, "No se encontró el proceso %d en EXEC por fin de quantum", pid);
                break;
            }

            proceso->pc = pc_actualizado;

            pthread_mutex_lock(&mutex_cola_ready);

            cambiar_estado_proceso(proceso, READY);
            list_add(cola_ready, proceso);
            pthread_cond_signal(&cond_cola_ready);

            pthread_mutex_unlock(&mutex_cola_ready);

            pthread_mutex_lock(&mutex_lista_cpu_conectadas);

            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);

            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

            log_info(log_ks, "## (%d) - Desalojado por fin de quantum", pid);

            break;
        }
        case INTERRUPCION_DESALOJO:
            break;
        case SYSCALL_MUTEX_LOCK:
            break;
        case SYSCALL_MUTEX_UNLOCK:
            break;
        case SYSCALL_INIT_PROC:
            break;
        case -1:
            log_error(log_ks, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
            return NULL;
        default:
            log_warning(log_ks, "operacion desconocida: %d", codigo_operacion);
            break;
        }
    }

    free(cpu);
}

int generar_id_cpu() {
    pthread_mutex_lock(&mutex_generador_id_cpu);
    int id = ++id_cpu_global;
    pthread_mutex_unlock(&mutex_generador_id_cpu);
    return id;
}

t_cpu *crear_cpu(int socket_cpu) {  
    t_cpu* cpu = malloc(sizeof(t_cpu));
    if (cpu == NULL) return NULL;

    cpu->socket_cpu = socket_cpu;
    cpu->estado = CPU_LIBRE;
    cpu->pid_actual = -1;
    cpu->cpu_id = generar_id_cpu();

    pthread_mutex_lock(&mutex_lista_cpu_conectadas);
    list_add(lista_cpu_conectadas, cpu);
    pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

    log_info(log_ks, "## CPU ID<%d> Conectada", cpu->cpu_id);

    return cpu;
}

const char *operacion_to_string(op_code_ks_cpu operacion)
{
    switch (operacion)
    {
    case SYSCALL_EXIT:
        return "SYSCALL_EXIT";
    case SYSCALL_SLEEP:
        return "SYSCALL_SLEEP";
    case SYSCALL_STDIN:
        return "SYSCALL_STDIN";
    case SYSCALL_STDOUT:
        return "SYSCALL_STDOUT";
    case INTERRUPCION_FIN_QUANTUM:
        return "INTERRUPCION_FIN_QUANTUM";
    case INTERRUPCION_DESALOJO:
        return "INTERRUPCION_DESALOJO";
    case SYSCALL_MUTEX_LOCK:
        return "SYSCALL_MUTEX_LOCK";
    case SYSCALL_MUTEX_UNLOCK:
        return "SYSCALL_MUTEX_UNLOCK";
    case SYSCALL_INIT_PROC:
        return "SYSCALL_INIT_PROC";
    default:
        return "DESCONOCIDO";
    }
}

t_proceso_pcb *buscar_proceso_por_id(int pid, t_list *cola)
{
    for (int i = 0; i < list_size(cola); i++)
    {
        t_proceso_pcb *proceso = list_get(cola, i);
        if (proceso->pid == pid)
            ;
        return proceso;
    }
    return NULL;
}

bool es_proceso_buscado(void *elemento, void *pid_buscado)
{
    t_proceso_pcb *pcb = (t_proceso_pcb *)elemento;
    return pcb->pid == *(int *)pid_buscado;
}

// REFACTOR: estas funciones de enviar peticiones a IO podrían unificarse en una sola función que reciba el tipo de IO y los parametros necesarios, y dentro de la función se encargue de buscar la IO correspondiente y enviar la petición, algo así como enviar_peticion_io(tipo_io tipo, ...parametros restantes, donde parametros restantes sea un paquete con los datos necesarios)

void enviar_proceso_a_io_sleep(int pid, int tiempo_sleep)
{
    t_peticion_sleep *peticion = malloc(sizeof(t_peticion_sleep));
    peticion->pid = pid;
    peticion->tiempo_sleep = tiempo_sleep;

    pthread_mutex_lock(&mutex_sleep);
    list_add(cola_espera_sleep, peticion);

    // avisamos a algun hilo IO Sleep
    pthread_cond_signal(&cond_sleep);

    pthread_mutex_unlock(&mutex_sleep);
}

// void enviar_peticion_io_stdout(tipo_io tipo, ...parametros restantes)
// void enviar_peticion_io_stdin(tipo_io tipo, ...parametros restantes)

t_io *buscar_io_libre(tipo_io tipo, int pid)
{
    // *IMPORTANTE* esta funcion tiene que llamarse con el mutex de la lista de IO conectadas bloqueado
    t_io *io_libre = NULL;
    for (int i = 0; i < list_size(lista_io_conectadas); i++)
    {
        t_io *io = list_get(lista_io_conectadas, i);
        if (io->tipo == tipo && io->estado == LIBRE)
        {
            io->estado = OCUPADA;
            io->pid_actual = pid;
            io_libre = io;
            break;
        }
    }
    return io_libre;
}