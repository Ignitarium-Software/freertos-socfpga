/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Multithreaded application to run different sample apps together
 */

/**
 * @file main.c
 * @brief Multi-threaded sample
 */

/**
 * @defgroup multi_thread_sample Multi-thread App
 * @ingroup samples
 *
 * Multi-thread Sample Application
 *
 * @details
 * @section trd_desc Description
 * This application demonstrates how to run multiple individual sample applications in parallel. By default,
 * dma sample, qspi sample and fatfs sample is specified in this application.
 * You can specify up to three different sample applications using the following macros:
 * - `SAMPLE_APP1`: Selects the first sample application.
 * - `SAMPLE_APP2`: Selects the second sample application.
 * - `SAMPLE_APP3`: Selects the third sample application.
 *
 * @section trd_pre Prerequisites
 * - Make sure that each selected sample application satisfies the prerequisites listed in its respective documentation.
 *
 * @section trd_howto How to Run
 * 1. Follow the common README for build and flashing instructions.
 * 2. Run the sample.
 *
 * @section trd_res Expected Results
 * - The console log will show successful completion of all the three samples
 *
 * @{
 */
/** @} */


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "socfpga_interrupt.h"
#include "socfpga_console.h"
#include "socfpga_smmu.h"
#include "osal_log.h"

#define TASK_PRIORITY    (configMAX_PRIORITIES - 2)

void dma_task(void);
void qspi_task(void);
void fatfs_task(void);

void run_sample1(void *arg);
void run_sample2(void *arg);
void run_sample3(void *arg);

#define SAMPLE_APP1    dma_task
#define SAMPLE_APP2    fatfs_task
#define SAMPLE_APP3    qspi_task

void vApplicationTickHook(void)
{
    /*
     * This is called from RTOS tick handler
     * Not used in this demo, But defined to keep the configuration sharing
     * simple
     * */
}

void vApplicationMallocFailedHook(void)
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
    for ( ;;)
        ;
}

void samples_main()
{
    BaseType_t xReturn;

    xReturn = xTaskCreate(run_sample1, "samples1", configMINIMAL_STACK_SIZE,
            NULL, TASK_PRIORITY, NULL);
    if (xReturn != pdPASS)
    {
        ERROR("Failed to create task");
        return;
    }

    xReturn = xTaskCreate(run_sample2, "samples2", configMINIMAL_STACK_SIZE,
            NULL, TASK_PRIORITY, NULL);
    if (xReturn != pdPASS)
    {
        ERROR("Failed to create task");
        return;
    }

    xReturn = xTaskCreate(run_sample3, "samples3", configMINIMAL_STACK_SIZE,
            NULL, TASK_PRIORITY, NULL);
    if (xReturn != pdPASS)
    {
        ERROR("Failed to create task");
        return;
    }

    vTaskStartScheduler();

}

void run_sample1(void *arg)
{
    (void)arg;

    PRINT("Starting sample application 1 %s", SAMPLE_APP1);

    SAMPLE_APP1();

    PRINT("Completed sample application 1 %s", SAMPLE_APP1);

    vTaskSuspend(NULL);
}

void run_sample2(void *arg)
{
    (void)arg;

    PRINT("Starting sample application 2 %s", SAMPLE_APP2);

    SAMPLE_APP2();

    PRINT("Completed sample application 2 %s", SAMPLE_APP2);

    vTaskSuspend(NULL);
}

void run_sample3(void *arg)
{
    (void)arg;

    PRINT("Starting sample application 3 %s", SAMPLE_APP3);

    SAMPLE_APP3();

    PRINT("Completed sample application 3 %s", SAMPLE_APP3);

    vTaskSuspend(NULL);
}

static void prvSetupHardware(void)
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

void vApplicationIdleHook(void)
{
}


int main(void)
{
    prvSetupHardware();

    samples_main();

    /*Block here indefinitely; Should never reach here*/
    while (1)
    {
    }
}
