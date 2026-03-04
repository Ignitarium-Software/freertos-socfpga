/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * A sample hello world application
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "socfpga_console.h"
#include "socfpga_interrupt.h"
#include "socfpga_gic_reg.h"
#include "socfpga_interrupt.h"
#include "socfpga_cache.h"
#include "portmacro.h"

#define CORE_0_AFFINITY    0U
#define CORE_1_AFFINITY    256U
#define CORE_2_AFFINITY    512U
#define CORE_3_AFFINITY    768U

uint64_t core_aff;
uint64_t check_val = 0;
static void setup_hardware( void )
{
    /* Initialize the GIC. */
    interrupt_init_gic();

    #if configENABLE_CONSOLE_UART
        /* Initialize the console uart*/
        console_init( configCONSOLE_UART_ID, "115200-8N1" );
    #endif
}
/*-----------------------------------------------------------*/
void run_hello_world( void * )
{
    uint8_t processor_name;
    uint32_t core_aff = gic_reg_get_cpu_affinity()/256;
    if( core_aff == CORE_0_AFFINITY || core_aff == CORE_1_AFFINITY)
    {
        processor_name = 55;
    }
    else if( core_aff == CORE_2_AFFINITY || core_aff == CORE_3_AFFINITY )
    {
        processor_name = 76;
    }
    do
    {
        core_aff = gic_reg_get_cpu_affinity()/256;
        printf( "\r\nhello world from A%d,Core ID = %d\n", processor_name, core_aff);
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }while( 1 );
}
void run_hello_world_secondary( void * )
{
    uint8_t processor_name;
    uint32_t core_aff = gic_reg_get_cpu_affinity()/256;
    if( core_aff == CORE_0_AFFINITY || core_aff == CORE_1_AFFINITY)
    {
        processor_name = 55;
    }
    else if( core_aff == CORE_2_AFFINITY || core_aff == CORE_3_AFFINITY )
    {
        processor_name = 76;
    }
    do
    {
        core_aff = gic_reg_get_cpu_affinity()/256;
        printf( "\rhello world 2 from A%d,Core ID = %d\n", processor_name, core_aff);
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }while( 1 );
}
/*-----------------------------------------------------------*/

int main( void )
{
BaseType_t xReturn;

    setup_hardware();

    xReturn = xTaskCreate( run_hello_world, "hello_world", configMINIMAL_STACK_SIZE,
                           NULL, configMAX_PRIORITIES - 1, NULL );

    xReturn = xTaskCreate( run_hello_world_secondary, "hello_world_secondary", configMINIMAL_STACK_SIZE,
                          NULL, configMAX_PRIORITIES - 1, NULL );

    if( xReturn == 1 )
    {
        vTaskStartScheduler();
    }

    return 0;
}
/*-----------------------------------------------------------*/


void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
     to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
     task.  It is essential that code added to this hook function never attempts
     to block in any way (for example, call xQueueReceive() with a block time
     specified, or call vTaskDelay()).  If the application makes use of the
     vTaskDelete() API function (as this demo application does) then it is also
     important that vApplicationIdleHook() is permitted to return to its calling
     function, because it is the responsibility of the idle task to clean up
     memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    /* This is called from RTOS tick handler
       Not used in this demo, But defined to keep the configuration sharing
       simple */
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
     configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
     function that will get called if a call to pvPortMalloc() fails.
     pvPortMalloc() is called internally by the kernel whenever a task, queue,
     timer or semaphore is created.  It is also called by various parts of the
     demo application.  If heap_1.c or heap_2.c are used, then the size of the
     heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
     FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
     to query the size of free heap space that remains (although it does not
     provide information on how the remaining heap might be fragmented). */
    taskDISABLE_INTERRUPTS();

    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/
