#include "main.h"

int main(int argc, char *argv[]){

    char* config_path = argv[1]; 
    char* config = config_create(config_path);
    char* str_log_level;
    char* tipo = argv[2];
    char* puerto_scheduler;
    char* ip_scheduler;
    int log_level;
   
    if (config == NULL){
        return EXIT_FAILURE;
    }
    
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);
    log_io = log_create("io.log", "IO", 1, log_level);

    ip_scheduler = config_get_string_value(config, "IP_KS");
    puerto_scheduler = config_get_string_value(config, "PUERTO_KS");

    //conexiones con scheduler
    int conexion_ks = crear_conexion(ip_scheduler, puerto_scheduler);

       if (conexion_ks == -1) { 
        log_error(log_io, "No se pudo conectar con Kernel Scheduler en IP: <%s> , puerto: <%s>", ip_scheduler, puerto_scheduler);

        close(conexion_ks);
        return EXIT_FAILURE;
    }

    handshake handshake_io = HANDSHAKE_IO;
    send(conexion_ks, &handshake_io, sizeof(handshake), 0);
    log_info(log_io, "## Conectado a Kernel Scheduler");

    log_destroy(log_io);
    config_destroy(config);

    while(1) {  
        // int codigo_operacion = recibir_operacion(conexion_ks);
    }
    
    return 0;
}