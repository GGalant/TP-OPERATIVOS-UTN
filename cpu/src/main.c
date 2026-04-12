#include <commons/log.h>
#include <commons/config.h>
#include <utils/conexiones.h>

int main(int argc, char *argv[]){

    if(argc < 2){
        return EXIT_FAILURE;
    }
    //init cpu
    char* config_path = argv(1); 
    char* id_cpu = argv[2];

    t_config* config = config_create(config_path);

    if(config == NULL){
        return EXIT_FAILURE;
    }
    char* str_log_level = config_get_string_value(config, "LOG_LEVEL");
    int log_level = log_level_from_string(str_log_level);

    int ip_scheduler = config_get_string_value(config, "IP_SCHEDULER");
    int puerto_scheduler = config_get_string_value(config, "PUERTO_SCHEDULER");

    int ip_memory = config_get_string_value(config, "IP_MEMORY");
    int puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");


    t_log* log_cpu = log_create("cpu.log", "CPU", 1, log_level);
    log_info(log_cpu, "Iniciando CPU <%s>", id_cpu);

    //conexiones con modulos

    int client_scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
    if(client_scheduler == -1){
        log_error(log_cpu, "Error al conectar con Kernel Memory en %s:%s", ip_scheduler, puerto_scheduler);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }


    int fd_memoria = crear_conexion(ip_memory, puerto_memory);

    log_destroy(log_cpu);
    config_destroy(config);

    return 0
}