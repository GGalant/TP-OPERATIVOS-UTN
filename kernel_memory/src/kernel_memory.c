 #include "kernel_memory.h"
//#include "globales.c" idea a desarrollar despues

//t_log* log_km;

int main(int argc, char *argv[]){

    int contador_sticks = 0;
    int posicion_memoria = 0;

    if(argc<2){
        perror("No se proporcionaron todos los argumentos [Archivo Config]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1];
    char* str_log_level;
    char* puerto_km;
    t_config* config = config_create(config_path);
    pthread_t hilo_escucha;
    
    if (config == NULL){
        perror("Error al intentar cargar el config");
        return EXIT_FAILURE;
    }

    puerto_km = config_get_string_value(config,"PUERTO");
    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    
    if(puerto_km == NULL || str_log_level == NULL){
        config_destroy(config);
        return EXIT_FAILURE;
    }
    
    int log_level = log_level_from_string(str_log_level);
    log_km = log_create("memory.log", "MEMORY", true, log_level);
    
    server_km = iniciar_servidor(log_km, puerto_km);

     if(server_km == -1){
        log_error(log_km, "Error al iniciar el servidor en el puerto %s", puerto_km);
        log_destroy(log_km);
        config_destroy(config);
        return EXIT_FAILURE;
    }
    log_info(log_km, "Kernel Memory iniciado");

    //inicializamos estructuras
    diccionario_contextos_ejecucion = dictionary_create();
    diccionario_paths = dictionary_create();
    sticks_conectados = list_create();
    huecos = list_create();
    
    //TODO duda: malloc para el socket para que no se pisen?;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);

    // dejar los pthread_join al final del main para que no bloqueen el main
    pthread_join(hilo_escucha, NULL); // Sirve para esperar a que un hilo termine su ejecución

    log_destroy(log_km);
    config_destroy(config);

    return 0;
}

void* handle_connections(void* arg){
    while(1) {  
        int client_fd = esperar_cliente(server_km, log_km);

        int *socket_cliente = malloc(sizeof(int));
        *socket_cliente = client_fd;

        op_code handshake_modulo = recibir_operacion(*socket_cliente);

        switch (handshake_modulo) {
 			case HANDSHAKE_CPU:
                pthread_t hilo_cpu;
				pthread_create(&hilo_cpu, NULL, handle_cpu, socket_cliente);
				pthread_detach(hilo_cpu);
 				break;
 			case HANDSHAKE_KS:
                pthread_t hilo_ks;
                pthread_create(&hilo_ks, NULL, handle_ks, socket_cliente);
				pthread_detach(hilo_ks);
 				//TODO IMPLEMENTAR
 				break;
            case HANDSHAKE_MS:
                log_info(log_km, "Memory Stick Conectado. Guardando socket.");
                // Guardamos el socket de MS de forma global y NO CREAMOS HILO, ya que no es necesariamente asincrono.
                //handle_ms();
                socket_memory_stick = *socket_cliente;
                free(socket_cliente);
                break;
            case HANDSHAKE_SWAP:
                log_info(log_km, "Swap Conectado. Guardando socket.");
                // Guardamos el socket de SWAP de forma global y NO CREAMOS HILO, ya que no es necesariamente asincrono.
                socket_swap = *socket_cliente;
                free(socket_cliente);
                break;
            default:
 				log_warning(log_km, "Handshake no reconocido: %d", handshake_modulo);
 				//close(socket);
 				break;
 		}
    }
} 

void* handle_ks(void* arg) {    
    int socket_ks = *(int*)arg;
    
    log_info(log_km, "Kernel Scheduler Conectado - FD del socket: %d", socket_ks);

    while(true) {   
        int codigo_operacion = recibir_operacion(socket_ks);

        switch(codigo_operacion) {
            case ERROR:
                log_error(log_km, "Kernel Scheduler FD %d se desconectó. Terminando handle", socket_ks);
                return NULL;
                
            
            case INICIALIZAR_PROCESO:
                log_debug(log_km, "Kernel Scheduler solicitó INICIALIZAR_PROCESO");

                // t_list* instrucciones = cargar_instrucciones(path_archivo_instrucciones);                
                // dictionary_put(diccionario_paths,str_pid_recibido,instrucciones);    
                
                t_list *proceso_inicial = recibir_paquete(socket_ks);
                pid_t pid_recibido = *(pid_t*) list_get(proceso_inicial, 0);
                char* path_instrucciones_recibido = (char*)list_get(proceso_inicial, 1);
                char *str_pid_recibido = string_itoa(pid_recibido);

                t_list* instrucciones = cargar_instrucciones(path_instrucciones_recibido); //arreglarla
                dictionary_put(diccionario_paths,str_pid_recibido,instrucciones); //diccionaria con instrucciones x pid
                //log_debug(log_km_debug, "Instrucciones del proceso: <%d> guardadas correctamente", pid_recibido);
                log_info(log_km, "Instrucciones del proceso: <%d> guardadas correctamente", pid_recibido);
                
                t_contextoEjec* contexto_proceso = malloc(sizeof(t_contextoEjec));
                contexto_proceso->pid = pid_recibido;
                contexto_proceso->pc = 0;
                contexto_proceso->ax = 0;
                contexto_proceso->bx = 0;
                contexto_proceso->cx = 0;
                contexto_proceso->dx = 0;                
                contexto_proceso->eax = 0;
                contexto_proceso->ebx = 0;
                contexto_proceso->ecx = 0;
                contexto_proceso->edx = 0;
                contexto_proceso->si = 0;
                contexto_proceso->di = 0;
                contexto_proceso->tabla_segmentos = list_create();

                dictionary_put(diccionario_contextos_ejecucion, str_pid_recibido, contexto_proceso);
                
                //log_debug(log_km_debug, "Contexto de ejecución del proceso <%s> creado!", str_pid_recibido);
                log_info(log_km, "Contexto de ejecución del proceso <%s> creado!", str_pid_recibido);
                

                op_code inicializar_proceso_ok = INICIALIZAR_PROCESO_OK;
                send(socket_ks, &inicializar_proceso_ok, sizeof(inicializar_proceso_ok),0);
                
                list_destroy_and_destroy_elements(proceso_inicial, free);

                break;

            case STDOUT_LECTURA:{ 
                //por pedido del ks
                t_list* pedido_lectura = recibir_paquete(socket_ks); //esperamos (PID), dirFisica y tamaño a leer
                pid_t pid = *(pid_t*)list_get(pedido_lectura,0);
                //char* direccion_logica = list_get(pedido_lectura, 1);
                char* direccion_fisica = list_get(pedido_lectura, 1);
                void* bytes_a_leer = list_get(pedido_lectura,2);
                
                // op_code solicitud_traduccion = SOLICITUD_TRADUCCION;
                // int tam_dirLogica = strlen(direccion_logica) + 1;
                
                // send(socket_cpu, &solicitud_traduccion, sizeof(solicitud_traduccion),0);
                // send(socket_cpu, direccion_logica,sizeof(tam_dirLogica),0);
                
                //recibimos la dirección traducida
                //int size_dirFisica;
                //char* direccion_fisica = recibir_buffer(size_dirFisica,socket_cpu);
                
                
                /* LEER EL BLOQUE FISICO*/
                
                
                // solicitar_lectura(direccion_fisica, socket_memory_stick); //TODO: FALTA IMPLEMENTARRRRR
                enviar_mensaje("LECTURA OK!", socket_ks);

                log_info(log_km,"## PID: <%d> - <Escritura/Lectura> - Dir. Física: <%s> - Tamaño: <TAMAÑO>",pid, direccion_fisica);

            }
                break;
            

            case STDIN_ESCRITURA:
            { //por pedido del ks
                t_list* pedido_escritura = recibir_paquete(socket_ks); //esperamos (PID), dirFisica y tamaño a ESCRIBIR
                pid_t pid = *(pid_t*)list_get(pedido_escritura, 0);
                //char* direccion_logica = list_get(direccion_logica, 1);
                char* direccion_fisica = list_get(pedido_escritura, 1);
                void* bytes_a_escribir = list_get(pedido_escritura,2); 

                //char* direccion_fisica; //entrada --> magia -->traduccion
                
                /* AÚN MÁS MAGIA */
              //  solicitar_escritura(direccion_fisica,socket_memory_stick);

                enviar_mensaje("ESCRITURA OK!", socket_ks);

                log_info(log_km,"## PID: <%d> - <Escritura/Lectura> - Dir. Física: <%s> - Tamaño: <TAMAÑO>", pid, direccion_fisica);
                
                /*t_paquete* paquete_lectura = crear_paquete(RTA_STDOUT_LECTURA);
                
                // retorna los bytes leidos que despues ks envia a IO

                enviar_paquete_send_all(paquete_lectura, socket_ks); */
            }
                break;


            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

void* handle_cpu(void* arg) {    
    int socket_cpu = *(int*)arg;

    int id_cpu;
    recv(socket_cpu, &id_cpu, sizeof(int),0);
    
    log_info(log_km, "CPU <%d> Conectada", id_cpu);

    while(true) {   
        //int codigo_operacion = recibir_operacion(socket_cpu);
        op_code codigo_operacion = recibir_operacion(socket_cpu);

        switch(codigo_operacion) {
            case ERROR:
                log_error(log_km, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
                close(socket_cpu);
                return NULL;
            
            case ENVIAR_CONTEXTO:{
                pid_t pid_recibido;
                
                recv(socket_cpu, &pid_recibido, sizeof(pid_recibido), MSG_WAITALL);

                char *str_pid_enviado = string_itoa(pid_recibido);
                t_contextoEjec* contexto_a_enviar = dictionary_get(diccionario_contextos_ejecucion, str_pid_enviado);
                if(contexto_a_enviar == NULL){
                    log_error(log_km,"No hay un contexto guardado para el PID <%d>", pid_recibido);
                    free(str_pid_enviado);
                    free(contexto_a_enviar);
                    //return EXIT_FAILURE;
                }else{
                    t_paquete *paquete_contexto = crear_paquete(ENVIAR_CONTEXTO);
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->pid),sizeof(pid_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->pc), sizeof(uint32_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->ax), sizeof(uint8_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->bx), sizeof(uint8_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->cx), sizeof(uint8_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->dx), sizeof(uint8_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->eax), sizeof(uint32_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->ebx), sizeof(uint32_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->ecx), sizeof(uint32_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->edx), sizeof(uint32_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->si), sizeof(uint32_t));
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->di), sizeof(uint32_t));

                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->tabla_segmentos),sizeof(&(contexto_a_enviar->tabla_segmentos)));
                    enviar_paquete_send_all(paquete_contexto,socket_cpu);
                    eliminar_paquete(paquete_contexto);

                    }

                    free(str_pid_enviado);
            break;
            }


            case RECIBIR_CONTEXTO: {
            
                t_list* contexto_cpu = recibir_paquete(socket_cpu);
                pid_t pid = *(pid_t*)list_get(contexto_cpu, 0);
                char* str_pid_recibido = string_itoa(pid);
                t_contextoEjec* contexto = dictionary_get(diccionario_contextos_ejecucion, str_pid_recibido);

                // caso de error si se intenta guardar el pcb de un proceso no creado
                if (contexto == NULL) {
                    
                    log_info(log_km, "El proceso con id: <%d> nunca fue creado en Kernel Memory", contexto->pid);
                
                    //liberamos porque no usamos key nueva, era una ya existente
                    free(str_pid_recibido);
                    free(contexto);
                    //return EXIT_FAILURE;
                    return NULL; 
                }
                
                free(str_pid_recibido);
                
                contexto->pc = *(uint32_t*)list_get(contexto_cpu, 1);
                contexto->ax = *(uint8_t*)list_get(contexto_cpu, 2);
                contexto->bx = *(uint8_t*)list_get(contexto_cpu, 3);
                contexto->cx = *(uint8_t*)list_get(contexto_cpu, 4);
                contexto->dx = *(uint8_t*)list_get(contexto_cpu, 5);
                contexto->eax = *(uint32_t*)list_get(contexto_cpu, 6);
                contexto->ebx = *(uint32_t*)list_get(contexto_cpu, 7);
                contexto->ecx = *(uint32_t*)list_get(contexto_cpu, 8);
                contexto->edx = *(uint32_t*)list_get(contexto_cpu, 9);
                contexto->si = *(uint32_t*)list_get(contexto_cpu, 10);
                contexto->di = *(uint32_t*)list_get(contexto_cpu, 11);

                log_info(log_km, "PID <%d> y contexto nuevo guardado correctamente", contexto->pid);
                list_destroy_and_destroy_elements(contexto_cpu, free);
            break;
            }
            
            case PEDIDO_INSTRUCCION: {
                t_list* instruccion_pedida = recibir_paquete(socket_cpu);
                pid_t pid_recibido = *(pid_t*)list_get(instruccion_pedida, 0);
                uint32_t pc = *(int*)list_get(instruccion_pedida,1);
                
                char* str_pid_recibido = string_itoa(pid_recibido);
                //Deberíamos tener algo más o menos así.

                t_list* instrucciones = dictionary_get(diccionario_paths, str_pid_recibido);
                
                char* instruccion = obtener_instruccion(instrucciones,pc);
                int tam_instruccion = strlen(instruccion) + 1;

                char** instruccion_separada = string_split(instruccion, " ");
                char* argumentos = armar_argumentos(instruccion_separada);

                if(strlen(argumentos) > 0){
                log_info(log_km, "## PID: <%d> - Obtener instrucción: <%d> - Instrucción: <%s> <%s>", pid_recibido, pc, instruccion, argumentos);
                } else {                
                    //maneja caso exit, noop
                    log_info(log_km, "## PID: <%d> - Obtener instrucción: <%d> - Instrucción: <%s>", pid_recibido, pc, instruccion);
                }

                
                op_code rta_pedido_instruccion = RTA_PEDIDO_INSTRUCCION;
                send(socket_cpu, &rta_pedido_instruccion, sizeof(op_code), 0);
                log_debug(log_km, "envio codigo RTA_PEDIDO_INSTRUCCION");

                send(socket_cpu, &tam_instruccion, sizeof(int), 0);
                log_debug(log_km, "envio TAMANIO DE INSTRUCCION");

                send(socket_cpu, instruccion, tam_instruccion, 0);
                log_debug(log_km, "envio INSTRUCCION");



                //buscar_instruccion(str_pid_recibido, pc);
                break;
            }


    
            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
            
            break;
        }
    }
}


////////////////////////////////////////


void* handle_ms(void* arg){

    int socket_ms = *(int*)arg;
    int id_ms;
    recv(socket_ms, &id_ms, sizeof(int),0);
    
    log_info(log_km, "Memory Stick <ID> se ha conectado!");

    while(true) {   
        op_code codigo_operacion = recibir_operacion(socket_ms);
        switch(codigo_operacion){
            
            case CONECTAR_MS:
            {

                contador_sticks += 1;
                
                int tamanio_stick;
                recv(socket_ms, &tamanio_stick, sizeof(int),0);

                //tamanio_stick =
                t_memory_stick* nuevo_memory_stick = malloc(sizeof(t_memory_stick));

                nuevo_memory_stick->id = contador_sticks;
                nuevo_memory_stick->tamanio = tamanio_stick;
                
                posicion_memoria += tamanio_stick + 1;

    
                list_add(sticks_conectados, nuevo_memory_stick);

                break;
        
            }
            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
            break;

        }
    }
}


t_list* cargar_instrucciones(char* path_instrucciones) {
    FILE* archivo = fopen(path_instrucciones, "r");

    if (archivo == NULL) {
        log_error(log_km, "No se pudo abrir el archivo de instrucciones: %s", path_instrucciones);
        return NULL;
    }
    t_list* instrucciones = list_create();

        char buffer[256];
        

        /* while (fgets(buffer, sizeof(buffer), archivo) != NULL) {
            // Sacamos el \n final si existe
            char* instruccion = string_trim(buffer);
            // Guardamos una copia de la instrucción en la lista
            list_add(instrucciones, instruccion);

        } */

        while (fgets(buffer, sizeof(buffer), archivo) != NULL) {
            char* instruccion = strdup(buffer);

            string_trim(&instruccion);

            list_add(instrucciones, instruccion);
        }

        fclose(archivo);

        return instrucciones;
    }


char* obtener_instruccion(t_list* instrucciones, uint32_t pc_actual) {
    if (pc_actual >= list_size(instrucciones)) {
        log_error(log_km, "PC fuera de rango. PC: %d - Cantidad instrucciones: %d",
                  pc_actual, list_size(instrucciones));
        return NULL;
    }
    return list_get(instrucciones, pc_actual);
}


char* armar_argumentos(char** tokens) {
    char* argumentos = string_new();

    for (int i = 1; tokens[i] != NULL; i++) {
        string_append(&argumentos, tokens[i]);

        if (tokens[i + 1] != NULL) {
            string_append(&argumentos, " ");
        }
    }

    return argumentos;

    free(argumentos);
}
    
//no abrir el archivo cada vez que cpu quiere una instruccion,
//sino que abrirlo una sola vez y tener una lista de instrucciones

// SI, la mmu tiene que realizar la traduccion de direc logica a fisica.