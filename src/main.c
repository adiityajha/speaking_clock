/* main.c
 * Entry point. Creates sync primitives and tasks, then hands off to FreeRTOS.
 *
 * Task priorities:
 *   key_task / speech_task : 1
 *   ntp_task               : 2  (slightly higher so NTP fetch isn't interrupted)
 *
 * IPC:
 *   time_request_sem – key_task signals ntp_task
 *   time_queue       – ntp_task sends result to speech_task
 *   dhcp_ready_sem   – DHCP callback unblocks ntp_task when IP is ready
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "../include/time_msg.h"
#include <stdio.h>

SemaphoreHandle_t time_request_sem;
QueueHandle_t     time_queue;
SemaphoreHandle_t dhcp_ready_sem;

void key_task(void *param);
void ntp_task(void *param);
void speech_task(void *param);

extern void network_init(void);

static void system_init(void)
{
    printf("\r\n=== Speaking Clock (QEMU mps2-an500) ===\r\n");
    printf("[MAIN] FreeRTOS %s\r\n", tskKERNEL_VERSION_NUMBER);
}

int main(void)
{
    system_init();

    /* create dhcp_ready_sem before network_init so the DHCP callback
     * can give it safely as soon as an IP is assigned */
    dhcp_ready_sem = xSemaphoreCreateBinary();
    configASSERT(dhcp_ready_sem != NULL);

    network_init();   /* starts lwIP + DHCP */

    time_request_sem = xSemaphoreCreateBinary();
    configASSERT(time_request_sem != NULL);

    time_queue = xQueueCreate(1, sizeof(time_msg));
    configASSERT(time_queue != NULL);

    xTaskCreate(key_task,    "KEY",    256,  NULL, 1, NULL);
    xTaskCreate(ntp_task,    "NTP",    1024, NULL, 2, NULL);
    xTaskCreate(speech_task, "SPEECH", 512,  NULL, 1, NULL);

    printf("[MAIN] Starting scheduler\r\n");
    vTaskStartScheduler();

    printf("[MAIN] ERROR: scheduler returned!\r\n");
    while (1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("[PANIC] Stack overflow: %s\r\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}
