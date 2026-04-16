#include"utils/conexiones.h"
#include<commons/log.h>
#include<commons/config.h>
#include<readline/readline.h> 
#include<commons/string.h>
#include<errno.h>
#include<unistd.h>

t_log* log_swap;
int conexion_kernel_memory; 