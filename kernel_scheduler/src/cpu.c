#include "kernel_scheduler.h"
#include "cpu.h"
#include "planificacion.h"
#include "io.h"

void agregar_proceso_a_ready_al_inicio(t_proceso_pcb* pcb);

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

        log_info(log_ks, "## (%d) - Solicitó syscall: %s", cpu->pid_actual, operacion_to_string(codigo_operacion));

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

            cancelar_hilo_quantum_si_existe(proceso_padre, "SYSCALL_INIT_PROC");

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
            log_debug(log_debug_ks, "Finaliza el proceso %d - Motivo: EXIT", cpu->pid_actual);
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

            cancelar_hilo_quantum_si_existe(proceso_exit, "SYSCALL_EXIT");

            pid_t pid = cpu->pid_actual;
            
            op_code codigo_operacion = FINALIZAR_PROCESO;

            send(conexion_km, &codigo_operacion, sizeof(op_code), 0);
            send(conexion_km, &pid, sizeof(pid_t), 0);

            pthread_mutex_lock(&mutex_lista_cpu_conectadas);
            cpu->estado = CPU_LIBRE;
            cpu->pid_actual = -1;
            pthread_cond_signal(&cond_lista_cpu_conectadas);
            pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

            pthread_mutex_lock(&mutex_cola_exit);
            cambiar_estado_proceso(proceso_exit, EXIT);
            list_add(cola_exit, proceso_exit);
            pthread_mutex_unlock(&mutex_cola_exit);

             log_info(log_ks, "## (%d) finalizó su ejecución con motivo de EXIT", pid);

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

            cancelar_hilo_quantum_si_existe(proceso, "SYSCALL_SLEEP");

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

            cancelar_hilo_quantum_si_existe(proceso, "SYSCALL_STDOUT");

            // proceso->pc = pc_actualizado; // No se recibe el PC actualizado en este syscall
            pthread_mutex_lock(&mutex_cola_block);
            cambiar_estado_proceso(proceso, BLOCK);
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
        case SYSCALL_STDIN: {
            t_list *paquete_stdin = recibir_paquete(cpu->socket_cpu);

            pid_t pid;
            uint32_t tamanio = *(uint32_t *)list_get(paquete_stdin, 0);
            uint32_t direccion_fisica = *(uint32_t *)list_get(paquete_stdin, 1);
            list_destroy_and_destroy_elements(paquete_stdin, free);

            pthread_mutex_lock(&mutex_cola_exec);
            t_proceso_pcb *proceso = remover_proceso_por_pid(cola_exec, cpu->pid_actual);
            pthread_mutex_unlock(&mutex_cola_exec);

            if (proceso == NULL)
            {
                log_error(log_ks, "Error: No se encontró el proceso con PID %d en la cola de EXEC.", cpu->pid_actual);
                break;
            }

            pthread_mutex_lock(&mutex_cola_block);
            cambiar_estado_proceso(proceso, BLOCK);
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

            enviar_proceso_a_io_stdin(pid, tamanio, direccion_fisica);

            break;
        }
        case SYSCALL_MEM_ALLOC:
        { 
            t_list* paquete_mem_alloc = recibir_paquete(cpu->socket_cpu);
            
            pid_t pid = cpu->pid_actual;
            int id_segmento = *(int*)list_get(paquete_mem_alloc, 0);
            int tamanio_segmento = *(int*)list_get(paquete_mem_alloc, 1);
            list_destroy_and_destroy_elements(paquete_mem_alloc, free); // Liberar memoria del paquete

            pthread_mutex_lock(&mutex_mem_alloc);

            t_paquete *paquete_segmento = crear_paquete(CREACION_DE_SEGMENTO);
            agregar_a_paquete(paquete_segmento, &pid, sizeof(int));
            agregar_a_paquete(paquete_segmento, &id_segmento, sizeof(int));
            agregar_a_paquete(paquete_segmento, &tamanio_segmento, sizeof(int));

            enviar_paquete_send_all(paquete_segmento, conexion_km);
            eliminar_paquete(paquete_segmento);

            sem_wait(&sem_creacion_segmento);

            if (strcmp(buffer_respuesta_creacion_segmento, "CREACION_DE_SEGMENTO_OK") == 0) {
                enviar_proceso_a_cpu(cpu->socket_cpu, pid);
                pthread_mutex_unlock(&mutex_mem_alloc); 
                break; 
            } 
            else if (strcmp(buffer_respuesta_creacion_segmento, "DESALOJAR_CPU") == 0) {
               
                log_info(log_ks, "## Inicio de compactación");
                pthread_mutex_lock(&mutex_lista_cpu_conectadas);
                
                t_list* cola_auxiliar_ready = list_create();
                t_list* procesos_exec_desalojados = list_create();
                int procesos_movidos = 0;

                // 1. MOVER PROCESOS DE COLA READY A UNA COLA AUXILIAR 
                for (int i = 0; i < cantidad_niveles; i++) {
                    pthread_mutex_lock(&array_colas_ready[i].mutex);
                    while (!list_is_empty(array_colas_ready[i].cola)) {
                        sem_wait(&sem_procesos_ready); 
                        t_proceso_pcb* proceso = list_remove(array_colas_ready[i].cola, 0);
                        list_add(cola_auxiliar_ready, proceso);
                        procesos_movidos++;
                    }
                    pthread_mutex_unlock(&array_colas_ready[i].mutex);
                }
                log_debug(log_ks, "Compactación: Movidos %d procesos de READY a cola auxiliar.", procesos_movidos);
                
                // 2. DESALOJAR PROCESOS EN EJECUCIÓN Y GUARDARLOS EN OTRA LISTA AUXILIAR
                for (int i = 0; i < list_size(lista_cpu_conectadas); i++) {
                    t_cpu* cpu_a_desalojar = list_get(lista_cpu_conectadas, i);
                    pid_t pid_a_desalojar = cpu_a_desalojar->pid_actual; 

                    cpu_a_desalojar->estado = CPU_LIBRE;
                    cpu_a_desalojar->pid_actual = -1;
                    
                    if (pid_a_desalojar != -1) {
                        pthread_mutex_lock(&mutex_cola_exec);
                        t_proceso_pcb* proceso_desalojado = remover_proceso_por_pid(cola_exec, pid_a_desalojar);
                        pthread_mutex_unlock(&mutex_cola_exec);

                        if (proceso_desalojado != NULL) {
                            list_add(procesos_exec_desalojados, proceso_desalojado);
                        }
                        send(cpu_a_desalojar->socket_cpu, &(op_code){INTERRUPCION_DESALOJO}, sizeof(op_code), 0);
                    }
                }

                // 3. MOVER PROCESOS DE LAS COLAS AUXILIARES A LAS COLAS DE READY (AL INICIO)
                // Primero los que estaban en EXEC
                log_debug(log_ks, "Compactación: Restaurando %d procesos de EXEC a las colas de READY.", list_size(procesos_exec_desalojados));
                while(!list_is_empty(procesos_exec_desalojados)) {
                    t_proceso_pcb* proceso_a_restaurar = list_remove(procesos_exec_desalojados, 0);
                    agregar_proceso_a_ready_al_inicio(proceso_a_restaurar);
                }
                list_destroy(procesos_exec_desalojados);

                // Luego los que ya estaban en READY
                log_debug(log_ks, "Compactación: Restaurando %d procesos de READY a sus colas.", list_size(cola_auxiliar_ready));
                while(!list_is_empty(cola_auxiliar_ready)) {
                    t_proceso_pcb* proceso_a_restaurar = list_remove(cola_auxiliar_ready, 0);
                    agregar_proceso_a_ready_al_inicio(proceso_a_restaurar);
                }
                list_destroy(cola_auxiliar_ready);
                
                send(conexion_km, &(op_code){DESALOJAR_CPU_OK}, sizeof(op_code), 0);
                sem_wait(&sem_compactacion_ok);
                
                if (!compactacion_exitosa) {
                    log_error(log_ks, "LA COMPACTACION FALLO");
                     pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
                     pthread_mutex_unlock(&mutex_mem_alloc);
                    break;
                }

                log_info(log_ks, "## Fin de compactación");

                desuspender_procesos();
                 
                // Restaurar el proceso original a ready
                cpu->estado = CPU_OCUPADA;
                cpu->pid_actual = pid;
                
                pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
                pthread_cond_signal(&cond_lista_cpu_conectadas);

                enviar_proceso_a_cpu(cpu->socket_cpu, pid);
                
            } else {
                // Caso de fallo en la creación del segmento (proceso asesinado)
                log_error(log_ks, "LA CREACION DE SEGMENTO FALLO (EL PROCESO FUE ASESINADO)");
                
                pthread_mutex_lock(&mutex_lista_cpu_conectadas);
                cpu->estado = CPU_LIBRE;
                cpu->pid_actual = -1;
                pthread_cond_signal(&cond_lista_cpu_conectadas);
                pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
                log_debug(log_debug_ks, "## CPU ID<%d> liberada.", cpu->cpu_id);

                // Aquí se debería finalizar el proceso, similar a SYSCALL_EXIT
            }

            pthread_mutex_unlock(&mutex_mem_alloc);
            break;
        }
        case SYSCALL_MUTEX_CREATE: {
            t_list* paquete_mutex_create = recibir_paquete(cpu->socket_cpu);
            char* nombre_mutex = (char*)list_get(paquete_mutex_create, 0); // Obtenemos el nombre
            
            crear_mutex(nombre_mutex); // Creamos el mutex (hace strdup internamente)

            // Ahora que ya no lo necesitamos, liberamos el paquete
            list_destroy_and_destroy_elements(paquete_mutex_create, free);

            // Devolvemos el proceso a la CPU para que continúe su ejecución
            enviar_proceso_a_cpu(cpu->socket_cpu, cpu->pid_actual);
            break;
        }
        case SYSCALL_MUTEX_LOCK:
        {
            t_list* paquete_mutex_lock = recibir_paquete(cpu->socket_cpu);


            char* nombre_mutex = (char*)list_get(paquete_mutex_lock, 0);
            
            pid_t pid = cpu->pid_actual;

            pthread_mutex_lock(&mutex_lista_mutex);
            t_mutex* mutex = buscar_mutex(nombre_mutex);

            if (mutex->estado == 0) { // Mutex libre
                mutex->estado = 1; // Se lo adueña
                mutex->pid_proceso = pid;
                pthread_mutex_unlock(&mutex_lista_mutex);

                log_info(log_ks, "## (%d) Toma el Mutex %s", pid, nombre_mutex);

                // Devuelve el proceso a la CPU para que continúe
                enviar_proceso_a_cpu(cpu->socket_cpu, pid);

            } else { // Mutex ocupado, bloquear proceso
                pthread_mutex_lock(&mutex_cola_exec);
                t_proceso_pcb *proceso = remover_proceso_por_pid(cola_exec, pid);
                pthread_mutex_unlock(&mutex_cola_exec);

                if (proceso == NULL) {
                    log_error(log_ks, "Error: No se encontró el PID %d en EXEC para bloquear por mutex.", pid);
                    pthread_mutex_unlock(&mutex_lista_mutex);
                    break;
                }

                cancelar_hilo_quantum_si_existe(proceso, "SYSCALL_MUTEX_LOCK");
                
                cambiar_estado_proceso(proceso, BLOCK);
                list_add(mutex->cola_procesos, proceso);

                // Herencia de Prioridades: el proceso que tiene el mutex hereda la prioridad del que se bloquea
                actualizar_prioridad_proceso(mutex->pid_proceso, proceso->prioridad);
                pthread_mutex_unlock(&mutex_lista_mutex);

                iniciar_temporizador_timeout(cpu->pid_actual);

                log_debug(log_debug_ks, "## (%d) - Intenta tomar Mutex '%s' y se bloquea.", pid, nombre_mutex);

                // Liberar la CPU
                pthread_mutex_lock(&mutex_lista_cpu_conectadas);
                cpu->estado = CPU_LIBRE;
                cpu->pid_actual = -1;
                pthread_cond_signal(&cond_lista_cpu_conectadas);
                pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
            }

            list_destroy_and_destroy_elements(paquete_mutex_lock, free);
            break;
        }
        case SYSCALL_MUTEX_UNLOCK: {
            t_list* paquete_mutex_unlock = recibir_paquete(cpu->socket_cpu);
            char* nombre_mutex = (char*)list_get(paquete_mutex_unlock, 0);

            pid_t pid = cpu->pid_actual;

            pthread_mutex_lock(&mutex_lista_mutex);
            t_mutex* mutex = buscar_mutex(nombre_mutex); 

            if (mutex->pid_proceso != pid) {
                pthread_mutex_unlock(&mutex_lista_mutex);
                log_error(log_ks, "## (%d) - Intenta liberar Mutex '%s' que no le pertenece. Se ignora.", pid, nombre_mutex);
            } else {
                // Restaurar prioridad original si fue heredada
                log_info(log_ks, "## (%d) Libera el Mutex %s", pid, nombre_mutex);
                pthread_mutex_lock(&mutex_cola_exec);
                t_proceso_pcb* proceso_liberador = buscar_proceso_por_id(pid, cola_exec);
                if (proceso_liberador != NULL && proceso_liberador->prioridad != proceso_liberador->prioridad_original) {

                    int prioridad_anterior = proceso_liberador->prioridad;
                    proceso_liberador->prioridad= proceso_liberador->prioridad_original;
                    log_info(log_ks, "## %d Cambio de prioridad: %d - %d", 
                             proceso_liberador->pid, prioridad_anterior ,proceso_liberador->prioridad);
                  
                }
                pthread_mutex_unlock(&mutex_cola_exec);

                if (!list_is_empty(mutex->cola_procesos)) {
                    t_proceso_pcb* desbloqueado = list_remove(mutex->cola_procesos, 0);
                    mutex->pid_proceso = desbloqueado->pid;
                    log_info(log_ks, "## (%d) Toma el Mutex %s", desbloqueado->pid ,nombre_mutex);
                    agregar_proceso_a_ready(desbloqueado);
                } else {
                    mutex->estado = 0;
                    mutex->pid_proceso = -1;
                }
                pthread_mutex_unlock(&mutex_lista_mutex);
            }
            // El proceso que libera el mutex siempre continúa su ejecución
            enviar_proceso_a_cpu(cpu->socket_cpu, pid);
            list_destroy_and_destroy_elements(paquete_mutex_unlock, free);
            break;
        }
        case SYSCALL_MEM_FREE: {
            t_list *paquete_mem_free = recibir_paquete(socket_cpu);

            pid_t pid = cpu->pid_actual;

            uint32_t id_segmento = *(uint32_t *)list_get(paquete_mem_free, 0);

            list_destroy_and_destroy_elements(paquete_mem_free, free);

            // Envio soli a km
            solicitar_mem_free_a_km(conexion_km, pid, id_segmento);

            sem_wait(&sem_solicitud_mem_free);

            
            if (mem_free_exitoso)
            {
                log_debug(log_ks, "MEM_FREE exitoso, devolviendo el PID <%d> a la CPU <%d>", pid, cpu->cpu_id);

                desuspender_procesos();

                enviar_proceso_a_cpu(cpu->socket_cpu, pid);

                break;
            }
            log_error(log_ks, "MEM_FREE falló para el PID <%d> - Segmento <%u>", pid, id_segmento);
            break;
        }
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

    log_info(log_ks, "## CPU %d Conectada", cpu->cpu_id);

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
    case SYSCALL_MUTEX_CREATE:
        return "SYSCALL_MUTEX_CREATE";
    case SYSCALL_MUTEX_LOCK:
        return "SYSCALL_MUTEX_LOCK";
    case SYSCALL_MUTEX_UNLOCK:
        return "SYSCALL_MUTEX_UNLOCK";
    case SYSCALL_INIT_PROC:
        return "SYSCALL_INIT_PROC";
    case SYSCALL_MEM_FREE:
        return "MEM_FREE";
    case SYSCALL_MEM_ALLOC:
        return "MEM_ALLOC";
    default:
        return "DESCONOCIDO";
    }
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

void enviar_proceso_a_io_stdin(pid_t pid, uint32_t tamanio, uint32_t direccion_fisica) {
    t_peticion_stdin *peticion = malloc(sizeof(t_peticion_stdin));
    peticion->pid = pid;
    peticion->tamanio = tamanio;
    peticion->direccion_fisica = direccion_fisica; 

    pthread_mutex_lock(&mutex_stdin);
    list_add(cola_espera_stdin, peticion);
    pthread_cond_signal(&cond_stdin);
    pthread_mutex_unlock(&mutex_stdin);

    return;
}

// IMPORTANTE: Debe ser llamada con mutex_lista_mutex bloqueado.
t_mutex* buscar_mutex(const char* nombre_mutex) {
    for (int i = 0; i < list_size(lista_mutex); i++) {
        t_mutex* m = list_get(lista_mutex, i);
        if (strcmp(m->nombre_mutex, nombre_mutex) == 0) {
            return m;
        }
    }
    return NULL;
}

void crear_mutex(char* nombre_mutex) {
    t_mutex* nuevo_mutex = malloc(sizeof(t_mutex));
    nuevo_mutex->nombre_mutex = strdup(nombre_mutex);
    nuevo_mutex->estado = 0; 
    nuevo_mutex->pid_proceso = -1;
    nuevo_mutex->cola_procesos = list_create();
    list_add(lista_mutex, nuevo_mutex);
    log_debug(log_ks, "Mutex '%s' creado.", nombre_mutex);
    return;
}

/**
 * actualizar_prioridad_proceso: Implementa la herencia de prioridad.
 * Si un proceso (`pid_duenio`) tiene menor prioridad que `prioridad_heredada`,
 * se le asigna temporalmente esta nueva prioridad. Si el proceso dueño estaba en
 * una cola de READY, se lo mueve a la cola de mayor prioridad que le corresponda.
 */
void actualizar_prioridad_proceso(pid_t pid_duenio, int prioridad_heredada) {
    t_proceso_pcb* proceso_duenio = NULL;
    int indice_cola_original = -1;
    bool encontrado_en_ready = false;

    // 1. Buscar en colas READY
    for (int i = 0; i < cantidad_niveles; i++) {
        pthread_mutex_lock(&array_colas_ready[i].mutex);
        proceso_duenio = buscar_proceso_por_id(pid_duenio, array_colas_ready[i].cola);
        if (proceso_duenio != NULL) {
            indice_cola_original = i;
            encontrado_en_ready = true;
            break; // Encontrado, mantener el lock para posible remoción
        }
        pthread_mutex_unlock(&array_colas_ready[i].mutex);
    }

    // 2. Si no está en READY, buscar en EXEC
    if (!encontrado_en_ready) {
        pthread_mutex_lock(&mutex_cola_exec);
        proceso_duenio = buscar_proceso_por_id(pid_duenio, cola_exec);
        pthread_mutex_unlock(&mutex_cola_exec);
    }

    // 3. Si no se encontró o la prioridad a heredar no es mayor (menor valor), no hacer nada.
    if (proceso_duenio == NULL || prioridad_heredada >= proceso_duenio->prioridad) {
        if (encontrado_en_ready) {
            pthread_mutex_unlock(&array_colas_ready[indice_cola_original].mutex); // Liberar el lock que mantuvimos
        }
        return;
    }

    // 4. Actualizar prioridad
    int prioridad_anterior = proceso_duenio->prioridad;
    proceso_duenio->prioridad = prioridad_heredada;
    log_info(log_ks, "## %d Cambio de prioridad: %d - %d", proceso_duenio->pid, prioridad_anterior, proceso_duenio->prioridad);

    // 5. Si estaba en READY, moverlo de cola (sin afectar el semáforo del planificador)
    if (encontrado_en_ready) {
        t_proceso_pcb* pcb_removido = remover_proceso_por_pid(array_colas_ready[indice_cola_original].cola, pid_duenio);
        pthread_mutex_unlock(&array_colas_ready[indice_cola_original].mutex);

        if (pcb_removido) {
            // Agregar a la nueva cola correspondiente
            int indice_cola_nueva = (algoritmo_global == CMN) ? pcb_removido->prioridad : 0;
            if (indice_cola_nueva < 0 || indice_cola_nueva >= cantidad_niveles) {
                indice_cola_nueva = 0; // Fallback
            }

            pthread_mutex_lock(&array_colas_ready[indice_cola_nueva].mutex);
            list_add(array_colas_ready[indice_cola_nueva].cola, pcb_removido);
            pthread_mutex_unlock(&array_colas_ready[indice_cola_nueva].mutex);

            log_debug(log_debug_ks, "Proceso PID<%d> movido de cola READY %d a %d por herencia de prioridad.",
                      pcb_removido->pid, indice_cola_original, indice_cola_nueva);
        }
    }
    // Si estaba en EXEC, solo se actualizó la prioridad. Cuando vuelva a READY,
    // la función agregar_proceso_a_ready() lo ubicará en la cola correcta.
}