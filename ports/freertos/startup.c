/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Minimal Cortex-M3 startup for MPS2-AN385 (QEMU).
 * Provides vector table, Reset_Handler, and default fault handlers.
 */
#include <stdint.h>
#include <string.h>

/* Symbols from linker script */
extern uint32_t _estack;
extern uint32_t _etext;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

/* Main application entry (not the C library main) */
extern int main(void);

/* libc init */
extern void __libc_init_array(void);

void Reset_Handler(void)
{
	uint32_t *pSrc, *pDst;

	/* Copy .data from flash to RAM */
	pSrc = &_etext;
	pDst = &_sdata;
	while( pDst < &_edata ){
		*pDst++ = *pSrc++;
	}
	/* Zero .bss */
	pDst = &_sbss;
	while( pDst < &_ebss ){
		*pDst++ = 0;
	}

	__libc_init_array();
	main();
	for(;;);
}

void Default_Handler(void)
{
	for(;;);
}

void NMI_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)     __attribute__((weak, alias("Default_Handler")));

/* FreeRTOS kernel handlers — linked directly in vector table below */
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

/* MPS2-AN385 has 48 external interrupts; pad vector table */
void UART0_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void UART1_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void TIMER0_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void TIMER1_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void GPIO0_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void GPIO1_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void GPIO2_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void GPIO3_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void MPS2_SPI0_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MPS2_SPI1_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MPS2_SPI2_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MPS2_SPI3_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MPS2_SPI4_Handler(void)    __attribute__((weak, alias("Default_Handler")));
/* EthernetISR is defined by FreeRTOS-Plus-TCP NetworkInterface.c */
extern void EthernetISR(void);
void ETHERNET_Handler(void) { EthernetISR(); }

__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
	(void (*)(void))&_estack,   /* Initial Stack Pointer */
	Reset_Handler,              /* Reset */
	NMI_Handler,                /* NMI */
	HardFault_Handler,          /* Hard Fault */
	MemManage_Handler,          /* MPU Fault */
	BusFault_Handler,           /* Bus Fault */
	UsageFault_Handler,         /* Usage Fault */
	0, 0, 0, 0,                /* Reserved */
	vPortSVCHandler,            /* SVCall — FreeRTOS */
	DebugMon_Handler,           /* Debug Monitor */
	0,                          /* Reserved */
	xPortPendSVHandler,         /* PendSV — FreeRTOS */
	xPortSysTickHandler,        /* SysTick — FreeRTOS */
	/* External Interrupts: IRQ 0-13+ */
	UART0_Handler,              /* IRQ 0 */
	UART1_Handler,              /* IRQ 1 */
	TIMER0_Handler,             /* IRQ 2 */
	TIMER1_Handler,             /* IRQ 3 */
	GPIO0_Handler,              /* IRQ 4 */
	GPIO1_Handler,              /* IRQ 5 */
	GPIO2_Handler,              /* IRQ 6 */
	GPIO3_Handler,              /* IRQ 7 */
	MPS2_SPI0_Handler,          /* IRQ 8 */
	MPS2_SPI1_Handler,          /* IRQ 9 */
	MPS2_SPI2_Handler,          /* IRQ 10 */
	MPS2_SPI3_Handler,          /* IRQ 11 */
	MPS2_SPI4_Handler,          /* IRQ 12 */
	ETHERNET_Handler,           /* IRQ 13: LAN9118 */
};
