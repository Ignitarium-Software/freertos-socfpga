/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SPI
 */

#include "socfpga_defines.h"
#include <errno.h>
#include "socfpga_spi_ll.h"
#include "socfpga_clk_mngr.h"
#include "socfpga_spi_reg.h"
#include "socfpga_rst_mngr.h"

/**
 * @brief Get clock shift based on role/instance.
 */
static uint32_t spi_get_clk_shift(uint32_t instance, spi_role_t role)
{
    if (role == SPI_ROLE_SLAVE)
    {
        return (instance == 1U) ? CLK_SPIS1_SHIFT : CLK_SPIS0_SHIFT;
    }
    return (instance == 1U) ? CLK_SPIM1_SHIFT : CLK_SPIM0_SHIFT;
}

/**
 * @brief Enable the clock for the SPI instance.
 */
static void spi_enable_clock(uint32_t instance, spi_role_t role)
{
    uint32_t val;
    uint32_t shift;

    shift = spi_get_clk_shift(instance, role);
    val = RD_REG32(CLK_PERPLL);
    val |= (1U << shift);
    WR_REG32(CLK_PERPLL_EN, val);
}

/**
 * @brief Disable the clock for the SPI instance.
 */
static void spi_disable_clock(uint32_t instance, spi_role_t role)
{
    uint32_t val;
    uint32_t shift;

    shift = spi_get_clk_shift(instance, role);
    val = RD_REG32(CLK_PERPLL);
    val &= ~(1U << shift);
    WR_REG32(CLK_PERPLL_EN, val);
}

/**
 * @brief Enable SPI serial interface.
 */
void spi_enable(uint32_t base_addr)
{
    uint32_t val = 0U;
    val |= 1U << SPI_SSIENR_SSI_EN_POS;
    WR_REG32((base_addr + SPI_SSIENR), val);
}

/**
 * @brief Disable SPI serial interface.
 */
void spi_disable(uint32_t base_addr)
{
    uint32_t val = 0U;
    WR_REG32((base_addr + SPI_SSIENR), val);
}

void spi_ll_enable_dma(uint32_t base_addr, bool tx_enable, bool rx_enable)
{
    uint32_t val;

    val = RD_REG32(base_addr + SPI_DMACR);
    if (tx_enable == true)
    {
        val |= (1U << SPI_DMACR_TDMAE_POS);
    }
    else
    {
        val &= ~(1U << SPI_DMACR_TDMAE_POS);
    }

    if (rx_enable == true)
    {
        val |= (1U << SPI_DMACR_RDMAE_POS);
    }
    else
    {
        val &= ~(1U << SPI_DMACR_RDMAE_POS);
    }

    WR_REG32((base_addr + SPI_DMACR), val);
}

void spi_ll_disable_dma(uint32_t base_addr)
{
    uint32_t val;

    val = RD_REG32(base_addr + SPI_DMACR);
    val &= ~((1U << SPI_DMACR_TDMAE_POS) | (1U << SPI_DMACR_RDMAE_POS));
    WR_REG32((base_addr + SPI_DMACR), val);
}

void spi_ll_set_dma_thresholds(uint32_t base_addr, uint8_t tx_level,
        uint8_t rx_level)
{
    uint32_t val;

    val = RD_REG32(base_addr + SPI_DMATDLR);
    val &= ~SPI_DMATDLR_DMATDL_MASK;
    val |= ((uint32_t)tx_level << SPI_DMATDLR_DMATDL_POS);
    WR_REG32((base_addr + SPI_DMATDLR), val);

    val = RD_REG32(base_addr + SPI_DMARDLR);
    val &= ~SPI_DMARDLR_DMARDL_MASK;
    val |= ((uint32_t)rx_level << SPI_DMARDLR_DMARDL_POS);
    WR_REG32((base_addr + SPI_DMARDLR), val);
}

uint32_t spi_get_base_addr(uint32_t instance, spi_role_t role)
{
    if (role == SPI_ROLE_SLAVE)
    {
        return GET_BASE_ADDR_SLAVE(instance);
    }
    return GET_BASE_ADDR_MASTER(instance);
}

void spi_set_slave_output(uint32_t base_addr, bool enable)
{
    uint32_t val;

    spi_disable(base_addr);
    val = RD_REG32(base_addr + SPI_CTRLR0);
    if (enable == true)
    {
        val &= ~(1U << SPI_CTRLR0_SLV_OE_POS);
    }
    else
    {
        val |= (1U << SPI_CTRLR0_SLV_OE_POS);
    }
    WR_REG32((base_addr + SPI_CTRLR0), val);
    spi_enable(base_addr);
}

/**
 * @brief Set SPI FIFO threshold.
 */
static void spi_set_fifo_threshold(uint32_t base_addr)
{
    uint32_t val;

    /*transmit FIFO threshold setting*/
    val = RD_REG32(base_addr + SPI_TXFTLR);
    val = val & ~(SPI_TXFTLR_TFT_MASK);
    val |= ((uint32_t)SPI_TX_FIFO_THRESHOLD << SPI_TXFTLR_TFT_POS) &
            SPI_TXFTLR_TFT_MASK;
    WR_REG32((base_addr + SPI_TXFTLR), val);

    /*receive FIFO threshold setting*/
    val = RD_REG32(base_addr + SPI_RXFTLR);
    val = val & ~(SPI_RXFTLR_RFT_MASK);
    val |= ((uint32_t)SPI_RX_FIFO_THRESHOLD << SPI_RXFTLR_RFT_POS) &
            SPI_RXFTLR_RFT_MASK;
    WR_REG32((base_addr + SPI_RXFTLR), val);
}

/**
 * @brief Initialize SPI instance.
 */
void spi_init(uint32_t instance, spi_role_t role)
{
    volatile int32_t i;
    reset_periphrl_t reset_id;

    spi_enable_clock(instance, role);

    if (role == SPI_ROLE_SLAVE)
    {
        reset_id = (instance == 1U) ? RST_SPIS1 : RST_SPIS0;
    }
    else
    {
        reset_id = (instance == 1U) ? RST_SPIM1 : RST_SPIM0;
    }

    if (rstmgr_assert_reset(reset_id) != 0)
    {
        return;
    }

    for (i = 0; i < 100; i++)
    {
    }
    if (rstmgr_deassert_reset(reset_id) != 0)
    {
        return;
    }

    spi_set_fifo_threshold(spi_get_base_addr(instance, role));
}

/**
 * @brief Deinitialize SPI instance.
 */
void spi_deinit(uint32_t instance, spi_role_t role)
{
    uint32_t base_addr;

    base_addr = spi_get_base_addr(instance, role);
    spi_disable(base_addr);
    spi_disable_clock(instance, role);
}

/**
 * @brief Set SPI configuration.
 */
int32_t spi_set_config(uint32_t base_addr, uint32_t freq, spi_mode_t mode)
{
    uint32_t sclk_dvsr;
    uint32_t spi_clk = 0U;
    uint32_t val;

    spi_disable(base_addr);

    val = RD_REG32(base_addr + SPI_CTRLR0);
    val &= ~((uint32_t)3U << SPI_CTRLR0_SPI_FRF_POS);
    val &= ~((uint32_t)1U << SPI_CTRLR0_SCPH_POS);
    val &= ~((uint32_t)1U << SPI_CTRLR0_SCPOL_POS);
    val &= ~SPI_CTRLR0_DFS_MASK;
    val &= ~SPI_CTRLR0_DFS_32_MASK;
    val |= (7U << SPI_CTRLR0_DFS_POS);

    switch (mode)
    {
        case SPI_MODE1:
            val |= 1U << SPI_CTRLR0_SCPH_POS;
            break;

        case SPI_MODE2:
            val |= 1U << SPI_CTRLR0_SCPOL_POS;
            break;

        case SPI_MODE3:
            val |= 1U << SPI_CTRLR0_SCPH_POS;
            val |= 1U << SPI_CTRLR0_SCPOL_POS;
            break;

        default:
            /* mode SPI_MODE0 */
            break;
    }

    WR_REG32((base_addr + SPI_CTRLR0), val);

    if (clk_mngr_get_clk(CLOCK_SSPI, &spi_clk) != 0U)
    {
        return -EIO;
    }
    if (spi_clk == 0U)
    {
        return -EIO;
    }
    if (freq == 0U)
    {
        return -EINVAL;
    }

    sclk_dvsr = spi_clk / freq;

    /* BAUDR.SCKDV must be even; clamp to 65534 (0 disables sclk_out). */
    if (sclk_dvsr > 65534U)
    {
        sclk_dvsr = 65534U;
    }
    if (sclk_dvsr < 2U)
    {
        sclk_dvsr = 2U;
    }
    sclk_dvsr &= ~1U;

    WR_REG32((base_addr + SPI_BAUDR), (sclk_dvsr & SPI_BAUDR_SCKDV_MASK));

    return 0;
}

/**
 * @brief Get SPI configuration parameters
 */
void spi_get_config(uint32_t base_addr, uint32_t *freq, spi_mode_t *mode)
{
    uint32_t val;
    uint32_t bf_val;
    val = 0U;
    bf_val = 0U;

    *freq = spi_get_freq(base_addr);

    val = RD_REG32(base_addr + SPI_CTRLR0);
    bf_val = (val >> SPI_CTRLR0_SCPH_POS) & 3U;

    if (bf_val == 3U)
    {
        *mode = SPI_MODE3;
    }

    else if (bf_val == 2U)
    {
        *mode = SPI_MODE2;
    }

    else if (bf_val == 1U)
    {
        *mode = SPI_MODE1;
    }

    else
    {
        *mode = SPI_MODE0;
    }
}

/**
 * @brief Set SPI transfer mode.
 */
void spi_set_transfermode(uint32_t base_addr, uint32_t mode)
{
    uint32_t val = 0U;

    spi_disable(base_addr);
    val = RD_REG32(base_addr + SPI_CTRLR0);

    val &= ~((uint32_t)3U << SPI_CTRLR0_TMOD_POS);
    val |= (mode & 3U) << SPI_CTRLR0_TMOD_POS;

    WR_REG32((base_addr + SPI_CTRLR0), val);
    spi_enable(base_addr);
}

/**
 * @brief Get SPI transfer frequency.
 */
uint32_t spi_get_freq(uint32_t base_addr)
{
    uint32_t sclk_dvsr, spi_clk = 0U;
    uint32_t freq;

    if (clk_mngr_get_clk(CLOCK_SSPI, &spi_clk) != 0U)
    {
        return 0U;
    }
    if (spi_clk == 0U)
    {
        return 0U;
    }

    sclk_dvsr = RD_REG32(base_addr + SPI_BAUDR);
    if (sclk_dvsr == 0U)
    {
        return 0U;
    }
    freq = spi_clk / sclk_dvsr;

    return freq;
}

/**
 * @brief Select SPI slave
 */
void spi_select_chip(uint32_t instance, uint32_t slave)
{
    uint32_t base_addr;
    uint32_t val;
    base_addr = GET_BASE_ADDR(instance);

    val = RD_REG32(base_addr + SPI_SER);
    val &= ~(SPI_SER_SER_MASK << 0);
    WR_REG32((base_addr + SPI_SER), val);

    switch (slave)
    {
        case 1U:
            val = 1U;
            break;
        case 2U:
            val = 2U;
            break;
        case 3U:
            val = 4U;
            break;
        case 4U:
            val = 8U;
            break;
        default:
            val = 0U;
            break;
    }
    WR_REG32((base_addr + SPI_SER), val);
}

/**
 * @brief Write data to Tx FIFO.
 */
uint16_t spi_write_fifo(uint32_t base_addr, uint8_t *buf, uint16_t bytes)
{
    uint16_t bytes_done = 0;

    /* dummy write */
    if (buf == NULL)
    {
        while (bytes > 0U)
        {
            if (((RD_REG32(base_addr + SPI_SR) >> SPI_SR_TFNF_POS) & 1U) ==
                    1U)
            {
                WR_REG32((base_addr + SPI_DR0), 0x55U);
                bytes_done++;
                bytes--;
            }
            else
            {
                break;
            }
        }
    }

    else
    {
        while (bytes > 0U)
        {
            if (((RD_REG32(base_addr + SPI_SR) >> SPI_SR_TFNF_POS) & 1U) ==
                    1U)
            {
                WR_REG32((base_addr + SPI_DR0), buf[bytes_done]);
                bytes_done++;
                bytes--;
            }
            else
            {
                break;
            }
        }
    }
    return bytes_done;
}

/**
 * @brief Read data from Rx FIFO.
 */
uint16_t spi_read_fifo(uint32_t base_addr, uint8_t *buf, uint16_t bytes)
{
    uint16_t bytes_done = 0;

    /* dummy read */
    if (buf == NULL)
    {
        while (bytes > 0U)
        {
            if (((RD_REG32(base_addr + SPI_SR) >> SPI_SR_RFNE_POS) & 1U) ==
                    1U)
            {
                (void)RD_REG32(base_addr + SPI_DR0);
                bytes_done++;
                bytes--;
            }
            else
            {
                break;
            }
        }

    }

    else
    {
        while (bytes > 0U)
        {
            if (((RD_REG32(base_addr + SPI_SR) >> SPI_SR_RFNE_POS) & 1U) ==
                    1U)
            {
                buf[bytes_done] = (uint8_t)RD_REG32(base_addr + SPI_DR0);
                bytes_done++;
                bytes--;
            }
            else
            {
                break;
            }
        }
    }
    return bytes_done;
}

/**
 * @brief Get SPI interrupt status.
 *
 * Returns the masked ISR bitmask (SPI_ISR). Callers check individual sources
 * with the SPI_*_INT bitmask constants (SPI_TX_EMPTY_INT, SPI_RX_FULL_INT,
 * SPI_RX_OVERFLOW_INT) rather than comparing the whole value.
 */
uint32_t spi_get_interrupt_status(uint32_t base_addr)
{
    return (RD_REG32(base_addr + SPI_ISR) & 0x3FU);
}

/**
 * @brief Enable SPI interrupts
 */
void spi_enable_interrupt(uint32_t base_addr, uint32_t ir_id)
{
    uint32_t val;
    val = RD_REG32(base_addr + SPI_IMR);

    switch (ir_id)
    {
        case SPI_TX_EMPTY_INT:
            val |= 1U << SPI_IMR_TXEIM_POS;
            break;

        case SPI_RX_OVERFLOW_INT:
            val |= 1U << SPI_IMR_RXOIM_POS;
            break;

        case SPI_RX_FULL_INT:
            val |= 1U << SPI_IMR_RXFIM_POS;
            break;

        case SPI_ALL_INTERRUPTS:
            val |= (0x3FU << SPI_IMR_TXEIM_POS);
            break;

        default:
            /* do nothing */
            break;
    }

    WR_REG32((base_addr + SPI_IMR), val);
}

/**
 * @brief Disable SPI interrupts
 */
void spi_disable_interrupt(uint32_t base_addr, uint32_t ir_id)
{
    uint32_t val;
    val = RD_REG32(base_addr + SPI_IMR);

    switch (ir_id)
    {
        case SPI_TX_EMPTY_INT:
            val &= ~(1U << SPI_IMR_TXEIM_POS);
            break;

        case SPI_RX_OVERFLOW_INT:
            val &= ~(1U << SPI_IMR_RXOIM_POS);
            break;

        case SPI_RX_FULL_INT:
            val &= ~(1U << SPI_IMR_RXFIM_POS);
            break;

        case SPI_ALL_INTERRUPTS:
            val &= ~(0x3FU << SPI_IMR_TXEIM_POS);
            break;

        default:
            /* do nothing */
            break;
    }

    WR_REG32((base_addr + SPI_IMR), val);
}

/**
 * @brief Clear the RX FIFO overflow interrupt by reading RXOICR.
 */
void spi_clear_rx_overflow(uint32_t base_addr)
{
    (void)RD_REG32(base_addr + SPI_RXOICR);
}

/**
 * @brief Set the RX FIFO threshold (RXFTLR).
 */
void spi_ll_set_rx_threshold(uint32_t base_addr, uint8_t rx_thr)
{
    WR_REG32(base_addr + SPI_RXFTLR, (uint32_t)rx_thr);
}
