/* startup_stm32.s
 * Startup for QEMU mps2-an500 (Cortex-M7)
 *
 * Stack and heap memory is reserved by the linker script (.stack/.heap sections
 * with '. += N').  We only declare the symbols as externals here so the vector
 * table can reference __StackTop.  Do NOT use .space/.section .stack here –
 * that creates orphan sections placed at address 0x00000000 in the ELF.
 */

    .syntax unified
    .cpu cortex-m7
    .thumb

    .globl __StackTop
    .globl __StackLimit
    .globl __HeapBase
    .globl __HeapLimit

/* ---- Vector table ---------------------------------------------------- */
    .section .isr_vector, "a"
    .align 2
    .globl __isr_vector
__isr_vector:
    .long   __StackTop              /* initial SP */
    .long   Reset_Handler           /* Reset */
    .long   NMI_Handler
    .long   HardFault_Handler
    .long   MemManage_Handler
    .long   BusFault_Handler
    .long   UsageFault_Handler
    .long   0
    .long   0
    .long   0
    .long   0
    .long   SVC_Handler             /* FreeRTOS SVCall */
    .long   DebugMon_Handler
    .long   0
    .long   PendSV_Handler          /* FreeRTOS context switch */
    .long   SysTick_Handler         /* FreeRTOS tick */

    /* External interrupts (only entries needed by LAN9118 IRQ13) */
    .long   Default_Handler         /*  0 WWDG      */
    .long   Default_Handler         /*  1 PVD       */
    .long   Default_Handler         /*  2 TAMP      */
    .long   Default_Handler         /*  3 RTC_WKUP  */
    .long   Default_Handler         /*  4 FLASH     */
    .long   Default_Handler         /*  5 RCC       */
    .long   Default_Handler         /*  6 EXTI0     */
    .long   Default_Handler         /*  7 EXTI1     */
    .long   Default_Handler         /*  8 EXTI2     */
    .long   Default_Handler         /*  9 EXTI3     */
    .long   Default_Handler         /* 10 EXTI4     */
    .long   Default_Handler         /* 11           */
    .long   Default_Handler         /* 12           */
    .long   Default_Handler         /* 13 LAN9118   */

    .size __isr_vector, . - __isr_vector

/* ---- Reset handler --------------------------------------------------- */
    .text
    .thumb_func
    .align 2
    .globl  Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* copy .data from flash (LMA = __etext) to RAM (VMA = __data_start__) */
    ldr     r1, =__etext
    ldr     r2, =__data_start__
    ldr     r3, =__data_end__
    subs    r3, r2
    ble     .L_zero_bss

.L_copy_data:
    subs    r3, #4
    ldr     r0, [r1, r3]
    str     r0, [r2, r3]
    bgt     .L_copy_data

    /* zero .bss */
.L_zero_bss:
    ldr     r1, =__bss_start__
    ldr     r2, =__bss_end__
    movs    r0, #0

.L_clear_bss:
    cmp     r1, r2
    bge     .L_call_main
    str     r0, [r1]
    adds    r1, #4
    b       .L_clear_bss

.L_call_main:
    bl      main
    b       .          /* should not return */

    .size Reset_Handler, . - Reset_Handler

/* ---- Default weak handlers ------------------------------------------- */
    .macro  def_handler  name
    .align  1
    .thumb_func
    .weak   \name
    .type   \name, %function
\name:
    b       .
    .size   \name, . - \name
    .endm

    def_handler  NMI_Handler
    def_handler  HardFault_Handler
    def_handler  MemManage_Handler
    def_handler  BusFault_Handler
    def_handler  UsageFault_Handler
    def_handler  SVC_Handler
    def_handler  DebugMon_Handler
    def_handler  PendSV_Handler
    def_handler  SysTick_Handler
    def_handler  Default_Handler

    .end
