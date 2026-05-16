/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for DWC3 driver
 */

#include "dwc3.h"
#include "dwc3_ll.h"
#include "osal_log.h"
#include "socfpga_defines.h"

int dwc3_init(void)
{
    uint32_t ghwparams0 = RD_REG32(USBBASE + USB3_GHWPARAMS0);
    uint32_t ghwparams1 = RD_REG32(USBBASE + USB3_GHWPARAMS1);
    uint32_t ghwparams3 = RD_REG32(USBBASE + USB3_GHWPARAMS3);
    uint32_t phy_info;
    int ret = 0;

    phy_info = get_usb3_phy_info(ghwparams3);
    switch (phy_info)
    {
        case DWC3_SS_PHY_3_0:
            INFO("Device supportes USB 3.0 Only");
            break;

        case DWC3_SS_PHY_3_1:
            INFO("Device supportes USB 3.1 Gen1");
            break;

        default:
            INFO("Device does not support USB3.1");
            break;
    }

    usb2_phy_setup(ghwparams3);

    setup_dwc3_gctl(ghwparams1);

    ret = dwc3_set_usb_host_mode(ghwparams0);
    if (ret != 0)
    {
        ERROR("Unable to setup DWC3 in host mode");
        return ret;
    }

    return 0;
}
