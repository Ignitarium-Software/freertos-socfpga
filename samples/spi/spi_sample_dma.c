/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SPI (DMA)
 */

/**
 * @file spi_sample_dma.c
 * @brief Sample Application for SPI (DMA)
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "socfpga_spi.h"
#include "socfpga_timer.h"
#include "osal.h"
#include "osal_log.h"

/**
 * @defgroup spi_sample_dma SPI (DMA)
 * @ingroup samples
 *
 * Sample Application for SPI using DMA
 *
 * @details
 * @section spi_dma_desc Description
 * This sample demonstrates using the SPI driver in DMA mode to write data to
 * an SPI EEPROM and read it back for verification.
 *
 * @section spi_dma_pre Prerequisites
 * - A serial EEPROM with SPI interface must be connected to the SPI bus.
 * - DMA must be available and the selected DMA channels must be free.
 *
 * @section spi_dma_param Configurable Parameters
 * - SPI instance: @c SPI_INSTANCE
 * - Slave select: @c SLAVE_SELECT_NUM
 * - SPI frequency: @c SPI_FREQ
 * - EEPROM address and transfer size: @c EEPROM_ADDR, @c XFER_SIZE
 * - DMA channel selection:
 *   - TX: @c SPI_DMA_TX_INSTANCE, @c SPI_DMA_TX_CHANNEL, @c SPI_DMA_TX_PRIO
 *   - RX: @c SPI_DMA_RX_INSTANCE, @c SPI_DMA_RX_CHANNEL, @c SPI_DMA_RX_PRIO
 *
 * @section spi_dma_how_to How to Run
 * 1. Build and flash the application.
 * 2. Run on the board with the EEPROM connected.
 * 3. Observe the UART terminal for status and timing output.
 *
 * @section spi_dma_result Expected Results
 * - Data written to the EEPROM is read back and compared.
 * - A success message is printed when the buffers match.
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

/**
 * EEPROM commands
 */
#define EEPROM_READ            0x03
#define EEPROM_WRITE           0x02
#define EEPROM_WR_DISABLE      0x04
#define EEPROM_WR_ENABLE       0x06
#define EEPROM_RD_SR           0x05
#define EEPROM_WR_SR           0x01
#define EEPROM_WIP_MASK        0x01
#define EEPROM_WIP_TIMEOUT_MS  100

/**
 * Configurable parameters for EEPROM data transfer
 * The EEPROM page size is 64 bytes.
 * Valid address range is 0x00 to 0x3FFF.
 */
#define EEPROM_ADDR            0x2780
#define EEPROM_PAGE_SIZE       64U
#define XFER_SIZE              1536U

/**
 * Configurable parameters for SPI controller
 */
#define SPI_INSTANCE           0
#define SLAVE_SELECT_NUM       1
#define SPI_FREQ               500000U

/**
 * Configurable parameters for SPI DMA transfer
 */
#define SPI_DMA_TX_INSTANCE    DMA_INSTANCE0
#define SPI_DMA_TX_CHANNEL     DMA_CH1
#define SPI_DMA_TX_PRIO        0U

#define SPI_DMA_RX_INSTANCE    DMA_INSTANCE0
#define SPI_DMA_RX_CHANNEL     DMA_CH2
#define SPI_DMA_RX_PRIO        1U

/**
 * Timer configuration used to measure transfer time.
 * The timer counts down; we measure elapsed time using the delta of remaining
 * microseconds.
 */
#define TIMER_INSTANCE         TIMER_SYS0
#define TIMER_PERIOD_US        0xFFFFFFFFU

static spi_handle_t spi_handle;

static uint32_t timer_elapsed_us(uint32_t start_rem_us, uint32_t end_rem_us,
        uint32_t wrap_us)
{
    if (start_rem_us >= end_rem_us)
    {
        return start_rem_us - end_rem_us;
    }
    return start_rem_us + (wrap_us - end_rem_us);
}

static uint32_t min_size(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

/**
 * @brief Send Write Enable command to EEPROM
 */
static int32_t eeprom_enable_write(void)
{
    /* DMA buffers need to be cache line aligned */
    uint32_t tx_dma_buf[1] __attribute__((aligned(64)));

    tx_dma_buf[0] = (uint32_t)EEPROM_WR_ENABLE;

    if (spi_xfer_sync(spi_handle, tx_dma_buf, NULL, 1) != 0)
    {
        return -EIO;
    }

    return 0;
}

/**
 * @brief Read EEPROM status register
 */
static int32_t eeprom_read_status(uint8_t *status)
{
    /* DMA buffers need to be cache line aligned */
    uint32_t tx_dma_buf[2] __attribute__((aligned(64)));
    uint8_t rx[2] __attribute__((aligned(64))) = { 0 };

    if (status == NULL)
    {
        return -EINVAL;
    }

    tx_dma_buf[0] = (uint32_t)EEPROM_RD_SR;
    tx_dma_buf[1] = 0U;

    if (spi_xfer_sync(spi_handle, tx_dma_buf, rx, 2) != 0)
    {
        return -EIO;
    }

    *status = rx[1];
    return 0;
}

/**
 * @brief Wait until EEPROM write cycle completes
 */
static int32_t eeprom_wait_ready(void)
{
    uint8_t status = 0;
    uint64_t delay_ticks = 5;
    uint32_t i;
    int32_t ret;

    for (i = 0; i < EEPROM_WIP_TIMEOUT_MS; i++)
    {
        ret = eeprom_read_status(&status);
        if (ret != 0)
        {
            return ret;
        }

        if ((status & EEPROM_WIP_MASK) == 0U)
        {
            return 0;
        }

        osal_delay_ms(delay_ticks);
    }

    return -ETIMEDOUT;
}

static int32_t eeprom_write_page(const uint8_t *buf, uint32_t size,
        uint16_t mem_addr)
{
    /* DMA buffers need to be cache line aligned */
    uint32_t tx_dma_buf[EEPROM_PAGE_SIZE + 3U] __attribute__((aligned(64)));
    uint32_t i;
    int32_t ret;

    if (size > EEPROM_PAGE_SIZE)
    {
        return -EINVAL;
    }

    ret = eeprom_enable_write();
    if (ret != 0)
    {
        return ret;
    }

    tx_dma_buf[0] = (uint32_t)EEPROM_WRITE;
    tx_dma_buf[1] = (uint32_t)((mem_addr >> 8) & 0xFFU);
    tx_dma_buf[2] = (uint32_t)(mem_addr & 0xFFU);

	/*
     * Important:
     * For SPI DMA transfer operation the byte transmit buffer has to be
     * padded to 32 bits. We define a intermediate uint32_t array and
     * copy bytes to it. The receive buffer need not be padded to 32 bits
     *
     * Refer section (A) at the top, this explains why we need
     * uint32_t arrays instead of uint8_t arrays in Tx direction.
    */
    for (i = 0; i < size; i++)
    {
        tx_dma_buf[i + 3U] = (uint32_t)buf[i];
    }

    if (spi_xfer_sync(spi_handle, tx_dma_buf, NULL, (uint16_t)(size + 3U)) != 0)
    {
        return -EIO;
    }

    return eeprom_wait_ready();
}

static int32_t eeprom_write(const uint8_t *buf, uint32_t size,
        uint16_t mem_addr)
{
    uint32_t offset = 0;
    uint32_t chunk;
    int32_t ret;

    while (offset < size)
    {
        chunk = min_size(EEPROM_PAGE_SIZE, size - offset);

        ret = eeprom_write_page(&buf[offset], chunk,
                (uint16_t)(mem_addr + offset));
        if (ret != 0)
        {
            return ret;
        }
        offset += chunk;
    }

    return 0;
}

static int32_t eeprom_read_chunk(uint8_t *buf, uint32_t size,
        uint16_t mem_addr)
{
    /* DMA buffers need to be cache line aligned */
    uint32_t tx_dma_buf[EEPROM_PAGE_SIZE + 3U] __attribute__((aligned(64)));
    uint8_t rx[EEPROM_PAGE_SIZE + 3U] __attribute__((aligned(64))) = { 0 };
    uint32_t i;

    if (size > EEPROM_PAGE_SIZE)
    {
        return -EINVAL;
    }

    tx_dma_buf[0] = (uint32_t)EEPROM_READ;
    tx_dma_buf[1] = (uint32_t)((mem_addr >> 8) & 0xFFU);
    tx_dma_buf[2] = (uint32_t)(mem_addr & 0xFFU);

    /*
     * Important:
     * For SPI DMA transfer operation the byte transmit buffer has to be
     * padded to 32 bits. We define a intermediate uint32_t array and
     * copy bytes to it. The receive buffer need not be padded to 32 bits
     *
     * Refer section (A) at the top, this explains why we need
     * uint32_t arrays instead of uint8_t arrays in Tx direction.
    */
    for (i = 0; i < size; i++)
    {
        tx_dma_buf[i + 3U] = (uint32_t)((uint8_t)(i + 0x0FU));
    }

    if (spi_xfer_sync(spi_handle, tx_dma_buf, rx, (uint16_t)(size + 3U)) != 0)
    {
        return -EIO;
    }

    memcpy(buf, &rx[3], size);
    return 0;
}

static int32_t eeprom_read(uint8_t *buf, uint32_t size, uint16_t mem_addr)
{
    uint32_t offset = 0;
    uint32_t chunk;
    int32_t ret;

    while (offset < size)
    {
        chunk = min_size(EEPROM_PAGE_SIZE, size - offset);

        ret = eeprom_read_chunk(&buf[offset], chunk,
                (uint16_t)(mem_addr + offset));
        if (ret != 0)
        {
            return ret;
        }
        offset += chunk;
    }

    return 0;
}

void spi_dma_task(void)
{
    static uint8_t rd_buf[XFER_SIZE];
    static uint8_t wr_buf[XFER_SIZE];
    int32_t retval = 0;
    spi_cfg_t config;
    spi_dma_config_t dma_cfg;
    timer_handle_t timer_handle;
    uint32_t i;
    uint32_t timer_wrap_us = 0;
    uint32_t start_rem_us = 0;
    uint32_t end_rem_us = 0;
    uint32_t write_time_us = 0;
    uint32_t read_time_us = 0;
    uint64_t write_bps = 0;
    uint64_t read_bps = 0;

    config.mode = SPI_MODE3;
    config.clk = SPI_FREQ;

    PRINT("Sample application to write and read EEPROM using SPI (DMA)");
    PRINT("Configuring SPI Master...");
    spi_handle = spi_open(SPI_INSTANCE);
    if (spi_handle == NULL)
    {
        ERROR("SPI instance cannot be open");
        return;
    }

    retval = spi_ioctl(spi_handle, SPI_SET_CONFIG, &config);
    if (retval != 0)
    {
        ERROR("Failed. Exiting the sample application");
        return;
    }
    PRINT("Done");

    timer_handle = timer_open(TIMER_INSTANCE);
    if (timer_handle == NULL)
    {
        ERROR("Failed to open timer instance");
        return;
    }
    if (timer_set_period_us(timer_handle, TIMER_PERIOD_US) != 0)
    {
        ERROR("Failed to configure timer period");
        (void)timer_close(timer_handle);
        return;
    }
    if (timer_start(timer_handle) != 0)
    {
        ERROR("Failed to start timer");
        (void)timer_close(timer_handle);
        return;
    }
    if (timer_get_value_us(timer_handle, &timer_wrap_us) != 0)
    {
        ERROR("Failed to read timer value");
        (void)timer_close(timer_handle);
        return;
    }

    dma_cfg.tx_instance = SPI_DMA_TX_INSTANCE;
    dma_cfg.tx_channel = SPI_DMA_TX_CHANNEL;
    dma_cfg.tx_prio = SPI_DMA_TX_PRIO;
    dma_cfg.rx_instance = SPI_DMA_RX_INSTANCE;
    dma_cfg.rx_channel = SPI_DMA_RX_CHANNEL;
    dma_cfg.rx_prio = SPI_DMA_RX_PRIO;

    retval = spi_ioctl(spi_handle, SPI_ENABLE_DMA, &dma_cfg);
    if (retval != 0)
    {
        ERROR("Failed to enable SPI DMA");
        return;
    }

    spi_select_slave(spi_handle, SLAVE_SELECT_NUM);

    for (i = 0; i < XFER_SIZE; i++)
    {
        wr_buf[i] = (uint8_t)(i & 0xFF);
    }

    PRINT("Writing %u bytes to EEPROM at address 0x%x...", XFER_SIZE,
            EEPROM_ADDR);

    if (timer_get_value_us(timer_handle, &start_rem_us) != 0)
    {
        ERROR("Failed to read timer before write");
        (void)timer_close(timer_handle);
        return;
    }
    retval = eeprom_write(wr_buf, XFER_SIZE, EEPROM_ADDR);
    if (retval != 0)
    {
        ERROR("Failed");
        (void)timer_close(timer_handle);
        return;
    }
    if (timer_get_value_us(timer_handle, &end_rem_us) != 0)
    {
        ERROR("Failed to read timer after write");
        (void)timer_close(timer_handle);
        return;
    }
    write_time_us = timer_elapsed_us(start_rem_us, end_rem_us, timer_wrap_us);
    if (write_time_us == 0U)
    {
        ERROR("Write time measured as 0 us");
        (void)timer_close(timer_handle);
        return;
    }
    write_bps = ((uint64_t)XFER_SIZE * 1000000ULL) / write_time_us;

    PRINT("Done");
    PRINT("Write time: %lu us, throughput: %llu B/s (%llu KiB/s)",
            (unsigned long)write_time_us,
            (unsigned long long)write_bps,
            (unsigned long long)(write_bps / 1024ULL));

    PRINT("Reading back %u bytes from EEPROM at address 0x%x...", XFER_SIZE,
            EEPROM_ADDR);

    if (timer_get_value_us(timer_handle, &start_rem_us) != 0)
    {
        ERROR("Failed to read timer before read");
        (void)timer_close(timer_handle);
        return;
    }
    retval = eeprom_read(rd_buf, XFER_SIZE, EEPROM_ADDR);
    if (retval != 0)
    {
        ERROR("Failed");
        (void)timer_close(timer_handle);
        return;
    }
    if (timer_get_value_us(timer_handle, &end_rem_us) != 0)
    {
        ERROR("Failed to read timer after read");
        (void)timer_close(timer_handle);
        return;
    }
    read_time_us = timer_elapsed_us(start_rem_us, end_rem_us, timer_wrap_us);
    if (read_time_us == 0U)
    {
        ERROR("Read time measured as 0 us");
        (void)timer_close(timer_handle);
        return;
    }
    read_bps = ((uint64_t)XFER_SIZE * 1000000ULL) / read_time_us;

    PRINT("Done");

    PRINT("Read time: %lu us, throughput: %llu B/s (%llu KiB/s)",
            (unsigned long)read_time_us,
            (unsigned long long)read_bps,
            (unsigned long long)(read_bps / 1024ULL));

    retval = memcmp(wr_buf, rd_buf, XFER_SIZE);
    if (retval != 0)
    {
        ERROR("Comparison failed");
        ERROR("Exiting the sample application");
        (void)timer_close(timer_handle);
        return;
    }

    (void)timer_close(timer_handle);

    PRINT("Closing SPI instance...");
    retval = spi_close(spi_handle);
    if (retval != 0)
    {
        ERROR("Failed");
        ERROR("Exiting the sample application");
        return;
    }
    PRINT("Done");

    PRINT("SPI sample application completed");
}
