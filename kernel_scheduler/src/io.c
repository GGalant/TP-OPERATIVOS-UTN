#include "io.h"
#include "kernel_scheduler.h"
#include "planificacion.h"

void* handle_io(void* arg) {    
    int socket_io = *(int*)arg;
    t_io* io = NULL;
    free(arg); 

    op_code op = recibir_operacion(socket_io);
    if (op != CREAR_IO) {
        log_error(log_ks, "Error en handshake de IO. Se esperaba CREAR_IO y se recibió %d", op);
        close(socket_io);
        return NULL;
    }

    io = crear_io(socket_io);
    if (io == NULL) {    
        perror("Error al crear IO");
        close(socket_io);
        free(io);
        return NULL;
    } 

    switch (io->tipo) {
    case SLEEP:
        handle_io_sleep(io);
        break;
    case STDIN:
        handle_io_stdin(io);
        break;
    case STDOUT:
        handle_io_stdout(io);
        break;
    default:
        log_error(log_ks, "Tipo de IO desconocida: <%d>", io->tipo);
        break;
    }
    return NULL;
}

void handle_io_sleep(t_io *io) {    
    while(true) {   
        log_debug(log_ks, "Esperando peticion de sleep");
        pthread_mutex_lock(&mutex_sleep);
        while (list_is_empty(cola_espera_sleep)) {
            pthread_cond_wait(&cond_sleep, &mutex_sleep);
        }
        
        log_debug(log_ks, "LLega una peticion de sleep");
        t_peticion_sleep* peticion = list_remove(cola_espera_sleep, 0); 
        
        pthread_mutex_unlock(&mutex_sleep);

        pthread_mutex_lock(&mutex_lista_io_conectadas);
        io->estado = OCUPADA;
        io->pid_actual = peticion->pid;
        pthread_mutex_unlock(&mutex_lista_io_conectadas);

        t_paquete* paquete_sleep = crear_paquete(SOLICITUD_IO_SLEEP);
        agregar_a_paquete(paquete_sleep, &peticion->pid, sizeof(int));
        agregar_a_paquete(paquete_sleep, &peticion->tiempo_sleep, sizeof(int));
        enviar_paquete_send_all(paquete_sleep, io->socket_io);
        eliminar_paquete(paquete_sleep);
        log_debug(log_ks, "Se envia una peticion de sleep");
        
        pid_t pid_actual = peticion->pid; // Guardamos el PID para la validación
        free(peticion); 

        if (!esperar_y_procesar_respuesta_io(io, pid_actual)) {
            return; // Si devolvió false, la IO se desconectó. Terminamos el hilo.
        }
    }
}

void handle_io_stdout(t_io *io) {   
    while(true) {   
        log_debug(log_ks, "Esperando peticion de stdout");
        pthread_mutex_lock(&mutex_stdout);
        while (list_is_empty(cola_espera_stdout)) {
            pthread_cond_wait(&cond_stdout, &mutex_stdout);
        }
        
        t_peticion_stdout* peticion = list_remove(cola_espera_stdout, 0);
        pthread_mutex_unlock(&mutex_stdout);

        pthread_mutex_lock(&mutex_lista_io_conectadas);
        io->estado = OCUPADA;
        io->pid_actual = peticion->pid;
        pthread_mutex_unlock(&mutex_lista_io_conectadas);

        t_paquete* paquete_stdout = crear_paquete(SOLICITUD_IO_STDOUT);
        agregar_a_paquete(paquete_stdout, &peticion->pid, sizeof(int));
        agregar_a_paquete(paquete_stdout, peticion->contenido, strlen(peticion->contenido) + 1);
        enviar_paquete_send_all(paquete_stdout, io->socket_io);
        eliminar_paquete(paquete_stdout);

        pid_t pid_actual = peticion->pid; 
        free(peticion->contenido);
        free(peticion); 

        if (!esperar_y_procesar_respuesta_io(io, pid_actual)) {
            return; 
        }
    }
}

void handle_io_stdin(t_io *io) {
    while (true) {
        log_debug(log_ks, "Esperando peticion de stdin");
        pthread_mutex_lock(&mutex_stdin);
        while (list_is_empty(cola_espera_stdin)) {
            pthread_cond_wait(&cond_stdin, &mutex_stdin);
        }

        t_peticion_stdin *peticion = list_remove(cola_espera_stdin, 0);
        pthread_mutex_unlock(&mutex_stdin);

        pthread_mutex_lock(&mutex_lista_io_conectadas);
        io->estado = OCUPADA;
        io->pid_actual = peticion->pid;
        pthread_mutex_unlock(&mutex_lista_io_conectadas);

        t_paquete *paquete_stdin = crear_paquete(SOLICITUD_IO_STDIN);
        agregar_a_paquete(paquete_stdin, &peticion->pid, sizeof(int));
        agregar_a_paquete(paquete_stdin, &peticion->tamanio, sizeof(uint32_t));
        agregar_a_paquete(paquete_stdin, &peticion->direccion_fisica, sizeof(uint32_t));
        enviar_paquete_send_all(paquete_stdin, io->socket_io);
        eliminar_paquete(paquete_stdin);

        pid_t pid_actual = peticion->pid;
        free(peticion);

        if (!esperar_y_procesar_respuesta_io(io, pid_actual)) {
            return; 
        }
    }
}

int generar_id_io() {
    pthread_mutex_lock(&mutex_generador_id_io);
    int id = ++id_io_global;
    pthread_mutex_unlock(&mutex_generador_id_io);
    return id;
}

const char* io_tipo_to_string(tipo_io tipo) {
    switch (tipo) {
        case STDIN: return "STDIN";
        case STDOUT: return "STDOUT";
        case SLEEP: return "SLEEP";
        default: return "DESCONOCIDO";
    }
}

t_io *crear_io(int socket_io) {  
    t_io *io = malloc(sizeof(t_io));
    if (io == NULL) return NULL; 

    t_list *paquete_io = recibir_paquete(socket_io);
    io->socket_io = socket_io;
    io->io_id = generar_id_io();
    io->tipo = *(int*)list_get(paquete_io, 0);
    io->estado = LIBRE;
    io->pid_actual = -1; // inicialmente ningun proceso esta utilizando la IO
    list_destroy_and_destroy_elements(paquete_io, free);

    pthread_mutex_lock(&mutex_lista_io_conectadas);
    list_add(lista_io_conectadas, io);
    pthread_mutex_unlock(&mutex_lista_io_conectadas);

    log_info(log_ks, "## IO ID<%d> de tipo <%s> Conectada", io->io_id, io_tipo_to_string(io->tipo));

    return io;
}

void finalizar_io(pid_t pid) {
    t_proceso_pcb* proceso = NULL;

    pthread_mutex_lock(&mutex_cola_block); //mutex por si esta siendo usada por otros hilos
    proceso = remover_proceso_por_pid(cola_block, pid); // primero busco en block porque el proceso que esta haciendo IO sleep va a estar bloqueado en la cola block, si no lo encuentro ahi busco en susp block
    pthread_mutex_unlock(&mutex_cola_block);

    //SI entra en este if entonces es que no es NULL porque estaba en la cola block y entonces lo paso a ready
    if (proceso != NULL) {
        // Usar la nueva función productora genérica para agregar procesos a ready
        agregar_proceso_a_ready(proceso);
        log_info(log_ks, "## (%d) finalizó IO y pasa a READY", pid);
        return;
    }
    //Misma secuencia pero para la cola de susp block, si lo encuentra ahi entonces lo pasa a susp ready
    pthread_mutex_lock(&mutex_cola_susp_block);
    proceso = remover_proceso_por_pid(cola_susp_block, pid);
    pthread_mutex_unlock(&mutex_cola_susp_block);

    if (proceso != NULL) {
        
        pthread_mutex_lock(&mutex_cola_susp_ready);
        cambiar_estado_proceso(proceso, SUSP_READY);
        list_add(cola_susp_ready, proceso);
        pthread_cond_signal(&cond_cola_susp_ready);
        pthread_mutex_unlock(&mutex_cola_susp_ready);

        log_info(log_ks, "## (%d) finalizó IO y pasa a SUSP. READY", pid);
        return;
    }

    log_error(log_ks, "No se encontró el proceso %d al finalizar IO", pid);

}

// Esta función espera la respuesta del módulo IO y se encarga de liberar la interfaz.
// Retorna 'true' si el ciclo debe continuar, o 'false' si la IO se desconectó y el hilo debe morir.
bool esperar_y_procesar_respuesta_io(t_io* io, pid_t pid_esperado) {
    op_code codigo_operacion = recibir_operacion(io->socket_io);   
        
    switch(codigo_operacion) {
        case FIN_IO: {
            t_list* paquete_fin_io = recibir_paquete(io->socket_io);
            if (paquete_fin_io == NULL) {
                log_error(log_ks, "IO <%d>: Error al recibir el paquete de FIN_IO.", io->io_id);
                // Si el paquete es NULL, es probable que la conexión se haya cerrado.
                return false;
            }
            pid_t pid_recibido = *(pid_t*)list_get(paquete_fin_io, 0);
            list_destroy_and_destroy_elements(paquete_fin_io, free);

            if (pid_recibido == pid_esperado) {
                finalizar_io(pid_recibido);
            } else {
                log_error(log_ks, "IO <%d> devolvió FIN_IO para PID %d, pero se esperaba %d", io->io_id, pid_recibido, pid_esperado);
            }
            break; 
        }
        case FIN_IO_STDIN: {
            t_list* paquete_fin_io_stdin = recibir_paquete(io->socket_io);
            if (paquete_fin_io_stdin == NULL) {
                log_error(log_ks, "IO <%d>: Error al recibir el paquete de FIN_IO_STDIN)", io->io_id);
                return false;
            }
            pid_t pid_recibido = *(pid_t*)list_get(paquete_fin_io_stdin, 0);
            uint32_t direccion_fisica = *(uint32_t*)list_get(paquete_fin_io_stdin, 1);
            char* contenido = (char*)list_get(paquete_fin_io_stdin, 2);

            list_destroy_and_destroy_elements(paquete_fin_io_stdin, free);

            if (pid_recibido == pid_esperado) {
                solicitar_escritura_a_km(conexion_km, pid_recibido, direccion_fisica, contenido);
                finalizar_io(pid_recibido);
            } else {
                log_error(log_ks, "IO <%d> devolvió FIN_IO_STDIN para PID %d, pero se esperaba %d", io->io_id, pid_recibido, pid_esperado);
            }

            break;
        }
        case -1:
            log_error(log_ks, "IO FD %d se desconectó.", io->socket_io);
            pthread_mutex_lock(&mutex_lista_io_conectadas);
            io->estado = LIBRE;
            io->pid_actual = -1;
            pthread_mutex_unlock(&mutex_lista_io_conectadas);
            return false; 
            
        default:
            log_warning(log_ks, "Operación desconocida: %d", codigo_operacion);
            break;
    }

    pthread_mutex_lock(&mutex_lista_io_conectadas);
    io->estado = LIBRE;
    io->pid_actual = -1;
    pthread_mutex_unlock(&mutex_lista_io_conectadas);
    
    return true; 
}
