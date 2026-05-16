/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application implementation for i2c
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "osal_log.h"
#include <task.h>
#include "socfpga_i2c.h"
#include "socfpga_dma.h"

/**
 * @defgroup i2c_sample I2C
 * @ingroup samples
 *
 * Sample application for I2C driver
 *
 * @details
 * @section i2c_desc Description
 * This sample application demonstrates the use of the I2C driver to
 * communicate with an EEPROM device over the I2C bus using DMA transfer.
 * It writes a block of N bytes to a specific memory address in the
 * EEPROM and then reads back the same.
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
 * @section i2c_dma_config Configurable Parameters
 * - The transmit DMA controller can be configured in @c TX_DMA_INST macro
 * - The transmit DMA channel can be configured in @c TX_DMA_CHAN macro
 * - The transmit DMA channel priority can be configured in @c TX_DMA_PRIO macro
 * - The receive DMA controller can be configured in @c RX_DMA_INST macro
 * - The receive DMA channel can be configured in @c RX_DMA_CHAN macro
 * - The receive DMA channel priority can be configured in @c RX_DMA_PRIO macro
 *
 * @section i2c_how_to How to Run
 * 1. Follow the common README instructions to build and flash the application.
 * 2. Run the application on the board.
 * 3. Observe the results in the console
 *
 * @section i2c_result Expected Results
 * - The write and read data are verified and a success message is displayed.
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

/* Test configuration parameters */

/* The I2C controller instances used in this app */
#define I2CBUS 0

/* The I2C devices slave address */
#define DEV_ADDR 0x50

/* The EEPROM memory address used */
#define MEM_ADDR 0x0000

/* Memory address size */
#define MEM_ADDR_SZ 2U

/* Bytes used in one transfer */
#define NUM_TEST_BYTES 60U

/* I2C Tx DMA params */
#define TX_DMA_INST DMA_INSTANCE0
#define TX_DMA_CHAN DMA_CH1
#define TX_DMA_PRIO 0U

/* I2C Rx DMA params */
/*
 * RX priority is set higher than TX to service incoming data promptly and
 * avoid RX FIFO overflow while TX pushes read commands.
 */
#define RX_DMA_INST DMA_INSTANCE1
#define RX_DMA_CHAN DMA_CH2
#define RX_DMA_PRIO 1U


/* Buffers */
/*
 * Important:
 * For I2C DMA transfer operation the byte transmit buffer has to be
 * padded to 32 bits with appropriate command bits (STOP/CMD) set.
 * The receive buffer need not be padded to 32 bits
 *
 * Refer section (A) at the top, this explains why we need
 * uint32_t arrays instead of uint8_t arrays in Tx direction.
 */
static uint32_t wbuf[MEM_ADDR_SZ + NUM_TEST_BYTES];
static uint8_t rbuf[NUM_TEST_BYTES];

/**
 * @brief fill the buffer with an incremental pattern
 *
 * Fills buf with nbytes of incremental pattern starting with start_num
 */

/**
 * We need to build a 32 bit wide intermediate buffer and copy the data byte
 * by byte to each word. We will be setting control flags later.
 *
 * The macro I2C_DMA_CMD_DATA can be used to typecast data from uint8_t to uint32_t
 */
static void fill_dma_buf(uint32_t *buf, uint32_t nbytes, uint8_t start_num)
{
    uint32_t i;
    for (i = 0; i < nbytes; i++)
    {
        *(buf + i) = I2C_DMA_CMD_DATA(start_num++);
    }
}

static void apply_dma_stop(uint32_t *buf, uint32_t nwords)
{
    if (nwords > 0U)
    {
    	/*
    	 * The macro I2C_DMA_CMD_FLAG_STOP is used to set the STOP flag in the
    	 * 32 bit field inside DMA buffer
    	 */
        buf[nwords - 1U] |= I2C_DMA_CMD_FLAG_STOP;
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
            printf("ERROR: mismatch at index %d: expected 0x%2.2X got 0x%2.2X\n",
                    i, start_num, (*(buf + i)));
            ret = -1;
            break;
        }
    }
    return ret;
}

void i2c_dma_task(void)
{
    int retval = 0;
    i2c_handle_t handle;
    i2c_config_t config;
    i2c_dma_config_t dma_config;
    uint16_t slave_addr;

    PRINT("Sample application to write and read EEPROM using i2c in DMA mode");

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

    PRINT("Performing EEPROM write, read and verify using DMA mode");

    /**
     * I2C_DMA_CMD_DATA packs a byte into the DATA_CMD word's data field.
     * For DMA, we build one 32-bit word per byte we want to send.
     */
    wbuf[0] = I2C_DMA_CMD_DATA((MEM_ADDR >> 8) & 0xFFU);
    wbuf[1] = I2C_DMA_CMD_DATA(MEM_ADDR & 0xFFU);

    /* Map DMA instances/channels to I2C WDMA (TX) and RDMA (RX) paths. */
    dma_config.tx_instance = TX_DMA_INST;
    dma_config.tx_channel = TX_DMA_CHAN;
    dma_config.rx_instance = RX_DMA_INST;
    dma_config.rx_channel = RX_DMA_CHAN;
    dma_config.tx_prio = TX_DMA_PRIO;
    dma_config.rx_prio = RX_DMA_PRIO;

    /* Open/configure DMA channels before enabling WDMA/RDMA paths. */
    retval = i2c_ioctl(handle, I2C_OPEN_DMA, (void *)(&dma_config));
    if (retval != 0)
    {
        ERROR("Setting DMA xfer failed");
        i2c_close(handle);
        return;
    }

    /* Enable WDMA/RDMA requests for DMA mode transfers. */
    retval = i2c_ioctl(handle, I2C_ENABLE_WDMA, NULL);
    if (retval != 0)
    {
        ERROR("Setting DMA xfer failed");
        i2c_close(handle);
        return;
    }

    retval = i2c_ioctl(handle, I2C_ENABLE_RDMA, NULL);
    if (retval != 0)
    {
        ERROR("Setting DMA xfer failed");
        i2c_close(handle);
        return;
    }

    /* Build the DMA write payload as DATA_CMD words (one word per byte). */
    fill_dma_buf((wbuf + MEM_ADDR_SZ), NUM_TEST_BYTES, 0x12);
    apply_dma_stop(wbuf, (MEM_ADDR_SZ + NUM_TEST_BYTES));
    memset(rbuf, 0x0, (NUM_TEST_BYTES * sizeof(rbuf[0])));

    /* Program the EEPROM with the pattern */
    retval = i2c_write_sync(handle, wbuf, (MEM_ADDR_SZ + NUM_TEST_BYTES));
    if (retval != 0)
    {
        ERROR("EEPROM write failed");
        i2c_close(handle);
        ERROR("Exiting sample application");
        return;
    }

    PRINT("Write complete");

    osal_task_delay(10);

    /* Read back from the EEPROM */
    wbuf[0] = I2C_DMA_CMD_DATA((MEM_ADDR >> 8) & 0xFFU);
    wbuf[1] = I2C_DMA_CMD_DATA(MEM_ADDR & 0xFFU);
    apply_dma_stop(wbuf, MEM_ADDR_SZ);

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

    retval = verify_buf(rbuf, NUM_TEST_BYTES, 0x12);

    if (retval == 0)
    {
        PRINT("EEPROM write, read and verify successful");
    }
    else
    {
        ERROR("EEPROM write, read and verify failed");
    }

    i2c_close(handle);
    PRINT("I2c sample application completed.");
}
