#include "main.h"

int main(int argc, char *argv[]){
    // if(argc < 2){
    //     return EXIT_FAILURE;
    // }
    char* config_path = argv[1];

    t_config* config = config_create(config_path);

    if(config == NULL){
        return EXIT_FAILURE;
    }

    char* ip_km = config_get_string_value(config, "IP_KM");
    char* puerto_km = config_get_string_value(config, "PUERTO_KM");
    char* str_log_level = config_get_string_value(config, "LOG_LEVEL");
    char* puerto = config_get_string_value(config, "PUERTO");

    // LOG CONFIG E INIT
    int log_level = log_level_from_string(str_log_level);

    log_ks = log_create("scheduler.log", "SCHEDULER", true, log_level);

    // INICIAR SERVIDOR
    int server_ks = iniciar_servidor(log_ks, puerto);
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
    
    // HILOS PARA ATENDER CLIENTES
    while(1){
        pthread_t hilo_cliente;
        int client_fd = esperar_cliente(server_ks, log_ks);
        if(client_fd == -1){
            log_error(log_ks, "Error al aceptar al cliente: %d", client_fd);
            continue;
        }
        log_info(log_ks, "Cliente conectado: %d", client_fd);
        //(void*)(intptr_t)client_fd sirve para que no se pisen los client_fd antes de que pthread_create cree efectivamente el hilo.
        pthread_create(&hilo_cliente, NULL, handle_connections, (void*)client_fd);
        pthread_detach(hilo_cliente);
    }

    

    log_destroy(log_ks);
    config_destroy(config);

    return 0;
}

void* handle_connections(void* arg){
    int cod_op = recibir_operacion(arg);
    if(cod_op == -1){
        //TODO EL LOG TIENE QUE SER GLOBAL O SER PASADA POR PARAMETRO.
        //log_error(log_ks, "Error al recibir operacion del cliente");
        return -1;
    }

    switch (cod_op)
 		{
 			case HANDSHAKE_CPU:
                //TODO IMPLEMENTAR
 				//handle_query(arg);
 				break;
 			case HANDSHAKE_IO:
 				//TODO IMPLEMENTAR
                //handle_worker(arg);
 				break;
            default:
                //TODO EL LOG TIENE QUE SER GLOBAL O SER PASADA POR PARAMETRO.
 				//log_warning(log_ks, "Handshake no reconocido: %d", cod_op);
 				close(arg);
 				break;
 		}
}
// void* hilo_escucha_conexiones(void* arg) {
//     while (1) {
//       int cliente_fd = esperar_cliente(master_fd, logger_master);
//       handshake handshake_modulo = recibir_operacion(cliente_fd);
// 			switch (handshake_modulo)
// 			{
// 				case HANDSHAKE_QUERY_CONTROL:
// 					pthread_t h_query;
// 					pthread_create(&h_query, NULL, handle_query, (void*)(intptr_t)cliente_fd);
// 					pthread_detach(h_query);
// 					break;
// 				case HANDSHAKE_WORKER:
// 					pthread_t h_worker;
// 					pthread_create(&h_worker, NULL, handle_worker, (void*)(intptr_t)cliente_fd);
// 					pthread_detach(h_worker);
// 					break;
// 				default:
// 					log_warning(logger_master, "Handshake no reconocido: %d", handshake_modulo);
// 					close(cliente_fd);
// 					break;
// 			}
//     }
//     return NULL;
// }