/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for FPGA manager driver
 */
#ifndef __SOCFPGA_FPGA_MANAGER_H__
#define __SOCFPGA_FPGA_MANAGER_H__

/**
 * @file socfpga_fpga_manager.h
 * @brief SoC FPGA fpga manager HAL driver
 */

#include <stdint.h>
#include <errno.h>

/**
 * @defgroup fpga_manager FPGA Manager
 * @ingroup drivers
 * @brief APIs for the SoC FPGA FPGA manager driver
 * @details
 * FPGA manager driver supports both FPGA configuration and partial reconfiguration through HPS.
 *
 * FPGA configuration : Fpga configuration configures the FPGA fabric via the core bitstream
 * via HPS. The HPS sends bitstream to FPGA via SDM.
 *
 * Partial Reconfiguration : PR allows the user to configure only a specific portion of the fpga
 * (referred as PR region ) fabric. The process of PR is same as fpga configuration except that the
 * PR region needs to be frozen before loading the bitstream, and unfreeze the PR region after
 * the bitstream is configured. The freeze/unfreeze operation can be performed using the freeze
 * IP driver. <br>
 * To see example usage, refer @ref fpga_manager_sample
 *
 * @{
 */

/**
 * @defgroup fpga_manager_fns Functions
 * @ingroup fpga_manager
 * FPGA MANAGER HAL APIs
 */

/**
 * @addtogroup fpga_manager_fns
 * @{
 */

/**
 * @brief  Sends the bitstream data to the fpga and configures the fpga.
 *
 * @param[in] rbf_ptr   Reference to the bitstream data
 * @param[in] rbf_file_size Length of bitstream file
 *
 * @return
 * - 0:    if fpga bitstream configuration is success
 * - -EIO: if fpga bitstream configuration fails
 */
int load_fpga_bitstream(uint8_t *rbf_ptr, uint32_t rbf_file_size);

/**
 * @}
 */
/* end of group fpga_manager_fns */

/**
 * @}
 */
/* end of group fpga_manager */

#endif  /* __SOCFPGA_FPGA_MANAGER_H__ */
