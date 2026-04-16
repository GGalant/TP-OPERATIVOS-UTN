#include "swap.h"

int main(int argc, char* argv[]) { 

    if(argc<2){
        perror("No se proporcionaron todos los argumentos [Archivo Config]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1];
    t_config* config = config_create(config_path);

    if (config == NULL) {   
        return EXIT_FAILURE;
    }

    char* ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    char* string_log_level = config_get_string_value(config, "LOG_LEVEL");
    char* file_path = config_get_string_value(config, "SWAP_FILE_PATH");
    char* file_size = config_get_string_value(config, "SWAP_FILE_SIZE");
    int conexion_kernel_memory; 

    t_log_level log_level = log_level_from_string(string_log_level);
    
    t_log* log_swap = log_create("swap.log", "SWAP", true, log_level);

    conexion_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory);

    if (conexion_kernel_memory == -1) { 
        log_error(log_swap, "No se pudo conectar con Kernel Memory en IP: <%s> , puerto: <%s>", ip_kernel_memory, puerto_kernel_memory);

        close(conexion_kernel_memory);
        return EXIT_FAILURE;
    }

    log_info(log_swap, "## Conectado a Kernel Memory");

    handshake handshake_swap = HANDSHAKE_SWAP;
    send(conexion_kernel_memory, &handshake_swap, sizeof(handshake), 0);
    

    while (1) { 
        op_code operacion = recibir_operacion(conexion_kernel_memory);
        
        
    }
}