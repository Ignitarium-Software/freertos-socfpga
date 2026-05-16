/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application implementation for i2c
 */
#include <string.h>
#include "osal.h"
#include "osal_log.h"
#include <task.h>
#include "socfpga_i2c.h"

/**
 * @defgroup i2c_sample I2C
 * @ingroup samples
 *
 * Sample application for I2C driver
 *
 * @details
 * @section i2c_desc Description
 * This sample application demonstrates the use of the I2C driver to
 * communicate with an EEPROM device over the I2C bus. It writes a block
 * of N bytes to a specific memory address in the EEPROM and then reads
 * back the same.
 *
 * @section i2c_pre Prerequisites
 * - The NAND daughter card shall be used.
 * - An EEPROM device must be connected to the I2C bus.
 *
 * @section i2c_param Configurable Parameters
 * - The I2C bus instance can be configured in @c I2CBUS macro.
 * - The I2C slave address can be configured in @c DEV_ADDR macro.
 * - The memory address in EEPROM can be configured in @c MEM_ADDR macro.
 * - The number of bytes to write and read back can be configured in @c NUM_TEST_BYTES macro.
 *
 * @section i2c_how_to How to Run
 * 1. Follow the common README instructions to build and flash the application.
 * 2. Run the application on the board.
 * 3. Observe the results in the console
 *
 * @section i2c_result Expected Results
 * - The write and read data are verified and a success message is displayed.
 */

/* Test configuration parameters */

/* The I2C controller instances used in this app */
#define I2CBUS    0

/* The I2C devices slave address */
#define DEV_ADDR    0x50

/* The EEPROM memory address used */
#define MEM_ADDR    0x0000

/* Memory address size */
#define MEM_ADDR_SZ    2U

/* Bytes used in one transfer */
#define NUM_TEST_BYTES  32U

/* Buffers */
static uint8_t wbuf[MEM_ADDR_SZ + NUM_TEST_BYTES];
static uint8_t rbuf[NUM_TEST_BYTES];

/**
 * @brief fill the buffer with an incremental pattern
 *
 * Fills buf with nbytes of incremental pattern starting with start_num
 */

static void fill_buf(uint8_t *buf, uint32_t nbytes, uint8_t start_num)
{
    uint32_t i;
    for (i = 0; i < nbytes; i++)
    {
        *(buf + i) = start_num++;
    }
}

/**
 * @brief verify the buffer contains and incremental pattern
 *
 * Verifies that buf contains an incremental pattern starting with start_num
 *
 * @return 0 on success -1 otherwise
 */
static int verify_buf(uint8_t *buf, uint32_t nbytes, uint8_t start_num)
{
    int ret = 0;
    uint32_t i;
    for (i = 0; i < nbytes; i++, start_num++)
    {
        if ((*(buf + i)) != start_num)
        {
            printf("ERROR: mismatch at index %d: expected 0x%2.2X got 0x%2.2X\n", i, start_num,
                    (*(buf + i)));
            ret = -1;
            break;
        }
    }
    return ret;
}

void i2c_task(void)
{
    int retval = 0;
    i2c_handle_t handle;
    i2c_config_t config;
    uint16_t slave_addr;

    PRINT("Sample application to write and read EEPROM using i2c driver");

    PRINT("Configuring the i2c as master ...");

    /* Open and configure the I2C interface for standard speed */

    handle = i2c_open(I2CBUS);
    if (handle == NULL)
    {
        ERROR("Cannot open the i2c instance");
        ERROR("Exiting sample application");
        return;
    }

    config.clk = I2C_STANDARD_MODE_BPS;
    retval = i2c_ioctl(handle, I2C_SET_MASTER_CFG, &config);
    if (retval != 0)
    {
        ERROR("Configuring i2c speed failed");
        i2c_close(handle);
        ERROR("Exiting sample application");
        return;
    }

    /* set slave address */
    slave_addr = DEV_ADDR;
    retval = i2c_ioctl(handle, I2C_SET_SLAVE_ADDR, (void *)(&slave_addr));
    if (retval != 0)
    {
        ERROR("Configuring slave address failed");
        i2c_close(handle);
        ERROR("Exiting sample application");
        return;
    }

    PRINT("Configuration done.");

    PRINT("Performing EEPROM write, read and verify");

    /* Write test bytes into EEPROM */
    /* First two bytes of the write buffer is the memory address in EEPROM */
    wbuf[0] = (uint8_t)((MEM_ADDR >> 8) & 0xFF);
    wbuf[1] = (uint8_t)(MEM_ADDR & 0xFF);

    fill_buf((wbuf + 2), NUM_TEST_BYTES, 0x10);

    /* Program the EEPROM with the pattern */
    retval = i2c_write_sync(handle, wbuf, (MEM_ADDR_SZ + NUM_TEST_BYTES));
    if (retval != 0)
    {
        ERROR("EEPROM write failed");
        i2c_close(handle);
        ERROR("Exiting sample application");
        return;
    }

    PRINT("I2C write successful");

    /* EEPROM may need a small delay between back to back write */
    osal_task_delay(20);

    /* Read back from the EEPROM */

    wbuf[0] = (uint8_t)((MEM_ADDR >> 8) & 0xFF);
    wbuf[1] = (uint8_t)(MEM_ADDR & 0xFF);

    /* Write the memory address first */
    retval = i2c_write_sync(handle, wbuf, MEM_ADDR_SZ);
    if (retval != 0)
    {
        ERROR("ERROR: writing address to EEPROM failed");
        ERROR("Exiting sample application");
        return;
    }

    /* perform the read */
    retval = i2c_read_sync(handle, rbuf, NUM_TEST_BYTES);
    if (retval != 0)
    {
        ERROR("ERROR: read from EEPROM failed");
        ERROR("Exiting sample application");
        return;
    }
    retval = verify_buf(rbuf, NUM_TEST_BYTES, 0x10);

    if (retval == 0)
    {
        PRINT("EEPROM write, read and verify successful");
    }
    else
    {
        ERROR("EEPROM write, read and verify failed");
    }

    PRINT("I2c sample application completed.");
}
