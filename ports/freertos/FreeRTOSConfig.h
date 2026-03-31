/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M3 core configuration */
#define configCPU_CLOCK_HZ                   ( 25000000UL )  /* MPS2-AN385 QEMU */
#define configTICK_RATE_HZ                   ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                 ( 7 )
#define configMINIMAL_STACK_SIZE             ( ( unsigned short ) 256 )
#define configTOTAL_HEAP_SIZE                ( ( size_t ) ( 512 * 1024 ) )
#define configMAX_TASK_NAME_LEN              ( 16 )
#define configUSE_16_BIT_TICKS               0
#define configIDLE_SHOULD_YIELD              1
#define configUSE_PREEMPTION                 1

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION      0
#define configSUPPORT_DYNAMIC_ALLOCATION     1

/* Hook functions */
#define configUSE_IDLE_HOOK                  0
#define configUSE_TICK_HOOK                  0
#define configUSE_MALLOC_FAILED_HOOK         1
#define configCHECK_FOR_STACK_OVERFLOW       2

/* Software timer */
#define configUSE_TIMERS                     1
#define configTIMER_TASK_PRIORITY            ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH             5
#define configTIMER_TASK_STACK_DEPTH         ( configMINIMAL_STACK_SIZE * 2 )

/* Semaphores (required by FreeRTOS-Plus-TCP) */
#define configUSE_COUNTING_SEMAPHORES        1
#define configUSE_MUTEXES                    1
#define configUSE_RECURSIVE_MUTEXES          1
#define configQUEUE_REGISTRY_SIZE            0

/* Co-routine (unused) */
#define configUSE_CO_ROUTINES                0
#define configMAX_CO_ROUTINE_PRIORITIES      ( 2 )

/* API includes */
#define INCLUDE_vTaskPrioritySet             1
#define INCLUDE_uxTaskPriorityGet            1
#define INCLUDE_vTaskDelete                  1
#define INCLUDE_vTaskCleanUpResources        0
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_vTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1

/* Cortex-M3 interrupt priorities */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS 3
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         0x07
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    0x05
#define configKERNEL_INTERRUPT_PRIORITY                 ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY            ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* Assert — print location before halting */
void vAssertCalled(const char *file, int line);
#define configASSERT( x ) if( ( x ) == 0 ) { vAssertCalled(__FILE__, __LINE__); }

#endif /* FREERTOS_CONFIG_H */
