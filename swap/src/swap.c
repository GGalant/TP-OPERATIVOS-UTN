#include "swap.h"

int main(int argc, char* argv[]) { 

    if(argc<2){
        perror("No se proporcionaron todos los argumentos [Archivo Config]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1];
    t_config* config = config_create(config_path);
    char* str_log_level;
    char* ip_kernel_memory;
    char* puerto_kernel_memory;
    char* file_path;
    char* file_size;
    int log_level;

    if (config == NULL) {   
        return EXIT_FAILURE;
    }

    ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    file_path = config_get_string_value(config, "SWAP_FILE_PATH");
    file_size = config_get_string_value(config, "SWAP_FILE_SIZE");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);
    
    log_swap = log_create("swap.log", "SWAP", true, log_level);

    conexion_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory);

    if (conexion_kernel_memory == -1) { 
        log_error(log_swap, "No se pudo conectar con Kernel Memory en IP: <%s> , puerto: <%s>", ip_kernel_memory, puerto_kernel_memory);

        close(conexion_kernel_memory);
        return EXIT_FAILURE;
    }

    op_code handshake_swap = HANDSHAKE_SWAP;
    send(conexion_kernel_memory, &handshake_swap, sizeof(op_code), 0);
    
    log_info(log_swap, "## Conectado a Kernel Memory");
    
    while (1) { 
        op_code codigo_operacion = recibir_operacion(conexion_kernel_memory);
        
        switch(codigo_operacion) {
            case -1:
                log_error(log_swap, "El servidor Kernel Memory se desconectó. Finalizando Swap");
                return EXIT_FAILURE;
            default:
                log_warning(log_swap, "operacion desconocida: %d", codigo_operacion);
                break;
        }
        
    }
}