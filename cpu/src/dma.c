#include "cpu.h"

void * buffer[128];

int dma_leer_memoria(uint32_t dir_fisica, uint32_t bytes_a_leer, void* buffer);
int dma_escribir_memoria(uint32_t dir_fisica, uint32_t bytes_a_escribir, void* buffer);
int dma_copiar_memoria(uint32_t dir_fis_origen, uint32_t dir_fis_destino, uint32_t bytes_a_copiar);


int dma_leer_memoria(uint32_t dir_fisica, uint32_t bytes_a_leer, void* buffer){

};

int dma_escribir_memoria(uint32_t dir_fisica, uint32_t bytes_a_escribir, void* buffer){

};

int dma_copiar_memoria(uint32_t dir_fis_origen, uint32_t dir_fis_destino, uint32_t bytes_a_copiar){

};