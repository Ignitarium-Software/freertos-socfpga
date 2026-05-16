/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 - 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SPI slave
 */

/**
 * @file spi_slave_sample.c
 * @brief SPI master to slave loopback sample
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "socfpga_spi.h"
#include "socfpga_mmc.h"
#include "socfpga_fpga_manager.h"
#include "osal.h"
#include "osal_log.h"

/**
 * @defgroup spi_slave_sample SPI Slave Loopback (PIO/Interrupt)
 * @ingroup samples
 *
 * SPI master to slave loopback sample
 *
 * @details
 * @section spi_slave_desc Description
 * This sample runs SPIM1 (master) and SPIS0 (slave) in the same application and
 * performs full-duplex loopback transfers. Each iteration:
 * - The slave arms @c spi_xfer_async() with its TX/RX buffers.
 * - The master clocks a transfer with its TX/RX buffers.
 * - The buffers are compared:
 *   - master TX vs slave RX
 *   - slave TX vs master RX
 *
 * A simple semaphore handshake is used so the master only clocks the transfer
 * after the slave has armed the async transfer.
 *
 * @section spi_slave_pre Prerequisites
 * - The FPGA bitstream (@c /core.rbf) must be present on the SD card.
 * - The bitstream must connect SPIM1 <-> SPIS0 appropriately for loopback.
 *
 * @section spi_slave_param Configurable Parameters
 * - Instances and slave select: @c SPI_MASTER_INSTANCE, @c SPI_SLAVE_INSTANCE,
 *   @c SLAVE_SELECT_NUM
 * - SPI frequency: @c SPI_FREQ
 * - Transfer size/iterations: @c XFER_SIZE, @c SPI_SLAVE_SAMPLE_NUM_XFERS
 *
 * @section spi_slave_how_to How to Run
 * 1. Enable this sample in @c samples/spi/main.c
 *    (see @c SPI_SAMPLE_ENABLE_SLAVE).
 * 2. Build and flash the application.
 * 3. Observe the UART terminal for pass/fail output.
 */

#define SPI_MASTER_INSTANCE  1U
#define SPI_SLAVE_INSTANCE   0U
#define SLAVE_SELECT_NUM     1U
#define SPI_FREQ             500000U
#define XFER_SIZE            64U
#define SLAVE_TIMEOUT_MS     1000U

/* Number of master<->slave transfers to run in this sample. */
#ifndef SPI_SLAVE_SAMPLE_NUM_XFERS
#define SPI_SLAVE_SAMPLE_NUM_XFERS 6U
#endif
#define RBF_FILENAME         "/core.rbf"
static volatile bool stop_xfers;

static uint8_t master_tx[XFER_SIZE];
static uint8_t master_rx[XFER_SIZE];
static uint8_t slave_tx[XFER_SIZE];
static uint8_t slave_rx[XFER_SIZE];

/*
 * This sample uses 3 semaphores.
 *
 * Handshake order per iteration:
 * - Slave arms spi_xfer_async(), then posts `slave_ready_sem`.
 * - Master waits `slave_ready_sem`, clocks a sync transfer, then posts
 *   `signal_slave_sem`.
 * - Slave waits `signal_slave_sem` before reusing buffers.
 * - Master waits `slave_done_sem` before verifying slave_rx.
 */
static osal_semaphore_def_t slave_ready_sem_mem;
static osal_semaphore_t slave_ready_sem;

static osal_semaphore_def_t signal_slave_sem_mem;
static osal_semaphore_t signal_slave_sem;

static osal_semaphore_def_t slave_done_sem_mem;
static osal_semaphore_t slave_done_sem;

typedef struct
{
    osal_semaphore_t sem;
    volatile spi_xfer_status_t status;
} spi_cb_ctx_t;

static spi_cb_ctx_t slave_cb_ctx;

static void spi_slave_xfer_task(void *arg);
static void spi_master_xfer_task(void *arg);

static void spi_done_callback(spi_xfer_status_t status, void *pparam)
{
    spi_cb_ctx_t *ctx = (spi_cb_ctx_t *)pparam;

    if (ctx == NULL)
    {
        return;
    }

    ctx->status = status;

    /* Always post so waiters don't deadlock on error statuses. */
    if (ctx->sem != NULL)
    {
        (void)osal_semaphore_post(ctx->sem);
    }
}

static void load_bitstream(void)
{
    uint32_t file_size;
    uint8_t *rbf_ptr;

    PRINT("Reading the rbf file from sdmmc");
    rbf_ptr = mmc_read_file(SOURCE_SDMMC, RBF_FILENAME, &file_size);
    if (rbf_ptr == NULL)
    {
        ERROR("Unable to read bitstream from memory !!!");
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

static void spi_slave_xfer_task(void *arg)
{
    (void)arg;
    int32_t ret;
    uint32_t iter;
    uint32_t i;

    spi_handle_t slave_handle = NULL;
    spi_cfg_t slave_cfg = { 0 };

    slave_handle = spi_slave_open(SPI_SLAVE_INSTANCE);
    if (slave_handle == NULL)
    {
        ERROR("Failed to open SPI slave instance");
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        (void)osal_semaphore_post(slave_ready_sem);
        osal_task_delete();
        return;
    }

    slave_cfg.mode = SPI_MODE0;
    slave_cfg.clk = SPI_FREQ;
    slave_cfg.role = SPI_ROLE_SLAVE;

    ret = spi_ioctl(slave_handle, SPI_SET_CONFIG, &slave_cfg);
    if (ret != 0)
    {
        ERROR("Failed to configure SPI slave");
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        (void)spi_close(slave_handle);
        osal_task_delete();
        return;
    }

    slave_cb_ctx = (spi_cb_ctx_t){
        .sem = slave_done_sem,
        .status = SPI_XFER_ERROR,
    };
    ret = spi_set_callback(slave_handle, spi_done_callback, &slave_cb_ctx);
    if (ret != 0)
    {
        ERROR("Failed to set SPI slave callback");
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        (void)spi_close(slave_handle);
        osal_task_delete();
        return;
    }

    for (iter = 0; iter < SPI_SLAVE_SAMPLE_NUM_XFERS; iter++)
    {
        if (stop_xfers)
        {
            break;
        }

        /* Prepare slave buffers (zero RX, update TX pattern). */
        for (i = 0; i < XFER_SIZE; i++)
        {
            slave_tx[i] = (uint8_t)(((i + 0xA0U) + iter) & 0xFFU);
            slave_rx[i] = 0U;
        }

        /* Arm the slave transfer, then tell the master it can clock. */
        slave_cb_ctx.status = SPI_XFER_ERROR;
        ret = spi_xfer_async(slave_handle, slave_tx, slave_rx,
                XFER_SIZE);
        if (ret != 0)
        {
            ERROR("SPI slave transfer_async failed");
            break;
        }

        (void)osal_semaphore_post(slave_ready_sem);

        /*
         * Master signals after verify/print, so we don't overwrite buffers
         * early.
         */
        if (osal_semaphore_wait(signal_slave_sem, SLAVE_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("SPI slave timed out waiting for master signal");
            break;
        }
    }

    /* Final signal to allow master cleanup (re-uses slave_ready_sem). */
    (void)osal_semaphore_post(slave_ready_sem);

    (void)spi_close(slave_handle);
    osal_task_delete();
}

static void spi_master_xfer_task(void *arg)
{
    (void)arg;
    int32_t ret;
    uint32_t iter;
    uint32_t i;

    spi_handle_t master_handle = NULL;
    spi_cfg_t master_cfg = { 0 };

    /* Use ret as overall status for this task: 0 = pass, nonzero = fail. */
    ret = 0;

    master_handle = spi_open(SPI_MASTER_INSTANCE);
    if (master_handle == NULL)
    {
        ERROR("Failed to open SPI master instance");
        stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        (void)osal_semaphore_post(slave_ready_sem);
        (void)osal_semaphore_post(slave_ready_sem);
        osal_task_delete();
        return;
    }

    master_cfg.mode = SPI_MODE0;
    master_cfg.clk = SPI_FREQ;
    master_cfg.role = SPI_ROLE_MASTER;

    ret = spi_ioctl(master_handle, SPI_SET_CONFIG, &master_cfg);
    if (ret != 0)
    {
        ERROR("Failed to configure SPI master");
        stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        (void)osal_semaphore_post(slave_ready_sem);
        (void)spi_close(master_handle);
        osal_task_delete();
        return;
    }

    ret = spi_select_slave(master_handle, SLAVE_SELECT_NUM);
    if (ret != 0)
    {
        ERROR("Failed to select SPI slave");
        ret = -EINVAL;
    }

    for (iter = 0; (iter < SPI_SLAVE_SAMPLE_NUM_XFERS) && (ret == 0); iter++)
    {
        /* Wait until the slave is ready (and will block in its transfer). */
        if (osal_semaphore_wait(slave_ready_sem, SLAVE_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("SPI master timed out waiting for slave_ready");
            ret = -ETIMEDOUT;
            break;
        }

        if (stop_xfers)
        {
            break;
        }

        /* Prepare master buffers (zero RX, update TX pattern). */
        for (i = 0; i < XFER_SIZE; i++)
        {
            master_tx[i] = (uint8_t)((i + iter) & 0xFFU);
            master_rx[i] = 0U;
        }

        /* Ensure completion semaphore starts empty for this iteration. */
        (void)osal_semaphore_wait(slave_done_sem, 0);

        ret = spi_xfer_sync(master_handle, master_tx, master_rx,
                XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Failed to start SPI master transfer");
            break;
        }

        /*
         * Wait for the slave async transfer completion before inspecting
         * slave_rx.
         */
        if (osal_semaphore_wait(slave_done_sem, SLAVE_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("SPI master timed out waiting for slave completion");
            ret = -ETIMEDOUT;
            break;
        }

        if (slave_cb_ctx.status != SPI_SUCCESS)
        {
            ERROR("SPI slave transfer failed (status %d)",
                    (int)slave_cb_ctx.status);
            ret = -EIO;
            break;
        }

        if ((memcmp(master_tx, slave_rx, XFER_SIZE) != 0)
                || (memcmp(slave_tx, master_rx, XFER_SIZE) != 0))
        {
            ERROR("SPI loopback verification FAILED");
            ret = -EIO;
            break;
        }

        /* Release the slave task for the next iteration. */
        (void)osal_semaphore_post(signal_slave_sem);
    }

    /* Unblock the slave if we exit early. */
    if (ret != 0)
    {
        stop_xfers = true;
    }
    (void)osal_semaphore_post(signal_slave_sem);

    /* Wait for the slave task to exit and stop touching buffers. */
    if (osal_semaphore_wait(slave_ready_sem, SLAVE_TIMEOUT_MS) != pdTRUE)
    {
        ERROR("Timed out waiting for slave task to exit");
        if (ret == 0)
        {
            ret = -ETIMEDOUT;
        }
    }

    if (ret == 0)
    {
        PRINT("SPI loopback verification PASSED");
        PRINT("SPI loopback sample completed");
    }

    (void)spi_close(master_handle);
    (void)osal_semaphore_delete(slave_ready_sem);
    (void)osal_semaphore_delete(signal_slave_sem);
    (void)osal_semaphore_delete(slave_done_sem);

    osal_task_delete();
}

void spi_slave_task(void)
{
    uint32_t i;

    stop_xfers = false;

    slave_ready_sem = osal_semaphore_create(&slave_ready_sem_mem);
    signal_slave_sem = osal_semaphore_create(&signal_slave_sem_mem);
    slave_done_sem = osal_semaphore_create(&slave_done_sem_mem);

    if ((slave_ready_sem == NULL) || (signal_slave_sem == NULL) ||
            (slave_done_sem == NULL))
    {
        ERROR("Failed to create semaphores");
        return;
    }

    load_bitstream();

    for (i = 0; i < XFER_SIZE; i++)
    {
        master_tx[i] = (uint8_t)(i & 0xFFU);
        slave_tx[i] = (uint8_t)((i + 0xA0U) & 0xFFU);
        master_rx[i] = 0U;
        slave_rx[i] = 0U;
    }

    PRINT("SPI loopback sample (SPIM1 <-> SPIS0)");
    PRINT("Transfers: %u, Bytes: %u",
            (unsigned)SPI_SLAVE_SAMPLE_NUM_XFERS,
            (unsigned)XFER_SIZE);

    /*
     * Start the slave and master transfers as separate tasks.
     * Keep slave above master so it can enter transfer setup first.
     */
    if (osal_task_create(spi_slave_xfer_task, "SPI_Slave", NULL,
            (configMAX_PRIORITIES - 2)) == false)
    {
        ERROR("Failed to create SPI slave task");
        (void)osal_semaphore_delete(slave_ready_sem);
        (void)osal_semaphore_delete(signal_slave_sem);
        (void)osal_semaphore_delete(slave_done_sem);
        return;
    }

    if (osal_task_create(spi_master_xfer_task, "SPI_Master", NULL,
            (configMAX_PRIORITIES - 3)) == false)
    {
        ERROR("Failed to create SPI master task");
        (void)osal_semaphore_delete(slave_ready_sem);
        (void)osal_semaphore_delete(signal_slave_sem);
        (void)osal_semaphore_delete(slave_done_sem);
        return;
    }

    /* Tasks run and self-delete. */
    return;
}
