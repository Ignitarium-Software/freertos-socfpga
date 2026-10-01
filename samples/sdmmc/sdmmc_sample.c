/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SoC FPGA sdmmc
 */

/**
 * @file sdmmc_sample.c
 * @brief Sample Application for sdmmc operations.
 */

/**
 * @defgroup sdmmc_rw_sample SD/eMMC
 * @ingroup samples
 *
 * Sample Application for SD/eMMC Block Read/Write Validation
 *
 * @details
 * @section sdmmc_des Description
 * This sample demonstrates the use of the SD/eMMC driver to perform block-level
 * read and write operations. It writes a pattern of data to a specified logical
 * block address (LBA), reads it back from the same address, and verifies the
 * correctness of the data.
 *
 * @section sdmmc_pre Prerequisites
 * - An SD card or eMMC device is present in the system
 *
 * @section sdmmc_howto How to Run
 * - Follow the common README for build and flashing instructions.
 *
 * @section sdmmc_res Expected Results
 * - Data write to SD/eMMC is successful.
 * - Data read back matches the originally written pattern.
 *
 * @{
 */
/** @} */


#include <string.h>
#include <stdint.h>
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "osal.h"
#include "osal_log.h"
#include "socfpga_sdmmc.h"
#include "socfpga_cache.h"

/* Block size of the card in bytes */
#define BLK_SIZE    512
/* The block used in the write-read operation */
#define BLK_NUM    8192
/* No of blocks for the transaction */
#define BLK_CNT    1
/* The value used to fill the write buffer */
#define SYMBOL    0x88
/* Select sync or async operation */
#define USE_SYNC    1

static int32_t xfer_resp;
static osal_semaphore_t xfer_lock;
static osal_semaphore_def_t osal_def_xfer;

static uint8_t write_buffer[BLK_SIZE * BLK_CNT];
static uint8_t read_buffer[BLK_SIZE * BLK_CNT];

#if !USE_SYNC
static void sdmmc_cb(int32_t state)
{
    xfer_resp = state;
    osal_semaphore_post(xfer_lock);
}

static void wait_xfer_done()
{
    osal_semaphore_wait(xfer_lock, OSAL_TIMEOUT_WAIT_FOREVER);
}
#endif


void sdmmc_task(void)
{
    int32_t status;
    uint64_t sector_count = 0UL;
    uint64_t write_address = BLK_SIZE * BLK_NUM;
    uint64_t read_address = BLK_SIZE * BLK_NUM;
    uint32_t block_size = BLK_SIZE;
    uint32_t number_of_blocks = BLK_CNT;

    xfer_lock = osal_semaphore_create(&osal_def_xfer);

    status = sdmmc_init_card(&sector_count);
    PRINT("\rInitializing the card...\r");
    if (status != 0)
    {
        PRINT("\rCard initialization failed with status: %u\r", status);
        return;
    }
    PRINT("Card initialized successfully");
    PRINT("Sector Count -> %lu", sector_count);

    memset(write_buffer, SYMBOL, 512 * BLK_CNT);

    /* Do cache write back to reflect the buffer content in physical memory */
    cache_force_write_back((uint64_t *)write_buffer, block_size *
            number_of_blocks);

    PRINT("\rWriting data to the card...\r");
#if USE_SYNC
    xfer_resp = sdmmc_write_block_sync(write_address, (uint64_t *)write_buffer,
            block_size, number_of_blocks);
#else
    status = sdmmc_write_block_async(write_address, (uint64_t *)write_buffer,
            block_size, number_of_blocks, sdmmc_cb);
    if (status != 0)
    {
        ERROR("Card write failed with status: %d", status);
        return;
    }
    wait_xfer_done();
#endif

    if (xfer_resp != 0)
    {
        ERROR("Card write failed with status: %d", xfer_resp);
        return;
    }

    PRINT("\rData written to the card successfully\r");

    cache_force_write_back((uint64_t *)read_buffer, block_size *
            number_of_blocks);
    cache_force_invalidate((uint64_t *)read_buffer, block_size *
            number_of_blocks);

    PRINT("\rReading data from the card...\r");
#if USE_SYNC
    xfer_resp = sdmmc_read_block_sync(read_address, (uint64_t *)read_buffer,
            block_size, number_of_blocks);
#else
    status = sdmmc_read_block_async(read_address, (uint64_t *)read_buffer,
            block_size, number_of_blocks, sdmmc_cb);

    if (status != 0)
    {
        ERROR("Card read failed with status: %d", status);
        return;
    }
    wait_xfer_done();
#endif

    if (xfer_resp != 0)
    {
        ERROR("Card read failed with status: %d", xfer_resp);
        return;
    }
    PRINT("Data read from the card successfully");

    /* Do cache invalidation to fetch the latest values from physical memory */
    cache_force_invalidate((uint64_t *)read_buffer, block_size *
            number_of_blocks);


    PRINT("Validating data...");
    if (memcmp(write_buffer, read_buffer, block_size * number_of_blocks) == 0)
    {
        PRINT("Data verification successful");
    }
    else
    {
        size_t total_bytes = block_size * number_of_blocks;
        for (size_t i = 0; i < total_bytes; i++)
        {
            if (write_buffer[i] != read_buffer[i])
            {
                ERROR("Data mismatch at byte %zu: written = 0x%02X, read = 0x%02X\n", i,
                        write_buffer[i], read_buffer[i]);
                break;
            }
        }
    }
    PRINT("SD/eMMC driver write - read example completed");
}
