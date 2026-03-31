/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * PHL on FreeRTOS: main application.
 *
 * Phase 1: Runs PHP test scripts and verifies output (CI test).
 * Phase 2: Initializes networking and starts an HTTP server (demo).
 */
#include "phl_port.h"

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS_IP.h"

#include <stdio.h>
#include <string.h>

/* Defined in http.c */
extern void vHTTPServerTask(void *pvParameters);

/* Semihosting exit: cleanly terminate QEMU */
static void semihosting_exit(int code)
{
	register uint32_t r0 __asm__("r0") = 0x18; /* angel_SWIreason_ReportException */
	register uint32_t r1 __asm__("r1") = (code == 0) ? 0x20026 : 0x20023;
	/* ADP_Stopped_ApplicationExit / ADP_Stopped_RunTimeErrorUnknown */
	__asm__ volatile("bkpt 0xab" : : "r"(r0), "r"(r1));
	(void)code;
	for(;;);
}

/* ---------- Phase 1: PHP Execution Tests ---------- */

typedef struct {
	const char *zName;
	const char *zScript;
	const char *zExpected;
} TestCase;

static const TestCase aTests[] = {
	{
		"hello_world",
		"<?php echo 'Hello from PHL on FreeRTOS!'; ?>",
		"Hello from PHL on FreeRTOS!"
	},
	{
		"arithmetic",
		"<?php $a = 7; $b = 6; echo $a * $b; ?>",
		"42"
	},
	{
		"fizzbuzz",
		"<?php\n"
		"for ($i = 1; $i <= 15; $i++) {\n"
		"    if ($i % 15 == 0) echo 'FizzBuzz';\n"
		"    else if ($i % 3 == 0) echo 'Fizz';\n"
		"    else if ($i % 5 == 0) echo 'Buzz';\n"
		"    else echo $i;\n"
		"    echo ',';\n"
		"}\n"
		"?>",
		"1,2,Fizz,4,Buzz,Fizz,7,8,Fizz,Buzz,11,Fizz,13,14,FizzBuzz,"
	},
	{
		"array_foreach",
		"<?php\n"
		"$fruits = ['apple', 'banana', 'cherry'];\n"
		"foreach ($fruits as $f) {\n"
		"    echo $f . ' ';\n"
		"}\n"
		"?>",
		"apple banana cherry "
	},
	{
		"fibonacci",
		"<?php\n"
		"function fib($n) {\n"
		"    if ($n <= 1) return $n;\n"
		"    return fib($n - 1) + fib($n - 2);\n"
		"}\n"
		"echo fib(10);\n"
		"?>",
		"55"
	},
};

#define NUM_TESTS (sizeof(aTests) / sizeof(aTests[0]))

static void vTestTask(void *pvParameters)
{
	char zBuf[2048];
	unsigned int nLen;
	int nPassed = 0;
	int nFailed = 0;
	unsigned int i;

	(void)pvParameters;

	printf("=== PHL FreeRTOS Test Suite ===\n");

	for( i = 0; i < NUM_TESTS; i++ ){
		nLen = 0;
		int rc = phl_port_exec(aTests[i].zScript, zBuf, sizeof(zBuf), &nLen);
		if( rc != PH7_OK ){
			printf("FAIL [%s]: compile/exec error %d\n", aTests[i].zName, rc);
			nFailed++;
		} else if( strcmp(zBuf, aTests[i].zExpected) != 0 ){
			printf("FAIL [%s]: expected '%s', got '%s'\n",
				aTests[i].zName, aTests[i].zExpected, zBuf);
			nFailed++;
		} else {
			printf("PASS [%s]\n", aTests[i].zName);
			nPassed++;
		}
	}

	printf("=== Results: %d passed, %d failed ===\n", nPassed, nFailed);

	if( nFailed == 0 ){
		printf("ALL TESTS PASSED\n");
	} else {
		printf("TESTS FAILED\n");
		semihosting_exit(1);
	}

	/* Tests passed — now start the HTTP server demo if networking is available */
	printf("Starting HTTP server demo...\n");

	/* Network configuration from FreeRTOSIPConfig.h */
	{
		static const uint8_t ucIPAddress[4]      = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
		static const uint8_t ucNetMask[4]        = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
		static const uint8_t ucGatewayAddress[4] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
		static const uint8_t ucDNSServerAddress[4] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };
		static const uint8_t ucMACAddress[6]     = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };

		/* Set Ethernet IRQ (13) priority for FreeRTOS compatibility */
		volatile uint8_t *pPriority = ((volatile uint8_t *)0xE000E400UL) + 13;
		*pPriority = (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS));

		FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress,
			ucDNSServerAddress, ucMACAddress);
	}

	/* This task is done — delete itself. HTTP server starts from network event hook. */
	vTaskDelete(NULL);
}

/* ---------- FreeRTOS-Plus-TCP Hooks ---------- */

BaseType_t xApplicationGetRandomNumber(uint32_t *pulNumber)
{
	static uint32_t ulSeed = 0x12345678UL;
	ulSeed ^= ulSeed << 13;
	ulSeed ^= ulSeed >> 17;
	ulSeed ^= ulSeed << 5;
	*pulNumber = ulSeed;
	return pdTRUE;
}

uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
                                            uint16_t usSourcePort,
                                            uint32_t ulDestinationAddress,
                                            uint16_t usDestinationPort)
{
	uint32_t ulReturn;
	(void)ulSourceAddress; (void)usSourcePort;
	(void)ulDestinationAddress; (void)usDestinationPort;
	xApplicationGetRandomNumber(&ulReturn);
	return ulReturn;
}

void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent)
{
	if( eNetworkEvent == eNetworkUp ){
		printf("[NET] Network is up\n");
		xTaskCreate(vHTTPServerTask, "HTTP",
			configMINIMAL_STACK_SIZE * 32, NULL,
			tskIDLE_PRIORITY + 2, NULL);
	}
}

/* ---------- FreeRTOS Hooks ---------- */

void vAssertCalled(const char *file, int line)
{
	taskDISABLE_INTERRUPTS();
	printf("[ASSERT] %s:%d\n", file, line);
	for(;;);
}

void vApplicationMallocFailedHook(void)
{
	printf("[FATAL] Malloc failed!\n");
	for(;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	(void)xTask;
	printf("[FATAL] Stack overflow in task: %s\n", pcTaskName);
	for(;;);
}

/* ---------- Main ---------- */

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("PHL on FreeRTOS - HTTP Server Demo\n");

	if( phl_port_init() != PH7_OK ){
		printf("[FATAL] PHL port init failed\n");
		semihosting_exit(1);
	}

	/* Create the test task — it runs PHP tests, then starts networking */
	xTaskCreate(vTestTask, "TEST",
		configMINIMAL_STACK_SIZE * 16, NULL,
		tskIDLE_PRIORITY + 3, NULL);

	vTaskStartScheduler();
	return 0;
}
