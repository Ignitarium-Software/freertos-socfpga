/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SoC FPGA DMA
 */


#include <stdlib.h>
#include <string.h>
#include "osal.h"
#include "osal_log.h"
#include <task.h>
#include "socfpga_dma.h"
#include "socfpga_cache.h"

/**
 * @defgroup dma_sample DMA
 * @ingroup samples
 *
 * Sample Application for DMA
 *
 * @details
 * @section dma_desc Description
 * This sample application demonstrates the use of the DMA driver to perform
 * a memory-to-memory data transfer. It transfers a predefined block of data
 * from a source buffer to a destination buffer and verifies that the transfer
 * completed successfully by comparing the contents.
 *
 * @section dma_param Configurable Parameters
 * - Transfer size can be configured by changing the value of @c TRANSFER_SIZE macro.
 * - Number of blocks can be configured by changing the value of @c NUM_BLK macro.
 * - Instance and channel can be configured by changing the values in respective APIs.
 *
 * @section dma_how_to How to Run
 * 1. Follow the common README instructions to build and flash the application.
 * 2. Run the application on the board.
 * 3. Output can be observed in the UART terminal.
 *
 * @section dma_result Expected Results
 * - The DMA controller initiates and completes a memory-to-memory transfer.
 * - The destination buffer contents match the source buffer.
 * - A message confirming successful transfer and verification is printed to the terminal.
 */

/* Test configurations */

#define TRANSFER_SIZE    1024U
#define NUM_BLK        2U


static uint16_t tx_buf[NUM_BLK][TRANSFER_SIZE];
static uint16_t rx_buf[NUM_BLK][TRANSFER_SIZE];

osal_semaphore_def_t done_sem_mem;
osal_semaphore_t done_sem;

/*
 * @brief DMA done callback function
 */

void dma_done_callback(void *data)
{
    (void)data;
    osal_semaphore_post(done_sem);
}

void dma_task(void)
{

    done_sem = osal_semaphore_create(&done_sem_mem);
    dma_config_t dma1channel1config;
    dma_xfer_cfg_t blk_xfer_list[NUM_BLK] = { 0 };
    BaseType_t ret_val;
    dma_handle_t dma_handle;

    PRINT("DMA data transfer sample application");

    PRINT("Preparing the data buffers ...");
    for (int j = 0; j < NUM_BLK; j++)
    {
        for (int i = 0; i < TRANSFER_SIZE; i++)
        {
            tx_buf[j][i] = (uint16_t)i;
            rx_buf[j][i] = 0x0;
        }
    }
    cache_force_write_back((void *)tx_buf, (NUM_BLK * TRANSFER_SIZE * 2U));
    cache_force_write_back((void *)rx_buf, (NUM_BLK * TRANSFER_SIZE * 2U));

    PRINT("Done.");

    PRINT("Preparing the block transfer list ...");

    for (int i = 0; i < NUM_BLK; i++)
    {
        blk_xfer_list[i].src = (uint64_t)(tx_buf[i]);
        blk_xfer_list[i].dst = (uint64_t)(rx_buf[i]);
        blk_xfer_list[i].blk_size = TRANSFER_SIZE * 2;
        /* Update next block transfer address if current block is not last in the list */
        if (i < (NUM_BLK - 1))
        {
            blk_xfer_list[i].next_xfer_cfg = &blk_xfer_list[i + 1];
        }
    }

    PRINT("Done.");

    PRINT("Configuring and setting up the DMA channel ...");
    dma_handle = dma_open(DMA_INSTANCE0, DMA_CH1);
    if (dma_handle == NULL)
    {
        ERROR("Opening DMA channel failed");
        return;
    }

    dma1channel1config.ch_dir = DMA_MEM_TO_MEM_DMAC;
    dma1channel1config.ch_prio = 0;
    dma1channel1config.callback = dma_done_callback;

    ret_val = dma_config(dma_handle, &dma1channel1config);
    if (ret_val != 0)
    {
        ERROR("Configuring DMA channel failed");
        dma_close(dma_handle);
        ERROR("Exiting DMA sample application");
        return;
    }
    /* source and destination width is set to DMA_XFER_WIDTH8 (8 bytes) */
    ret_val = dma_setup_transfer(dma_handle, &blk_xfer_list[0], NUM_BLK,
            DMA_XFER_WIDTH8, DMA_XFER_WIDTH8);
    if (ret_val != 0)
    {
        ERROR("Setting up DMA channel failed");
        dma_close(dma_handle);
        ERROR("Exiting DMA sample application");
        return;
    }

    PRINT("Done.");

    PRINT("Triggering the data transfer ...");
    dma_start_transfer(dma_handle);

    ret_val = osal_semaphore_wait(done_sem, 1000);

    if (ret_val == pdTRUE)
    {
        PRINT("Data transfer done.");

        PRINT("Verifying data ...");

        cache_force_invalidate((void *)rx_buf, sizeof(rx_buf));

        if (memcmp((void *)tx_buf, (void *)rx_buf,
                (TRANSFER_SIZE * NUM_BLK)) == 0)
        {
            PRINT("Verification PASSED");
        }
        else
        {
            ERROR("Verification FAILED");
        }
    }
    else
    {
        ERROR("Failed to get DMA callback");
    }

    osal_semaphore_delete(done_sem);

    dma_close(dma_handle);

    PRINT("DMA data transfer sample completed.");

}
