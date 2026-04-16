#include "memory_stick.h"

int main(int argc, char *argv[]){

    if(argc<3){
        perror("No se proporcionaron todos los argumentos [Archivo Config] [Tamaño]");
        return EXIT_FAILURE;
    }
    
    char* config_path = argv[1]; 
    char* tamanio = argv[2];
    t_config* config = config_create(config_path);
    char* str_log_level;
    char* puerto_scheduler;
    char* ip_km;
    char* puerto_km;
    char* puerto;
    int log_level;
    int err;
   
    if (config == NULL){
        return EXIT_FAILURE;
    }
    
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);
    log_ms = log_create("memory_stick.log", "MemoryStick", 1, log_level);

    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km= config_get_string_value(config, "PUERTO_KM");

    puerto = config_get_string_value(config, "PUERTO");

    //iniciar servidor
    server_ms = iniciar_servidor(log_ms, puerto);

    //conexiones con Kernel Memory
    int conexion_km = crear_conexion(ip_km, puerto_km);

       if (conexion_km == -1) { 
        log_error(log_ms, "No se pudo conectar con Kernel Memory en IP: <%s> , puerto: <%s>", ip_km, puerto_km);

        close(conexion_km);
        return EXIT_FAILURE;
    }

    handshake handshake_MS = HANDSHAKE_MS;
    
    err = send(conexion_km, &handshake_MS, sizeof(handshake), 0);
    if(err == -1){
        perror("Error al intentar el handshake con Kernel Memory");
        return EXIT_FAILURE;
    } 
    log_info(log_ms, "Conectado a Kernel Memory");
    
    //TODO mover la creacion de hilos al switch dentro de handle_connections para el siguiente checkpoint
    //handle_connections();
    //TODO eliminar esto idem arriba
    
    pthread_t hilo_escucha;
    pthread_t hilo_km;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);
    pthread_create(&hilo_km, NULL, handle_km, (void*)(int)conexion_km);

    pthread_join(hilo_escucha, NULL);
    pthread_join(hilo_km, NULL);

    log_destroy(log_ms);
    config_destroy(config);
    return 0;
}

void* handle_connections(void *arg){
    while(1){
        int client_fd = esperar_cliente(server_ms, log_ms);

        int *socket_cliente = malloc(sizeof(int));
        *socket_cliente = client_fd;

        handshake handshake_modulo = recibir_operacion(*socket_cliente);

        switch(handshake_modulo){
            case HANDSHAKE_CPU:
                pthread_t hilo_cpu;
				pthread_create(&hilo_cpu, NULL, handle_cpu, (void*)(int)*socket_cliente);
				pthread_detach(hilo_cpu);
                break;
            default:
                log_warning(log_ms, "Handshake recibido de un modulo desconocido");
                break;
        }
    }
}

void* handle_km(void* arg) {    
    int socket_km = (int)(void*)arg;
    
    log_info(log_ms, "## Conectado a Kernel Memory");
    
    handshake handshake_ks = HANDSHAKE_KS;
    send(socket_km, &handshake_ks, sizeof(handshake), 0);

    while(true) {   
        // aca iria la logica de operaciones con Kernel Memory
        int codigo_operacion = recibir_operacion(socket_km);

        switch(codigo_operacion) {
            case -1:
                log_error(log_ms, "El servidor Kernel Memory se desconectó. Terminando hilo.");
                return NULL;
            default:
                log_warning(log_ms, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

void* handle_cpu(void* arg) {    
    int socket_cpu = (int)(void*)arg;
    
    log_info(log_ms, "CPU <CPU_ID> Conectada");

    while(true) {   
        int codigo_operacion = recibir_operacion(socket_cpu);

        switch(codigo_operacion) {
            case -1:
                log_error(log_ms, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
                return NULL;
            default:
                log_warning(log_ms, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}