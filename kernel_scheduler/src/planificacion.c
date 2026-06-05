#include "kernel_scheduler.h"
#include "planificacion.h"
#include "cpu.h"

typedef struct {
    t_cpu* cpu;
    int pid;
} t_args_quantum;

void *handle_planificacion(void *arg) {
    t_algoritmo_planificacion algoritmo_planificacion = string_to_algoritmo((char*)arg);  
    
    switch (algoritmo_planificacion) {    
        case FIFO:
            planificacion_fifo();
            break;
        case RR:
            planificacion_rr();
            break;
        case CMN:
            //planificacion_multinivel()
            break;
        default:
            perror("Algoritmo de planificacion invalido");
    }
    return NULL;
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
    t_paquete* paquete_proceso = crear_paquete(EJECUTAR_PROCESO);
    agregar_a_paquete(paquete_proceso, &pid_proceso, sizeof(int));
    agregar_a_paquete(paquete_proceso, &pc, sizeof(int));
    agregar_a_paquete(paquete_proceso, &prioridad, sizeof(int));

    enviar_paquete_send_all(paquete_proceso, socket_cpu);
    eliminar_paquete(paquete_proceso);

    return;
}

t_algoritmo_planificacion string_to_algoritmo(const char* algoritmo) {

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

void planificacion_rr() { 
    while(true) {   
        pthread_mutex_lock(&mutex_cola_ready);

        while (list_is_empty(cola_ready)) { 
                pthread_cond_wait(&cond_cola_ready, &mutex_cola_ready);
            }

        t_proceso_pcb* proceso = list_remove(cola_ready, 0);//saca primer proceso de cola ready 

        pthread_mutex_unlock(&mutex_cola_ready);

        pthread_mutex_lock(&mutex_lista_cpu_conectadas);

        t_cpu* cpu = NULL;

        while ((cpu = buscar_cpu_libre()) == NULL) {   
            pthread_cond_wait(&cond_lista_cpu_conectadas, &mutex_lista_cpu_conectadas);
        }//Busca CPU libre para el proceso

        cpu->estado = CPU_OCUPADA;
        cpu->pid_actual = proceso->pid;

        pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

        pthread_mutex_lock(&mutex_cola_exec);

        cambiar_estado_proceso(proceso, EXEC); //Pasa el proceso a estado execute 
        list_add(cola_exec, proceso); // añade el proceso a la cola de exec 

        pthread_mutex_unlock(&mutex_cola_exec); 

        enviar_proceso_a_cpu(cpu->socket_cpu, proceso->pid, proceso->pc, proceso->prioridad); //manda a cpu el proceso 

        log_debug(log_debug_ks, "Se envia el proceso ID<%d> a la cpu ID<%d> por RR", proceso->pid, cpu->cpu_id);

        iniciar_quantum(cpu, proceso->pid); // inicia el timer de quantum para ese proceso
    } 
    // La lógica para enviar el proceso a la CPU sería similar a la de FIFO, pero con la diferencia de que aca tambien tendriamos que manejar el quantum y las interrupciones por fin de quantum o desalojo, lo cual implicaría agregar lógica adicional para manejar esos casos específicos.
}

void iniciar_quantum(t_cpu* cpu, int pid) {
    t_args_quantum* args = malloc(sizeof(t_args_quantum)); //crea la estructura

    if (args == NULL) {
        log_error(log_ks, "No se pudo reservar memoria para el quantum del PID %d", pid);
        return;
    }

    args->cpu = cpu; //guarda los datos dentro de la estructura
    args->pid = pid;

    pthread_t hilo_quantum;
    pthread_create(&hilo_quantum, NULL, controlar_quantum, args);
    pthread_detach(hilo_quantum);
}

void* controlar_quantum(void* arg) {
    t_args_quantum* args = (t_args_quantum*) arg;

    usleep(rr_quantum * 1000);

    pthread_mutex_lock(&mutex_lista_cpu_conectadas);
//si la cpu esta ocupadaa manda interrupcion de fin de quantum a la cpu 
    if (args->cpu->estado == CPU_OCUPADA && args->cpu->pid_actual == args->pid) {
        op_code_ks_cpu codigo = INTERRUPCION_FIN_QUANTUM;

        send(args->cpu->socket_cpu, &codigo, sizeof(op_code_ks_cpu), 0);

        log_debug(
            log_debug_ks,
            "Se envia interrupcion de fin de quantum a CPU ID<%d> por PID <%d>",
            args->cpu->cpu_id,
            args->pid
        );
    }

    pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

    free(args);

    return NULL;
}