#include "kernel_scheduler.h"

void* handle_io(void* arg) {    
    int socket_io = *(int*)arg;
    t_io* io;
    free(arg);

    io = crear_io(socket_io);
    if (io->io_id < 0) {    
        perror("No se le pudo asignar un ID a la IO");
        close(socket_io);
        return NULL;
    }

    pthread_mutex_lock(&mutex_lista_io_conectadas);
    list_add(lista_io_conectadas, io);
    pthread_mutex_unlock(&mutex_lista_io_conectadas);
    
    log_info(log_debug_ks, "IO id <%d> tipo <%d> conectada", io->io_id, io->tipo);

    while(true) {   
        // aca iria la logica de operaciones con IO 
        op_code_ks_io codigo_operacion = recibir_operacion(socket_io);

        switch(codigo_operacion) {
            case FIN_IO:
                int pid;
                recv(socket_io, &pid, sizeof(int), MSG_WAITALL);
                log_info(log_ks, "## (<%d>) finalizó IO y pasa a READY / SUSP. READY", pid);
                //deberiamos tener una funcion que reciba el pid y mueva el proceso a ready o susp. ready segun corresponda, esa logica la manejaria el planificador de mediano plazo
                
                // tambien deberiamos liberar la IO, cambiar su estado a libre y dejarla disponible para otro proceso que la necesite
                break;
            case -1:
                log_error(log_ks, "IO FD %d se desconectó. Terminando handle", socket_io);
                return NULL;
            default:
                log_warning(log_ks, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

t_io *crear_io(int socket_io) {  
  t_io *io = malloc(sizeof(t_io));
  if (io == NULL) return NULL; 

  t_list *paquete_io = recibir_paquete(socket_io);
  io->socket_io = socket_io;
  io->io_id = *(int*)list_get(paquete_io, 0);
  io->tipo = *(int*)list_get(paquete_io, 1);
  io->estado = LIBRE;

  list_destroy_and_destroy_elements(paquete_io, free);

  log_debug_ks(log_debug_ks, "IO de tipo <%d> y id <%d> creada", io->tipo, io->io_id);

  return io;
}