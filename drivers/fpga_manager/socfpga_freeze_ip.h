/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for freeze bridge HAL driver
 */

#ifndef __SOCFPGA_FREEZE_IP_H__
#define __SOCFPGA_FREEZE_IP_H__

/**
 * @file socfpga_freeze_ip.h
 * @brief SoC FPGA freeze IP HAL driver
 */

/**
 * @defgroup fpga_manager FPGA Manager
 * @ingroup drivers
 *
 * @{
 */

/**
 * @addtogroup fpga_manager_fns
 * @{
 */

/**
 * @brief freeze the pr region using the freeze ip before initiating the partial
 *      reconfiguration
 *
 * @return
 * - 0:         if the freeze operation is successful
 * - -ETIMEDOUT: if the freeze operation gets timed out
 */
int do_freeze_pr_region(void);

/**
 * @brief unfreeze the pr region using the freeze ip after completing the partial
 *      reconfiguration
 *
 * @return
 * - 0:         if the unfreeze operation is successful,
 * - -ETIMEDOUT: if the unfreeze operation gets timed out
 */
int do_unfreeze_pr_region(void);

/**
 * @}
 */
/* end of group fpga_manager_fns */

/**
 * @}
 */
/* end of group fpga_manager */

#endif /* __SOCFPGA_FREEZE_IP_H__ */
