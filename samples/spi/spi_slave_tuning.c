/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 * SPDX-License-Identifier: MIT-0
 */

/**
 * @file spi_slave_isolated_dma_sample.c
 * @brief Isolated SPI Slave Reference Design (DMA)
 *
 * @details
 * @section slave_iso_desc Architecture & Synchronization
 * This file represents a physically isolated SPI Slave device.
 *
 * ====================================================================================
 * SPI SLAVE REFERENCE DESIGN FOR TUNING
 * ====================================================================================
 *
 * This file implements the Slave side of a full-duplex SPI DMA loopback reference
 * design. It coordinates with the Master using a strict, in-band command protocol
 * followed by time-paced bulk data transfers.
 *
 * ------------------------------------------------------------------------------------
 * 1. COMMAND AND DATA SYNCHRONIZATION FLOW
 * ------------------------------------------------------------------------------------
 * Because the Slave operates on an isolated physical boundary (or isolated core)
 * without shared RTOS semaphores, it relies entirely on the Master's SPI clocks
 * and OS-level pacing to synchronize its DMA engines.
 *
 * SLAVE (SPIS0)                                     MASTER (SPIM1)
 * =============                                     ==============
 * * [PHASE 0: TUNING PROTOCOL (PIO)]
 * Enter Tuning Mode         <-------------------- Receive TUNE_CMD_START (0xB1)
 * * Loop [Until END Command]:
 * Prepare 8-Byte Pattern    <-------------------- Receive TUNE_CMD_ARM (0xA1)
 * Transmit 8-Byte Pattern   ====================> Clocked by Master
 * Exit Tuning Mode          <-------------------- Receive TUNE_CMD_END (0xE1)
 *
 * [PHASE 1: DATA READ (DMA)]
 * Arm DMA RX & Block
 * (Wait during Master Pacing)
 * Capture 1024-Byte Payload <==================== Master Transmits 1024 Bytes
 *
 * [PHASE 2: DATA WRITE (DMA)]
 * Arm DMA TX & Block
 * (Wait during Master Pacing)
 * Transmit 1024-Byte Payload ====================> Master Receives 1024 Bytes
 *
 * [PHASE 3: VERIFICATION]
 * Validate Received Payload                         Validate Received Payload
 *
 * ------------------------------------------------------------------------------------
 * 2. SLAVE TUNING RESPONSE (BUS TRAINING)
 * ------------------------------------------------------------------------------------
 * At elevated clock speeds, the Master must adjust its RX Sample Delay to
 * compensate for round-trip propagation delays.
 *
 * The Slave facilitates this training during Phase 0 by acting as a predictable
 * data source. It sits in a blocking PIO read loop, listening for 1-byte command
 * codes. When it receives the `TUNE_CMD_ARM` command, it immediately loads a
 * static, 8-byte training pattern (`tune_pattern`) into its TX buffer and blocks.
 * * The Master then generates the SPI clocks to pull this pattern across the bus,
 * analyzing it to calculate the optimal sample delay. Once the Master sends
 * `TUNE_CMD_END`, the Slave exits the PIO state machine and transitions to
 * high-speed DMA mode for the bulk data phases.
 * ====================================================================================
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "socfpga_spi.h"
#include "socfpga_cache.h"
#include "osal.h"
#include "osal_log.h"

#define SPI_SLAVE_INSTANCE      0U
#define SPI_OP_MOD              SPI_MODE3

#define XFER_SIZE               1024U
#define TUNE_BYTES              24U

#define TUNE_CMD_START          0xB1U
#define TUNE_CMD_ARM            0xA1U
#define TUNE_CMD_END            0xE1U
#define TUNE_CMD_DUMMY          0x00U

/* DMA channel allocation for this slave; the paired master sample uses
 * CH1 (TX)/CH2 (RX) so both peers can run their bulk transfers concurrently
 * on separate channels. */
#define SLAVE_DMA_TX_INSTANCE   DMA_INSTANCE0
#define SLAVE_DMA_TX_CHANNEL    DMA_CH3
#define SLAVE_DMA_TX_PRIO       0U
#define SLAVE_DMA_RX_INSTANCE   DMA_INSTANCE0
#define SLAVE_DMA_RX_CHANNEL    DMA_CH4
#define SLAVE_DMA_RX_PRIO       1U

#define TASK_STACK_SIZE         (configMINIMAL_STACK_SIZE * 4U)
#define SLAVE_TASK_PRIORITY     (configMAX_PRIORITIES - 2U)

static uint32_t slave_tx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t slave_rx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t tune_slave_tx[TUNE_BYTES] __attribute__((aligned(64)));
static uint8_t tune_slave_rx[TUNE_BYTES] __attribute__((aligned(64)));

/* Known pattern loaded on TUNE_CMD_ARM and clocked out for the master to
 * sample while it sweeps its RX sampling delay. */
static const uint8_t tune_pattern[TUNE_BYTES] = {
    0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU,
    0x0FU, 0xF0U, 0x69U, 0x96U, 0xC3U, 0x3CU, 0x5AU, 0xA5U,
    0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU
};

/* Payload the slave expects to read back from the master in the DMA read
 * stage. */
static const uint8_t pattern_m2s[] = {
        0x00U, 0xFFU, 0x55U, 0xAAU, 0x0FU, 0xF0U, 0x33U, 0xCCU
};

/* Payload the slave sends to the master in the DMA write stage. */
static const uint8_t pattern_s2m[] = {
        0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU, 0xDEU, 0xF0U
};

/* Repeats `pattern` to fill dst[0..len). */
static void fill_pattern(uint8_t *dst, uint32_t len, const uint8_t *pattern, uint32_t pat_len)
{
    uint32_t i;
    for (i = 0; i < len; i++)
    {
        dst[i] = pattern[i % pat_len];
    }
}

static osal_semaphore_def_t slave_done_sem_mem;
static osal_semaphore_t slave_done_sem;

typedef struct
{
    osal_semaphore_t sem;
    volatile spi_xfer_status_t status;
} spi_cb_ctx_t;

static spi_cb_ctx_t slave_cb_ctx;

static void spi_slave_done_callback(spi_xfer_status_t status, void *pparam)
{
    spi_cb_ctx_t *ctx = (spi_cb_ctx_t *)pparam;
    if (ctx != NULL)
    {
        ctx->status = status;
        /* Always post so the waiting task doesn't block forever on error
         * statuses. */
        if (ctx->sem != NULL)
        {
            (void)osal_semaphore_post(ctx->sem);
        }
    }
}

/*
 * Blocking PIO command loop for the tuning phase. The master drives all
 * timing here; this just reacts to whatever 1-byte command it clocks in
 * next, so the slave's own TX content is irrelevant until TUNE_CMD_ARM.
 */
static int32_t spi_slave_serve_tuning(spi_handle_t slave_handle)
{
    bool tuning_active = false;
    int32_t ret;
    uint8_t cmd_tx, cmd_rx;
    uint32_t i;

    PRINT("Isolated Slave: Waiting for Master Tuning Commands...");

    /* Wait for the master to start the tuning protocol. */
    while (!tuning_active) {
        cmd_tx = TUNE_CMD_DUMMY; cmd_rx = 0U;
        ret = spi_xfer_sync(slave_handle, &cmd_tx, &cmd_rx, 1U);
        if (ret != 0)
        {
            return ret;
        }
        if (cmd_rx == TUNE_CMD_START)
        {
            tuning_active = true;
        }
    }

    /* Serve one command per iteration until the master signals END. */
    while (tuning_active) {
        cmd_tx = TUNE_CMD_DUMMY; cmd_rx = 0U;
        ret = spi_xfer_sync(slave_handle, &cmd_tx, &cmd_rx, 1U);
        if (ret != 0)
        {
            return ret;
        }

        if (cmd_rx == TUNE_CMD_END) {
            break;
        }

        if (cmd_rx == TUNE_CMD_ARM) {
            /* Load the known pattern and clock it out for the master to
             * sample at its current delay setting. */
            for (i = 0U; i < TUNE_BYTES; i++)
            {
                tune_slave_tx[i] = tune_pattern[i];
                tune_slave_rx[i] = 0U;
            }
            ret = spi_xfer_sync(slave_handle, tune_slave_tx, tune_slave_rx, TUNE_BYTES);
            if (ret != 0)
            {
                return ret;
            }
        }
    }
    return 0;
}

static void spi_slave_isolated_xfer_task(void *arg)
{
    int32_t ret;
    uint32_t i;
    spi_handle_t slave_handle = NULL;
    spi_cfg_t slave_cfg = {0};
    spi_dma_config_t slave_dma_cfg = {0};
    bool success = true;

    (void)arg;
    (void)success;

    slave_done_sem = osal_semaphore_create(&slave_done_sem_mem);

    slave_handle = spi_slave_open(SPI_SLAVE_INSTANCE);
    if (slave_handle == NULL) {
        ERROR("Isolated Slave: failed to open SPI");
        goto slave_out;
    }

    /* Bring the controller up as slave; the bit clock is driven by the
     * master, so no clock field is needed here. */
    slave_cfg.mode = SPI_OP_MOD;
    slave_cfg.role = SPI_ROLE_SLAVE;
    spi_ioctl(slave_handle, SPI_SET_CONFIG, &slave_cfg);

    if (spi_slave_serve_tuning(slave_handle) != 0)
    {
        ERROR("Isolated Slave: Tuning failed");
        goto slave_out;
    }
    PRINT("Isolated Slave: Tuning Finished.");

    /* Switch to DMA for the bulk payload stages below; the tuning phase
     * above stays on PIO since it only ever moves a handful of bytes. */
    slave_dma_cfg.tx_instance = SLAVE_DMA_TX_INSTANCE;
    slave_dma_cfg.tx_channel = SLAVE_DMA_TX_CHANNEL;
    slave_dma_cfg.tx_prio = SLAVE_DMA_TX_PRIO;
    slave_dma_cfg.rx_instance = SLAVE_DMA_RX_INSTANCE;
    slave_dma_cfg.rx_channel = SLAVE_DMA_RX_CHANNEL;
    slave_dma_cfg.rx_prio = SLAVE_DMA_RX_PRIO;
    spi_ioctl(slave_handle, SPI_ENABLE_DMA, &slave_dma_cfg);

    slave_cb_ctx.sem = slave_done_sem;
    slave_cb_ctx.status = SPI_XFER_ERROR;
    spi_set_callback(slave_handle, spi_slave_done_callback, &slave_cb_ctx);

    /* Stage 1: read pattern_m2s from the master via DMA. */
    PRINT("Isolated Slave: Arming Stage 1 DMA Read...");

    for (i = 0; i < XFER_SIZE; i++) {
        slave_tx[i] = 0U;
        slave_rx[i] = 0U;
    }

    (void)osal_semaphore_wait(slave_done_sem, 0);
    slave_cb_ctx.status = SPI_XFER_ERROR;
    ret = spi_xfer_async(slave_handle, slave_tx, slave_rx, XFER_SIZE);
    osal_semaphore_wait(slave_done_sem, OSAL_TIMEOUT_WAIT_FOREVER);

    if (ret != 0 || slave_cb_ctx.status != SPI_SUCCESS) {
        ERROR("Isolated Slave: Stage 1 failed");
        success = false;
        goto slave_out;
    }

    /* Stage 2: write pattern_s2m to the master via DMA. */
    PRINT("Isolated Slave: Arming Stage 2 DMA Write...");

    /* Prepare TX buffer with Pattern 2 */
    uint8_t temp_pattern[XFER_SIZE];
    fill_pattern(temp_pattern, XFER_SIZE, pattern_s2m, sizeof(pattern_s2m));
    for (i = 0; i < XFER_SIZE; i++) {
        slave_tx[i] = (uint32_t)temp_pattern[i];
        /* Don't clear slave_rx here, we need it for verification at the end! */
    }

    (void)osal_semaphore_wait(slave_done_sem, 0);
    slave_cb_ctx.status = SPI_XFER_ERROR;
    /* Pass NULL for RX so we don't overwrite Stage 1's received data */
    ret = spi_xfer_async(slave_handle, slave_tx, NULL, XFER_SIZE);
    osal_semaphore_wait(slave_done_sem, OSAL_TIMEOUT_WAIT_FOREVER);

    if (ret != 0 || slave_cb_ctx.status != SPI_SUCCESS) {
        ERROR("Isolated Slave: Stage 2 failed");
        success = false;
        goto slave_out;
    }

    /* Both stages are done; verify the data received in stage 1. */
    (void) cache_force_invalidate(slave_rx, XFER_SIZE);
    osal_task_delay(10);

    /* Generate expected buffer for Pattern 1 */
    uint8_t expected_slave_read[XFER_SIZE];
    fill_pattern(expected_slave_read, XFER_SIZE, pattern_m2s, sizeof(pattern_m2s));

    if (memcmp(expected_slave_read, slave_rx, XFER_SIZE) != 0)
    {
        ERROR("Isolated Slave: FAILED - Received data does not match pattern_m2s : %d", ret);
        success = false;
    }
    else
    {
        PRINT("Isolated Slave: PASSED - Successfully verified pattern_m2s from Master!");
    }

slave_out:
    if (slave_handle != NULL) spi_close(slave_handle);
    vTaskDelete(NULL);
}

void spi_slave_isolated_task(void)
{
    TaskHandle_t slave_task_handle = NULL;
    if (xTaskCreate(spi_slave_isolated_xfer_task, "SPI_Slave_Iso", TASK_STACK_SIZE,
            NULL, SLAVE_TASK_PRIORITY, &slave_task_handle) == pdPASS)
    {

    }
}
