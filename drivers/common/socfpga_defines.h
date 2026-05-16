/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for common HAL register access helpers
 */

#ifndef __SOCFPGA_DEFINES_H__
#define __SOCFPGA_DEFINES_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "osal.h"

#define WR_REG16(address, val)    io_write16(address, val)
#define RD_REG16(address)           io_read16(address)

#define WR_REG32(address, val)    io_write32(address, val)
#define RD_REG32(address)           io_read32(address)

#define WR_REG64(address, val)    io_write64(address, val)
#define RD_REG64(address)           io_read64(address)

static inline void io_write16(uint32_t address, uint32_t value) {
    volatile uint16_t *paddress = (volatile uint16_t *)((uint64_t)address);
    *paddress = value;
}

static inline uint32_t io_read16(uint32_t address) {
    volatile uint16_t *paddress = (volatile uint16_t *)((uint64_t)address);
    return *paddress;
}

static inline void io_write32(uint32_t address, uint32_t value) {
    volatile uint32_t *paddress = (volatile uint32_t *)((uint64_t)address);
    *paddress = value;
}

static inline uint32_t io_read32(uint32_t address) {
    volatile uint32_t *paddress = (volatile uint32_t *)((uint64_t)address);
    return *paddress;
}

static inline void io_write64(uint32_t address, uint64_t value) {
    volatile uint64_t *paddress =
            (volatile uint64_t *)((uint64_t)address);
    *paddress = value;
}

static inline uint64_t io_read64(uint32_t address) {
    volatile uint64_t *paddress =
            (volatile uint64_t *)((uint64_t)address);
    return *paddress;
}

#endif //__SOCFPGA_DEFINES_H__
