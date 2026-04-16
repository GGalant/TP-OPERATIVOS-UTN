#include "cpu.h"

int main(int argc, char *argv[]){

    if(argc<3){
        perror("No se proporcionaron todos los argumentos [Archivo Config] [Identificador]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1]; 
    char* id_cpu = argv[2];
    char* str_log_level;
    char* ip_ks;
    char* puerto_ks;
    char* ip_km;
    char* puerto_km;
    char* ip_ms;
    char* puerto_ms;
    int log_level;
    t_config* config = config_create(config_path);
    pthread_t hilo_ks;

    if(config == NULL){
        return EXIT_FAILURE;
    }

    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);

    ip_ks = config_get_string_value(config, "IP_KS");
    puerto_ks = config_get_string_value(config, "PUERTO_KS");

    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km = config_get_string_value(config, "PUERTO_KM");

    ip_ms= config_get_string_value(config, "IP_MS");
    puerto_ms = config_get_string_value(config, "PUERTO_MS");



    log_cpu = log_create("cpu.log", "CPU", 1, log_level);
    log_info(log_cpu, "Iniciando CPU <%s>", id_cpu);

    //conexiones con modulos

    int conexion_ks = crear_conexion(ip_ks, puerto_ks);

    if(conexion_ks == -1){
        log_error(log_cpu, "Error al conectar con Kernel Scheduler en %s:%s", ip_ks, puerto_ks);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    handshake handshake_cpu_ks = HANDSHAKE_CPU;
    send(conexion_ks, &handshake_cpu_ks, sizeof(handshake), 0);

    pthread_create(&hilo_ks, NULL, handle_ks, (void*)(int)conexion_ks);
	

    int conexion_km = crear_conexion(ip_km, puerto_km);

    if(conexion_km == -1){
        log_error(log_cpu, "Error al conectar con Kernel Memory en %s:%s", ip_km, puerto_km);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    handshake handshake_cpu_km = HANDSHAKE_CPU;
    send(conexion_km, &handshake_cpu_km, sizeof(handshake), 0);

    int conexion_ms = crear_conexion(ip_ms, puerto_ms);

    if(conexion_ms == -1){
        log_error(log_cpu, "Error al conectar con Memory Stick en %s:%s", ip_ms, puerto_ms);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    handshake handshake_cpu_ms = HANDSHAKE_CPU;
    send(conexion_ms, &handshake_cpu_ms, sizeof(handshake), 0);
    log_info(log_cpu, "Conectado a Memory Stick");

    while(true) {   
        int codigo_operacion = recibir_operacion(conexion_km);

        switch(codigo_operacion) {  
            case -1:
                log_error(log_cpu, "El servidor Kernel Memory se desconectó.");
                return EXIT_FAILURE;  
            default:
                log_warning(log_cpu, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
    
    pthread_join(hilo_ks, NULL);

    log_destroy(log_cpu);
    config_destroy(config);
    
    return 0;
}

void* handle_ks(void* arg) {    
    int socket_ks = (int)(void*)arg;
    
    log_info(log_cpu, "Conectado a kernel scheduler");

    while(true) {   
        // aca iria la logica de operaciones con Kernel Schedule
        int codigo_operacion = recibir_operacion(socket_ks);

        switch(codigo_operacion) {
            case -1:
                log_error(log_cpu, "El servidor Kernel Scheduler se desconectó. Terminando hilo.");
                return NULL;
            default:
                log_warning(log_cpu, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}