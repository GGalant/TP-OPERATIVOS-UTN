#include "io.h"

int main(int argc, char *argv[]){
  
    inicializar_io(argc, argv); 
    //conexiones con scheduler
    conexion_ks = crear_conexion(ip_scheduler, puerto_scheduler);
    if (conexion_ks == -1) { 
        log_error(log_io, "No se pudo conectar con Kernel Scheduler en IP: <%s> , puerto: <%s>", ip_scheduler, puerto_scheduler);
        log_destroy(log_io);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    log_info(log_io, "## Conectado a Kernel Scheduler");
    handshake handshake_io = HANDSHAKE_IO;
    send(conexion_ks, &handshake_io, sizeof(handshake), 0);
    
    enviar_io();

    switch (obtener_tipo_io(tipo_io)) {
    case SLEEP:
        handle_io_sleep();
        break;
    case STDIN:
        handle_io_stdin();
        break;
    case STDOUT:
        handle_io_stdout();
        break;
    default:
        log_error(log_io, "Tipo de IO desconocida: <%s>", tipo_io);
        break;  
    }
  
    liberar_recursos();
  
    return 0;
}

void inicializar_io(int argc, char *argv[]) {  
    if(argc<3) {
          perror("No se proporcionaron todos los argumentos [Archivo Config] [Tipo]");
          return EXIT_FAILURE;
    }
    char* config_path = argv[1]; 
    config = config_create(config_path);
    tipo_io = argv[2];
    int log_level;

    if (config == NULL){
        return EXIT_FAILURE;
    }

    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);
    log_io = log_create("io.log", "IO", 1, log_level);
    ip_scheduler = config_get_string_value(config, "IP_KS");
    puerto_scheduler = config_get_string_value(config, "PUERTO_KS");

    return;
}

void handle_io_sleep() {
    while (true) {
        list* paquete_sleep = recibir_paquete(conexion_ks); 
        int pid = *(int*)list_get(paquete_sleep, 0);
        int tiempo_sleep = *(int*)list_get(paquete_sleep, 1);
        list_destroy_and_destroy_elements(paquete_sleep, free); 

        log_info(log_io, "## PID: %d - Inicio de IO", pid);
        usleep(tiempo_sleep);
        log_info(log_io, "## PID: %d - Fin de IO", pid);    

        enviar_fin_io(pid);
    }   

    return;
}

void handle_io_stdin() {
    while (true) {
      //TODO IMPLEMENTAR 
    }  

    return;
}

void handle_io_stdout() {
    while (true) {
      //TODO IMPLEMENTAR 
    }   

    return;
}

void enviar_io() {
    t_paquete* paquete_io = crear_paquete(CREAR_IO);
    int tipo = obtener_tipo_io(tipo_io);
    agregar_a_paquete(paquete_io, &tipo, sizeof(int));
    enviar_paquete_send_all(paquete_io, conexion_ks);
    eliminar_paquete(paquete_io);
    return;
}

void enviar_fin_io(int pid) {
    t_paquete* paquete_fin_io = crear_paquete(FIN_IO);
    agregar_a_paquete(paquete_fin_io, &pid, sizeof(int));
    enviar_paquete_send_all(paquete_fin_io, conexion_ks);
    eliminar_paquete(paquete_fin_io);   

    return;
}

tipo_io obtener_tipo_io(char* tipo) {
    if (strcmp(tipo, "STDIN") == 0) return STDIN;
    if (strcmp(tipo, "STDOUT") == 0) return STDOUT;
    if (strcmp(tipo, "SLEEP") == 0) return SLEEP;
    return -1; 
}

void liberar_recursos() {
    log_destroy(log_io);
    config_destroy(config);
    free(ip_scheduler);
    free(puerto_scheduler);
    free(str_log_level);   

    return;
}