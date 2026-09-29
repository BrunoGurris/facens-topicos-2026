/*
 * Configuracao do FreeRTOS para o controlador de AMV (STM32C031C6, Cortex-M0+).
 *
 * Escolhas principais:
 *  - Preemptivo com prioridades fixas (tarefa pronta de maior prioridade roda).
 *  - Tick de 1 ms: e' a base de tempo de vTaskDelayUntil() e dos timeouts.
 *  - Somente alocacao estatica: nenhum malloc depois do boot, o uso de RAM
 *    fica todo visivel no .map (importante em sistema de seguranca).
 *  - O SysTick pertence ao FreeRTOS; o HAL_GetTick() le o tick do kernel
 *    (ver amv_hw.c).
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#if defined(__GNUC__) && !defined(__ASSEMBLER__)
#include <stdint.h>
extern uint32_t SystemCoreClock;
void amv_assert_failed(const char *file, int line);
#endif

/* ---- Escalonador ---------------------------------------------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0   /* M0+ nao tem CLZ */
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    6
#define configMINIMAL_STACK_SIZE                80      /* palavras (320 B) */
#define configMAX_TASK_NAME_LEN                 10
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1

/* ---- Memoria -------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configKERNEL_PROVIDED_STATIC_MEMORY     1   /* idle task: memoria do kernel */

/* ---- Recursos usados ------------------------------------------------ */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TASK_NOTIFICATIONS            1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIMERS                        0
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE /* exigido pelo kernel; sem timers o --gc-sections descarta */
#define configUSE_CO_ROUTINES                   0
#define configUSE_EVENT_GROUPS                  0
#define configUSE_STREAM_BUFFERS                0

/* ---- Hooks e diagnostico -------------------------------------------- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configRECORD_STACK_HIGH_ADDRESS         0

/* ---- Cortex-M0 port ------------------------------------------------- */
#define configENABLE_MPU                        0
#define configCHECK_HANDLER_INSTALLATION        0   /* Wokwi nao simula leitura do VTOR */

/* ---- API incluida --------------------------------------------------- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0

#define configASSERT( x ) \
    do { if( ( x ) == 0 ) { amv_assert_failed( __FILE__, __LINE__ ); } } while( 0 )

#endif /* FREERTOS_CONFIG_H */
