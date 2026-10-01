/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * PHY driver implementation for Microchip KSZ9031RNX
 */

#include "socfpga_phy_mdio.h"
#include "socfpga_phy.h"
#include <osal_log.h>

/* PHY registers */
#define PHY_BASIC_CONTROL_REG 0
#define PHY_BASIC_STATUS_REG 1
#define PHY_ID_1_REG 2
#define PHY_ID_2_REG 3
#define PHY_AUTO_NEG_ADV_REG 4
#define PHY_AUTO_NEG_LINK_PARTNER_REG 5
#define PHY_1G_BASE_T_CONTROL_REG 9
#define PHY_1G_BASE_T_STATUS_REG 10

/* MMD registers */
#define PHY_MMD_ADDRESS 0x2U
#define PHY_RGMII_CLK_PAD_SKEW_REG 0x8U

/* Microchip vendor specific*/
#define MMD_ACCESS_CONTROL_REG 13
#define MMD_ACCESS_DATA_REG 14
#define PHY_CONTROL_REG 31
#define PHY_ID_MICROCHIP 0x16220022

/* Values tuned for Microchip KSZ9031RNX PHY */
#define MICROCHIP_GTX_CLK_SKEW 18U
#define MICROCHIP_RX_CLK_SKEW 13U
#define CLK_SKEW_MASK       (0x1FU)
#define CLK_SKEW_BIT_COUNT  (5U)

/* Masks */
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

#define PHY_RECONFIG_TIMEOUT    5000

/*
 * @brief Perform a register read on MMD registers
 */
static uint16_t phy_read_reg_mmd(uint32_t base_address, uint32_t phy_address, uint8_t mmd_addr, uint8_t mmd_reg)
{
    uint16_t val;
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_CONTROL_REG, mmd_addr);
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_DATA_REG, mmd_reg);
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_CONTROL_REG, 0x4000 | mmd_addr);
    val = PHY_READ(base_address, phy_address, MMD_ACCESS_DATA_REG);
    return val;
}

/*
 * @brief Perform a register write on MMD registers
 */
static void phy_write_reg_mmd(uint32_t base_address, uint32_t phy_address, uint8_t mmd_addr, uint8_t mmd_reg, uint16_t reg_val)
{
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_CONTROL_REG, mmd_addr);
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_DATA_REG, mmd_reg);
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_CONTROL_REG, 0x4000 | mmd_addr);
    (void)PHY_WRITE(base_address, phy_address, MMD_ACCESS_DATA_REG, reg_val);
}

void eth_phy_get_id(uint32_t base_address, uint32_t phy_address,
        uint16_t *phy_id1, uint16_t *phy_id2)
{
    *phy_id1 = PHY_READ(base_address, phy_address, PHY_ID_1_REG);
    *phy_id2 = PHY_READ(base_address, phy_address, PHY_ID_2_REG);
}

/*
 * @brief PHY initialization for Microchip KSZ9031RNX PHY.
 */
bool eth_phy_setup(uint32_t base_address, uint32_t phy_address)
{
    /*
     * By default the microchip KSZ9031RNX phy has incorrect clock skew which causes
     * high packet loss in 1Gbps where clock delays have tight
     * tolerances. This function is to offset the clock delays for
     * optimal functioning
     */

    uint32_t curr_delay = phy_read_reg_mmd(base_address, phy_address,
            PHY_MMD_ADDRESS, PHY_RGMII_CLK_PAD_SKEW_REG);
    uint16_t tx_clk = (curr_delay >> CLK_SKEW_BIT_COUNT) & CLK_SKEW_MASK;
    uint16_t rx_clk = (curr_delay & CLK_SKEW_MASK);

    tx_clk = MICROCHIP_GTX_CLK_SKEW;
    rx_clk = MICROCHIP_RX_CLK_SKEW;

    tx_clk = tx_clk << CLK_SKEW_BIT_COUNT;
    /* Replacing default values with updated clock pad skews */
    curr_delay = curr_delay & ~(CLK_SKEW_MASK << CLK_SKEW_BIT_COUNT);
    curr_delay = curr_delay & ~(CLK_SKEW_MASK);
    curr_delay = curr_delay | (tx_clk) | (rx_clk);
    phy_write_reg_mmd(base_address, phy_address, PHY_MMD_ADDRESS,
            PHY_RGMII_CLK_PAD_SKEW_REG, (uint16_t)curr_delay);

    return true;
}

bool eth_phy_set_cfg_link(uint32_t base_address, uint32_t phy_address,
                uint32_t speed_mbps, uint8_t duplex)
{
    uint32_t data;
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_BASIC_CONTROL_REG);
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
            PHY_BASIC_CONTROL_REG, (uint16_t)data) != 0)
    {
        return false;
    }
    return true;
}

bool eth_phy_disable_autoneg(uint32_t base_address, uint32_t phy_address)
{
    uint32_t data;
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_BASIC_CONTROL_REG);

    data &= ~COPPER_CONTROL_AUTONEG_ENABLE_MASK;

    if ( PHY_WRITE(base_address, phy_address, PHY_BASIC_CONTROL_REG,
            (uint16_t)data) != 0 )
    {
        return false;
    }

    return true;
}

bool eth_phy_reset(uint32_t base_address, uint32_t phy_address)
{
    uint32_t data;
    uint8_t cnt = 0;
    data = (uint32_t)PHY_READ(base_address, phy_address, PHY_BASIC_CONTROL_REG);

    data |= COPPER_CONTROL_PHY_RESET_MASK;
    if (PHY_WRITE(base_address, phy_address,
            PHY_BASIC_CONTROL_REG,
                      (uint16_t)data) != 0)
    {
        return false;
    }

    /* Wait for PHY reset to complete */
    do
    {
        data = (uint32_t)PHY_READ(base_address, phy_address,
                PHY_BASIC_CONTROL_REG);

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
    (void)(uint32_t)PHY_READ(base_address, phy_address, PHY_BASIC_STATUS_REG);
    data = (uint32_t)PHY_READ(base_address, phy_address,
            PHY_BASIC_STATUS_REG);

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
            PHY_AUTO_NEG_ADV_REG, (uint16_t)adv) != 0)
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
            PHY_BASIC_CONTROL_REG);

    data |= (COPPER_CONTROL_AUTONEG_ENABLE_MASK |
            COPPER_CONTROL_AUTONEG_RESET_MASK);
    data &= ~COPPER_CONTROL_ISOLATE_MASK;

    if (PHY_WRITE(base_address, phy_address, PHY_BASIC_CONTROL_REG,
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
                PHY_BASIC_STATUS_REG);
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
        data = PHY_READ(base_address, phy_address, PHY_AUTO_NEG_LINK_PARTNER_REG);
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
