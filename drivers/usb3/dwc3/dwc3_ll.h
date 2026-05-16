/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for DWC3 low level driver
 */

#ifndef _SOCFPGA_DWC3_LL_H_
#define _SOCFPGA_DWC3_LL_H_

#include <stdint.h>
#include "socfpga_usb3_reg.h"

#define USB_MODE_HOST                  ((uint32_t) 1) /* !< USB host mode */

#define DWC3_HS_PHY_DIS                (0U) /* !< HS phy is disabled */
#define DWC3_HS_PHY_UTMI               (1U) /* !< HS phy interface is UTMI */
#define DWC3_HS_PHY_ULPI               (2U) /* !< HS phy interface is ULPI */
#define DWC3_HS_PHY_UTMI_ULPI          (3U) /* !< DWC3 supports both UTMI/ULPI HS phy interface */

#define DWC3_SS_PHY_DIS                (0U) /* !< DWC3 SS phy interface is disabled */
#define DWC3_SS_PHY_3_0                (1U) /* !< DWC3 supports only USB3.0 mode */
#define DWC3_SS_PHY_3_1                (2U) /* !< DWC3 supports USB3.1 mode */

#define DWC3_GHWPARAMS0_MODE_GADGET    (0U) /* !< DWC3 Core is in gadget mode */
#define DWC3_GHWPARAMS0_MODE_HOST      (1U) /* !< DWC3 Core is in host mode*/
#define DWC3_GHWPARAMS0_MODE_DRD       (2U) /* !< DWC3 Core is in DRD mode*/


/**
 * @brief Configure the usb2 phy.
 *
 * @param[in] ghwparams3 ghwparams3 register value
 */
void usb2_phy_setup(uint32_t ghwparams3);

/**
 * @brief Get info regarding USB3.1 phy.
 *
 * @param[in] ghwparams3 ghwparams3 register value
 * @return usb3 SS phy info
 */
uint32_t get_usb3_phy_info(uint32_t ghwparams3);

/**
 * @brief Configure the GCTL register.
 *
 * @param[in] ghwparams1 ghwparams1 register value
 */
void setup_dwc3_gctl(uint32_t ghwparams1);

/**
 * @brief Configure the controller in host mode.
 *
 * @param[in] ghwparams0 ghwparams0 register value
 * @return
 *  RET_SUCCESS: if host mode setup is successful
 *  RET_FAIL:    if host mode setup fails
 */
int dwc3_set_usb_host_mode(uint32_t ghwparams0);

#endif /* _SOCFPGA_DWC3_LL_H_ */
