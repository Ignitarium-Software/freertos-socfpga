/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * PHY driver implementation for Marvell 88E1512
 */

#include "socfpga_phy_mdio.h"
#include "socfpga_phy.h"
#include <osal_log.h>

/* PHY registers */
#define PHY_COPPER_CONTROL_REG 0
#define PHY_COPPER_STATUS_REG 1
#define PHY_ID_1_REG 2
#define PHY_ID_2_REG 3
#define PHY_COPPER_AUTO_NEG_ADV_REG 4
#define PHY_COPPER_LINK_PARTNER_ABILITY_REG 5
#define PHY_1G_BASE_T_CONTROL_REG 9
#define PHY_1G_BASE_T_STATUS_REG 10

/*Marvell Vendor Specific*/
#define GENERAL_CONTROL_REG_1 20
#define MAC_SPECIFIC_CONTROL_REG_2 21
#define PAGE_ADDRESS_SELECT_REG 22
#define PHY_ID_MARVELL 0x0DD10141

/* Page numbers */
#define SELECT_PAGE_ZERO 0
#define SELECT_PAGE_EIGHTEEN 0x12
#define SELECT_PAGE_TWO 2

#define COPPER_CONTROL_PHY_RESET_MASK 0x8000U
#define COPPER_LINK_STATUS_MASK 0x0004U
#define COPPER_SPEED_SELECT_1000MBPS_MASK ((~(1 << 13)) | (1 << 6)) /* 0 1 */
#define COPPER_SPEED_SELECT_100MBPS_MASK ((1 << 13) | (~(1 << 6))) /* 1 0 */
#define COPPER_SPEED_SELECT_10MBPS_MASK ((~(1 << 13)) | (~(1 << 6))) /* 0 0 */
#define COPPER_CONTROL_FULLDPLX_MASK 0x0100U
#define COPPER_CONTROL_ISOLATE_MASK 0x0400U
#define COPPER_CONTROL_AUTONEG_ENABLE_MASK 0x1000U
#define COPPER_CONTROL_AUTONEG_RESET_MASK 0x0200U
#define COPPER_STATUS_AUTONEG_COMPLETE_MASK 0x0020U
#define PHY_1000_BASE_T_ALL_DPLX_MASK 0x0C00U
#define PHY_1000_BASE_T_FULL_DPLX_MASK 0x0800U
#define PHY_100_BASE_TX_ALL_DPLX_MASK 0x180U
#define PHY_100_BASE_TX_FULL_DPLX_MASK 0x100U
#define PHY_10_BASE_T_ALL_DPLX_MASK 0x60U
#define PHY_10_BASE_T_FULL_DPLX_MASK 0x40U
#define GENERAL_CONTROL_RGMII_COPPER_SELECT_MASK 0x0007U
#define GENERAL_CONTROL_RESET_MASK 0x8000U

#define PHY_RECONFIG_TIMEOUT    5000

void eth_phy_get_id(uint32_t base_address, uint32_t phy_address,
        uint16_t *phy_id1, uint16_t *phy_id2)
{
    *phy_id1 = PHY_READ(base_address, phy_address, PHY_ID_1_REG);
    *phy_id2 = PHY_READ(base_address, phy_address, PHY_ID_2_REG);
}

/*
 * @brief PHY initialization for Marvell 88E151X PHY.
 */
bool eth_phy_setup(uint32_t base_address, uint32_t phy_address)
{
    uint32_t data;
    uint16_t cont_reg2;
    uint32_t max_count = 0U;
    /* Select page from page address register */
    data = (uint32_t)PHY_WRITE(base_address, phy_address,
                                    PAGE_ADDRESS_SELECT_REG, SELECT_PAGE_EIGHTEEN);

    /*
        * Set Interface mode in General control register and changes
        * to this mode bit should be needed to reset the General PHY
        */
    data =
        (uint32_t)PHY_READ(base_address, phy_address,
                                GENERAL_CONTROL_REG_1);
    data &= ~GENERAL_CONTROL_RGMII_COPPER_SELECT_MASK;
    if (PHY_WRITE(base_address, phy_address, GENERAL_CONTROL_REG_1,
                        (uint16_t)data) != 0)
    {
        return false;
    }

    data &= ~GENERAL_CONTROL_RESET_MASK;
    if (PHY_WRITE(base_address, phy_address, GENERAL_CONTROL_REG_1,
                        (uint16_t)data) != 0)
    {
        return false;
    }

    if (PHY_WRITE(base_address, phy_address, PAGE_ADDRESS_SELECT_REG,
                        SELECT_PAGE_TWO) != 0)
    {
        return false;
    }

    cont_reg2 =
        PHY_READ(base_address, phy_address, MAC_SPECIFIC_CONTROL_REG_2);
    cont_reg2 &= ~((uint16_t)(3U << 4));
    if (PHY_WRITE(base_address, phy_address, MAC_SPECIFIC_CONTROL_REG_2,
                        cont_reg2) != 0)
    {
        return false;
    }

    /* Poll the reset bit to complete. */
    while (((data & GENERAL_CONTROL_RESET_MASK) != 0U) && (max_count <
                                                            MAX_GEN_TIMER_COUNT))
    {
        data = (uint32_t)PHY_READ(base_address, phy_address,
                                        GENERAL_CONTROL_REG_1);
        max_count++;
    }
    if (max_count == MAX_GEN_TIMER_COUNT)
    {
        ERROR("General PHY reset address timed out:%0d.", max_count);
        return false;
    }
    /* Selecting page 0 for remaining operations */
    if (PHY_WRITE(base_address, phy_address, PAGE_ADDRESS_SELECT_REG,
                      SELECT_PAGE_ZERO) != 0)
    {
        return false;
    }
    return true;
}

bool eth_phy_disable_autoneg(uint32_t base_address, uint32_t phy_address)
{
    uint32_t data;
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_COPPER_CONTROL_REG);

    data &= ~COPPER_CONTROL_AUTONEG_ENABLE_MASK;

    if ( PHY_WRITE(base_address, phy_address, PHY_COPPER_CONTROL_REG,
            (uint16_t)data) != 0 )
    {
        return false;
    }

    return true;
}

bool eth_phy_set_cfg_link(uint32_t base_address, uint32_t phy_address,
                uint32_t speed_mbps, uint8_t duplex)
{
    uint32_t data;
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_COPPER_CONTROL_REG);
    /*
     * Select the speed. If auto-negotiation is disabled, set the speed
     * manually in copper control register:
     * bits [6][13] -> 00 -> 10MbPS
     *      [6][13] -> 01 -> 100MbPS
     *      [6][13] -> 10 -> 1000MbPS
     *      [6][13] -> 11 -> Reserved
     */
    if( speed_mbps == ETH_SPEED_1000_MBPS )
    {
        data &= COPPER_SPEED_SELECT_1000MBPS_MASK;
    }
    else if( speed_mbps == ETH_SPEED_100_MBPS )
    {
        data &= COPPER_SPEED_SELECT_100MBPS_MASK;
    }
    else if( speed_mbps == ETH_SPEED_10_MBPS )
    {
        data &= COPPER_SPEED_SELECT_10MBPS_MASK;
    }
    else
    {
        WARN("Unsupported speed configuration selected.");
        return false;
    }
    if (duplex == ETH_FULL_DUPLEX)
    {
        data |= COPPER_CONTROL_FULLDPLX_MASK;
    }

    /* Write Mode and Speed to Copper Control Register */
    if (PHY_WRITE(base_address, phy_address,
            PHY_COPPER_CONTROL_REG, (uint16_t)data) != 0)
    {
        return false;
    }
    return true;
}

bool eth_phy_reset(uint32_t base_address, uint32_t phy_address)
{
    uint32_t data;
    uint8_t cnt = 0;
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_COPPER_CONTROL_REG);

    data |= COPPER_CONTROL_PHY_RESET_MASK;
    if (PHY_WRITE(base_address, phy_address,
            PHY_COPPER_CONTROL_REG, (uint16_t)data) != 0)
    {
        return false;
    }

    /* Wait for PHY reset to complete */
    do
    {
        data = (uint32_t)PHY_READ(base_address, phy_address,
                PHY_COPPER_CONTROL_REG);

        cnt++;
        if (cnt >= MAX_GEN_TIMER_COUNT)
        {
            ERROR("PHY reset timeout reached.");
            return false;
        }
    /* Check if the reset bit is still set */
    } while ((data & COPPER_CONTROL_PHY_RESET_MASK) != 0U);

    return true;
}

bool eth_phy_get_link(uint32_t base_address, uint32_t phy_address)
{
    uint32_t data;
    /* Link status bit is latching low, read back to back to clear latch */
    (void)(uint32_t)PHY_READ(base_address, phy_address, PHY_COPPER_STATUS_REG);
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_COPPER_STATUS_REG);

    if ((data & COPPER_LINK_STATUS_MASK) != 0U)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool eth_phy_auto_negotiate(uint32_t base_address, uint32_t phy_address,
        uint32_t *speed_mbps, uint8_t *duplex)
{
    uint32_t data = 0U;
    uint32_t task_timeout = 0U;
    uint32_t adv = 0U;

    /*Enable 100/10 Full/Half duplex Advertisement*/
    adv = AUTONEG_ADV_100_10_TX_ALLDPLX_MASK;
    if (PHY_WRITE(base_address, phy_address,
            PHY_COPPER_AUTO_NEG_ADV_REG, (uint16_t)adv) != 0)
    {
        return false;
    }

    /* Enable 1G Full/Half duplex Advertisement */
    adv = AUTONEG_ADV_1GBASE_TX_ALLDPLX_MASK;
    if (PHY_WRITE(base_address, phy_address,
            PHY_1G_BASE_T_CONTROL_REG, (uint16_t)adv) != 0)
    {
        return false;
    }

    /* Restart Auto-Negotiation */
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_COPPER_CONTROL_REG);

    data |= (COPPER_CONTROL_AUTONEG_ENABLE_MASK |
            COPPER_CONTROL_AUTONEG_RESET_MASK);
    data &= ~COPPER_CONTROL_ISOLATE_MASK;

    if (PHY_WRITE(base_address, phy_address, PHY_COPPER_CONTROL_REG,
            (uint16_t)data) != 0)
    {
        return false;
    }
    /* Resetting the PHY to effect the changes made for speed and mode */
    if (eth_phy_reset(base_address, phy_address) != true)
    {
        ERROR("Failed to reset the PHY to reset the auto-negotiation in CCR.");
    }
    do
    {
        data = (uint32_t)PHY_READ(base_address, phy_address,
                PHY_COPPER_STATUS_REG);
        if (task_timeout >= PHY_RECONFIG_TIMEOUT)
        {
            return false;
        }
        task_timeout++;
    } while ((data & COPPER_STATUS_AUTONEG_COMPLETE_MASK) == 0U);

    INFO("Auto negotiation process completed.");

    /* Get the negotiated speed and duplex */
    *speed_mbps = 0U;
    *duplex = 0U;

    /* Check 1Gbps link */
    data = PHY_READ(base_address, phy_address, PHY_1G_BASE_T_STATUS_REG);
    if ((data & PHY_1000_BASE_T_ALL_DPLX_MASK) != 0)
    {
        if( (data & PHY_1000_BASE_T_FULL_DPLX_MASK) != 0)
        {
            *duplex = ETH_FULL_DUPLEX;
        }
        else
        {
            *duplex = ETH_HALF_DUPLEX;
        }
        *speed_mbps = ETH_SPEED_1000_MBPS;
    }
    else
    {
        data = PHY_READ(base_address, phy_address,
            PHY_COPPER_LINK_PARTNER_ABILITY_REG);
        if( (data & PHY_100_BASE_TX_ALL_DPLX_MASK) != 0)
        {
            if( (data & PHY_100_BASE_TX_FULL_DPLX_MASK) != 0)
            {
                *duplex = ETH_FULL_DUPLEX;
            }
            else
            {
                *duplex = ETH_HALF_DUPLEX;
            }
            *speed_mbps = ETH_SPEED_100_MBPS;
        }
        else if( (data & PHY_10_BASE_T_ALL_DPLX_MASK) != 0)
        {
            if( (data & PHY_10_BASE_T_FULL_DPLX_MASK) != 0)
            {
                *duplex = ETH_FULL_DUPLEX;
            }
            else
            {
                *duplex = ETH_HALF_DUPLEX;
            }
            *speed_mbps = ETH_SPEED_10_MBPS;
        }
    }
    if( *speed_mbps == 0U || *duplex == 0U )
    {
        ERROR("Speed or duplex is not resolved");
        return false;
    }
    return true;
}
