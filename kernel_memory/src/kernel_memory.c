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

        handshake handshake_modulo = recibir_operacion(*socket_cliente);

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
            case -1:
                log_error(log_km, "Kernel Scheduler FD %d se desconectó. Terminando handle", socket_ks);
                return NULL;
            
            case INICIALIZAR_PROCESO:
                t_list *proceso_inicial = recibir_paquete(socket_ks);
                int pid_recibido = *(int*)list_get(proceso_inicial, 0);
                char* path_recibido = (char*)list_get(proceso_inicial, 1);
                char *str_pid_recibido = string_itoa(pid_recibido);

                
                // TODO Inicializar contexto en 0
                
                t_contextoEjec* contexto_proceso_inicial = malloc(sizeof(t_contextoEjec));
                contexto_proceso_inicial->pid = pid_recibido;
                contexto_proceso_inicial->pc = 0;
                contexto_proceso_inicial->ax = 0;
                contexto_proceso_inicial->bx = 0;
                contexto_proceso_inicial->cx = 0;
                contexto_proceso_inicial->dx = 0;                
                contexto_proceso_inicial->eax = 0;
                contexto_proceso_inicial->ebx = 0;
                contexto_proceso_inicial->ecx = 0;
                contexto_proceso_inicial->edx = 0;
                contexto_proceso_inicial->si = 0;
                contexto_proceso_inicial->di = 0;
                contexto_proceso_inicial->tabla_segmentos = list_create();

                log_info(log_km, "Contexto de ejecución del proceso inicial creado!");
                
                dictionary_put(diccionario_contextos_ejecucion, str_pid_recibido, contexto_proceso_inicial);
                dictionary_put(diccionario_paths,str_pid_recibido, strdup(path_recibido));

                log_info(log_km, "Path del archivo de instrucciones guardado!");

                op_code inicializar_proceso_ok = INICIALIZAR_PROCESO_OK;
                send(socket_ks, &inicializar_proceso_ok, sizeof(inicializar_proceso_ok),0);
                
                list_destroy_and_destroy_elements(proceso_inicial, free);

                break;
            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

void* handle_cpu(void* arg) {    
    int socket_cpu = *(int*)arg;
    
    log_info(log_km, "CPU <CPU_ID> Conectada");

    while(true) {   
        //int codigo_operacion = recibir_operacion(socket_cpu);
        op_code codigo_operacion = recibir_operacion(socket_cpu);

        switch(codigo_operacion) {
            case -1:
                log_error(log_km, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
                close(socket_cpu);
                return NULL;
            
            case ENVIAR_CONTEXTO:{
                //TODO: *****recibir pid de la cpu******
                int pid_recibido;
                char *str_pid_enviado = string_itoa(pid_recibido);
                t_contextoEjec* contexto_a_enviar = dictionary_get(diccionario_contextos_ejecucion, str_pid_enviado);
                if(contexto_a_enviar == NULL){
                    log_error(log_km,"No hay un contexto guardado para el PID <%d>", pid_recibido);
                }else{
                    t_paquete *paquete_contexto = crear_paquete(ENVIAR_CONTEXTO);
                    agregar_a_paquete(paquete_contexto,&(contexto_a_enviar->pid),sizeof(int));
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
                int pid = *(int*)list_get(contexto_cpu, 0);
                char* str_pid_recibido = string_itoa(pid);
                t_contextoEjec* contexto = dictionary_get(diccionario_contextos_ejecucion, str_pid_recibido);

                if (contexto == NULL) {
                    //si es proceso nuevo
                    contexto = malloc(sizeof(t_contextoEjec));
                    contexto->pid = pid;
                    contexto->tabla_segmentos = list_create(); 

                    dictionary_put(diccionario_contextos_ejecucion, str_pid_recibido, contexto);
                    log_info(log_km, "PID <%d> nuevo guardado correctamente!", contexto->pid);

                } else {
                    //liberamos porque no usamos key nueva, era una ya existente
                    free(str_pid_recibido);
                }
                
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
                int pid_recibido = *(int*)list_get(instruccion_pedida, 0);
                uint32_t pc = *(int*)list_get(instruccion_pedida,1);
                
                char* str_pid_recibido = string_itoa(pid_recibido);
                //Deberíamos tener algo más o menos así.
                buscar_instruccion(str_pid_recibido, pc);
                break;
            }
    
            default:
                log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
            
            break;
        }

    }

}

void* handle_ms(void* arg){

    int socket_ms = *(int*)arg;
    
    log_info(log_km, "Memory Stick <ID> se ha conectado!");

    while(true) {   
        op_code codigo_operacion = recibir_operacion(socket_ms);
        switch(codigo_operacion){
            
            case CONECTAR_MS:
            {

                contador_sticks += 1;
                
                int tamanio_stick;
                recv(socket_ms, (void*)tamanio_stick, sizeof(int),0);

                //tamanio_stick =
                t_memory_stick* nuevo_memory_stick = malloc(sizeof(t_memory_stick));

                nuevo_memory_stick->id = contador_sticks;
                nuevo_memory_stick->tamanio = tamanio_stick;
                
                posicion_memoria += tamanio_stick + 1;

    
                list_add(sticks_conectados, nuevo_memory_stick);

                break;
        
            }

        }
    }
}

void buscar_instruccion(char* str_pid_recibido, uint32_t pc_nuevo) {
        
    t_contextoEjec* contexto = dictionary_get (diccionario_contextos_ejecucion , str_pid_recibido);
    uint32_t pc_actual = contexto -> pc;

    
    char* path_instrucciones = dictionary_get(diccionario_paths, str_pid_recibido);

    if(path_instrucciones == NULL){
        log_error(log_km, "No se encontró un path de instrucciones asociado al PID <%d>", str_pid_recibido);
    }else{
        char* instruccion = buscar_en_archivo(path_instrucciones,pc_actual); //TODO: implementar esta funcion
        if (instruccion == NULL){
            log_error(log_km, "No se encontró la instrucción %d para el PID <%d>", instruccion, str_pid_recibido);
        } else {
           /*  t_paquete* rta_instruccion = crear_paquete(RTA_PEDIDO_INSTRUCCION);
            agregar_a_paquete(rta_instruccion,instruccion,strlen(instruccion)+1);
            enviar_paquete_send_all(rta_instruccion,socket_cpu); // sugerir a gabi q reciba como paquete
            eliminar_paquete(rta_instruccion); */
            
            //envío sin paquete
            op_code codigo = RTA_PEDIDO_INSTRUCCION;
            send(socket_cpu, &codigo,sizeof(op_code),0);

            // verificar si recibe esto ---------------------- !!!
            int tamanio_instr = strlen(instruccion) + 1;
            //send(socket_cpu, &tamanio_instr, sizeof(int), 0);

            send(socket_cpu, instruccion, tamanio_instr,0);

        }
    }  
    
    if (path_instrucciones == NULL) {
        log_error(log_km, "No se pudo abrir el archivo");
        return;
    }

    contexto->pc = pc_nuevo + 1;

    dictionary_put(diccionario_contextos_ejecucion,str_pid_recibido, contexto);


}
        
    
char* buscar_en_archivo(char* path_instrucciones,uint32_t pc_actual){
    
    FILE* archivo = fopen(path_instrucciones, "r");

    if (archivo == NULL) {
        log_error(log_km, "No se pudo abrir el archivo de instrucciones");
        return NULL;
    }

    char buffer[256];

    uint32_t linea_actual = 0;

    // avanzar hasta la linea del PC
    while (linea_actual < pc_actual) {

        if (fgets(buffer, sizeof(buffer), archivo) == NULL) {
            fclose(archivo);
            return NULL;
        }

        linea_actual++;
    }

    // leer la instruccion buscada
    if (fgets(buffer, sizeof(buffer), archivo) == NULL) {
        fclose(archivo);
        return NULL;
    }

    fclose(archivo);

    return strdup(buffer);
}
