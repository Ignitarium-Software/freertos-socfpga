/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for PHY HAL driver
 */

#ifndef __SOCFPGA_PHY_H__
#define __SOCFPGA_PHY_H__

#include <stdbool.h>
#include "socfpga_xgmac.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* Modes and speed */
#define ETH_FULL_DUPLEX 1U
#define ETH_HALF_DUPLEX 2U
#define ETH_SPEED_1000_MBPS 1000U
#define ETH_SPEED_100_MBPS 100U
#define ETH_SPEED_10_MBPS 10U
#define ETH_PHY_IF_RGMII 1U
#define ETH_PHY_IF_SGMII 2U
#define ETH_ENABLE_AUTONEG 1U
#define ETH_DISABLE_AUTONEG 0U
#define ETH_ADVERTISE_ALL 1U
#define ETH_ADVERTISE_ALL_TX_FULLDPLX 2U
#define ETH_ADVERTISE_ALL_TX_HALFDPLX 4U
#define BIT(nr) (1UL << (nr))

#define PHY_MIN_ADDRESS 0U
#define PHY_MAX_ADDRESS 31U

#define MAX_GEN_TIMER_COUNT 100U

/* Define Mask fields */
#define AUTONEG_ADVERTISE_1GBASE_TX_FULLDPLX_MASK 0x0200U
#define AUTONEG_ADVERTISE_1GBASE_TX_HALFDPLX_MASK 0x0100U

#define AUTONEG_ADVERTISE_100BASE_TX_FULLDPLX_MASK 0x0100U
#define AUTONEG_ADVERTISE_100BASE_TX_HALFDPLX_MASK 0x0080U
#define AUTONEG_ADVERTISE_10BASE_TX_FULLDPLX_MASK 0x0040U
#define AUTONEG_ADVERTISE_10BASE_TX_HALFDPLX_MASK 0x0020U

#define AUTONEG_ADV_1GBASE_TX_ALLDPLX_MASK 0x0300U
#define AUTONEG_ADV_100_10_TX_ALLDPLX_MASK 0x01E0U

#define AUTO_ADV_100_10_TX_FULLDPX_MASK 0x0140U
#define AUTONEG_ADV_100_10_TX_HALFDPLX_MASK 0x00A0U

#define REAL_TIME_LINK_STATUS 0U

#define ADVERTISE_1GBASE_TX_ALLDPLX 22U
#define ADVERTISE_1GBASE_TX_FULLDPLX 20U
#define ADVERTISE_1GBASE_TX_HALFDPLX 18U

#define ADVERTISE_100_10_BASE_TX_ALLDPLX 16U
#define ADVERTISE_100_10_BASE_TX_FULLDPLX 14U
#define ADVERTISE_100_10_BASE_TX_HALFDPLX 12U

#define ADVERTISE_100BASE_TX_ALLDPLX 10U
#define ADVERTISE_100BASE_TX_FULLDPLX 8U
#define ADVERTISE_100BASE_TX_HALFDPLX 6U

#define ADVERTISE_10BASE_TX_ALLDPLX 4U
#define ADVERTISE_10BASE_TX_FULLDPLX 2U
#define ADVERTISE_10BASE_TX_HALFDPLX 1U

/*
 * @brief  Define the configuration structure for SoC FPGA PHY parameters.
 */
typedef struct eth_phy_config_t
{
    uint32_t phy_address;
    uint32_t phy_identifier;
    uint8_t phy_interface;
    bool enable_autonegotiation;
    uint32_t speed_mbps;
    uint8_t duplex;
    uint8_t advertise;
    bool link_status;
    bool async_pause;
    bool pause;
} eth_phy_config_t;

/**
 * @brief Detect PHY presence. This function is called during network interface init.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[out] pphy_config The configuration structure which gets populated once the phy is
 *             discovered.
 *
 * @return
 * - 0: if PHY is detected
 * - -EIO: if PHY is not detected
 */
int32_t eth_phy_discover(uint32_t base_address, eth_phy_config_t *pphy_config);

/**
 * @brief Initialize the PHY. This function is called after PHY discovery.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] pphy_config The configuration structure which will be used to configure the PHY
 *            and check phy link status.
 *
 * @return
 * - 0: if PHY is initialized
 * - -EINVAL: if PHY configuration fails
 * - -EIO: if the link is down
 */
int32_t eth_phy_initialize(uint32_t base_address, eth_phy_config_t *pphy_config);

/**
 * @brief Reconfigure the PHY and update link status.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] pphy_config The configuration structure used to reconfigure the PHY
 *            and update link status.
 *
 * @return
 * - 0: if PHY is reconfigured and the link is up
 * - -EIO: if PHY reconfiguration fails or link is down
 */
int32_t eth_phy_update_link(uint32_t base_address, eth_phy_config_t *pphy_config);

/**
 * @brief Return the current link status and update the configuration.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] pphy_config The configuration structure to update.
 *
 * @return
 * - true, when phy link is up
 * - false, when phy link is down
 */
bool eth_phy_get_link_status(uint32_t base_address, eth_phy_config_t *pphy_config);

/**
 * @brief Perform PHY-specific setup.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 *
 * @return true if successful, otherwise false
 */
bool eth_phy_setup(uint32_t base_address, uint32_t phy_address);

/**
 * @brief Disable auto-negotiation on the PHY.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 *
 * @return true if successful, otherwise false
 */
bool eth_phy_disable_autoneg(uint32_t base_address, uint32_t phy_address);

/**
 * @brief Read the PHY identifier registers.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 * @param[out] phy_id1 The first 16-bit PHY ID value.
 * @param[out] phy_id2 The second 16-bit PHY ID value.
 *
 */
void eth_phy_get_id(uint32_t base_address, uint32_t phy_address,
        uint16_t *phy_id1, uint16_t *phy_id2);

/**
 * @brief Perform auto-negotiation to determine speed and duplex.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 * @param[out] speed_mbps The negotiated speed in Mbps.
 * @param[out] duplex The negotiated duplex mode (ETH_FULL_DUPLEX or ETH_HALF_DUPLEX).
 *
 * @return true if successful, otherwise false
 */
bool eth_phy_auto_negotiate(uint32_t base_address, uint32_t phy_address,
        uint32_t *speed_mbps, uint8_t *duplex);

/**
 * @brief Get the current link status from the PHY.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 *
 * @return true if link is up, otherwise false
 */
bool eth_phy_get_link(uint32_t base_address, uint32_t phy_address);

/**
 * @brief Reset the PHY.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 *
 * @return true if successful, otherwise false
 */
bool eth_phy_reset(uint32_t base_address, uint32_t phy_address);

/**
 * @brief Configure the PHY link speed and duplex mode.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 * @param[in] speed_mbps The desired speed in Mbps.
 * @param[in] duplex The desired duplex mode (ETH_FULL_DUPLEX or ETH_HALF_DUPLEX).
 *
 * @return true if successful, otherwise false
 */
bool eth_phy_set_cfg_link(uint32_t base_address, uint32_t phy_address,
                uint32_t speed_mbps, uint8_t duplex);

#ifdef __cplusplus
}
#endif

#endif /* ifndef __SOCFPGA_PHY_H__ */
