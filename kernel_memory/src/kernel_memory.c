#include "kernel_memory.h"
// #include "globales.c" idea a desarrollar despues
t_config_km config_km;

// t_log* log_km;
int posicion_memoria = 0;
int contador_sticks = 0;
static t_list *segmentos_para_compactar = NULL;

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        perror("No se proporcionaron todos los argumentos [Archivo Config]");
        return EXIT_FAILURE;
    }

    char *config_path = argv[1];
//    char *str_log_level;
//    char *puerto_km;
//    char *scripts_basepath;
    t_config *config = config_create(config_path);
    pthread_t hilo_escucha;

    if (config == NULL)
    {
        perror("Error al intentar cargar el config");
        return EXIT_FAILURE;
    }

    // puerto_km = config_get_string_value(config,"PUERTO");
    // str_log_level = config_get_string_value(config, "LOG_LEVEL");
    // scripts_basepath = config_get_string_value("SCRIPTS_BASEPATH");
    config_km.puerto = config_get_string_value(config, "PUERTO");
    config_km.log_level = config_get_string_value(config, "LOG_LEVEL");
    config_km.segment_max_size = config_get_int_value(config, "SEGMENT_MAX_SIZE");
    config_km.alloc_strategy = config_get_string_value(config, "ALLOCATION_STRATEGY");
    config_km.instr_delay = config_get_int_value(config, "INSTRUCTION_DELAY");
    config_km.compaction_delay = config_get_int_value(config, "COMPACTION_DELAY");
    config_km.scripts_basepath = config_get_string_value(config, "SCRIPTS_BASEPATH");

    // if(puerto_km == NULL || str_log_level == NULL){
    //     config_destroy(config);
    //     return EXIT_FAILURE;
    // }

    if (config_km.puerto == NULL || config_km.log_level == NULL)
    {
        config_destroy(config);
        return EXIT_FAILURE;
    }

    int log_level = log_level_from_string(config_km.log_level);
    log_km = log_create("memory.log", "MEMORY", true, log_level);

    // server_km = iniciar_servidor(log_km, puerto_km);
    server_km = iniciar_servidor(log_km, config_km.puerto);
    if (server_km == -1)
    {
        log_error(log_km, "Error al iniciar el servidor en el puerto %s", config_km.puerto);
        log_destroy(log_km);
        config_destroy(config);
        return EXIT_FAILURE;
    }
    log_info(log_km, "Kernel Memory iniciado");

    // inicializamos estructuras
    diccionario_contextos_ejecucion = dictionary_create();
    diccionario_instrucciones = dictionary_create();
    sticks_conectados = list_create();
    huecos = list_create();

    // TODO duda: malloc para el socket para que no se pisen?;
    pthread_create(&hilo_escucha, NULL, handle_connections, NULL);

    // dejar los pthread_join al final del main para que no bloqueen el main
    pthread_join(hilo_escucha, NULL); // Sirve para esperar a que un hilo termine su ejecución

    log_destroy(log_km);
    config_destroy(config);

    return 0;
}

void *handle_connections(void *arg)
{
    while (1)
    {
        int client_fd = esperar_cliente(server_km, log_km);

        int *socket_cliente = malloc(sizeof(int));
        *socket_cliente = client_fd;

        op_code handshake_modulo = recibir_operacion(*socket_cliente);

        switch (handshake_modulo)
        {
        case HANDSHAKE_CPU:
            pthread_t hilo_cpu;
            pthread_create(&hilo_cpu, NULL, handle_cpu, socket_cliente);
            pthread_detach(hilo_cpu);
            break;
        case HANDSHAKE_KS: //TODO: REVISAR HILOS DE KS?????? ---------------------------
            pthread_t hilo_ks;
            pthread_create(&hilo_ks, NULL, handle_ks, socket_cliente);
            pthread_detach(hilo_ks);
            // TODO IMPLEMENTAR
            break;
        case HANDSHAKE_MS:
            log_info(log_km, "Memory Stick Conectado. Guardando socket.");
            // Guardamos el socket de MS de forma global y NO CREAMOS HILO, ya que no es necesariamente asincrono.
            // handle_ms();
            pthread_t hilo_ms;
            pthread_create(&hilo_ms, NULL, handle_ms,socket_cliente);
            //socket_memory_stick = *socket_cliente;
            //free(socket_cliente);
            break;
        case HANDSHAKE_SWAP:
            log_info(log_km, "Swap Conectado. Guardando socket.");
            // Guardamos el socket de SWAP de forma global y NO CREAMOS HILO, ya que no es necesariamente asincrono.
            socket_swap = *socket_cliente;
            free(socket_cliente);
            break;
        default:
            log_warning(log_km, "Handshake no reconocido: %d", handshake_modulo);
            // close(socket);
            break;
        }
    }
}

void *handle_ks(void *arg)
{
    int socket_ks = *(int *)arg;

    log_info(log_km, "## Kernel Scheduler Conectado - FD del socket: %d", socket_ks);

    while (true)
    {
        int codigo_operacion = recibir_operacion(socket_ks);

        switch (codigo_operacion)
        {
        case ERROR:
            log_error(log_km, "Kernel Scheduler FD %d se desconectó. Terminando handle", socket_ks);
            return NULL;

        case INICIALIZAR_PROCESO:
            log_debug(log_km, "Kernel Scheduler solicitó INICIALIZAR_PROCESO");
            char *base_path = config_km.scripts_basepath; // hardcodeado por ahora

            t_list *proceso_inicial = recibir_paquete(socket_ks);
            pid_t pid_recibido = *(pid_t *)list_get(proceso_inicial, 0);
            char *path_instrucciones_recibido = (char *)list_get(proceso_inicial, 1);
            char *str_pid_recibido = string_itoa(pid_recibido);

            char *path_absoluto = string_from_format("%s%s", base_path, path_instrucciones_recibido);
            log_debug(log_km, "Intentando cargar las instrucciones desde %s", path_instrucciones_recibido);

            t_list *instrucciones = cargar_instrucciones(path_absoluto);
            free(path_absoluto);
            list_destroy_and_destroy_elements(proceso_inicial, free);

            if (instrucciones == NULL)
            {
                log_error(log_km, "No se logró obtener las instrucciones");
                return NULL;
            }

            dictionary_put(diccionario_instrucciones, str_pid_recibido, instrucciones); // diccionaria con instrucciones x pid
            // log_debug(log_km_debug, "Instrucciones del proceso: <%d> guardadas correctamente", pid_recibido);
            log_debug(log_km, "Instrucciones del proceso: <%d> guardadas correctamente", pid_recibido);

            log_info(log_km, "## PID: <%d> - Proceso Creado", pid_recibido);

            t_contextoEjec *contexto_proceso = malloc(sizeof(t_contextoEjec));
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

            // log_debug(log_km_debug, "Contexto de ejecución del proceso <%s> creado!", str_pid_recibido);
            log_info(log_km, "Contexto de ejecución del proceso <%s> creado!", str_pid_recibido);

            op_code inicializar_proceso_ok = INICIALIZAR_PROCESO_OK;
            send(socket_ks, &inicializar_proceso_ok, sizeof(inicializar_proceso_ok), 0);

            break;

        case STDOUT_LECTURA:
        {
            // por pedido del ks
            t_list *pedido_lectura = recibir_paquete(socket_ks); // esperamos (PID), dirFisica y tamaño a leer
            pid_t pid = *(pid_t *)list_get(pedido_lectura, 0);
            uint32_t direccion_fisica = *(uint32_t*)list_get(pedido_lectura, 1);
            uint32_t cantidad_bytes = *(uint32_t*)list_get(pedido_lectura, 2);

            if(!acceso_valido_en_segmentos(pid,direccion_fisica,cantidad_bytes)){
               log_error(log_km, "No es un segmento valido/el proceso no tiene acceso a este segmento");
               op_code respuesta_lectura = ERROR_ESCRITURA;
               send(socket_ks, &respuesta_lectura, sizeof(respuesta_lectura), 0);

               list_destroy_and_destroy_elements(pedido_lectura, free);
               break;
            }

            t_paquete *solicitud_lectura = crear_paquete(SOLICITUD_LECTURA);

            agregar_a_paquete(solicitud_lectura, &direccion_fisica, sizeof(uint32_t));
            agregar_a_paquete(solicitud_lectura, &cantidad_bytes, sizeof(cantidad_bytes));

            enviar_paquete_send_all(solicitud_lectura, socket_memory_stick);

            eliminar_paquete(solicitud_lectura);

            op_code respuesta_lectura;
            int recibidos = recv(socket_memory_stick, &respuesta_lectura, sizeof(respuesta_lectura), MSG_WAITALL);
            if (recibidos != sizeof(respuesta_lectura))
            {
                log_error(log_km, "NO recibimos respuesta del Memory Stick");
                list_destroy_and_destroy_elements(pedido_lectura, free);
                break;
            }

            if (respuesta_lectura == LECTURA_OK)
            {
                void *contenido_leido = malloc(cantidad_bytes);

                if (contenido_leido == NULL)
                {
                    log_error(log_km, "No se pudo reservar memoria para la lectura");
                    list_destroy_and_destroy_elements(pedido_lectura, free);
                    break;
                }

                int bytes_leidos = recv(socket_memory_stick, contenido_leido, cantidad_bytes, MSG_WAITALL);

                if (bytes_leidos != cantidad_bytes)
                {
                    log_error(log_km, "Se esperaban <%d> bytes pero se recibieron <%d>", cantidad_bytes, bytes_leidos);

                    free(contenido_leido);
                    list_destroy_and_destroy_elements(pedido_lectura, free);

                    break;
                }
                t_paquete* enviar_respuesta_lectura = crear_paquete(respuesta_lectura);
                agregar_a_paquete(enviar_respuesta_lectura,contenido_leido,sizeof(contenido_leido));
                // send(socket_ks, &respuesta_lectura, sizeof(respuesta_lectura), 0);
                // send(socket_ks, contenido_leido, cantidad_bytes, 0);
                log_info(log_km, "## PID: %d - Lectura - Dir. Física: %u - Tamaño: %d", pid, direccion_fisica, cantidad_bytes);
                free(contenido_leido);
            }
            else
            {
                log_error(log_km, "Ocurrio un error: No se pudo leer el bloque.");
                printf("soy el print del error de leer de bloque");
                // TODO: avisar a team KS que tienen que manejar el caso de error

                send(socket_ks, &respuesta_lectura, sizeof(respuesta_lectura), 0);
            }

            list_destroy_and_destroy_elements(pedido_lectura, free);
        }
        break;

        case STDIN_ESCRITURA:
        {                                                          // por pedido del ks TODO: CHEQUEAR CON TEAM SCHEDULER y chequear que gabi este mandando los pedidos de A UNO.
            t_list *pedido_escritura = recibir_paquete(socket_ks); // esperamos (PID), dirFisica y tamaño a ESCRIBIR
            pid_t pid = *(pid_t *)list_get(pedido_escritura, 0);
            uint32_t direccion_fisica = *(uint32_t *)list_get(pedido_escritura, 1);
            uint32_t cantidad_bytes = *(uint32_t *)list_get(pedido_escritura, 2); // estanusando uint para el tamanio (lpm)
                                                                        //  e ok, es actualizar los tipos
            char *contenido_a_escribir = (char *)list_get(pedido_escritura, 3);

                if(!acceso_valido_en_segmentos(pid,direccion_fisica,cantidad_bytes)){
                    log_error(log_km, "No es un segmento valido/el proceso no tiene acceso a este segmento");
                    op_code respuesta_escritura = ERROR_ESCRITURA;
                    send(socket_ks, &respuesta_escritura, sizeof(respuesta_escritura), 0);

                    list_destroy_and_destroy_elements(pedido_escritura, free);
                    //return EXIT_FAILURE;
                    break;

                }

                t_paquete* solicitud_escritura = crear_paquete(SOLICITUD_ESCRITURA);
                
                agregar_a_paquete(solicitud_escritura, &direccion_fisica, sizeof(uint32_t));
                agregar_a_paquete(solicitud_escritura, &cantidad_bytes,sizeof(int));
                agregar_a_paquete(solicitud_escritura, contenido_a_escribir, cantidad_bytes);

                enviar_paquete_send_all(solicitud_escritura,socket_memory_stick);

                eliminar_paquete(solicitud_escritura);

                op_code respuesta_escritura;

                int recibidos = recv(socket_memory_stick,&respuesta_escritura,sizeof(respuesta_escritura),MSG_WAITALL);
                
                if(recibidos != sizeof(respuesta_escritura)){
                log_error(log_km, "NO recibimos respuesta del Memory Stick");
                list_destroy_and_destroy_elements(pedido_escritura, free);
                break;
                }

                if(respuesta_escritura == ESCRITURA_OK){
                send(socket_ks, &respuesta_escritura, sizeof(respuesta_escritura), 0);
                log_info(log_km, "## PID: <%d> - <Escritura> - Dir. Física: <%u> - Tamaño: <%d>", pid, direccion_fisica, cantidad_bytes);

                }else{
                log_error(log_km, "Ocurrio un error: No se pudo escribir el bloque.");
                printf("soy el print del error de escritura de bloque");
                // TODO: avisar a team KS que tienen que manejar el caso de error

                send(socket_ks, &respuesta_escritura, sizeof(respuesta_escritura), 0); 
                }

                list_destroy_and_destroy_elements(pedido_escritura, free);
        }
        break;

        case CREACION_DE_SEGMENTO:
        {

            t_list *pedido_creacion_seg = recibir_paquete(socket_ks);
            pid_t pid = *(pid_t *)list_get(pedido_creacion_seg, 0);
            int id_segmento = *(int *)list_get(pedido_creacion_seg, 1);
            int tamanio_segmento = *(int *)list_get(pedido_creacion_seg, 2);
            char *pid_string = string_itoa(pid);

            iniciar_creacion_segmento(pid, id_segmento, tamanio_segmento, socket_ks);

            op_code creacion_ok = CREACION_DE_SEGMENTO_OK;
            send(socket_ks,&creacion_ok, sizeof(creacion_ok),0);
            log_info(log_km, "## PID: %s - Segmento Creado %d - Tamaño: %d", pid, id_segmento, tamanio_segmento);

           
            free(pid_string);
            list_destroy_and_destroy_elements(pedido_creacion_seg, free);
        }
            break;

        case DESUSPENDER_PROCESO:
        {
            pid_t pid;
            recv(socket_ks, &pid, sizeof(pid_t), 0);

            char* pid_str = string_itoa(pid);
            t_list* segmentos_del_proceso_suspendido = dictionary_get(diccionario_procesos_suspendidos, pid_str);
            
            free(pid_str);

            
            if (segmentos_del_proceso_suspendido == NULL) {
                log_error(log_km, "Error al desuspender PID %d", pid);
                break;
            }

            /////////////////////////////////////////////////

            
            for (int i = 0; i < list_size(segmentos_del_proceso_suspendido); i++){
                
                //traemos la metadata
                t_metadata_swap* contexto_suspendido = list_get(segmentos_del_proceso_suspendido, i);
                
                //
                t_segmento* segmento_restaurado = iniciar_creacion_segmento(
                    contexto_suspendido->pid,
                    contexto_suspendido->id_segmento, //id_segmento,
                    contexto_suspendido->limite, //tamanio,
                    socket_ks
                );

                //void* contenido = leer_bloque_swap(contexto_suspendido->bloque_swap);

                char* contenido = obtener_contenido_bloque_swap(contexto_suspendido->bloque_swap);

                escribir_segmento_en_ms(segmento_restaurado, contenido);
            }
        }
            break;

        case SUSPENDER_PROCESO:
        {

            pid_t pid;
            recv(socket_ks, &pid, sizeof(pid_t), MSG_WAITALL);

            //1. BUscar en diccionario el pid
            char* pid_str = string_itoa(pid);
            t_contextoEjec* contexto = dictionary_get(diccionario_contextos_ejecucion,pid_str);
            free(pid_str);

            if (contexto == NULL) {
                log_error(log_km, "No se encontró el contexto para suspender al PID %d", pid);
                break;
            }

            t_list* segmentos = contexto->tabla_segmentos;
            //t_list* direcciones_logicas = list_create();
            t_segmento* segmento;
            void* contenido;
            t_list* backup_metadata = list_create();
            
            for (int i = 0; i < list_size(segmentos); i++){
                
                segmento = list_get(segmentos, i);
                
                //void* buffer_contenido = malloc(segmento->limite); //limite = tamanio del segmento !!!!!!!!!!!
                contenido = obtener_contenido_segmento(segmento);
                if(contenido == NULL){
                    log_error(log_km, "NO se pudo obtener el segmento <%d> del PID <%d>",segmento->id_segmento, pid);
                    break;
                }
                //encontrar_stick_por_base_del_segmento(segmento->base); --> ahora lo hace obtener_contenido_segmento()
               //obtener_contenido_segmento(segmento);
                uint32_t bloque_en_swap = enviar_segmento_a_swap(pid, segmento, contenido);
                free(contenido);

                //guardo metadata del segmento --> puede ir todo en enviar_segmento_a_swap() ----???
                t_metadata_swap* backup = malloc(sizeof(t_metadata_swap));
                backup->id_segmento = segmento->id_segmento;
                backup->limite = segmento->limite;
                backup->pid = pid;
                backup->bloque_swap = bloque_en_swap;

                list_add(backup_metadata,backup);
            }

            dictionary_put(diccionario_procesos_suspendidos, pid_str,backup_metadata);

           

            //como le seteo el bloque?

            //ahora que se esta guardado, limpiamos la tabla de segmentos

            for(int i = 0; list_size(segmentos); i++){
                t_segmento* segmento_aux = list_get(segmentos,i);

                // TODO: PENSAR --> funcion para liberar segmentos? liberar manualmente con free?
            } 

            list_clean_and_destroy_elements(segmentos, free);

            log_debug(log_km, "Proceso suspendido correctamente. PID <%d>: ",pid);

        }
            break;
        default:
            log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
            break;
        }
    }
}

void *handle_cpu(void *arg)
{
    int socket_cpu = *(int *)arg;

    int id_cpu;
    recv(socket_cpu, &id_cpu, sizeof(int), 0);

    log_info(log_km, "## CPU <%d> Conectada", id_cpu);

    while (true)
    {
        // int codigo_operacion = recibir_operacion(socket_cpu);
        op_code codigo_operacion = recibir_operacion(socket_cpu);

        switch (codigo_operacion)
        {
        case ERROR:
            log_error(log_km, "CPU FD %d se desconectó. Terminando handle", socket_cpu);
            close(socket_cpu);
            return NULL;

        case ENVIAR_CONTEXTO:
        {
            pid_t pid_recibido;

            recv(socket_cpu, &pid_recibido, sizeof(pid_recibido), MSG_WAITALL);

            char *str_pid_enviado = string_itoa(pid_recibido);
            t_contextoEjec *contexto_a_enviar = dictionary_get(diccionario_contextos_ejecucion, str_pid_enviado);
            if (contexto_a_enviar == NULL)
            {
                log_error(log_km, "No hay un contexto guardado para el PID <%d>", pid_recibido);
                free(str_pid_enviado);
                free(contexto_a_enviar);
                // return EXIT_FAILURE;
            }
            else
            {
                t_paquete *paquete_contexto = crear_paquete(ENVIAR_CONTEXTO);
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->pid), sizeof(pid_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->pc), sizeof(uint32_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->ax), sizeof(uint8_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->bx), sizeof(uint8_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->cx), sizeof(uint8_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->dx), sizeof(uint8_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->eax), sizeof(uint32_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->ebx), sizeof(uint32_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->ecx), sizeof(uint32_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->edx), sizeof(uint32_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->si), sizeof(uint32_t));
                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->di), sizeof(uint32_t));

                agregar_a_paquete(paquete_contexto, &(contexto_a_enviar->tabla_segmentos), sizeof(&(contexto_a_enviar->tabla_segmentos)));
                enviar_paquete_send_all(paquete_contexto, socket_cpu);
                eliminar_paquete(paquete_contexto);
            }

            free(str_pid_enviado);
            break;
        }

        case RECIBIR_CONTEXTO:
        {

            t_list *contexto_cpu = recibir_paquete(socket_cpu);
            pid_t pid = *(pid_t *)list_get(contexto_cpu, 0);
            char *str_pid_recibido = string_itoa(pid);
            t_contextoEjec *contexto = dictionary_get(diccionario_contextos_ejecucion, str_pid_recibido);

            // caso de error si se intenta guardar el pcb de un proceso no creado
            if (contexto == NULL)
            {

                log_info(log_km, "El proceso con id: <%d> nunca fue creado en Kernel Memory", pid);

                // liberamos porque no usamos key nueva, era una ya existente
                free(str_pid_recibido);
                // return EXIT_FAILURE;

                free(str_pid_recibido);
                list_destroy_and_destroy_elements(contexto_cpu, free);
                return NULL;
            }

            free(str_pid_recibido);

            contexto->pc = *(uint32_t *)list_get(contexto_cpu, 1);
            contexto->ax = *(uint8_t *)list_get(contexto_cpu, 2);
            contexto->bx = *(uint8_t *)list_get(contexto_cpu, 3);
            contexto->cx = *(uint8_t *)list_get(contexto_cpu, 4);
            contexto->dx = *(uint8_t *)list_get(contexto_cpu, 5);
            contexto->eax = *(uint32_t *)list_get(contexto_cpu, 6);
            contexto->ebx = *(uint32_t *)list_get(contexto_cpu, 7);
            contexto->ecx = *(uint32_t *)list_get(contexto_cpu, 8);
            contexto->edx = *(uint32_t *)list_get(contexto_cpu, 9);
            contexto->si = *(uint32_t *)list_get(contexto_cpu, 10);
            contexto->di = *(uint32_t *)list_get(contexto_cpu, 11);

            log_info(log_km, "PID <%d> y contexto nuevo guardado correctamente", contexto->pid);
            list_destroy_and_destroy_elements(contexto_cpu, free);
            break;
        }

        case PEDIDO_INSTRUCCION:
        {
            t_list *instruccion_pedida = recibir_paquete(socket_cpu);
            pid_t pid_recibido = *(pid_t *)list_get(instruccion_pedida, 0);
            uint32_t pc = *(uint32_t *)list_get(instruccion_pedida, 1);

            char *str_pid_recibido = string_itoa(pid_recibido);
            // Deberíamos tener algo más o menos así.

            t_list *instrucciones = dictionary_get(diccionario_instrucciones, str_pid_recibido);

            char *instruccion = obtener_instruccion(instrucciones, pc);
            int tam_instruccion = strlen(instruccion) + 1;

            char **instruccion_separada = string_split(instruccion, " ");
            char *argumentos = armar_argumentos(instruccion_separada);

            if (strlen(argumentos) > 0)
            {
                log_info(log_km, "## PID: <%d> - Obtener instrucción: <%d> - Instrucción: <%s> <%s>", pid_recibido, pc, instruccion, argumentos);
            }
            else
            {
                // maneja caso exit, noop
                log_info(log_km, "## PID: <%d> - Obtener instrucción: <%d> - Instrucción: <%s>", pid_recibido, pc, instruccion);
            }

            op_code rta_pedido_instruccion = RTA_PEDIDO_INSTRUCCION;
            send(socket_cpu, &rta_pedido_instruccion, sizeof(op_code), 0);
            log_debug(log_km, "envio codigo RTA_PEDIDO_INSTRUCCION");

            send(socket_cpu, &tam_instruccion, sizeof(int), 0);
            log_debug(log_km, "envio TAMANIO DE INSTRUCCION");

            send(socket_cpu, instruccion, tam_instruccion, 0);
            log_debug(log_km, "envio INSTRUCCION");

            // buscar_instruccion(str_pid_recibido, pc);
            break;
        }

        default:
            log_warning(log_km, "operacion desconocida: %d", codigo_operacion);

            break;
        }
    }
}

////////////////////////////////////////

void *handle_ms(void *arg)
{

    int socket_ms = *(int *)arg;
    int id_ms;
    int tamanio_ms;
    recv(socket_ms, &id_ms, sizeof(int), 0);
    recv(socket_ms, &tamanio_ms, sizeof(int), 0);

    log_info(log_km, "## Memory Stick de %d bytes Conectada", tamanio_ms);
    log_debug(log_km, "Se conecta memory stick %d", id_ms);
    while (true)
    {
        op_code codigo_operacion = recibir_operacion(socket_ms);
        switch (codigo_operacion)
        {

        case CONECTAR_MS:
        {
            contador_sticks += 1;

            // creamos el stick nuevo que se agrega
            // malloc para estructura, no para memoria
            t_memory_stick *nuevo_memory_stick = malloc(sizeof(t_memory_stick));
            nuevo_memory_stick->id = contador_sticks;
            nuevo_memory_stick->inicio = posicion_memoria;
            nuevo_memory_stick->tamanio = tamanio_ms;
            nuevo_memory_stick->socket = socket_ms;
            // la primera vez, pos_mem = 0, luego se actualiza con cada MemStick agregado
            // avanzamos +1 para no pisar el ultimo byte
            posicion_memoria += tamanio_ms + 1; // TODO: PENSAR
            list_add(sticks_conectados, nuevo_memory_stick);

            break;
        }

        case DESCONECTAR_MS:
        {
            log_debug(log_km, "## BSOD: BLUE SCREEN OF DEATH");
            // avisar a KS

            break;
        }
        default:
            log_warning(log_km, "operacion desconocida: %d", codigo_operacion);
            break;
        }
    }
}

t_list *cargar_instrucciones(char *path_instrucciones)
{
    FILE *archivo = fopen(path_instrucciones, "r");

    if (archivo == NULL)
    {
        log_error(log_km, "No se pudo abrir el archivo de instrucciones: %s", path_instrucciones);
        return NULL;
    }
    t_list *instrucciones = list_create();

    char buffer[256];

    /* while (fgets(buffer, sizeof(buffer), archivo) != NULL) {
        // Sacamos el \n final si existe
        char* instruccion = string_trim(buffer);
        // Guardamos una copia de la instrucción en la lista
        list_add(instrucciones, instruccion);

    } */

    while (fgets(buffer, sizeof(buffer), archivo) != NULL)
    {
        char *instruccion = strdup(buffer);

        string_trim(&instruccion);

        list_add(instrucciones, instruccion);
    }

    fclose(archivo);

    return instrucciones;
}

char *obtener_instruccion(t_list *instrucciones, uint32_t pc_actual)
{
    if (pc_actual >= list_size(instrucciones))
    {
        log_error(log_km, "PC fuera de rango. PC: %d - Cantidad instrucciones: %d",
                  pc_actual, list_size(instrucciones));
        return NULL;
    }
    return list_get(instrucciones, pc_actual);
}

char *armar_argumentos(char **tokens)
{
    char *argumentos = string_new();

    for (int i = 1; tokens[i] != NULL; i++)
    {
        string_append(&argumentos, tokens[i]);

        if (tokens[i + 1] != NULL)
        {
            string_append(&argumentos, " ");
        }
    }

    return argumentos;

    free(argumentos);
}

// no abrir el archivo cada vez que cpu quiere una instruccion,
// sino que abrirlo una sola vez y tener una lista de instrucciones

// SI, la mmu tiene que realizar la traduccion de direc logica a fisica.

// TODO: Desarrollar !!

///////////////////////////////////////////

t_segmento* buscar_best_bloque(char *pid_string, int id_segmento, int tamanio_segmento)
{
    // 2. Buscar el mejor hueco: el más chico donde entre el segmento
    t_hueco_libre *mejor_hueco = NULL;

    for (int i = 0; i < list_size(huecos); i++)
    {
        t_hueco_libre *hueco_actual = list_get(huecos, i);

        if (hueco_actual->tamanio >= tamanio_segmento)
        {
            if (mejor_hueco == NULL || hueco_actual->tamanio < mejor_hueco->tamanio)
            {
                mejor_hueco = hueco_actual;
            }
        }
    }

    // 3. Si no hay hueco contiguo, no puede crear directamente
    if (mejor_hueco == NULL)
    {
        log_warning(log_km,
                    "No hay hueco contiguo disponible para crear el segmento <%d> de tamaño <%d> del PID <%s>",
                    id_segmento,
                    tamanio_segmento,
                    pid_string);

        // TODO:
        // evaluar si hay memoria total suficiente pero no contigua.
        // Si hay memoria total suficiente -> pedir compactación.
        // Si no hay memoria total suficiente -> responder error al Kernel Scheduler.
        return NULL;
    }

    // 4. Si encontró hueco, crea el segmento usando ese hueco
    t_segmento* nuevo_segmento = crear_segmento(mejor_hueco, pid_string, id_segmento, tamanio_segmento);

    return nuevo_segmento;
}

t_segmento* buscar_worst_bloque(char *pid_string, int id_segmento, int tamanio_segmento)
{

    // 2. Encontrar bloque
    t_hueco_libre *peor_hueco = NULL;

    for (int i = 0; i < list_size(huecos); i++)
    {
        t_hueco_libre *hueco_actual = list_get(huecos, i);

        if (hueco_actual->tamanio >= tamanio_segmento)
        {
            if (peor_hueco == NULL || hueco_actual->tamanio > peor_hueco->tamanio)
            {
                peor_hueco = hueco_actual;
            }
        }
    }

    // 3. Llamar a crear_segmento()
    if (peor_hueco == NULL)
    {
        log_warning(log_km,
                    "No hay hueco contiguo disponible para crear el segmento <%d> de tamaño <%d> del PID <%s>",
                    id_segmento,
                    tamanio_segmento,
                    pid_string);

        // TODO:
        // evaluar si hay memoria total suficiente pero no contigua.
        // Si hay memoria total suficiente -> pedir compactacion.
        // Si no hay memoria total suficiente -> responder error al Kernel Scheduler.
        return NULL;
    }

    // 4. Si encontró hueco, crea el segmento usando ese hueco
    t_segmento* nuevo_segmento = crear_segmento(peor_hueco, pid_string, id_segmento, tamanio_segmento);

    return nuevo_segmento;
}

t_segmento* crear_segmento(t_hueco_libre *hueco_hallado, char *pid, uint32_t id_segmento, uint32_t tamanio_segmento)
{
    // 1. Crear segmento
    // 2. Agregar a la lista de segmentos
    t_segmento *nuevo_segmento = malloc(sizeof(t_segmento));
    nuevo_segmento->id_segmento = id_segmento;
    nuevo_segmento->base = hueco_hallado->base;
    nuevo_segmento->limite = tamanio_segmento;

    t_contextoEjec *contexto_ejecucion = dictionary_get(diccionario_contextos_ejecucion, pid);

    if (contexto_ejecucion == NULL){
    log_error(log_km, "No existe contexto para el PID <%s>", pid);
    return NULL;
}

    list_add(contexto_ejecucion->tabla_segmentos, nuevo_segmento);

    ////////// LISTA DE HUECOS

    int nuevoTamanioHuecoLibre = hueco_hallado->tamanio - tamanio_segmento;

    if (nuevoTamanioHuecoLibre == 0)
    { // ya no hay espacio
        list_remove_element(huecos, hueco_hallado);
        free(hueco_hallado);
    }
    else
    {

        // si no ocupo todo el hueco

        hueco_hallado->base += tamanio_segmento;
        hueco_hallado->tamanio = nuevoTamanioHuecoLibre;
        // no lo agrego, ya esta en la lista
    }

    return nuevo_segmento;
}

bool verificar_tamanio_valido(int tamanio_segmento)
{
    if (tamanio_segmento <= 0)
    {
        log_error(log_km, "tamanio invalido (0 o negativo)");
        return false;
    }
    else if (tamanio_segmento > config_km.segment_max_size)
    {
        log_error(log_km, "No se puede crear el segmento: tamaño <%d> supera SEGMENT_MAX_SIZE <%d>",
                  tamanio_segmento,
                  config_km.segment_max_size);
        return false;
    }
    return true;
}

uint32_t calcular_memoria_libre()
{
    uint32_t memoria_libre_total = 0;

    for (int i = 0; i < list_size(huecos); i++)
    {
        t_hueco_libre *hueco = list_get(huecos, i);
        memoria_libre_total += hueco->tamanio;
    }

    return memoria_libre_total;
}

bool existe_hueco_contiguo(uint32_t tamanio_segmento)
{
    for (int i = 0; i < list_size(huecos); i++)
    {
        t_hueco_libre *hueco = list_get(huecos, i);

        if (hueco->tamanio >= tamanio_segmento)
        {
            return true;
        }
    }

    return false;
}

t_segmento* elegir_algoritmo(char *pid_string, int id_segmento, int tamanio_segmento)
{
    t_segmento* nuevo_segmento;
    if (strcmp(config_km.alloc_strategy, "BEST") == 0)
    {
        nuevo_segmento = buscar_best_bloque(pid_string, id_segmento, tamanio_segmento);
    }
    else if (strcmp(config_km.alloc_strategy, "WORST") == 0)
    {
        nuevo_segmento = buscar_worst_bloque(pid_string, id_segmento, tamanio_segmento);
    }
    else
    {
        perror("El algoritmo de asignacion no es valido");
        return NULL; // TODO: AVISAR AL KS
    }
    return nuevo_segmento;
}

void iniciar_compactacion(int socket_ks)
{
    segmentos_para_compactar = obtener_todos_los_segmentos();

    list_sort(segmentos_para_compactar, comparar_segmentos_por_base);

    uint32_t proxima_base_libre = 0;

    for (int i = 0; i < list_size(segmentos_para_compactar); i++)
    {

        t_segmento *segmento = list_get(segmentos_para_compactar, i);

        uint32_t base_vieja = segmento->base;
        uint32_t base_nueva = proxima_base_libre;
        uint32_t tamanio_segmento = segmento->limite;

        if (base_vieja != base_nueva)
        {
            log_debug(log_km,
                      "Compactando segmento <%d>: base vieja <%d> - base nueva <%d> - tamaño <%d>",
                      segmento->id_segmento,
                      base_vieja,
                      base_nueva,
                      tamanio_segmento);

            /*
             * TODO: CAMNIAR REALMENTE LA MEMORIA EN MEMORY STICK
             * Acá después hay q que mover los bytes reales:
             * leer desde base_vieja
             * escribir en base_nueva
             *
             *
             */
            segmento->base = base_nueva;
        }

        proxima_base_libre += tamanio_segmento;
    }

    reconstruir_huecos_despues_de_compactar(proxima_base_libre);

    list_destroy(segmentos_para_compactar);

    log_debug(log_km, "## Fin de compactación");

    op_code compactacion_ok;
    send(socket_ks, &compactacion_ok,sizeof(compactacion_ok),0);

}

bool comparar_segmentos_por_base(void *a, void *b)
{
    t_segmento *segmento_a = (t_segmento *)a;
    t_segmento *segmento_b = (t_segmento *)b;

    return segmento_a->base < segmento_b->base;
}

void reconstruir_huecos_despues_de_compactar(uint32_t proxima_base_libre)
{

    list_clean_and_destroy_elements(huecos, free);

    uint32_t memoria_total = calcular_memoria_total();

    if (proxima_base_libre < memoria_total)
    {
        t_hueco_libre *hueco_libre_nuevo = malloc(sizeof(t_hueco_libre));

        hueco_libre_nuevo->base = proxima_base_libre;
        hueco_libre_nuevo->tamanio = memoria_total - proxima_base_libre;

        list_add(huecos, hueco_libre_nuevo);
    }
}

t_list *obtener_todos_los_segmentos()
{
    segmentos_para_compactar = list_create();
    dictionary_iterator(diccionario_contextos_ejecucion, obtener_segmentos);
    return segmentos_para_compactar;
}

void obtener_segmentos(char *pid_str, void *contexto_ejec)
{
    t_contextoEjec *contexto = (t_contextoEjec *)contexto_ejec;
    list_add_all(segmentos_para_compactar, contexto->tabla_segmentos);
}

uint32_t calcular_memoria_total()
{
    uint32_t memoria_total = 0;
    for (int i = 0; i < list_size(sticks_conectados); i++)
    {
        t_memory_stick *stick = list_get(sticks_conectados, i);
        memoria_total += stick->tamanio;
    }
    return memoria_total;
}

bool acceso_valido_en_segmentos(pid_t pid, uint32_t direccion_fisica, uint32_t cantidad_bytes)
{

    char *pid_str = string_itoa(pid);

    t_contextoEjec *contexto = dictionary_get(diccionario_contextos_ejecucion, pid_str);
    free(pid_str);

    if (contexto == NULL)
    {
        return false;
    }

    for (int i = 0; i < list_size(contexto->tabla_segmentos); i++)
    {
        t_segmento *segmento = list_get(contexto->tabla_segmentos, i);

        if (direccion_fisica >= segmento->base)
        {
            uint32_t desplazamiento = direccion_fisica - segmento->base;

            if (desplazamiento < segmento->limite && cantidad_bytes <= segmento->limite - desplazamiento)
            {
                return true;
            }
        }
    }

    return false;
}

t_memory_stick* encontrar_stick_por_base_del_segmento(uint32_t base_segmento){
   
    for (int i = 0; i < list_size(sticks_conectados); i++){

        t_memory_stick *stick = list_get(sticks_conectados, i);

        uint32_t fin_stick = stick->inicio + stick->tamanio;

        if (base_segmento >= stick->inicio && base_segmento < fin_stick){
            return stick; //retorna el stick en el que esta el inicio del segmento, puede ser que el fin del segmento este en otro stick
        }
    }

    return NULL;
}

void *obtener_contenido_segmento(t_segmento *segmento){
    uint32_t tamanio_segmento = segmento->limite;

    void *contenido_completo = malloc(tamanio_segmento);

    if (contenido_completo == NULL){
        log_error(log_km, "No se pudo reservar memoria para leer el segmento <%d>", segmento->id_segmento);
        return NULL;
    }

    uint32_t base_segmento = segmento->base;

    uint32_t bytes_restantes = tamanio_segmento;

    uint32_t desplazamiento_buffer = 0;

    while (bytes_restantes > 0)
    {
        t_memory_stick *stick = encontrar_stick_por_base_del_segmento(base_segmento);

        if (stick == NULL)
        {
            log_error(
                log_km, "No existe un MS para la BASE <%u>", base_segmento);

            free(contenido_completo);
            return NULL;
        }

        uint32_t base_segmento_en_stick = base_segmento - stick->inicio;

        uint32_t bytes_disponibles_en_stick = stick->tamanio - base_segmento_en_stick;

        uint32_t bytes_a_leer;

        if (bytes_restantes < bytes_disponibles_en_stick){
            bytes_a_leer = bytes_restantes;
        }
        else{
            bytes_a_leer = bytes_disponibles_en_stick;
        }

        bool lectura_ok = solicitar_lectura_a_stick(stick, base_segmento_en_stick, bytes_a_leer, (char *)contenido_completo+ desplazamiento_buffer);

        if (!lectura_ok)
        {
            free(contenido_completo);
            return NULL;
        }

        log_debug(log_km, "Segmento <%d> - MS <%d> - BASE <%u> - BASE_segmento_en_stick <%u> - Bytes <%u>", segmento->id_segmento,stick->id,base_segmento,base_segmento_en_stick,bytes_a_leer);

        base_segmento += bytes_a_leer;
        bytes_restantes -= bytes_a_leer;
        desplazamiento_buffer += bytes_a_leer;
    }

    return contenido_completo;
}

// resisar: obtener_contenido_segmento y  solicitar_lectura_a_stick
// falta agregarlas al .h
// pensar si lo que hicimos esta bien jiji

bool solicitar_lectura_a_stick(t_memory_stick *stick, uint32_t base_segmento_en_stick, uint32_t tamanio, void *destino){
    t_paquete *pedido = crear_paquete(SOLICITUD_LECTURA);
    agregar_a_paquete(pedido, &base_segmento_en_stick, sizeof(uint32_t));
    agregar_a_paquete(pedido, &tamanio, sizeof(uint32_t));
    enviar_paquete_send_all(pedido, stick->socket);
    eliminar_paquete(pedido);

    op_code respuesta;
    int recibidos = recv(stick->socket, &respuesta, sizeof(respuesta), MSG_WAITALL);

    if (recibidos != sizeof(respuesta)){
        log_error(log_km, "No se recibió respuesta del MS <%d>", stick->id);
        return false;
    }

    if (respuesta != LECTURA_OK){
        log_error(log_km, "El MS <%d> rechazó la lectura", stick->id);
        return false;
    }

    recibidos = recv(stick->socket, destino, tamanio, MSG_WAITALL);

    if (recibidos != tamanio){
        log_error(log_km, "MS <%d>: se esperaban <%u> bytes y se recibieron <%d>", stick->id, tamanio, recibidos);
        return false;
    }

    return true;
}

uint32_t enviar_segmento_a_swap(pid_t pid, t_segmento* segmento, char* contenido){
    
    uint32_t bloque_libre = obtener_bloque_libre_swap();
    
    int id_segmento = segmento->id_segmento;
    uint32_t base = segmento->base;
    uint32_t limite = segmento->limite;

    t_paquete* segmento_a_guardar = crear_paquete(GUARDAR_SEGMENTO);
    agregar_a_paquete(segmento_a_guardar, &id_segmento, sizeof(int));
    agregar_a_paquete(segmento_a_guardar, &base, sizeof(uint32_t));
    agregar_a_paquete(segmento_a_guardar, &limite, sizeof(uint32_t));
    agregar_a_paquete(segmento_a_guardar, contenido,sizeof(contenido));
    agregar_a_paquete(segmento_a_guardar, &bloque_libre, sizeof(uint32_t));
    //por las dudas re pensar si hace falta mandar el pid o no

    return bloque_libre;
}


//TODO: IMPLEMENTAR RTA EN SWAP !!!!!!!!!!!!!!!!!!!!!!
uint32_t obtener_bloque_libre_swap(){
    op_code solicitud_bloque_libre = SOLICITUD_BLOQUE_LIBRE;
    send(socket_swap,&solicitud_bloque_libre, sizeof(solicitud_bloque_libre),0);
    //verificacion de enviado????
    uint32_t bloque;
    recv(socket_swap, &bloque, sizeof(int), MSG_WAITALL);
    return bloque;
}


t_segmento* iniciar_creacion_segmento(pid_t pid, int id_segmento, uint32_t tamanio_segmento, int socket_ks){

    char* pid_string = string_itoa(pid);
    t_segmento* segmento_creado;

      if (verificar_tamanio_valido(tamanio_segmento))
            {

                if (existe_hueco_contiguo(tamanio_segmento))
                {
                    // TODO: meter comparacion de algoritmos en una funcion

                    segmento_creado = elegir_algoritmo(pid_string, id_segmento, tamanio_segmento);
                }
                else
                {
                    // buscamos si hay espacio libre en toda la memoria
                    uint32_t memoria_libre_total = calcular_memoria_libre();
                    if (memoria_libre_total >= tamanio_segmento)
                    {
                        log_debug(log_km, "Hay espacio en memoria pero no contiguo. Es necesario compactar");
                        op_code desalojar = DESALOJAR_CPU; // TODO FIJARSE CON GABI Q LO MANDE ASI
                        send(socket_ks, &desalojar, sizeof(desalojar), 0);
                        // TODO: COMPACTAR
                        // 1. avisar al KS de la compactacion. Solicitar desalojo de cpus
                        op_code desalojo_ok;
                        recv(socket_ks, &desalojo_ok, sizeof(desalojo_ok), 0);
                        // 2. Compactar memoria despues de confirmacion de desalojo
                        iniciar_compactacion(socket_ks);
                        // 3. Avisar compactacion OK
                    }
                    else
                    {
                        log_error(log_km, "No hay espacio en memoria, no se puede crear el segmento <%d> de tamanio <%d> del PID <%d>. Memoria libre total: <%d>",
                                  id_segmento,
                                  tamanio_segmento,
                                  pid,
                                  memoria_libre_total);
                        return NULL;

                        // TODO: avisar ks?
                    }
                }
            }
        return segmento_creado;

}

void escribir_segmento_en_ms(t_segmento* segmento_restaurado, void* contenido){
    
    t_memory_stick* stick = encontrar_stick_por_base_del_segmento(segmento_restaurado->base);

    uint32_t base_segmento_en_stick = segmento_restaurado->base - stick->inicio;
    uint32_t direccion_fisica = base_segmento_en_stick;
    
    t_paquete* paquete_solicitud_escritura = crear_paquete(SOLICITUD_ESCRITURA);

    uint32_t cantidad_bytes = sizeof(contenido);

    agregar_a_paquete(paquete_solicitud_escritura, &direccion_fisica,sizeof(direccion_fisica));
    agregar_a_paquete(paquete_solicitud_escritura, &cantidad_bytes,sizeof(uint32_t));
    agregar_a_paquete(paquete_solicitud_escritura, contenido, cantidad_bytes);
    
    enviar_paquete_send_all(paquete_solicitud_escritura,socket_memory_stick);

    eliminar_paquete(paquete_solicitud_escritura);

}

char* obtener_contenido_bloque_swap(uint32_t bloque_swap){
    
    op_code obtener_contenido_bloque = OBTENER_CONTENIDO_BLOQUE;

    send(socket_swap, &obtener_contenido_bloque, sizeof(obtener_contenido_bloque),0);
        
    t_list* paquete_bloque_swap = recibir_paquete(socket_swap);
    char* contenido = (char*)list_get(paquete_bloque_swap,0);

    list_destroy(paquete_bloque_swap);

    return contenido;

}

