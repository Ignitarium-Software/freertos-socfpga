/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL Implementation for SMC calls
 */


#include "socfpga_sip_handler.h"
#include "socfpga_console.h"

int smc_call(uint64_t fn_id, uint64_t *reg_val)
{
    int ret = 0;

    /*
     * Console writes can interleave with secure firmware UART output or
     * transactions while one core is in EL3, producing corrupted logs.
     * If the console semaphore was never created, locking is a no-op.
     */
    (void)console_lock();

    asm volatile (
        "mov x0, %1\n"
        "ldr x1, [%0]\n"
        "ldr x2, [%0, #8]\n"
        "ldr x3, [%0, #16]\n"
        "ldr x4, [%0, #24]\n"
        "ldr x5, [%0, #32]\n"
        "ldr x6, [%0, #40]\n"
        "ldr x7, [%0, #48]\n"
        "ldr x8, [%0, #56]\n"
        "ldr x9, [%0, #64]\n"
        "ldr x10, [%0, #72]\n"
        "ldr x11, [%0, #80]\n"
        "smc     #0\n"
        :
        : "r" (reg_val), "r" (fn_id)
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
        "x9", "x10", "x11");

    /* Return the secure monitor response register values to caller. */
    asm volatile (
        "str x0, %0\n"
        "str x1, [%1]\n"
        "str x2, [%1, #8]\n"
        "str x3, [%1, #16]\n"
        "str x4, [%1, #24]\n"
        "str x5, [%1, #32]\n"
        "str x6, [%1, #40]\n"
        "str x7, [%1, #48]\n"
        "str x8, [%1, #56]\n"
        "str x9, [%1, #64]\n"
        "str x10, [%1, #72]\n"
        "str x11, [%1, #80]\n"
        : "=m" (ret)
        : "r" (reg_val)
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
        "x9", "x10", "x11");

    (void)console_unlock();

    return ret;
}
