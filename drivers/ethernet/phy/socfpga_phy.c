/*
 * FreeRTOS+TCP V3.1.0
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#include <errno.h>
#include <osal_log.h>

/* Driver includes */
#include "socfpga_phy.h"

static bool phy_set_parameters(uint32_t base_address, eth_phy_config_t *pphy_config);

static bool is_phy_id_valid(uint32_t ulreg_val)
{
    if ((ulreg_val != (uint16_t)~0U) && (ulreg_val != (uint16_t)0U))
    {
        return true;
    }
    return false;
}

int32_t eth_phy_discover(uint32_t base_address, eth_phy_config_t *pphy_config)
{
    uint16_t aul_phy_reg[2] =
    {
        0
    };
    uint32_t phy_address = PHY_MIN_ADDRESS;

    while (phy_address <= PHY_MAX_ADDRESS)
    {
        eth_phy_get_id(base_address, phy_address, &aul_phy_reg[0], &aul_phy_reg[1]);
        if (is_phy_id_valid(aul_phy_reg[0]) && is_phy_id_valid(aul_phy_reg[1]))
        {
            pphy_config->phy_identifier = ((uint32_t)aul_phy_reg[1] << 16) |
                    aul_phy_reg[0];
            pphy_config->phy_address = phy_address;
            INFO("PHY at address %d with PHY Identifier2: 0x%04X and PHY identifier3 : 0x%04X.",
                    pphy_config->phy_address, aul_phy_reg[0], aul_phy_reg[1]);

            INFO("Detected PHY at address %d with ID 0x%08X.", pphy_config->phy_address,
                    pphy_config->phy_identifier);
            return 0;
        }
        phy_address++;
    }
    ERROR("No PHY detected.");
    return -EIO;
}

int32_t eth_phy_initialize(uint32_t base_address, eth_phy_config_t *pphy_config)
{
    bool ret;

    ret = eth_phy_setup(base_address, pphy_config->phy_address);
    if (ret != true)
    {
        ERROR("XGMAC PHY: Setup failed.");
        return -EIO;
    }
    ret = phy_set_parameters(base_address, pphy_config);
    if (ret != true)
    {
        ERROR("XGMAC PHY: Set PHY parameter failed.");
        return -EINVAL;
    }

    ret = eth_phy_get_link_status(base_address, pphy_config);
    if (ret == true)
    {
        INFO("PHY link is up.");
    }
    else
    {
        INFO("PHY link is down.");
        return -EIO;
    }
    return 0;
}

int32_t eth_phy_update_link(uint32_t base_address, eth_phy_config_t *pphy_config)
{
    if (phy_set_parameters(base_address, pphy_config) != true)
    {
        ERROR("Failed to do PHY Re-Configuration");
        return -EIO;
    }
    if (eth_phy_get_link_status(base_address, pphy_config) != true)
    {
        ERROR("PHY link is down.");
        return -EIO;
    }
    return 0;
}

static bool phy_set_parameters(uint32_t base_address, eth_phy_config_t *pphy_config)
{
    bool ret;

    /* Auto-negotiation disabled */
    if (pphy_config->enable_autonegotiation != ETH_ENABLE_AUTONEG)
    {
        if (eth_phy_disable_autoneg(base_address, pphy_config->phy_address) != true)
        {
            ERROR("Failed to disable auto-negotiation.");
            return false;
        }

        if (eth_phy_set_cfg_link(base_address, pphy_config->phy_address,
                pphy_config->speed_mbps, pphy_config->duplex) != true)
        {
            ERROR("Failed to configure link.");
            return false;
        }
        /* Reset the PHY so the new speed/duplex settings take effect. */
        ret = eth_phy_reset(base_address, pphy_config->phy_address);
        if (ret != true)
        {
            ERROR("Failed to reset the PHY to set speed and mode.");
            return false;
        }
        /* Allow link to settle when auto-negotiation is disabled. */
        osal_task_delay(3000);
    }
    /* Auto-negotiation is enabled, advertise features, resolve speed and duplex */
    else
    {
        if (eth_phy_auto_negotiate(base_address, pphy_config->phy_address,
                &pphy_config->speed_mbps, &pphy_config->duplex) != true)
        {
            ERROR("Auto-negotiation failed.");
            return false;
        }
    }
    return true;
}

bool eth_phy_get_link_status(uint32_t base_address, eth_phy_config_t *pphy_config)
{
    uint32_t status;
    status = eth_phy_get_link(base_address, pphy_config->phy_address);

    if (status == true)
    {
        pphy_config->link_status = true;
        return true;
    }
    pphy_config->link_status = false;
    return false;
}
