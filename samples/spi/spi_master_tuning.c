/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 * SPDX-License-Identifier: MIT-0
 */

/**
 * @file spi_master_isolated_dma_sample.c
 * @brief Isolated SPI Master Reference Design (DMA)
 *
 * @details
 * @section master_iso_desc Architecture & Synchronization
 * This file represents a physically isolated SPI Master device. It shares no
 * RAM or RTOS semaphores with the Slave device.
 *
 * ====================================================================================
 * SPI MASTER REFERENCE DESIGN FOR TUNING
 * ====================================================================================
 *
 * This file implements the Master side of a full-duplex SPI DMA loopback reference
 * design. It coordinates with the Slave using a strict, in-band command protocol
 * followed by time-paced bulk data transfers.
 *
 * ------------------------------------------------------------------------------------
 * 1. COMMAND AND DATA SYNCHRONIZATION FLOW
 * ------------------------------------------------------------------------------------
 * The communication protocol is divided into a PIO-based command phase for tuning,
 * followed by high-speed DMA data phases.
 *
 * MASTER (SPIM1)                                    SLAVE (SPIS0)
 * ==============                                    =============
 * * [PHASE 0: TUNING PROTOCOL (PIO)]
 * Send TUNE_CMD_START (0xB1)  --------------------> Enter Tuning Mode
 * * Loop [Delay 0 to 64]:
 * Send TUNE_CMD_ARM (0xA1)  --------------------> Prepare 8-Byte Pattern
 * Read 8-Byte Response      <==================== Send 8-Byte Pattern
 * (Validate Response)
 * * Send TUNE_CMD_END (0xE1)    --------------------> Exit Tuning Mode
 *
 * [PHASE 1: DATA WRITE (DMA)]
 * (Pacing Delay)                                    Arm DMA RX
 * Transmit 1024-Byte Payload  ====================> Capture 1024-Byte Payload
 *
 * [PHASE 2: DATA READ (DMA)]
 * (Pacing Delay)                                    Arm DMA TX
 * Receive 1024-Byte Payload   <==================== Transmit 1024-Byte Payload
 *
 * [PHASE 3: VERIFICATION]
 * Validate Received Payload                         Validate Received Payload
 *
 * ------------------------------------------------------------------------------------
 * 2. RX SAMPLE DELAY TUNING (BUS TRAINING)
 * ------------------------------------------------------------------------------------
 * At elevated clock speeds (e.g., 20MHz - 30MHz), physical trace lengths and
 * capacitive loads introduce measurable propagation delays. Round-trip routing
 * delays on the master's `sclk_out` signal and the slave's returning `rxd` signal
 * can mean that the timing of the `rxd` signal, as seen by the master, has moved
 * away from the normal sampling time.
 *
 * To compensate for this phase shift, the SPI hardware provides a programmable
 * RX Sample Delay register. This sample delay logic has a resolution of exactly
 * one `l4_main_clk` cycle.
 *
 * Software "trains" the serial bus during initialization (Phase 0) by coding a
 * loop that continually reads from the slave and increments the master's RXD
 * sample delay value until the correct, expected data pattern is received by
 * the master. The first matching delay value is locked in for the remainder of
 * the high-speed DMA transfers.
 * ====================================================================================
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "socfpga_spi.h"
#include "socfpga_timer.h"
#include "socfpga_cache.h"
#include "osal.h"
#include "osal_log.h"

#define SPI_MASTER_INSTANCE     1U
#define SLAVE_SELECT_NUM        1U
#define SPI_FREQ                30000000U
#define SPI_OP_MOD              SPI_MODE3

#define XFER_SIZE               1024U
#define TUNE_BYTES              24U
#define TUNE_DELAY_MAX          64U

#define TUNE_CMD_START          0xB1U
#define TUNE_CMD_ARM            0xA1U
#define TUNE_CMD_END            0xE1U
#define TUNE_CMD_DUMMY          0x00U

/* DMA channel allocation for this master; the paired slave sample uses
 * CH3 (TX)/CH4 (RX) so both peers can run their bulk transfers concurrently
 * on separate channels. */
#define MASTER_DMA_TX_INSTANCE  DMA_INSTANCE0
#define MASTER_DMA_TX_CHANNEL   DMA_CH1
#define MASTER_DMA_TX_PRIO      0U
#define MASTER_DMA_RX_INSTANCE  DMA_INSTANCE0
#define MASTER_DMA_RX_CHANNEL   DMA_CH2
#define MASTER_DMA_RX_PRIO      1U

#define TASK_STACK_SIZE         (configMINIMAL_STACK_SIZE * 4U)
#define MASTER_TASK_PRIORITY    (configMAX_PRIORITIES - 3U)

static uint32_t master_tx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t master_rx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t expected_master_write[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t expected_master_read[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t tune_master_tx[TUNE_BYTES] __attribute__((aligned(64)));
static uint8_t tune_master_rx[TUNE_BYTES] __attribute__((aligned(64)));

/* Known pattern the slave echoes back on TUNE_CMD_ARM; used to detect the
 * RX sampling delay at which the master reads it back correctly. */
static const uint8_t tune_pattern[TUNE_BYTES] = {
    0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU,
    0x0FU, 0xF0U, 0x69U, 0x96U, 0xC3U, 0x3CU, 0x5AU, 0xA5U,
    0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU
};

/* Payload the master writes to the slave in the DMA write stage. */
static const uint8_t pattern_m2s[] = {
        0x00U, 0xFFU, 0x55U, 0xAAU, 0x0FU, 0xF0U, 0x33U, 0xCCU
};

/* Payload the master expects to read back from the slave in the DMA read
 * stage. */
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
static osal_semaphore_def_t master_done_sem_mem;
static osal_semaphore_t master_done_sem;

typedef struct
{
    osal_semaphore_t sem;
    volatile spi_xfer_status_t status;
} spi_cb_ctx_t;

static spi_cb_ctx_t master_cb_ctx;

static void spi_master_done_callback(spi_xfer_status_t status, void *pparam)
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

/* Sends a single in-band command byte (TUNE_CMD_*) and discards the slave's
 * response. */
static int32_t spi_send_tune_cmd(spi_handle_t handle, uint8_t cmd)
{
    uint8_t tx_word = cmd;
    uint8_t rx_byte = 0U;
    return spi_xfer_sync(handle, &tx_word, &rx_byte, 1U);
}

/*
 * Sweeps the RX sampling delay from 0 to TUNE_DELAY_MAX, asking the slave to
 * echo back its known tune_pattern at each step. The first delay that
 * reproduces the pattern exactly is returned in *selected_delay.
 */
static int32_t spi_master_run_tuning(spi_handle_t master_handle, uint32_t *selected_delay)
{
    bool found = false;
    uint32_t delay;
    int32_t ret;

    PRINT("Isolated Master: Starting Tuning Phase...");
    /* Tell the slave to enter its tuning command loop. */
    ret = spi_send_tune_cmd(master_handle, TUNE_CMD_START);
    if (ret != 0) return ret;

    for (delay = 0U; delay <= TUNE_DELAY_MAX; delay++) {
        /* Program the candidate delay before this attempt. */
        ret = spi_ioctl(master_handle, SPI_SET_RX_SAMPLING_DELAY, &delay);
        if (ret != 0) break;

        /* Ask the slave to arm its training pattern for this attempt. */
        ret = spi_send_tune_cmd(master_handle, TUNE_CMD_ARM);
        if (ret != 0) break;

        memset(tune_master_tx, TUNE_CMD_DUMMY, TUNE_BYTES);
        memset(tune_master_rx, 0U, TUNE_BYTES);

        /* Clock the pattern across the bus at the current delay setting. */
        ret = spi_xfer_sync(master_handle, tune_master_tx, tune_master_rx, TUNE_BYTES);
        if (ret != 0) break;

        (void) cache_force_invalidate(tune_master_rx, TUNE_BYTES);

        if (memcmp(tune_master_rx, tune_pattern, TUNE_BYTES) == 0) {
            found = true;
            *selected_delay = delay;
            break;
        }
    }

    /* Always exit tuning mode, even on failure, so the slave doesn't stay
     * stuck waiting for TUNE_CMD_END. */
    (void)spi_send_tune_cmd(master_handle, TUNE_CMD_END);
    return found ? 0 : -EIO;
}

static void spi_master_isolated_xfer_task(void *arg)
{
    bool success = true;
    uint32_t tuned_delay = 0U;
    int32_t ret;
    spi_handle_t master_handle = NULL;
    spi_cfg_t master_cfg = {0};
    spi_dma_config_t master_dma_cfg = {0};
    uint32_t i;

    (void)arg;
    (void)success;

    master_done_sem = osal_semaphore_create(&master_done_sem_mem);

    master_handle = spi_open(SPI_MASTER_INSTANCE);
    if (master_handle == NULL)
    {
        ERROR("Isolated Master: failed to open SPI");
        goto master_out;
    }

    /* Bring the controller up as master at the target clock/mode, then
     * address the slave peer this sample talks to. */
    master_cfg.mode = SPI_OP_MOD;
    master_cfg.clk = SPI_FREQ;
    master_cfg.role = SPI_ROLE_MASTER;
    spi_ioctl(master_handle, SPI_SET_CONFIG, &master_cfg);
    spi_select_slave(master_handle, SLAVE_SELECT_NUM);

    if (spi_master_run_tuning(master_handle, &tuned_delay) != 0)
    {
        ERROR("Isolated Master: Tuning failed");
        success = false;
        goto master_out;
    }
    PRINT("Isolated Master: Tuning success. Selected delay: %u", tuned_delay);

    /* Switch to DMA for the bulk payload stages below; the tuning phase
     * above stays on PIO since it only ever moves a handful of bytes. */
    master_dma_cfg.tx_instance = MASTER_DMA_TX_INSTANCE;
    master_dma_cfg.tx_channel = MASTER_DMA_TX_CHANNEL;
    master_dma_cfg.tx_prio = MASTER_DMA_TX_PRIO;
    master_dma_cfg.rx_instance = MASTER_DMA_RX_INSTANCE;
    master_dma_cfg.rx_channel = MASTER_DMA_RX_CHANNEL;
    master_dma_cfg.rx_prio = MASTER_DMA_RX_PRIO;
    spi_ioctl(master_handle, SPI_ENABLE_DMA, &master_dma_cfg);

    master_cb_ctx.sem = master_done_sem;
    master_cb_ctx.status = SPI_XFER_ERROR;
    spi_set_callback(master_handle, spi_master_done_callback, &master_cb_ctx);

    /* Stage 1: write pattern_m2s to the slave via DMA. There is no shared
     * semaphore with the slave, so a fixed delay is what gives it time to
     * arm its DMA RX before the master starts clocking. */
    PRINT("Isolated Master: Delaying 1s. Arming Stage 1 DMA Write...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Prepare TX buffer with Pattern 1 */
    fill_pattern(expected_master_write, XFER_SIZE, pattern_m2s, sizeof(pattern_m2s));
    for (i = 0; i < XFER_SIZE; i++) {
        master_tx[i] = (uint32_t)expected_master_write[i];
        master_rx[i] = 0U;
    }

    (void)osal_semaphore_wait(master_done_sem, 0);
    master_cb_ctx.status = SPI_XFER_ERROR;
    ret = spi_xfer_async(master_handle, master_tx, master_rx, XFER_SIZE);
    osal_semaphore_wait(master_done_sem, OSAL_TIMEOUT_WAIT_FOREVER);

    if (ret != 0 || master_cb_ctx.status != SPI_SUCCESS)
    {
        ERROR("Isolated Master: Stage 1 failed");
        success = false;
        goto master_out;
    }

    /* Stage 2: read pattern_s2m back from the slave via DMA. Same pacing,
     * this time to let the slave arm its DMA TX. */
    PRINT("Isolated Master: Delaying 1s. Arming Stage 2 DMA Read...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (i = 0; i < XFER_SIZE; i++)
    {
        master_tx[i] = 0U; /* Dummy bytes */
        master_rx[i] = 0U;
    }

    (void)osal_semaphore_wait(master_done_sem, 0);
    master_cb_ctx.status = SPI_XFER_ERROR;
    ret = spi_xfer_async(master_handle, master_tx, master_rx, XFER_SIZE);
    osal_semaphore_wait(master_done_sem, OSAL_TIMEOUT_WAIT_FOREVER);

    if (ret != 0 || master_cb_ctx.status != SPI_SUCCESS)
    {
        ERROR("Isolated Master: Stage 2 failed");
        success = false;
        goto master_out;
    }

    /* Both stages are done; verify the data received in stage 2. */
    cache_force_invalidate(master_rx, XFER_SIZE);

    /* Generate expected buffer for Pattern 2 */
    fill_pattern(expected_master_read, XFER_SIZE, pattern_s2m, sizeof(pattern_s2m));

    if (memcmp(expected_master_read, master_rx, XFER_SIZE) != 0)
    {
        ERROR("Isolated Master: FAILED - Received data does not match pattern_s2m");
        success = false;
    }
    else
    {
        PRINT("Isolated Master: PASSED - Successfully verified pattern_s2m from Slave!");
    }

master_out:
    if (master_handle != NULL) spi_close(master_handle);
    vTaskDelete(NULL);
}

void spi_master_isolated_task(void)
{
    TaskHandle_t master_task_handle = NULL;
    if (xTaskCreate(spi_master_isolated_xfer_task, "SPI_Master_Iso", TASK_STACK_SIZE,
            NULL, MASTER_TASK_PRIORITY, &master_task_handle) == pdPASS)
    {

    }
}
