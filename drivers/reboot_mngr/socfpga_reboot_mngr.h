/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for reboot manager HAL driver
 */


#ifndef __SOCFPGA_REBOOT_MNGR_H__
#define __SOCFPGA_REBOOT_MNGR_H__

/**
 * @file socfpga_reboot_mngr.h
 * @brief This file contains all the Reboot Manager HAL API definitions
 *
 */

#include <errno.h>

/**
 * @defgroup rbt Reboot Manager
 * @ingroup drivers
 * @brief APIs for SoC FPGA Reboot Manager driver.
 * @details This is the Reboot Manager driver implementation for SoC FPGA.
 * It provides APIs for initiating warm and cold reboots. For example usage,
 * refer @ref rbt_sample "reboot manager sample application".
 * @{
 */

/**
 * @defgroup rbt_fns Functions
 * @ingroup rbt
 * Reboot Manager HAL APIs
 */

/**
 * @defgroup rbt_macros Macros
 * @ingroup rbt
 * Reboot Manager Specific Macros
 */

/**
 * @addtogroup rbt_macros
 * @{
 */
#define REBOOT_WARM    1U        /*!<Command to initiate warm reboot*/
#define REBOOT_COLD    0U        /*!<Command to initiate cold reboot*/
/**
 * @}
 */

/**
 * @addtogroup rbt_fns
 * @{
 */
/**
 * @brief Callback function to be invoked before reboot
 *
 * @param[in] param Pointer to user param that can be passed to the callback
 */
typedef void (*reboot_callback_t)(void *param);



/**
 * @brief Set the callback function
 *
 * Sets the callback function that is invoked before initiating cold
 * /warm reboot.
 *
 * @param[in] callback Call back function pointer
 * @param[in] event    Callback for warm/cold reboot.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if any argument is invalid
 */
int32_t reboot_mngr_set_callback(reboot_callback_t callback, uint32_t event);

/**
 * @brief Initiate a warm or cold reboot.
 *
 * @param[in] event Callback for warm/cold reboot.
 *
 * @return
 * - 0: on success
 * - -ENOTSUP: if operation is not supported
 * - -EINVAL:  if invalid argument is passed
 */
int32_t reboot_mngr_do_reboot(uint32_t event);
/**
 * @}
 */


/**
 * @}
 */
#endif
