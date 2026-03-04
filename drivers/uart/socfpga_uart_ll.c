/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for UART
 */

#include <string.h>
#include "osal.h"

#include "socfpga_uart_ll.h"
#include "socfpga_defines.h"
#include "socfpga_uart_reg.h"
#include "socfpga_clk_mngr.h"
#include "socfpga_rst_mngr.h"

/**
 * @brief Enable clock for UART
 */
static void uart_enable_clock(uint32_t instance)
{
    uint32_t val;
    val = RD_REG32(CLK_PERPLL);
    val |= (1U << (CLK_EN_SHIFT + instance));
    WR_REG32(CLK_PERPLL_EN, val);
}

/**
 * @brief Disable UART clock
 */
static void uart_disable_clock(uint32_t instance)
{
    uint32_t val;
    val = RD_REG32(CLK_PERPLL);
    val &= ~(1U << (CLK_EN_SHIFT + instance));
    WR_REG32(CLK_PERPLL_EN, val);
}

/**
 * @brief Assert UART reset
 */
static void uart_assert_reset(uint32_t instance)
{
    reset_periphrl_t reset_inst;
    reset_inst = (instance == 1U) ? RST_UART1: RST_UART0;
    (void)rstmgr_assert_reset(reset_inst);
}

/**
 * @brief Deassert UART reset
 */
static void uart_deassert_reset(uint32_t instance)
{
    reset_periphrl_t reset_inst;
    reset_inst = (instance == 1U) ? RST_UART1: RST_UART0;
    (void)rstmgr_deassert_reset(reset_inst);
}

/**
 * @brief Initialize UART and configure the FIFO
 */
void uart_init(uint32_t instance)
{
    volatile int32_t i = 0;
    uart_enable_clock(instance);

    uart_assert_reset(instance);

    /* ~12.5 us delay is needed after reset */
    for (i = 0; i < 100; i++)
    {

    }

    uart_deassert_reset(instance);

    for (i = 0; i < 10000; i++)
    {

    }

    uart_config_fifo(GET_UART_BASE_ADDRESS(instance));
}

/**
 * @brief Deinitialize UART
 */
void uart_deinit(uint32_t instance)
{
    uart_disable_clock(instance);
}

/**
 * @brief Configure UART
 */
void uart_set_config(uint32_t base_address, uart_partity_t parity,
        uart_stop_bits_t stopbit, uint32_t wordlen)
{
    uint32_t val = 0U;

    switch (parity)
    {
        case UART_PARITY_ODD:
            val |= (1U << UART_LCR_PEN_POS);
            break;
        case UART_PARITY_EVEN:
            val |= (1U << UART_LCR_PEN_POS);
            val |= (1U << UART_LCR_EPS_POS);
            break;
        default:
            /* default - no parity*/
            break;
    }

    val |= ((wordlen - 5U) << UART_LCR_DLS_POS);

    if (stopbit == UART_STOP_BITS_2)
    {
        val |= (1U << UART_LCR_STOP_POS);
    }
    else
    {
        /* default - 1 stop bit */
    }

    WR_REG32((base_address + UART_LCR), val);
}

/**
 * @brief Set UART baud rate
 */
uint32_t uart_set_baud(uint32_t base_address, uint32_t baud)
{
    uint32_t peri_clk = 0U;
    uint32_t lcr_var;
    uint32_t val;
    uint32_t lcr_data;

    if (clk_mngr_get_clk(CLOCK_UART, &peri_clk) != 0)
    {
        return 0U;
    }

    if (baud == 0U)
    {
        return 0U;
    }

    lcr_var = ((peri_clk + (baud << 3U)) / baud) >> 4U;

    lcr_data = RD_REG32(base_address + UART_LCR);

    /* The DLAB bit shall be set to enable access to the divisor latch registers */
    lcr_data |= (1U << UART_LCR_DLAB_POS);
    WR_REG32((base_address + UART_LCR), lcr_data);

    val = (lcr_var & 0xFFU);
    WR_REG32((base_address + UART_DLL), val);

    val = ((lcr_var >> 8U) & 0xFFU);
    WR_REG32((base_address + UART_DLH), val);

    /* The DLAB bit shall be set to 0 after setting baud rate to access RBR and IER */
    lcr_data &= ~(1U << UART_LCR_DLAB_POS);
    WR_REG32((base_address + UART_LCR), lcr_data);

    return 1U;
}

/**
 * @brief Configure UART FIFO
 */
void uart_config_fifo(uint32_t base_address)
{
    uint32_t val = 0U;

    val |= (1U << UART_FCR_FIFOE_POS);
    val |= (1U << UART_FCR_RFIFOR_POS);
    val |= (1U << UART_FCR_XFIFOR_POS);

    /* rx trigger is set as 1 byte
     * as we don't know the exact number of bytes to receive
     */

    WR_REG32((base_address + UART_FCR), val);
}

/**
 * @brief Get UART configuration parameters
 */
void uart_get_config(uint32_t base_address, uint32_t *baud,
        uart_partity_t *parity, uart_stop_bits_t *stopbit,
        uint32_t *wordlen)
{
    uint32_t val;
    uint32_t bf_val;

    *baud = uart_get_baud(base_address);

    val = RD_REG32(base_address + UART_LCR);
    bf_val = (val >> UART_LCR_PEN_POS) & 3U;
    if (bf_val == 3U)
    {
        *parity = UART_PARITY_EVEN;
    }
    else if (bf_val == 1U)
    {
        *parity = UART_PARITY_ODD;
    }
    else
    {
        *parity = UART_PARITY_NONE;
    }

    bf_val = (val >> UART_LCR_STOP_POS) & 1U;
    if (bf_val == 1U)
    {
        *stopbit = UART_STOP_BITS_2;
    }
    else
    {
        *stopbit = UART_STOP_BITS_1;
    }

    bf_val = (val >> UART_LCR_DLS_POS) & 3U;
    *wordlen = bf_val + 5U;
}

/**
 * @brief Get UART baud rate
 */
uint32_t uart_get_baud(uint32_t base_address)
{
    uint32_t lcr_var;
    uint32_t baud;
    uint32_t lcr_data;
    uint32_t div_lsb;
    uint32_t div_msb;
    uint32_t peri_clk = 0U;

    lcr_var = 0U;
    /* enable DLAB bit to access DLL and DLH registers */
    lcr_data = RD_REG32(base_address + UART_LCR);

    lcr_data |= (1U << UART_LCR_DLAB_POS);
    WR_REG32((base_address + UART_LCR), lcr_data);

    div_lsb = RD_REG32(base_address + UART_DLL);
    div_msb = RD_REG32(base_address + UART_DLH);

    /* disable DLAB bit to access RBR and IER registers */
    lcr_data &= ~(1U << UART_LCR_DLAB_POS);
    WR_REG32((base_address + UART_LCR), lcr_data);

    lcr_var |= div_msb << 8U;
    lcr_var |= div_lsb;

    if (clk_mngr_get_clk(CLOCK_UART, &peri_clk) != 0)
    {
        baud = 0U;
    }
    else
    {
        if (lcr_var != 0U)
        {
            baud = peri_clk / ((lcr_var << 4U) - 8U);
        }
        else
        {
            baud = 0U;
        }
    }
    return baud;
}

/**
 * @brief Get UART interrupt status
 */
uint32_t get_int_status(uint32_t base_address)
{
    volatile uint32_t val;
    uint8_t mask;
    uint32_t ret = UART_NO_INT;

    mask = 0xFU;
    val = RD_REG32(base_address + UART_IIR);
    val &= mask;

    switch (val)
    {
        case NO_INTERRUPT:
            ret = UART_NO_INT;
            break;
        case RX_DATA_RDY:
        case CHAR_TIMEOUT:
            ret = UART_RXBUF_RDY_INT;
            break;

        case THR_EMPTY:
            ret = UART_TXBUF_EMPTY_INT;
            break;

        default:
            ret = UART_HW_ERR_INT;
            break;
    }
    return ret;
}

/**
 * @brief Write data to UART transmit FIFO
 */
uint16_t uart_write_fifo(uint32_t base_address, uint8_t *const buf,
        uint32_t bytes)
{
    uint16_t bytes_done = 0U;

    while (bytes > 0U)
    {
        if (((RD_REG32(base_address + UART_USR) >> UART_USR_TFNF_POS) & 1U) ==
                1U)
        {
            WR_REG32((base_address + UART_THR), buf[bytes_done]);
            bytes_done++;
            bytes--;
        }
        else
        {
            break;
        }
    }

    return bytes_done;
}

/**
 * @brief Read data from UART receive FIFO
 */
uint16_t uart_read_fifo(uint32_t base_address, uint8_t *const buf,
        uint32_t bytes)
{

    uint16_t bytes_done;
    bytes_done = 0U;

    while (bytes > 0U)
    {
        if (((RD_REG32(base_address + UART_USR) >> UART_USR_RFNE_POS) & 1U) ==
                1U)
        {
            buf[bytes_done] = (uint8_t)RD_REG32(base_address + UART_RBR);
            bytes--;
            bytes_done++;
        }
        else
        {
            break;
        }
    }
    return bytes_done;
}

/**
 * @brief Enable UART interrupts
 */
void uart_enable_interrupt(uint32_t base_address, uart_interrupt_id_t id)
{
    uint32_t val;
    val = RD_REG32(base_address + UART_IER);

    switch (id)
    {
        case INTERRUPT_RX:
            val |= (1U << UART_IER_ERBFI_POS);
            WR_REG32((base_address + UART_IER), val);
            break;

        case INTERRUPT_TX:
            val |= (1U << UART_IER_ETBEI_POS);
            val |= (1U << UART_IER_PTIME_POS);
            WR_REG32((base_address + UART_IER), val);
            break;

        case INTERRUPT_HW:
            val |= (1U << UART_IER_ELSI_POS);
            WR_REG32((base_address + UART_IER), val);
            break;

        default:
            /*do nothing*/
            break;
    }
}

/**
 * @brief Disable UART interrupts
 */
void uart_disable_interrupt(uint32_t base_address, uart_interrupt_id_t id)
{
    uint32_t val;
    val = RD_REG32(base_address + UART_IER);

    switch (id)
    {
        case INTERRUPT_RX:
            val &= ~(1U << UART_IER_ERBFI_POS);
            WR_REG32((base_address + UART_IER), val);
            break;

        case INTERRUPT_TX:
            val &= ~(1U << UART_IER_ETBEI_POS);
            val &= ~(1U << UART_IER_PTIME_POS);
            WR_REG32((base_address + UART_IER), val);
            break;

        case INTERRUPT_HW:
            val &= ~(1U << UART_IER_ELSI_POS);
            WR_REG32((base_address + UART_IER), val);
            break;

        default:
            /*do nothing*/
            break;
    }
}

/**
 * @brief Polling write to UART Tx
 */
void uart_tx_polling(uint32_t base_address, uint8_t *const buf,
        uint32_t nbytes)
{
    for(uint32_t len = 0; len < nbytes; len++)
    {
        while (!(RD_REG32(base_address + UART_LSR) & UART_LSR_THRE_MASK));
        WR_REG32(base_address + UART_THR, buf[len]);
    }
}
