#include "kernel_scheduler.h"

int main(int argc, char *argv[]){

    if(argc<2){
        perror("No se proporcionaron todos los argumentos [Archivo Config]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1];
    char* puerto;
    char* str_log_level;
    char* puerto_km;
    char* ip_km;
    int log_level;
    t_config* config = config_create(config_path);

    if(config == NULL){
        return EXIT_FAILURE;
    }

    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km = config_get_string_value(config, "PUERTO_KM");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    puerto = config_get_string_value(config, "PUERTO");
    log_level = log_level_from_string(str_log_level);

    log_ks = log_create("scheduler.log", "SCHEDULER", true, log_level);

    // INICIAR SERVIDOR
    server_ks = iniciar_servidor(log_ks, puerto);
    if(server_ks == -1){
        log_error(log_ks, "Error al iniciar el servidor en el puerto %s", puerto);
        log_destroy(log_ks);
        config_destroy(config);
        return EXIT_FAILURE;
    }
    
    log_info(log_ks, "Kernel Scheduler iniciado");
    
    // CONEXION CON KERNEL MEMORY
    int conexion_km = crear_conexion(ip_km, puerto_km);
    if(conexion_km == -1){
        log_error(log_ks, "Error al conectar con Kernel Memory en %s:%s", ip_km, puerto_km);
        log_destroy(log_ks);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    //TODO mover la creacion de hilos al switch dentro de handle_connections para el siguiente checkpoint
    //handle_connections();
    //TODO eliminar esto idem arriba
    
    pthread_t hilo_escucha;
    pthread_t hilo_km;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);
    pthread_create(&hilo_km, NULL, handle_km, (void*)(int)conexion_km);

    // dejar los pthread_join al final del main para que no bloqueen el main 
    pthread_join(hilo_escucha, NULL);
    pthread_join(hilo_km, NULL);

    log_destroy(log_ks);
    config_destroy(config);

    return 0;
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
    int socket_km = (int)(void*)arg;
    
    log_info(log_ks, "## Conectado a Kernel Memory");
    
    handshake handshake_ks = HANDSHAKE_KS;
    send(socket_km, &handshake_ks, sizeof(handshake), 0);

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
    
    log_info(log_ks, "## CPU <ID CPU> Conectada: %d", socket_cpu);

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