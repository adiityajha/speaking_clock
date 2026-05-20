/* ntp_task.c
 * Waits on time_request_sem (given by key_task when 't' is pressed).
 * Fetches current IST via ntp_get_time() and passes the result
 * to speech_task through time_queue.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "../include/time_msg.h"
#include <stdio.h>

extern SemaphoreHandle_t time_request_sem;
extern QueueHandle_t     time_queue;
extern SemaphoreHandle_t dhcp_ready_sem;
extern struct netif      gnetif;
extern int ntp_get_time(time_msg *msg);

/* wait up to 30 s for DHCP – QEMU SLIRP is usually much faster */
#define DHCP_WAIT_TIMEOUT_MS    30000u

/* how long to wait when pushing to the queue before giving up */
#define QUEUE_SEND_TIMEOUT_MS   1000u

void ntp_task(void *param)
{
    (void)param;
    time_msg msg;

    /* block here until DHCP gives us an IP; semaphore is given by
     * netif_status_callback() in lwip_init.c once the address is up */
    printf("[NTP] Waiting for DHCP...\r\n");

    if (xSemaphoreTake(dhcp_ready_sem,
                       pdMS_TO_TICKS(DHCP_WAIT_TIMEOUT_MS)) == pdTRUE)
    {
        printf("[NTP] Network ready: %s\r\n",
               ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    }
    else
    {
        /* timed out – NTP will probably fail but let the user try anyway */
        printf("[NTP] DHCP timeout – continuing\r\n");
    }

    printf("[NTP] Press 't' to get current IST time\r\n");

    while (1)
    {
        xSemaphoreTake(time_request_sem, portMAX_DELAY);

        printf("[NTP] Fetching time...\r\n");

        if (ntp_get_time(&msg))
        {
            printf("[NTP] Got time: %02d:%02d:%02d IST\r\n",
                   msg.hour, msg.minute, msg.second);

            if (xQueueSend(time_queue, &msg,
                           pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) != pdTRUE)
            {
                printf("[NTP] Queue full, try again\r\n");
            }
        }
        else
        {
            printf("[NTP] Failed to get time\r\n");
        }
    }
}
