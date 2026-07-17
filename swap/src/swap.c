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
    int file_size;
    int block_size;
    int log_level;

    if (config == NULL) {   
        return EXIT_FAILURE;
    }

    ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    
    file_path = config_get_string_value(config, "SWAP_FILE_PATH");
    file_size = config_get_int_value(config, "SWAP_FILE_SIZE");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level); // TODO: REVISAR
    block_size = config_get_int_value(config, "BLOCK_SIZE");
    
    log_swap = log_create("swap.log", "SWAP", true, log_level);

    inicializar_swap(file_path, file_size);

    conexion_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory);

    if (conexion_kernel_memory == -1) { 
        log_error(log_swap, "No se pudo conectar con Kernel Memory en IP: <%s> , puerto: <%s>", ip_kernel_memory, puerto_kernel_memory);

        close(conexion_kernel_memory);
        return EXIT_FAILURE;
    }

    op_code handshake_swap = HANDSHAKE_SWAP;
    send(conexion_kernel_memory, &handshake_swap, sizeof(op_code), 0);
    //swap avisa su block size y file size
    //km tiene que administrarlo
    send(conexion_kernel_memory, &file_size, sizeof(file_size),0);
    send(conexion_kernel_memory, &block_size,sizeof(block_size), 0);
    

    log_info(log_swap, "## Conectado a Kernel Memory");

    
    while (1) { 
        op_code codigo_operacion = recibir_operacion(conexion_kernel_memory);
        
        switch(codigo_operacion) {

            case OBTENER_CONTENIDO_BLOQUE:
            {
                uint32_t numero_bloque;
                recv(conexion_kernel_memory, &numero_bloque, sizeof(uint32_t), MSG_WAITALL);

                char* contenido_leido = malloc(block_size);
                leer_archivo_swap(numero_bloque, contenido_leido,block_size);

                log_info(log_swap, "## Lectura del bloque: %u", numero_bloque);

                //op_code confirmacion = LECTURA_OK;
                t_paquete* contenido_bloque = crear_paquete(LECTURA_OK);
                agregar_a_paquete(contenido_bloque,contenido_leido,block_size);
                //send(conexion_kernel_memory, &confirmacion, sizeof(op_code), 0);
                //send(conexion_kernel_memory, contenido_leido, block_size, 0);
                enviar_paquete_send_all(contenido_bloque,conexion_kernel_memory);


                eliminar_paquete(contenido_bloque);
                free(contenido_leido);
                break;

            }
                
            case GUARDAR_SEGMENTO:
            {
                t_list* segmento_recibido = recibir_paquete(conexion_kernel_memory);
                uint32_t numero_bloque = *(uint32_t*)list_get(segmento_recibido,0);
                
                char* contenido = (char*)list_get(segmento_recibido,1);

                escribir_en_archivo_swap(numero_bloque, contenido, block_size);

                log_info(log_swap, "## Escritura del bloque: %u", numero_bloque);

                op_code confirmacion = ESCRITURA_OK;
                send(conexion_kernel_memory, &confirmacion, sizeof(op_code), 0);

                list_destroy_and_destroy_elements(segmento_recibido,free);

                free(contenido);
                break;
            }
            
            case -1:
                log_error(log_swap, "El servidor Kernel Memory se desconectó. Finalizando Swap");
                munmap(espacio_swap_mmap, file_size);
                return EXIT_FAILURE;
            default:
                log_warning(log_swap, "operacion desconocida: %d", codigo_operacion);
                break;
        }
        
    }
}

void escribir_en_archivo_swap(uint32_t numero_bloque, char* contenido, int block_size){
    int desplazamiento = numero_bloque * block_size;
    memcpy(espacio_swap_mmap + desplazamiento, contenido, block_size);
    msync(espacio_swap_mmap + desplazamiento, block_size, MS_SYNC);

}

void leer_archivo_swap(uint32_t numero_bloque, char* contenido_leido, int block_size){
    int desplazamiento = numero_bloque * block_size;

    memcpy(contenido_leido, espacio_swap_mmap + desplazamiento, block_size);
}

void inicializar_swap(char* file_path, int file_size){
    FILE *archivo = fopen(file_path, "a+");
    int fildes = fileno(archivo);
    ftruncate(fildes,file_size);

    espacio_swap_mmap = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fildes, 0);
}
