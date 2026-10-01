/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for GIC low level functions
 */


#ifndef __ARM_GIC_REG_H__
#define __ARM_GIC_REG_H__

/**
 * @file arm_gic_reg.h
 * @brief GIC register-level helper APIs.
 */

#include <stdint.h>

/**
 * @defgroup gic_reg GIC Register Access
 * @ingroup drivers
 * @brief Low-level helpers for GIC system registers.
 * @details These APIs wrap GIC system register access for enabling
 * interrupts, setting masks, and retrieving CPU affinity.
 * @{
 */

/**
 * @brief Enable Group 0 interrupts.
 */
void gic_enable_group0_interrupts(void);

/**
 * @brief Enable Group 1 interrupts.
 */
void gic_enable_group1_interrupts(void);

/**
 * @brief Signal end of interrupt for Group 1.
 *
 * @param[in] interrupt_id Interrupt ID to acknowledge.
 */
void gic_end_group1_interrupt(uint32_t interrupt_id);

/**
 * @brief Set the priority mask.
 *
 * @param[in] mask Priority mask value.
 */
void gic_set_priority_mask(uint32_t mask);

/**
 * @brief Get the CPU affinity value.
 *
 * @return CPU affinity value.
 */
uint64_t gic_get_cpu_affinity(void);

/** @} */

#endif /* __ARM_GIC_REG_H__ */
