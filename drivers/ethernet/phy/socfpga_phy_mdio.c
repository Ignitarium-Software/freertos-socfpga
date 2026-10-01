/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * MDIO access implementation for SoC FPGA PHY
 */

#include "socfpga_phy_mdio.h"
#include <errno.h>
#include "socfpga_xgmac_reg.h"
#include "socfpga_defines.h"

#define MDIO_BUSY_RETRY_MAX 100U
#define MDIO_BUSY_SPIN_DELAY 100U
#define MDIO_CMD_DELAY_COUNT 10000U
#define MDIO_READ_STABLE_DELAY 100000U

static inline bool is_mdio_busy(uint32_t base_address)
{
    /* Read the mdio cmd control register */
    uint32_t status = RD_REG32(base_address +
            XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA);

    if ((status & XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA_SBUSY_MASK) != 0U)
    {
        /* mdio operation is busy */
        return true;
    }
    /* MDIO is not busy */
    return false;
}

uint16_t read_phy_reg(uint32_t base_address, uint32_t phy_address, uint8_t phy_reg)
{
    uint32_t data;
    uint32_t addr_data;
    uint8_t count = 0;
    uint16_t least_bits;

    if (base_address == 0U)
    {
        return 0U;
    }
    /* Wait for MDIO busy bit to be cleared */
    while (is_mdio_busy(base_address))
    {
        if (count > MDIO_BUSY_RETRY_MAX)
        {
            return 0U;
        }
        for (volatile uint32_t i = 0U; i < MDIO_BUSY_SPIN_DELAY; i++)
        {
        }
        count++;
    }

    /* PHYs support clause 22 register access protocol */
    data = RD_REG32(base_address + XGMAC_MDIO_CLAUSE_22_PORT);
    data |= ((uint32_t)1 << phy_address);

    /* selecting clause to given PHY address port. */
    WR_REG32(base_address + XGMAC_MDIO_CLAUSE_22_PORT, data);

    /*
     * Read command address register and mask with our PHY address and PHY
     * register value.
     */
    addr_data = RD_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_ADDRESS);

    /* Mask PHY address and register */
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

    /* Allow MDIO command to start and the SBUSY bit to latch. */
    for (volatile uint32_t i = 0U; i < MDIO_CMD_DELAY_COUNT; i++)
    {
    }

    /* Verify MDIO sbusy bit status, wait for it to be cleared */
    count = 0;
    while (is_mdio_busy(base_address))
    {
        if (count > MDIO_BUSY_RETRY_MAX)
        {
            return 0U;
        }
        for (volatile uint32_t i = 0U; i < MDIO_BUSY_SPIN_DELAY; i++)
        {
        }
        count++;
    }
    /* Provide a short settle time before reading the data back. */
    for (volatile uint32_t i = 0U; i < MDIO_READ_STABLE_DELAY; i++)
    {
    }

    /* Read the data from the cmd control data register */
    data = RD_REG32(base_address + XGMAC_MDIO_SINGLE_COMMAND_CONTROL_DATA);
    least_bits = (uint16_t)(data & 0xFFFFU);

    return least_bits;
}

int8_t write_phy_reg(uint32_t base_address, uint32_t phy_address, uint8_t phy_reg, uint16_t reg_val)
{
    uint32_t data;
    uint32_t addr_data;
    uint32_t count = 0;

    if (base_address == 0U)
    {
        return -EINVAL;
    }
    /* Check MDIO sbusy bit status */
    while (is_mdio_busy(base_address))
    {
        if (count > MDIO_BUSY_RETRY_MAX)
        {
            return -ETIMEDOUT;
        }
        for (volatile uint32_t i = 0U; i < MDIO_BUSY_SPIN_DELAY; i++)
        {
        }
        count++;
    }

    /* PHYs support clause 22 register access protocol */
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

    /* Allow the write command to complete before polling. */
    for (volatile uint32_t i = 0U; i < (MDIO_BUSY_RETRY_MAX * MDIO_CMD_DELAY_COUNT); i++)
    {
    }
    count = 0U;
    while (is_mdio_busy(base_address))
    {
        if (count > MDIO_BUSY_RETRY_MAX)
        {
            return -ETIMEDOUT;
        }
        for (volatile uint32_t i = 0U; i < MDIO_BUSY_SPIN_DELAY; i++)
        {
        }
        count++;
    }

    return 0;
}
