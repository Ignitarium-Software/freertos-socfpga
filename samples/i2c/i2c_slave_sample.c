/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * I2C slave sample — interrupt-driven (byte callback) path
 */

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "socfpga_fpga_manager.h"
#include "socfpga_i2c.h"
#include "socfpga_mmc.h"

#include "osal.h"
#include "osal_log.h"

/**
 * @defgroup i2c_slave_sample I2C
 * @ingroup samples
 *
 * Sample application for I2C slave loopback (IRQ callbacks).
 *
 * @details
 * This sample configures I2C0 as master and I2C1 as slave, connected by the
 * FPGA fabric. The master writes a buffer to the slave, then reads back a
 * different buffer from the slave. The slave side uses byte callbacks to
 * receive and provide data.
 *
 * System flow overview
 * - Load core.rbf to wire I2C0 <-> I2C1 in the FPGA fabric.
 * - Create two tasks:
 *   - Slave task sets I2C1 to slave mode and arms a buffer for RX/TX.
 *   - Master task sets I2C0 to master mode and performs write/read transfers.
 * - The tasks coordinate with semaphores so the master only transfers when the
 *   slave is ready.
 * - Each transfer is verified and results are printed to UART.
 *
 * Prerequisites
 * - The FPGA bitstream (`/core.rbf`) must be present on the SD card.
 * - The bitstream must connect I2C0 <-> I2C1 for loopback.
 *
 * How to run
 * 1. Set `I2C_SAMPLE_ENABLE_SLAVE` to 1 in `samples/i2c/main.c`.
 * 2. Build and flash the application.
 * 3. Observe UART for per-iteration status messages.
 */

/* Configuration */
#define MASTER_INST         0U
#define SLAVE_INST          1U
#define SLAVE_ADDR          0x42U
#define XFER_SIZE           256U

#ifndef SLAVE_NUM_ITERS
#define SLAVE_NUM_ITERS     4U
#endif

#define SYNC_TIMEOUT_MS     pdMS_TO_TICKS(2000U)
#define RBF_FILE            "/core.rbf"

static uint8_t master_tx[XFER_SIZE];
static uint8_t master_rx[XFER_SIZE];
static uint8_t slave_rx[XFER_SIZE];
static uint8_t slave_tx[XFER_SIZE];

/*
 * Inter-task semaphores
 * - slave_ready_sem: slave signals it's armed for the phase
 * - signal_slave_sem: master signals verification complete
 * - rx_done_sem: ISR signals RX completion
 *
 * The handshake is simple:
 * 1) Slave posts slave_ready_sem when ready.
 * 2) Master does the transfer and verifies.
 * 3) Master posts signal_slave_sem so slave can move to next phase.
 */
static osal_semaphore_def_t slave_ready_sem_mem;
static osal_semaphore_t slave_ready_sem;

static osal_semaphore_def_t signal_slave_sem_mem;
static osal_semaphore_t signal_slave_sem;

static osal_semaphore_def_t rx_done_sem_mem;
static osal_semaphore_t rx_done_sem;

static volatile bool is_stop_xfers;

typedef struct
{
    volatile uint32_t rx_len;
    uint32_t expected_rx_len;
    uint8_t *rx_buf;
    osal_semaphore_t rx_done_sem;
    uint8_t *tx_buf;
    uint32_t tx_len;
    volatile uint32_t tx_off;
} slave_ctx_t;

static slave_ctx_t ctx;

static void fill_buf(uint8_t *buf, uint32_t len, uint8_t seed)
{
    uint32_t i;

    for (i = 0U; i < len; i++)
    {
        buf[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

/*
 * Slave callbacks (ISR context)
 * - write_requested: start of a write transaction; reset RX counter
 * - write_received: byte arrived from master; store and post completion
 * - read_requested: master is about to read; provide current TX byte
 * - read_processed: byte consumed by master; advance TX offset
 * - stop: optional end-of-transaction hook (unused here)
 *
 * Notes for application writers:
 * - Keep callbacks short; they run in interrupt context.
 * - Use write_received to build the RX buffer one byte at a time.
 * - Use read_requested/read_processed to stream out a TX buffer.
 * - If you need larger payloads, increase XFER_SIZE and buffers.
 */
static void handle_slave_write_requested(i2c_handle_t hi2c, void *param)
{
    slave_ctx_t *c = (slave_ctx_t *)param;

    (void)hi2c;
    if (c != NULL)
    {
        c->rx_len = 0U;
    }
}

static void handle_slave_write_received(i2c_handle_t hi2c, uint8_t data, void *param)
{
    slave_ctx_t *c = (slave_ctx_t *)param;

    (void)hi2c;
    if (c == NULL)
    {
        return;
    }
    if (c->rx_len < c->expected_rx_len)
    {
        c->rx_buf[c->rx_len] = data;
        c->rx_len++;
    }
    if ((c->rx_done_sem != NULL) && (c->rx_len >= c->expected_rx_len))
    {
        (void)osal_semaphore_post(c->rx_done_sem);
    }
}

static void handle_slave_read_requested(i2c_handle_t hi2c, uint8_t *data, void *param)
{
    slave_ctx_t *c = (slave_ctx_t *)param;

    (void)hi2c;
    if ((c == NULL) || (data == NULL))
    {
        return;
    }
    if (c->tx_off < c->tx_len)
    {
        *data = c->tx_buf[c->tx_off];
    }
}

static void handle_slave_read_processed(i2c_handle_t hi2c, uint8_t *data, void *param)
{
    slave_ctx_t *c = (slave_ctx_t *)param;

    (void)hi2c;
    (void)data;
    if ((c != NULL) && (c->tx_off < c->tx_len))
    {
        c->tx_off++;
    }
}

static void handle_slave_stop(i2c_handle_t hi2c, void *param)
{
    (void)hi2c;
    (void)param;
}

static int load_bitstream(void)
{
    uint8_t *rbf;
    uint32_t fsize = 0U;
    int ret;

    /* Load the FPGA fabric that connects I2C0 and I2C1. */
    rbf = mmc_read_file(SOURCE_SDMMC, RBF_FILE, &fsize);
    if (rbf == NULL)
    {
        ERROR("Failed to read bitstream");
        return -1;
    }

    ret = load_fpga_bitstream(rbf, fsize);
    vPortFree(rbf);
    if (ret != 0)
    {
        ERROR("FPGA configuration failed");
        return ret;
    }

    PRINT("Loaded bitstream %s", RBF_FILE);
    return 0;
}

static void i2c_slave_xfer_task(void *arg)
{
    i2c_handle_t slave = NULL;
    i2c_slave_config_t scfg = { 0 };
    uint32_t iter;
    uint32_t i;
    int32_t ret;

    (void)arg;

    slave = i2c_open(SLAVE_INST);
    if (slave == NULL)
    {
        ERROR("Slave: failed to open I2C%u", (unsigned)SLAVE_INST);
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        osal_task_delete();
        return;
    }

    /* Configure I2C1 as a slave with IRQ callbacks. */
    scfg.slave_address = SLAVE_ADDR;
    scfg.is_10bit_addr = false;
    scfg.tx_default_byte = 0xEEU;
    scfg.rx_tl = 0U;
    scfg.tx_tl = 0U;
    scfg.write_requested_cb = handle_slave_write_requested;
    scfg.write_received_cb = handle_slave_write_received;
    scfg.read_requested_cb = handle_slave_read_requested;
    scfg.read_processed_cb = handle_slave_read_processed;
    scfg.stop_cb = handle_slave_stop;
    scfg.cb_usercontext = &ctx;

    if (i2c_ioctl(slave, I2C_SLAVE_INIT, &scfg) != 0)
    {
        ERROR("Slave: I2C_SLAVE_INIT failed");
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }
    PRINT("Slave: configured on I2C%u addr 0x%02X", (unsigned)SLAVE_INST,
            (unsigned)SLAVE_ADDR);

    for (iter = 0U; (iter < SLAVE_NUM_ITERS) && !is_stop_xfers; iter++)
    {
        /* Phase 1: receive from master (IRQ callbacks fill slave_rx). */
        (void)memset(slave_rx, 0, sizeof(slave_rx));
        ctx.rx_len = 0U;
        ctx.expected_rx_len = XFER_SIZE;
        ctx.rx_buf = slave_rx;
        (void)osal_semaphore_post(slave_ready_sem);
        (void)osal_semaphore_wait(signal_slave_sem, portMAX_DELAY);

        /* Phase 2: transmit to master (read callbacks stream slave_tx). */
        for (i = 0U; i < XFER_SIZE; i++)
        {
            slave_tx[i] = (uint8_t)(0xA0U + (uint8_t)iter + (uint8_t)i);
        }
        ctx.tx_buf = slave_tx;
        ctx.tx_len = XFER_SIZE;
        ctx.tx_off = 0U;
        (void)osal_semaphore_post(slave_ready_sem);
        (void)osal_semaphore_wait(signal_slave_sem, portMAX_DELAY);
    }

out:
    (void)osal_semaphore_post(slave_ready_sem);

    ret = i2c_ioctl(slave, I2C_SLAVE_DEINIT, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_SLAVE_DEINIT failed (%d)", (int)ret);
    }
    ret = i2c_close(slave);
    if (ret != 0)
    {
        ERROR("Slave: i2c_close failed (%d)", (int)ret);
    }
    osal_task_delete();
}

static void i2c_master_xfer_task(void *arg)
{
    i2c_handle_t master = NULL;
    i2c_config_t mcfg = { 0 };
    uint16_t saddr = SLAVE_ADDR;
    uint32_t iter;
    int32_t ret;
    int32_t xfer_ret = 0;

    (void)arg;

    master = i2c_open(MASTER_INST);
    if (master == NULL)
    {
        ERROR("Master: failed to open I2C%u", (unsigned)MASTER_INST);
        is_stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        osal_task_delete();
        return;
    }

    /* Configure I2C0 as master and set slave address. */
    mcfg.clk = I2C_STANDARD_MODE_BPS;
    ret = i2c_ioctl(master, I2C_SET_MASTER_CFG, &mcfg);
    if (ret != 0)
    {
        ERROR("Master: I2C_SET_MASTER_CFG failed (%d)", (int)ret);
        is_stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        xfer_ret = ret;
        goto out;
    }
    ret = i2c_ioctl(master, I2C_SET_SLAVE_ADDR, (void *)&saddr);
    if (ret != 0)
    {
        ERROR("Master: I2C_SET_SLAVE_ADDR failed (%d)", (int)ret);
        is_stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        xfer_ret = ret;
        goto out;
    }
    PRINT("Master: configured on I2C%u -> slave 0x%02X", (unsigned)MASTER_INST,
            (unsigned)SLAVE_ADDR);

    for (iter = 0U; (iter < SLAVE_NUM_ITERS) && (xfer_ret == 0); iter++)
    {
        fill_buf(master_tx, XFER_SIZE, (uint8_t)iter);
        (void)memset(master_rx, 0, sizeof(master_rx));

        /* Phase 1: master write → slave RX */
        if (osal_semaphore_wait(slave_ready_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Master: iter %u — timeout waiting for slave RX ready", iter);
            xfer_ret = -ETIMEDOUT;
            break;
        }
        (void)osal_semaphore_wait(rx_done_sem, 0);

        ret = i2c_write_sync(master, master_tx, XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Master: iter %u — i2c_write_sync failed (%d)", iter, (int)ret);
            xfer_ret = ret;
            break;
        }
        PRINT("Master: iter %u — write to slave OK", iter);
        if (osal_semaphore_wait(rx_done_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Master: iter %u — slave RX did not complete", iter);
            xfer_ret = -ETIMEDOUT;
            break;
        }
        if (memcmp(master_tx, slave_rx, XFER_SIZE) != 0)
        {
            ERROR("Master: iter %u — write loopback mismatch", iter);
            xfer_ret = -EIO;
            break;
        }
        PRINT("Slave: iter %u — RX verified", iter);
        (void)osal_semaphore_post(signal_slave_sem);

        /* Phase 2: master read ← slave TX */
        if (osal_semaphore_wait(slave_ready_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Master: iter %u — timeout waiting for slave TX ready", iter);
            xfer_ret = -ETIMEDOUT;
            break;
        }

        ret = i2c_read_sync(master, master_rx, XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Master: iter %u — i2c_read_sync failed (%d)", iter, (int)ret);
            xfer_ret = ret;
            break;
        }
        PRINT("Master: iter %u — read from slave OK", iter);
        if (memcmp(master_rx, slave_tx, XFER_SIZE) != 0)
        {
            ERROR("Master: iter %u — read loopback mismatch", iter);
            xfer_ret = -EIO;
            break;
        }
        PRINT("Slave: iter %u — TX verified", iter);

        PRINT("I2C slave IRQ sample: iter %u/%u OK", iter + 1U, (unsigned)SLAVE_NUM_ITERS);
        (void)osal_semaphore_post(signal_slave_sem);
    }

out:
    ret = i2c_close(master);
    if (ret != 0)
    {
        ERROR("Master: i2c_close failed (%d)", (int)ret);
    }
    if (xfer_ret != 0)
    {
        ERROR("Master: exiting with error (%d)", (int)xfer_ret);
    }
    is_stop_xfers = true;
    (void)osal_semaphore_post(signal_slave_sem);
    osal_task_delete();
}

int i2c_slave_sample(void)
{
    bool master_task_ok;
    bool slave_task_ok;

    PRINT("I2C slave IRQ sample: master<->slave loopback");

    is_stop_xfers = false;
    (void)memset(&ctx, 0, sizeof(ctx));

    /* Ensure the I2C loopback wiring is present in the FPGA fabric. */
    if (load_bitstream() != 0)
    {
        return -EIO;
    }

    slave_ready_sem = osal_semaphore_create(&slave_ready_sem_mem);
    signal_slave_sem = osal_semaphore_create(&signal_slave_sem_mem);
    rx_done_sem = osal_semaphore_create(&rx_done_sem_mem);

    if ((slave_ready_sem == NULL) || (signal_slave_sem == NULL) || (rx_done_sem == NULL))
    {
        ERROR("Failed to create semaphores");
        return -ENOMEM;
    }
    ctx.rx_done_sem = rx_done_sem;

    slave_task_ok = osal_task_create(i2c_slave_xfer_task, "i2c_slave",
            NULL, configMAX_PRIORITIES - 2U);
    if (!slave_task_ok)
    {
        ERROR("Failed to create slave task");
        return -ENOMEM;
    }

    master_task_ok = osal_task_create(i2c_master_xfer_task, "i2c_master",
            NULL, configMAX_PRIORITIES - 3U);
    if (!master_task_ok)
    {
        ERROR("Failed to create master task");
        return -ENOMEM;
    }

    PRINT("I2C slave IRQ sample started");
    return 0;
}

void i2c_slave_task(void)
{
    (void)i2c_slave_sample();
}
