/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for GIC low level functions
 */


#ifndef __ARM_GIC_REG_H__
#define __ARM_GIC_REG_H__

void gic_reg_enable_group0_interrupts(void);
void gic_reg_enable_group1_interrupts(void);
void gic_reg_write_group1_end_of_interrupt(uint32_t interrupt_id);
void gic_reg_set_priority_mask(uint32_t interrupt_id);
uint64_t gic_reg_get_cpu_affinity(void);

#endif /* __ARM_GIC_REG_H__ */
