#include "memory_stick.h"
int memory_delay;
void* espacio_memoria;
int tamanio_int;

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

    tamanio_int = atoi(tamanio);
   
    if (config == NULL){
        return EXIT_FAILURE;
    }
    
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);
    log_ms = log_create("memory_stick.log", "MemoryStick", 1, log_level);

    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km= config_get_string_value(config, "PUERTO_KM");

    puerto = config_get_string_value(config, "PUERTO");

    memory_delay = config_get_int_value(config, "MEMORY_DELAY");

    //iniciar servidor
    server_ms = iniciar_servidor(log_ms, puerto);

    //malloc del tamaño del ms
    //void* inicio_memoria = malloc(tamanio);
    espacio_memoria = malloc(tamanio_int);
    
    if(espacio_memoria == NULL){
        log_error(log_ms, "No se pudo reservar la memoria indicada");
        return EXIT_FAILURE;
    }

    //conexiones con Kernel Memory
    int conexion_km = crear_conexion(ip_km, puerto_km);

       if (conexion_km == -1) { 
        log_error(log_ms, "No se pudo conectar con Kernel Memory en IP: <%s> , puerto: <%s>", ip_km, puerto_km);

        close(conexion_km);
        return EXIT_FAILURE;
    }

    op_code handshake_MS = HANDSHAKE_MS;
    
    err = send(conexion_km, &handshake_MS, sizeof(op_code), 0);
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
    pthread_create(&hilo_km, NULL, handle_km, (void*)&conexion_km);

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

        op_code handshake_modulo = recibir_operacion(*socket_cliente);

        switch(handshake_modulo){
            case HANDSHAKE_CPU:
                pthread_t hilo_cpu;
				pthread_create(&hilo_cpu, NULL, handle_cpu, (void*)&socket_cliente);
				pthread_detach(hilo_cpu);
                break;
            default:
                log_warning(log_ms, "Handshake recibido de un modulo desconocido");
                break;
        }
    }
}

void* handle_km(void* arg) {    
    int socket_km = *(int*)arg;
    
    log_info(log_ms, "## Conectado a Kernel Memory");


    //como deliramos aca ee --------------------------------------------
    // op_code handshake_ks = HANDSHAKE_KS; 
    // send(socket_km, &handshake_ks, sizeof(op_code), 0);

    // op_code handshake_ms = HANDSHAKE_MEMORYSTICK;
    // send(socket_km, &handshake_ms, sizeof(op_code), 0);

    while(true) {   
        // aca iria la logica de operaciones con Kernel Memory
        int codigo_operacion = recibir_operacion(socket_km);

        switch(codigo_operacion) {
            case SOLICITUD_ESCRITURA:
            {

                t_list* solicitud_escritura = recibir_paquete(socket_km);
                uint32_t direccion_fisica = *(uint32_t*)list_get(solicitud_escritura,0);
                uint32_t cantidad_bytes = *(uint32_t*)list_get(solicitud_escritura,1);
                char* contenido_a_escribir = (char*)list_get(solicitud_escritura,2);

                //verficiar que entre en el ms
                bool escritura_valida = cantidad_bytes > 0 && direccion_fisica < tamanio_int && (uint32_t) cantidad_bytes <= tamanio_int - direccion_fisica;

                if (!escritura_valida) {
                    log_error(log_ms, "Escritura fuera de rango");
                    op_code error_escritura = ERROR_ESCRITURA;
                    send(socket_km, &error_escritura, sizeof(error_escritura),0);

                    list_destroy_and_destroy_elements(solicitud_escritura, free);
                    //return EXIT_FAILURE;
                    break;
                    
                }

                //aplicar memory delay
                usleep(memory_delay * 1000);
                //escribir los bytes
                memcpy((char*)espacio_memoria + direccion_fisica, contenido_a_escribir,cantidad_bytes);



                //confirmacion
                op_code escritura_ok = ESCRITURA_OK;
                send(socket_km, &escritura_ok,sizeof(escritura_ok),0);

                log_info(log_ms, "## Escritura de <%d> bytes",cantidad_bytes);
            }
                break;

            case SOLICITUD_LECTURA:
            {
                t_list* solicitud_lectura = recibir_paquete(socket_km);
                uint32_t direccion_fisica = *(uint32_t*)list_get(solicitud_lectura,0);
                int cantidad_bytes = *(int*)list_get(solicitud_lectura,1);

                bool lectura_valida = cantidad_bytes > 0 && direccion_fisica < tamanio_int && (uint32_t) cantidad_bytes <= tamanio_int - direccion_fisica;
                
                if (!lectura_valida) {
                    log_error(log_ms, "Lectura fuera de rango");
                    op_code error_lectura = ERROR_ESCRITURA;
                    send(socket_km, &error_lectura, sizeof(error_lectura), 0);

                    list_destroy_and_destroy_elements(solicitud_lectura, free);
                    //return EXIT_FAILURE;
                    break;
                    
                }
                //aplicar memory_delay

                usleep(memory_delay * 1000);
                //leer bloques
                void* buffer_temp = malloc(cantidad_bytes);

                memcpy(buffer_temp, espacio_memoria + direccion_fisica,cantidad_bytes);

                //confirmacion
                op_code lectura_ok = LECTURA_OK;
                send(socket_km, &lectura_ok,sizeof(lectura_ok),0);
                send(socket_km, buffer_temp, cantidad_bytes,0);

                log_info(log_ms, "## Lectura de <%d> bytes", cantidad_bytes);
            }
                break;
            
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
    int socket_cpu = *(int*)arg;
    
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