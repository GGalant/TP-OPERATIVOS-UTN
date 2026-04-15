#include "main.h"

int main(int argc, char *argv[]){
    char* config_path = argv[1];
    char* puerto;
    char* str_log_level;
    char* puerto_km;
    char* ip_km;
    int log_level;
    t_config* config = config_create(config_path);
    pthread_t hilo_escucha;

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

    log_info(log_ks, "Conectado a Kernel Memory");
    
    handshake handshake_ks = HANDSHAKE_KS;
    send(conexion_km, &handshake_ks, sizeof(handshake), 0);

    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);
    pthread_join(hilo_escucha, NULL);

    log_destroy(log_ks);
    config_destroy(config);

    return 0;
}


void* handle_connections(void* arg){
    while(1) {  
        int client_fd = esperar_cliente(server_ks, log_ks);
        handshake handshake_modulo = recibir_operacion(client_fd);

        switch (handshake_modulo) {
 			case HANDSHAKE_CPU:
                log_info(log_ks, "CPU conectada: %d", client_fd);
 				//handle_query(socket);
 				break;
 			case HANDSHAKE_IO:
                log_info(log_ks, "IO conectada: %d", client_fd);
 				//TODO IMPLEMENTAR
                //handle_worker(socket);
 				break;
            default:
 				log_warning(log_ks, "Handshake no reconocido: %d", handshake_modulo);
 				//close(socket);
 				break;
 		}
    }
}    
