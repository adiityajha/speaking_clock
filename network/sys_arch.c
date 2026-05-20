/* sys_arch.c
 * lwIP OS abstraction layer backed by FreeRTOS.
 * Implements the sys_* functions declared in lwip/sys.h.
 */

#include "lwip/sys.h"
#include "lwip/opt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* ---- Random number (used by DNS for port allocation) ----------------- */

unsigned int lwip_rand_func(void)
{
    static unsigned int seed = 123456789u;
    seed = seed * 1664525u + 1013904223u;   /* Numerical Recipes LCG */
    return seed;
}

/* ---- Time ------------------------------------------------------------- */

u32_t sys_now(void)
{
    return (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ---- Init ------------------------------------------------------------- */

void sys_init(void) { /* nothing */ }

/* ---- Mutex ------------------------------------------------------------ */

err_t sys_mutex_new(sys_mutex_t *m)
{
    *m = xSemaphoreCreateMutex();
    return (*m != NULL) ? ERR_OK : ERR_MEM;
}

void sys_mutex_lock(sys_mutex_t *m)
{
    xSemaphoreTake(*m, portMAX_DELAY);
}

void sys_mutex_unlock(sys_mutex_t *m)
{
    xSemaphoreGive(*m);
}

void sys_mutex_free(sys_mutex_t *m)
{
    vSemaphoreDelete(*m);
}

int sys_mutex_valid(sys_mutex_t *m)        { return (*m != NULL); }
void sys_mutex_set_invalid(sys_mutex_t *m) { *m = NULL; }

/* ---- Semaphore -------------------------------------------------------- */

err_t sys_sem_new(sys_sem_t *s, u8_t count)
{
    *s = xSemaphoreCreateCounting(0x7FFFFFFF, (UBaseType_t)count);
    if (*s == NULL)
    {
        *s = xSemaphoreCreateBinary();
        if (*s == NULL) return ERR_MEM;
        if (count > 0) xSemaphoreGive(*s);
    }
    return ERR_OK;
}

u32_t sys_arch_sem_wait(sys_sem_t *s, u32_t timeout_ms)
{
    TickType_t t0 = xTaskGetTickCount();
    TickType_t ticks = (timeout_ms == 0)
                       ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms);

    if (xSemaphoreTake(*s, ticks) == pdTRUE)
    {
        u32_t elapsed = (u32_t)((xTaskGetTickCount() - t0) * portTICK_PERIOD_MS);
        return elapsed;
    }
    return SYS_ARCH_TIMEOUT;
}

void sys_sem_signal(sys_sem_t *s)          { xSemaphoreGive(*s); }
void sys_sem_free(sys_sem_t *s)            { vSemaphoreDelete(*s); }
int  sys_sem_valid(sys_sem_t *s)           { return (*s != NULL); }
void sys_sem_set_invalid(sys_sem_t *s)     { *s = NULL; }

/* ---- Mailbox (message queue) ----------------------------------------- */

err_t sys_mbox_new(sys_mbox_t *mb, int size)
{
    *mb = xQueueCreate((UBaseType_t)size, sizeof(void *));
    return (*mb != NULL) ? ERR_OK : ERR_MEM;
}

void sys_mbox_post(sys_mbox_t *mb, void *msg)
{
    xQueueSend(*mb, &msg, portMAX_DELAY);
}

err_t sys_mbox_trypost(sys_mbox_t *mb, void *msg)
{
    return (xQueueSend(*mb, &msg, 0) == pdTRUE) ? ERR_OK : ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mb, void *msg)
{
    BaseType_t woken = pdFALSE;
    err_t e = (xQueueSendFromISR(*mb, &msg, &woken) == pdTRUE)
              ? ERR_OK : ERR_MEM;
    portYIELD_FROM_ISR(woken);
    return e;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mb, void **msg, u32_t timeout_ms)
{
    void *dummy;
    if (!msg) msg = &dummy;

    TickType_t t0    = xTaskGetTickCount();
    TickType_t ticks = (timeout_ms == 0)
                       ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(*mb, msg, ticks) == pdTRUE)
    {
        u32_t elapsed = (u32_t)((xTaskGetTickCount() - t0) * portTICK_PERIOD_MS);
        return elapsed;
    }
    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mb, void **msg)
{
    void *dummy;
    if (!msg) msg = &dummy;
    return (xQueueReceive(*mb, msg, 0) == pdTRUE) ? 0 : SYS_MBOX_EMPTY;
}

void sys_mbox_free(sys_mbox_t *mb)            { vQueueDelete(*mb); }
int  sys_mbox_valid(sys_mbox_t *mb)           { return (*mb != NULL); }
void sys_mbox_set_invalid(sys_mbox_t *mb)     { *mb = NULL; }

/* ---- Thread ----------------------------------------------------------- */

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn fn,
                             void *arg, int stacksize, int prio)
{
    TaskHandle_t h = NULL;
    xTaskCreate(fn, name, (uint16_t)stacksize, arg, (UBaseType_t)prio, &h);
    return h;
}

/* ---- Critical section ------------------------------------------------- */

sys_prot_t sys_arch_protect(void)
{
    taskENTER_CRITICAL();
    return 0;
}

void sys_arch_unprotect(sys_prot_t p)
{
    (void)p;
    taskEXIT_CRITICAL();
}
