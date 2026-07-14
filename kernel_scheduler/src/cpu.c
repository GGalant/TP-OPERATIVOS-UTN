#include "kernel_scheduler.h"
#include "cpu.h"
#include "planificacion.h"
#include "io.h"

void *handle_cpu(void *arg)
{
    int socket_cpu = *(int *)arg;
    int cpu_id = recibir_cpu_id(socket_cpu);
    t_cpu *cpu = NULL; // liberar memoria cuando se desconecte
    free(arg);

    cpu = crear_cpu(socket_cpu, cpu_id);    

    if (cpu == NULL) {
        perror("Error al crear CPU");
        close(socket_cpu);
        free(cpu);
        return NULL;
    }

    pthread_cond_signal(&cond_lista_cpu_conectadas);

    while (true)
    {
        // aca iria la logica de operaciones con CPU
        op_code codigo_operacion = recibir_operacion(cpu->socket_cpu);

        log_info(log_ks, "## Proceso ID(<%d>) - Solicitó syscall: <%s>", cpu->pid_actual, operacion_to_string(codigo_operacion));

        switch (codigo_operacion)
        {
        case SYSCALL_INIT_PROC:
            t_list *paquete_init_proc = recibir_paquete(cpu->socket_cpu);
            char *path = (char*)list_get(paquete_init_proc, 0);
            int prioridad = *(int *)list_get(paquete_init_proc, 1);

            log_debug(log_ks, "SYSCALL_INIT_PROC: Paquete recibido");

            pthread_mutex_lock(&mutex_cola_exec);
            t_proceso_pcb *proceso_padre = remover_proceso_por_pid(cola_exec, cpu->pid_actual);
            pthread_mutex_unlock(&mutex_cola_exec);

            log_debug(log_ks, "SYSCALL_INIT_PROC: Proceso original removido de la cola");
            
            if (proceso_padre == NULL)
            {
                log_error(log_ks, "Error: No se encontró el proceso con PID %d en la cola de EXEC.", cpu->pid_actual);
                break;
            }

            // Liberamos la CPU para que el planificador pueda usarla
            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            log_debug(log_ks, "SYSCALL_INIT_PROC: CPU <%d> liberada", cpu->cpu_id);

            
            pid_t pid_hijo = generar_pid();
            enviar_proceso(conexion_km, pid_hijo, path);
            list_destroy_and_destroy_elements(paquete_init_proc, free);

            log_debug(log_ks, "Esperando confirmacion de Kernel Memory (INICIALIZAR_PROCESO_OK)...");
            sem_wait(&sem_inicializar_proceso);
            log_debug(log_ks, "Recibida confirmacion de Kernel Memory (INICIALIZAR_PROCESO_OK)");

            t_proceso_pcb* pcb_hijo = crear_pcb(pid_hijo, prioridad);

            // Usar la nueva función productora genérica para agregar procesos a ready
            agregar_proceso_a_ready(proceso_padre);
            agregar_proceso_a_ready(pcb_hijo);

            break;
        case SYSCALL_EXIT:
            log_info(log_debug_ks, "Finaliza el proceso %d - Motivo: EXIT", cpu->pid_actual);
            // Cpu queda libre, liberar el struct del pcb
            //No solo le vamos a mandar a cpu aviso, si no también a km.
            
            pthread_mutex_lock(&mutex_cola_exec);
            t_proceso_pcb *proceso_exit = remover_proceso_por_pid(cola_exec, cpu->pid_actual);
            pthread_mutex_unlock(&mutex_cola_exec);

            if (proceso_exit == NULL)
            {
                log_error(log_ks, "Error: No se encontró el proceso con PID %d en la cola de EXEC.", cpu->pid_actual);
                break;
            }

            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

            pthread_mutex_lock(&mutex_cola_exit);
            cambiar_estado_proceso(proceso_exit, EXIT);
            list_add(cola_exit, proceso_exit);
            pthread_mutex_unlock(&mutex_cola_exit);
            break;
        case SYSCALL_SLEEP: {
            t_list *paquete_sleep = recibir_paquete(cpu->socket_cpu);
            int tiempo_sleep = *(int *)list_get(paquete_sleep, 0);
            list_destroy_and_destroy_elements(paquete_sleep, free);

            log_debug(log_debug_ks,"REcibo paquete luego de SYSCALL_SLEEP");
            
            // Guardamos el PID antes de modificar la CPU
            pid_t pid = cpu->pid_actual;

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
            pthread_mutex_lock(&mutex_cola_block);
            cambiar_estado_proceso(proceso, BLOCK);
            list_add(cola_block, proceso);
            pthread_mutex_unlock(&mutex_cola_block);

            iniciar_temporizador_timeout(cpu->pid_actual);

            // Liberar la CPU
            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            log_debug(log_debug_ks, "## CPU ID<%d> liberada.", cpu->cpu_id);

            enviar_proceso_a_io_sleep(pid, tiempo_sleep);

            log_info(log_debug_ks, "PID: %d - Bloqueado por: SLEEP", pid);

            break;
        }
        case SYSCALL_STDOUT: {
            log_info(log_ks, "## (%d) - Solicitó syscall: STDOUT", cpu->pid_actual);

            t_list *paquete_stdout = recibir_paquete(cpu->socket_cpu);
            pid_t pid;
            uint32_t direccion_fisica = *(uint32_t *)list_get(paquete_stdout, 0);
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
            pid = proceso->pid;
            list_add(cola_block, proceso);
            pthread_mutex_unlock(&mutex_cola_block);

            iniciar_temporizador_timeout(cpu->pid_actual);

            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            log_debug(log_debug_ks, "## CPU ID<%d> liberada.", cpu->cpu_id);

            char *contenido = solicitar_contenido_a_km(conexion_km, proceso->pid, direccion_fisica, tamanio);

            enviar_proceso_a_io_stdout(pid, contenido);

            break;
        }
        case INTERRUPCION_FIN_QUANTUM: {
            // La CPU nos avisa el fin de quantum
            t_list *paquete_quantum = recibir_paquete(cpu->socket_cpu);

            pid_t pid = *(int *)list_get(paquete_quantum, 0);
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

            // Usar la nueva función productora genérica para agregar procesos a ready
            agregar_proceso_a_ready(proceso);

            pthread_mutex_lock(&mutex_lista_cpu_conectadas);

            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);

            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

            log_info(log_ks, "## (%d) - Desalojado por fin de quantum", pid);

            break;
        }
        case SYSCALL_MEM_ALLOC:
        // *** IMPORTANTE: Esta syscall y las que esten relacionadas a memoria, deberian volver a ejecutar el proceso en la CPU que la solicito
            t_list* paquete_mem_alloc = recibir_paquete(cpu->socket_cpu);
            
            pid_t pid = cpu->pid_actual;
            int id_segmento = *(int*)list_get(paquete_mem_alloc, 0);
            int tamanio_segmento = *(int*)list_get(paquete_mem_alloc, 1);

            t_paquete *paquete_segmento = crear_paquete(CREACION_DE_SEGMENTO);
            agregar_a_paquete(paquete_segmento, &pid, sizeof(int));
            agregar_a_paquete(paquete_segmento, &id_segmento, sizeof(int));
            agregar_a_paquete(paquete_segmento, &tamanio_segmento, sizeof(int));

            // Aca deberia ir un mutex para que la creacion del segmento lo haga un proceso a la vez.
            enviar_paquete_send_all(paquete_segmento, conexion_km);
            eliminar_paquete(paquete_segmento);

            op_code respuesta_km = recibir_operacion(conexion_km);

            if (respuesta_km == CREACION_DE_SEGMENTO_OK) {
                log_debug(log_ks, "La Creacion de Segmento fue exitosa, devolviendo el proceso a la CPU");
                enviar_proceso_a_cpu(cpu->socket_cpu, pid);
                break;
            }

            if (respuesta_km == DISPARO_COMPACTACION) {

                pthread_mutex_lock(&mutex_lista_cpu_conectadas);
                
                //TODO 1. MOVER PROCESOS DE COLA READY A UNA COLA AUXILIAR 
                
                for (int i = 0; i < list_size(lista_cpu_conectadas); i++) {
                    t_cpu* cpu_a_desalojar = list_get(lista_cpu_conectadas, i);
                    cpu_a_desalojar->estado = CPU_LIBRE;
                    cpu_a_desalojar->pid_actual = -1;
                    
                    pthread_mutex_lock(&mutex_cola_exec);
                    t_proceso_pcb* proceso_a_desalojar = remover_proceso_por_pid(cola_exec, cpu_a_desalojar->pid_actual);
                    pthread_mutex_unlock(&mutex_cola_exec);

                    if (proceso_a_desalojar != NULL) {
                        // Usar la nueva función productora genérica para agregar procesos a ready
                        agregar_proceso_a_ready(proceso_a_desalojar);
                    }

                    send(cpu_a_desalojar->socket_cpu, &(op_code){INTERRUPCION_DESALOJO}, sizeof(op_code), 0);
                }

                //TODO 3. MOVER PROCESOS DE COLA AUXILIAR A COLA READY

                send(conexion_km, &(op_code){CPUS_DESALOJADAS}, sizeof(op_code), 0);
                respuesta_km = recibir_operacion(conexion_km);
                
                if (respuesta_km != COMPACTACION_OK) {
                    log_error(log_ks, "LA COMPACTACION FALLO");
                    break;
                }                
                 
                // Restaurar el proceso original a ready
                cpu->estado = CPU_OCUPADA;
                cpu->pid_actual = pid;
                enviar_proceso_a_cpu(cpu->socket_cpu, pid);
                
                pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
                pthread_cond_signal(&cond_lista_cpu_conectadas);
                
                break;
            }

            log_error(log_ks, "LA CREACION DE SEGMENTO FALLO (EL PROCESO FUE ASESINADO)");
            
            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            log_debug(log_debug_ks, "## CPU ID<%d> liberada.", cpu->cpu_id);
            
            break;
        case SYSCALL_MEM_FREE:
        
            // int id_segmento;
        
            // recv(socket, &id_segmento, sizeof(int), MSG_WAITALL);
            
            break;
        case SYSCALL_MUTEX_LOCK:
            break;
        case SYSCALL_MUTEX_UNLOCK:
            break;
        case ERROR:
            log_error(log_ks, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
            return NULL;
        default:
            log_warning(log_ks, "operacion desconocida: %d", codigo_operacion);
            break;
        }
    }

    free(cpu);
}

int recibir_cpu_id(int socket) {
    int cpu_id;
    int tamanio_recibido = recv(socket, &cpu_id, sizeof(int), MSG_WAITALL);
    if (tamanio_recibido > 0) 
    {
        log_debug(log_debug_ks, "cpu_id que me llega: %d", cpu_id);
        return cpu_id;
    }
    else 
    {
        perror("Error al recibir cpu_id");
        return -1;
    }
}

int generar_pid() { 
    pthread_mutex_lock(&mutex_generador_pid);
    pid_t pid = id_proceso_global++;
    pthread_mutex_unlock(&mutex_generador_pid);
    return pid;
}

t_cpu *crear_cpu(int socket_cpu, int cpu_id) {  
    t_cpu* cpu = malloc(sizeof(t_cpu));
    if (cpu == NULL) return NULL;

    cpu->socket_cpu = socket_cpu;
    cpu->estado = CPU_LIBRE;
    cpu->pid_actual = -1;
    cpu->cpu_id = cpu_id;

    pthread_mutex_lock(&mutex_lista_cpu_conectadas);
    list_add(lista_cpu_conectadas, cpu);
    pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

    log_info(log_ks, "## CPU ID<%d> Conectada", cpu->cpu_id);

    return cpu;
}

const char *operacion_to_string(op_code operacion)
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

t_proceso_pcb *buscar_proceso_por_id(pid_t pid, t_list *cola)
{
    for (int i = 0; i < list_size(cola); i++)
    {
        t_proceso_pcb *proceso = list_get(cola, i);
        if (proceso->pid == pid) return proceso;
    }
    return NULL;
}

// REFACTOR: estas funciones de enviar peticiones a IO podrían unificarse en una sola función que reciba el tipo de IO y los parametros necesarios. 
//enviar_peticion_io(tipo_io tipo, ...param restantes)

void enviar_proceso_a_io_sleep(pid_t pid, int tiempo_sleep)
{
    t_peticion_sleep *peticion = malloc(sizeof(t_peticion_sleep));
    peticion->pid = pid;
    peticion->tiempo_sleep = tiempo_sleep;

    pthread_mutex_lock(&mutex_sleep);
    list_add(cola_espera_sleep, peticion);
    // avisamos a algun hilo IO Sleep
    pthread_cond_signal(&cond_sleep);
    pthread_mutex_unlock(&mutex_sleep);

    log_debug(log_ks, "Se envia una peticion de sleep");

    return;
}

void enviar_proceso_a_io_stdout(pid_t pid, char *contenido) 
{
    t_peticion_stdout *peticion = malloc(sizeof(t_peticion_stdout));
    peticion->pid = pid;
    peticion->contenido = contenido;

    pthread_mutex_lock(&mutex_stdout);
    list_add(cola_espera_stdout, peticion);
    pthread_cond_signal(&cond_stdout);
    pthread_mutex_unlock(&mutex_stdout);
    
    return;
}
// void enviar_peticion_io_stdin(tipo_io tipo, ...parametros restantes)
