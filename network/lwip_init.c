/* lwip_init.c
 * Initialise the lwIP + DHCP stack and register the DHCP status callback.
 *
 * IMPORTANT: this function is called from main() BEFORE vTaskStartScheduler().
 * Do NOT call vTaskDelay() here – pxCurrentTCB is NULL before the scheduler
 * starts, so vTaskDelay() dereferences a NULL pointer and hard-faults.
 *
 * tcpip_init() is safe: it creates the tcpip_thread task (queued to start
 * later) and returns immediately without any blocking.
 *
 * DHCP readiness signalling:
 *   When DHCP assigns a valid IP address, lwIP calls the netif status
 *   callback registered here.  The callback gives dhcp_ready_sem, which
 *   wakes ntp_task reliably without any polling race condition.
 *   This replaces the previous poll-loop approach that could print a
 *   spurious "DHCP timeout" message even when the address was assigned.
 */

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"
#include "netif/etharp.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>

struct netif gnetif;

/* Shared with main.c and ntp_task.c */
extern SemaphoreHandle_t dhcp_ready_sem;

extern err_t qemu_netif_init(struct netif *netif);

/* ---- DHCP / netif status callback ------------------------------------ */
/* Called by lwIP's tcpip_thread whenever the netif status changes.
 * We give dhcp_ready_sem exactly once when a non-zero IPv4 address is set,
 * which wakes ntp_task without any polling or race condition. */
static void netif_status_callback(struct netif *netif)
{
    BaseType_t woken = pdFALSE;

    if (!ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        printf("[LWIP] DHCP assigned: %s\r\n",
               ip4addr_ntoa(netif_ip4_addr(netif)));
        /* Give from a lwIP callback (runs in tcpip_thread context, which is
         * a task, not an ISR) so we use the regular xSemaphoreGive. */
        (void)woken;
        xSemaphoreGive(dhcp_ready_sem);
    }
}

/* ---- network_init ---------------------------------------------------- */
void network_init(void)
{
    ip4_addr_t ipaddr, netmask, gw;

    /* Start with all-zeros: DHCP will fill these in */
    IP4_ADDR(&ipaddr,  0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw,      0, 0, 0, 0);

    /* Creates tcpip_thread internally – non-blocking, safe before scheduler */
    tcpip_init(NULL, NULL);

    netif_add(&gnetif, &ipaddr, &netmask, &gw,
              NULL, qemu_netif_init, tcpip_input);

    netif_set_default(&gnetif);
    netif_set_up(&gnetif);

    /* Register status callback BEFORE starting DHCP so we never miss the
     * address-assigned notification. */
    netif_set_status_callback(&gnetif, netif_status_callback);

    dhcp_start(&gnetif);   /* DHCP runs asynchronously in tcpip_thread */

    printf("[LWIP] DHCP started – waiting for address assignment\r\n");
}
