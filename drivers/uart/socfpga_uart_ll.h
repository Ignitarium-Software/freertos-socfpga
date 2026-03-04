/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for UART low level driver
 */

#ifndef __SOCFPGA_UART_LL_H__
#define __SOCFPGA_UART_LL_H__

#include <stdint.h>

#include "socfpga_uart.h"

#define UART_MAX_INSTANCE    2U

#define MODEM_STATUS      0x0U
#define NO_INTERRUPT      0x1U
#define THR_EMPTY         0x2U
#define RX_DATA_RDY       0x4U
#define RX_LINE_STATUS    0x6U
#define BUSY_DETECT       0x7U
#define CHAR_TIMEOUT      0xCU

#define UART_NO_INT             0U
#define UART_TXBUF_EMPTY_INT    0x1U
#define UART_RXBUF_RDY_INT      0x2U
#define UART_HW_ERR_INT         0x3U

typedef enum
{
    INTERRUPT_RX, INTERRUPT_TX, INTERRUPT_HW
} uart_interrupt_id_t;

void uart_init(uint32_t instance);
void uart_deinit(uint32_t instance);

void uart_tx_polling(uint32_t base_address, uint8_t *const buffer,
        uint32_t nbytes);
uint16_t uart_write_fifo(uint32_t base_address, uint8_t *const buffer,
        uint32_t bytes);
uint16_t uart_read_fifo(uint32_t base_address, uint8_t *const buffer,
        uint32_t bytes);


void uart_enable_interrupt(uint32_t base_address, uart_interrupt_id_t id);
void uart_disable_interrupt(uint32_t base_address, uart_interrupt_id_t id);

uint32_t get_int_status(uint32_t base_address);

void uart_config_fifo(uint32_t base_address);

void uart_set_config(uint32_t base_address, uart_partity_t parity,
        uart_stop_bits_t stopbit, uint32_t wordlen);

uint32_t uart_set_baud(uint32_t base_address, uint32_t baud);


void uart_get_config(uint32_t base_address, uint32_t *baud,
        uart_partity_t *parity, uart_stop_bits_t *stopbit,
        uint32_t *wordlen);
uint32_t uart_get_baud(uint32_t base_address);

#endif /* __SOCFPGA_UART_LL_H__ */
