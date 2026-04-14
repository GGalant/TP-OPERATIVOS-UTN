#define KERNEL_SCHEDULER_H_
#ifndef KERNEL_SCHEDULER_H_

#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <utils/conexiones.h>
#include <utils/serializacion.h>

t_log *log_ks;

void* handle_connections(void* arg);

#endif /* KERNEL_SCHEDULER_H_ */