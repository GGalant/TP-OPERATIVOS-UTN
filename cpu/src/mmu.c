#include "cpu.h"

uint32_t mmu_traducir_direccion(uint32_t direccion_logica_base, size_t bytes_a_copiar) {

    uint32_t num_segmento = direccion_logica_base / tam_max_segmentos;
    uint32_t desplazamiento_segmento = direccion_logica_base % tam_max_segmentos;

        t_segmento* segmento = get_segmento_por_id(num_segmento);
    
    if (segmento == NULL) {
        log_error(log_cpu, "MMU: SEG_FAULT tipo 1, segmento inválido - El segmento %u no existe para la direccion logica %u EN EL SCOPE DE ESTE PROCESO.", num_segmento, direccion_logica_base);
        return ERROR;
    }

    if (desplazamiento_segmento + bytes_a_copiar > segmento->limite) {
        log_error(log_cpu, "MMU: SEG_FAULT tipo 2, overflow - con Segmento %u, desplazamiento %u, bytes %zu, tamanio segmento %u",
                   num_segmento, desplazamiento_segmento, bytes_a_copiar, segmento->limite);
        return ERROR;
    }

    uint32_t direccion_fisica = segmento->base + desplazamiento_segmento;

    return direccion_fisica;
}

t_segmento* get_segmento_por_id(uint32_t id_segmento){
    for (int i = 0; i < list_size(cpu.tabla_segmentos); i++)
    {
        t_segmento* segmento = list_get(cpu.tabla_segmentos, i);
        if (segmento->id_segmento == id_segmento)
        {
            return segmento;
        }
        
    }

    return NULL;
    
}