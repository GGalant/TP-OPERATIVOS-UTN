#include "planificacion.h"

void *hilo_planificacion(void *arg) {
    t_algoritmo_planificacion algortimo_planificacion = *(int*)arg;  
    
    switch (algortimo_planificacion) {    
        case FIFO:
            planificacion_fifo();
            break;
        case RR:
            //planificacion_rr()
            break;
        case COLAS_MULTINIVEL:
            //planificacion_multinivel()
            break;
        default:
            perror("Algortimo de planificacion invalido");
    }
}

void planificacion_fifo() { 
    while(true) {   

        pthread_mutex_lock(&mutex_cola_ready);
        while (list_is_empty(cola_ready)) { 
            pthread_cond_wait(&cond_cola_ready, &mutex_cola_ready);
            // alguien deberia despertar el hilo cuando haya un proceso en la cola
        }
        
        t_proceso_pcb* proceso = list_remove(cola_ready, 0);
        
        pthread_mutex_unlock(&mutex_cola_ready);

        // aca deberiamos buscar una cpu libre para enviar a ejecutar el proces
        // Bloqueamos la lista de CPUs para buscar una libre de forma segura
        pthread_mutex_lock(&mutex_lista_cpu_conectadas);
        t_cpu* cpu = NULL;
        while ((cpu = buscar_cpu_libre()) == NULL) {   
            // Si no hay CPUs libres, esperamos a que alguna se libere.
            // pthread_cond_wait desbloquea el mutex mientras espera y lo vuelve a bloquear al despertar.
            pthread_cond_wait(&cond_lista_cpu_conectadas, &mutex_lista_cpu_conectadas);
        }

        cpu->estado = CPU_OCUPADA;
        cpu->pid_actual = proceso->pid;
        pthread_mutex_unlock(&mutex_lista_cpu_conectadas);
        
        enviar_proceso_a_cpu(cpu->socket_cpu, proceso->pid, proceso->pc, proceso->prioridad);
        log_debug(log_debug_ks, "Se envia el proceso ID<%d> a la cpu ID<%d>", proceso->pid, cpu->cpu_id);

        // Bloqueamos para cambiar el estado y mover el proceso a la cola de EXEC de forma atómica.
        pthread_mutex_lock(&mutex_cola_exec);
        cambiar_estado_proceso(proceso, EXEC);
        list_add(cola_exec, proceso);
        pthread_mutex_unlock(&mutex_cola_exec);
    } 
}

t_cpu* buscar_cpu_libre() {
    // IMPORTANTE: Esta función debe ser llamada con el mutex_lista_cpu_conectadas bloqueado.
    for (int i = 0; i < list_size(lista_cpu_conectadas); i++) {
        t_cpu* cpu = list_get(lista_cpu_conectadas, i);
        if (cpu->estado == CPU_LIBRE) {
            log_debug(log_debug_ks, "¡CPU %d libre encontrada!", cpu->cpu_id);
            return cpu;
        }
    }
    log_debug(log_debug_ks, "No se encontraron CPUs libres en este momento.");
	return NULL;
}

void enviar_proceso_a_cpu(int socket_cpu, int pid_proceso, int pc, int prioridad) {   
    op_code codigo_operacion = EJECUTAR_PROCESO;

    t_paquete* paquete_proceso = crear_paquete(EJECUTAR_PROCESO);
    agregar_a_paquete(paquete_proceso, &pid_proceso, sizeof(int));
    agregar_a_paquete(paquete_proceso, &pc, sizeof(int));
    agregar_a_paquete(paquete_proceso, &prioridad, sizeof(int));

    enviar_paquete_send_all(paquete_proceso, socket_cpu);
    eliminar_paquete(paquete_proceso);

    return;
}

