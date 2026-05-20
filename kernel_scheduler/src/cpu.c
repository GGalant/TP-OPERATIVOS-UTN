#include "kernel_scheduler.h"

void* handle_cpu(void* arg) {    
    int socket_cpu = *(int*)arg;
    t_cpu* cpu; // liberar memoria cuando se desconecte
    free(arg);
    int cpu_id_recibido;

    cpu_id_recibido = recibir_cpu_id(socket_cpu);
    if (cpu_id_recibido >= 0) { 
        cpu = malloc(sizeof(t_cpu));
        cpu->cpu_id = cpu_id_recibido;
        cpu->socket_cpu = socket_cpu;
        cpu->estado = CPU_LIBRE;
        cpu->pid_actual = -1;

        pthread_mutex_lock(&mutex_lista_cpu_conectadas);
        list_add(lista_cpu_conectadas, cpu);
        pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

        log_info(log_ks, "## CPU ID<%d> Conectada", cpu_id_recibido);
    } else {    
        perror("No se le pudo asignar un ID a la CPU");
        close(socket_cpu);
        return NULL;
    }
    
    while(true) {   
        // aca iria la logica de operaciones con CPU 
        op_code_ks_cpu codigo_operacion = recibir_operacion(socket_cpu);

        log_info(log_ks, "## Proceso ID(<%d>) - Solicitó syscall: <%s>", cpu->pid_actual, operacion_to_string(codigo_operacion));

        switch(codigo_operacion) {
             case SYSCALL_EXIT:
                log_info(log_debug_ks, "Finaliza el proceso %d - Motivo: EXIT", cpu->pid_actual);
                //Cpu queda libre, liberar el struct del pcb
                break;
            case SYSCALL_SLEEP:
                log_info(log_debug_ks, "PID: %d - Bloqueado por: SLEEP", cpu->pid_actual);

                //Aca deberiamos recibir pcb actualizado (o por lo menos pc del proceso) y tiempo sleep
                t_list* paquete_sleep = recibir_paquete(socket_cpu); 
                int pid = *(int*)list_get(paquete_sleep, 0);
                int pc_actualizado = *(int*)list_get(paquete_sleep, 1);
                int tiempo_sleep = *(int*)list_get(paquete_sleep, 2);
                list_destroy_and_destroy_elements(paquete_sleep, free);
                
                // *** TERMINAR: deberia sacar el proceso que esta ejecutando la cola exec y ponerlo en la cola block 
                // pthread_mutex_lock(&mutex_cola_exec);
                // t_proceso_pcb* proceso = buscar_proceso_por_id(pid, cola_exec);
                // list_remove(cola_exec, proceso);

                // cambiar_estado_proceso(proceso, BLOCK);

                // pthread_mutex_unlock(&mutex_cola_exec);
                
                //enviar_peticion_io()

                
                break;
            case SYSCALL_STDOUT:
                // log_info();
                break;
            case INTERRUPCION_FIN_QUANTUM:
                break;
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

int recibir_cpu_id(int socket){
	int cpu_id;
	int tamanio_recibido = recv(socket, &cpu_id, sizeof(int), MSG_WAITALL);
	if(tamanio_recibido > 0){
        printf("este es el id que me llega: %d\n",cpu_id);
	    return cpu_id;
    } else {
        perror("No se recibio ningun ID");
        return -1;
    }

}

const char* operacion_to_string(op_code_ks_cpu operacion) {
    switch (operacion) {
        case SYSCALL_EXIT: return "SYSCALL_EXIT";
        case SYSCALL_SLEEP: return "SYSCALL_SLEEP";
        case SYSCALL_STDIN: return "SYSCALL_STDIN";
        case SYSCALL_STDOUT: return "SYSCALL_STDOUT";
        case INTERRUPCION_FIN_QUANTUM: return "INTERRUPCION_FIN_QUANTUM";  
        case INTERRUPCION_DESALOJO: return "INTERRUPCION_DESALOJO";
        case SYSCALL_MUTEX_LOCK: return "SYSCALL_MUTEX_LOCK";
        case SYSCALL_MUTEX_UNLOCK: return "SYSCALL_MUTEX_UNLOCK";
        case SYSCALL_INIT_PROC: return "SYSCALL_INIT_PROC";
        default: return "DESCONOCIDO";
    }
}

t_proceso_pcb* buscar_proceso_por_id(int pid, t_list* cola) { 
    for (int i = 0; i < list_size(cola); i++) {
        t_proceso_pcb* proceso = list_get(cola, i);
        if (proceso->pid == pid);
            return proceso;
    }
    return NULL; 
}