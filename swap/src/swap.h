//#include"utils/conexiones.h"
#include "utils/serializacion.h"
#include<commons/log.h>
#include<commons/config.h>
#include<readline/readline.h> 
#include<commons/string.h>
#include<errno.h>
#include<unistd.h>
#include<sys/mman.h>
#include <commons/txt.h>

t_log* log_swap;
int conexion_kernel_memory; 
void* espacio_swap_mmap;

void escribir_en_archivo_swap(uint32_t numero_bloque, char* contenido, int block_size);

void leer_archivo_swap(uint32_t numero_bloque, char* contenido_leido, int block_size);
void inicializar_swap(char* file_path, int file_size);