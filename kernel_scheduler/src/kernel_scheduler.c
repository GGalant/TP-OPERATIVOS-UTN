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

    cola_ready = list_create();
    cola_block = list_create();
    cola_exec = list_create();
    lista_mutex = list_create();
    lista_cpu_conectadas = list_create();

    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_block, NULL);
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
    free(arg);
    int cpu_id_recibido;

    cpu_id_recibido = recibir_cpu_id(socket_cpu);
    if (cpu_id_recibido >= 0) { 
        t_cpu* cpu = malloc(sizeof(t_cpu));
        cpu->cpu_id = cpu_id_recibido;
        cpu->socket_cpu = socket_cpu;
        cpu->estado = CPU_LIBRE;
        cpu->pid_actual = -1;

        pthread_mutex_lock(&mutex_lista_cpu_conectadas);
        list_add(lista_cpu_conectadas, cpu);
        pthread_mutex_unlock(&mutex_lista_cpu_conectadas);

        log_info(log_ks, "## CPU <ID CPU> Conectada: %d", cpu_id_recibido);
    } else {    
        perror("No se le pudo asignar un ID a la CPU");
        close(socket_cpu);
        return NULL;
    }
    
    while(true) {   
        // aca iria la logica de operaciones con CPU 
        int codigo_operacion = recibir_operacion(socket_cpu);

        switch(codigo_operacion) {
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

        pcb_proceso_inicial->estado = READY;
        // aca podriamos proteger la variable global con un mutex de C
        list_add(cola_ready, pcb_proceso_inicial);
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

//Algo así debería hacer Gabi, para poder enviarnos el id (id), intentar enviarlo como MENSAJE, no como paquete :)
// void enviar_cpu_id(int socket_ks) {  
//     op_code codigo_operacion = ENVIAR_CPU_ID; // debria estar en el enum de op_code
//     t_paquete paquete = crear_paquete(codigo_operacion);
//     int cpu_id = cpu_id_config;

//     agregar_a_paquete(paquete, &cpu_id, sizeof(int));

//     enviar_paquete_send_all(paquete, socket_ks);

//     return;
// }