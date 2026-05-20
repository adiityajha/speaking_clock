#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Scheduler -------------------------------------------------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* ---- Clock / tick ----------------------------------------------------- */
/* mps2-an500 default FPGA clock is 25 MHz */
#define configCPU_CLOCK_HZ                      ( 25000000UL )
#define configTICK_RATE_HZ                      ( 1000 )

/* ---- Tasks ------------------------------------------------------------ */
#define configMAX_PRIORITIES                    ( 8 )
#define configMINIMAL_STACK_SIZE                ( 128 )
#define configMAX_TASK_NAME_LEN                 ( 12 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/* ---- Heap ------------------------------------------------------------- */
/* heap_4.c allocates from this array in .bss */
#define configTOTAL_HEAP_SIZE                   ( 128 * 1024 )

/* ---- Queues & semaphores ---------------------------------------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configQUEUE_REGISTRY_SIZE               0

/* ---- Co-routines (not used) ------------------------------------------- */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         1

/* ---- Timers (lwIP uses them) ------------------------------------------ */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* ---- NVIC priority config for Cortex-M7 ------------------------------- */
/* QEMU mps2-an500 has 8 NVIC priority bits (0 = highest, 255 = lowest).
 * FreeRTOS masks interrupts at configMAX_SYSCALL_INTERRUPT_PRIORITY and
 * below (numerically higher = lower priority).
 * SysTick / PendSV run at the lowest priority (255). */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configLIBRARY_PRIO_BITS                      4

#define configKERNEL_INTERRUPT_PRIORITY     \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configLIBRARY_PRIO_BITS) )

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configLIBRARY_PRIO_BITS) )

/* ---- Assert ----------------------------------------------------------- */
#define configASSERT( x ) \
    if ( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* ---- Map FreeRTOS handlers to CMSIS names ----------------------------- */
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   PendSV_Handler
#define xPortSysTickHandler  SysTick_Handler

/* ---- Optional features ------------------------------------------------ */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xResumeFromISR              1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 0
#define INCLUDE_xTaskGetIdleTaskHandle      0
#define INCLUDE_eTaskGetState               0
#define INCLUDE_xEventGroupSetBitFromISR    1
#define INCLUDE_xTimerPendFunctionCall      1
#define INCLUDE_xTaskAbortDelay             0
#define INCLUDE_xTaskGetHandle              0

#endif /* FREERTOS_CONFIG_H */
