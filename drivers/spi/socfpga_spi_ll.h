/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SPI low level driver
 */

#ifndef __SOCFPGA_SPI_LL_H__
#define __SOCFPGA_SPI_LL_H__

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "socfpga_spi.h"

#include "socfpga_spi_reg.h"

#define SPI_GET_INT_ID_MASTER(instance) (((instance) == 1U) ? SPI1IRQ : SPI0IRQ)
#define SPI_GET_INT_ID_SLAVE(instance)  (((instance) == 1U) ? SPI3IRQ : SPI2IRQ)

#define SPI_TX_RX_MOD    0x00U
#define SPI_TX_MOD       0x01U
#define SPI_RX_MOD       0x02U

#define SPI_TX_FIFO_THRESHOLD    127U
#define SPI_RX_FIFO_THRESHOLD    0U

/* ISR/IMR bitmask values */
#define SPI_NO_INTERRUPT      0x00U
#define SPI_TX_EMPTY_INT      0x01U  /* TXEIS / TXEIM bit 0 */
#define SPI_RX_OVERFLOW_INT   0x08U  /* RXOIS / RXOIM bit 3 */
#define SPI_RX_FULL_INT       0x10U  /* RXFIS / RXFIM bit 4 */
#define SPI_ALL_INTERRUPTS    0x3FU  /* all six IMR bits */

void spi_init(uint32_t instance, spi_role_t role);
void spi_deinit(uint32_t instance, spi_role_t role);
uint32_t spi_get_base_addr(uint32_t instance, spi_role_t role);
void spi_set_slave_output(uint32_t base_addr, bool enable);

void spi_select_chip(uint32_t instance, uint32_t slave);
/**
 * @brief Set SPI controller configuration.
 *
 * @param base_addr SPI base address.
 * @param freq         Requested SCLK frequency in Hz.
 * @param mode         SPI mode (CPOL/CPHA).
 *
 * @return 0 on success, negative errno on failure.
 */
int32_t spi_set_config(uint32_t base_addr, uint32_t freq, spi_mode_t mode);
void spi_get_config(uint32_t base_addr, uint32_t *freq, spi_mode_t *mode);
void spi_set_transfermode(uint32_t base_addr, uint32_t mode);

void spi_enable(uint32_t base_addr);
void spi_disable(uint32_t base_addr);

uint32_t spi_get_freq(uint32_t base_addr);

uint16_t spi_write_fifo(uint32_t base_addr, uint8_t *buf, uint16_t bytes);
uint16_t spi_read_fifo(uint32_t base_addr, uint8_t *buf, uint16_t bytes);

uint32_t spi_get_interrupt_status(uint32_t base_addr);
void spi_enable_interrupt(uint32_t base_addr, uint32_t ir_id);
void spi_disable_interrupt(uint32_t base_addr, uint32_t ir_id);
void spi_clear_rx_overflow(uint32_t base_addr);
void spi_ll_set_rx_threshold(uint32_t base_addr, uint8_t rx_thr);

void spi_ll_enable_dma(uint32_t base_addr, bool tx_enable, bool rx_enable);
void spi_ll_disable_dma(uint32_t base_addr);
void spi_ll_set_dma_thresholds(uint32_t base_addr, uint8_t tx_level,
        uint8_t rx_level);

#endif /* __SOCFPGA_SPI_LL_H__ */
