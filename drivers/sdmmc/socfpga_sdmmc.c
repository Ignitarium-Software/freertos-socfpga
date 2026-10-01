/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for SDMMC
 */

/*
 * This is the implementation of the SDMMC/eMMC driver. It supports both the
 * SD card and eMMC device. The driver can be used directly or via the FAT
 * file system. The typical usage is via FAT file system.
 *
 * The below diagram shows how this driver interacts with the file system
 * and the hardware.
 * +==========================================================================+
 * |                         Application Layer                                |
 * |                  (FF_Write, FF_Read, FF_Open, etc.)                      |
 * +==========================================================================+
 *                                 |
 *                                 v
 * +==========================================================================+
 * |                          FATFS Stack                                     |
 * +==========================================================================+
 *                                 |
 *                                 v
 * +==========================================================================+
 * |                   Portable Layer (ff_sddisk.c)                           |
 * +==========================================================================+
 *                                 |
 *                                 v
 * +==========================================================================+
 * |                                                                          |
 * |  +====================================================================+  |
 * |  |                                                                    |  |
 * |  |                       +----------------------+                     |  |
 * |  |                       |      SDMMC HAL       |                     |  |
 * |  |                       +----------------------+                     |  |
 * |  |  +----------------+   +---------------------+   +----------------+ |  |
 * |  |  | DMA and Buffer |   |  SDMMC Low Level    |   |   SDMMC PHY    | |  |
 * |  |  |  Management    |   |      Layer          |   |                | |  |
 * |  |  +----------------+   +---------------------+   +----------------+ |  |
 * |  +====================================================================+  |
 * +==========================================================================+
 *
 * The file ff_sddisk.c serves as the portable layer and translates the FAT
 * file system requests to driver calls. The HAL layer in the driver implements
 * the driver APIs, which can be used by the porting layer or can be invoked
 * directly if used with no file system.
 *
 * The low level driver implements the low level register sequences for the
 * SDMMC controller. And the combo phy module implements the low level register
 * configurations for the combo phy
 */

#include <stdint.h>
#include <errno.h>
#include "osal.h"
#include "osal_log.h"
#include "socfpga_cache.h"
#include "socfpga_gpio.h"
#include "socfpga_gpio_reg.h"
#include "socfpga_interrupt.h"
#include "socfpga_sdmmc_ll.h"
#include "socfpga_sdmmc.h"

#define SCR_SPEC_VER_2    (2U)
#define SDMMC_R1_APP_CMD_MASK   (1U << 5U)
#define SDMMC_INIT_STATE_NOT_INIT   (0U)
#define SDMMC_INIT_STATE_INIT_DONE  (1U)

#if SDMMC_MAX_DESCRIPTOR < 1
#error Invalid descriptor count
#endif

#if (DEV_TYPE == DEV_TYPE_SD)
#define BUS_PARAM    0
static int32_t sd_mmc_init(uint64_t *sec_num);
static int32_t sd_go_idle(cmd_parameters_t *pcmd);
static int32_t sd_send_if_cond(cmd_parameters_t *pcmd);
static int32_t sd_send_app_cmd(cmd_parameters_t *pcmd, uint32_t argument);
static int32_t sd_check_ocr(cmd_parameters_t *pcmd, uint32_t ocr_arg);
static int32_t sd_send_all_cid(cmd_parameters_t *pcmd);
static int32_t sd_send_rel_add(cmd_parameters_t *pcmd);
static int32_t sd_sel_card(cmd_parameters_t *pcmd);
static int32_t sd_send_csd(cmd_parameters_t *pcmd);
static int32_t sd_en_card_ready(cmd_parameters_t *pcmd_handle, uint32_t ocr_arg);
static int32_t sd_set_bus_width(cmd_parameters_t *pcmd_handle, uint32_t bus_width);
static int32_t sd_cmd_switch_high_speed(cmd_parameters_t *pcmd);
static int32_t sd_cmd_switch_uhs_mode(cmd_parameters_t *pcmd, uint8_t mode);
static int32_t sd_cmd_switch_current_limit(cmd_parameters_t *pcmd);
static int32_t sd_check_if_cond_version(void);
static void sd_check_switch_command_capability(void);
static int32_t sd_read_scr(cmd_parameters_t *pcmd);
static int32_t sd_try_uhs_mode(cmd_parameters_t *pcmd, uint8_t mode);
static int32_t sd_cmd_voltage_switch(cmd_parameters_t *pcmd);
static int32_t sd_switch_host_to_1v8(void);
static void sd_fallback_to_hs_mode(void);
static int32_t sd_toggle_volt_reg_sdio(bool is_1v8);
static int32_t sd_recover_after_lvs_fail(void);
static uint32_t sd_build_acmd41_ocr_arg(bool is_sdv2, bool request_s18r);
static int32_t sd_read_switch_caps(cmd_parameters_t *pcmd);
static int32_t sd_execute_voltage_switch(cmd_parameters_t *pcmd);
static int32_t sd_negotiate_voltage(cmd_parameters_t *pcmd, bool *uhs_ok);
static int32_t sd_identify_card(cmd_parameters_t *pcmd, uint64_t *sec_num);
static int32_t sd_negotiate_speed(cmd_parameters_t *pcmd, bool uhs_ok);
static uint32_t sd_extract_resp_bits(const uint32_t *resp, uint32_t start,
        uint32_t size);
static int32_t sd_parse_scr(const uint32_t *resp);
static uint64_t sdmmc_read_sector_count(void);

static uint8_t switch_status[SDMMC_SWITCH_STATUS_SIZE] = {0};
static uint8_t scr_status[8] = {0};
static gpio_handle_t sdio_sel_gpio = 0;

#elif (DEV_TYPE == DEV_TYPE_EMMC)
#define BUS_PARAM    1
static int32_t sd_mmc_init(uint64_t *sec_num);
static int32_t mmc_go_idle(cmd_parameters_t *pcmd);
static int32_t mmc_send_all_cid(cmd_parameters_t *pcmd);
static int32_t mmc_set_rel_add(cmd_parameters_t *pcmd);
static int32_t mmc_check_ocr(cmd_parameters_t *pcmd);
static int32_t mmc_sel_card(cmd_parameters_t *pcmd);
static int32_t mmc_send_csd(cmd_parameters_t *pcmd);
static int32_t mmc_send_ext_csd(cmd_parameters_t *pcmd,
        uint64_t *sector_count_ref);
static int32_t mmc_switch_bus_width(cmd_parameters_t *pcmd);

static uint8_t ext_csd_buff[512];

#else
#error "Invalid device/device not supported"
#endif

static int32_t sdmmc_setup_host(void);
static void sdmmc_wait_xfer_done(void);
static void sdmmc_wait_cmd_done(void);
void sdmmc_irq_handler(void *data);
static card_data_t *pcard_specific_data;
static card_data_t card_data;

struct sdmmc_context
{
    bool is_api_sync;
    int32_t status_code;
    osal_semaphore_def_t xfer_sem_def;
    osal_semaphore_def_t cmd_sem_def;
    osal_semaphore_t xfer_sem;
    osal_semaphore_t cmd_sem;
    sdmmc_cb_fun xfer_call_back;
    dma_descriptor_t dma_desc_buf[SDMMC_MAX_DESCRIPTOR];
    uint32_t dev_type;
    uint32_t host_hs_supported;
    uint32_t host_uhs_caps;
    uint32_t card_caps_valid;
    uint32_t card_uhs_caps;
    sdmmc_phy_cfg_t phy_cfg;
    sdmmc_host_cfg_t host_cfg;
    sdmmc_speed_mode_t speed_mode;
    uint32_t init_state;
    uint64_t sector_count;
};

static struct sdmmc_context sdmmc_descriptor = {0};

int32_t sdmmc_read_block_async(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks,
        sdmmc_cb_fun xfer_done_call_back)
{
    cmd_parameters_t *pcmd;
    cmd_parameters_t command_config;
    pcmd = &command_config;
    uint32_t req_desc_count;
    int32_t state;
    sdmmc_descriptor.status_code = 0;

    sdmmc_descriptor.is_api_sync = false;
    sdmmc_descriptor.xfer_call_back = xfer_done_call_back;

    if (block_size == 0U)
    {
        return -EINVAL;
    }

    if (addr % block_size == 0)
    {
        addr /= block_size;
    }
    else
    {
        ERROR("Address is not aligned to block size");
        return -EINVAL;
    }

    if ((buf == NULL) || (nblocks == 0U))
    {
        return -EINVAL;
    }

    pcmd->argument = block_size;
    pcmd->command_index = SDMMC_CMD_SET_BLOCK_LEN;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);

    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_cmd_done();

    if (sdmmc_descriptor.status_code != 0)
    {
        return -EIO;
    }

    pcmd->argument = addr;
    pcmd->command_index = (nblocks > 1U) ?
        SDMMC_CMD_READ_MULT_BLOCK : SDMMC_CMD_READ_SINGLE_BLOCK;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;

    req_desc_count = ((block_size * nblocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;

    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, buf,
            block_size, nblocks);
    sdmmc_set_xfer_config(pcmd);
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }

    return 0;
}

int32_t sdmmc_read_block_sync(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks)
{
    cmd_parameters_t *pcmd;
    cmd_parameters_t command_config;
    pcmd = &command_config;
    uint32_t req_desc_count;
    int32_t state;

    sdmmc_descriptor.is_api_sync = true;
    if (block_size == 0U)
    {
        return -EINVAL;
    }

    if (addr % block_size == 0)
    {
        addr /= block_size;
    }
    else
    {
        ERROR("Address is not aligned to block size");
        return -EINVAL;
    }

    if ((buf == NULL) || (nblocks == 0U))
    {
        return -EINVAL;
    }

    pcmd->argument = block_size;
    pcmd->command_index = SDMMC_CMD_SET_BLOCK_LEN;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);

    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_cmd_done();

    if (sdmmc_descriptor.status_code != 0)
    {
        return -EIO;
    }

    pcmd->argument = addr;
    pcmd->command_index = (nblocks > 1U) ?
        SDMMC_CMD_READ_MULT_BLOCK : SDMMC_CMD_READ_SINGLE_BLOCK;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;

    req_desc_count = ((block_size * nblocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;

    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, buf,
            block_size, nblocks);
    sdmmc_set_xfer_config(pcmd);
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_xfer_done();

    if (sdmmc_descriptor.status_code == 0)
    {
        cache_force_invalidate((uint64_t *)buf, block_size *
                nblocks);
        INFO("Read %x blocks", nblocks);
        return 0;
    }
    else
    {
        return -EIO;
    }
}

int32_t sdmmc_write_block_async(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks,
        sdmmc_cb_fun xfer_done_call_back)
{
    cmd_parameters_t *pcmd;
    cmd_parameters_t command_config;
    pcmd = &command_config;
    uint32_t req_desc_count;
    int32_t state;

    sdmmc_descriptor.is_api_sync = false;
    sdmmc_descriptor.xfer_call_back = xfer_done_call_back;

    if (block_size == 0U)
    {
        return -EINVAL;
    }

    if (addr % block_size == 0)
    {
        addr /= block_size;
    }
    else
    {
        ERROR("Address is not aligned to block size");
        return -EINVAL;
    }

    if ((buf == NULL) || (nblocks == 0U))
    {
        return -EINVAL;
    }

    pcmd->argument = block_size;
    pcmd->command_index = SDMMC_CMD_SET_BLOCK_LEN;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);

    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_cmd_done();

    if (sdmmc_descriptor.status_code != 0)
    {
        return -EIO;
    }

    pcmd->argument = addr;
    pcmd->command_index = (nblocks > 1U) ?
        SDMMC_CMD_WRITE_MULT_BLOCK : SDMMC_CMD_WRITE_SINGLE_BLOCK;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;

    req_desc_count = ((block_size * nblocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;
    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    cache_force_write_back((uint64_t *)buf, block_size *
            nblocks);
    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, buf,
            block_size, nblocks);
    sdmmc_set_xfer_config(pcmd);

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }

    return 0;
}

int32_t sdmmc_write_block_sync(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks)
{
    cmd_parameters_t *pcmd;
    cmd_parameters_t command_config;
    pcmd = &command_config;
    uint32_t req_desc_count;
    int32_t state;

    sdmmc_descriptor.is_api_sync = true;
    if (block_size == 0U)
    {
        return -EINVAL;
    }

    if (addr % block_size == 0)
    {
        addr /= block_size;
    }
    else
    {
        ERROR("Address is not aligned to block size");
        return -EINVAL;
    }

    if ((buf == NULL) || (nblocks == 0U))
    {
        return -EINVAL;
    }

    pcmd->argument = block_size;
    pcmd->command_index = SDMMC_CMD_SET_BLOCK_LEN;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);

    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_cmd_done();

    if (sdmmc_descriptor.status_code != 0)
    {
        return -EIO;
    }

    pcmd->argument = addr;
    pcmd->command_index = (nblocks > 1U) ?
        SDMMC_CMD_WRITE_MULT_BLOCK : SDMMC_CMD_WRITE_SINGLE_BLOCK;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;

    req_desc_count = ((block_size * nblocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;

    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    cache_force_write_back((uint64_t *)buf, block_size *
            nblocks);
    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, buf,
            block_size, nblocks);
    sdmmc_set_xfer_config(pcmd);

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_xfer_done();

    if (sdmmc_descriptor.status_code == 0)
    {
        INFO("Written %x blocks", nblocks);
        return 0;
    }
    else
    {
        return -EIO;
    }
}


int32_t sdmmc_init_card(uint64_t *n_sector)
{
    int32_t ret;
    int32_t cmd_status;
    int err;

    pcard_specific_data = &card_data;

    if (n_sector == NULL)
    {
        return -EINVAL;
    }

    if (sdmmc_descriptor.init_state == SDMMC_INIT_STATE_INIT_DONE)
    {
        *n_sector = sdmmc_descriptor.sector_count;
        INFO("SDMMC: card already initialized");
        return 0;
    }

    if (sdmmc_is_card_present() != SDMMC_IS_CARD_DET)
    {
        ERROR("Device detection failed");
        return -EIO;
    }

    sdmmc_descriptor.dev_type = DEV_TYPE;
    sdmmc_descriptor.host_hs_supported = SDMMC_HOST_HS_SUPPORTED;
    sdmmc_descriptor.host_uhs_caps = SDMMC_HOST_UHS_MODE_CAPS;
    sdmmc_descriptor.card_caps_valid = 0U;
    sdmmc_descriptor.card_uhs_caps = 0U;
    sdmmc_descriptor.speed_mode = SDMMC_SPEED_MODE_DS;
    pcard_specific_data->sd_spec_v2 = 0U;
    pcard_specific_data->supports_switch_cmd = 0U;
    pcard_specific_data->scr = (sd_scr_t){0};
    pcard_specific_data->ocr_response = 0U;
    sdmmc_get_default_phy_cfg(&sdmmc_descriptor.phy_cfg,
            &sdmmc_descriptor.host_cfg);

    sdmmc_descriptor.xfer_sem =
            osal_semaphore_create(&sdmmc_descriptor.xfer_sem_def);
    sdmmc_descriptor.cmd_sem =
            osal_semaphore_create(&sdmmc_descriptor.cmd_sem_def);
    if ((sdmmc_descriptor.xfer_sem == NULL) ||
                (sdmmc_descriptor.cmd_sem == NULL))
    {
        ERROR("Semaphore creation failed");
        return -ENOMEM;
    }
    err = interrupt_register_isr(SDMMC_IRQ, sdmmc_irq_handler, NULL);
    if (err != 0)
    {
        ERROR("ISR registration failed");
        return -EIO;
    }
    err = interrupt_enable(SDMMC_IRQ, GIC_INTERRUPT_PRIORITY_SDMMC);
    if (err != 0)
    {
        ERROR("IRQ enable failed");
        return -EIO;
    }

    ret = sdmmc_setup_host();

    if (ret != 0)
    {
        ERROR("Host setup failed");
        return -EIO;
    }

    cmd_status = sd_mmc_init(n_sector);

    if (cmd_status == 0)
    {
        sdmmc_descriptor.init_state = SDMMC_INIT_STATE_INIT_DONE;
        sdmmc_descriptor.sector_count = *n_sector;
        INFO("SDMMC: initialized at %s",
                (sdmmc_descriptor.speed_mode == SDMMC_SPEED_MODE_UHS_SDR50) ? "SDR50 (100 MHz, 1.8V)" :
                (sdmmc_descriptor.speed_mode == SDMMC_SPEED_MODE_UHS_SDR25) ? "SDR25 (50 MHz, 1.8V)"  :
                (sdmmc_descriptor.speed_mode == SDMMC_SPEED_MODE_UHS_SDR12) ? "SDR12 (25 MHz, 1.8V)"  :
                (sdmmc_descriptor.speed_mode == SDMMC_SPEED_MODE_HS)        ? "High Speed (50 MHz)"    :
                "Default Speed (25 MHz)");
        return 0;
    }
    else
    {
        sdmmc_descriptor.init_state = SDMMC_INIT_STATE_NOT_INIT;
        ERROR("Card initialization failed");
        return -EIO;
    }
}
#if (DEV_TYPE ==  DEV_TYPE_SD)
/**
 * @brief Execute CMD11 voltage switch handshake; check DAT0 busy before
 *        and after host switch to 1.8V.
 * @param[in] pcmd Command parameter instance used for CMD11.
 * @return
 * - 0 on success.
 * - -EIO on command or host-controller failure.
 */
static int32_t sd_execute_voltage_switch(cmd_parameters_t *pcmd)
{
    int32_t state;

    state = sd_cmd_voltage_switch(pcmd);
    if (state != 0)
    {
        return state;
    }

    /* Give the card time to pull DAT0 low after the CMD11 response before polling it. */
    osal_task_delay(1);
    if (sdmmc_is_card_busy_dat0() == false)
    {
        ERROR("Card did not drive DAT0 low after CMD11");
        return -EIO;
    }

    state = sd_switch_host_to_1v8();
    if (state != 0)
    {
        return state;
    }

    /* Give the card time to release DAT0 after the host's 1.8V switch completes. */
    osal_task_delay(1);
    if (sdmmc_is_card_busy_dat0() == true)
    {
        ERROR("Card kept DAT0 low after 1.8V switch");
        return -EIO;
    }

    return 0;
}

/**
 * @brief Run CMD0/CMD8/ACMD41 negotiation loop; request 1.8V if host
 *        supports UHS, perform voltage switch on acceptance.
 * @param[in]  pcmd   Command parameter instance.
 * @param[out] uhs_ok Set true if the 1.8V switch succeeded.
 * @return
 * - 0 on success.
 * - -EIO on command or host-controller failure.
 */
static int32_t sd_negotiate_voltage(cmd_parameters_t *pcmd, bool *uhs_ok)
{
    int32_t state;
    uint32_t ocr_arg;
    int32_t uhs_retries;
    bool request_s18r;
    bool is_sd_spec_v2;

    *uhs_ok = false;

    if (sd_toggle_volt_reg_sdio(false) != 0)
    {
        INFO("Unable to force IO rail to 3.3V at start");
    }

    state = sd_go_idle(pcmd);
    if (state != 0)
    {
        ERROR("CMD0 GO_IDLE failed");
        return -EIO;
    }
    /* Let the card complete the internal reset that CMD0 (GO_IDLE_STATE) triggers. */
    osal_task_delay(10);

    request_s18r = (sdmmc_descriptor.host_uhs_caps != 0U);
    uhs_retries = SDMMC_UHS_SWITCH_RETRY_COUNT;

    /*
     * Per SD Spec Section 3.6/4.2.3.1: bid for 1.8V via S18R in ACMD41; if the
     * card echoes S18A, immediately issue CMD11 to switch voltage. On switch
     * failure, power-cycle the card and restart from CMD0 (its state is
     * otherwise undefined). After SDMMC_UHS_SWITCH_RETRY_COUNT such failures,
     * S18R is dropped and the card falls back to running at 3.3V.
     */
    while (1)
    {
        pcard_specific_data->sd_spec_v2 = 0U;
        (void)sd_send_if_cond(pcmd);
        (void)sd_check_if_cond_version();
        is_sd_spec_v2 = pcard_specific_data->sd_spec_v2;
        ocr_arg = sd_build_acmd41_ocr_arg(is_sd_spec_v2, request_s18r);

        state = sd_en_card_ready(pcmd, ocr_arg);
        if (state != 0)
        {
            ERROR("ACMD41 card ready failed");
            return -EIO;
        }

        if ((request_s18r == false) || ((ocr_arg & SDMMC_OCR_S18R_MASK) == 0U) ||
                ((pcard_specific_data->ocr_response & SDMMC_OCR_S18A_MASK) == 0U))
        {
            INFO("Card operating at 3.3V");
            return 0;
        }

        INFO("Card accepts 1.8V, starting voltage switch");
        state = sd_execute_voltage_switch(pcmd);
        if (state == 0)
        {
            INFO("1.8V switch successful");
            *uhs_ok = true;
            return 0;
        }

        ERROR("CMD11/1.8V switch failed, power cycling card");
        state = sd_recover_after_lvs_fail();
        if (state != 0)
        {
            ERROR("Power cycle recovery failed");
            return -EIO;
        }

        if (uhs_retries > 0)
        {
            uhs_retries--;
        }
        else
        {
            request_s18r = false;
        }

        state = sd_go_idle(pcmd);
        if (state != 0)
        {
            ERROR("CMD0 GO_IDLE failed during UHS retry");
            return -EIO;
        }
        /* Let the card complete the internal reset that CMD0 (GO_IDLE_STATE) triggers. */
        osal_task_delay(10);
    }
}

/**
 * @brief Extract bit-field value from a 128-bit response array.
 * @param[in] resp  Response words array.
 * @param[in] start Start bit index.
 * @param[in] size  Bit-field width.
 * @return Extracted field value.
 */
static uint32_t sd_extract_resp_bits(const uint32_t *resp, uint32_t start,
        uint32_t size)
{
    uint32_t offset;
    uint32_t shift;
    uint32_t value;
    uint32_t mask;

    if ((resp == NULL) || (size == 0U) || (size > 32U))
    {
        return 0U;
    }

    /*
     * The SDMMC host controller reports long responses (CID/CSD/SCR) across
     * four 32-bit response words, with resp[3] holding the most significant
     * bits and resp[0] the least significant bits - the reverse of the
     * increasing bit-index numbering used by the SD/eMMC physical layer spec.
     * Converting a spec bit index therefore inverts the word index
     * (3 - start / 32) while the intra-word shift stays start % 32.
     */
    offset = 3U - (start / 32U);
    shift = start & 31U;

    value = resp[offset] >> shift;
    if ((size + shift) > 32U)
    {
        value |= resp[offset - 1U] << (32U - shift);
    }

    if (size == 32U)
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
 * @brief Parse SCR response fields and update card SCR data structure.
 * @param[in] resp SCR response packed in response words.
 * @return
 * - 0 on success.
 * - -EIO when SCR content is invalid.
 */
static int32_t sd_parse_scr(const uint32_t *resp)
{
    uint32_t scr_struct;

    scr_struct = sd_extract_resp_bits(resp, 60U, 4U);
    if (scr_struct != 0U)
    {
        ERROR("ACMD51 unrecognised SCR structure version %u", scr_struct);
        return -EIO;
    }

    pcard_specific_data->scr.sda_vsn =
            (uint8_t)sd_extract_resp_bits(resp, 56U, 4U);
    pcard_specific_data->scr.bus_widths =
            (uint8_t)sd_extract_resp_bits(resp, 48U, 4U);
    pcard_specific_data->scr.sda_spec3 = 0U;
    pcard_specific_data->scr.sda_spec4 = 0U;
    pcard_specific_data->scr.sda_specx = 0U;
    pcard_specific_data->scr.cmds23_support = 0U;

    if (pcard_specific_data->scr.sda_vsn == SCR_SPEC_VER_2)
    {
        pcard_specific_data->scr.sda_spec3 =
                (uint8_t)sd_extract_resp_bits(resp, 47U, 1U);
    }

    if (pcard_specific_data->scr.sda_spec3 != 0U)
    {
        pcard_specific_data->scr.sda_spec4 =
                (uint8_t)sd_extract_resp_bits(resp, 42U, 1U);
        pcard_specific_data->scr.sda_specx =
                (uint8_t)sd_extract_resp_bits(resp, 38U, 4U);
        pcard_specific_data->scr.cmds23_support =
                (uint8_t)sd_extract_resp_bits(resp, 33U, 1U);
    }

    return 0;
}

/**
 * @brief Calculate sector count from the currently latched CSD response.
 * @return Sector count for the card.
 */
static uint64_t sdmmc_read_sector_count(void)
{
    uint32_t resp[4] = {0U};
    uint64_t c_size;
    uint64_t card_size;

    sdmmc_read_r2_response(resp);

    c_size = sd_extract_resp_bits(resp, 48U, 22U);
    card_size = (c_size + 1UL) * SDMMC_BLOCK_SIZE * 1024UL;

    return card_size / SDMMC_BLOCK_SIZE;
}

/**
 * @brief Identify the card via CMD2/CMD3/CMD9/CMD7; read SCR, set
 *        bus width, default speed, and optionally read CMD6 capabilities.
 * @param[in]  pcmd    Command parameter instance.
 * @param[out] sec_num Pointer to store the card sector count.
 * @return
 * - 0 on success.
 * - -EIO on command/response or host-controller failure.
 */
static int32_t sd_identify_card(cmd_parameters_t *pcmd, uint64_t *sec_num)
{
    int32_t state;

    sd_get_card_type(pcard_specific_data);

    state = sd_send_all_cid(pcmd);
    if (state != 0)
    {
        ERROR("CMD2 ALL_SEND_CID failed");
        return -EIO;
    }

    state = sd_send_rel_add(pcmd);
    if (state != 0)
    {
        ERROR("CMD3 SEND_REL_ADDR failed");
        return -EIO;
    }
    sd_read_response_rel_addr(pcard_specific_data);

    state = sd_send_csd(pcmd);
    if (state != 0)
    {
        ERROR("CMD9 SEND_CSD failed");
        return -EIO;
    }
    sd_check_switch_command_capability();
    *sec_num = sdmmc_read_sector_count();

    state = sd_sel_card(pcmd);
    if (state != 0)
    {
        ERROR("CMD7 SELECT_CARD failed");
        return -EIO;
    }

    state = sd_read_scr(pcmd);
    if (state != 0)
    {
        ERROR("Failed to read SCR");
        return -EIO;
    }

    state = sd_set_bus_width(pcmd, SDMMC_BUS_WIDTH_4);
    if (state != 0)
    {
        ERROR("Failed to set bus width");
        return -EIO;
    }

    state = sdmmc_host_set_clock(SDMMC_HOST_CLK_FREQ_DS);
    if (state != 0)
    {
        ERROR("Failed to set host default speed clock");
        return -EIO;
    }

    if (pcard_specific_data->supports_switch_cmd != 0U)
    {
        state = sd_read_switch_caps(pcmd);
        if (state != 0)
        {
            ERROR("SDMMC: failed to read switch capabilities, UHS unavailable");
        }
    }

    return 0;
}

/**
 * @brief Select best speed mode: UHS (SDR50/SDR25/SDR12) if voltage was
 *        switched, then HS, then DS.
 * @param[in] pcmd   Command parameter instance.
 * @param[in] uhs_ok True if the 1.8V switch succeeded and UHS may be tried.
 * @return
 * - 0 on success.
 * - -EOPNOTSUPP if no compatible UHS/HS mode can be selected.
 * - -EINVAL if an unsupported mode selector is provided.
 * - -EIO on host clock programming failure.
 */
static int32_t sd_negotiate_speed(cmd_parameters_t *pcmd, bool uhs_ok)
{
    uint32_t common_caps;
    int32_t state;

    if ((uhs_ok == true) && (sdmmc_descriptor.card_caps_valid != 0U))
    {
        common_caps = sdmmc_descriptor.host_uhs_caps & sdmmc_descriptor.card_uhs_caps;
        if ((common_caps & SDMMC_SWITCH_SUP_SDR50) != 0U)
        {
            return sd_try_uhs_mode(pcmd, SDMMC_SWITCH_GRP1_SDR50);
        }
        else if ((common_caps & SDMMC_SWITCH_SUP_SDR25) != 0U)
        {
            return sd_try_uhs_mode(pcmd, SDMMC_SWITCH_GRP1_SDR25);
        }
        else if ((common_caps & SDMMC_SWITCH_SUP_SDR12) != 0U)
        {
            return sd_try_uhs_mode(pcmd, SDMMC_SWITCH_GRP1_SDR12);
        }
        INFO("No common UHS caps, fallback to HS");
    }

    state = sd_cmd_switch_high_speed(pcmd);
    if (state == 0)
    {
        sdmmc_descriptor.speed_mode = SDMMC_SPEED_MODE_HS;
        return 0;
    }

    state = sdmmc_host_set_clock(SDMMC_HOST_CLK_FREQ_DS);
    if (state != 0)
    {
        ERROR("Failed to set host default speed clock");
        return -EIO;
    }
    return 0;
}

/**
 * @brief Top-level SD card initialization; calls voltage negotiation, card
 *        identification, and speed negotiation.
 * @param[in] sec_num Pointer to store the card sector count.
 * @return
 * - 0 on success.
 * - -EIO on command, transfer, or host-programming failure.
 */
static int32_t sd_mmc_init(uint64_t *sec_num)
{
    cmd_parameters_t command_config;
    cmd_parameters_t *pcmd_handle = &command_config;
    bool uhs_ok;
    int32_t state;

    state = sd_negotiate_voltage(pcmd_handle, &uhs_ok);
    if (state != 0)
    {
        return -EIO;
    }

    if (sd_identify_card(pcmd_handle, sec_num) != 0)
    {
        return -EIO;
    }

    return sd_negotiate_speed(pcmd_handle, uhs_ok);
}

/**
 * @brief Issue CMD6 to switch the card current limit to 200mA if host and
 *        card support it.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EOPNOTSUPP if card/host does not support requested current limit.
 * - -EIO on command/transfer failure.
 */
static int32_t sd_cmd_switch_current_limit(cmd_parameters_t *pcmd)
{
    int32_t state;
    uint16_t curr_caps;

    if (pcard_specific_data->supports_switch_cmd == 0U)
    {
        return -EOPNOTSUPP;
    }

    state = sd_read_switch_caps(pcmd);
    if (state != 0)
    {
        return state;
    }

    curr_caps = (uint16_t)switch_status[7] | ((uint16_t)switch_status[6] << 8);

    if ((SDMMC_HOST_MAX_CURRENT_MA < 200U) ||
            ((curr_caps & SDMMC_SWITCH_CURR_200MA) == 0U))
    {
        return 0;
    }

    pcmd->argument = SDMMC_ARG_SWITCH_SET_GRP3_200MA;
    pcmd->command_index = SDMMC_CMD_SWITCH;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    sdmmc_descriptor.is_api_sync = true;

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, (uint64_t *)switch_status,
            SDMMC_SWITCH_STATUS_SIZE, SDMMC_SINGLE_BLOCK);
    sdmmc_set_xfer_config(pcmd);

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_xfer_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        return -EIO;
    }

    cache_force_invalidate((uint64_t *)switch_status, SDMMC_SWITCH_STATUS_SIZE);
    if (((switch_status[15] >> 4U) & 0x0FU) != SDMMC_SWITCH_GRP3_200MA)
    {
        return 0;
    }

    return 0;
}

/**
 * @brief Configure host and card for the given UHS mode (SDR50/SDR25/SDR12)
 *        via CMD6 and PHY update.
 * @param[in] pcmd Command parameter instance.
 * @param[in] mode Target UHS group1 mode value.
 * @return
 * - 0 on success.
 * - -EOPNOTSUPP if card/host does not accept requested UHS/current mode.
 * - -EINVAL if unsupported UHS mode is requested.
 */
static int32_t sd_try_uhs_mode(cmd_parameters_t *pcmd, uint8_t mode)
{
    int32_t state;
    uint32_t freq_sel;

    state = sd_cmd_switch_current_limit(pcmd);
    if (state != 0)
    {
        ERROR("Current-limit switch failed (state=%d), fallback to HS",
                state);
        sd_fallback_to_hs_mode();
        return -EOPNOTSUPP;
    }

    state = sd_cmd_switch_uhs_mode(pcmd, mode);
    if (state != 0)
    {
        ERROR("CMD6 UHS mode switch failed (state=%d), fallback to HS",
                state);
        sd_fallback_to_hs_mode();
        return -EOPNOTSUPP;
    }

    if (mode == SDMMC_SWITCH_GRP1_SDR50)
    {
        state = sdmmc_host_set_uhs_mode(SDMMC_UHS_MODE_SDR50);
        freq_sel = SDMMC_SDR50_FREQ_SEL;
    }
    else if (mode == SDMMC_SWITCH_GRP1_SDR25)
    {
        state = sdmmc_host_set_uhs_mode(SDMMC_UHS_MODE_SDR25);
        freq_sel = SDMMC_SDR25_FREQ_SEL;
    }
    else if (mode == SDMMC_SWITCH_GRP1_SDR12)
    {
        state = sdmmc_host_set_uhs_mode(SDMMC_UHS_MODE_SDR12);
        freq_sel = SDMMC_SDR12_FREQ_SEL;
    }
    else
    {
        return -EINVAL;
    }

    if (state != 0)
    {
        ERROR("Host UMS update failed (state=%d), fallback to HS",
                state);
        sd_fallback_to_hs_mode();
        return -EOPNOTSUPP;
    }

    state = sdmmc_host_set_uhs_clock(freq_sel);
    if (state != 0)
    {
        ERROR("UHS clock program failed (state=%d), fallback to HS",
                state);
        sd_fallback_to_hs_mode();
        return -EOPNOTSUPP;
    }

    if (mode == SDMMC_SWITCH_GRP1_SDR50)
    {
        sdmmc_descriptor.speed_mode = SDMMC_SPEED_MODE_UHS_SDR50;
    }
    else if (mode == SDMMC_SWITCH_GRP1_SDR25)
    {
        sdmmc_descriptor.speed_mode = SDMMC_SPEED_MODE_UHS_SDR25;
    }
    else if (mode == SDMMC_SWITCH_GRP1_SDR12)
    {
        sdmmc_descriptor.speed_mode = SDMMC_SPEED_MODE_UHS_SDR12;
    }
    return 0;
}

/**
 * @brief Build the OCR argument for ACMD41 based on host capabilities and
 *        SD v2 support.
 * @param[in] is_sdv2      True if the card reported SD spec v2 or later.
 * @param[in] request_s18r True to request 1.8V signaling (S18R).
 * @return The assembled ACMD41 OCR argument.
 */
static uint32_t sd_build_acmd41_ocr_arg(bool is_sdv2, bool request_s18r)
{
    uint32_t ocr_arg;

    ocr_arg = SDMMC_ARG_SDHC_OCR & ~(SDMMC_OCR_CCS_MASK |
            SDMMC_OCR_2T_MASK |
            SDMMC_OCR_S18R_MASK |
            SDMMC_OCR_XPC_MASK);

    if (is_sdv2 == true)
    {
        ocr_arg |= SDMMC_OCR_CCS_MASK | SDMMC_OCR_2T_MASK;
    }

    if ((request_s18r == true) &&
            (sdmmc_descriptor.host_uhs_caps != 0U))
    {
        ocr_arg |= SDMMC_OCR_S18R_MASK;
    }

    if (SDMMC_HOST_MAX_CURRENT_MA > 150U)
    {
        ocr_arg |= SDMMC_OCR_XPC_MASK;
    }

    return ocr_arg;
}

/**
 * @brief Issue CMD6 check-function (mode 0) and update card UHS capability
 *        fields from the response.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on CMD6 send/transfer failure.
 */
static int32_t sd_read_switch_caps(cmd_parameters_t *pcmd)
{
    int32_t state;

    pcmd->argument = SDMMC_ARG_SWITCH_CHECK_GRP1;
    pcmd->command_index = SDMMC_CMD_SWITCH;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    sdmmc_descriptor.is_api_sync = true;

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, (uint64_t *)switch_status,
            SDMMC_SWITCH_STATUS_SIZE, SDMMC_SINGLE_BLOCK);
    sdmmc_set_xfer_config(pcmd);

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }

    sdmmc_wait_xfer_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        return -EIO;
    }

    cache_force_invalidate((uint64_t *)switch_status, SDMMC_SWITCH_STATUS_SIZE);

    sdmmc_descriptor.card_caps_valid = 1U;
    sdmmc_descriptor.card_uhs_caps = switch_status[SDMMC_SWITCH_GRP1_SUP_IDX] &
            (SDMMC_SWITCH_SUP_SDR50 | SDMMC_SWITCH_SUP_SDR25 | SDMMC_SWITCH_SUP_SDR12);

    if ((sdmmc_descriptor.card_uhs_caps & SDMMC_SWITCH_SUP_SDR50) != 0U)
    {
        sdmmc_descriptor.card_uhs_caps |= SDMMC_SWITCH_SUP_SDR25 | SDMMC_SWITCH_SUP_SDR12;
    }
    else if ((sdmmc_descriptor.card_uhs_caps & SDMMC_SWITCH_SUP_SDR25) != 0U)
    {
        sdmmc_descriptor.card_uhs_caps |= SDMMC_SWITCH_SUP_SDR12;
    }

    return 0;
}

/**
 * @brief Send CMD11 voltage switch command and validates the R1 response.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on CMD11 send/response failure.
 */
static int32_t sd_cmd_voltage_switch(cmd_parameters_t *pcmd)
{
    int32_t state;
    int32_t cmd_status;
    uint32_t resp;

    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_VOLTAGE_SWITCH;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        ERROR("CMD11 send failed");
        return -EIO;
    }

    sdmmc_wait_cmd_done();
    cmd_status = sdmmc_descriptor.status_code;
    if (cmd_status != 0)
    {
        ERROR("CMD11 failed status=%d", cmd_status);
        return -EIO;
    }

    resp = sdmmc_read_response();
    if ((resp & SDMMC_R1_ERROR_MASK) != 0U)
    {
        ERROR("CMD11 R1 error resp=0x%08x", resp);
        return -EIO;
    }

    return 0;
}

/**
 * @brief Open and drive the external voltage regulator GPIO to select 3.3V
 *        or 1.8V on the SDIO rail.
 * @param[in] is_1v8 True to select 1.8V, false to select 3.3V.
 * @return
 * - 0 on success.
 * - -EIO on GPIO open/config/write/read failure.
 */
static int32_t sd_toggle_volt_reg_sdio(bool is_1v8)
{
    int32_t ret;
    int gpio_dir;
    uint8_t gpio_wval;
    uint8_t gpio_rval;

    if (sdio_sel_gpio == NULL)
    {
        sdmmc_host_configure_volt_reg_gpio_pinmux();

        sdio_sel_gpio = gpio_open(SDMMC_VOLT_REG_SDIO_PIN);
        if (sdio_sel_gpio == NULL)
        {
            ERROR("Failed to open SDIO_SEL gpio");
            return -EIO;
        }

        gpio_dir = GPIO_DIR_OUT;
        ret = gpio_ioctl(sdio_sel_gpio, SET_GPIO_DIR, &gpio_dir);
        if (ret != 0)
        {
            ERROR("Failed to set gpio direction");
            return -EIO;
        }
    }

    gpio_wval = is_1v8 ? 1U : 0U;
    ret = gpio_write_sync(sdio_sel_gpio, gpio_wval);
    if (ret != 0)
    {
        ERROR("Failed to set gpio value=%u", gpio_wval);
        return -EIO;
    }

    ret = gpio_read_sync(sdio_sel_gpio, &gpio_rval);
    if (ret != 0)
    {
        ERROR("Failed to read gpio value");
        return -EIO;
    }

    if (gpio_rval != gpio_wval)
    {
        ERROR("Readback mismatch value=%u readback=%u", gpio_wval, gpio_rval);
    }
    return 0;
}

/**
 * @brief Restore the host to default state by power-cycling the card after a
 *        failed 1.8V switch.
 * @return
 * - 0 on success.
 * - -EIO on power-cycle, reset, or host reinitialization failure.
 */
static int32_t sd_recover_after_lvs_fail(void)
{
    int32_t state;

    (void)sd_toggle_volt_reg_sdio(false);
    (void)sdmmc_host_set_sd_clock_enable(0U);
    (void)sdmmc_host_set_1v8_signaling(false);
    (void)sdmmc_host_set_uhs_mode(SDMMC_UHS_MODE_LEGACY);

    state = sdmmc_set_bus_power(0U);
    if (state != 0)
    {
        return -EIO;
    }
    /* Let the supply rail fully discharge before re-asserting power. */
    osal_task_delay(SDMMC_POWER_CYCLE_DELAY_MS);

    state = sdmmc_set_bus_power(1U);
    if (state != 0)
    {
        return -EIO;
    }
    /* Let the supply rail ramp up and settle before the card is clocked again. */
    osal_task_delay(SDMMC_POWER_CYCLE_DELAY_MS);

    state = sdmmc_reset_configs();
    if (state != 0)
    {
        return -EIO;
    }

    state = sdmmc_init_configs(sdmmc_descriptor.dev_type);
    if (state != 0)
    {
        return -EIO;
    }

    state = sdmmc_wait_cmd_data_busy_clear();
    if (state != 0)
    {
        return -EIO;
    }

    return 0;
}

/**
 * @brief Switch host signaling to 1.8V: disables clock, asserts regulator,
 *        enables 1.8V mode, reinits PHY, re-enables clock.
 * @return
 * - 0 on success.
 * - -EIO on clock, signaling, GPIO, or PHY reinitialization failure.
 */
static int32_t sd_switch_host_to_1v8(void)
{
    int32_t state;

    state = sdmmc_host_set_sd_clock_enable(0U);
    if (state != 0)
    {
        ERROR("Failed to disable SD clock");
        return -EIO;
    }

    state = sd_toggle_volt_reg_sdio(true);
    if (state != 0)
    {
        ERROR("Failed to select external 1.8V rail");
        return -EIO;
    }

    state = sdmmc_host_set_1v8_signaling(true);
    if (state != 0)
    {
        ERROR("Failed to set 1.8V signaling");
        return -EIO;
    }

    /* Keep the clock gated while the card's regulator settles at 1.8V before it resumes. */
    osal_task_delay(SDMMC_VOLT_SWITCH_DELAY_MS);

    state = sdmmc_host_set_sd_clock_enable(1U);
    if (state != 0)
    {
        ERROR("Failed to enable SD clock");
        return -EIO;
    }

    /*
     * The Cadence combo PHY DLL must be reset and retrained after the supply
     * voltage changes. Without this the clock output is malformed at 1.8V
     * signaling levels and the card keeps DAT0 low indefinitely.
     * Mirrors sdhci_cdns6_set_uhs_signaling() in the Linux driver, which is
     * invoked from set_ios() immediately after every voltage-level change.
     */
    state = sdmmc_init_phy_cfg(&sdmmc_descriptor.phy_cfg,
            &sdmmc_descriptor.host_cfg);
    if (state != 0)
    {
        ERROR("PHY re-init after 1.8V switch failed");
        return -EIO;
    }

    /* Allow clock and PHY output to stabilize before DAT0 is sampled. */
    osal_task_delay(5U);

    return 0;
}

/**
 * @brief Issue CMD6 switch to set the card's group1 bus-speed mode and
 *        verify the card accepted it.
 * @param[in] pcmd Command parameter instance.
 * @param[in] mode Target UHS group1 mode value.
 * @return
 * - 0 on success.
 * - -EOPNOTSUPP if card does not accept the requested UHS mode.
 * - -EINVAL if mode argument is invalid.
 * - -EIO on command or transfer failure.
 */
static int32_t sd_cmd_switch_uhs_mode(cmd_parameters_t *pcmd, uint8_t mode)
{
    int32_t state;
    uint8_t grp1_sel;
    uint8_t grp1_raw;
    uint32_t switch_arg;

    if (pcard_specific_data->supports_switch_cmd == 0U)
    {
        ERROR("Skipping CMD6, CCC_SWITCH not supported");
        return -EOPNOTSUPP;
    }

    if (mode == SDMMC_SWITCH_GRP1_SDR50)
    {
        switch_arg = SDMMC_ARG_SWITCH_SDR50;
    }
    else if (mode == SDMMC_SWITCH_GRP1_SDR25)
    {
        switch_arg = SDMMC_ARG_SWITCH_SDR25;
    }
    else if (mode == SDMMC_SWITCH_GRP1_SDR12)
    {
        switch_arg = SDMMC_ARG_SWITCH_SDR12;
    }
    else
    {
        return -EINVAL;
    }

    pcmd->argument = switch_arg;
    pcmd->command_index = SDMMC_CMD_SWITCH;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    sdmmc_descriptor.is_api_sync = true;

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, (uint64_t *)switch_status,
            SDMMC_SWITCH_STATUS_SIZE, SDMMC_SINGLE_BLOCK);
    sdmmc_set_xfer_config(pcmd);
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        ERROR("CMD6 send failed");
        return -EIO;
    }

    sdmmc_wait_xfer_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        ERROR("CMD6 transfer failed status=%d int=0x%08x",
                sdmmc_descriptor.status_code, sdmmc_get_int_status());
        return -EIO;
    }

    cache_force_invalidate((uint64_t *)switch_status, SDMMC_SWITCH_STATUS_SIZE);
    grp1_raw = switch_status[SDMMC_SWITCH_GRP1_SEL_IDX];
    grp1_sel = grp1_raw & SDMMC_SWITCH_GRP1_MASK;

    if (grp1_sel != mode)
    {
        ERROR("Card did not accept mode=%u (sel=0x%02x)",
                mode, grp1_sel);
        return -EOPNOTSUPP;
    }

    return 0;
}

/**
 * @brief Reset host UHS mode to legacy and restore HS clock after a UHS
 *        configuration failure.
 */
static void sd_fallback_to_hs_mode(void)
{
    int32_t state;

    state = sd_toggle_volt_reg_sdio(false);
    if (state != 0)
    {
        ERROR("Failed to select external 3.3V rail");
    }

    state = sdmmc_host_set_uhs_mode(SDMMC_UHS_MODE_LEGACY);
    if (state != 0)
    {
        ERROR("Failed to set legacy UMS");
    }

    state = sdmmc_host_set_clock(SDMMC_HOST_CLK_FREQ_HS);
    if (state != 0)
    {
        ERROR("Failed to restore HS clock");
        return;
    }

    sdmmc_descriptor.speed_mode = SDMMC_SPEED_MODE_HS;
}

/**
 * @brief Read the SD Configuration Register (SCR) via ACMD51; extract spec
 *        version and supported bus widths.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on CMD55/ACMD51 send, transfer, or SCR validation failure.
 */
static int32_t sd_read_scr(cmd_parameters_t *pcmd)
{
    int32_t state;
    uint32_t raw_scr[2];
    uint32_t resp[4] = {0U};

    state = sd_send_app_cmd(pcmd, (uint32_t)(pcard_specific_data->relative_address <<
            SDMMC_ARG_MASK_REL_ADD));
    if (state != 0)
    {
        ERROR("CMD55 APP_CMD failed");
        return -EIO;
    }

    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_SEND_SCR;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    sdmmc_descriptor.is_api_sync = true;

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, (uint64_t *)scr_status,
            SDMMC_SCR_STATUS_SIZE, SDMMC_SINGLE_BLOCK);
    sdmmc_set_xfer_config(pcmd);
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        ERROR("ACMD51 send failed");
        return -EIO;
    }

    sdmmc_wait_xfer_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        ERROR("ACMD51 transfer failed status=%d int=0x%08x",
                sdmmc_descriptor.status_code, sdmmc_get_int_status());
        return -EIO;
    }

    state = sdmmc_wait_data_busy_clear();
    if (state != 0)
    {
        ERROR("Data busy clear failed");
        return -EIO;
    }

    cache_force_invalidate((uint64_t *)scr_status, SDMMC_SCR_STATUS_SIZE);

    INFO("SCR raw bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
            scr_status[0], scr_status[1], scr_status[2], scr_status[3],
            scr_status[4], scr_status[5], scr_status[6], scr_status[7]);

    raw_scr[0] = ((uint32_t)scr_status[0] << 24U) |
            ((uint32_t)scr_status[1] << 16U) |
            ((uint32_t)scr_status[2] << 8U) |
            (uint32_t)scr_status[3];
    raw_scr[1] = ((uint32_t)scr_status[4] << 24U) |
            ((uint32_t)scr_status[5] << 16U) |
            ((uint32_t)scr_status[6] << 8U) |
            (uint32_t)scr_status[7];

    INFO("SCR raw words: hi=0x%08x lo=0x%08x", raw_scr[0], raw_scr[1]);

    resp[2] = raw_scr[0];
    resp[3] = raw_scr[1];

    state = sd_parse_scr(resp);
    if (state != 0)
    {
        return state;
    }

    INFO("SCR decode: sda_vsn=%u spec3=%u spec4=%u specx=%u bus_widths=0x%x cmd23=%u",
            pcard_specific_data->scr.sda_vsn,
            pcard_specific_data->scr.sda_spec3,
            pcard_specific_data->scr.sda_spec4,
            pcard_specific_data->scr.sda_specx,
            pcard_specific_data->scr.bus_widths,
            pcard_specific_data->scr.cmds23_support);

    return 0;
}

/**
 * @brief Issue CMD6 switch to set card to High Speed mode and verify
 *        acceptance.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EOPNOTSUPP if high-speed mode is unsupported.
 * - -EIO on CMD6 send/transfer failure.
 */
static int32_t sd_cmd_switch_high_speed(cmd_parameters_t *pcmd)
{
    int32_t state;

    if (pcard_specific_data->supports_switch_cmd == 0U)
    {
        return -EOPNOTSUPP;
    }

    if ((sdmmc_descriptor.card_caps_valid != 0U) &&
            ((sdmmc_descriptor.card_uhs_caps & SDMMC_SWITCH_SUP_SDR25) == 0U))
    {
        return -EOPNOTSUPP;
    }

    if (sdmmc_descriptor.host_hs_supported == 0U)
    {
        return -EOPNOTSUPP;
    }

    pcmd->argument = SDMMC_ARG_SWITCH_HS;
    pcmd->command_index = SDMMC_CMD_SWITCH;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    sdmmc_descriptor.is_api_sync = true;

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, (uint64_t *)switch_status,
            SDMMC_SWITCH_STATUS_SIZE, SDMMC_SINGLE_BLOCK);
    sdmmc_set_xfer_config(pcmd);

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        ERROR("CMD6 send failed");
        return -EIO;
    }

    sdmmc_wait_xfer_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        ERROR("CMD6 transfer failed");
        return -EIO;
    }

    cache_force_invalidate((uint64_t *)switch_status, SDMMC_SWITCH_STATUS_SIZE);

    if ((switch_status[SDMMC_SWITCH_GRP1_SEL_IDX] & SDMMC_SWITCH_GRP1_MASK) !=
            SDMMC_SWITCH_GRP1_HS)
    {
        return -EOPNOTSUPP;
    }

    return 0;
}

/**
 * @brief Read CMD8 response echo-back; set sd_spec_v2 flag if voltage range
 *        and check pattern match.
 * @return 0 on success.
 */
static int32_t sd_check_if_cond_version(void)
{
    uint32_t response;

    response = sdmmc_read_response();
    if ((response & SDMMC_CMD8_VHS_CHECK_MASK) != SDMMC_CMD8_VHS_CHECK_PATTERN)
    {
        pcard_specific_data->sd_spec_v2 = 0U;
        return 0;
    }

    pcard_specific_data->sd_spec_v2 = 1U;
    return 0;
}

/**
 * @brief Read CSD command class bits; set supports_switch_cmd if CCC_SWITCH
 *        is present.
 */
static void sd_check_switch_command_capability(void)
{
    uint32_t cmd_class;
    cmd_class = sdmmc_read_csd_cmd_class();
    pcard_specific_data->supports_switch_cmd =
            ((cmd_class & SDMMC_CCC_SWITCH_MASK) != 0U) ? 1U : 0U;
}

/**
 * @brief Send CMD0 to reset all cards to idle state.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t sd_go_idle(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_GO_IDLE_STATE;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_NO_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send CMD8 with voltage range and check pattern to test host
 *        compatibility.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t sd_send_if_cond(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_ARG_CHECK_PATTERN;
    pcmd->command_index = SDMMC_CMD_SEND_IF_COND;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send CMD55 to indicate the next command is an application-specific
 *        command.
 * @param[in] pcmd     Command parameter instance.
 * @param[in] argument CMD55 argument (0 or RCA << 16).
 * @return
 * - 0 on success.
 * - -EIO on CMD55 send/completion failure or missing APP_CMD response bit.
 */
static int32_t sd_send_app_cmd(cmd_parameters_t *pcmd, uint32_t argument)
{
    int32_t state;
    int32_t cmd_status;
    uint32_t response;

    pcmd->argument = argument;
    pcmd->command_index = SDMMC_CMD_SEND_APP;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        ERROR("CMD55 send failed");
        return -EIO;
    }
    sdmmc_wait_cmd_done();
    cmd_status = sdmmc_descriptor.status_code;
    if (cmd_status != 0)
    {
        return cmd_status;
    }

    if (argument != SDMMC_NO_CMD_ARG)
    {
        response = sdmmc_read_response();
        if ((response & SDMMC_R1_APP_CMD_MASK) == 0U)
        {
            ERROR("CMD55 response missing APP_CMD bit resp=0x%08x", response);
            return -EIO;
        }
    }

    return 0;
}

/**
 * @brief Send ACMD41 with OCR argument to check and initiate card power-up.
 * @param[in] pcmd    Command parameter instance.
 * @param[in] ocr_arg OCR argument for ACMD41.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t sd_check_ocr(cmd_parameters_t *pcmd, uint32_t ocr_arg)
{
    int32_t state;
    pcmd->argument = ocr_arg;
    pcmd->command_index = SDMMC_CMD_READ_OCR;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    pcard_specific_data->ocr_response = sdmmc_read_response();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send CMD2 to broadcast request for all card identification numbers.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t sd_send_all_cid(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_ALL_SEND_CID;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_LONG_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send CMD3 to request the card to publish a new relative address.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t sd_send_rel_add(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_SND_REL_ADDR;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;

}
/**
 * @brief Poll ACMD41 in a loop until the card completes power-up
 *        initialization.
 * @param[in] pcmd_handle Command parameter instance.
 * @param[in] ocr_arg     OCR argument for ACMD41.
 * @return
 * - 0 on success.
 * - -EIO on CMD55/ACMD41 command failure.
 * - -ETIMEDOUT if card-ready polling exceeds retry limit.
 */
static int32_t sd_en_card_ready(cmd_parameters_t *pcmd_handle, uint32_t ocr_arg)
{
    int32_t state;
    int retry = 100;

    /*
     * Poll for sd card ready by reapetedly issuing APP_CMD and followed
     * by OCR check. A retry count of 100 is provided because some time
     * may be needed to complete interanl power-up and voltage negotiation.
     * If the sdmmc card is not ready within 100 iteration, a timeout is
     * returned.
     */
    while ((sdmmc_is_card_ready()) == 0U && (retry > 0 ))
    {
        state = sd_send_app_cmd(pcmd_handle, SDMMC_NO_CMD_ARG);
        if (state != 0)
        {
            ERROR("CMD55 APP_CMD failed");
            return -EIO;
        }
        state = sd_check_ocr(pcmd_handle, ocr_arg);
        if (state != 0)
        {
            ERROR("ACMD41 OCR failed");
            return -EIO;
        }
        retry--;
        /* ACMD41 power-up polling interval; avoid flooding the card with requests while it is busy. */
        osal_task_delay(10);
    }
    if(retry <= 0 )
    {
        ERROR("Card ready timeout");
        return -ETIMEDOUT;
    }
    return 0;
}

/**
 * @brief Send CMD7 to select the card using its relative address; waits for
 *        DAT0 busy clear.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 * - -ETIMEDOUT if DAT busy does not clear.
 */
static int32_t sd_sel_card(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = (pcard_specific_data->relative_address) <<
            SDMMC_ARG_MASK_REL_ADD;
    pcmd->command_index = SDMMC_CMD_SELECT_CARD;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE_BUSY;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    sdmmc_descriptor.is_api_sync = true;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        return sdmmc_descriptor.status_code;
    }

    state = sdmmc_wait_data_busy_clear();
    if (state != 0)
    {
        return state;
    }

    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send CMD9 to read the card-specific data register.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t sd_send_csd(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = (pcard_specific_data->relative_address) <<
            SDMMC_ARG_MASK_REL_ADD;
    pcmd->command_index = SDMMC_CMD_SEND_CSD;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_LONG_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Issue ACMD6 to switch the card to 4-bit or 1-bit bus width and
 *        update the host accordingly.
 * @param[in] pcmd_handle Command parameter instance.
 * @param[in] bus_width   Target bus width (SDMMC_BUS_WIDTH_1 or _4).
 * @return
 * - 0 on success.
 * - -EINVAL if requested bus width is not 1-bit or 4-bit.
 * - -EIO on CMD55/ACMD6 or host-width configuration failure.
 */
static int32_t sd_set_bus_width(cmd_parameters_t *pcmd_handle, uint32_t bus_width)
{
    int32_t state;
    int32_t cmd_status;
    uint32_t is_width_4;

    if ((bus_width != SDMMC_BUS_WIDTH_1) && (bus_width != SDMMC_BUS_WIDTH_4))
    {
        return -EINVAL;
    }

    is_width_4 = (bus_width == SDMMC_BUS_WIDTH_4) ? 1U : 0U;

    if ((is_width_4 != 0U) &&
            ((pcard_specific_data->scr.bus_widths & SDMMC_SCR_BUS_WIDTH_4) == 0U))
    {
        INFO("SDMMC: SCR bus widths=0x%x, no 4-bit support, keeping 1-bit",
                pcard_specific_data->scr.bus_widths);
        return 0;
    }

    state = sd_send_app_cmd(pcmd_handle,
            (uint32_t)(pcard_specific_data->relative_address <<
            SDMMC_ARG_MASK_REL_ADD));

    if (state != 0)
    {
        ERROR("CMD55 APP_CMD failed status=%d int=0x%08x",
                state, sdmmc_get_int_status());
        return -EIO;
    }

    pcmd_handle->argument = (is_width_4 != 0U) ? SDMMC_ARG_BUS_WIDTH : SDMMC_NO_CMD_ARG;
    pcmd_handle->command_index = SDMMC_CMD_SWITCH;
    pcmd_handle->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd_handle->response_type = SDMMC_SHORT_RESPONSE;
    pcmd_handle->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd_handle->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;

    state = sdmmc_send_command(pcmd_handle);
    sdmmc_wait_cmd_done();
    cmd_status = sdmmc_descriptor.status_code;
    if (cmd_status != 0)
    {
        ERROR("ACMD6 SET_BUS_WIDTH failed status=%d int=0x%08x",
                cmd_status, sdmmc_get_int_status());
        return -EIO;
    }

    state = sdmmc_set_data_bus_width(bus_width);
    if (state != 0)
    {
        ERROR("Failed to set host bus width");
        return -EIO;
    }

    INFO("SDMMC: bus width set to %u-bit", (is_width_4 != 0U) ? 4U : 1U);
    return 0;
}
#endif

#if (DEV_TYPE ==  DEV_TYPE_EMMC)
/**
 * @brief Top-level eMMC initialization sequence per JESD84-B51A.
 * @param[in] sec_num Pointer to store eMMC sector count.
 * @return
 * - 0 on success.
 * - -EIO on command, response, or transfer failure.
 */
static int32_t sd_mmc_init(uint64_t *sec_num)
{
    cmd_parameters_t *pcmd_handle;
    cmd_parameters_t command_config;
    int32_t state;

    pcmd_handle = &command_config;
    /* Let the card complete its power-up/reset before CMD0 is issued on a re-init. */
    osal_task_delay(10);
    state = mmc_go_idle(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /* send command to set the card into ready state */
    state = mmc_check_ocr(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /* send cmd to request cid of the card */
    state = mmc_send_all_cid(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /* send cmd to set the rel addr of the card */
    state = mmc_set_rel_add(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    sd_read_response_rel_addr(pcard_specific_data);
    /* send command to req csd of the card */
    state = mmc_send_csd(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /* send cmd to select the card */
    state = mmc_sel_card(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /* send cmd to set ext_csd */
    state = mmc_switch_bus_width(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /* send cmd to request extended csd of the card */
    state = mmc_send_ext_csd(pcmd_handle, sec_num);
    return state;
}


/**
 * @brief Send MMC CMD0 to place device in idle state.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t mmc_go_idle(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_GO_IDLE_STATE;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_NO_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}
/**
 * @brief Send MMC CMD7 to select the addressed card.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send failure.
 * - -ETIMEDOUT on transfer-timeout interrupt.
 */
static int32_t mmc_sel_card(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = (SDMMC_SET_ADDR << SDMMC_ARG_MASK_REL_ADD);
    pcmd->command_index = SDMMC_CMD_SELECT_CARD;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE_BUSY;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        return sdmmc_descriptor.status_code;
    }

    state = sdmmc_wait_data_busy_clear();
    if (state != 0)
    {
        return state;
    }

    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send MMC CMD9 to read card-specific data (CSD).
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t mmc_send_csd(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = (SDMMC_REL_CARD_ADDRESS << SDMMC_ARG_MASK_REL_ADD);
    pcmd->command_index = SDMMC_CMD_SEND_CSD;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_LONG_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send MMC CMD3 to set relative card address.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t mmc_set_rel_add(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = (SDMMC_REL_CARD_ADDRESS << SDMMC_ARG_MASK_REL_ADD);
    pcmd->command_index = SDMMC_CMD_SET_REL_ADD;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    sdmmc_descriptor.is_api_sync = true;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send MMC CMD2 to read card identification (CID).
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t mmc_send_all_cid(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_ALL_SEND_CID;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_LONG_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Poll MMC CMD1 until OCR indicates device ready.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send or command-timeout failure.
 */
static int32_t mmc_check_ocr(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_ARG_SDHC_OCR;
    pcmd->command_index = SDMMC_CMD_CHECK_OCR;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;

    while (sdmmc_is_card_ready() == 0U)
    {
        state = sdmmc_send_command(pcmd);
        if (state != 0)
        {
            return state;
        }
        sdmmc_wait_cmd_done();
    }
    return sdmmc_descriptor.status_code;
}

/**
 * @brief Send CMD8 to read EXT_CSD and extract sector count.
 * @param[in]  pcmd            Command parameter instance.
 * @param[out] sector_count_ref Pointer to receive sector count.
 * @return
 * - 0 on success.
 * - -EIO on command submission failure.
 * - -ETIMEDOUT on transfer-timeout interrupt.
 */
static int32_t mmc_send_ext_csd(cmd_parameters_t *pcmd,
        uint64_t *sector_count_ref)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_SEND_EXT_CSD;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;

    sdmmc_descriptor.is_api_sync = true;

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_desc_buf, (uint64_t *)ext_csd_buff,
            SDMMC_BLOCK_SIZE, SDMMC_SINGLE_BLOCK);
    sdmmc_set_xfer_config(pcmd);

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return -EIO;
    }
    sdmmc_wait_xfer_done();

    if (sdmmc_descriptor.status_code == 0)
    {
        cache_force_invalidate(ext_csd_buff, SDMMC_BLOCK_SIZE);
        *sector_count_ref = *(uint32_t *)(ext_csd_buff + SDMMC_EXT_CSD_SEC_NUM);
    }
    return sdmmc_descriptor.status_code;
}
/**
 * @brief Send MMC CMD6 to switch card bus width to 8-bit setting.
 * @param[in] pcmd Command parameter instance.
 * @return
 * - 0 on success.
 * - -EIO on command-send failure.
 * - -ETIMEDOUT on transfer-timeout interrupt.
 */
static int32_t mmc_switch_bus_width(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_SET_EXT_BUS_WIDTH;
    pcmd->command_index = SDMMC_CMD_SWITCH;
    pcmd->data_xfer_present = SDMMC_DATA_XFER_NOT_PST;
    pcmd->response_type = SDMMC_SHORT_RESPONSE_BUSY;
    pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_DI;
    pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_DI;
    sdmmc_descriptor.is_api_sync = true;

    state = sdmmc_send_command(pcmd);
    if (state != 0)
    {
        return state;
    }
    sdmmc_wait_cmd_done();
    if (sdmmc_descriptor.status_code != 0)
    {
        return sdmmc_descriptor.status_code;
    }

    state = sdmmc_wait_data_busy_clear();
    if (state != 0)
    {
        return state;
    }

    return sdmmc_descriptor.status_code;
}
#endif

/**
 * @brief Reset and configure the SDMMC host controller hardware.
 * @return
 * - 0 on success.
 * - -EIO on reset/PHY/host configuration failure.
 * - -ETIMEDOUT if host clock stabilization does not complete.
 */
static int32_t sdmmc_setup_host(void)
{
    /*
     * the default mux for shared combo phy is configured
     * for the nand ,if combophy is not enabled for
     * sdmmc at ATF configure dfi_interface_cfg to 1 through smc
     */
    int32_t ret = 0;
    ret = sdmmc_reset_per0();
    if (ret != 0)
    {
        return ret;
    }
    ret = sdmmc_reset_configs();
    if (ret != 0)
    {
        return ret;
    }
    ret = sdmmc_init_phy_cfg(&sdmmc_descriptor.phy_cfg,
            &sdmmc_descriptor.host_cfg);

    if (ret != 0)
    {
        return ret;
    }
    ret = sdmmc_init_configs(sdmmc_descriptor.dev_type);
    if (ret != 0)
    {
        ERROR("Host init config failed");
        return -EIO;
    }
    return ret;
}

uint32_t sdmmc_is_card_present(void)
{
    return sdmmc_is_card_detected();
}

/**
 * @brief Block the calling task until a data transfer completes, waiting on
 *        the transfer semaphore.
 */
static void sdmmc_wait_xfer_done(void)
{
    (void)osal_semaphore_wait(sdmmc_descriptor.xfer_sem,
            OSAL_TIMEOUT_WAIT_FOREVER);
}

/**
 * @brief Block the calling task until a command response is received, with a
 *        timeout.
 */
static void sdmmc_wait_cmd_done(void)
{
    (void)osal_semaphore_wait(sdmmc_descriptor.cmd_sem,
            SDMMC_CMD_TIMEOUT_MS);
}

void sdmmc_irq_handler(void *data)
{
    (void)data;
    uint32_t volatile int_status = sdmmc_get_int_status();

    sdmmc_clear_int(int_status);

    if ((int_status & SDMMC_XFER_TIMOUT_INT_LOG) == SDMMC_XFER_TIMOUT_INT_LOG)
    {
        if (sdmmc_descriptor.is_api_sync == true)
        {
            sdmmc_descriptor.status_code = -ETIMEDOUT;
            (void)osal_semaphore_post(sdmmc_descriptor.xfer_sem);
        }
        else
        {
            sdmmc_descriptor.status_code = -ETIMEDOUT;
            if (sdmmc_descriptor.xfer_call_back != NULL)
            {
                sdmmc_descriptor.xfer_call_back(-EIO);
            }
        }
        return;
    }

    if ((int_status & SDMMC_CMD_TIMOUT_INT_LOG) == SDMMC_CMD_TIMOUT_INT_LOG)
    {
        sdmmc_descriptor.status_code = -EIO;
        if (sdmmc_recover_cmd_dat_lines() != 0)
        {
            ERROR("CMD timeout recovery failed");
        }
        (void)osal_semaphore_post(sdmmc_descriptor.cmd_sem);
        return;
    }

    if ((int_status & SDMMC_XFER_CPT_INT_LOG) == SDMMC_XFER_CPT_INT_LOG)
    {
        if (sdmmc_descriptor.is_api_sync == true)
        {
            sdmmc_descriptor.status_code = 0;
            (void)osal_semaphore_post(sdmmc_descriptor.xfer_sem);
        }
        else
        {
            sdmmc_descriptor.status_code = 0;
            if (sdmmc_descriptor.xfer_call_back != NULL)
            {
                sdmmc_descriptor.xfer_call_back(0);
            }
        }
    }

    if ((int_status & SDMMC_CMD_CPT_INT_LOG) == SDMMC_CMD_CPT_INT_LOG)
    {
        sdmmc_descriptor.status_code = 0;
        (void)osal_semaphore_post(sdmmc_descriptor.cmd_sem);
    }
}
