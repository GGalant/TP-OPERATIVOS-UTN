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
    
    handshake handshake_ks = HANDSHAKE_KS;
    send(conexion_km, &handshake_ks, sizeof(handshake), 0);
    *conexion_km_ptr = conexion_km;

    crear_proceso_inicial(conexion_km, path_proceso_inicial);

    pthread_t hilo_escucha;
    pthread_t hilo_km;
    pthread_t hilo_planificacion;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);
    pthread_create(&hilo_km, NULL, handle_km, conexion_km_ptr);
    pthread_create(&hilo_planificacion, NULL, handle_planificacion, algoritmo_planificacion);

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
    suspension_timeout = config_get_int_value(config, "SUSPENSION_TIMEOUT");
    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km = config_get_string_value(config, "PUERTO_KM");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    puerto = config_get_string_value(config, "PUERTO");
    algoritmo_planificacion = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
    log_level = log_level_from_string(str_log_level);
    rr_quantum = config_get_int_value(config, "RR_QUANTUM");

    log_ks = log_create("scheduler.log", "SCHEDULER", true, log_level);
    log_debug_ks = log_create("scheduler_debug.log", "SCHEDULER_DEBUG", true, DEBUG);

    cola_ready = list_create();
    cola_block = list_create();
    cola_exec = list_create();
    lista_mutex = list_create();
    lista_cpu_conectadas = list_create();
    cola_espera_sleep = list_create();
    lista_io_conectadas = list_create();
    cola_susp_block = list_create();
    cola_susp_ready = list_create();

    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_block, NULL);
    pthread_mutex_init(&mutex_cola_exec, NULL);
    pthread_mutex_init(&mutex_lista_cpu_conectadas, NULL);
    pthread_mutex_init(&mutex_lista_io_conectadas, NULL);
    pthread_mutex_init(&mutex_generador_id_io, NULL);
    pthread_mutex_init(&mutex_sleep, NULL);
    pthread_cond_init(&cond_cola_ready, NULL);
    pthread_cond_init(&cond_cola_block, NULL);
    pthread_cond_init(&cond_lista_cpu_conectadas, NULL);
    pthread_cond_init(&cond_sleep, NULL);
    pthread_mutex_init(&mutex_cola_susp_block, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);

    pthread_cond_init(&cond_cola_susp_block, NULL);
    pthread_cond_init(&cond_cola_susp_ready, NULL);

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

        handshake handshake_modulo = recibir_operacion(*socket_cliente);

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
 				//TODO IMPLEMENTAR
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
            case -1:
                log_error(log_ks, "El servidor Kernel Memory se desconectó. Terminando hilo.");
                return NULL;
            default:
                log_warning(log_ks, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

// Kernel memory debería recibir el proceso (Vick y Aylu)
void crear_proceso_inicial(int socket_km, char* path_proceso_inicial) {    
    
    enviar_proceso(socket_km, 0, path_proceso_inicial);
    
    // Esperamos la confirmación de Kernel Memory
    op_code codigo_operacion = recibir_operacion(socket_km);

    if (codigo_operacion == INICIALIZAR_PROCESO_OK) {
        t_proceso_pcb* pcb_proceso_inicial = crear_pcb(0, PRIORIDAD_MAXIMA);

        pthread_mutex_lock(&mutex_cola_ready);
        cambiar_estado_proceso(pcb_proceso_inicial, READY);
        // aca podriamos proteger la variable global con un mutex de C
        list_add(cola_ready, pcb_proceso_inicial);
        pthread_mutex_unlock(&mutex_cola_ready);
        pthread_cond_signal(&cond_cola_ready);
    } else {
        log_error(log_ks, "Error al crear el proceso inicial en Kernel Memory. Código de op: %d", codigo_operacion);
        return; 
    }
}


// Envia la operacion para que KM cree las estructuras internas para el proceso
void enviar_proceso(int socket_km, int pid, char* path_proceso) { 
    op_code codigo_operacion = INICIALIZAR_PROCESO;  
    
    t_paquete *paquete_proceso = crear_paquete(codigo_operacion);

    agregar_a_paquete(paquete_proceso, &pid, sizeof(int));
    agregar_a_paquete(paquete_proceso, path_proceso, strlen(path_proceso) + 1);

    enviar_paquete_send_all(paquete_proceso, socket_km);
    
    eliminar_paquete(paquete_proceso);
}

t_proceso_pcb* crear_pcb(int pid, int prioridad) {
    t_proceso_pcb* pcb = malloc(sizeof(t_proceso_pcb));

    pcb->pid = pid;
    pcb->prioridad = prioridad;
    pcb->estado = NEW;
    pcb->nombre_mutex = NULL;
    pcb->pc = 0;
    pcb->quantum = 0; 

    log_info(log_ks, "## (<%d>) Se crea el proceso - Estado: NEW", pid);

    return pcb;
}

/**
 * Obtener el PID de un proceso
 * obtener_pid() {}
 */


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
    // *IMPORTANTE* esta funcion tiene que llamarse con el mutex de la cola correspondiente bloqueado
    t_estado_proceso estado_anterior = proceso->estado;
    proceso->estado = estado_siguiente;

    log_info(log_ks, "## (<%d>) Pasa del estado <%s> al estado <%s>", proceso->pid, estado_to_string(estado_anterior), estado_to_string(estado_siguiente));
    return;
}

char* solicitar_contenido_a_km(int socket_km, int pid, int direccion_logica, int tamanio){  
    op_code_ks_km codigo_operacion = SOLICITAR_CONTENIDO_MEMORIA;  
    
    t_paquete *paquete_contexto = crear_paquete(codigo_operacion);

    agregar_a_paquete(paquete_contexto, &pid, sizeof(int));
    agregar_a_paquete(paquete_contexto, &direccion_logica, sizeof(int));
    agregar_a_paquete(paquete_contexto, &tamanio, sizeof(int));

    enviar_paquete_send_all(paquete_contexto, socket_km);
    
    eliminar_paquete(paquete_contexto);

    op_code respuesta = recibir_operacion(socket_km);
    if (respuesta != RESPUESTA_CONTENIDO_MEMORIA) {
        log_error(log_ks, "Error al recibir contenido de Kernel Memory");
        return NULL;
    }

    t_list* paquete_respuesta = recibir_paquete(socket_km);

    void* contenido_recibido = list_get(paquete_respuesta, 0);

    char* contenido = malloc(tamanio + 1);
    memcpy(contenido, contenido_recibido, tamanio);
    contenido[tamanio] = '\0';

    list_destroy_and_destroy_elements(paquete_respuesta, free);

    return contenido;
}



void liberar_recursos() {   
    log_destroy(log_ks);
    log_destroy(log_debug_ks);
    config_destroy(config);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_block);
    pthread_mutex_destroy(&mutex_cola_exec);
    pthread_mutex_destroy(&mutex_lista_cpu_conectadas);
    pthread_mutex_destroy(&mutex_lista_io_conectadas);
    pthread_mutex_destroy(&mutex_generador_id_io);
    pthread_mutex_destroy(&mutex_sleep);
    pthread_cond_destroy(&cond_cola_ready);
    pthread_cond_destroy(&cond_cola_block);
    pthread_cond_destroy(&cond_lista_cpu_conectadas);
    pthread_cond_destroy(&cond_sleep);
    list_destroy(cola_espera_sleep);
    free(conexion_km_ptr);
    free(config_path);
    free(path_proceso_inicial);
    free(puerto);
    free(str_log_level);
    free(puerto_km);
    free(ip_km);
    free(algoritmo_planificacion);
    list_destroy(cola_ready);
    list_destroy(cola_block);
    list_destroy(cola_exec);
    list_destroy(lista_mutex);
    list_destroy(lista_cpu_conectadas);
    list_destroy(lista_io_conectadas);
    close(server_ks);
    close(conexion_km);
    
    return;
}

//Algo así debería hacer Gabi, para poder enviarnos el id (id), intentar enviarlo como MENSAJE, no como paquete :)
// void enviar_cpu_id(int socket_ks) {  
//     op_code codigo_operacion = ENVIAR_CPU_ID; // debria estar en el enum de op_code
//     t_paquete paquete = crear_paquete(codigo_operacion);
//     int cpu_id = cpu_id_config;

//     agregar_a_paquete(paquete, &cpu_id, sizeof(int));

//     enviar_paquete_send_all(paquete, socket_ks);

//     return;
// }   enviar_paquete_send_all(paquete, socket_ks);

//     return;
// }