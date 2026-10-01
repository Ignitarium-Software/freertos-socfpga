/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA SDMMC
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include "socfpga_cache.h"
#include "socfpga_defines.h"
#include "socfpga_combo_phy.h"
#include "socfpga_sdmmc.h"
#include "socfpga_sdmmc_ll.h"
#include "socfpga_sdmmc_reg.h"
#include "socfpga_clk_mngr_reg.h"

#define SINGLE_BLOCK           (0U)
#define MULTI_BLOCK            (1U)
#define DATA_WRITE             (0U)
#define DATA_READ              (1U)
#define DATA_PRESENT           (1U)

#define CARD_DETECTED          (1U)
#define VAL_DESCRIPTOR         (1U)
#define EN_BUS_PWR             (1U)
#define EN_INTERN_CLK          (1U)
#define EN_SD_CLK              (1U)

/*
 * DMASEL=2 selects ADMA2 in Host Control 1. In v4 mode (HV4E=1) this is
 * correct: ADMA2 is selected here and 64-bit system address support is
 * enabled separately via A64B in Host Control 2. Using DMASEL=3 in v4 mode
 * would activate ADMA3 on controllers that support it, which requires a
 * different descriptor protocol.
 */
#define SEL_ADMA2             (2U)
#define EN_DMA                (1U)
#define EN_BCT                (1U)

#define SET_VOLT_3_3           (7U)
#define SET_VOLT_1_8           (5U)

#define PHY_SW_RST             (1U)
#define EN_EXT_WR_MODE         (1U)
#define EN_EXT_RDCMD_MODE      (1U)
#define EN_EXT_RDDATA_MODE     (1U)

#define CMD_INHIBIT_SET     SDMMC_SRS09_CICMD_MASK
#define DATA_INHIBIT_SET    SDMMC_SRS09_CIDAT_MASK

#define AUTO_CMD_23_EN         (2U)

#define SDSC_DETECTED     (0x0U)
#define SDHC_DETECTED     (0x1U)
#define DAT_TIMOUT_CTR    (0xeU)

#define SDMMC_DMA_MAX_BUFFER_SIZE    (64U * 1024U)
#define RESET_SOFTPHY                (1U << 6U)
#define RESET_SDMMC                  (1U << 7U)
#define RESET_SDMMC_ECC              ((uint32_t)1 << 15U)
#define IS_CARD_READY                ((uint32_t)1 << 31U)
#define END_DESCRIPTOR               (1U << 1U)
#define XFER_DATA                    (1U << 5U)
#define BIT_MASK_32                  (0xFFFFFFFFUL)

#define RESET_TIMEOUT         10000

#define ADMA2_TRAN_VALID    ((uint8_t)(XFER_DATA | VAL_DESCRIPTOR))
#define ADMA2_END           ((uint8_t)END_DESCRIPTOR)

static uint32_t sdmmc_srs03_xfer_cfg;
static bool sdmmc_v4_mode = true;

/**
 * @brief Writes one ADMA2 descriptor entry and advances descriptor cursor.
 *
 * @param[in,out] pdesc_cursor Pointer to current descriptor cursor.
 * @param[in]     data_addr    System memory address for this descriptor.
 * @param[in]     xfer_len     Transfer length for this descriptor.
 * @param[in]     attr         ADMA2 attribute byte.
 */
/**
 * @brief Writes one ADMA2 descriptor entry and advances descriptor cursor.
 *
 * @param[in,out] pdesc_cursor Pointer to current descriptor cursor.
 * @param[in]     data_addr    System memory address for this descriptor.
 * @param[in]     xfer_len     Transfer length for this descriptor.
 * @param[in]     attr         ADMA2 attribute byte.
 */
static void adma_write_desc(void **pdesc_cursor, uint64_t data_addr,
        uint16_t xfer_len, uint8_t attr)
{
    uint8_t *desc_entry = *(uint8_t **)pdesc_cursor;

    desc_entry[0] = attr;
    desc_entry[1] = 0U;
    *(uint16_t *)(desc_entry + 2U) = xfer_len;
    *(uint32_t *)(desc_entry + 4U) = (uint32_t)(data_addr & BIT_MASK_32);
    *(uint32_t *)(desc_entry + 8U) = (uint32_t)((data_addr >> 32U) & BIT_MASK_32);
    if (sdmmc_v4_mode)
    {
        *(uint32_t *)(desc_entry + 12U) = 0U;
    }

    *(uint8_t **)pdesc_cursor +=
            sdmmc_v4_mode ? SDMMC_DESC_V4_SIZE : SDMMC_DESC_V3_SIZE;
}

/**
 * @brief Marks the last ADMA2 descriptor with END bit.
 *
 * @param[in,out] last_desc Pointer to last descriptor entry.
 */
/**
 * @brief Marks the last ADMA2 descriptor with END bit.
 *
 * @param[in,out] last_desc Pointer to last descriptor entry.
 */
static void adma_mark_end(void *last_desc)
{
    ((uint8_t *)last_desc)[0] |= ADMA2_END;
}

/**
 * @brief Resets and waits for PHY DLL initialization completion.
 *
 * @return
 * - 0 on success.
 * - -EIO on timeout.
 */
/**
 * @brief Resets and waits for PHY DLL initialization completion.
 *
 * @return
 * - 0 on success.
 * - -EIO on timeout.
 */
static int32_t reset_dll(void)
{
    uint32_t reg_val;
    uint32_t count = RESET_TIMEOUT;

    do
    {
        reg_val = RD_REG32(HRS_BASE_ADDR + SDMMC_HRS09);
        reg_val |= 1U;
        WR_REG32(HRS_BASE_ADDR + SDMMC_HRS09, reg_val);
        if (count == 0U)
        {
            return -EIO;
        }
        count--;
    } while ((RD_REG32(HRS_BASE_ADDR + SDMMC_HRS09) &
            (1U << SDMMC_HRS09_PHY_INIT_COMPLETE_POS)) ==
            (0U << SDMMC_HRS09_PHY_INIT_COMPLETE_POS));

    return 0;
}

/**
 * @brief Programs host-side PHY timing configuration registers.
 *
 * @param[in] phost_cfg Pointer to host PHY configuration.
 */
/**
 * @brief Programs host-side PHY timing configuration registers.
 *
 * @param[in] phost_cfg Pointer to host PHY configuration.
 */
static void prgm_host_config_from_cfg(const sdmmc_host_cfg_t *phost_cfg)
{
    uint32_t reg_val;

    if (phost_cfg == NULL)
    {
        return;
    }

    reg_val = PHY_SW_RST |
            (phost_cfg->sdhc_rddata_en << SDMMC_HRS09_RDDATA_EN_POS) |
            (phost_cfg->sdhc_rdcmd_en << SDMMC_HRS09_RDCMD_EN_POS) |
            (phost_cfg->sdhc_extended_rd_mode << SDMMC_HRS09_EXTENDED_RD_MODE_POS) |
            (phost_cfg->sdhc_extended_wr_mode << SDMMC_HRS09_EXTENDED_WR_MODE_POS);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS09, reg_val);

    reg_val = phost_cfg->sdhc_hcsdclkadj << SDMMC_HRS10_HCSDCLKADJ_POS;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS10, reg_val);

    reg_val = (phost_cfg->sdhc_wrcmd0_sdclk_dly << SDMMC_HRS16_WRCMD0_SDCLK_DLY_POS) |
            (phost_cfg->sdhc_wrdata0_dly << SDMMC_HRS16_WRDATA0_DLY_POS) |
            (phost_cfg->sdhc_wrcmd0_dly << SDMMC_HRS16_WRCMD0_DLY_POS);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS16, reg_val);

    reg_val = phost_cfg->sdhc_rw_compensate << SDMMC_HRS07_RW_COMPENSATE_POS;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS07, reg_val);
}

/**
 * @brief Extracts bit-fields from a 136-bit response array.
 *
 * @param[in] presp Aligned response words.
 * @param[in] start Field start bit.
 * @param[in] size  Field width in bits.
 *
 * @return Extracted field value.
 */
/**
 * @brief Extracts bit-fields from a 136-bit response array.
 *
 * @param[in] presp Aligned response words.
 * @param[in] start Field start bit.
 * @param[in] size  Field width in bits.
 *
 * @return Extracted field value.
 */
static uint32_t sdmmc_unstuff_bits(const uint32_t *presp, int32_t start,
        int32_t size)
{
    int32_t offset;
    int32_t shift;
    uint32_t mask;
    uint32_t value;

    if ((presp == NULL) || (size <= 0) || (size > 32))
    {
        return 0U;
    }

    offset = 3 - (start / 32);
    shift = start & 31;

    value = presp[offset] >> shift;
    if ((size + shift) > 32)
    {
        value |= presp[offset - 1] << (32 - shift);
    }

    if (size == 32)
    {
        mask = 0xFFFFFFFFU;
    }
    else
    {
        mask = (1U << size) - 1U;
    }

    return value & mask;
}

/**
 * @brief Enables extended PHY read/write command/data modes.
 */
/**
 * @brief Enables extended PHY read/write command/data modes.
 */
static void config_phy_xfer_params(void)
{
    uint32_t hrs09_reg_val;

    hrs09_reg_val = RD_REG32(HRS_BASE_ADDR + SDMMC_HRS09);
    hrs09_reg_val &= ~(PHY_SW_RST);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS09, hrs09_reg_val);

    hrs09_reg_val |= ((EN_EXT_WR_MODE << SDMMC_HRS09_EXTENDED_WR_MODE_POS) |
            ((uint32_t)EN_EXT_RDCMD_MODE << SDMMC_HRS09_RDCMD_EN_POS) |
            ((uint32_t)EN_EXT_RDDATA_MODE << SDMMC_HRS09_RDDATA_EN_POS));

    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS09, hrs09_reg_val);
    hrs09_reg_val |= PHY_SW_RST;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS09, hrs09_reg_val);
}

/**
 * @brief Enables command-related interrupt status/signal bits.
 */
/**
 * @brief Enables command-related interrupt status/signal bits.
 */
static void sdmmc_enable_cmd_int(void)
{
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS14, SDMMC_CMD_INT_MASK);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS13, SDMMC_CMD_INT_MASK);
}

/**
 * @brief Enables command and transfer interrupt status/signal bits.
 */
/**
 * @brief Enables command and transfer interrupt status/signal bits.
 */
static void sdmmc_enable_xfer_int(void)
{
    uint32_t int_mask = SDMMC_CMD_INT_MASK | SDMMC_XFER_INT_MASK;

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS14, int_mask);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS13, int_mask);
}

int32_t sdmmc_send_command(const cmd_parameters_t *params)
{
    uint32_t count = 0;
    uint32_t srs03_reg_value;
    uint32_t srs02_reg_value;
    uint32_t prsnt_state_mask = CMD_INHIBIT_SET;

    if (params->data_xfer_present == DATA_PRESENT)
    {
        srs03_reg_value = sdmmc_srs03_xfer_cfg;
        prsnt_state_mask |= DATA_INHIBIT_SET;
    }
    else
    {
        srs03_reg_value = 0U;
    }

    count = RESET_TIMEOUT;
    while ((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09) & prsnt_state_mask) != 0U)
    {
        if (count == 0U)
        {
            return -EIO;
        }
        count--;
    }

    /* argument and dependent param configuration */
    srs03_reg_value |= (uint32_t)params->command_index << SDMMC_SRS03_CIDX_POS;
    srs03_reg_value |= (uint32_t)params->data_xfer_present <<
            SDMMC_SRS03_DPS_POS;
    srs03_reg_value |= (uint32_t)params->id_check_enable <<
            SDMMC_SRS03_CICE_POS;
    srs03_reg_value |= (uint32_t)params->crc_check_enable <<
            SDMMC_SRS03_CRCCE_POS;
    srs03_reg_value |= (uint32_t)params->response_type << SDMMC_SRS03_RTS_POS;

    srs02_reg_value = (uint32_t)params->argument;
    /* send stop request once block count decrements to 0 in case of multi block read/write */
    if ((params->command_index == SDMMC_CMD_WRITE_MULT_BLOCK) ||
            (params->command_index == SDMMC_CMD_READ_MULT_BLOCK))
    {
        srs03_reg_value |= (AUTO_CMD_23_EN << SDMMC_SRS03_ACE_POS);
    }
    if (params->data_xfer_present == DATA_PRESENT)
    {
        sdmmc_enable_xfer_int();
    }
    else
    {
        sdmmc_enable_cmd_int();
    }

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS02, srs02_reg_value);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS03, srs03_reg_value);

    return 0;
}

void sdmmc_set_xfer_config(cmd_parameters_t const *params)
{
    uint32_t srs03_reg_value = 0;

    /* configure no of blocks for the transaction */
    switch (params->command_index)
    {
        case SDMMC_CMD_SWITCH:
        case SDMMC_CMD_SEND_SCR:
        case SDMMC_CMD_READ_MULT_BLOCK:
        case SDMMC_CMD_READ_SINGLE_BLOCK:
        case SDMMC_CMD_SEND_EXT_CSD:
            srs03_reg_value |= DATA_READ << SDMMC_SRS03_DTDS_POS;
            break;

        case SDMMC_CMD_WRITE_MULT_BLOCK:
        case SDMMC_CMD_WRITE_SINGLE_BLOCK:
            srs03_reg_value |= DATA_WRITE << SDMMC_SRS03_DTDS_POS;
            break;
        default:
            /* Do Nothing */
            break;
    }
    /* configure for write and read */
    switch (params->command_index)
    {
        case SDMMC_CMD_READ_MULT_BLOCK:
        case SDMMC_CMD_WRITE_MULT_BLOCK:
            srs03_reg_value |= (uint32_t)MULTI_BLOCK << SDMMC_SRS03_MSBS_POS;
            srs03_reg_value |= AUTO_CMD_23_EN << SDMMC_SRS03_ACE_POS;
            srs03_reg_value |= (uint32_t)1U << SDMMC_SRS03_BCE_POS;
            break;

        case SDMMC_CMD_SWITCH:
        case SDMMC_CMD_SEND_SCR:
        case SDMMC_CMD_READ_SINGLE_BLOCK:
        case SDMMC_CMD_WRITE_SINGLE_BLOCK:
        case SDMMC_CMD_SEND_EXT_CSD:
            srs03_reg_value |= (uint32_t)SINGLE_BLOCK << SDMMC_SRS03_MSBS_POS;
            srs03_reg_value |= (uint32_t)1U << SDMMC_SRS03_BCE_POS;
            break;
        default:
            /* Do Nothing */
            break;
    }

    srs03_reg_value |= (uint32_t)1U << SDMMC_SRS03_DMAE_POS;
    sdmmc_srs03_xfer_cfg = srs03_reg_value;
    /* wait for the configurations to reflect */
    for (volatile int i = 0; i < 10000; i++)
    {

    }
}

void sdmmc_set_up_xfer(void *desc_buf, uint64_t *buf,
        uint32_t block_size, uint32_t block_ct)
{
    uint32_t desc_sz = sdmmc_v4_mode ? SDMMC_DESC_V4_SIZE : SDMMC_DESC_V3_SIZE;
    uint32_t size = block_size * block_ct;
    uint32_t descriptor_count = (size + DESC_MAX_XFER_SIZE - 1U) / DESC_MAX_XFER_SIZE;
    void *desc = desc_buf;
    uint32_t i;

    cache_force_invalidate((uint64_t *)buf, size);

    for (i = 0U; i < descriptor_count; i++)
    {
        uint64_t buf_addr = (uint64_t)(uintptr_t)buf + ((uint64_t)DESC_MAX_XFER_SIZE * i);
        uint16_t len;

        if ((i + 1U) < descriptor_count)
        {
            len = 0U;                        /* 0 == 65536 bytes in ADMA2 */
            size -= SDMMC_DMA_MAX_BUFFER_SIZE;
        }
        else
        {
            len = (uint16_t)size;            /* remaining bytes for last entry */
        }

        adma_write_desc(&desc, buf_addr, len, ADMA2_TRAN_VALID);
    }

    adma_mark_end((uint8_t *)desc - desc_sz);

    cache_force_write_back((uint64_t *)desc_buf, descriptor_count * desc_sz);

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS22, (uint32_t)(uint64_t)(uintptr_t)desc_buf);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS23, (uint32_t)(((uint64_t)(uintptr_t)desc_buf) >> 32U));
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS00, (uint32_t)block_ct);

    uint32_t val = (uint32_t)((block_ct << SDMMC_SRS01_BCCT_POS) & SDMMC_SRS01_BCCT_MASK);
    val |= (uint32_t)((block_size << SDMMC_SRS01_TBS_POS) & SDMMC_SRS01_TBS_MASK);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS01, val);
}

int32_t sdmmc_host_set_clock(uint32_t is_high_speed)
{
    uint32_t srs10_reg_value;
    uint32_t srs11_reg_value;
    uint32_t count = RESET_TIMEOUT;

    if (is_high_speed > 1U)
    {
        return -EINVAL;
    }

    srs10_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS10);
    srs11_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11);

    srs11_reg_value &= ~SDMMC_SRS11_SDCE_MASK;
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    srs11_reg_value &= ~SDMMC_SRS11_SDCFSL_MASK;
    srs10_reg_value &= ~SDMMC_SRS10_HSE_MASK;

    if (is_high_speed == 0U)
    {
        srs11_reg_value |= (uint32_t)SDMMC_SDR12_FREQ_SEL << SDMMC_SRS11_SDCFSL_POS;
    }
    else
    {
        srs11_reg_value |= (uint32_t)SDMMC_SDR25_FREQ_SEL << SDMMC_SRS11_SDCFSL_POS;
        srs10_reg_value |= (uint32_t)1U << SDMMC_SRS10_HSE_POS;
    }

    srs11_reg_value |= ((uint32_t)EN_INTERN_CLK << SDMMC_SRS11_ICE_POS);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    while (((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11) & SDMMC_SRS11_ICS_MASK) ==
            0U) && (count > 0U))
    {
        count--;
    }
    if (count == 0U)
    {
        return -ETIMEDOUT;
    }

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS10, srs10_reg_value);

    srs11_reg_value |= ((uint32_t)EN_SD_CLK << SDMMC_SRS11_SDCE_POS);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    return 0;
}

int32_t sdmmc_set_data_bus_width(uint32_t bus_width)
{
    uint32_t dtw;
    uint32_t edtw;
    uint32_t srs10_reg_value;

    switch (bus_width)
    {
    case 1U:
        dtw = 0U;
        edtw = 0U;
        break;
    case 4U:
        dtw = 1U;
        edtw = 0U;
        break;
    case 8U:
        dtw = 1U;
        edtw = 1U;
        break;
    default:
        return -EINVAL;
    }

    srs10_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS10);
    srs10_reg_value &= ~(SDMMC_SRS10_DTW_MASK | SDMMC_SRS10_EDTW_MASK);
    srs10_reg_value |= dtw << SDMMC_SRS10_DTW_POS;
    srs10_reg_value |= edtw << SDMMC_SRS10_EDTW_POS;
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS10, srs10_reg_value);

    return 0;
}

int32_t sdmmc_host_set_sd_clock_enable(uint32_t enable)
{
    uint32_t srs11_reg_value;
    uint32_t count;

    srs11_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11);

    switch (enable)
    {
    case 0U:
        srs11_reg_value &= ~SDMMC_SRS11_ICE_MASK;
        srs11_reg_value &= ~SDMMC_SRS11_SDCE_MASK;
        break;
    case 1U:
        srs11_reg_value |= SDMMC_SRS11_ICE_MASK;
        WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

        count = RESET_TIMEOUT;
        while (((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11) & SDMMC_SRS11_ICS_MASK) ==
                0U) && (count > 0U))
        {
            count--;
        }

        if (count == 0U)
        {
            return -ETIMEDOUT;
        }

        srs11_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11);
        srs11_reg_value |= SDMMC_SRS11_SDCE_MASK;
        WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);
        return 0;
    default:
        return -EINVAL;
    }

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);
    return 0;
}

int32_t sdmmc_set_bus_power(uint32_t enable)
{
    uint32_t srs10_reg_value;

    srs10_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS10);

    switch (enable)
    {
    case 0U:
        srs10_reg_value &= ~SDMMC_SRS10_BP_MASK;
        break;
    case 1U:
        srs10_reg_value |= SDMMC_SRS10_BP_MASK;
        break;
    default:
        return -EINVAL;
    }

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS10, srs10_reg_value);
    return 0;
}

int32_t sdmmc_host_set_1v8_signaling(bool enable)
{
    uint32_t count = RESET_TIMEOUT;
    uint32_t srs10_reg_value;
    uint32_t srs15_reg_value;
    bool is_set;

    srs10_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS10);
    srs15_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS15);

    srs10_reg_value &= ~SDMMC_SRS10_BVS_MASK;

    if (enable)
    {
        srs15_reg_value |= SDMMC_SRS15_V18SE_MASK;
        srs10_reg_value |= ((uint32_t)SET_VOLT_1_8 << SDMMC_SRS10_BVS_POS);
    }
    else
    {
        srs15_reg_value &= ~SDMMC_SRS15_V18SE_MASK;
        srs10_reg_value |= ((uint32_t)SET_VOLT_3_3 << SDMMC_SRS10_BVS_POS);
    }

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS10, srs10_reg_value);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS15, srs15_reg_value);

    while (count > 0U)
    {
        is_set = (RD_REG32(SRS_BASE_ADDR + SDMMC_SRS15) &
                SDMMC_SRS15_V18SE_MASK) != 0U;
        if (is_set == enable)
        {
            return 0;
        }
        count--;
    }

    return -ETIMEDOUT;
}

int32_t sdmmc_wait_cmd_data_busy_clear(void)
{
    uint32_t count = RESET_TIMEOUT;

    while ((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09) &
            (SDMMC_SRS09_CICMD_MASK | SDMMC_SRS09_CIDAT_MASK)) != 0U)
    {
        if (count == 0U)
        {
            return -ETIMEDOUT;
        }
        count--;
    }

    return 0;
}

int32_t sdmmc_recover_cmd_dat_lines(void)
{
    uint32_t count = RESET_TIMEOUT;
    uint32_t srs11_reg_value;

    srs11_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11);
    srs11_reg_value |= SDMMC_SRS11_SRCMD_MASK | SDMMC_SRS11_SRDAT_MASK;
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    while ((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11) &
            (SDMMC_SRS11_SRCMD_MASK | SDMMC_SRS11_SRDAT_MASK)) != 0U)
    {
        if (count == 0U)
        {
            return -ETIMEDOUT;
        }
        count--;
    }

    return 0;
}

int32_t sdmmc_host_set_uhs_mode(uint32_t uhs_mode)
{
    uint32_t srs15_reg_value;

    if ((uhs_mode != SDMMC_UHS_MODE_LEGACY) &&
            (uhs_mode != SDMMC_UHS_MODE_SDR12) &&
            (uhs_mode != SDMMC_UHS_MODE_SDR25) &&
            (uhs_mode != SDMMC_UHS_MODE_SDR50))
    {
        return -EINVAL;
    }

    srs15_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS15);
    srs15_reg_value &= ~SDMMC_SRS15_UMS_MASK;
    srs15_reg_value |= uhs_mode << SDMMC_SRS15_UMS_POS;
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS15, srs15_reg_value);

    return 0;
}

int32_t sdmmc_host_set_uhs_clock(uint32_t freq_sel)
{
    uint32_t srs10_reg_value;
    uint32_t srs11_reg_value;
    uint32_t count = RESET_TIMEOUT;

    if (freq_sel > 0xFFU)
    {
        return -EINVAL;
    }

    srs10_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS10);
    srs11_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11);

    srs11_reg_value &= ~SDMMC_SRS11_SDCE_MASK;
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    srs11_reg_value &= ~SDMMC_SRS11_SDCFSL_MASK;
    srs10_reg_value &= ~SDMMC_SRS10_HSE_MASK;
    srs11_reg_value |= freq_sel << SDMMC_SRS11_SDCFSL_POS;
    srs11_reg_value |= ((uint32_t)EN_INTERN_CLK << SDMMC_SRS11_ICE_POS);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    while (((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS11) & SDMMC_SRS11_ICS_MASK) ==
            0U) && (count > 0U))
    {
        count--;
    }

    if (count == 0U)
    {
        return -ETIMEDOUT;
    }

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS10, srs10_reg_value);

    srs11_reg_value |= ((uint32_t)EN_SD_CLK << SDMMC_SRS11_SDCE_POS);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);
    return 0;
}

bool sdmmc_is_card_busy_dat0(void)
{
    uint32_t datsl1;

    datsl1 = (RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09) &
            SDMMC_SRS09_DATSL1_MASK) >> SDMMC_SRS09_DATSL1_POS;

    return (datsl1 & 0x1U) == 0U;
}

int32_t sdmmc_init_configs(uint32_t dev_type)
{
    uint32_t default_bus_width;
    uint32_t srs10_reg_value = 0;
    uint32_t srs11_reg_value = 0;
    uint32_t srs03_reg_value = 0;
    int32_t ret;

    if (dev_type == DEV_TYPE_EMMC)
    {
        default_bus_width = 8U;
    }
    else if (dev_type == DEV_TYPE_SD)
    {
        default_bus_width = 1U;
    }
    else
    {
        return -EINVAL;
    }

    srs03_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS03);
    srs10_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS10);
    srs10_reg_value &= ~((SDMMC_SRS10_DMASEL_MASK) | (SDMMC_SRS10_DTW_MASK) |
            (SDMMC_SRS10_EDTW_MASK) | (SDMMC_SRS10_SBGR_MASK) |
            (SDMMC_SRS10_BVS_MASK) | (SDMMC_SRS10_BVS_MASK));

    srs11_reg_value |= ((uint32_t)DAT_TIMOUT_CTR << SDMMC_SRS11_DTCV_POS);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS11, srs11_reg_value);

    srs10_reg_value |= ((uint32_t)SET_VOLT_3_3 << SDMMC_SRS10_BVS_POS);
    srs10_reg_value |= (SEL_ADMA2 << SDMMC_SRS10_DMASEL_POS);
    srs10_reg_value |= (uint32_t)EN_BUS_PWR << SDMMC_SRS10_BP_POS;

    srs03_reg_value |= (EN_BCT << SDMMC_SRS03_BCE_POS);
    srs03_reg_value |= (EN_DMA << SDMMC_SRS03_DMAE_POS);

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS03, srs03_reg_value);
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS10, srs10_reg_value);

    ret = sdmmc_set_data_bus_width(default_bus_width);
    if (ret != 0)
    {
        return ret;
    }

    uint32_t srs15_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS15);
    srs15_reg_value |= SDMMC_SRS15_HV4E_MASK;
    srs15_reg_value |= SDMMC_SRS15_A64B_MASK;
    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS15, srs15_reg_value);

#if (DEV_TYPE == DEV_TYPE_EMMC)
    ret = sdmmc_host_set_clock(SDMMC_HOST_CLK_FREQ_HS);
#else
    ret = sdmmc_host_set_uhs_clock(SDMMC_FREQ_SEL_400KHZ);
#endif
    if (ret != 0)
    {
        return ret;
    }

    config_phy_xfer_params();
    return 0;
}

void sd_read_response_rel_addr(card_data_t *pxcard)
{
    uint32_t srs04_reg_value = 0;
    srs04_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS04);
    pxcard->relative_address = ((uint64_t)srs04_reg_value >> 16U) & 0xFFFFUL;
}

int32_t sdmmc_reset_configs(void)
{
    uint32_t count = 0;
    uint32_t hrs00_reg_value = 0;
    uint32_t ulhrs09_reg_value = 0;

    hrs00_reg_value = RD_REG32(HRS_BASE_ADDR + SDMMC_HRS00);
    ulhrs09_reg_value = RD_REG32(HRS_BASE_ADDR + SDMMC_HRS09);

    hrs00_reg_value |= 1U << SDMMC_HRS00_SWR_POS;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS00, hrs00_reg_value);
    /* Host requires some delay to reset */
    count = RESET_TIMEOUT;
    while ((RD_REG32(HRS_BASE_ADDR + SDMMC_HRS00) & 1U) == 1U)
    {
        if (count == 0U)
        {
            return -EIO;
        }
        else
        {
            count--;
        }
    }
    ulhrs09_reg_value = 0U;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS09, ulhrs09_reg_value);
    return 0;
}

void sdmmc_get_default_phy_cfg(sdmmc_phy_cfg_t *pphy_cfg,
        sdmmc_host_cfg_t *phost_cfg)
{
    if ((pphy_cfg == NULL) || (phost_cfg == NULL))
    {
        return;
    }

    pphy_cfg->cp_use_ext_lpbk_dqs = 1U;
    pphy_cfg->cp_use_lpbk_dqs = 1U;
    pphy_cfg->cp_use_phony_dqs = 1U;
    pphy_cfg->cp_use_phony_dqs_cmd = 1U;
    pphy_cfg->cp_dqs_sel_oe_end = 4U;
    pphy_cfg->cp_sync_method = 1U;
    pphy_cfg->cp_rd_del_sel = 52U;
    pphy_cfg->cp_sw_half_cycle_shift = 0U;
    pphy_cfg->cp_underrun_suppress = 1U;
    pphy_cfg->cp_gate_cfg_always_on = 1U;
    pphy_cfg->cp_dll_bypass_mode = 1U;
    pphy_cfg->cp_dll_start_point = 4U;
    pphy_cfg->cp_read_dqs_cmd_delay = 0U;
    pphy_cfg->cp_clk_wrdqs_delay = 0U;
    pphy_cfg->cp_clk_wr_delay = 0U;
    pphy_cfg->cp_read_dqs_delay = 0U;
    pphy_cfg->cp_io_mask_always_on = 0U;
    pphy_cfg->cp_io_mask_end = 0U;
    pphy_cfg->cp_io_mask_start = 0U;
    pphy_cfg->cp_data_select_oe_end = 1U;

    phost_cfg->sdhc_rdcmd_en = 1U;
    phost_cfg->sdhc_rddata_en = 1U;
    phost_cfg->sdhc_extended_rd_mode = 1U;
    phost_cfg->sdhc_extended_wr_mode = 1U;
    phost_cfg->sdhc_hcsdclkadj = 3U;
    phost_cfg->sdhc_wrcmd0_sdclk_dly = 0U;
    phost_cfg->sdhc_wrdata0_dly = 1U;
    phost_cfg->sdhc_wrcmd0_dly = 1U;
    phost_cfg->sdhc_rw_compensate = 0xAU;
}

int32_t sdmmc_init_phy_cfg(const sdmmc_phy_cfg_t *pphy_cfg,
        const sdmmc_host_cfg_t *phost_cfg)
{
    uint32_t reg_value;
    uint32_t reg_add;
    int32_t ret = 0;

    if ((pphy_cfg == NULL) || (phost_cfg == NULL))
    {
        return -EINVAL;
    }

    reg_value = (pphy_cfg->cp_use_ext_lpbk_dqs << 22U) |
            (pphy_cfg->cp_use_lpbk_dqs << 21U) |
            (pphy_cfg->cp_use_phony_dqs << 20U) |
            (pphy_cfg->cp_use_phony_dqs_cmd << 19U) |
            (pphy_cfg->cp_dqs_sel_oe_end << 0U);
    reg_add = PHY_DQS_TIM_REG_ADD & REG_ADD_LSB_MASK;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS04, reg_add);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS05, reg_value);

    reg_value = (pphy_cfg->cp_sync_method << 31U) |
            (pphy_cfg->cp_sw_half_cycle_shift << 28U) |
            (pphy_cfg->cp_rd_del_sel << 19U) |
            (pphy_cfg->cp_underrun_suppress << 18U) |
            (pphy_cfg->cp_gate_cfg_always_on << 6U);
    reg_add = PHY_GATE_LPBK_CTL_ADD & REG_ADD_LSB_MASK;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS04, reg_add);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS05, reg_value);

    reg_value = (pphy_cfg->cp_dll_bypass_mode << 23U) |
            (pphy_cfg->cp_dll_start_point << 0U);
    reg_add = PHY_DLL_MASTER_CTL_ADD & REG_ADD_LSB_MASK;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS04, reg_add);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS05, reg_value);

    reg_value = (pphy_cfg->cp_read_dqs_cmd_delay << 24U) |
            (pphy_cfg->cp_clk_wrdqs_delay << 16U) |
            (pphy_cfg->cp_clk_wr_delay << 8U) |
            (pphy_cfg->cp_read_dqs_delay << 0U);
    reg_add = PHY_DLL_SLAVE_CTL_ADD & REG_ADD_LSB_MASK;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS04, reg_add);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS05, reg_value);

    reg_add = PHY_CTL_REG_ADD & REG_ADD_LSB_MASK;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS04, reg_add);
    reg_value = RD_REG32(HRS_BASE_ADDR + SDMMC_HRS05);
    reg_value &= ~PHONY_DQS_DELAY;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS05, reg_value);

    ret = reset_dll();
    if (ret != 0)
    {
        return ret;
    }

    reg_value = (pphy_cfg->cp_io_mask_always_on << 31U) |
            (pphy_cfg->cp_io_mask_end << 27U) |
            (pphy_cfg->cp_io_mask_start << 24U) |
            (pphy_cfg->cp_data_select_oe_end << 0U);
    reg_add = PHY_DQ_TIM_REG_ADD & REG_ADD_LSB_MASK;
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS04, reg_add);
    WR_REG32(HRS_BASE_ADDR + SDMMC_HRS05, reg_value);

    prgm_host_config_from_cfg(phost_cfg);
    return ret;
}

uint32_t sdmmc_read_response(void)
{
    return RD_REG32(SRS_BASE_ADDR + SDMMC_SRS04);
}

void sdmmc_read_r2_response(uint32_t *presp)
{
    uint32_t raw_resp[4];
    uint32_t index;

    if (presp == NULL)
    {
        return;
    }

    raw_resp[0] = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS04);
    raw_resp[1] = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS05);
    raw_resp[2] = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS06);
    raw_resp[3] = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS07);

    /*
     * SDHCI returns 136-bit responses with CRC bits stripped.
     */
    presp[0] = raw_resp[3];
    presp[1] = raw_resp[2];
    presp[2] = raw_resp[1];
    presp[3] = raw_resp[0];

    for (index = 0U; index < 4U; index++)
    {
        presp[index] <<= 8U;
        if (index != 3U)
        {
            presp[index] |= presp[index + 1U] >> 24U;
        }
    }
}

uint32_t sdmmc_read_csd_cmd_class(void)
{
    uint32_t resp[4];

    sdmmc_read_r2_response(resp);
    /* CCC field [95:84] */
    return sdmmc_unstuff_bits(resp, 84, 12);
}

uint32_t sdmmc_read_csd_structure(void)
{
    uint32_t resp[4];

    sdmmc_read_r2_response(resp);
    /* CSD_STRUCTURE field [127:126] */
    return sdmmc_unstuff_bits(resp, 126, 2);
}

int32_t sdmmc_reset_per0(void)
{
    uint32_t reg_add_val;
    uint32_t count = 0;

#if SOFTPHY_CLK_200_MHZ
    /*
     * The clock gated to combo-phy is 200MHz instead of 800MHz.
     * Dividing the combo-phy clock by 1 instead of 4
     * to make the combo-phy to its optimal
     * clock frequency.
     */
    reg_add_val = RD_REG32(PER0MODRST_ADDR);
    /* Assert the sdmmc module reset signal */
    reg_add_val |= RESET_SOFTPHY;
    reg_add_val |= RESET_SDMMC;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);
    uint32_t read_val = RD_REG32(CLK_MNGR_BASE_ADDR + MAINPLL_CLK_MNGR_NOCDIV);

    /* divide the clock by 1 */
    read_val &= ~(0x30000U);
    WR_REG32((CLK_MNGR_BASE_ADDR + MAINPLL_CLK_MNGR_NOCDIV), read_val);
#endif

    reg_add_val = RD_REG32(PER0MODRST_ADDR);
    reg_add_val |= RESET_SOFTPHY;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);

    reg_add_val &= ~RESET_SOFTPHY;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);
    /* Host requires some delay to reset */
    count = RESET_TIMEOUT;

    while ((RD_REG32(PER0MODRST_ADDR) & RESET_SOFTPHY) == (1U << 6U))
    {
        if (count == 0U)
        {
            return -EIO;
        }
        else
        {
            count--;
        }
    }

    reg_add_val |= RESET_SDMMC;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);

    reg_add_val &= ~RESET_SDMMC;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);
    /* Host requires some delay to reset */
    count = RESET_TIMEOUT;

    while ((RD_REG32(PER0MODRST_ADDR) & RESET_SDMMC) == (1U << 7U))
    {
        if (count == 0U)
        {
            return -EIO;
        }
        else
        {
            count--;
        }
    }

    reg_add_val |= RESET_SDMMC_ECC;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);

    reg_add_val &= ~RESET_SDMMC_ECC;
    WR_REG32(PER0MODRST_ADDR, reg_add_val);
    count = RESET_TIMEOUT;
    while ((RD_REG32(PER0MODRST_ADDR) & RESET_SDMMC_ECC) ==
            ((uint32_t)1U << 15U))
    {
        if (count == 0U)
        {
            return -EIO;
        }
        else
        {
            count--;
        }
    }
    return 0;
}

uint32_t sdmmc_is_card_ready(void)
{
    uint32_t card_read_status = (RD_REG32(SRS_BASE_ADDR + SDMMC_SRS04) &
            IS_CARD_READY);
    card_read_status = card_read_status >> 31;
    return card_read_status;
}

void sd_get_card_type(card_data_t *pxcard)
{
    pxcard->card_type = ((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS04) &
            ((uint32_t)1 << 31U)) == ((uint32_t)1 << 31U))
            ? SDHC_DETECTED : SDSC_DETECTED;
}

void sdmmc_clear_int(uint32_t int_status)
{
    uint32_t clr_mask;

    clr_mask = int_status & (SDMMC_SRS12_CQINT_MASK | SDMMC_SRS12_FXE_MASK |
            SDMMC_SRS12_CINT_MASK | SDMMC_SRS12_CR_MASK |
            SDMMC_SRS12_CIN_MASK | SDMMC_SRS12_BRR_MASK |
            SDMMC_SRS12_BWR_MASK | SDMMC_SRS12_DMAINT_MASK |
            SDMMC_SRS12_BGE_MASK | SDMMC_SRS12_TC_MASK |
            SDMMC_SRS12_CC_MASK | SDMMC_SRS12_ERSP_MASK |
            SDMMC_SRS12_EADMA_MASK | SDMMC_SRS12_EAC_MASK |
            SDMMC_SRS12_ECL_MASK | SDMMC_SRS12_EDEB_MASK |
            SDMMC_SRS12_EDCRC_MASK | SDMMC_SRS12_EDT_MASK |
            SDMMC_SRS12_ECI_MASK | SDMMC_SRS12_ECEB_MASK |
            SDMMC_SRS12_ECCRC_MASK | SDMMC_SRS12_ECT_MASK |
            SDMMC_SRS12_EINT_MASK);

    WR_REG32(SRS_BASE_ADDR + SDMMC_SRS12, clr_mask);
}

uint32_t sdmmc_get_int_status(void)
{
    return RD_REG32(SRS_BASE_ADDR + SDMMC_SRS12);
}

void sdmmc_get_timeout_debug_regs(uint32_t *psrs09, uint32_t *psrs12,
        uint32_t *psrs21, uint32_t *psrs22)
{
    if ((psrs09 == NULL) || (psrs12 == NULL) ||
            (psrs21 == NULL) || (psrs22 == NULL))
    {
        return;
    }

    *psrs09 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09);
    *psrs12 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS12);
    *psrs21 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS21);
    *psrs22 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS22);
}

void sdmmc_get_cmd_xfer_debug_regs(uint32_t *psrs03, uint32_t *psrs09,
        uint32_t *psrs12)
{
    if ((psrs03 == NULL) || (psrs09 == NULL) || (psrs12 == NULL))
    {
        return;
    }

    *psrs03 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS03);
    *psrs09 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09);
    *psrs12 = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS12);
}

int32_t sdmmc_wait_data_busy_clear(void)
{
    uint32_t count = RESET_TIMEOUT;

    while ((RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09) & SDMMC_SRS09_CIDAT_MASK)
            != 0U)
    {
        if (count == 0U)
        {
            return -ETIMEDOUT;
        }
        count--;
    }

    return 0;
}

uint32_t sdmmc_is_card_detected(void)
{
    uint32_t srs09_reg_value;
    uint32_t card_check;

    srs09_reg_value = RD_REG32(SRS_BASE_ADDR + SDMMC_SRS09);
    card_check = (srs09_reg_value >> SDMMC_SRS09_CI_POS) & CARD_DETECTED;
    return card_check;
}

void sdmmc_host_configure_volt_reg_gpio_pinmux(void)
{
    uint32_t pinmux_val;
    pinmux_val = RD_REG32(PINMUX_REG(SDMMC_VOLT_REG_SDIO_PIN));
    pinmux_val = (pinmux_val & ~PINMUX_MASK) | PINMUX_GPIO;
    WR_REG32(PINMUX_REG(SDMMC_VOLT_REG_SDIO_PIN), pinmux_val);
}
