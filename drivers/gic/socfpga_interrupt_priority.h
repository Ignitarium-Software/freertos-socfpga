/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Interrupt priority configurations for hardware blocks
 */

#ifndef __SOCFPGA_INTERRUPT_PRIORITY_H__
#define __SOCFPGA_INTERRUPT_PRIORITY_H__

/**
 * @file socfpga_interrupt_priority.h
 * @brief Default interrupt priorities for SoC FPGA blocks.
 */

/**
 * @defgroup socfpga_intr_prio Interrupt Priority Defaults
 * @ingroup drivers
 * @brief Priority defaults for SoC FPGA interrupts.
 * @{
 */

#define GIC_INTERRUPT_PRIORITY_GPIO     14
#define GIC_INTERRUPT_PRIORITY_WDOG     14
#define GIC_INTERRUPT_PRIORITY_I2C      14
#define GIC_INTERRUPT_PRIORITY_UART     14
#define GIC_INTERRUPT_PRIORITY_TIMER    14
#define GIC_INTERRUPT_PRIORITY_DMA      14
#define GIC_INTERRUPT_PRIORITY_IOSSM    14
#define GIC_INTERRUPT_PRIORITY_SDMMC    14
#define GIC_INTERRUPT_PRIORITY_SPI      14
#define GIC_INTERRUPT_PRIORITY_SEU      14
#define GIC_INTERRUPT_PRIORITY_USB3     14
#define GIC_INTERRUPT_PRIORITY_QSPI     14
#define GIC_INTERRUPT_PRIORITY_ENET     14
#define GIC_INTERRUPT_PRIORITY_I3C      14
#define GIC_INTERRUPT_PRIORITY_EDAC     14
#define GIC_INTERRUPT_PRIORITY_USB2     14

/** @} */

#endif
