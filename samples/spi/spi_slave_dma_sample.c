/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SPI slave DMA transfers
 */

/**
 * @file spi_slave_dma_sample.c
 * @brief SPI master to slave loopback sample (DMA)
 *
 * This sample mirrors the structure/handshake of samples/spi/spi_slave_sample.c
 * but uses DMA-backed asynchronous transfers on both SPIM1 (master) and SPIS0
 * (slave).
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
#include "socfpga_timer.h"
#include "socfpga_mmc.h"
#include "socfpga_fpga_manager.h"
#include "socfpga_cache.h"

#include "osal.h"
#include "osal_log.h"

/**
 * @defgroup spi_slave_dma_sample SPI Slave Loopback (DMA)
 * @ingroup samples
 *
 * SPI master to slave loopback sample using DMA-backed async transfers
 *
 * @details
 * @section spi_slave_dma_desc Description
 * This sample runs SPIM1 (master) and SPIS0 (slave) in the same application and
 * performs full-duplex loopback using @c spi_xfer_async() with DMA enabled.
 *
 * Because the SPI DMA path enforces a maximum transfer size, the sample splits
 * each iteration into fixed-size segments and handshakes per segment so the
 * slave arms the next async transfer before the master clocks it.
 *
 * The sample also measures average time per iteration using @c TIMER_SYS0 and
 * prints throughput in KiB/s for master and slave.
 *
 * @section spi_slave_dma_pre Prerequisites
 * - The FPGA bitstream (@c /core.rbf) must be present on the SD card.
 * - DMA must be available and the selected DMA channels must be free.
 *
 * @section spi_slave_dma_param Configurable Parameters
 * - Instances and slave select: @c SPI_MASTER_INSTANCE, @c SPI_SLAVE_INSTANCE,
 *   @c SLAVE_SELECT_NUM
 * - SPI frequency: @c SPI_FREQ
 * - Transfer sizing: @c XFER_SIZE, @c XFER_SEG_SIZE
 * - Iterations: @c SPI_SLAVE_DMA_SAMPLE_NUM_XFERS
 * - DMA channels: @c MASTER_DMA_*, @c SLAVE_DMA_*
 *
 * @section spi_slave_dma_how_to How to Run
 * 1. Enable this sample in @c samples/spi/main.c
 *    (see @c SPI_SAMPLE_ENABLE_SLAVE_DMA).
 * 2. Build and flash the application.
 * 3. Observe UART for pass/fail and timing/throughput output.
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

#define SPI_MASTER_INSTANCE     1U
#define SPI_SLAVE_INSTANCE      0U
#define SLAVE_SELECT_NUM        1U
#define SPI_FREQ                40000000U

/* Total loopback bytes per iteration (split into DMA-sized segments). */
#define XFER_SIZE               4096U
#define XFER_SEG_SIZE           2048U
#define XFER_NUM_SEGS           (XFER_SIZE / XFER_SEG_SIZE)

#define SLAVE_TIMEOUT_MS        2000U

#ifndef SPI_SLAVE_DMA_SAMPLE_NUM_XFERS
#define SPI_SLAVE_DMA_SAMPLE_NUM_XFERS 6U
#endif

#define RBF_FILENAME            "/core.rbf"

/*
 * DMA channel allocation.
 * Master uses CH1 (TX) and CH2 (RX); slave uses CH3 (TX) and CH4 (RX).
 */
#define MASTER_DMA_TX_INSTANCE  DMA_INSTANCE0
#define MASTER_DMA_TX_CHANNEL   DMA_CH1
#define MASTER_DMA_TX_PRIO      0U
#define MASTER_DMA_RX_INSTANCE  DMA_INSTANCE0
#define MASTER_DMA_RX_CHANNEL   DMA_CH2
#define MASTER_DMA_RX_PRIO      1U

#define SLAVE_DMA_TX_INSTANCE   DMA_INSTANCE0
#define SLAVE_DMA_TX_CHANNEL    DMA_CH3
#define SLAVE_DMA_TX_PRIO       0U
#define SLAVE_DMA_RX_INSTANCE   DMA_INSTANCE0
#define SLAVE_DMA_RX_CHANNEL    DMA_CH4
#define SLAVE_DMA_RX_PRIO       1U

/*
 * Timer configuration used to measure transfer time.
 * The timer counts down; we measure elapsed time using the delta of remaining
 * microseconds.
 */
#define TIMER_INSTANCE          TIMER_SYS0
#define TIMER_PERIOD_US         0xFFFFFFFFU

/*
 * Task priorities and stack size.
 * Keep slave above master so it can arm its transfer before the master clocks.
 */
#define SLAVE_TASK_PRIORITY     (configMAX_PRIORITIES - 2U)
#define MASTER_TASK_PRIORITY    (configMAX_PRIORITIES - 3U)
#define TASK_STACK_SIZE         (configMINIMAL_STACK_SIZE * 4U)

static volatile bool stop_xfers;

static timer_handle_t timer_handle;
static uint32_t timer_wrap_us;

/*
 * DMA TX uses uint32_t[nbytes] (one SPI byte per word, in bits [7:0]).
 * DMA RX uses uint8_t[nbytes].
 *
 * Important:
 * For SPI DMA transfer operation the byte transmit buffer has to be
 * padded to 32 bits. The receive buffer need not be padded to 32 bits
 *
 * Refer section (A) at the top, this explains why we need
 * uint32_t arrays instead of uint8_t arrays in Tx direction.
 */

static uint32_t master_tx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t master_rx[XFER_SIZE] __attribute__((aligned(64)));
static uint32_t slave_tx[XFER_SIZE] __attribute__((aligned(64)));
static uint8_t slave_rx[XFER_SIZE] __attribute__((aligned(64)));

/*
 * Handshake order per segment:
 * - Slave arms spi_xfer_async() then posts slave_ready_sem.
 * - Master waits slave_ready_sem, clocks the segment, then continues.
 * Per iteration:
 * - Master posts signal_slave_sem once after verifying the full buffer.
 */
static osal_semaphore_def_t slave_ready_sem_mem;
static osal_semaphore_t slave_ready_sem;

static osal_semaphore_def_t signal_slave_sem_mem;
static osal_semaphore_t signal_slave_sem;

static osal_semaphore_def_t master_done_sem_mem;
static osal_semaphore_t master_done_sem;

static osal_semaphore_def_t slave_done_sem_mem;
static osal_semaphore_t slave_done_sem;

static osal_semaphore_def_t slave_exit_sem_mem;
static osal_semaphore_t slave_exit_sem;

static osal_semaphore_def_t master_exit_sem_mem;
static osal_semaphore_t master_exit_sem;

static volatile bool slave_success;
static volatile bool master_success;

typedef struct
{
    osal_semaphore_t sem;
    volatile spi_xfer_status_t status;
} spi_cb_ctx_t;

static spi_cb_ctx_t master_cb_ctx;
static spi_cb_ctx_t slave_cb_ctx;

static uint32_t timer_elapsed_us(uint32_t start_rem_us, uint32_t end_rem_us,
        uint32_t wrap_us)
{
    if (start_rem_us >= end_rem_us)
    {
        return start_rem_us - end_rem_us;
    }
    return start_rem_us + (wrap_us - end_rem_us);
}

static void load_bitstream(void)
{
    uint32_t file_size;
    uint8_t *rbf_ptr;

    PRINT("Reading bitstream from SD card");
    rbf_ptr = mmc_read_file(SOURCE_SDMMC, RBF_FILENAME, &file_size);
    if (rbf_ptr == NULL)
    {
        ERROR("Unable to read bitstream");
        return;
    }

    PRINT("Configuring FPGA");
    if (load_fpga_bitstream(rbf_ptr, file_size) != 0)
    {
        ERROR("Failed to load bitstream");
        vPortFree(rbf_ptr);
        return;
    }

    vPortFree(rbf_ptr);
    PRINT("Bitstream loaded successfully");
}

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

static void spi_slave_xfer_task(void *arg)
{
    uint32_t iter;
    uint32_t i;
    uint32_t seg;
    uint32_t off = 0U;
    uint32_t total_us = 0U;
    uint32_t start_rem_us = 0U;
    uint32_t end_rem_us = 0U;
    uint32_t avg_us = 0U;
    uint64_t bps = 0U;
    uint64_t kibps = 0U;
    bool success = true;

    spi_handle_t slave_handle = NULL;
    spi_cfg_t slave_cfg = { 0 };
    spi_dma_config_t slave_dma_cfg = { 0 };
    int32_t ret;

    (void)arg;

    slave_handle = spi_slave_open(SPI_SLAVE_INSTANCE);
    if (slave_handle == NULL)
    {
        ERROR("Slave: failed to open SPI instance");
        success = false;
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }

    slave_cfg.mode = SPI_MODE0;
    slave_cfg.clk = SPI_FREQ;
    slave_cfg.role = SPI_ROLE_SLAVE;
    ret = spi_ioctl(slave_handle, SPI_SET_CONFIG, &slave_cfg);
    if (ret != 0)
    {
        ERROR("Slave: failed to configure SPI");
        success = false;
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }

    slave_dma_cfg.tx_instance = SLAVE_DMA_TX_INSTANCE;
    slave_dma_cfg.tx_channel = SLAVE_DMA_TX_CHANNEL;
    slave_dma_cfg.tx_prio = SLAVE_DMA_TX_PRIO;
    slave_dma_cfg.rx_instance = SLAVE_DMA_RX_INSTANCE;
    slave_dma_cfg.rx_channel = SLAVE_DMA_RX_CHANNEL;
    slave_dma_cfg.rx_prio = SLAVE_DMA_RX_PRIO;
    ret = spi_ioctl(slave_handle, SPI_ENABLE_DMA, &slave_dma_cfg);
    if (ret != 0)
    {
        ERROR("Slave: failed to enable DMA");
        success = false;
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }

    slave_cb_ctx = (spi_cb_ctx_t)
    {
        .sem = slave_done_sem,
        .status = SPI_XFER_ERROR,
    };
    ret = spi_set_callback(slave_handle, spi_done_callback, &slave_cb_ctx);
    if (ret != 0)
    {
        ERROR("Slave: failed to set callback");
        success = false;
        stop_xfers = true;
        (void)osal_semaphore_post(slave_ready_sem);
        goto out;
    }

    for (iter = 0; iter < SPI_SLAVE_DMA_SAMPLE_NUM_XFERS; iter++)
    {
        if (stop_xfers)
        {
            success = false;
            break;
        }

        start_rem_us = 0U;
        end_rem_us = 0U;

		/*
    	 * Important:
    	 * For SPI DMA transfer operation the byte transmit buffer has to be
    	 * padded to 32 bits. The receive buffer need not be padded to 32 bits
    	 *
    	 * Refer section (A) at the top, this explains why we need
    	 * uint32_t arrays instead of uint8_t arrays in Tx direction.
    	 */
        for (i = 0; i < XFER_SIZE; i++)
        {
            slave_tx[i] = (uint32_t)(((i + 0xA0U) + iter) & 0xFFU);
            slave_rx[i] = 0U;
        }

        if (timer_get_value_us(timer_handle, &start_rem_us) != 0)
        {
            ERROR("Slave: failed to read timer before transfer");
            success = false;
            break;
        }

        for (seg = 0; seg < XFER_NUM_SEGS; seg++)
        {
            off = seg * XFER_SEG_SIZE;

            if (stop_xfers)
            {
                success = false;
                break;
            }

            /* Ensure completion semaphore starts empty for this segment. */
            (void)osal_semaphore_wait(slave_done_sem, 0);
            slave_cb_ctx.status = SPI_XFER_ERROR;

            ret = spi_xfer_async(slave_handle, &slave_tx[off],
                    &slave_rx[off], (uint16_t)XFER_SEG_SIZE);

            /* Let the master know this segment is armed and ready. */
            (void)osal_semaphore_post(slave_ready_sem);

            if (osal_semaphore_wait(slave_done_sem, SLAVE_TIMEOUT_MS) != pdTRUE)
            {
                ERROR("Slave: timeout waiting for segment completion (seg %lu)",
                        (unsigned long)seg);
                success = false;
                break;
            }

            if (slave_cb_ctx.status != SPI_SUCCESS)
            {
                ERROR("Slave: transfer failed (status %d, seg %lu)",
                        (int)slave_cb_ctx.status, (unsigned long)seg);
                success = false;
                break;
            }

            /* SPI async DMA doesn't do post-transfer cache invalidation. */
            cache_force_invalidate(&slave_rx[off], XFER_SEG_SIZE);
        }

        if (timer_get_value_us(timer_handle, &end_rem_us) != 0)
        {
            ERROR("Slave: failed to read timer after transfer");
            success = false;
            break;
        }

        if (success == false)
        {
            break;
        }

        total_us += timer_elapsed_us(start_rem_us, end_rem_us, timer_wrap_us);

        /*
         * Master signals after verification/print, so we don't overwrite
         * buffers.
         */
        if (osal_semaphore_wait(signal_slave_sem, SLAVE_TIMEOUT_MS) != pdTRUE)
        {
            ERROR("Slave: timed out waiting for master signal");
            success = false;
            break;
        }
    }

    if (success)
    {
        avg_us = total_us / SPI_SLAVE_DMA_SAMPLE_NUM_XFERS;
        PRINT("Slave: average time per %u-byte iteration: %lu us\r\n",
                (unsigned)XFER_SIZE, (unsigned long)avg_us);

        if (avg_us != 0U)
        {
            bps = ((uint64_t)XFER_SIZE * 1000000ULL) / (uint64_t)avg_us;
            kibps = bps / 1024ULL;
            PRINT("Slave: throughput: %llu KiB/s", (unsigned long long)kibps);
        }
    }

out:
    if (slave_handle != NULL)
    {
        (void)spi_close(slave_handle);
    }
    slave_success = success;
    (void)osal_semaphore_post(slave_exit_sem);
    vTaskDelete(NULL);
}

static void spi_master_xfer_task(void *arg)
{
    uint32_t iter;
    uint32_t i;
    uint32_t seg;
    uint32_t off = 0U;
    uint32_t total_us = 0U;
    uint32_t start_rem_us = 0U;
    uint32_t end_rem_us = 0U;
    uint32_t avg_us = 0U;
    uint64_t bps = 0U;
    uint64_t kibps = 0U;
    bool success = true;
    int32_t ret;

    spi_handle_t master_handle = NULL;
    spi_cfg_t master_cfg = { 0 };
    spi_dma_config_t master_dma_cfg = { 0 };

    (void)arg;

    master_handle = spi_open(SPI_MASTER_INSTANCE);
    if (master_handle == NULL)
    {
        ERROR("Master: failed to open SPI instance");
        success = false;
        stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
        (void)osal_semaphore_post(master_exit_sem);
        vTaskDelete(NULL);
        return;
    }

    master_cfg.mode = SPI_MODE0;
    master_cfg.clk = SPI_FREQ;
    master_cfg.role = SPI_ROLE_MASTER;
    ret = spi_ioctl(master_handle, SPI_SET_CONFIG, &master_cfg);
    if (ret != 0)
    {
        ERROR("Master: failed to configure SPI");
        success = false;
        stop_xfers = true;
        goto master_out;
    }

    master_dma_cfg.tx_instance = MASTER_DMA_TX_INSTANCE;
    master_dma_cfg.tx_channel = MASTER_DMA_TX_CHANNEL;
    master_dma_cfg.tx_prio = MASTER_DMA_TX_PRIO;
    master_dma_cfg.rx_instance = MASTER_DMA_RX_INSTANCE;
    master_dma_cfg.rx_channel = MASTER_DMA_RX_CHANNEL;
    master_dma_cfg.rx_prio = MASTER_DMA_RX_PRIO;
    ret = spi_ioctl(master_handle, SPI_ENABLE_DMA, &master_dma_cfg);
    if (ret != 0)
    {
        ERROR("Master: failed to enable DMA");
        success = false;
        stop_xfers = true;
        goto master_out;
    }

    master_cb_ctx = (spi_cb_ctx_t)
    {
        .sem = master_done_sem,
        .status = SPI_XFER_ERROR,
    };
    ret = spi_set_callback(master_handle, spi_done_callback, &master_cb_ctx);
    if (ret != 0)
    {
        ERROR("Master: failed to set callback");
        success = false;
        stop_xfers = true;
        goto master_out;
    }

    ret = spi_select_slave(master_handle, SLAVE_SELECT_NUM);
    if (ret != 0)
    {
        ERROR("Master: failed to select slave");
        success = false;
    }

    for (iter = 0; (iter < SPI_SLAVE_DMA_SAMPLE_NUM_XFERS) && success; iter++)
    {
        start_rem_us = 0U;
        end_rem_us = 0U;

		/*
     	 * Important:
     	 * For SPI DMA transfer operation the byte transmit buffer has to be
     	 * padded to 32 bits. The receive buffer need not be padded to 32 bits
     	 *
     	 * Refer section (A) at the top, this explains why we need
     	 * uint32_t arrays instead of uint8_t arrays in Tx direction.
    	 */
        for (i = 0; i < XFER_SIZE; i++)
        {
            master_tx[i] = (uint32_t)((i + iter) & 0xFFU);
            master_rx[i] = 0U;
        }

        if (timer_get_value_us(timer_handle, &start_rem_us) != 0)
        {
            ERROR("Master: failed to read timer before transfer");
            success = false;
            break;
        }

        for (seg = 0; seg < XFER_NUM_SEGS; seg++)
        {
            off = seg * XFER_SEG_SIZE;

            if (stop_xfers)
            {
                success = false;
                break;
            }

            if (osal_semaphore_wait(slave_ready_sem,
                    SLAVE_TIMEOUT_MS) != pdTRUE)
            {
                ERROR("Master: timed out waiting for slave_ready (seg %lu)",
                        (unsigned long)seg);
                success = false;
                break;
            }

            if (stop_xfers)
            {
                success = false;
                break;
            }

            /* Ensure completion semaphore starts empty for this segment. */
            (void)osal_semaphore_wait(master_done_sem, 0);
            master_cb_ctx.status = SPI_XFER_ERROR;

            ret = spi_xfer_async(master_handle, &master_tx[off],
                    &master_rx[off], (uint16_t)XFER_SEG_SIZE);
            if (ret != 0)
            {
                ERROR("Master: failed to start async transfer (seg %lu)",
                        (unsigned long)seg);
                success = false;
                break;
            }

            if (osal_semaphore_wait(master_done_sem,
                    SLAVE_TIMEOUT_MS) != pdTRUE)
            {
                ERROR("Master: timeout waiting for segment completion "
                        "(seg %lu)",
                        (unsigned long)seg);
                success = false;
                break;
            }

            if (master_cb_ctx.status != SPI_SUCCESS)
            {
                ERROR("Master: transfer failed (status %d, seg %lu)",
                        (int)master_cb_ctx.status, (unsigned long)seg);
                success = false;
                break;
            }

            /* SPI async DMA doesn't do post-transfer cache invalidation. */
            cache_force_invalidate(&master_rx[off], XFER_SEG_SIZE);
        }

        if (timer_get_value_us(timer_handle, &end_rem_us) != 0)
        {
            ERROR("Master: failed to read timer after transfer");
            success = false;
            break;
        }

        if (!success)
        {
            break;
        }

        total_us += timer_elapsed_us(start_rem_us, end_rem_us, timer_wrap_us);

        for (i = 0; i < XFER_SIZE; i++)
        {
            if (((uint8_t)master_tx[i] != slave_rx[i])
                    || ((uint8_t)slave_tx[i] != master_rx[i]))
            {
                ERROR("Master: loopback verification FAILED (iter %lu idx %lu)",
                        (unsigned long)iter, (unsigned long)i);
                success = false;
                break;
            }
        }

        if (success == false)
        {
            break;
        }

        /* Release the slave task for the next iteration. */
        (void)osal_semaphore_post(signal_slave_sem);
    }

    if (success)
    {
        avg_us = total_us / SPI_SLAVE_DMA_SAMPLE_NUM_XFERS;
        PRINT("Master: average time per %u-byte iteration: %lu us\r\n",
                (unsigned)XFER_SIZE, (unsigned long)avg_us);

        if (avg_us != 0U)
        {
            bps = ((uint64_t)XFER_SIZE * 1000000ULL) / (uint64_t)avg_us;
            kibps = bps / 1024ULL;
            PRINT("Master: throughput: %llu KiB/s", (unsigned long long)kibps);
        }
    }

    /* Unblock the slave if we exit early. */
    (void)osal_semaphore_post(signal_slave_sem);

master_out:
    if (master_handle != NULL)
    {
        (void)spi_close(master_handle);
    }

    /* Unblock the slave if we exit early. */
    if (!success)
    {
        stop_xfers = true;
        (void)osal_semaphore_post(signal_slave_sem);
    }

    master_success = success;
    (void)osal_semaphore_post(master_exit_sem);
    vTaskDelete(NULL);
}

void spi_slave_dma_task(void)
{
    TaskHandle_t slave_task_handle = NULL;
    TaskHandle_t master_task_handle = NULL;
    uint32_t wait_timeout_ms;

    if ((XFER_SIZE == 0U) || (XFER_SEG_SIZE == 0U) ||
            ((XFER_SIZE % XFER_SEG_SIZE) != 0U))
    {
        ERROR("Invalid transfer sizing");
        return;
    }

    slave_ready_sem = osal_semaphore_create(&slave_ready_sem_mem);
    signal_slave_sem = osal_semaphore_create(&signal_slave_sem_mem);
    master_done_sem = osal_semaphore_create(&master_done_sem_mem);
    slave_done_sem = osal_semaphore_create(&slave_done_sem_mem);
    slave_exit_sem = osal_semaphore_create(&slave_exit_sem_mem);
    master_exit_sem = osal_semaphore_create(&master_exit_sem_mem);

    if ((slave_ready_sem == NULL) || (signal_slave_sem == NULL) ||
            (master_done_sem == NULL) || (slave_done_sem == NULL) ||
            (slave_exit_sem == NULL) || (master_exit_sem == NULL))
    {
        ERROR("Failed to create semaphores");
        goto cleanup;
    }

    load_bitstream();

    PRINT("SPI slave DMA loopback sample (SPIM1 <-> SPIS0, DMA-enabled)");
    PRINT("Transfers: %u, Bytes: %u (%u x %u-byte segments)",
            (unsigned)SPI_SLAVE_DMA_SAMPLE_NUM_XFERS,
            (unsigned)XFER_SIZE,
            (unsigned)XFER_NUM_SEGS,
            (unsigned)XFER_SEG_SIZE);

    timer_handle = timer_open(TIMER_INSTANCE);
    if (timer_handle == NULL)
    {
        ERROR("Failed to open timer instance");
        goto cleanup;
    }
    if (timer_set_period_us(timer_handle, TIMER_PERIOD_US) != 0)
    {
        ERROR("Failed to configure timer period");
        goto cleanup;
    }
    if (timer_start(timer_handle) != 0)
    {
        ERROR("Failed to start timer");
        goto cleanup;
    }
    if (timer_get_value_us(timer_handle, &timer_wrap_us) != 0)
    {
        ERROR("Failed to read timer value");
        goto cleanup;
    }

    stop_xfers = false;
    slave_success = false;
    master_success = false;

    wait_timeout_ms =
            (SLAVE_TIMEOUT_MS * SPI_SLAVE_DMA_SAMPLE_NUM_XFERS * 2U) +
            SLAVE_TIMEOUT_MS;

    if (xTaskCreate(spi_slave_xfer_task, "SPI_Slave_DMA", TASK_STACK_SIZE,
            NULL, SLAVE_TASK_PRIORITY, &slave_task_handle) != pdPASS)
    {
        ERROR("Failed to create slave task");
        goto cleanup;
    }

    if (xTaskCreate(spi_master_xfer_task, "SPI_Master_DMA", TASK_STACK_SIZE,
            NULL, MASTER_TASK_PRIORITY, &master_task_handle) != pdPASS)
    {
        ERROR("Failed to create master task");
        vTaskDelete(slave_task_handle);
        goto cleanup;
    }

    (void)osal_semaphore_wait(master_exit_sem, wait_timeout_ms);
    (void)osal_semaphore_wait(slave_exit_sem, wait_timeout_ms);

    if (slave_success && master_success)
    {
        PRINT("SPI slave DMA loopback sample completed successfully");
    }
    else
    {
        ERROR("SPI slave DMA loopback sample FAILED");
    }

cleanup:
    if (timer_handle != NULL)
    {
        (void)timer_close(timer_handle);
        timer_handle = NULL;
    }
    osal_semaphore_delete(slave_ready_sem);
    osal_semaphore_delete(signal_slave_sem);
    osal_semaphore_delete(master_done_sem);
    osal_semaphore_delete(slave_done_sem);
    osal_semaphore_delete(slave_exit_sem);
    osal_semaphore_delete(master_exit_sem);
}
