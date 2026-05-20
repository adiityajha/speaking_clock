/* stm32.ld
 * Linker script for QEMU mps2-an500 (Cortex-M7)
 *
 * Memory map (from QEMU hw/arm/mps2.c):
 *   ZBTSRAM1 alias : 0x00400000  4MB  (code / read-only)
 *   ZBTSRAM2&3     : 0x20000000  4MB  (data / stack / heap)
 *   LAN9118 eth    : 0xA0000000        (NOT at 0x40200000!)
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00400000, LENGTH = 4096K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}

SECTIONS
{
    /* vector table must be first in flash */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* code, read-only data, and exception unwind tables */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        /* ARM exception unwind tables – must be inside .text so that
         * __etext marks the true end of flash content and the correct
         * LMA start of .data (otherwise .ARM.exidx becomes an orphan
         * section placed BETWEEN .text and .data, shifting the LMA by
         * its size and breaking the Reset_Handler data-copy loop). */
        /* newlib init/fini stubs – must also be here */
        *(.init)
        *(.fini)
        *(.ARM.exidx*)
        *(.ARM.extab*)
        . = ALIGN(4);
        __etext = .;
    } > FLASH

    /* initialised data: LMA in flash, VMA in RAM */
    .data :
    {
        . = ALIGN(4);
        __data_start__ = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > FLASH

    /* zero-initialised data */
    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    /* stack: 8 KB, grows downward from __StackTop */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackLimit = .;
        . += 0x2000;
        __StackTop = .;
    } > RAM

    /* heap: 32 KB above stack */
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __HeapBase = .;
        . += 0x8000;
        __HeapLimit = .;
    } > RAM
}
