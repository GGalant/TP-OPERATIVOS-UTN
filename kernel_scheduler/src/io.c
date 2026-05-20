#include "io.h"

void* handle_io(void* arg) {    
    int socket_io = *(int*)arg;
    free(arg);
    
    log_info(log_ks, "IO conectada: %d", socket_io);

    while(true) {   
        // aca iria la logica de operaciones con IO 
        int codigo_operacion = recibir_operacion(socket_io);

        switch(codigo_operacion) {
            case -1:
                log_error(log_ks, "IO FD %d se desconectó. Terminando handle", socket_io);
                return NULL;
            default:
                log_warning(log_ks, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}