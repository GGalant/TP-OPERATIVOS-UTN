#include "main.h"
//#include "globales.c" idea a desarrollar despues

//t_log* log_km;

int main(int argc, char *argv[]){

    char* config_path = argv[1];
    t_config* config = config_create(config_path);
    
    if (config == NULL){
        perror("Error al intentar cargar el config");
        return EXIT_FAILURE;
    }

    char* puerto_km = config_get_string_value(config,"PUERTO");
    char* str_log_level = config_get_string_value(config, "LOG_LEVEL");
    
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
    
    while(1){
        pthread_t hilo_cliente;
        // printf("lo q llega del handshake: %d \n", handshake_modulo);

        // if(client_fd == -1){
        //     log_error(log_km, "Error al aceptar cliente");
        //     continue;
        // }
        // log_info(log_km, "Cliente conectado: %d", client_fd);
        //TODO duda: malloc para el socket para que no se pisen?;
        pthread_create(&hilo_cliente, NULL, handle_connections, NULL);
        pthread_detach(hilo_cliente);
    }

    log_destroy(log_km);
    config_destroy(config);

    return 0;
}

void* handle_connections(void* arg){
    int client_fd = esperar_cliente(server_km, log_km);
    handshake handshake_modulo = recibir_operacion(client_fd);
    // if(cod_op == -1){
    //     log_error(log_km, "Error al recibir operacion del cliente");
    //     return -1;
    // }

    switch (handshake_modulo)
 		{
 			case HANDSHAKE_CPU:
                //TODO IMPLEMENTAR
 				//handle_query(socket);
 				break;
 			case HANDSHAKE_KS:
                log_info(log_km, "Cliente conectado: %d", client_fd);
 				//TODO IMPLEMENTAR
                //handle_worker(socket);
 				break;
            case HANDSHAKE_MEMORYSTICK:
                //IMPLEMENTAR
                break;
            case HANDSHAKE_SWAP:
                //IMPLEMENTAR
                break;
            default:
 				log_warning(log_km, "Handshake no reconocido: %d", handshake_modulo);
 				close(socket);
 				break;
 		}
}
    


