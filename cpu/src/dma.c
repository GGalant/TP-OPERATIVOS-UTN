#include "cpu.h"

//#define MAX_BUFFER 128

#define MIN(a, b) ((a) < (b) ? (a) : (b))

//void * buffer[128];

int dma_copiar_memoria(uint32_t dir_fisica_origen, uint32_t dir_fisica_destino, uint32_t bytes_a_copiar){
    log_debug(log_cpu, "DMA_copiar_memoria solicitado");

    if(bytes_a_copiar == 0){
        return 0;
    }

    uint8_t* buffer = malloc(bytes_a_copiar); //! malloc

    if (buffer == NULL) {
        log_error(log_cpu, "DMA: Fallo al asignar buffer intermedio (Out of memory)");
        return ERROR; 
    }

    int error = 0;

    error = dma_leer_memoria(dir_fisica_origen, bytes_a_copiar, buffer);

    if(error == ERROR){
        free(buffer);
        return ERROR;
    }

    error = dma_escribir_memoria(dir_fisica_destino, bytes_a_copiar, buffer);

    if(error == ERROR){
        free(buffer);
        return ERROR;
    }

    free(buffer);
    return 0;

}

int dma_leer_memoria(uint32_t dir_fisica, uint32_t bytes_a_leer, uint8_t* buffer_de_tamanio_exacto){ //TODO tipo de buffer
    log_debug(log_cpu, "DMA_leer_memoria solicitado");

    uint32_t bytes_restantes = bytes_a_leer;
    uint32_t offset_bytes_leidos = 0;
    uint32_t puntero_direccion = dir_fisica;

    while(bytes_restantes > 0){

        t_memory_stick* stick = encontrar_stick_segun_direccion(puntero_direccion);

        if (stick == NULL) {
            log_error(log_cpu, "DMA: No se encontro un Memory Stick para la direccion %u", puntero_direccion);
            return ERROR;
        }

        uint32_t base_en_stick = puntero_direccion - stick->inicio;
        uint32_t bytes_a_leer_de_stick = MIN(stick->tamanio - base_en_stick, bytes_restantes);

        
        bool exito = solicitar_lectura_a_stick(stick, base_en_stick, bytes_a_leer_de_stick, buffer_de_tamanio_exacto + offset_bytes_leidos);
        
        if(!exito){
            log_error(log_cpu, "DMA: NO SE PUDO LEER DEL MEMORY STICK. stick: %u, nbytes: %u", puntero_direccion, bytes_a_leer_de_stick);
            return ERROR;
        }

        bytes_restantes -= bytes_a_leer_de_stick;
        offset_bytes_leidos += bytes_a_leer_de_stick; 
        puntero_direccion += bytes_a_leer_de_stick;
    }

    return 0;
    
}

int dma_escribir_memoria(uint32_t dir_fisica, uint32_t bytes_a_escribir, uint8_t* buffer){
    log_debug(log_cpu, "DMA_escribir_memoria solicitado");

    uint32_t bytes_restantes = bytes_a_escribir;
    uint32_t offset_bytes_escritos = 0;
    uint32_t puntero_direccion = dir_fisica;

    while(bytes_restantes > 0){

        t_memory_stick* stick = encontrar_stick_segun_direccion(puntero_direccion);

        if (stick == NULL) {
            log_error(log_cpu, "DMA: No se encontro un Memory Stick para la direccion %u", puntero_direccion);
            return ERROR;
        }

        uint32_t base_en_stick = puntero_direccion - stick->inicio;
        uint32_t bytes_a_escribir_en_stick = MIN(stick->tamanio - base_en_stick, bytes_restantes);

        
        bool exito = solicitar_escritura_a_stick(stick, base_en_stick, bytes_a_escribir_en_stick, buffer + offset_bytes_escritos);
        
        if(!exito){
            log_error(log_cpu, "DMA: NO SE PUDO ESCRIBIR EN EL MEMORY STICK. stick: %u, nbytes: %u", puntero_direccion, bytes_a_escribir_en_stick);
            return ERROR;
        }

        bytes_restantes -= bytes_a_escribir_en_stick;
        offset_bytes_escritos += bytes_a_escribir_en_stick; 
        puntero_direccion += bytes_a_escribir_en_stick;
    }

    return 0;

}

t_memory_stick* encontrar_stick_segun_direccion(uint32_t direccion_fisica){
   
    for (int i = 0; i < list_size(cpu.sticks_conectados); i++){

        t_memory_stick *stick = list_get(cpu.sticks_conectados, i);

        uint32_t fin_stick = stick->inicio + stick->tamanio;

        if (direccion_fisica >= stick->inicio && direccion_fisica < fin_stick){
            return stick;
        }

    }

    return NULL;
}

bool solicitar_escritura_a_stick(t_memory_stick *stick, uint32_t base_en_stick, uint32_t tamanio, void *buffer_origen){

    t_paquete* pedido = crear_paquete(SOLICITUD_ESCRITURA);

    agregar_a_paquete(pedido, &base_en_stick, sizeof(uint32_t));
    agregar_a_paquete(pedido, &tamanio, sizeof(uint32_t));
    agregar_a_paquete(pedido, buffer_origen, tamanio);
    
    enviar_paquete_send_all(pedido, stick->socket);

    eliminar_paquete(pedido);

    op_code respuesta;
    int recibidos = recv(stick->socket, &respuesta, sizeof(respuesta), MSG_WAITALL);

    if (recibidos != sizeof(respuesta)){
        log_error(log_cpu, "DMA: No se recibió respuesta del MS <%d>", stick->id);
        return false;
    }

    if (respuesta != ESCRITURA_OK){
        log_error(log_cpu, "DMA: El MS <%d> rechazó la escritura", stick->id);
        return false;
    }

    return true;
}

bool solicitar_lectura_a_stick(t_memory_stick *stick, uint32_t base_en_stick, uint32_t tamanio, void *destino){
    t_paquete *pedido = crear_paquete(SOLICITUD_LECTURA);
    agregar_a_paquete(pedido, &base_en_stick, sizeof(uint32_t));
    agregar_a_paquete(pedido, &tamanio, sizeof(uint32_t));
    enviar_paquete_send_all(pedido, stick->socket);
    eliminar_paquete(pedido);

    op_code respuesta;
    int recibidos = recv(stick->socket, &respuesta, sizeof(respuesta), MSG_WAITALL);

    if (recibidos != sizeof(respuesta)){
        log_error(log_cpu, "DMA: No se recibió respuesta del MS <%d>", stick->id);
        return false;
    }

    if (respuesta != LECTURA_OK){
        log_error(log_cpu, "DMA: El MS <%d> rechazó la lectura", stick->id);
        return false;
    }

    recibidos = recv(stick->socket, destino, tamanio, MSG_WAITALL);

    if (recibidos != tamanio){
        log_error(log_cpu, "DMA: MS <%d>: se esperaban <%u> bytes y se recibieron <%d>", stick->id, tamanio, recibidos);
        return false;
    }

    return true;
}