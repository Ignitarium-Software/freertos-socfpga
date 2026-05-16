/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SMC calls
 */

#ifndef __SOCFPGA_SIP_HANDLER_H__
#define __SOCFPGA_SIP_HANDLER_H__

/**
 * @defgroup smc_call SMC Call
 * @ingroup drivers
 * @brief API to perform SMC calls
 * @details This is the implementation of SMC call API for SoC FPGA.
 * It loads values into CPU registers and performs a SMC call.
 * @{
 */

/**
 * @defgroup smc_call_fns Functions
 * @ingroup smc_call
 * SDM Mailbox HAL APIs
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @addtogroup smc_call_fns
 * @{
 */

/**
 * @brief   Perform SMC call
 *
 * smc_call is used to load values into CPU registers and perform a SMC call.
 * The value in the x0 register after the SMC call is used as the return value
 *
 * @param[in] function_id           Id of the SIP call
 * @param[in, out] register_val     An array with each element corresponding to a
 *                                   register value. Once the SMC call is performed
 *                                  the new values in the registers are stored back
 *                                  into the array
 * @return
 *   -The value of the x0 register after the SMC call
 */
int smc_call(uint64_t function_id, uint64_t *register_val);

/**
 * @}
 */
/* end of group smc_call_fns */

/**
 * @}
 */
#endif /*_SOCFPGA_SIP_HANDLER_H_*/
