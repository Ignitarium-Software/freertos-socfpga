/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Common entry function for all sample apps
 */


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "socfpga_interrupt.h"
#include "socfpga_console.h"
#include "socfpga_smmu.h"

/* Select which I2C sample task(s) to run. */
#define I2C_SAMPLE_ENABLE_PIO       1
#define I2C_SAMPLE_ENABLE_DMA       0
#define I2C_SAMPLE_ENABLE_SLAVE     0
#define I2C_SAMPLE_ENABLE_SLAVE_DMA 0

#define TASK_PRIORITY    (configMAX_PRIORITIES - 2)

void run_samples( void *arg );
void i2c_task( void );
void i2c_dma_task( void );
void i2c_slave_task( void );
void i2c_slave_dma_task( void );

void vApplicationTickHook( void )
{
    /*
     * This is called from RTOS tick handler
     * Not used in this demo, But defined to keep the configuration sharing
     * simple
     * */
}

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
    for ( ;; )
        ;
}

void samples_main()
{
    BaseType_t xReturn;

    xReturn = xTaskCreate(run_samples, "Run_Samples", configMINIMAL_STACK_SIZE,
            NULL, TASK_PRIORITY, NULL);
    if (xReturn == 1)
    {
        vTaskStartScheduler();
    }

}

void run_samples( void *arg )
{
    (void) arg;

#if I2C_SAMPLE_ENABLE_PIO
    i2c_task();
#endif
#if I2C_SAMPLE_ENABLE_DMA
    i2c_dma_task();
#endif
#if I2C_SAMPLE_ENABLE_SLAVE
    i2c_slave_task();
#endif
#if I2C_SAMPLE_ENABLE_SLAVE_DMA
    i2c_slave_dma_task();
#endif

    vTaskSuspend(NULL);
}

static void prvSetupHardware( void )
{
    /* Initialize the GIC. */
    interrupt_init_gic();

    /* Enable SMMU */
    (void)smmu_enable();

    /* Initialize the console uart*/
#if configENABLE_CONSOLE_UART
    console_init(configCONSOLE_UART_ID, "115200-8N1");
#endif
}

void vApplicationIdleHook( void )
{
}


int main( void )
{
    prvSetupHardware();

    samples_main();

    /*Block here indefinitely; Should never reach here*/
    while ( 1 )
    {
    }
}
