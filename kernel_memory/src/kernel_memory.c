#include "kernel_memory.h"
//#include "globales.c" idea a desarrollar despues

//t_log* log_km;

int main(int argc, char *argv[]){

    if(argc<2){
        perror("No se proporcionaron todos los argumentos [Archivo Config]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1];
    char* str_log_level;
    char* puerto_km;
    t_config* config = config_create(config_path);
    pthread_t hilo_escucha;
    
    if (config == NULL){
        perror("Error al intentar cargar el config");
        return EXIT_FAILURE;
    }

    puerto_km = config_get_string_value(config,"PUERTO");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    
    if(puerto_km == NULL || str_log_level == NULL){
        config_destroy(config);
        return EXIT_FAILURE;
    }
    
    int log_level = log_level_from_string(str_log_level);
    log_km = log_create("memory.log", "MEMORY", true, log_level); 
    
    server_km = iniciar_servidor(log_km, puerto_km);

     if(server_km == -1){
        log_error(log_km, "Error al iniciar el servidor en el puerto %s", puerto_km);
        log_destroy(log_km);
        config_destroy(config);
        return EXIT_FAILURE;
    }
    log_info(log_km, "Kernel Memory iniciado");
    
    //TODO duda: malloc para el socket para que no se pisen?;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);

    // dejar los pthread_join al final del main para que no bloqueen el main
    pthread_join(hilo_escucha, NULL); // Sirve para esperar a que un hilo termine su ejecución

    log_destroy(log_km);
    config_destroy(config);

    return 0;
}

void* handle_connections(void* arg){
    while(1) {  
        int client_fd = esperar_cliente(server_km, log_km);

        int *socket_cliente = malloc(sizeof(int));
        *socket_cliente = client_fd;

        handshake handshake_modulo = recibir_operacion(*socket_cliente);

        switch (handshake_modulo) {
 			case HANDSHAKE_CPU:
                pthread_t hilo_cpu;
				pthread_create(&hilo_cpu, NULL, handle_cpu, socket_cliente);
				pthread_detach(hilo_cpu);
 				break;
 			case HANDSHAKE_KS:
                pthread_t hilo_ks;
                pthread_create(&hilo_ks, NULL, handle_ks, socket_cliente);
				pthread_detach(hilo_ks);
 				//TODO IMPLEMENTAR
 				break;
            case HANDSHAKE_MS:
                log_info(log_km, "Memory Stick Conectado. Guardando socket.");
                // Guardamos el socket de MS de forma global y NO CREAMOS HILO, ya que no es necesariamente asincrono.
                socket_memory_stick = *socket_cliente;
                free(socket_cliente);
                break;
            case HANDSHAKE_SWAP:
                log_info(log_km, "Swap Conectado. Guardando socket.");
                // Guardamos el socket de SWAP de forma global y NO CREAMOS HILO, ya que no es necesariamente asincrono.
                socket_swap = *socket_cliente;
                free(socket_cliente);
                break;
            default:
 				log_warning(log_km, "Handshake no reconocido: %d", handshake_modulo);
 				//close(socket);
 				break;
 		}
    }
} 

void* handle_ks(void* arg) {    
    int socket_ks = *(int*)arg;
    
    log_info(log_km, "Kernel Scheduler Conectado - FD del socket: %d", socket_ks);

    while(true) {   
        int codigo_operacion = recibir_operacion(socket_ks);

        switch(codigo_operacion) {
            case -1:
                log_error(log_km, "Kernel Scheduler FD %d se desconectó. Terminando handle", socket_ks);
                return NULL;
            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

void* handle_cpu(void* arg) {    
    int socket_cpu = *(int*)arg;
    
    log_info(log_km, "CPU <CPU_ID> Conectada");

    while(true) {   
        int codigo_operacion = recibir_operacion(socket_cpu);

        switch(codigo_operacion) {
            case -1:
                log_error(log_km, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
                return NULL;
            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}