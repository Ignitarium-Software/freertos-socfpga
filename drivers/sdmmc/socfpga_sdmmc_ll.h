/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SoC FPGA SDMMC low level driver
 */

#ifndef __SOCFPGA_SDMMC_LL_H__
#define __SOCFPGA_SDMMC_LL_H__

#include <stdbool.h>
#include <stdint.h>
#include "socfpga_gpio.h"

#define PER0MODRST_ADDR      (0x10D11024U)

/* ADMA2 transfer limits and descriptor strides. */
#define DESC_MAX_XFER_SIZE   (64U * 1024U)
#define SDMMC_DESC_V3_SIZE   (12U)
#define SDMMC_DESC_V4_SIZE   (16U)

/* SD clock divider selections (SRS11[SDCFSL], base clock 200MHz). */
#define SDMMC_FREQ_SEL_400KHZ  (0xFAU)
#define SDMMC_FREQ_SEL_25MHZ   (0x04U)
#define SDMMC_FREQ_SEL_50MHZ   (0x02U)
#define SDMMC_FREQ_SEL_100MHZ  (0x01U)

/* Speed-mode aliases used by HAL policy. */
#define SDMMC_SDR12_FREQ_SEL  SDMMC_FREQ_SEL_25MHZ
#define SDMMC_SDR25_FREQ_SEL  SDMMC_FREQ_SEL_50MHZ
#define SDMMC_SDR50_FREQ_SEL  SDMMC_FREQ_SEL_100MHZ

/* Host UHS mode values for SRS15[UMS]. */
#define SDMMC_UHS_MODE_SDR12   (0U)
#define SDMMC_UHS_MODE_SDR25   (1U)
#define SDMMC_UHS_MODE_SDR50   (2U)
#define SDMMC_UHS_MODE_LEGACY  (0U)

#ifndef SDMMC_HOST_MAX_CURRENT_MA
#define SDMMC_HOST_MAX_CURRENT_MA    (200U)
#endif

#ifndef SDMMC_HOST_HS_SUPPORTED
#define SDMMC_HOST_HS_SUPPORTED      (1U)
#endif

#define SDMMC_HOST_CLK_FREQ_DS       (0U)
#define SDMMC_HOST_CLK_FREQ_HS       (1U)

/*
 * Maximum time to wait for an sdmmc command response before timeout.
 * 10ms provides a safe margin for back to back command responses.
 */
#ifndef SDMMC_CMD_TIMEOUT_MS
#define SDMMC_CMD_TIMEOUT_MS            (10UL)
#endif

/*
 * Enable voltage-switch path for UHS bring-up. Duration the SD clock stays
 * gated while the card's I/O regulator settles at 1.8V; kept above the
 * expected settle time as a conservative margin.
 */
#ifndef SDMMC_VOLT_SWITCH_DELAY_MS
#define SDMMC_VOLT_SWITCH_DELAY_MS      (10U)
#endif

#ifndef SDMMC_VOLT_REG_SDIO_PIN
#define SDMMC_VOLT_REG_SDIO_PIN         GPIO1_PIN3
#endif

#ifndef SDMMC_UHS_SWITCH_RETRY_COUNT
#define SDMMC_UHS_SWITCH_RETRY_COUNT    (10)
#endif

#ifndef SOFTPHY_CLK_200_MHZ
#define SOFTPHY_CLK_200_MHZ             (1U)
#endif

/*
 * Duration held after de-asserting and after re-asserting bus power during a
 * recovery power cycle, giving the supply rail time to discharge and to ramp
 * up/settle respectively before the card is clocked again.
 */
#ifndef SDMMC_POWER_CYCLE_DELAY_MS
#define SDMMC_POWER_CYCLE_DELAY_MS      (10U)
#endif

#define DEV_TYPE_SD      0
#define DEV_TYPE_EMMC    1

/*specify max descriptor count here
 * 1 descriptor can handle up to 64KB of data
 */
#ifndef SDMMC_MAX_DESCRIPTOR
#define SDMMC_MAX_DESCRIPTOR    160U
#endif

/* specify your device here */
#ifndef DEV_TYPE
#define DEV_TYPE    DEV_TYPE_SD
#endif

typedef struct
{
    uint32_t argument;
    uint8_t command_index;
    uint8_t data_xfer_present;
    uint8_t response_type;
    uint8_t id_check_enable;
    uint8_t crc_check_enable;

} cmd_parameters_t;

/* SD Configuration Register decoded fields — mirrors Linux include/linux/mmc/card.h */
typedef struct
{
    uint8_t sda_vsn;        /* SD spec version (SCR_SPEC[3:0]) */
    uint8_t sda_spec3;      /* 1 if spec v3.0 or later */
    uint8_t sda_spec4;      /* 1 if spec v4.0 or later */
    uint8_t sda_specx;      /* spec vX extension field */
    uint8_t bus_widths;     /* SD_BUS_WIDTHS[3:0]: bit0=1-bit, bit2=4-bit */
    uint8_t cmds23_support; /* CMD23 (SET_BLOCK_COUNT) supported */
} sd_scr_t;

typedef struct
{
    uint64_t relative_address;
    uint32_t ocr_response;
    uint8_t card_type;
    uint8_t sd_spec_v2;
    uint8_t supports_switch_cmd;
    sd_scr_t scr;
} card_data_t;

typedef struct __attribute__((packed))
{
    uint8_t attribute;
    uint8_t reserved;
    uint16_t len;
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t ext;
} dma_descriptor_t;

typedef struct
{
    uint32_t cp_use_ext_lpbk_dqs;
    uint32_t cp_use_lpbk_dqs;
    uint32_t cp_use_phony_dqs;
    uint32_t cp_use_phony_dqs_cmd;
    uint32_t cp_dqs_sel_oe_end;
    uint32_t cp_sync_method;
    uint32_t cp_rd_del_sel;
    uint32_t cp_sw_half_cycle_shift;
    uint32_t cp_underrun_suppress;
    uint32_t cp_gate_cfg_always_on;
    uint32_t cp_dll_bypass_mode;
    uint32_t cp_dll_start_point;
    uint32_t cp_read_dqs_cmd_delay;
    uint32_t cp_clk_wrdqs_delay;
    uint32_t cp_clk_wr_delay;
    uint32_t cp_read_dqs_delay;
    uint32_t cp_io_mask_always_on;
    uint32_t cp_io_mask_end;
    uint32_t cp_io_mask_start;
    uint32_t cp_data_select_oe_end;
} sdmmc_phy_cfg_t;

typedef struct
{
    uint32_t sdhc_rdcmd_en;
    uint32_t sdhc_rddata_en;
    uint32_t sdhc_extended_rd_mode;
    uint32_t sdhc_extended_wr_mode;
    uint32_t sdhc_hcsdclkadj;
    uint32_t sdhc_wrcmd0_sdclk_dly;
    uint32_t sdhc_wrdata0_dly;
    uint32_t sdhc_wrcmd0_dly;
    uint32_t sdhc_rw_compensate;
} sdmmc_host_cfg_t;

/* Interrupt masks programmed to SRS13/SRS14 (status/signal enables). */
#define SDMMC_CMD_INT_MASK    (0x10001U)
#define SDMMC_XFER_INT_MASK   (0x100002U)


/**
 * @brief Configures the host for data transmission and reception.
 *
 * Configures the host based on read/write operation and the number of blocks.
 *
 * @param[in] params The command parameter instance.
 */
void sdmmc_set_xfer_config(cmd_parameters_t const *params);

/**
 * @brief Reads the Relative Card Address (RCA).
 *
 * @param[in] pxcard Instance containing card-specific data.
 */
void sd_read_response_rel_addr(card_data_t *pxcard);

/**
 * @brief Sets the DMA attributes.
 *
 * Prepares the DMA for data transmission/reception with appropriate attributes.
 *
 * @param[in] desc_buf   Pointer to dynamically allocated memory for descriptor preparation.
 * @param[in] buf        Memory buffer used for data transmission/reception.
 * @param[in] block_size Size of each block in bytes.
 * @param[in] block_ct   Number of blocks to be transferred/received.
 */
void sdmmc_set_up_xfer(void *desc_buf, uint64_t *buf, uint32_t
        block_size, uint32_t block_ct);

/**
 * @brief Checks the card type based on its capacity.
 *
 * @param[in] pxcard Reference to card-specific data.
 */
void sd_get_card_type(card_data_t *pxcard);

/**
 * @brief Configures send parameters and sends a command to the card.
 *
 * @param[in] params Reference to the parameters loaded for sending the command.
 *
 * @return
 * - 0 on successful command issue.
 * - -EIO if command/data inhibit does not clear before timeout.
 */
int32_t sdmmc_send_command(const cmd_parameters_t *params);

/**
 * @brief Check if card is present.
 *
 * @return
 * - 1 , if card is detected
 * - 0, if card is not detected
 */
uint32_t sdmmc_is_card_detected(void);

/**
 * @brief Check if card enable bit is set.
 *
 * @return
 * - 1 , if enable bit not set.
 * - 0 , if enable bit is set.
 */
uint32_t sdmmc_is_card_ready(void);

/**
 * @brief Read interrupt status register.
 *
 * @return
 *  Interrupt status.
 */
uint32_t sdmmc_get_int_status(void);

/**
 * @brief Read debug registers for timeout analysis.
 *
 * @param[out] psrs09 Pointer to SRS09 value.
 * @param[out] psrs12 Pointer to SRS12 value.
 * @param[out] psrs21 Pointer to SRS21 value.
 * @param[out] psrs22 Pointer to SRS22 value.
 */
void sdmmc_get_timeout_debug_regs(uint32_t *psrs09, uint32_t *psrs12,
        uint32_t *psrs21, uint32_t *psrs22);

/**
 * @brief Read command/transfer debug registers.
 *
 * @param[out] psrs03 Pointer to SRS03 value.
 * @param[out] psrs09 Pointer to SRS09 value.
 * @param[out] psrs12 Pointer to SRS12 value.
 */
void sdmmc_get_cmd_xfer_debug_regs(uint32_t *psrs03, uint32_t *psrs09,
        uint32_t *psrs12);

/**
 * @brief Wait until data line busy is released.
 *
 * @return
 * - 0, when data inhibit is cleared.
 * - -ETIMEDOUT, if busy state does not clear.
 */
int32_t sdmmc_wait_data_busy_clear(void);

/**
 * @brief Read 32-bit command response register.
 *
 * @return
 *  32-bit response value from SRS04.
 */
uint32_t sdmmc_read_response(void);

/**
 * @brief Read and align 136-bit command response.
 *
 * Converts the SRS04-SRS07 response register layout into a Linux-style
 * response word array (resp[0] MSW .. resp[3] LSW) with 8-bit shift applied.
 *
 * @param[out] presp Output response array with 4 words.
 */
void sdmmc_read_r2_response(uint32_t *presp);

/**
 * @brief Read CSD command class field.
 *
 * @return
 *  12-bit CSD command class field value.
 */
uint32_t sdmmc_read_csd_cmd_class(void);

/**
 * @brief Read CSD structure field.
 *
 * @return
 *  CSD_STRUCTURE field value.
 */
uint32_t sdmmc_read_csd_structure(void);

/**
 * @brief Resets all sdmmc host configurations.
 *
 * @return
 *  - 0, on reset success.
 *  - -EIO, if reset status bit does not clear within timeout.
 */
int32_t sdmmc_reset_configs(void);

/**
 * @brief  Initializes sdmmc host configuration for device type.
 * @param[in]  dev_type Device type selector (#DEV_TYPE_SD or #DEV_TYPE_EMMC).
 * @return
 *  - 0, on success.
 *  - -EINVAL, on invalid device type.
 *  - -EIO, on host programming failure.
 *  - -ETIMEDOUT, on clock stabilization timeout.
 */
int32_t sdmmc_init_configs(uint32_t dev_type);

/**
 * @brief Set host speed mode.
 *
 * @param[in] is_high_speed
 * - 0, selects default speed mode.
 * - 1, selects high speed mode.
 *
 * @return
 *  - 0, on success.
 *  - -EINVAL, if `is_high_speed` is not 0 or 1.
 *  - -ETIMEDOUT, on clock stabilization timeout.
 */
int32_t sdmmc_host_set_clock(uint32_t is_high_speed);

/**
 * @brief Update host data bus width.
 *
 * @param[in] bus_width
 * - 1, selects 1-bit mode.
 * - 4, selects 4-bit mode.
 * - 8, selects 8-bit mode.
 *
 * @return
 *  - 0, on success.
 *  - -EINVAL, if bus width is not 1, 4, or 8.
 */
int32_t sdmmc_set_data_bus_width(uint32_t bus_width);

/**
 * @brief Resets the sdmmc peripheral.
 *
 * @return
 *  - 0, on reset success.
 *  - -EIO, if reset bits do not clear within timeout.
 */
int32_t sdmmc_reset_per0(void);

/**
 * @brief Gets default PHY and host configuration values.
 *
 * @param[out] pphy_cfg  Pointer to PHY configuration structure.
 * @param[out] phost_cfg Pointer to host configuration structure.
 */
void sdmmc_get_default_phy_cfg(sdmmc_phy_cfg_t *pphy_cfg,
        sdmmc_host_cfg_t *phost_cfg);

/**
 * @brief Configures the sdmmc combo-phy using supplied settings.
 *
 * @param[in] pphy_cfg    Pointer to phy configuration structure.
 * @param[in] phost_cfg   Pointer to host configuration structure.
 *
 * @return
 *  - 0, on success
 *  - -EINVAL, if `pphy_cfg` or `phost_cfg` is NULL
 *  - -EIO, if PHY DLL reset sequence times out
 */
int32_t sdmmc_init_phy_cfg(const sdmmc_phy_cfg_t *pphy_cfg,
        const sdmmc_host_cfg_t *phost_cfg);

/**
 * @brief Clears sdmmc interrupt flags.
 *
 * @param[in] int_status Raw interrupt status value read from SRS12.
 */
void sdmmc_clear_int(uint32_t int_status);

/**
 * @brief Enable or disable SD clock output.
 *
 * @param[in] enable
 * - 0U: disable internal and SD clocks.
 * - 1U: enable internal clock, wait for stable, then enable SD clock.
 *
 * @return
 * - 0 on success.
 * - -EINVAL if `enable` is not 0 or 1.
 * - -ETIMEDOUT if internal clock does not stabilize.
 */
int32_t sdmmc_host_set_sd_clock_enable(uint32_t enable);

/**
 * @brief Enable or disable SD bus power.
 *
 * @param[in] enable
 * - 0U: disable bus power.
 * - 1U: enable bus power.
 *
 * @return
 * - 0 on success.
 * - -EINVAL if `enable` is not 0 or 1.
 */
int32_t sdmmc_set_bus_power(uint32_t enable);

/**
 * @brief Enable or disable host 1.8V signaling.
 *
 * Programs host voltage-signaling bits and waits until the state is reflected
 * in controller status.
 *
 * @param[in] enable
 * - true: switch host signaling to 1.8V.
 * - false: switch host signaling to 3.3V.
 *
 * @return
 * - 0 on success.
 * - -ETIMEDOUT if signaling state does not settle.
 */
int32_t sdmmc_host_set_1v8_signaling(bool enable);

/**
 * @brief Wait until command and data inhibit bits clear.
 *
 * @return
 * - 0 on success.
 * - -ETIMEDOUT when inhibit bits remain set.
 */
int32_t sdmmc_wait_cmd_data_busy_clear(void);

/**
 * @brief Recover command and data lines by issuing software reset bits.
 *
 * @return
 * - 0 on success.
 * - -ETIMEDOUT when reset bits do not clear.
 */
int32_t sdmmc_recover_cmd_dat_lines(void);

/**
 * @brief Program host UHS mode in SRS15[UMS].
 *
 * @param[in] uhs_mode One of #SDMMC_UHS_MODE_LEGACY,
 *                     #SDMMC_UHS_MODE_SDR12,
 *                     #SDMMC_UHS_MODE_SDR25,
 *                     #SDMMC_UHS_MODE_SDR50.
 *
 * @return
 * - 0 on success.
 * - -EINVAL on invalid mode.
 */
int32_t sdmmc_host_set_uhs_mode(uint32_t uhs_mode);

/**
 * @brief Program SD clock divider for UHS/legacy mode transitions.
 *
 * @param[in] freq_sel Divider field value for SRS11[SDCFSL].
 *
 * @return
 * - 0 on success.
 * - -EINVAL if `freq_sel` exceeds 8-bit divider field range.
 * - -ETIMEDOUT if internal clock does not stabilize.
 */
int32_t sdmmc_host_set_uhs_clock(uint32_t freq_sel);

/**
 * @brief Check whether DAT0 indicates card busy.
 *
 * @return
 * - true when DAT0 is low (card busy).
 * - false when DAT0 is high (card ready).
 */
bool sdmmc_is_card_busy_dat0(void);

/**
 * @brief Configure pinmux for SDIO voltage regulator GPIO pin.
 */
void sdmmc_host_configure_volt_reg_gpio_pinmux(void);

#endif /*__SOCFPGA_SDMMC_LL_H__*/
