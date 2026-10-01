/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
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
#include "socfpga_mmc.h"
#include "socfpga_fpga_manager.h"
#include "osal.h"
#include "osal_log.h"

/*
 * The SPI slave/slave-DMA and sampling-delay tuning samples run the SoC's
 * own SPIM1 and SPIS0 controllers as a master/slave pair rather than
 * talking to an external SPI device. They are wired to each other through
 * the FPGA fabric, so that connection only exists once a bitstream that
 * routes SPIM1 <-> SPIS0 has been loaded onto the fabric via the FPGA
 * manager/SDM.
 *
 * The rbf file must be copied to the root of the SD card before running
 * the sample; it is read from the SD card over SDMMC and pushed to the
 * fabric by load_bitstream() below. If a different rbf file is used,
 * update RBF_FILENAME to match.
 */
#define RBF_FILENAME    "/core.rbf"

/* Select which SPI sample task(s) to run. */
#define SPI_SAMPLE_ENABLE_PIO           1
#define SPI_SAMPLE_ENABLE_DMA           0
#define SPI_SAMPLE_ENABLE_SLAVE         0
#define SPI_SAMPLE_ENABLE_SLAVE_DMA     0
#define SPI_SAMPLE_ENABLE_MASTER_TUNING 0
#define SPI_SAMPLE_ENABLE_SLAVE_TUNING  0

#if ((SPI_SAMPLE_ENABLE_SLAVE == 1) || \
        (SPI_SAMPLE_ENABLE_SLAVE_DMA == 1) || \
        (SPI_SAMPLE_ENABLE_MASTER_TUNING == 1) || \
        (SPI_SAMPLE_ENABLE_SLAVE_TUNING == 1))
    #define LOAD_BITSTREAM  1
#else
    #define LOAD_BITSTREAM  0
#endif

void spi_task(void);
void spi_dma_task(void);
void spi_slave_task(void);
void spi_slave_dma_task(void);
void spi_master_isolated_task(void);
void spi_slave_isolated_task(void);

#define TASK_PRIORITY    (configMAX_PRIORITIES - 2)
void run_samples( void *arg );

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

#if (LOAD_BITSTREAM == 1)
static void load_bitstream(void)
{
    uint32_t file_size;
    uint8_t *rbf_ptr;

    PRINT("Reading the rbf file from sdmmc");
    rbf_ptr = mmc_read_file(SOURCE_SDMMC, RBF_FILENAME, &file_size);
    if (rbf_ptr == NULL)
    {
        ERROR("Unable to read bitstream from memory");
        return;
    }

    PRINT("Starting fpga configuration");
    if (load_fpga_bitstream(rbf_ptr, file_size) != 0)
    {
        ERROR("Failed to load bitstream !!!");
        vPortFree(rbf_ptr);
        return;
    }

    vPortFree(rbf_ptr);

    PRINT("Loaded bitstream file successfully");
}
#endif



void run_samples( void *arg )
{
    (void) arg;

#if (LOAD_BITSTREAM == 1)
    /*
     * Configure the FPGA fabric before starting any sample that uses the
     * SPIM1 <-> SPIS0 loopback topology (slave, slave-DMA, master/slave
     * tuning). Without this, SPIM1 and SPIS0 are not physically connected
     * and those samples' transfers will simply time out.
     */
    load_bitstream();
#endif

#if SPI_SAMPLE_ENABLE_PIO
    spi_task();
#endif
#if SPI_SAMPLE_ENABLE_DMA
    spi_dma_task();
#endif
#if SPI_SAMPLE_ENABLE_SLAVE
    spi_slave_task();
#endif
#if SPI_SAMPLE_ENABLE_SLAVE_DMA
    spi_slave_dma_task();
#endif
#if SPI_SAMPLE_ENABLE_MASTER_TUNING
    spi_master_isolated_task();
#endif
#if SPI_SAMPLE_ENABLE_SLAVE_TUNING
    spi_slave_isolated_task();
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
