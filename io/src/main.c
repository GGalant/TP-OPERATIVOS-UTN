int main(){

    char* config_path = argv(1); 
    char* config = config_create(config_path);
    
    char* tipo = argv(2);

    if (config == NULL){
        return EXIT_FAILURE;
    }
    
    char* str_log_level = config_get_string_value(config, "LOG_LEVEL");
    int log_level = log_level_from_string(str_log_level);
    t_log* log_io = log_create("io.log", "IO", 1, log_level);

        
    char* ip_scheduler = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");

    //conexiones con scheduler

    int conexion_scheduler = crear_conexion(ip_scheduler, puerto_scheduler);

       if (conexion_scheduler == -1) { 
        log_error(log_io, "No se pudo conectar con Kernel Scheduler en IP: <%s> , puerto: <%s>", ip_scheduler, puerto_scheduler);

        close(conexion_scheduler);
        return EXIT_FAILIRE;
    }

    log_info(log_io, "## Conectado a Kernel Scheduler");

    handshake handshake_io = HANDSHAKE_IO;
    send(conexion_scheduler, &handshake_io, sizeof(handshake), 0);


    log_destroy(log_io);
    config_destroy(config);
    
    return 0
}