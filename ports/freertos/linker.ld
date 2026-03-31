/* SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com> */
/* SPDX-License-Identifier: BSD-3-Clause */

/* Linker script for ARM MPS2-AN385 (Cortex-M3) under QEMU.
 * The MPS2-AN385 in QEMU provides 4MB flash + 4MB RAM. */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

/* Initial stack pointer at top of RAM */
_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        *(.eh_frame)
        . = ALIGN(4);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Initialised data: stored in FLASH, copied to RAM at startup */
    .data : AT(_etext)
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    /* Zero-initialised data */
    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Heap starts after BSS */
    .heap :
    {
        . = ALIGN(8);
        _heap_start = .;
        . = . + 0x100;   /* minimal reservation; FreeRTOS heap_4 manages its own */
        _heap_end = .;
    } > RAM

    /* Stack at end of RAM (grows downward) */
    ._stack :
    {
        . = ALIGN(8);
        . = . + 0x2000;   /* 8KB initial stack */
        _stack_top = .;
    } > RAM
}
