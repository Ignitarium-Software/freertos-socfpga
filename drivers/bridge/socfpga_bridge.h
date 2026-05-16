/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for fpga bridge HAL driver
 */

#ifndef __SOCFPGA_BRIDGE_H__
#define __SOCFPGA_BRIDGE_H__

/**
 * @file socfpga_bridge.h
 * @brief SoC FPGA Bridge HAL driver
 */

#include <stdint.h>
#include <errno.h>

/**
 * @defgroup bridge Bridge
 * @ingroup drivers
 * @brief APIs for the SoC FPGA Bridge driver
 * @details
 * Bridges are used to move data between the FPGA fabric and HPS. It consists of the following four
 * bridges :-
 * - FPGA2HPS
 * - FPGA2SDRAM
 * - HPS2FPGA
 * - LWHPS2FPGA
 *
 * To see example usage, refer @ref bridge_sample
 *
 * @{
 */

/**
 * @defgroup bridge_fns Functions
 * @ingroup bridge
 * Bridge HAL APIs
 */

/**
 * @addtogroup bridge_fns
 * @{
 */

/**
 * @brief   enables the hps2fpga bridge
 *          The application should call this api to enable the HPS2FPGA bridge
 *          before any bridge operation is performed.
 *
 * @return
 * - 0:     if bridge enable operation is successful
 * - -EIO:  if bridge enable operation fails or any timeout occurs
 */
int32_t enable_hps2fpga_bridge(void);

/**
 * @brief  disable the hps2fpga bridge
 *         The api disables the HPS2FPGA bridge.
 *
 * @return
 * - 0:     if bridge is disabled successfully
 * - -EIO:  if bridge disable operation is unsuccessful
 */
int32_t disable_hps2fpga_bridge(void);

/**
 * @brief  The api enabled the lwhps2fpga bridge.
 *
 * @return
 * - 0:    if lwhps2fpga bridge is enabled successfully
 * - -EIO: if bridge enable operation is unsuccessful
 */
int32_t enable_lwhps2fpga_bridge(void);

/**
 * @brief  The api disables the lwhps2fpga bridge
 *
 * @return
 * - 0:    if lwhps2fpga bridge is disabled successfully
 * - -EIO: if bridge enable operation is unsuccessful
 */
int32_t disable_lwhps2fpga_bridge(void);

/**
 * @brief  This api enables the fpga2hps bridge
 *
 * @return
 * -  0:    if bridge is enabled successfully
 * - -EIO:  if bridge enable operation is  unsuccessful
 */
int32_t enable_fpga2hps_bridge(void);

/**
 * @brief  The api disables the fpga2hps bridge
 *
 * @return
 * - 0:     if bridge disable operation is successful
 * - -EIO:  if bridge disable operation is unsuccessful
 */
int32_t disable_fpga2hps_bridge(void);

/**
 * @brief  The api enables the fpga2sdram bridge
 *
 * @return
 * - 0:    if bridge is enabled successful
 * - -EIO: if bridge enable operation is unsuccessful
 */
int32_t enable_fpga2sdram_bridge(void);

/**
 * @brief  The api disables the fpga2sdram bridge
 * @return
 * -  0:     if bridge disable operation is successful
 * - -EIO:  if bridge disable operation is unsuccessful
 */
int32_t disable_fpga2sdram_bridge(void);
/**
 * @}
 */
/**
 * @}
 */
/* end of group bridge  */

#endif  /*__SOCFPGA_BRIDGE_H__ */
