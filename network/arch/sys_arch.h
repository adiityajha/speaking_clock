#ifndef ARCH_SYS_ARCH_H
#define ARCH_SYS_ARCH_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* lwIP OS abstraction types backed by FreeRTOS primitives */
typedef SemaphoreHandle_t   sys_sem_t;
typedef SemaphoreHandle_t   sys_mutex_t;
typedef QueueHandle_t       sys_mbox_t;
typedef TaskHandle_t        sys_thread_t;

#define SYS_MBOX_NULL   ( (sys_mbox_t)  NULL )
#define SYS_SEM_NULL    ( (sys_sem_t)   NULL )
#define SYS_MUTEX_NULL  ( (sys_mutex_t) NULL )

#endif /* ARCH_SYS_ARCH_H */
