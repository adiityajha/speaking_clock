/* syscalls.c
 * Newlib syscall stubs for bare-metal QEMU on mps2-an500.
 *
 * _write  → CMSDK APB UART0 MMIO (0x40004000) – output goes through QEMU's
 *            chardev layer and appears on stdout in -nographic mode, so it
 *            is captured by pipes (e.g. make qemu-tts).
 * _read   → not used (key_task polls UART MMIO directly)
 *
 * CMSDK APB UART registers:
 *   DATA  (RW, 0x000) – write byte to TX
 *   STATE (RO, 0x004) – bit 0 = TXBF (TX buffer full), bit 1 = RXBF
 *   CTRL  (RW, 0x008) – bit 0 = TXEN, bit 1 = RXEN
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

#define UART0_BASE   0x40004000UL
#define UART0_DATA   (*(volatile uint32_t *)(UART0_BASE + 0x000U))
#define UART0_STATE  (*(volatile uint32_t *)(UART0_BASE + 0x004U))
#define UART0_CTRL   (*(volatile uint32_t *)(UART0_BASE + 0x008U))
#define UART_TXBF    (1u << 0)   /* TX buffer full */
#define UART_TXEN    (1u << 0)   /* TX enable bit in CTRL */

/* Ensure UART TX is enabled (safe to call multiple times). */
static void uart_tx_enable(void)
{
    UART0_CTRL |= UART_TXEN;
}

/* Blocking single-character output via CMSDK UART MMIO. */
static void uart_putchar(char c)
{
    while (UART0_STATE & UART_TXBF);   /* spin while TX buffer full */
    UART0_DATA = (uint32_t)(uint8_t)c;
}

/* ---- _write : send bytes to UART0 ------------------------------------ */
int _write(int fd, const char *buf, int len)
{
    (void)fd;
    uart_tx_enable();
    for (int i = 0; i < len; i++) {
        uart_putchar(buf[i]);
    }
    return len;
}

/* ---- _read : not used (key_task polls MMIO directly) ----------------- */
int _read(int fd, char *buf, int len)
{
    (void)fd; (void)buf; (void)len;
    return -1;
}

/* ---- Minimal stubs ---------------------------------------------------- */
int _close(int fd)   { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int fd)  { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir) { (void)fd; (void)ptr; (void)dir; return 0; }
void _exit(int code) { (void)code; while (1); }
int _getpid(void)    { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }

/* sbrk – newlib heap is not used (FreeRTOS heap_4 handles allocation) */
void *_sbrk(int incr)
{
    extern char __HeapBase;
    extern char __HeapLimit;
    static char *heap_end = &__HeapBase;
    char *prev = heap_end;
    if (heap_end + incr > &__HeapLimit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return prev;
}
