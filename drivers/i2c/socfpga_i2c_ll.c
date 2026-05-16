/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA i2c
 */

#include "socfpga_defines.h"
#include <stdbool.h>
#include "socfpga_i2c.h"
#include "socfpga_i2c_ll.h"
#include "socfpga_i2c_reg.h"

#define I2C_CONTRL_STD_SPEED     (0x01U)
#define I2C_CONTRL_FAST_SPEED    (0x02U)
#define I2C_CONTRL_HIGH_SPEED    (0x03U)

/*
 * Clock count values for DW_apb_i2c speed modes.
 *
 * I2C bus rates are typically 100 kHz (standard) and 400 kHz (fast).
 * The HCNT/LCNT programming values below are platform-tuned and depend
 * on the controller input clock and board timing.
 *
 * NOTE: Updating these values can affect bus stability; validate on target.
 */
#define SS_SCL_HCNT_VAL    400U         /*!< SCL High count for standard speed I2C clock */
#define SS_SCL_LCNT_VAL    470U         /*!< SCL Low count for standard speed I2C clock */
#define FS_SCL_HCNT_VAL    60U          /*!< SCL High count for fast speed I2C clock */
#define FS_SCL_LCNT_VAL    130U         /*!< SCL Low count for fast speed I2C clock */

/**
 * @brief Enable I2C interrupt
 */
void i2c_enable_interrupt(uint32_t base_addr, uint32_t interrupt_req)
{
    uint32_t val;
    val = RD_REG32(base_addr + I2C_INTR_MASK);
    if ((interrupt_req & I2C_TX_EMPTY_INT) == I2C_TX_EMPTY_INT)
    {
        val |= I2C_INTR_MASK_M_TX_EMPTY_MASK;
    }
    if ((interrupt_req & I2C_RX_FULL_INT) == I2C_RX_FULL_INT)
    {
        val |= I2C_INTR_MASK_M_RX_FULL_MASK;
    }
    if ((interrupt_req & I2C_TX_ABORT_INT) == I2C_TX_ABORT_INT)
    {
        val |= I2C_INTR_MASK_M_TX_ABRT_MASK;
    }
    if ((interrupt_req & I2C_STOP_DET_INT) == I2C_STOP_DET_INT)
    {
        val |= I2C_INTR_MASK_M_STOP_DET_MASK;
    }
    if ((interrupt_req & I2C_RD_REQ_INT) == I2C_RD_REQ_INT)
    {
        val |= I2C_INTR_MASK_M_RD_REQ_MASK;
    }
    if ((interrupt_req & I2C_RX_OVER_INT) == I2C_RX_OVER_INT)
    {
        val |= I2C_INTR_MASK_M_RX_OVER_MASK;
    }

    WR_REG32(base_addr + I2C_INTR_MASK, val);
}

/**
 * @brief Disable I2C interrupt
 */
void i2c_disable_interrupt(uint32_t base_addr, uint32_t interrupt_req)
{
    uint32_t val;
    val = RD_REG32(base_addr + I2C_INTR_MASK);
    if ((interrupt_req & I2C_TX_EMPTY_INT) == I2C_TX_EMPTY_INT)
    {
        val &= ~(I2C_INTR_MASK_M_TX_EMPTY_MASK);
    }
    if ((interrupt_req & I2C_RX_FULL_INT) == I2C_RX_FULL_INT)
    {
        val &= ~(I2C_INTR_MASK_M_RX_FULL_MASK);
    }
    if ((interrupt_req & I2C_TX_ABORT_INT) == I2C_TX_ABORT_INT)
    {
        val &= ~(I2C_INTR_MASK_M_TX_ABRT_MASK);
    }
    if ((interrupt_req & I2C_STOP_DET_INT) == I2C_STOP_DET_INT)
    {
        val &= ~(I2C_INTR_MASK_M_STOP_DET_MASK);
    }
    if ((interrupt_req & I2C_RD_REQ_INT) == I2C_RD_REQ_INT)
    {
        val &= ~(I2C_INTR_MASK_M_RD_REQ_MASK);
    }
    if ((interrupt_req & I2C_RX_OVER_INT) == I2C_RX_OVER_INT)
    {
        val &= ~(I2C_INTR_MASK_M_RX_OVER_MASK);
    }
    WR_REG32(base_addr + I2C_INTR_MASK, val);
}

/**
 * @brief Get I2C interrupt status
 */
uint32_t i2c_get_interrupt_status(uint32_t base_addr)
{
    uint32_t res = I2C_NO_INT;
    uint32_t val;
    val = RD_REG32(base_addr + I2C_INTR_STAT);
    if ((val & I2C_INTR_STAT_R_TX_EMPTY_MASK) != 0U)
    {
        res |= I2C_TX_EMPTY_INT;
    }
    if ((val & I2C_INTR_STAT_R_RX_FULL_MASK) != 0U)
    {
        res |= I2C_RX_FULL_INT;
    }
    if ((val & I2C_INTR_STAT_R_TX_ABRT_MASK) != 0U)
    {
        res |= I2C_TX_ABORT_INT;
    }
    if ((val & I2C_INTR_STAT_R_STOP_DET_MASK) != 0U)
    {
        res |= I2C_STOP_DET_INT;
    }
    if ((val & I2C_INTR_STAT_R_RD_REQ_MASK) != 0U)
    {
        res |= I2C_RD_REQ_INT;
    }
    if ((val & I2C_INTR_STAT_R_RX_OVER_MASK) != 0U)
    {
        res |= I2C_RX_OVER_INT;
    }
    return res;
}

uint32_t i2c_ll_get_raw_interrupt_status(uint32_t base_addr)
{
    return RD_REG32(base_addr + I2C_RAW_INTR_STAT);
}

uint32_t i2c_ll_get_tx_abrt_source(uint32_t base_addr)
{
    return RD_REG32(base_addr + I2C_TX_ABRT_SOURCE);
}

void i2c_ll_clear_tx_abrt(uint32_t base_addr)
{
    (void)RD_REG32(base_addr + I2C_CLR_TX_ABRT);
}

void i2c_ll_clear_rd_req(uint32_t base_addr)
{
    (void)RD_REG32(base_addr + I2C_CLR_RD_REQ);
}

void i2c_ll_clear_stop_det(uint32_t base_addr)
{
    (void)RD_REG32(base_addr + I2C_CLR_STOP_DET);
}

void i2c_ll_clear_rx_over(uint32_t base_addr)
{
    (void)RD_REG32(base_addr + I2C_CLR_RX_OVER);
}

void i2c_ll_config_slave(uint32_t base_addr,
        uint16_t slave_addr,
        bool is_10bit_addr,
        bool stop_det_ifaddressed,
        bool ack_general_call,
        uint8_t rx_tl,
        uint8_t tx_tl)
{
    uint32_t val;

    /* Disable controller */
    WR_REG32(base_addr + I2C_ENABLE, 0U);

    /* Program slave address (IC_SAR) */
    val = ((uint32_t)slave_addr & I2C_SAR_IC_SAR_MASK);
    WR_REG32(base_addr + I2C_SAR, val);

    /* Program control register for slave-only mode */
    val = RD_REG32(base_addr + I2C_CON);
    val &= ~(I2C_CON_MASTER_MODE_MASK |
            I2C_CON_IC_SLAVE_DISABLE_MASK |
            I2C_CON_IC_10BITADDR_SLAVE_MASK |
            I2C_CON_STOP_DET_IFADDRESSED_MASK);

    /* Slave enabled, master disabled */
    if (is_10bit_addr)
    {
        val |= I2C_CON_IC_10BITADDR_SLAVE_MASK;
    }
    if (stop_det_ifaddressed)
    {
        val |= I2C_CON_STOP_DET_IFADDRESSED_MASK;
    }

    /* Recommended: enable restart */
    val |= I2C_CON_IC_RESTART_EN_MASK;
    WR_REG32(base_addr + I2C_CON, val);

    /* Program FIFO interrupt thresholds */
    WR_REG32(base_addr + I2C_RX_TL, (uint32_t)rx_tl);
    WR_REG32(base_addr + I2C_TX_TL, (uint32_t)tx_tl);

    /* General call ACK behavior */
    WR_REG32(base_addr + I2C_ACK_GENERAL_CALL,
            ack_general_call ? I2C_ACK_GENERAL_CALL_ACK_GEN_CALL_MASK : 0U);

    /* Clear any pending interrupts once at enable */
    (void)RD_REG32(base_addr + I2C_CLR_INTR);

    /* Enable controller */
    WR_REG32(base_addr + I2C_ENABLE, I2C_ENABLE_ENABLE_MASK);
}

uint32_t i2c_read_status(uint32_t base_addr)
{
    uint32_t val;

    val = RD_REG32(base_addr + I2C_STATUS);
    return val;
}

/**
 * @brief Clear I2C interrupt
 */
void i2c_clear_interrupt(uint32_t base_addr)
{
    /*
     * Reading IC_CLR_INTR clears all interrupts that can be cleared
     * by software.
     */
    (void)RD_REG32(base_addr + I2C_CLR_INTR);
}

/**
 * @brief Set target address
 */
void i2c_set_target_addr(uint32_t base_addr, uint32_t slave_addr)
{
    uint32_t val;
    /*Disable i2c*/
    val = RD_REG32(base_addr + I2C_ENABLE);
    val &= ~(1U << I2C_ENABLE_ENABLE_POS);
    WR_REG32(base_addr + I2C_ENABLE, val);

    /*
     * Set the Target address
     * And keep the 10 bit addressing enable cleared
     */
    val = ((slave_addr) << I2C_TAR_IC_TAR_POS);
    WR_REG32(base_addr + I2C_TAR, val);

    /*Enable i2c*/
    val = RD_REG32(base_addr + I2C_ENABLE);
    val |= (1U << I2C_ENABLE_ENABLE_POS);
    WR_REG32(base_addr + I2C_ENABLE, val);
}

/**
 * @brief Write data to the I2C FIFO
 */
uint16_t i2c_write_fifo(uint32_t base_addr, uint8_t *const buf, uint32_t nbytes,
        bool no_stop_flag)
{
    uint32_t nwr;
    uint32_t val;
    /*Write data to the data command register*/
    for (nwr = 0; nwr < nbytes; nwr++)
    {
        /* Exit if fifo is full */
        if (((RD_REG32(base_addr + I2C_STATUS)) & I2C_STATUS_TFNF_MASK) == 0U)
        {
            break;
        }
        else if ((nwr == (nbytes - 1U)) && (!no_stop_flag))
        {
            /* Write with stop */
            val = buf[nwr];
            val |= ((uint32_t)1U << I2C_DATA_CMD_STOP_POS);
        }
        else
        {
            /* Write with no stop */
            val = buf[nwr];
        }

        WR_REG32(base_addr + I2C_DATA_CMD, val);
    }
    return nwr;
}

/**
 * @brief Enqueue read commands
 */
uint16_t i2c_enq_read_cmd(uint32_t base_addr, uint32_t nbytes, bool no_stop_flag)
{
    uint32_t ncmd;
    uint32_t val;

    for (ncmd = 0; ncmd < nbytes; ncmd++)
    {
        /* exit if fifo full*/
        if (((RD_REG32(base_addr + I2C_STATUS)) & I2C_STATUS_TFNF_MASK) == 0U)
        {
            break;
        }
        else if ((ncmd == (nbytes - 1U)) && (!no_stop_flag))
        {
            /* read command with stop */
            val = ((uint32_t)(1U << I2C_DATA_CMD_STOP_POS) |
                    (uint32_t)(1U << I2C_DATA_CMD_CMD_POS));
        }
        else
        {
            /*  read command with no stop */
            val = ((uint32_t)1U << I2C_DATA_CMD_CMD_POS);
        }

        WR_REG32(base_addr + I2C_DATA_CMD, val);
    }
    return ncmd;
}

/**
 * @brief Read data from the I2C FIFO
 */
uint16_t i2c_read_fifo(uint32_t base_addr, uint8_t *const buf, uint32_t nbytes)
{
    uint32_t nrd;
    uint32_t val;

    for (nrd = 0; nrd < nbytes; nrd++)
    {
        /* Exit if fifo is empty */
        if (((RD_REG32(base_addr + I2C_STATUS)) & I2C_STATUS_RFNE_MASK) == 0U)
        {
            break;
        }
        val = RD_REG32(base_addr + I2C_DATA_CMD);
        buf[nrd] = (uint8_t)val & I2C_DATA_CMD_DAT_MASK;
    }
    return nrd;
}

/**
 * @brief Configure I2C master parameters
 */
uint32_t i2c_config_master(uint32_t base_addr, uint32_t speed)
{
    uint32_t val;

    /* Disable i2c module while configuring the speed */
    val = RD_REG32(base_addr + I2C_ENABLE);
    val &= ~(1U << I2C_ENABLE_ENABLE_POS);
    WR_REG32(base_addr + I2C_ENABLE, val);

    if (speed <= I2C_STANDARD_MODE_BPS)
    {
        val = RD_REG32(base_addr + I2C_CON) & ~I2C_CON_SPEED_MASK;
        val |= (I2C_CONTRL_STD_SPEED << I2C_CON_SPEED_POS);
        WR_REG32(base_addr + I2C_CON, val);

        WR_REG32(base_addr + I2C_SS_SCL_HCNT, SS_SCL_HCNT_VAL);
        WR_REG32(base_addr + I2C_SS_SCL_LCNT, SS_SCL_HCNT_VAL);
    }
    else if ((speed > I2C_STANDARD_MODE_BPS) && (speed <=
            I2C_FAST_MODE_PLUS_BPS))
    {
        val = RD_REG32(base_addr + I2C_CON) & ~I2C_CON_SPEED_MASK;
        val |= (I2C_CONTRL_FAST_SPEED << I2C_CON_SPEED_POS);
        WR_REG32(base_addr + I2C_CON, val);

        WR_REG32(base_addr + I2C_FS_SCL_HCNT, FS_SCL_HCNT_VAL);
        WR_REG32(base_addr + I2C_FS_SCL_LCNT, FS_SCL_HCNT_VAL);
    }
    else if ((speed > I2C_FAST_MODE_PLUS_BPS) && (speed <= I2C_HIGH_SPEED_BPS))
    {
        val = RD_REG32(base_addr + I2C_CON) & ~I2C_CON_SPEED_MASK;
        val |= (I2C_CONTRL_HIGH_SPEED << I2C_CON_SPEED_POS);
        WR_REG32(base_addr + I2C_CON, val);
        WR_REG32(base_addr + I2C_SS_SCL_HCNT, SS_SCL_HCNT_VAL);
        WR_REG32(base_addr + I2C_SS_SCL_LCNT, SS_SCL_HCNT_VAL);

    }
    else
    {
        return 0;
    }

    /* Enable the i2c module */
    val = RD_REG32(base_addr + I2C_ENABLE);
    val |= (1U << I2C_ENABLE_ENABLE_POS);
    WR_REG32(base_addr + I2C_ENABLE, val);

    return 1;
}

/**
 * @brief Get I2C configuration parameters
 */
uint32_t i2c_get_config(uint32_t base_addr)
{
    uint32_t baud = 0U;
    i2c_mode_t val = (i2c_mode_t)((RD_REG32(base_addr + I2C_CON) &
            I2C_CON_SPEED_MASK) >> I2C_CON_SPEED_POS);

    if (val == I2C_MODE_FAST)
    {
        baud = I2C_FAST_MODE_BPS;
    }
    else if (val == I2C_MODE_HS)
    {
        baud = I2C_HIGH_SPEED_BPS;
    }
    else
    {
        baud = I2C_STANDARD_MODE_BPS;
    }
    return baud;
}

/**
 * @brief Disable the I2C controller.
 */
void i2c_ll_disable_controller(uint32_t base_addr)
{
    WR_REG32(base_addr + I2C_ENABLE, 0U);
}

/**
 * @brief Enable the I2C controller.
 */
void i2c_ll_enable_controller(uint32_t base_addr)
{
    WR_REG32(base_addr + I2C_ENABLE, (1U << I2C_ENABLE_ENABLE_POS));
}

/**
 * @brief Set I2C interrupt mask.
 */
void i2c_ll_set_interrupt_mask(uint32_t base_addr, uint32_t mask)
{
    WR_REG32(base_addr + I2C_INTR_MASK, mask);
}

static void i2c_ll_init_controller(uint32_t base_addr)
{
    uint32_t val;

    /* Disable controller before configuring */
    i2c_ll_disable_controller(base_addr);

    /*
     * Configure I2C control register:
     * - Enable master mode
     * - Standard speed (can be updated later via i2c_config_master)
     * - Disable slave
     * - Enable restart
     */
    val = 1U << I2C_CON_MASTER_MODE_POS;
    val |= 1U << I2C_CON_IC_SLAVE_DISABLE_POS;
    val |= 1U << I2C_CON_IC_RESTART_EN_POS;
    val |= I2C_CONTRL_STD_SPEED << I2C_CON_SPEED_POS;
    WR_REG32(base_addr + I2C_CON, val);

    /* Mask all interrupts */
    i2c_ll_set_interrupt_mask(base_addr, 0U);

    /* Enable controller */
    i2c_ll_enable_controller(base_addr);
}

static void i2c_ll_deinit_controller(uint32_t base_addr)
{
    /* Mask interrupts and disable controller */
    i2c_ll_set_interrupt_mask(base_addr, 0U);
    i2c_ll_disable_controller(base_addr);

    /* Clear abort state to release TX FIFO from flushed condition */
    (void)RD_REG32(base_addr + I2C_CLR_TX_ABRT);
}

/**
 * @brief Initializes I2C peripheral.
 *
 * Places the controller into a known master-default state:
 * - controller disabled while configuring
 * - master enabled, slave disabled
 * - standard-speed mode selected
 * - restart enabled
 * - all interrupts masked
 * - controller enabled
 */
void i2c_init(uint32_t base_addr)
{
    i2c_ll_init_controller(base_addr);
}

/**
 * @brief Deinitializes I2C peripheral.
 *
 * Disables the controller and masks interrupts. Clears abort state so that
 * subsequent initializations do not start with TX FIFO in flushed condition.
 */
void i2c_deinit(uint32_t base_addr)
{
    i2c_ll_deinit_controller(base_addr);
}

/**
 * @brief Cancel current transaction
 */
void i2c_ll_cancel(uint32_t base_addr)
{
    uint32_t val;
    /* Abort current operation */
    val = RD_REG32(base_addr + I2C_ENABLE);
    val |= I2C_ENABLE_ABORT_MASK;
    WR_REG32(base_addr + I2C_ENABLE, val);

    /* Clear all abort interrupts */
    (void)RD_REG32(base_addr + I2C_CLR_TX_ABRT);
}

void i2c_ll_tdma_enable(uint32_t base_addr)
{
    uint32_t val;

    val = RD_REG32(base_addr + I2C_DMA_CR);
    val |= (1U << I2C_DMA_CR_TDMAE_POS);
    WR_REG32((base_addr + I2C_DMA_CR), val);
}

void i2c_ll_rdma_enable(uint32_t base_addr)
{
    uint32_t val;

    val = RD_REG32(base_addr + I2C_DMA_CR);
    val |= (1U << I2C_DMA_CR_RDMAE_POS);
    WR_REG32((base_addr + I2C_DMA_CR), val);
}

void i2c_ll_tdma_disable(uint32_t base_addr)
{
    uint32_t val;

    val = RD_REG32(base_addr + I2C_DMA_CR);
    val &= ~(1U << I2C_DMA_CR_TDMAE_POS);
    WR_REG32((base_addr + I2C_DMA_TDLR), 0U);
    WR_REG32((base_addr + I2C_DMA_CR), val);
}

void i2c_ll_rdma_disable(uint32_t base_addr)
{
    uint32_t val;

    val = RD_REG32(base_addr + I2C_DMA_CR);
    val &= ~(1U << I2C_DMA_CR_RDMAE_POS);
    WR_REG32((base_addr + I2C_DMA_RDLR), 0U);
    WR_REG32((base_addr + I2C_DMA_CR), val);
}

void i2c_ll_set_tdlr(uint32_t base_addr, uint32_t burst_size)
{
    uint32_t val;

    val = I2C_FIFO_DEPTH - burst_size;
    WR_REG32((base_addr + I2C_DMA_TDLR), val);
}

void i2c_ll_set_rdlr(uint32_t base_addr, uint32_t burst_size)
{
    uint32_t val;

    val = burst_size - 1;
    WR_REG32((base_addr + I2C_DMA_RDLR), val);
}
