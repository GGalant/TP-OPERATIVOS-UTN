//#define KERNEL_SCHEDULER_H_
//#ifndef KERNEL_SCHEDULER_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
//#include <utils/conexiones.h> serealizacion tiene a conexiones.h


t_log *log_ks;


void* handle_connections(void* arg);

//#endif /* KERNEL_SCHEDULER_H_ */