/*
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PHL on FreeRTOS: Embedding Example
 * ===================================
 *
 * This example shows how to embed the PHL (PH7) engine inside a FreeRTOS
 * application. It demonstrates:
 *
 *   1. Registering a custom memory allocator backed by pvPortMalloc/vPortFree
 *   2. Compiling a PHP script from a C string (no filesystem required)
 *   3. Capturing VM output via a callback
 *   4. Running the interpreter inside a FreeRTOS task
 *
 * This file is a reference for porting PHL to your own FreeRTOS board.
 * It is NOT compiled by the CI — adapt it to your platform.
 *
 * Build requirements:
 *   - PHL engine source (src/sx/ and src/ph7/)
 *   - FreeRTOS Kernel (tasks.c, queue.c, list.c, portable/GCC/<your_port>)
 *   - Compile with: -DOS_OTHER -DPH7_DISABLE_DISK_IO
 *   - Optionally: -DPH7_OMIT_FLOATING_POINT -DPH7_DISABLE_BUILTIN_FUNC
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "ph7.h"

/* ---------- Step 1: Custom Memory Allocator ----------
 *
 * FreeRTOS heap_4 (and most FreeRTOS heap implementations) do not provide
 * realloc(). PHL's SyMemMethods vtable requires xRealloc, so we implement
 * it by prefixing each allocation with its size.
 */
static void *my_alloc(unsigned int nByte)
{
	unsigned int *p = (unsigned int *)pvPortMalloc(nByte + sizeof(unsigned int));
	if( p ){
		*p = nByte;
		return (char *)p + sizeof(unsigned int);
	}
	return 0;
}

static void my_free(void *ptr)
{
	if( ptr ){
		vPortFree((char *)ptr - sizeof(unsigned int));
	}
}

static void *my_realloc(void *pOld, unsigned int nByte)
{
	void *pNew;
	unsigned int nOld = 0;
	if( pOld ){
		nOld = *((unsigned int *)pOld - 1);
	}
	pNew = my_alloc(nByte);
	if( pNew && pOld ){
		memcpy(pNew, pOld, nOld < nByte ? nOld : nByte);
		my_free(pOld);
	}
	return pNew;
}

static unsigned int my_chunk_size(void *ptr)
{
	if( ptr ){
		return *((unsigned int *)ptr - 1);
	}
	return 0;
}

static const SyMemMethods my_mem_methods = {
	my_alloc,      /* xAlloc */
	my_realloc,    /* xRealloc */
	my_free,       /* xFree */
	my_chunk_size, /* xChunkSize (required!) */
	0,             /* xInit (optional) */
	0,             /* xRelease (optional) */
	0              /* pUserData (optional) */
};

/* ---------- Step 2: Output Consumer ----------
 *
 * PHL calls this function each time the VM produces output (echo, print).
 * Here we simply write to stdout (via whatever _write syscall your board
 * provides — semihosting, UART, etc.).
 */
static int output_consumer(const void *pOutput, unsigned int nLen, void *pUserData)
{
	(void)pUserData;
	printf("%.*s", nLen, (const char *)pOutput);
	return PH7_OK;
}

/* ---------- Step 3: The PHP Script ---------- */
static const char php_script[] =
	"<?php\n"
	"echo 'Hello from PHL on FreeRTOS!' . PHP_EOL;\n"
	"$a = 2; $b = 3;\n"
	"echo $a . ' + ' . $b . ' = ' . ($a + $b) . PHP_EOL;\n"
	"?>";

/* ---------- Step 4: FreeRTOS Task ---------- */
static void phl_task(void *pvParameters)
{
	ph7 *pEngine;
	ph7_vm *pVm;
	int rc;

	(void)pvParameters;

	printf("Starting PHL engine...\n");

	/* Register the custom allocator BEFORE calling ph7_init() */
	rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC, &my_mem_methods);
	if( rc != PH7_OK ){
		printf("Failed to register allocator: %d\n", rc);
		vTaskDelete(NULL);
		return;
	}

	rc = ph7_init(&pEngine);
	if( rc != PH7_OK ){
		printf("ph7_init failed: %d\n", rc);
		vTaskDelete(NULL);
		return;
	}

	/* Compile the script from a string — no filesystem needed */
	rc = ph7_compile_v2(pEngine, php_script, -1, &pVm, 0);
	if( rc != PH7_OK ){
		printf("Compile failed: %d\n", rc);
		ph7_release(pEngine);
		vTaskDelete(NULL);
		return;
	}

	/* Install the output consumer */
	ph7_vm_config(pVm, PH7_VM_CONFIG_OUTPUT, output_consumer, 0);

	/* Execute */
	ph7_vm_exec(pVm, 0);

	/* Cleanup */
	ph7_vm_release(pVm);
	ph7_release(pEngine);

	printf("PHL task complete.\n");
	vTaskDelete(NULL);
}

/* ---------- main ----------
 *
 * Create the PHL task and start the FreeRTOS scheduler.
 * Adapt the stack size to your board's available RAM.
 */
int main(void)
{
	xTaskCreate(phl_task, "PHL",
		4096,         /* Stack depth in words — adjust for your board */
		NULL,         /* Task parameter */
		tskIDLE_PRIORITY + 1,
		NULL);

	vTaskStartScheduler();

	/* Should never reach here */
	for(;;);
	return 0;
}
