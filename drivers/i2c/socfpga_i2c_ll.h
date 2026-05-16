/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for i2c ll driver
 */

#ifndef __SOCFPGA_I2C_LL_H__
#define __SOCFPGA_I2C_LL_H__

#include <stdint.h>
#include <stdbool.h>

/*Max number of i2c instances*/
#define MAX_I2C_INSTANCES   5U

#define I2C_NO_INT          0U           /*!< No interrupt */
#define I2C_TX_EMPTY_INT    1U           /*!< Tx FIFO Empty interrupt */
#define I2C_RX_FULL_INT     2U           /*!< Rx FIFO Full interrupt */
#define I2C_TX_ABORT_INT    4U           /*!< Tx Abort interrupt */
#define I2C_STOP_DET_INT    8U           /*!< Stop detected interrupt */
#define I2C_RD_REQ_INT      16U          /*!< Read request interrupt (slave mode) */
#define I2C_RX_OVER_INT     32U          /*!< Rx FIFO overflow interrupt */

#define I2C_FIFO_DEPTH      64U         /*!< I2C TX/RX FIFO depth (entries) */

void i2c_enable_interrupt(uint32_t base_addr, uint32_t interrupt_req);

void i2c_disable_interrupt(uint32_t base_addr, uint32_t interrupt_req);

uint32_t i2c_get_interrupt_status(uint32_t base_addr);

uint32_t i2c_read_status(uint32_t base_addr);

void i2c_clear_interrupt(uint32_t base_addr);

void i2c_set_target_addr(uint32_t base_addr, uint32_t slave_addr);

void i2c_deinit(uint32_t base_addr);

void i2c_ll_disable_controller(uint32_t base_addr);

void i2c_ll_enable_controller(uint32_t base_addr);

void i2c_ll_set_interrupt_mask(uint32_t base_addr, uint32_t mask);

uint16_t i2c_write_fifo(uint32_t base_addr, uint8_t *const buffer,
        uint32_t bytes, bool no_stop_flag);

uint16_t i2c_enq_read_cmd(uint32_t base_addr, uint32_t bytes,
        bool no_stop_flag);

uint16_t i2c_read_fifo(uint32_t base_addr, uint8_t *const buffer,
        uint32_t bytes);

uint32_t i2c_config_master(uint32_t base_addr, uint32_t speed);

void i2c_init(uint32_t base_addr);

void i2c_ll_cancel(uint32_t base_addr);

uint32_t i2c_get_config(uint32_t base_addr);

void i2c_ll_tdma_enable(uint32_t base_addr);

void i2c_ll_rdma_enable(uint32_t base_addr);

void i2c_ll_tdma_disable(uint32_t base_addr);

void i2c_ll_rdma_disable(uint32_t base_addr);

void i2c_ll_set_tdlr(uint32_t base_addr, uint32_t burst_size);

void i2c_ll_set_rdlr(uint32_t base_addr, uint32_t burst_size);

uint32_t i2c_ll_get_raw_interrupt_status(uint32_t base_addr);

uint32_t i2c_ll_get_tx_abrt_source(uint32_t base_addr);

void i2c_ll_clear_tx_abrt(uint32_t base_addr);

void i2c_ll_clear_rd_req(uint32_t base_addr);

void i2c_ll_clear_stop_det(uint32_t base_addr);

void i2c_ll_clear_rx_over(uint32_t base_addr);

void i2c_ll_config_slave(uint32_t base_addr, uint16_t slave_addr,
        bool is_10bit_addr, bool stop_det_ifaddressed,
        bool ack_general_call, uint8_t rx_tl, uint8_t tx_tl);

#endif   /* ifndef __SOCFPGA_I2C_LL_H__ */
