#include "kernel_scheduler.h"
#include "globales.h"

int main(int argc, char *argv[]){

    inicializar_ks(argc, argv);
    
    // CONEXION CON KERNEL MEMORY
    int conexion_km = crear_conexion(ip_km, puerto_km);
    if(conexion_km == -1){
        log_error(log_ks, "Error al conectar con Kernel Memory en %s:%s", ip_km, puerto_km);
        log_destroy(log_ks);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    log_info(log_ks, "## Conectado a Kernel Memory");
    
    handshake handshake_ks = HANDSHAKE_KS;
    send(conexion_km, &handshake_ks, sizeof(handshake), 0);

    crear_proceso_inicial(conexion_km, path_proceso_inicial);

    pthread_t hilo_escucha;
    pthread_t hilo_km;
    int* conexion_km_ptr = malloc(sizeof(int));
    *conexion_km_ptr = conexion_km;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);
    pthread_create(&hilo_km, NULL, handle_km, conexion_km_ptr);

    pthread_join(hilo_escucha, NULL);
    pthread_join(hilo_km, NULL);

    log_destroy(log_ks);
    config_destroy(config);

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
    log_level = log_level_from_string(str_log_level);

    log_ks = log_create("scheduler.log", "SCHEDULER", true, log_level);
    log_debug_ks = log_create("scheduler_debug.log", "SCHEDULER_DEBUG", true, DEBUG);

    cola_ready = list_create();
    cola_block = list_create();
    cola_exec = list_create();
    lista_mutex = list_create();
    lista_cpu_conectadas = list_create();

    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_block, NULL);
    pthread_mutex_init(&mutex_cola_exec, NULL);
    pthread_mutex_init(&mutex_lista_cpu_conectadas, NULL);
    pthread_cond_init(&cond_cola_ready, NULL);
    pthread_cond_init(&cond_cola_block, NULL);
    pthread_cond_init(&cond_lista_cpu_conectadas, NULL);

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
}

void* handle_io(void* arg) {    
    int socket_io = *(int*)arg;
    free(arg);
    
    log_info(log_ks, "IO conectada: %d", socket_io);

    while(true) {   
        // aca iria la logica de operaciones con IO 
        int codigo_operacion = recibir_operacion(socket_io);

        switch(codigo_operacion) {
            case -1:
                log_error(log_ks, "IO FD %d se desconectó. Terminando handle", socket_io);
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

void cambiar_estado_proceso(t_proceso_pcb *proceso, t_estado_proceso estado_siguiente) {    
    t_estado_proceso estado_anterior = proceso->estado;
    proceso->estado = estado_siguiente;

    log_info(log_ks, "## (<%d>) Pasa del estado <%s> al estado <%s>", proceso->pid, estado_to_string(estado_anterior), estado_to_string(estado_siguiente));
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