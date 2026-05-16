/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * I2C slave sample — DMA transfer path
 */

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "socfpga_cache.h"
#include "socfpga_dma.h"
#include "socfpga_fpga_manager.h"
#include "socfpga_i2c.h"
#include "socfpga_mmc.h"

#include "osal.h"
#include "osal_log.h"

/**
 * @defgroup i2c_slave_dma_sample I2C
 * @ingroup samples
 *
 * Sample application for I2C slave loopback (DMA).
 *
 * @details
 * This sample configures I2C0 as master and I2C1 as slave, connected by the
 * FPGA fabric. The slave uses RDMA for master writes and WDMA for master reads.
 * The master performs synchronous transfers, while the slave uses async DMA
 * and a completion callback.
 *
 * System flow overview
 * - Load core.rbf to wire I2C0 <-> I2C1 in the FPGA fabric.
 * - Create two tasks:
 *   - Slave task sets I2C1 to slave mode, opens DMA, and arms RDMA/WDMA.
 *   - Master task sets I2C0 to master mode and performs write/read transfers.
 * - Each DMA phase is armed on the slave before the master starts the transfer.
 * - The DMA completion callback signals the slave task to advance.
 *
 * Prerequisites
 * - The FPGA bitstream (`/core.rbf`) must be present on the SD card.
 * - The bitstream must connect I2C0 <-> I2C1 for loopback.
 *
 * How to run
 * 1. Set `I2C_SAMPLE_ENABLE_SLAVE_DMA` to 1 in `samples/i2c/main.c`.
 * 2. Build and flash the application.
 * 3. Observe UART for per-iteration status messages.
 */

/*
 * A) Peripheral DMA limitation:
 * The Peripherals are on APB bus and it has limited features compared to the
 * AXI bus, Here all transactions has to be 32bit wide and cannot do byte
 * transfers, Below diagram shows the register setup
 *
 * The data register is 32bit wide and in that only 8bits are valid, due to which
 * the remaining 24bits needs to discarded, but the DMA cannot distinguish this
 * and write 4bytes from the buffer to register/fifo and will lose 3bytes of data
 *
 * So the solution is to extend the data to be 4bytes wide even though out data
 * if only 1 byte wide (pad with zeros)
 * This is why we need uint32_t arrays for TX data buffers
 *
 * Important:
 * In the reverse direction (peripheral -> Memory), The AXI bridge will discard
 * dontcare bits and only present 1byte to the DMA (AXI can do this, but APB cant)
 * Due to which we don't require this wide arrays in this direction
 *
 *
 *                    Peripheral data Reg.                               --+
 *                   +--------------+-------+   +--------------+-------+   |
 *                   | dont care    |data   |   | dont care    |data   |   |
 *                   +--------------+-------+   +--------------+-------+   |
 *                                              | dont care    |data   |   |
 *                                              +--------------+-------+   |
 *                                              | dont care    |data   |   |
 *                                              +--------------+-------+   |
 *                                              | dont care    |data   |   |  Peripheral
 *                                              +---------+----+-------+   +- FIFO
 *                                                        |                |
 *                                                        |                |
 *         +--                                            |                |
 *         |   +--------------+-------+   APB   +---------+----+-------+   |
 *         |   |              |data   | <-----> | dont care    |data   |   |
 *         |   +--------------+-------+         +--------------+-------+   |
 *         |   |              |data   |                 Read end         --+
 *         |   +--------------+-------+
 *         |   |              |data   |
 * DMA    -+   +--------------+-------+
 * Buffer  |             -
 *         |   +----------------------+
 *         |   |                      |
 *         |   +----------------------+
 *         +--
 */

/* Configuration */
#define MASTER_INST             0U
#define SLAVE_INST              1U
#define SLAVE_ADDR              0x42U
#define XFER_SIZE               1024U

#ifndef SLAVE_DMA_NUM_ITERS
#define SLAVE_DMA_NUM_ITERS     4U
#endif

#define SYNC_TIMEOUT_MS         pdMS_TO_TICKS(3000U)
#define RBF_FILE                "/core.rbf"

/* DMA channel assignments for the slave */
#define SLAVE_TX_DMA_INST       DMA_INSTANCE0
#define SLAVE_TX_DMA_CH         DMA_CH3
#define SLAVE_RX_DMA_INST       DMA_INSTANCE0
#define SLAVE_RX_DMA_CH         DMA_CH4

static uint32_t slave_dma_tx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t slave_dma_rx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t master_tx[XFER_SIZE];
static uint8_t master_rx[XFER_SIZE];

/*
 * Inter-task semaphores
 * - slave_ready_sem: slave signals it's armed for the phase
 * - slave_done_sem: DMA completion callback posts
 * - signal_slave_sem: master signals verification complete
 * - slave_exit_sem: slave posts on exit
 *
 * The DMA flow is:
 * 1) Slave arms DMA and posts slave_ready_sem.
 * 2) Master starts the transfer.
 * 3) DMA completion callback posts slave_done_sem.
 * 4) Master verifies and posts signal_slave_sem for the next phase.
 */
static osal_semaphore_def_t slave_ready_sem_mem;
static osal_semaphore_t slave_ready_sem;

static osal_semaphore_def_t slave_done_sem_mem;
static osal_semaphore_t slave_done_sem;

static osal_semaphore_def_t slave_rx_done_sem_mem;
static osal_semaphore_t slave_rx_done_sem;

static osal_semaphore_def_t signal_slave_sem_mem;
static osal_semaphore_t signal_slave_sem;

static osal_semaphore_def_t slave_exit_sem_mem;
static osal_semaphore_t slave_exit_sem;

static volatile bool is_stop_xfers;
static volatile bool is_slave_ok;
static volatile bool is_master_ok;

typedef struct
{
    osal_semaphore_t done_sem;
    volatile i2c_op_status_t status;
} dma_cb_ctx_t;

static dma_cb_ctx_t slave_cb_ctx;

static void handle_slave_dma_complete(i2c_op_status_t status, void *param)
{
    dma_cb_ctx_t *cb = (dma_cb_ctx_t *)param;

    if (cb != NULL)
    {
        cb->status = status;
        (void)osal_semaphore_post(cb->done_sem);
    }
}

/*
 * Stub callbacks required by I2C_SLAVE_INIT. These are not used when DMA is
 * enabled, but they must be valid.
 *
 * Application note:
 * - If you want a mixed DMA/IRQ design, replace these with real callbacks.
 */
static void handle_stub_write_requested(i2c_handle_t handle, void *param)
{
    (void)handle;
    (void)param;
}

static void handle_stub_write_received(i2c_handle_t handle, uint8_t data, void *param)
{
    (void)handle;
    (void)data;
    (void)param;
}

static void handle_stub_read_requested(i2c_handle_t handle, uint8_t *data, void *param)
{
    (void)handle;
    (void)data;
    (void)param;
}

static void handle_stub_read_processed(i2c_handle_t handle, uint8_t *data, void *param)
{
    (void)handle;
    (void)data;
    (void)param;
}

static void handle_stub_stop(i2c_handle_t handle, void *param)
{
    (void)handle;
    (void)param;
}

static void fill_buf(uint8_t *buf, uint32_t len, uint8_t seed)
{
    uint32_t i;

    for (i = 0U; i < len; i++)
    {
        buf[i] = (uint8_t)(seed + (uint8_t)i);
    }
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
    i2c_dma_config_t dma_cfg = { 0 };
    uint32_t iter;
    uint32_t i;
    bool is_ok = true;
    int32_t ret;

    (void)arg;

    slave = i2c_open(SLAVE_INST);
    if (slave == NULL)
    {
        ERROR("Slave: failed to open I2C%u", (unsigned)SLAVE_INST);
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }

    /* Configure I2C1 as a slave with DMA enabled. */
    scfg.slave_address = SLAVE_ADDR;
    scfg.is_10bit_addr = false;
    scfg.tx_default_byte = 0xEEU;
    scfg.rx_tl = 0U;
    scfg.tx_tl = 0U;
    scfg.write_requested_cb = handle_stub_write_requested;
    scfg.write_received_cb = handle_stub_write_received;
    scfg.read_requested_cb = handle_stub_read_requested;
    scfg.read_processed_cb = handle_stub_read_processed;
    scfg.stop_cb = handle_stub_stop;
    scfg.cb_usercontext = NULL;
    if (i2c_ioctl(slave, I2C_SLAVE_INIT, &scfg) != 0)
    {
        ERROR("Slave: I2C_SLAVE_INIT failed");
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }
    PRINT("Slave: configured on I2C%u addr 0x%02X", (unsigned)SLAVE_INST,
            (unsigned)SLAVE_ADDR);

    /* Open and enable DMA channels for slave transfers. */
    dma_cfg.tx_instance = SLAVE_TX_DMA_INST;
    dma_cfg.tx_channel = SLAVE_TX_DMA_CH;
    dma_cfg.tx_prio = 0U;
    dma_cfg.rx_instance = SLAVE_RX_DMA_INST;
    dma_cfg.rx_channel = SLAVE_RX_DMA_CH;
    dma_cfg.rx_prio = 1U;
    if (i2c_ioctl(slave, I2C_OPEN_DMA, &dma_cfg) != 0)
    {
        ERROR("Slave: I2C_OPEN_DMA failed");
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }
    PRINT("Slave: DMA enabled (TX ch %u, RX ch %u)",
            (unsigned)SLAVE_TX_DMA_CH, (unsigned)SLAVE_RX_DMA_CH);
    ret = i2c_ioctl(slave, I2C_ENABLE_WDMA, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_ENABLE_WDMA failed (%d)", (int)ret);
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }
    ret = i2c_ioctl(slave, I2C_ENABLE_RDMA, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_ENABLE_RDMA failed (%d)", (int)ret);
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }

    slave_cb_ctx.done_sem = slave_done_sem;
    slave_cb_ctx.status = I2C_OP_FAIL;
    i2c_set_callback(slave, handle_slave_dma_complete, &slave_cb_ctx);

    for (iter = 0U; (iter < SLAVE_DMA_NUM_ITERS) && is_ok && !is_stop_xfers; iter++)
    {
        /*
 		 * We need to build a 32 bit wide intermediate buffer and copy the data byte
 	     * by byte to each word. We will be setting control flags later.
		 *
	 	 * The macro I2C_DMA_CMD_DATA can be used to typecast data from uint8_t to uint32_t
		 */

        /* Phase 1: RX DMA (master writes) */
        (void)memset(slave_dma_rx, 0, sizeof(slave_dma_rx));
        (void)osal_semaphore_wait(slave_done_sem, 0);
        slave_cb_ctx.status = I2C_OP_FAIL;

        ret = i2c_read_async(slave, slave_dma_rx, XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Slave: iter %u — i2c_read_async failed (%d)", iter, (int)ret);
            is_ok = false;
            is_stop_xfers = true;
            (void)osal_semaphore_post(slave_ready_sem);
            break;
        }

        (void)osal_semaphore_post(slave_ready_sem);

        if (osal_semaphore_wait(slave_done_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Slave: iter %u — RX DMA timed out", iter);
            is_ok = false;
            is_stop_xfers = true;
            break;
        }
        if (slave_cb_ctx.status != I2C_SUCCESS)
        {
            ERROR("Slave: iter %u — RX DMA failed", iter);
            is_ok = false;
            is_stop_xfers = true;
            break;
        }

        /* Signal master that RX DMA completed. */
        (void)osal_semaphore_post(slave_rx_done_sem);

        /* Wait for master to verify RX before proceeding to TX. */
        if (osal_semaphore_wait(signal_slave_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Slave: iter %u — timeout waiting for master RX verify", iter);
            is_ok = false;
            is_stop_xfers = true;
            break;
        }

		/*
 		 * We need to build a 32 bit wide intermediate buffer and copy the data byte
 	     * by byte to each word. We will be setting control flags later.
		 *
	 	 * The macro I2C_DMA_CMD_DATA can be used to typecast data from uint8_t to uint32_t
		 */
        /* Phase 2: TX DMA (master reads) */
        for (i = 0U; i < XFER_SIZE; i++)
        {
            slave_dma_tx[i] = (uint32_t)(uint8_t)(0xA0U + (uint8_t)iter + (uint8_t)i);
        }
        cache_force_write_back((void *)slave_dma_tx, sizeof(slave_dma_tx));

        (void)osal_semaphore_wait(slave_done_sem, 0);
        slave_cb_ctx.status = I2C_OP_FAIL;

        ret = i2c_write_async(slave, slave_dma_tx, XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Slave: iter %u — i2c_write_async failed (%d)", iter, (int)ret);
            is_ok = false;
            is_stop_xfers = true;
            (void)osal_semaphore_post(slave_ready_sem);
            break;
        }

        (void)osal_semaphore_post(slave_ready_sem);

        if (osal_semaphore_wait(slave_done_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Slave: iter %u — TX DMA timed out", iter);
            is_ok = false;
            is_stop_xfers = true;
            break;
        }
        if (slave_cb_ctx.status != I2C_SUCCESS)
        {
            ERROR("Slave: iter %u — TX DMA failed", iter);
            is_ok = false;
            is_stop_xfers = true;
            break;
        }

        if (osal_semaphore_wait(signal_slave_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Slave: iter %u — timeout waiting for master verification", iter);
            is_ok = false;
            break;
        }
    }

    ret = i2c_ioctl(slave, I2C_DISABLE_WDMA, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_DISABLE_WDMA failed (%d)", (int)ret);
    }
    ret = i2c_ioctl(slave, I2C_DISABLE_RDMA, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_DISABLE_RDMA failed (%d)", (int)ret);
    }
    ret = i2c_ioctl(slave, I2C_CLOSE_DMA, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_CLOSE_DMA failed (%d)", (int)ret);
    }
    ret = i2c_ioctl(slave, I2C_SLAVE_DEINIT, NULL);
    if (ret != 0)
    {
        ERROR("Slave: I2C_SLAVE_DEINIT failed (%d)", (int)ret);
    }

out:
    if (slave != NULL)
    {
        (void)i2c_close(slave);
    }
    is_slave_ok = is_ok;
    (void)osal_semaphore_post(slave_exit_sem);
    osal_task_delete();
}

static void i2c_master_xfer_task(void *arg)
{
    i2c_handle_t master = NULL;
    i2c_config_t mcfg = { 0 };
    uint16_t saddr = SLAVE_ADDR;
    uint32_t iter;
    int32_t ret;
    bool is_ok = true;

    (void)arg;

    master = i2c_open(MASTER_INST);
    if (master == NULL)
    {
        ERROR("Master: failed to open I2C%u", (unsigned)MASTER_INST);
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        goto out;
    }

    /* Configure I2C0 as master and set slave address. */
    mcfg.clk = I2C_STANDARD_MODE_BPS;
    ret = i2c_ioctl(master, I2C_SET_MASTER_CFG, &mcfg);
    if (ret != 0)
    {
        ERROR("Master: I2C_SET_MASTER_CFG failed (%d)", (int)ret);
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        goto out;
    }
    ret = i2c_ioctl(master, I2C_SET_SLAVE_ADDR, (void *)&saddr);
    if (ret != 0)
    {
        ERROR("Master: I2C_SET_SLAVE_ADDR failed (%d)", (int)ret);
        is_ok = false;
        is_stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        goto out;
    }
    PRINT("Master: configured on I2C%u -> slave 0x%02X", (unsigned)MASTER_INST,
            (unsigned)SLAVE_ADDR);

    for (iter = 0U; (iter < SLAVE_DMA_NUM_ITERS) && is_ok; iter++)
    {
        fill_buf(master_tx, XFER_SIZE, (uint8_t)iter);
        (void)memset(master_rx, 0, sizeof(master_rx));

        (void)osal_semaphore_wait(signal_slave_sem, 0);
        (void)osal_semaphore_wait(slave_rx_done_sem, 0);

        /* Phase 1: master write → slave RDMA */
        if (osal_semaphore_wait(slave_ready_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Master: iter %u — timeout waiting for slave RX ready", iter);
            is_ok = false;
            break;
        }
        ret = i2c_write_sync(master, master_tx, XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Master: iter %u — i2c_write_sync failed (%d)", iter, (int)ret);
            is_ok = false;
            break;
        }
        PRINT("Master: iter %u — write to slave OK", iter);

        if (osal_semaphore_wait(slave_rx_done_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Master: iter %u — timeout waiting for slave RX DMA", iter);
            is_ok = false;
            break;
        }
        /* RX DMA updates slave_dma_rx; invalidate before compare. */
        cache_force_invalidate(slave_dma_rx, XFER_SIZE);
        if (memcmp(master_tx, slave_dma_rx, XFER_SIZE) != 0)
        {
            ERROR("Master: iter %u — write loopback mismatch", iter);
            is_ok = false;
            is_stop_xfers = true;
            (void)osal_semaphore_post(signal_slave_sem);
            break;
        }
        PRINT("Slave: iter %u — RX DMA verified", iter);

        /* Allow slave to proceed to TX phase. */
        (void)osal_semaphore_post(signal_slave_sem);

        /* Phase 2: master read ← slave WDMA */
        if (osal_semaphore_wait(slave_ready_sem, SYNC_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Master: iter %u — timeout waiting for slave TX ready", iter);
            is_ok = false;
            break;
        }
        ret = i2c_read_sync(master, master_rx, XFER_SIZE);
        if (ret != 0)
        {
            ERROR("Master: iter %u — i2c_read_sync failed (%d)", iter, (int)ret);
            is_ok = false;
            break;
        }
        PRINT("Master: iter %u — read from slave OK", iter);

        for (uint32_t i = 0U; i < XFER_SIZE; i++)
        {
            uint8_t expected = (uint8_t)(0xA0U + (uint8_t)iter + (uint8_t)i);
            if (master_rx[i] != expected)
            {
                ERROR("Master: iter %u — read loopback mismatch", iter);
                is_ok = false;
                break;
            }
        }
        if (is_ok)
        {
            PRINT("Slave: iter %u — TX DMA verified", iter);
        }
        if (!is_ok)
        {
            break;
        }

        PRINT("I2C slave DMA sample: iter %u/%u OK", iter + 1U,
                (unsigned)SLAVE_DMA_NUM_ITERS);
        (void)osal_semaphore_post(signal_slave_sem);
    }

out:
    if (master != NULL)
    {
        (void)i2c_close(master);
    }
    is_master_ok = is_ok;
    is_stop_xfers = true;
    (void)osal_semaphore_post(signal_slave_sem);
    osal_task_delete();
}

int i2c_slave_dma_sample(void)
{
    bool master_task_ok;
    bool slave_task_ok;

    PRINT("I2C slave DMA sample: master<->slave loopback");

    is_stop_xfers = false;
    is_slave_ok = false;
    is_master_ok = false;

    /* Ensure the I2C loopback wiring is present in the FPGA fabric. */
    if (load_bitstream() != 0)
    {
        return -EIO;
    }

    slave_ready_sem = osal_semaphore_create(&slave_ready_sem_mem);
    slave_done_sem = osal_semaphore_create(&slave_done_sem_mem);
    slave_rx_done_sem = osal_semaphore_create(&slave_rx_done_sem_mem);
    signal_slave_sem = osal_semaphore_create(&signal_slave_sem_mem);
    slave_exit_sem = osal_semaphore_create(&slave_exit_sem_mem);

    if ((slave_ready_sem == NULL) || (slave_done_sem == NULL) ||
        (slave_rx_done_sem == NULL) || (signal_slave_sem == NULL) ||
        (slave_exit_sem == NULL))
    {
        ERROR("Failed to create semaphores");
        return -ENOMEM;
    }

    slave_task_ok = osal_task_create(i2c_slave_xfer_task, "i2c_slave_dma",
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

    PRINT("I2C slave DMA sample started");

    if (osal_semaphore_wait(slave_exit_sem, SYNC_TIMEOUT_MS) != pdTRUE)
    {
        ERROR("Slave task did not exit in time");
        return -ETIMEDOUT;
    }

    if (!is_slave_ok || !is_master_ok)
    {
        ERROR("I2C slave DMA sample failed");
        return -EIO;
    }

    return 0;
}

void i2c_slave_dma_task(void)
{
    (void)i2c_slave_dma_sample();
}
