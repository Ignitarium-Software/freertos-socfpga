/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
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
 * - The memory address to write and read back can be configured in @c EEPROM_ADDRESS macro.
 * - The number of bytes to transfer can be configured in @c TRANSFER_SIZE macro.
 * - Slave select can be configured in @c SLAVE_SELECT_NUM macro.
 * @note Ensure the instance and slave select are valid. <br>
 * The EEPROM commands and address can vary depending on the EEPROM used.
 *
 * @section spi_how_to How to Run
 * 1. Follow the common README instructions to build and flash the application.
 * 2. Run the application on the board with the EEPROM connected.
 * 3. Observe the UART terminal for status messages.
 *
 * @section spi_result Expected Results
 * - The application sends the Write Enable and Write commands to store data in EEPROM.
 * - It then reads the data back using the Read command.
 * - The read and written data are compared, and a success message is printed on match.
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
#define EEPROM_ADDRESS       0x2780
#define TRANSFER_SIZE        24

/**
 * Configurable parameters for SPI controller
 */
#define SPI_INSTANCE         0
#define SLAVE_SELECT_NUM     1
#define SPI_FREQ             500000U

spi_handle_t spi_handle;
uint8_t dummyBuf[10] = { 0 };

/**
 * @brief Send Write Enable command to EEPROM
 *
 * Write enable command should be sent before any write operation.
 */
int32_t eeprom_enable_write()
{
    uint8_t cmd = EEPROM_WR_ENABLE;

    if (spi_transfer_sync(spi_handle, &cmd, NULL, 1) != 0)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Read EEPROM status register
 */
static int32_t eeprom_read_status(uint8_t *status)
{
    uint8_t cmd[2] = { EEPROM_RD_SR, 0x00 };
    uint8_t rx[2] = { 0 };

    if ((status == NULL) || (spi_transfer_sync(spi_handle, cmd, rx, 2) != 0))
    {
        return 0;
    }

    *status = rx[1];
    return 1;
}

/**
 * @brief Wait until EEPROM write cycle completes
 */
static int32_t eeprom_wait_ready(void)
{
    uint8_t status = 0;
    uint64_t delay_ticks = 5;

    for (uint32_t i = 0; i < EEPROM_WIP_TIMEOUT_MS; i++)
    {
        if (eeprom_read_status(&status) != 1)
        {
            return 0;
        }

        if ((status & EEPROM_WIP_MASK) == 0U)
        {
            return 1;
        }

        osal_delay_ms(delay_ticks);
    }

    return 0;
}

/**
 * @brief Send write command followed by address and data to EEPROM
 *
 * Only one page can be written at a time.
 * writing more than one page at a time will result in wrapping around
 * and overwriting the previous data.
 */
int32_t eeprom_write(uint8_t *buf, size_t size, uint16_t mem_add)
{
    uint8_t cmd[150] = { 0 };
    uint8_t rx_count = 0, i = 0;

    if (eeprom_enable_write() != 1)
    {
        return 0;
    }

    cmd[0] = EEPROM_WRITE;
    cmd[1] = (mem_add >> 8) & 0xFF;
    cmd[2] = mem_add & 0xFF;

    for (i = 0; i < size; i++)
    {
        cmd[i + 3] = buf[i];
    }

    rx_count = size + 3;
    /* For write operations using transfer the rxbuf is NULL */
    if (spi_transfer_sync(spi_handle, cmd, NULL,  rx_count) != 0)
    {
        return 0;
    }

    if (eeprom_wait_ready() != 1)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Read data from EEPROM
 *
 * The read command is followed by address and dummy bytes.
 * The number of dummy bytes is equal to the number of bytes to be read.
 * The first three bytes of the read data will be dummy bytes and should be ignored.
 */
int32_t eeprom_read(uint8_t *buf, size_t size, uint16_t mem_add)
{
    uint32_t i = 0;
    uint8_t cmd[40] = { 0 };
    cmd[0] = EEPROM_READ;
    cmd[1] = (mem_add >> 8) & 0xFF;
    cmd[2] = mem_add & 0xFF;

    for (i = 0; i < size; i++)
    {
        cmd[i + 3] = i + 0x0F;
    }

    /* For read operations using transfer the txbuf contains dummy data */
    if (spi_transfer_sync(spi_handle, cmd, buf, (size + 3)) != 0)
    {
        return 0;
    }
    return 1;
}

void spi_task(void)
{
    uint8_t rd_buf[30] = { 0 };
    uint8_t wr_buf[24] =
    {
        0x2, 0x21, 0x22, 0x23, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb,
        0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
    };

    int32_t retval = 0;
    spi_cfg_t config;

    /* SPI mode specifies the clock phase(CPH) and clock polarity(CPO)
     * Mode0: CPH toggles in the middle of first bit and CPO is inactive when high
     * Mode1: CPH toggles at the start of first bit and CPO is inactive when high
     * Mode2: CPH toggles in the middle of first bit and CPO is inactive when low
     * Mode3: CPH toggles at the start of first bit and CPO is inactive when low
     * The mode should match the EEPROM's SPI mode.
     * The EEPROM used in this sample application supports Mode 3.
     */
    config.mode = SPI_MODE3;
    config.clk = SPI_FREQ;

    PRINT("Sample application to write and read EEPROM using SPI");
    PRINT("Configuring SPI Master...");
    spi_handle = spi_open(0);
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

    spi_select_slave(spi_handle, 1);
    PRINT("Enabling write operation in EEPROM...");
    retval = eeprom_enable_write();
    if (retval != 1)
    {
        ERROR("Failed");
        return;
    }
    PRINT("Done");

    PRINT("Write Data");
    for (int i = 0; i < TRANSFER_SIZE; i++)
    {
        printf(" 0x%x ", wr_buf[i]);
    }
    printf("\r\n");

    PRINT("Writing %d bytes to EEPROM at address 0x%x...", TRANSFER_SIZE,
            EEPROM_ADDRESS);
    retval = eeprom_write(wr_buf, TRANSFER_SIZE, EEPROM_ADDRESS);
    if (retval != 1)
    {
        ERROR("Failed");
        return;
    }
    PRINT("Done");

    PRINT("Reading back %d bytes from EEPROM at address 0x%x...", TRANSFER_SIZE,
            EEPROM_ADDRESS);
    retval = eeprom_read(rd_buf, TRANSFER_SIZE, EEPROM_ADDRESS);
    if (retval != 1)
    {
        ERROR("Failed");
    }
    PRINT("Done");

    PRINT("Read Data");
    for (int i = 3; i < TRANSFER_SIZE + 3; i++)
    {
        printf(" 0x%x ", rd_buf[i]);
    }
    printf("\r\n");

    retval = memcmp(wr_buf, &rd_buf[3], TRANSFER_SIZE);
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
