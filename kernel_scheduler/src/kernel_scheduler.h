//#define KERNEL_SCHEDULER_H_
//#ifndef KERNEL_SCHEDULER_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>


t_log *log_ks;
int server_ks;

void* handle_connections(void* arg);
void* handle_km(void* arg);
void* handle_cpu(void* arg);
void* handle_io(void* arg);

//#endif /* KERNEL_SCHEDULER_H_ */