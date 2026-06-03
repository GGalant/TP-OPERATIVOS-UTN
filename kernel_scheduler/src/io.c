#include "kernel_scheduler.h"
#include "planificacion.h"

void* handle_io(void* arg) {    
    int socket_io = *(int*)arg;
    t_io* io = NULL;
    free(arg); 

    io = crear_io(socket_io);
    if (io->io_id < 0) {    
        perror("No se le pudo asignar un ID a la IO");
        close(socket_io);
        free(io);
        return NULL;
    } 
    pthread_mutex_lock(&mutex_lista_io_conectadas);
    list_add(lista_io_conectadas, io);
    pthread_mutex_unlock(&mutex_lista_io_conectadas);
    
    log_info(log_debug_ks, "IO id <%d> tipo <%d> conectada", io->io_id, io->tipo);

    switch (io->tipo) {
    case SLEEP:
        handle_io_sleep(io);
        break;
    case STDIN:
        //handle_io_stdin(io);
        break;
    case STDOUT:
        //handle_io_stdout(io);
        break;
    default:
        log_error(log_ks, "Tipo de IO desconocida: <%d>", io->tipo);
        break;
    }
    return NULL;
}

void handle_io_sleep(t_io *io) {    
    while(true) {   
        pthread_mutex_lock(&mutex_sleep);
        while (list_is_empty(cola_espera_sleep)) {
            pthread_cond_wait(&cond_sleep, &mutex_sleep);
        }
        
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

        op_code_ks_io codigo_operacion = recibir_operacion(io->socket_io);   
        
        switch(codigo_operacion) {
            case FIN_IO:
                int pid;
                int bytes_recibidos = recv(io->socket_io, &pid, sizeof(int), MSG_WAITALL);
                //log_info(log_ks, "## (<%d>) finalizó IO y pasa a READY / SUSP. READY", pid); Saco este loginfo y lo creo dentro de la funci'on finalizar io ya que en esa funci'on diferencio cual va a ready y cual a susp ready segun corresponda
                    if (bytes_recibidos <= 0) {
                      log_error(log_ks, "No se pudo recibir el PID de fin de IO");
                      free(peticion);
                      return;
                    }
                //IMPLEMENTAR: deberiamos tener una funcion que reciba el pid y mueva el proceso a ready o susp. ready segun corresponda, esa logica la manejaria el planificador de mediano plazo
                finalizar_io(pid);

               
                pthread_mutex_lock(&mutex_lista_io_conectadas);
                io->estado = LIBRE;
                io->pid_actual = -1;
                pthread_mutex_unlock(&mutex_lista_io_conectadas);

                free(peticion);
                break;
            case -1:
                log_error(log_ks, "IO FD %d se desconectó. Terminando handle", io->socket_io);
                 pthread_mutex_lock(&mutex_lista_io_conectadas);
                 io->estado = LIBRE;
                 io->pid_actual = -1;
                 pthread_mutex_unlock(&mutex_lista_io_conectadas);
                free(peticion);
                return;
            default:
                log_warning(log_ks, "operacion desconocida: %d", codigo_operacion);
                 pthread_mutex_lock(&mutex_lista_io_conectadas);
                 io->estado = LIBRE;
                 io->pid_actual = -1;
                 pthread_mutex_unlock(&mutex_lista_io_conectadas);
                free(peticion);
                break;
        }
    }
}

int generar_id_io() {
    pthread_mutex_lock(&mutex_generador_id_io);
    int id = ++id_io_global;
    pthread_mutex_unlock(&mutex_generador_id_io);
    return id;
}

t_io *crear_io(int socket_io) {  
    t_io *io = malloc(sizeof(t_io));
    if (io == NULL) return NULL; 

    t_list *paquete_io = recibir_paquete(socket_io);
    io->socket_io = socket_io;
    io->io_id = generar_id_io();
    io->tipo = *(int*)list_get(paquete_io, 1);
    io->estado = LIBRE;
    io->pid_actual = -1; // inicialmente ningun proceso esta utilizando la IO
    list_destroy_and_destroy_elements(paquete_io, free);

    log_error(log_debug_ks, "IO de tipo <%d> y id <%d> creada", io->tipo, io->io_id);

    return io;
    
}

    void finalizar_io(int pid) {
    t_proceso_pcb* proceso = NULL;

    pthread_mutex_lock(&mutex_cola_block); //mutex por si esta siendo usada por otros hilos
    proceso = remover_proceso_por_pid(cola_block, pid); // primero busco en block porque el proceso que esta haciendo IO sleep va a estar bloqueado en la cola block, si no lo encuentro ahi busco en susp block
    pthread_mutex_unlock(&mutex_cola_block);

    //SI entra en este if entonces es que no es NULL porque estaba en la cola block y entonces lo paso a ready
    if (proceso != NULL) {
        
        pthread_mutex_lock(&mutex_cola_ready);
        cambiar_estado_proceso(proceso, READY);
        list_add(cola_ready, proceso);
        pthread_cond_signal(&cond_cola_ready);
        pthread_mutex_unlock(&mutex_cola_ready);

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

//Saca de la lista si lo encuentra al pid

