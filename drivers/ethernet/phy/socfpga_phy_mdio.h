/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SoC FPGA PHY MDIO driver
 */

#ifndef __SOCFPGA_PHY_MDIO_H__
#define __SOCFPGA_PHY_MDIO_H__

#include <stdint.h>

#define PHY_READ(base_address, phy_address, phy_reg) \
    read_phy_reg(base_address, phy_address, phy_reg)
#define PHY_WRITE(base_address, phy_address, phy_reg, reg_val) \
    write_phy_reg(base_address, phy_address, phy_reg, reg_val)

/**
 * @brief Read a PHY register over MDIO.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 * @param[in] phy_reg The PHY register address.
 *
 * @return The register value read from the PHY.
 */
uint16_t read_phy_reg(uint32_t base_address, uint32_t phy_address,
        uint8_t phy_reg);

/**
 * @brief Write a PHY register over MDIO.
 *
 * @param[in] base_address The base address of the XGMAC registers.
 * @param[in] phy_address The PHY address on the MDIO bus.
 * @param[in] phy_reg The PHY register address.
 * @param[in] reg_val The value to write.
 *
 * @return 0 on success, or a negative errno on failure.
 */
int8_t write_phy_reg(uint32_t base_address, uint32_t phy_address,
        uint8_t phy_reg, uint16_t reg_val);

#endif /* ifndef __SOCFPGA_PHY_MDIO_H__ */
