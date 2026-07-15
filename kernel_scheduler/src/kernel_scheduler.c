#include "kernel_scheduler.h"

int main(int argc, char *argv[]){

    inicializar_ks(argc, argv);
    
    // CONEXION CON KERNEL MEMORY
    conexion_km_ptr = malloc(sizeof(int));
    conexion_km = crear_conexion(ip_km, puerto_km);
    if(conexion_km == -1){
        log_error(log_ks, "Error al conectar con Kernel Memory en %s:%s", ip_km, puerto_km);
        log_destroy(log_ks);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    log_info(log_ks, "## Conectado a Kernel Memory");
    
    op_code handshake_ks = HANDSHAKE_KS;
    send(conexion_km, &handshake_ks, sizeof(handshake_ks), 0);
    *conexion_km_ptr = conexion_km;

    crear_proceso_inicial(conexion_km, path_proceso_inicial);

    pthread_t hilo_escucha;
    pthread_t hilo_km;
    pthread_t hilo_planificacion;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);
    pthread_create(&hilo_km, NULL, handle_km, conexion_km_ptr);
    pthread_create(&hilo_planificacion, NULL, hilo_planificador_principal, NULL);

    pthread_join(hilo_escucha, NULL);
    pthread_join(hilo_km, NULL);
    pthread_join(hilo_planificacion, NULL);

    liberar_recursos();

    return 0;
}

void inicializar_ks(int argc, char *argv[]) {  
    
    if(argc<3){
        perror("No se proporcionaron todos los argumentos: [Archivo Config] [Path Proceso Inicial]");
        exit(EXIT_FAILURE);
    }

    config_path = argv[1];
    path_proceso_inicial = argv[2];

    config = config_create(config_path);
    if(config == NULL){
        perror("No se pudo crear el config");
        exit(EXIT_FAILURE);
    }

    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km = config_get_string_value(config, "PUERTO_KM");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    puerto = config_get_string_value(config, "PUERTO");
    algoritmo_planificacion = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
    
    // Obtener el algoritmo global y el flag de preemption
    algoritmo_global = string_to_algoritmo(algoritmo_planificacion);
    queue_preemption = config_get_int_value(config, "QUEUE_PREEMPTION") != 0;
    
    // Obtener parámetros de planificación
    rr_quantum = config_get_int_value(config, "RR_QUANTUM");
    suspension_timeout = config_get_int_value(config, "SUSPENSION_TIMEOUT");
    
    log_level = log_level_from_string(str_log_level);

    log_ks = log_create("scheduler.log", "SCHEDULER", true, log_level);
    log_debug_ks = log_create("scheduler_debug.log", "SCHEDULER_DEBUG", true, 'DEBUG');

    // Inicializar colas de estados (READY ahora es array_colas_ready)
    cola_block = list_create();
    cola_exec = list_create();
    cola_exit = list_create();
    cola_espera_sleep = list_create();
    cola_espera_stdout = list_create();
    cola_susp_block = list_create();
    cola_susp_ready = list_create();
    lista_io_conectadas = list_create();
    lista_mutex = list_create();
    lista_cpu_conectadas = list_create();

    pthread_mutex_init(&mutex_cola_block, NULL);
    pthread_mutex_init(&mutex_cola_exec, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_mutex_init(&mutex_lista_cpu_conectadas, NULL);
    pthread_mutex_init(&mutex_lista_mutex, NULL);
    pthread_mutex_init(&mutex_lista_io_conectadas, NULL);
    pthread_mutex_init(&mutex_generador_id_io, NULL);
    pthread_mutex_init(&mutex_generador_pid, NULL);
    pthread_mutex_init(&mutex_sleep, NULL);
    pthread_mutex_init(&mutex_stdout, NULL);
    pthread_mutex_init(&mutex_cola_susp_block, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);
    pthread_cond_init(&cond_cola_block, NULL);
    pthread_mutex_init(&mutex_solicitud_km, NULL);
    pthread_cond_init(&cond_lista_cpu_conectadas, NULL);
    pthread_cond_init(&cond_sleep, NULL);
    pthread_cond_init(&cond_stdout, NULL);
    pthread_cond_init(&cond_cola_susp_ready, NULL);
    pthread_cond_init(&cond_cola_susp_block, NULL);
    sem_init(&sem_inicializar_proceso, 0, 0);
    sem_init(&sem_respuesta_contenido_km, 0, 0);

    // Inicializar el planificador con la estructura de colas adaptativa
    inicializar_planificador();
    log_info(log_ks, "Estructura de planificación inicializada correctamente");

    // INICIAR SERVIDOR
    server_ks = iniciar_servidor(log_ks, puerto);
    if(server_ks == -1){
        log_error(log_ks, "Error al iniciar el servidor en el puerto %s", puerto);
        log_destroy(log_ks);
        config_destroy(config);
        exit(EXIT_FAILURE);
    }
    
    log_info(log_ks, "Kernel Scheduler iniciado");
}


void* handle_connections(void* arg){
    while(1) {  
        int client_fd = esperar_cliente(server_ks, log_ks);

        int *socket_cliente = malloc(sizeof(int));
        *socket_cliente = client_fd;

        op_code handshake_modulo = recibir_operacion(*socket_cliente);

        switch (handshake_modulo) {
 			case HANDSHAKE_CPU:
                pthread_t hilo_cpu;
				pthread_create(&hilo_cpu, NULL, handle_cpu, socket_cliente);
				pthread_detach(hilo_cpu);
                //Entendemos que los hilos deberían estar creados en el switch para destinar cada operación al hilo correspondiente
 				break;
 			case HANDSHAKE_IO:
                // No se si es necesario un hilo para este modulo
                pthread_t hilo_io;
				pthread_create(&hilo_io, NULL, handle_io, socket_cliente);
				pthread_detach(hilo_io);
 				break;
            default:
 				log_warning(log_ks, "Handshake no reconocido: %d", handshake_modulo);
 				//close(socket);
 				break;
 		}
    }
}

void* handle_km(void* arg) {    
    int socket_km = *(int*)arg;
    free(arg);

    while(true) {   
        // aca iria la logica de operaciones con Kernel Memory
        int codigo_operacion = recibir_operacion(socket_km);

        switch(codigo_operacion) {
            case INICIALIZAR_PROCESO_OK:
                log_debug(log_ks, "KM confirmo INICIALIZAR_PROCESO_OK");
                sem_post(&sem_inicializar_proceso);
                break;
            case -1:
                log_error(log_ks, "El servidor Kernel Memory se desconectó. Terminando hilo.");
                return NULL;
            case MEMORIA_CORRUPTA:
                ejecutar_bsod();
                return NULL;
            case RESPUESTA_CONTENIDO_MEMORIA: {
                t_list* paquete_respuesta = recibir_paquete(socket_km);
                if (paquete_respuesta == NULL) {
                    log_error(log_ks, "Error al recibir paquete de RESPUESTA_CONTENIDO_MEMORIA");
                    buffer_global_contenido_km = NULL;
                    sem_post(&sem_respuesta_contenido_km);
                    break;
                }

                void* contenido_recibido = list_get(paquete_respuesta, 0);
                uint32_t tamanio = buffer_global_tamanio_contenido_km;

                buffer_global_contenido_km = malloc(tamanio + 1);
                if (buffer_global_contenido_km != NULL) {
                    memcpy(buffer_global_contenido_km, contenido_recibido, tamanio);
                    buffer_global_contenido_km[tamanio] = '\0';
                } else {
                    log_error(log_ks, "Error de malloc al recibir contenido de KM");
                }

                list_destroy_and_destroy_elements(paquete_respuesta, free);
                sem_post(&sem_respuesta_contenido_km);
                break;
            }
            default:
                log_warning(log_ks, "Operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}
//Terminar
void ejecutar_bsod(void){
    if(bsod_activo){
        return;
    }
    log_error(log_ks, "## BSOD: BLUE SCREEN OF DEATH");

    bsod_activo = true;


}

// Kernel memory debería recibir el proceso (Vick y Aylu)
void crear_proceso_inicial(int socket_km, char* path_proceso_inicial) {    
    
    enviar_proceso(socket_km, 0, path_proceso_inicial);
    
    // Esperamos la confirmación de Kernel Memory
    op_code codigo_operacion = recibir_operacion(socket_km);

    if (codigo_operacion == INICIALIZAR_PROCESO_OK) {
        log_debug(log_ks, "Kernel Memory confirmo INICIALIZAR_PROCESO_OK PARA EL PROCESO INICIAL");
        t_proceso_pcb* pcb_proceso_inicial = crear_pcb(0, PRIORIDAD_MAXIMA);

        // Usar la nueva función productora genérica
        agregar_proceso_a_ready(pcb_proceso_inicial);
    } else {
        log_error(log_ks, "Error al crear el proceso inicial en Kernel Memory. Código de op: %d", codigo_operacion);
        return; 
    }
}


// Envia la operacion para que KM cree las estructuras internas para el proceso
void enviar_proceso(int socket_km, pid_t pid, char* path_proceso) { 
    
    log_debug(log_ks, "Enviando solicitud INICIALIZAR_PROCESO a Kernel Memory");
    
    op_code codigo_operacion = INICIALIZAR_PROCESO;  
    
    t_paquete *paquete_proceso = crear_paquete(codigo_operacion);

    agregar_a_paquete(paquete_proceso, &pid, sizeof(int));
    agregar_a_paquete(paquete_proceso, path_proceso, strlen(path_proceso) + 1);

    enviar_paquete_send_all(paquete_proceso, socket_km);
    
    eliminar_paquete(paquete_proceso);

    log_debug(log_ks, "Solicitud INICIALIZAR_PROCESO enviada a Kernel Memory");

}

t_proceso_pcb* crear_pcb(pid_t pid, int prioridad) {
    t_proceso_pcb* pcb = malloc(sizeof(t_proceso_pcb));

    pcb->pid = pid;
    pcb->prioridad = prioridad;
    pcb->estado = NEW;
    pcb->nombre_mutex = NULL;
    pcb->pc = 0;
    pcb->quantum = 0; 
    pcb->tiene_hilo_quantum = false;
    pcb->id_hilo_quantum = 0;
 
    log_info(log_ks, "## (<%d>) Se crea el proceso - Estado: NEW", pid);

    return pcb;
}

const char* estado_to_string(t_estado_proceso estado) {
    switch (estado) {
        case NEW: return "NEW";
        case READY: return "READY";
        case BLOCK: return "BLOCK";
        case EXEC: return "EXEC";
        case EXIT: return "EXIT";
        case SUSP_READY: return "SUSPENDED-READY";
        case SUSP_BLOCK: return "SUSPENDED-BLOCKED";
        default: return "DESCONOCIDO"; 
    }
}

void cambiar_estado_proceso(t_proceso_pcb *proceso, t_estado_proceso estado_siguiente) {    
    t_estado_proceso estado_anterior = proceso->estado;
    proceso->estado = estado_siguiente;

    log_info(log_ks, "## (<%d>) Pasa del estado <%s> al estado <%s>", proceso->pid, estado_to_string(estado_anterior), estado_to_string(estado_siguiente));
    return;
}

char* solicitar_contenido_a_km(int socket_km, pid_t pid, uint32_t direccion_fisica, uint32_t tamanio){  
    // Se bloquea para asegurar que solo una solicitud a KM que requiera respuesta esté en curso.
    pthread_mutex_lock(&mutex_solicitud_km);

    // Guardamos el tamaño esperado de la respuesta para que el hilo handle_km lo sepa.
    buffer_global_tamanio_contenido_km = tamanio;

    // Enviamos la solicitud a Kernel Memory
    op_code codigo_operacion = SOLICITAR_CONTENIDO_MEMORIA;
    t_paquete *paquete_contexto = crear_paquete(codigo_operacion);
    agregar_a_paquete(paquete_contexto, &pid, sizeof(pid_t));
    agregar_a_paquete(paquete_contexto, &direccion_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto, &tamanio, sizeof(uint32_t));
    enviar_paquete_send_all(paquete_contexto, socket_km);
    eliminar_paquete(paquete_contexto);

    // Esperamos a que el hilo handle_km reciba la respuesta y nos la deje en buffer_global_contenido_km
    sem_wait(&sem_respuesta_contenido_km);

    // Tomamos posesión del puntero al contenido y lo reseteamos para la próxima solicitud.
    char* contenido = buffer_global_contenido_km;
    buffer_global_contenido_km = NULL;

    pthread_mutex_unlock(&mutex_solicitud_km);

    return contenido;
}

void destruir_mutex(void* elemento) {
    t_mutex* mutex = (t_mutex*)elemento;
    free(mutex->nombre_mutex);
    // pero no deben ser liberados aquí para evitar doble free. La lista se vacía y destruye.
    list_destroy(mutex->cola_procesos);
    free(mutex);
}

void solicitar_mem_free_a_km(int socket_km, pid_t pid, uint32_t id_segmento){
op_code codigo_operacion = SOLICITAR_MEM_FREE;

t_paquete *paquete_mem_free = crear_paquete(codigo_operacion);
agregar_a_paquete(paquete_mem_free, &pid, sizeof(pid_t));
agregar_a_paquete(paquete_mem_free, &id_segmento, sizeof(uint32_t));

enviar_paquete_send_all(paquete_mem_free, socket_km);

eliminar_paquete(paquete_mem_free);
}



void liberar_recursos() {   
    log_destroy(log_ks);
    log_destroy(log_debug_ks);
    config_destroy(config);
    pthread_mutex_destroy(&mutex_cola_block);
    pthread_mutex_destroy(&mutex_cola_exec);
    pthread_mutex_destroy(&mutex_cola_exit);
    pthread_mutex_destroy(&mutex_lista_cpu_conectadas);
    pthread_mutex_destroy(&mutex_lista_mutex);
    pthread_mutex_destroy(&mutex_lista_io_conectadas);
    pthread_mutex_destroy(&mutex_generador_id_io);
    pthread_mutex_destroy(&mutex_generador_pid);
    pthread_mutex_destroy(&mutex_sleep);
    pthread_mutex_destroy(&mutex_stdout);
    pthread_mutex_destroy(&mutex_cola_susp_block);
    pthread_mutex_destroy(&mutex_cola_susp_ready);
    pthread_cond_destroy(&cond_cola_block);
    pthread_mutex_destroy(&mutex_solicitud_km);
    pthread_cond_destroy(&cond_lista_cpu_conectadas);
    pthread_cond_destroy(&cond_sleep);
    pthread_cond_destroy(&cond_stdout);
    pthread_cond_destroy(&cond_cola_susp_block);
    pthread_cond_destroy(&cond_cola_susp_ready);
    sem_destroy(&sem_inicializar_proceso);
    sem_destroy(&sem_respuesta_contenido_km);
    free(conexion_km_ptr);
    free(config_path);
    free(path_proceso_inicial);
    free(puerto);
    free(str_log_level);
    free(puerto_km);
    free(ip_km);
    free(algoritmo_planificacion);
    liberar_planificador();
    free(buffer_global_contenido_km);
    list_destroy(cola_block);
    list_destroy(cola_exec);
    list_destroy(cola_exit);
    list_destroy_and_destroy_elements(lista_mutex, destruir_mutex);
    list_destroy(lista_cpu_conectadas);
    list_destroy(lista_io_conectadas);
    list_destroy(cola_espera_sleep);
    list_destroy(cola_espera_stdout);
    close(server_ks);
    close(conexion_km);
    
    return;
}

t_proceso_pcb* remover_proceso_por_pid(t_list* cola, pid_t pid) {
    //**IMPORTANTE**: esta funcion se tiene que llamar con el mutex de su cola bloqueado 
    for (int i = 0; i < list_size(cola); i++) {
        t_proceso_pcb* proceso = list_get(cola, i);

        if (proceso->pid == pid) {
            return list_remove(cola, i);
        }
    }
    return NULL;
}

void iniciar_temporizador_timeout(pid_t pid) {    
    pthread_t hilo_timeout;
    pid_t *pid_ptr = malloc(sizeof(pid_t));
    *pid_ptr = pid;
    pthread_create(&hilo_timeout, NULL, temporizador_timeout, (void*)pid_ptr); 
    pthread_detach(hilo_timeout); 
}

void* temporizador_timeout(void* arg) {    
    pid_t pid = *(pid_t*)arg;
    free(arg);

    usleep(suspension_timeout * 1000);

    pthread_mutex_lock(&mutex_cola_block);
    t_proceso_pcb *proceso = remover_proceso_por_pid(cola_block, pid);
    if (proceso != NULL) {
        // CASO A: El tiempo se agoto y la IO no termino ahora lo metemos a cola_susp_block
        
        pthread_mutex_lock(&mutex_cola_susp_block);
        cambiar_estado_proceso(proceso, SUSP_BLOCK);
        list_add(cola_susp_block, proceso);
        pthread_mutex_unlock(&mutex_cola_susp_block);
        
        pthread_mutex_unlock(&mutex_cola_block); 

        log_info(log_ks, "Timeout agotado. El proceso %d pasa a SUSP_BLOCK", pid);

        // 3. LA ORDEN A MEMORIA (Swapping) aca tenemos que avisarle a memoria que suspenda el proceso
        t_paquete *paquete_suspension = crear_paquete(SUSPENDER_PROCESO); 
        agregar_a_paquete(paquete_suspension, &pid, sizeof(pid_t));

        enviar_paquete_send_all(paquete_suspension, conexion_km);
        eliminar_paquete(paquete_suspension); 
        
    } else {
        // CASO B: El proceso no está en la cola block
        pthread_mutex_unlock(&mutex_cola_block);
    }

    return NULL;
}