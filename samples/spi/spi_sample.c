/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SPI
 */

/**
 * @file spi_sample.c
 * @brief Sample Application for SPI
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "socfpga_spi.h"
#include "osal.h"
#include "osal_log.h"

/**
 * @defgroup spi_sample SPI
 * @ingroup samples
 *
 * Sample Application for SPI
 *
 * @details
 * @section spi_desc Description
 * This is a sample application to demonstrate the usage of SPI driver.
 * It writes data to an EEPROM using SPI and then reads it back.
 *
 * @section spi_pre Prerequisites
 * - A serial EEPROM with SPI interface must be connected to the SPI bus.
 *
 * @section spi_param Configurable Parameters
 * - SPI instance can be configured in @c SPI_INSTANCE macro.
 * - The memory address to write and read back can be configured in
 *   @c EEPROM_ADDR macro.
 * - The number of bytes to transfer can be configured in @c XFER_SIZE
 *   macro.
 * - Slave select can be configured in @c SLAVE_SELECT_NUM macro.
 * @note Ensure the instance and slave select are valid.
 * The EEPROM commands and address can vary depending on the EEPROM used.
 *
 * @section spi_how_to How to Run
 * 1. Follow the common README instructions to build and flash the application.
 * 2. Run the application on the board with the EEPROM connected.
 * 3. Observe the UART terminal for status messages.
 *
 * @section spi_result Expected Results
 * - The application sends the Write Enable and Write commands to store data in
 *   EEPROM.
 * - It then reads the data back using the Read command.
 * - The read and written data are compared, and a success message is printed
 *   on match.
 */

/**
 * EEPROM commands
 */
#define EEPROM_READ          0x03
#define EEPROM_WRITE         0x02
#define EEPROM_WR_DISABLE    0x04
#define EEPROM_WR_ENABLE     0x06
#define EEPROM_RD_SR         0x05
#define EEPROM_WR_SR         0x01
#define EEPROM_WIP_MASK      0x01
#define EEPROM_WIP_TIMEOUT_MS 10

/**
 * Configurable parameters for EEPROM data transfer
 * The EEPROM page size is 64 bytes.
 * Valid address range is 0x00 to 0x3FFF.
 */
#define EEPROM_ADDR          0x2780
#define EEPROM_PAGE_SIZE     64U
#define XFER_SIZE            97U

/**
 * Configurable parameters for SPI controller
 */
#define SPI_INSTANCE         0
#define SLAVE_SELECT_NUM     1
#define SPI_FREQ             500000U
spi_handle_t spi_handle;

static uint32_t min_size(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static void print_buf(const char *label, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if (label != NULL)
    {
        PRINT("%s", label);
    }

    for (i = 0; i < len; i++)
    {
        printf(" 0x%02x ", buf[i]);
        if (((i + 1U) % 16U) == 0U)
        {
            printf("\r\n");
        }
    }
    printf("\r\n");
}

/**
 * @brief Send Write Enable command to EEPROM
 *
 * Write enable command should be sent before any write operation.
 */
static int32_t eeprom_enable_write(void)
{
    uint8_t cmd = EEPROM_WR_ENABLE;

    if (spi_xfer_sync(spi_handle, &cmd, NULL, 1) != 0)
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
    uint8_t cmd[2] = { EEPROM_RD_SR, 0x00 };
    uint8_t rx[2] = { 0 };

    if (status == NULL)
    {
        return -EINVAL;
    }

    if (spi_xfer_sync(spi_handle, cmd, rx, 2) != 0)
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

/**
 * @brief Send write command followed by address and data to EEPROM
 *
 * Only one page can be written at a time.
 * writing more than one page at a time will result in wrapping around
 * and overwriting the previous data.
 */
static int32_t eeprom_write_page(const uint8_t *buf, uint32_t size,
        uint16_t mem_addr)
{
    uint8_t cmd[EEPROM_PAGE_SIZE + 3] = { 0 };
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

    cmd[0] = EEPROM_WRITE;
    cmd[1] = (mem_addr >> 8) & 0xFF;
    cmd[2] = mem_addr & 0xFF;

    for (i = 0; i < size; i++)
    {
        cmd[i + 3] = buf[i];
    }

    /* For write operations using transfer the rxbuf is NULL */
    if (spi_xfer_sync(spi_handle, cmd, NULL, (uint16_t)(size + 3U)) != 0)
    {
        return -EIO;
    }

    return eeprom_wait_ready();
}

static int32_t eeprom_write_buf(const uint8_t *buf, uint32_t size,
        uint16_t mem_addr)
{
    uint32_t offset = 0;
    int32_t ret;

    while (offset < size)
    {
        uint32_t chunk;

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

/**
 * @brief Read data from EEPROM
 *
 * The read command is followed by address and dummy bytes.
 * The number of dummy bytes is equal to the number of bytes to be read.
 * The first three bytes of the read data will be dummy bytes and should be
 * ignored.
 */
static int32_t eeprom_read_chunk(uint8_t *buf, uint32_t size,
        uint16_t mem_addr)
{
    uint8_t cmd[EEPROM_PAGE_SIZE + 3] = { 0 };
    uint8_t rx[EEPROM_PAGE_SIZE + 3] = { 0 };
    uint32_t i;

    if (size > EEPROM_PAGE_SIZE)
    {
        return -EINVAL;
    }

    cmd[0] = EEPROM_READ;
    cmd[1] = (mem_addr >> 8) & 0xFF;
    cmd[2] = mem_addr & 0xFF;

    for (i = 0; i < size; i++)
    {
        cmd[i + 3] = i + 0x0F;
    }

    /* For read operations using transfer the txbuf contains dummy data */
    if (spi_xfer_sync(spi_handle, cmd, rx, (uint16_t)(size + 3U)) != 0)
    {
        return -EIO;
    }

    memcpy(buf, &rx[3], size);
    return 0;
}

static int32_t eeprom_read_buf(uint8_t *buf, uint32_t size, uint16_t mem_addr)
{
    uint32_t offset = 0;
    int32_t ret;

    while (offset < size)
    {
        uint32_t chunk;

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

void spi_task(void)
{
    uint8_t rd_buf[XFER_SIZE] = { 0 };
    uint8_t wr_buf[XFER_SIZE] = { 0 };

    int32_t retval = 0;
    spi_cfg_t config;
    int i;

    /*
     * SPI mode specifies the clock phase (CPH) and clock polarity (CPO)
     * Mode0: CPH toggles in the middle of first bit and CPO is inactive when
     *        high
     * Mode1: CPH toggles at the start of first bit and CPO is inactive when
     *        high
     * Mode2: CPH toggles in the middle of first bit and CPO is inactive when
     *        low
     * Mode3: CPH toggles at the start of first bit and CPO is inactive when
     *        low
     * The mode should match the EEPROM's SPI mode.
     * The EEPROM used in this sample application supports Mode 3.
     */
    config.mode = SPI_MODE3;
    config.clk = SPI_FREQ;

    PRINT("Sample application to write and read EEPROM using SPI");
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

    spi_select_slave(spi_handle, SLAVE_SELECT_NUM);

    for (i = 0; i < (int)XFER_SIZE; i++)
    {
        wr_buf[i] = (uint8_t)(i & 0xFF);
    }

    print_buf("Write Data", wr_buf, XFER_SIZE);

    PRINT("Writing %d bytes to EEPROM at address 0x%x...", XFER_SIZE,
            EEPROM_ADDR);
    retval = eeprom_write_buf(wr_buf, XFER_SIZE, EEPROM_ADDR);
    if (retval != 0)
    {
        ERROR("Failed");
        return;
    }
    PRINT("Done");

    PRINT("Reading back %d bytes from EEPROM at address 0x%x...", XFER_SIZE,
            EEPROM_ADDR);
    retval = eeprom_read_buf(rd_buf, XFER_SIZE, EEPROM_ADDR);
    if (retval != 0)
    {
        ERROR("Failed");
        return;
    }
    PRINT("Done");

    print_buf("Read Data", rd_buf, XFER_SIZE);

    retval = memcmp(wr_buf, rd_buf, XFER_SIZE);
    if (retval != 0)
    {
        ERROR("Comparison failed");
        ERROR("Exiting the sample application");
        return;
    }

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
