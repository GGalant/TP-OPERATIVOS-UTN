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
    


    while(1) {  
        int codigo_operacion = recibir_operacion(conexion_km);
    }
    
    log_destroy(log_ms);
    config_destroy(config);
    return 0;
}