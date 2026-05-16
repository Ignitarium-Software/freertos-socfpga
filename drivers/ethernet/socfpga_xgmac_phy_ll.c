/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA PHY
 */
#include "socfpga_defines.h"
#include "socfpga_xgmac_phy_ll.h"
#include "socfpga_xgmac_reg.h"
#include "socfpga_xgmac_ll.h"
#include <errno.h>
#include "osal_log.h"

#define CLK_SKEW_MASK       (0x1FU)
#define CLK_SKEW_BIT_COUNT  (5U)

#define MARVELL_COPPER_SPECIFIC_STATUS_SPEED_MASK    0xC000U
#define MARVELL_COPPER_SPECIFIC_STATUS_SPEED_1000    0x8000U
#define MARVELL_COPPER_SPECIFIC_STATUS_SPEED_100     0x4000U
#define MARVELL_COPPER_SPECIFIC_STATUS_DPLX_MASK     0x2000U

#define MICROCHIP_PHY_CONTROL_SPEED_1000    0x40
#define MICROCHIP_PHY_CONTROL_SPEED_100     0x20U
#define MICROCHIP_PHY_CONTROL_DPLX_MASK     0x8U

static inline bool check_mdio_busy(uint32_t base_address)
{
    /* Read the mdio cmd control register */
    uint32_t status = RD_REG32(base_address +
            XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA);

    if ((status & XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SBUSY_MASK) != 0U)
    {
        /* mdio operation is busy */
        return 1;
    }
    else
    {
        /* MDIO is not busy */
        return 0;
    }
}

uint16_t read_phy_reg(uint32_t base_address, uint32_t phy_address, uint8_t phy_reg)
{
    uint32_t data;
    uint32_t addr_data;
    uint8_t count = 0;
    uint8_t max_cnt = 100;

    if (base_address == 0U)
    {
        return 0U;
    }
    /* Check MDIO sbusy bit status */
    while (check_mdio_busy(base_address))
    {
        if (count > max_cnt)
        {
            return 0U;
        }
        for (volatile uint32_t i = 0U; i < max_cnt; i++)
        {
        }
        count++;
    }

    /* Set clause 22 format for PHY address */
    data = RD_REG32(base_address + XGMAC_MDIO_CLAUSE_22_PORT);
    data |= ((uint32_t)1 << phy_address);

    /* selecting clause to given PHY address port. */
    WR_REG32(base_address + XGMAC_MDIO_CLAUSE_22_PORT, data);

    /*
     * Read command address register and mask with our PHY address and PHY
     * register value.
     */
    addr_data = RD_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_ADDRESS);

    /*Mask PHY address and register */
    addr_data = (phy_address << XGMAC_MDIO_SINGLE_COMMAND_ADDRESS_PA_POS) |
                ((uint32_t)phy_reg << XGMAC_MDIO_SINGLE_COMMAND_ADDRESS_RA_POS);

    WR_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_ADDRESS, addr_data);

    /*
     * Read data from MDIO control data register. Mask with the
     * data which we want make.
     */

    /* 3'b010 corresponds to clk_csr_i: 250-300 MHz, with MDC clock: clk_csr_i/122 */
    data = ((uint32_t)4 << XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_CR_POS);

    /* Mask with data var for read operation */
    data |= (XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SAADR_MASK) |
            (XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_CMD_MASK) |
            (XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SBUSY_MASK);

    /* Write masked data to cmd cntrl data register */
    WR_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA, data);

    /* wait for some time */
    for (volatile int i = 0; i < 10000; i++)
    {
    }

    /* Verify MDIO sbusy bit status */
    count = 0;
    while (check_mdio_busy(base_address))
    {
        if (count > max_cnt)
        {
            return 0U;
        }
        for (volatile uint32_t i = 0U; i < max_cnt; i++)
        {
        }
        count++;
    }
    for (volatile uint32_t i = 0U; i < 100000U; i++)
    {
    }

    /* Read the data from the cmd control data register */
    data = RD_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA);
    uint16_t least_bits = (uint16_t)(data & 0xFFFFU);

    return least_bits;
}

int8_t write_phy_reg(uint32_t base_address, uint32_t phy_address, uint8_t phy_reg, uint16_t reg_val)
{
    uint32_t data;
    uint32_t addr_data;
    uint32_t count = 0;
    uint32_t max_cnt = 100;

    if (base_address == 0U)
    {
        return -1;
    }
    /* Check MDIO sbusy bit status */
    while (check_mdio_busy(base_address))
    {
        if (count > max_cnt)
        {
            return -1;
        }
        for (volatile uint32_t i = 0U; i < max_cnt; i++)
        {
        }
        count++;
    }

    /* Set clause 22 format for PHY address */
    data = RD_REG32(base_address + XGMAC_MDIO_CLAUSE_22_PORT);
    data |= ((uint32_t)1 << phy_address);

    /* Selecting clause to given PHY address port. */
    WR_REG32(base_address + XGMAC_MDIO_CLAUSE_22_PORT, data);

    /*
     * Read command address register and mask with our PHY address and PHY
     * register value.
     */
    addr_data = RD_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_ADDRESS);

    /* Mask the phy_address and phy_register to mdio_register */
    addr_data = (phy_address << XGMAC_MDIO_SINGLE_COMMAND_ADDRESS_PA_POS) |
                ((uint32_t)phy_reg << XGMAC_MDIO_SINGLE_COMMAND_ADDRESS_RA_POS);

    WR_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_ADDRESS, addr_data);

    /*
     * Read data MDIO control data register. Mask with the
     * data which we want make.
     */
    data = RD_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA);

    /* Set the application Clock Range 350-400 MHz */
    data = ((uint32_t)4 << XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_CR_POS);

    data |= (XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SAADR_MASK) |
            ((uint32_t)1 << XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_CMD_POS) |
            (XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SBUSY_MASK) |
            ((uint32_t)reg_val << XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SDATA_POS);

    /* Write masked data to MDIO cmd cntrl data register */
    WR_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA, data);

    for (volatile uint32_t i = 0U; i < (max_cnt * 10000U); i++)
    {
    }
    count = 0U;
    while (check_mdio_busy(base_address))
    {
        if (count > max_cnt)
        {
            return -1;
        }
        for (volatile uint32_t i = 0U; i < max_cnt; i++)
        {
        }
        count++;
    }

    return 0;
}

/*
 * @brief Perform a register read on MMD registers
 */
static uint16_t phy_read_reg_mmd(uint32_t base_address, uint32_t phy_address, uint8_t mmd_addr, uint8_t mmd_reg)
{
    uint16_t val;
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_CONTROL_REG, mmd_addr);
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_DATA_REG, mmd_reg);
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_CONTROL_REG, 0x4000 | mmd_addr);
    val = read_phy_reg(base_address, phy_address, MMD_ACCESS_DATA_REG);
    return val;
}

/*
 * @brief Perform a register write on MMD registers
 */
static void phy_write_reg_mmd(uint32_t base_address, uint32_t phy_address, uint8_t mmd_addr, uint8_t mmd_reg, uint16_t reg_val)
{
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_CONTROL_REG, mmd_addr);
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_DATA_REG, mmd_reg);
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_CONTROL_REG, 0x4000 | mmd_addr);
    (void)write_phy_reg(base_address, phy_address, MMD_ACCESS_DATA_REG, reg_val);
}

/*
 * @brief PHY initialization for Microchip KSZ9031RNX PHY.
 */
BaseType_t phy_setup_microchip(uint32_t base_address, xgmac_phy_config_t *pphy_config)
{
    /*
     * By default the microchip KSZ9031RNX phy has incorrect clock skew which causes
     * high packet loss in 1Gbps where clock delays have tight
     * tolerances. This function is to offset the clock delays for
     * optimal functioning
     */

    uint32_t curr_delay = phy_read_reg_mmd(base_address, pphy_config->phy_address, 2U, 0x8U);
    uint16_t tx_clk = (curr_delay >> CLK_SKEW_BIT_COUNT) & CLK_SKEW_MASK;
    uint16_t rx_clk = (curr_delay & CLK_SKEW_MASK);

    tx_clk = MICROCHIP_GTX_CLK_SKEW;
    rx_clk = MICROCHIP_RX_CLK_SKEW;

    tx_clk = tx_clk << CLK_SKEW_BIT_COUNT;
    /* Replacing default values with updated clock pad skews */
    curr_delay = curr_delay & ~(CLK_SKEW_MASK << CLK_SKEW_BIT_COUNT);
    curr_delay = curr_delay & ~(CLK_SKEW_MASK);
    curr_delay = curr_delay | (tx_clk) | (rx_clk);
    phy_write_reg_mmd(base_address, pphy_config->phy_address, 2U, 0x8U, (uint16_t)curr_delay);

    return true;
}

/*
 * @brief PHY initialization for Marvell 88E151X PHY.
 */
BaseType_t phy_setup_marvell(uint32_t base_address, xgmac_phy_config_t *pphy_config)
{
    uint32_t data;
    uint16_t cont_reg2;
    uint32_t max_count = 0U;
    /* Check the selected PHY interface */
    if ((pphy_config->phy_interface & ETH_PHY_IF_RGMII) != 0U)
    {
        /* Select page from page address register */
        data = (uint32_t)write_phy_reg(base_address, pphy_config->phy_address,
                                       PAGE_ADDRESS_SELECT_REG, SELECT_PAGE_EIGHTEEN);

        /*
         * Set Interface mode in General control register and changes
         * to this mode bit should be needed to reset the General PHY
         */
        data =
            (uint32_t)read_phy_reg(base_address, pphy_config->phy_address,
                                   GENERAL_CONTROL_REG_1);
        data &= ~GENERAL_CONTROL_RGMII_COPPER_SELECT_MASK;
        if (write_phy_reg(base_address, pphy_config->phy_address, GENERAL_CONTROL_REG_1,
                          (uint16_t)data) != 0)
        {
            return false;
        }

        data &= ~GENERAL_CONTROL_RESET_MASK;
        if (write_phy_reg(base_address, pphy_config->phy_address, GENERAL_CONTROL_REG_1,
                          (uint16_t)data) != 0)
        {
            return false;
        }

        if (write_phy_reg(base_address, pphy_config->phy_address, PAGE_ADDRESS_SELECT_REG,
                          SELECT_PAGE_TWO) != 0)
        {
            return false;
        }

        cont_reg2 =
            read_phy_reg(base_address, pphy_config->phy_address, MAC_SPECIFIC_CONTROL_REG_2);
        cont_reg2 &= ~((uint16_t)(3U << 4));
        if (write_phy_reg(base_address, pphy_config->phy_address, MAC_SPECIFIC_CONTROL_REG_2,
                          cont_reg2) != 0)
        {
            return false;
        }

        /* Poll the reset bit to complete. */
        while (((data & GENERAL_CONTROL_RESET_MASK) != 0U) && (max_count <
                                                               MAX_GEN_TIMER_COUNT))
        {
            data = (uint32_t)read_phy_reg(base_address, pphy_config->phy_address,
                                          GENERAL_CONTROL_REG_1);
            max_count++;
        }
        if (max_count == MAX_GEN_TIMER_COUNT)
        {
            ERROR("General PHY reset address timed out:%0d.", max_count);
            return false;
        }
    }
    else
    {
        ERROR("Unsupported interface type selected.");
        return false;
    }
    /* Selecting page 0 for remaining operations */
    if (write_phy_reg(base_address, pphy_config->phy_address, PAGE_ADDRESS_SELECT_REG,
                      SELECT_PAGE_ZERO) != 0)
    {
        return false;
    }
    return true;
}

void phy_cfg_link_microchip(uint32_t xgmac_base_addr, xgmac_phy_config_t *pphy_config)
{
    uint32_t val = (uint32_t)read_phy_reg(xgmac_base_addr, pphy_config->phy_address,
            PHY_CONTROL_REG);

    /* Check speed */
    if ((val & MICROCHIP_PHY_CONTROL_SPEED_1000) != 0)
    {
        pphy_config->speed_mbps = ETH_SPEED_1000_MBPS;
    }
    else if ((val & MICROCHIP_PHY_CONTROL_SPEED_100) != 0)
    {
        pphy_config->speed_mbps = ETH_SPEED_100_MBPS;
    }
    else
    {
        pphy_config->speed_mbps = ETH_SPEED_10_MBPS;
    }

    /* Check duplex */
    if ((val & MICROCHIP_PHY_CONTROL_DPLX_MASK) != 0)
    {
        pphy_config->duplex = ETH_FULL_DUPLEX;
    }
    else
    {
        pphy_config->duplex = ETH_HALF_DUPLEX;
    }
}
void phy_cfg_link_marvell(uint32_t xgmac_base_addr, xgmac_phy_config_t *pphy_config)
{
    /* Read 1G status register */
    uint32_t val = (uint32_t)read_phy_reg( xgmac_base_addr, pphy_config->phy_address,
            COPPER_SPECIFIC_STATUS_REG1);

    /* Check speed */
    if ((val & MARVELL_COPPER_SPECIFIC_STATUS_SPEED_MASK) ==
            MARVELL_COPPER_SPECIFIC_STATUS_SPEED_1000)
    {
        pphy_config->speed_mbps = ETH_SPEED_1000_MBPS;
    }
    else if ((val & MARVELL_COPPER_SPECIFIC_STATUS_SPEED_MASK) ==
            MARVELL_COPPER_SPECIFIC_STATUS_SPEED_100)
    {
        pphy_config->speed_mbps = ETH_SPEED_100_MBPS;
    }
    else
    {
        pphy_config->speed_mbps = ETH_SPEED_10_MBPS;
    }

    /* Check duplex */
    if ((val & MARVELL_COPPER_SPECIFIC_STATUS_DPLX_MASK) != 0)
    {
        pphy_config->duplex = ETH_FULL_DUPLEX;
    }
    else
    {
        pphy_config->duplex = ETH_HALF_DUPLEX;
    }
}
