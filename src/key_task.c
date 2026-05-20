/* key_task.c
 * Polls UART0 every 100 ms. When 't' is pressed, signals ntp_task
 * via time_request_sem.
 *
 * Non-blocking poll lets other tasks run while waiting for input.
 * CMSDK APB UART0 on mps2-an500 is at 0x40004000.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>
#include <stdio.h>

extern SemaphoreHandle_t time_request_sem;

/* CMSDK APB UART0 registers */
#define UART0_BASE       0x40004000UL
#define UART0_DATA       ( *(volatile uint32_t *)(UART0_BASE + 0x00) )
#define UART0_STATE      ( *(volatile uint32_t *)(UART0_BASE + 0x04) )
#define UART0_CTRL       ( *(volatile uint32_t *)(UART0_BASE + 0x08) )

#define UART_CTRL_TX_EN  (1u << 0)
#define UART_CTRL_RX_EN  (1u << 1)
#define UART_STATE_RX_BF (1u << 1)   /* set when a byte is waiting */

static void uart_init(void)
{
    UART0_CTRL = UART_CTRL_TX_EN | UART_CTRL_RX_EN;
}

/* returns char or -1 if nothing ready */
static int uart_getchar_nb(void)
{
    if (UART0_STATE & UART_STATE_RX_BF)
        return (int)(UART0_DATA & 0xFFu);
    return -1;
}

void key_task(void *param)
{
    (void)param;
    uart_init();
    printf("[KEY] Press 't' to speak current IST time\r\n");

    while (1)
    {
        int c = uart_getchar_nb();
        if (c == 't' || c == 'T')
        {
            printf("[KEY] Time request received\r\n");
            xSemaphoreGive(time_request_sem);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
