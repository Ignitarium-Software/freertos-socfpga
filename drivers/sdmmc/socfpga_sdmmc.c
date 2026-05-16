/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
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
#include <stdio.h>
#include <errno.h>
#include "osal.h"
#include "osal_log.h"
#include "socfpga_cache.h"
#include "socfpga_interrupt.h"
#include "socfpga_sdmmc_ll.h"
#include "socfpga_sdmmc.h"

/*
 * Maximum time to wait for an sdmmc command response before timeout.
 * 10ms provides a safe margin for back to back command responses.
 */
#define SDMMC_CMD_TIMEOUT_MS   10UL
#define DEF_SPEED_EN    1
#define DEF_SPEED_DI    0

#define DEV_TYPE_SD      0
#define DEV_TYPE_EMMC    1

/*specify max descriptor count here
 * 1 descriptor can handle up to 64KB of data
 */
#define SDMMC_MAX_DESCRIPTOR    160U
/* specify your device here */
#define DEV_TYPE    DEV_TYPE_SD

/*INFO:
 * Driver supports High speed[speed upto 25 MBps at 50 MHz] and
 * Default speed[speed upto 12.5 MBps at 25 MHz] mode.
 **/

#define SUPPORT_DEF_SPEED    DEF_SPEED_DI

#if SDMMC_MAX_DESCRIPTOR < 1
#error Invalid descriptor count
#endif

#if (DEV_TYPE ==  DEV_TYPE_SD)
#define BUS_PARAM    0
static int32_t sd_mmc_init(uint64_t *sec_num);
static int32_t sd_go_idle(cmd_parameters_t *pcmd);
static int32_t sd_snd_if_cond(cmd_parameters_t *pcmd);
static int32_t sd_snd_app_cmd(cmd_parameters_t *pcmd);
static int32_t sd_check_ocr(cmd_parameters_t *pcmd);
static int32_t sd_snd_all_cid(cmd_parameters_t *pcmd);
static int32_t sd_snd_rel_add(cmd_parameters_t *pcmd);
static int32_t sd_sel_card(cmd_parameters_t *pcmd);
static int32_t sd_snd_csd(cmd_parameters_t *pcmd);
static int32_t sd_en_card_ready(cmd_parameters_t *pcmd_handle);
static int32_t sd_snd_app_bus_cmd(cmd_parameters_t *pcmd);
static int32_t sd_set_bus_width(cmd_parameters_t *pcmd_handle);
static int32_t sd_bus_width_4(cmd_parameters_t *pcmd);

#elif (DEV_TYPE ==  DEV_TYPE_EMMC)
#define BUS_PARAM    1
static int32_t sd_mmc_init(uint64_t *sec_num);
static int32_t mmc_go_idle(cmd_parameters_t *pcmd);
static int32_t mmc_snd_all_cid(cmd_parameters_t *pcmd);
static int32_t mmc_set_rel_add(cmd_parameters_t *pcmd);
static int32_t mmc_check_ocr(cmd_parameters_t *pcmd);
static int32_t mmc_sel_card(cmd_parameters_t *pcmd);
static int32_t mmc_snd_csd(cmd_parameters_t *pcmd);
static int32_t mmc_send_ext_csd(cmd_parameters_t *pcmd,
        uint64_t *sector_count_ref);
static int32_t mmc_switch_bus_width(cmd_parameters_t *pcmd);

static uint8_t ext_csd_buff[512];

#else
#error "Device not supported"
#endif

static int32_t sdmmc_setup_host(void);

static void sdmmc_wait_xfer_done(void);
void sdmmc_irq_handler(void *data);
static void sdmmc_wait_cmd_done(void);

static card_data_t *pcard_specific_data;
static card_data_t card_data;

static osal_semaphore_def_t osal_def_xfer;
static osal_semaphore_def_t osal_def_cmd;

struct sdmmc_context
{
    bool is_api_sync;
    int32_t status_code;
    osal_semaphore_t semaphore_xfer;
    osal_semaphore_t semaphore_cmd;
    sdmmc_cb_fun xfer_call_back;
    dma_descriptor_t dma_descriptor[SDMMC_MAX_DESCRIPTOR];
    uint32_t is_def_speed_supported;
    uint32_t dev_type;
};

static struct sdmmc_context sdmmc_descriptor;

int32_t sdmmc_read_block_async(uint64_t *pread_buffer, uint64_t read_addr,
        uint32_t block_size, uint32_t number_of_blocks,
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

    /*convert address into block number*/
    read_addr /= block_size;

    if ((pread_buffer == NULL) || (number_of_blocks == 0U))
    {
        return -EINVAL;
    }
    /*send cmd to set block size*/
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
    /*argument preparation for data read*/
    if (number_of_blocks > 1U)
    {
        pcmd->argument = read_addr;
        pcmd->command_index = SDMMC_CMD_READ_MULT_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    else
    {
        pcmd->argument = read_addr;
        pcmd->command_index = SDMMC_CMD_READ_SINGLE_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    /*calculate no of descriptors required*/
    req_desc_count = ((block_size * number_of_blocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;

    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    /*send cmd to request data from the card*/
    else
    {
        sdmmc_set_up_xfer(sdmmc_descriptor.dma_descriptor, pread_buffer,
                block_size, number_of_blocks);
        sdmmc_set_xfer_config(pcmd);
        DEBUG("Initiating sdmmc data read");
        state = sdmmc_send_command(pcmd);
        if (state != 0)
        {
            return -EIO;
        }

        return 0;
    }
}

int32_t sdmmc_read_block_sync(uint64_t *pread_buffer, uint64_t read_addr,
        uint32_t block_size, uint32_t number_of_blocks)
{
    sdmmc_descriptor.is_api_sync = false;
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

    /*convert address into block number*/
    read_addr /= block_size;

    if ((pread_buffer == NULL) || (number_of_blocks == 0U))
    {
        return -EINVAL;
    }
    /*send cmd to set block size*/
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
    /*argument preparation for data read*/
    if (number_of_blocks > 1U)
    {
        pcmd->argument = read_addr;
        pcmd->command_index = SDMMC_CMD_READ_MULT_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    else
    {
        pcmd->argument = read_addr;
        pcmd->command_index = SDMMC_CMD_READ_SINGLE_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    /*calculate no of descriptors required*/
    req_desc_count = ((block_size * number_of_blocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;

    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    /*send cmd to request data from the card*/
    else
    {
        sdmmc_set_up_xfer(sdmmc_descriptor.dma_descriptor, pread_buffer,
                block_size, number_of_blocks);
        sdmmc_set_xfer_config(pcmd);
        DEBUG("Initiating sdmmc data read");
        state = sdmmc_send_command(pcmd);
        if (state != 0)
        {
            return -EIO;
        }

        sdmmc_wait_xfer_done();

        if (sdmmc_descriptor.status_code == 0)
        {
            cache_force_invalidate((uint64_t *)pread_buffer, block_size *
                    number_of_blocks);
            DEBUG("Read %x blocks", number_of_blocks);

            return 0;
        }
        else
        {
            return -EIO;
        }
    }
}

int32_t sdmmc_write_block_async(uint64_t *pwrite_buffer, uint64_t write_addr,
        uint32_t block_size, uint32_t number_of_blocks,
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

    /*converts address into block number*/
    write_addr /= block_size;

    if ((pwrite_buffer == NULL) || (number_of_blocks == 0U))
    {
        return -EINVAL;
    }
    /*set cmd to set block size*/
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
    /*argument preparation for data transfer*/
    if (number_of_blocks > 1U)
    {
        pcmd->argument = write_addr;
        pcmd->command_index = SDMMC_CMD_WRITE_MULT_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    else
    {
        pcmd->argument = write_addr;
        pcmd->command_index = SDMMC_CMD_WRITE_MULT_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    /*calculate no of descriptors required*/
    req_desc_count = ((block_size * number_of_blocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;
    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }

    /*send cmd to transfer data to the card*/
    else
    {
        cache_force_write_back((uint64_t *)pwrite_buffer, block_size *
                number_of_blocks);
        sdmmc_set_up_xfer(sdmmc_descriptor.dma_descriptor, pwrite_buffer,
                block_size, number_of_blocks);
        sdmmc_set_xfer_config(pcmd);

        state = sdmmc_send_command(pcmd);
        INFO("Initiating sdmmc data write");
        if (state != 0)
        {
            return -EIO;
        }
    }
    return 0;
}

int32_t sdmmc_write_block_sync(uint64_t *pwrite_buffer, uint64_t write_addr,
        uint32_t block_size, uint32_t number_of_blocks)
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

    /*converts address into block number*/
    write_addr /= block_size;

    if ((pwrite_buffer == NULL) || (number_of_blocks == 0U))
    {
        return -EINVAL;
    }
    /*set cmd to set block size*/
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
    /*argument preparation for data transfer*/
    if (number_of_blocks > 1U)
    {
        pcmd->argument = write_addr;
        pcmd->command_index = SDMMC_CMD_WRITE_MULT_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    else
    {
        pcmd->argument = write_addr;
        pcmd->command_index = SDMMC_CMD_WRITE_MULT_BLOCK;
        pcmd->data_xfer_present = SDMMC_DATA_XFER_PST;
        pcmd->response_type = SDMMC_SHORT_RESPONSE;
        pcmd->id_check_enable = SDMMC_CMD_ID_CHECK_EN;
        pcmd->crc_check_enable = SDMMC_CMD_CRC_CHECK_EN;
    }
    /*calculate no of descriptors required*/
    req_desc_count = ((block_size * number_of_blocks) + (DESC_MAX_XFER_SIZE)
            -1U) / DESC_MAX_XFER_SIZE;

    if (req_desc_count > SDMMC_MAX_DESCRIPTOR)
    {
        return -EIO;
    }
    /*send cmd to transfer data to the card*/
    else
    {
        cache_force_write_back((uint64_t *)pwrite_buffer, block_size *
                number_of_blocks);
        sdmmc_set_up_xfer(sdmmc_descriptor.dma_descriptor, pwrite_buffer,
                block_size, number_of_blocks);
        sdmmc_set_xfer_config(pcmd);

        state = sdmmc_send_command(pcmd);
        INFO("Initiating sdmmc data write");
        if (state != 0)
        {
            return -EIO;
        }

        sdmmc_wait_xfer_done();

        if (sdmmc_descriptor.status_code == 0)
        {
            INFO("Written %x blocks", number_of_blocks);
            return 0;
        }
        else
        {
            return -EIO;
        }
    }
}


int32_t sdmmc_init_card(uint64_t *ptr_sec_num)
{
    int32_t ret;
    int32_t cmd_status;
    socfpga_interrupt_err_t intr_ret;

    pcard_specific_data = &card_data;
    if (pcard_specific_data == NULL)
    {
        return -EIO;
    }

    if (ptr_sec_num == NULL)
    {
        return -EINVAL;
    }

    if (sdmmc_is_card_present() != SDMMC_IS_CARD_DET)
    {
        ERROR("Device detection failed");
        return -EIO;
    }

    sdmmc_descriptor.is_def_speed_supported = SUPPORT_DEF_SPEED;
    sdmmc_descriptor.dev_type = DEV_TYPE;

    sdmmc_descriptor.semaphore_xfer = osal_semaphore_create(&osal_def_xfer);
    sdmmc_descriptor.semaphore_cmd = osal_semaphore_create(&osal_def_cmd);
    if ((sdmmc_descriptor.semaphore_xfer == NULL) ||
                (sdmmc_descriptor.semaphore_cmd == NULL))
    {
        return -ENOMEM;
    }
    intr_ret = interrupt_register_isr(SDMMC_IRQ, sdmmc_irq_handler, NULL);
    if (intr_ret != ERR_OK)
    {
        return -EIO;
    }
    intr_ret = interrupt_enable(SDMMC_IRQ, GIC_INTERRUPT_PRIORITY_SDMMC);
    if (intr_ret != ERR_OK)
    {
        return -EIO;
    }

    ret = sdmmc_setup_host();

    if (ret != CTRL_CONFIG_PASS)
    {
        return -EIO;
    }
    cmd_status = sd_mmc_init(ptr_sec_num);

    if (cmd_status == 0)
    {
        return 0;
    }
    else
    {
        return -EIO;
    }
}
#if (DEV_TYPE ==  DEV_TYPE_SD)
/*follows SD association standard*/
static int32_t sd_mmc_init(uint64_t *sec_num)
{
    cmd_parameters_t *pcmd_handle;
    cmd_parameters_t command_config;
    int32_t state;

    pcmd_handle = &command_config;
    /*send cmd to set the card idle*/
    state = sd_go_idle(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to echo back the argument*/
    state = sd_snd_if_cond(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*delay required when the combo phy clock -> 200 MHz*/
    osal_task_delay(10);
    /*send cmd to put card into ready state*/
    state = sd_en_card_ready(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    sd_get_card_type(pcard_specific_data);
    /*send cmd to request cid of the card*/
    state = sd_snd_all_cid(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to req the rel card addr of the card*/
    state = sd_snd_rel_add(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    sd_read_response_rel_addr(pcard_specific_data);
    /*send cmd to request csd of the card*/
    state = sd_snd_csd(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    *sec_num = sdmmc_read_sector_count();
    /*send cmd to select the card*/
    state = sd_sel_card(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    state = sd_set_bus_width(pcmd_handle);

    return state;
}
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

static int32_t sd_snd_if_cond(cmd_parameters_t *pcmd)
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

static int32_t sd_snd_app_cmd(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_NO_CMD_ARG;
    pcmd->command_index = SDMMC_CMD_SEND_APP;
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
    return sdmmc_descriptor.status_code;
}

static int32_t sd_check_ocr(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_ARG_SDHC_OCR;
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
    return sdmmc_descriptor.status_code;
}

static int32_t sd_snd_all_cid(cmd_parameters_t *pcmd)
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

static int32_t sd_snd_rel_add(cmd_parameters_t *pcmd)
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
static int32_t sd_en_card_ready(cmd_parameters_t *pcmd_handle)
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
        state = sd_snd_app_cmd(pcmd_handle);
        if (state != 0)
        {
            return -EIO;
        }
        state = sd_check_ocr(pcmd_handle);
        if (state != 0)
        {
            return -EIO;
        }
        retry--;
        osal_task_delay(10);
    }
    if(retry <= 0 )
    {
        return -ETIMEDOUT;
    }
    return 0;
}

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
    sdmmc_wait_xfer_done();
    return sdmmc_descriptor.status_code;
}

static int32_t sd_snd_csd(cmd_parameters_t *pcmd)
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

static int32_t sd_set_bus_width(cmd_parameters_t *pcmd_handle)
{
    int32_t state;
    state = sd_snd_app_bus_cmd(pcmd_handle);

    if (state != 0)
    {
        return -EIO;
    }

    state = sd_bus_width_4(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }

    return 0;
}

static int32_t sd_bus_width_4(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = SDMMC_ARG_BUS_WIDTH;
    pcmd->command_index = SDMMC_CMD_SWITCH;
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
    return sdmmc_descriptor.status_code;
}
static int32_t sd_snd_app_bus_cmd(cmd_parameters_t *pcmd)
{
    int32_t state;
    pcmd->argument = (pcard_specific_data->relative_address) <<
            SDMMC_ARG_MASK_REL_ADD;
    pcmd->command_index = SDMMC_CMD_SEND_APP;
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
    return sdmmc_descriptor.status_code;
}
#endif

#if (DEV_TYPE ==  DEV_TYPE_EMMC)
/*follows e.MMC standard: JESD84-B51A: Embedded MultiMediaCard (e.MMC),
 * Electrical Standard (5.1A)
 */
static int32_t sd_mmc_init(uint64_t *sec_num)
{
    cmd_parameters_t *pcmd_handle;
    cmd_parameters_t command_config;
    int32_t state;

    pcmd_handle = &command_config;
    /*required delay for re-init*/
    osal_task_delay(10);
    /*send cmd to set the card idle*/
    state = mmc_go_idle(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send command to set the card into ready state*/
    state = mmc_check_ocr(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to request cid of the card*/
    state = mmc_snd_all_cid(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to set the rel addr of the card*/
    state = mmc_set_rel_add(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    sd_read_response_rel_addr(pcard_specific_data);
    /*send command to req csd of the card*/
    state = mmc_snd_csd(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to select the card*/
    state = mmc_sel_card(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to set ext_csd */
    state = mmc_switch_bus_width(pcmd_handle);
    if (state != 0)
    {
        return -EIO;
    }
    /*send cmd to request extended csd of the card*/
    state = mmc_send_ext_csd(pcmd_handle, sec_num);
    return state;
}


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
    sdmmc_wait_xfer_done();
    return sdmmc_descriptor.status_code;
}

static int32_t mmc_snd_csd(cmd_parameters_t *pcmd)
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

static int32_t mmc_snd_all_cid(cmd_parameters_t *pcmd)
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

    sdmmc_set_up_xfer(sdmmc_descriptor.dma_descriptor, (uint64_t *)ext_csd_buff,
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
    sdmmc_wait_xfer_done();
    return sdmmc_descriptor.status_code;
}
#endif

static int32_t sdmmc_setup_host(void)
{
    /*
       the default mux for shared combo phy is configured
       for the nand ,if combophy is not enabled for
       sdmmc at ATF configure dfi_interface_cfg to 1 through smc
     */
    int32_t ret = CTRL_CONFIG_PASS;
    ret = sdmmc_reset_per0();
    if (ret != CTRL_CONFIG_PASS)
    {
        return ret;
    }
    ret = sdmmc_reset_configs();
    if (ret != CTRL_CONFIG_PASS)
    {
        return ret;
    }
    ret = sdmmc_init_phy();

    if (ret != CTRL_CONFIG_PASS)
    {
        return ret;
    }
    sdmmc_init_configs(sdmmc_descriptor.dev_type,
            sdmmc_descriptor.is_def_speed_supported);
    return ret;
}

uint32_t sdmmc_is_card_present(void)
{
    return sdmmc_is_card_detected();
}

static void sdmmc_wait_xfer_done(void)
{
    (void)osal_semaphore_wait(sdmmc_descriptor.semaphore_xfer,
            OSAL_TIMEOUT_WAIT_FOREVER);
}

static void sdmmc_wait_cmd_done(void)
{
    (void)osal_semaphore_wait(sdmmc_descriptor.semaphore_cmd,
            SDMMC_CMD_TIMEOUT_MS);
}

void sdmmc_irq_handler(void *data)
{
    (void)data;
    uint32_t volatile int_status = sdmmc_get_int_status();
    sdmmc_disable_int();
    sdmmc_clear_int();

    switch (int_status)
    {
        case SDMMC_CMD_CPT_INT_LOG:
            sdmmc_descriptor.status_code = 0;
            (void)osal_semaphore_post(sdmmc_descriptor.semaphore_cmd);
            break;
        case SDMMC_XFER_CPT_INT_LOG:
            if (sdmmc_descriptor.is_api_sync == true)
            {
                sdmmc_descriptor.status_code = 0;
                (void)osal_semaphore_post(sdmmc_descriptor.semaphore_xfer);
            }
            else
            {
                sdmmc_descriptor.status_code = 0;
                if (sdmmc_descriptor.xfer_call_back != NULL)
                {
                    sdmmc_descriptor.xfer_call_back(0);
                }
            }
            break;

        case SDMMC_CMD_TIMOUT_INT_LOG:
            sdmmc_descriptor.status_code = -EIO;
            (void)osal_semaphore_post(sdmmc_descriptor.semaphore_cmd);
            break;

        case SDMMC_XFER_TIMOUT_INT_LOG:
            if (sdmmc_descriptor.is_api_sync == true)
            {
                sdmmc_descriptor.status_code = XFER_TIMOUT_ERR;
                (void)osal_semaphore_post(sdmmc_descriptor.semaphore_xfer);
            }
            else
            {
                sdmmc_descriptor.status_code = XFER_TIMOUT_ERR;
                if (sdmmc_descriptor.xfer_call_back != NULL)
                {
                    sdmmc_descriptor.xfer_call_back(-EIO);
                }
            }
            break;
        default:
            /*Do Nothing*/
            break;
    }
}
