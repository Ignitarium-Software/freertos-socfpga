/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Driver implementation for SEU driver
 */

/*
 * This driver implements the APIs to exercise the SEU features.
 * Single event upsets (SEUs) are rare and unintended changes in the internal memory
 * elements of an FPGA caused by cosmic radiation. The memory state change is a soft
 * error with no permanent damage but the FPGA may operate erroneously until
 * background scrubbing fixes the upset.
 *
 * The SEU driver supports error injection. The below diagram show the
 * SEU error Injection Flow (via SDM Mailbox):
 *
 * +------------------------------+
 * |  Prepare Injection Params    |
 * |  (Sector, CRAM, Cycles)      |
 * +--------------+---------------+
 *                |
 *                v
 * +------------------------------+
 * |  Send Command via Mailbox    |
 * +--------------+---------------+
 *                |
 *                v
 * +------------------------------+
 * | Injection Triggered Securely |
 * +--------------+---------------+
 *                |
 *                v
 * +------------------------------+
 * |  Receive interrupt Callback  |
 * +--------------+---------------+
 *                |
 *                v
 * +------------------------------+
 * | Check Stats for              |
 * | error Detection & Correction |
 * +--------------+---------------+
 *                |
 *                v
 * +------------------------------+
 * |  review Timestamps & Status  |
 * +--------------+---------------+
 *                |
 *                v
 * +------------------------------+
 * |     End of Injection Flow    |
 * +------------------------------+
 */
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "osal_log.h"
#include "osal.h"
#include "socfpga_seu.h"
#include "socfpga_mbox_client.h"
#include "socfpga_cache.h"
#include "socfpga_interrupt.h"

#define SEU_READ_ERR_CMD           0x3c
#define SEU_READ_ERR_RESP          12U
#define SEU_INSERT_ECC_ERR_CMD     0x42
#define SEU_INSERT_ECC_ERR_LEN     4U
#define SEU_INSERT_SAFE_ERR_CMD    0x41
#define SEU_INSERT_SAFE_ERR_LEN    8U
#define SEU_READ_STATS             0x40
#define SEU_READ_STATS_LEN         4U
#define SEU_READ_STAT_RESP         24U
#define SEU_MBOX_STATUS            0U
#define SEU_MBOX_RESP_SIZE         1U

#define COMMAND_TIMEOUT    2000U
#define SEU_BUFFER_SIZE    16

/*NOTE:enabling seu interrupt only for seu injections*/
struct seu_context
{
    osal_semaphore_def_t seu_semphr_def;
    osal_semaphore_t seu_semphr;
    sdm_client_handle_t pseu_handle;
    seu_call_back_t seu_call_back;
};

static struct seu_context seu_descriptor;

void seu_irq_handler(void *param);

void seu_mailbox_complete(uint64_t *resp_data);

void seu_set_call_back(seu_call_back_t call_back)
{
    seu_descriptor.seu_call_back = call_back;
}

int32_t seu_init(void)
{
    int32_t ret;
    socfpga_interrupt_err_t int_ret;

    ret = mbox_init();
    if (ret != 0)
    {
        ERROR("Cannot initialise mailbox");
        return -EIO;
    }

    seu_descriptor.seu_semphr = osal_semaphore_create(
            &seu_descriptor.seu_semphr_def);
    seu_descriptor.pseu_handle = mbox_open_client();
    if (seu_descriptor.pseu_handle == NULL)
    {
        ERROR("Failed to open mbox client");
        return -EIO;
    }

    int_ret = interrupt_register_isr(SDM_HPS_SPARE_INTR0, seu_irq_handler,
            NULL);
    if (int_ret != ERR_OK)
    {
        return -EIO;
    }
    return 0;
}

int32_t seu_insert_safe_err(seu_err_params_t err_params)
{
    /* Providing 64 byte aligned address for cache operations */
    uint32_t seu_inject_arg_buff[SEU_BUFFER_SIZE] __attribute__((aligned(64)));
    int ret;
    /* SEU is implemented using the generic mailbox commands.
     * This SMC call returns the status of mailbox comand
     * and response size of the mailbox response. */
    uint64_t smc_resp[2] =
    {
        0
    };
    if (seu_descriptor.pseu_handle == NULL)
    {
        ERROR("error opening mailbox client");
        return -EIO;
    }
    /* Preserve bit fields of other params */
    if ((err_params.injection_cycle >= 4U) || (err_params.cram_sel0 > 0xFU) ||
            (err_params.cram_sel1 > 0xFU) || (err_params.no_of_injection >
            0xFU))
    {
        ERROR("Invalid  parameters");
        return -EINVAL;
    }

    /* Check for valid cram combinations for basic error injections */
    if ((err_params.cram_sel0  == err_params.cram_sel1) &&
            (err_params.no_of_injection != 0U))
    {
        ERROR("Invalid  parameters");
        return -EINVAL;
    }

    /* Prepare parameter set for error injection */
    if (interrupt_enable(SDM_HPS_SPARE_INTR0, GIC_INTERRUPT_PRIORITY_SEU) !=
            ERR_OK)
    {
        ERROR("SEU interrupt enable failed");
        return -EIO;
    }
    seu_inject_arg_buff[0] = ((uint32_t)err_params.sector_addr << 16) |
            ((uint32_t)err_params.injection_cycle << 4) |
            ((uint32_t)err_params.no_of_injection);
    seu_inject_arg_buff[1] = ((uint32_t)err_params.cram_sel0) |
            ((uint32_t)err_params.cram_sel1 << 4);
    ret = mbox_set_callback(seu_descriptor.pseu_handle, seu_mailbox_complete);
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        return -EIO;
    }
    DEBUG("SEU sector addr: %x, cram_sel0: %x, cram_sel1: %x, "
            "injection_cycle: %x, no_of_injection: %x", err_params.sector_addr,
            err_params.cram_sel0, err_params.cram_sel1,
            err_params.injection_cycle, err_params.no_of_injection);
    DEBUG("SEU inject args: %x %x", seu_inject_arg_buff[0],
            seu_inject_arg_buff[1]);
    cache_force_write_back(seu_inject_arg_buff, SEU_INSERT_SAFE_ERR_LEN);
    ret = mbox_send_command(seu_descriptor.pseu_handle, SEU_INSERT_SAFE_ERR_CMD,
            seu_inject_arg_buff, SEU_INSERT_SAFE_ERR_LEN, NULL, 0, smc_resp,
            sizeof(smc_resp));

    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        return -EIO;
    }
    if (osal_semaphore_wait(seu_descriptor.seu_semphr, COMMAND_TIMEOUT) != true)
    {
        return -EIO;
    }
    DEBUG("SEU insert safe error response: x1: %x x2: %x", smc_resp[0],
            smc_resp[1]);
    if (smc_resp[SEU_MBOX_STATUS] != 0UL)
    {
        ERROR("SEU error injection failed");
        return -EIO;
    }
    return 0;
}

read_err_data_t seu_read_err(void)
{
    uint32_t read_err_resp_buff[SEU_BUFFER_SIZE] __attribute__((aligned(64)));
    int ret;
    read_err_data_t err_data;
    uint64_t smc_resp[2] =
    {
        0
    };

    ret = mbox_set_callback(seu_descriptor.pseu_handle, seu_mailbox_complete);
    (void)memset(&err_data, 0, sizeof(read_err_data_t));
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        err_data.op_state = -EIO;
        return err_data;
    }
    DEBUG("SEU read error command: no args");
    cache_force_invalidate(read_err_resp_buff, SEU_READ_ERR_RESP);
    ret = mbox_send_command(seu_descriptor.pseu_handle, SEU_READ_ERR_CMD, NULL,
            0, read_err_resp_buff, SEU_READ_ERR_RESP, smc_resp,
            sizeof(smc_resp));
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        err_data.op_state = -EIO;
        return err_data;
    }
    if (osal_semaphore_wait(seu_descriptor.seu_semphr, COMMAND_TIMEOUT) != true)
    {
        ERROR("SDM mailbox error");
        err_data.op_state = -EIO;
        return err_data;
    }
    DEBUG("SEU read error response: x1: %x x2: %x", smc_resp[0], smc_resp[1]);
    if (smc_resp[SEU_MBOX_STATUS] != 0UL)
    {
        ERROR("SEU read error failed");
        return err_data;
    }
    /* Prepare and structure read error data */
    cache_force_invalidate(read_err_resp_buff, smc_resp[SEU_MBOX_RESP_SIZE]);
    err_data.op_state = 0;
    err_data.err_cnt = read_err_resp_buff[0];
    err_data.sector_addr = (uint8_t)((read_err_resp_buff[1] >> 16) & 0xFFU);
    err_data.err_type = (uint8_t)((read_err_resp_buff[2] >> 29) & 0x7U);
    err_data.node_specific_status = (uint16_t)(read_err_resp_buff[2] & 0x7FFU);
    err_data.correction_status = ((read_err_resp_buff[2] & (1UL << 28)) != 0U);

    return err_data;
}

seu_stat_t seu_read_stat(uint8_t sec_addr)
{
    uint32_t read_stat_resp_buff[48] __attribute__((aligned(64)));
    uint32_t mbox_arg[SEU_BUFFER_SIZE] __attribute__((aligned(64)));
    int ret;
    seu_stat_t seu_read_stats;
    uint64_t smc_resp[2] =
    {
        0
    };

    (void)memset(&seu_read_stats, 0, sizeof(seu_stat_t));

    mbox_arg[0] = (uint32_t)sec_addr << 16;
    ret = mbox_set_callback(seu_descriptor.pseu_handle, seu_mailbox_complete);
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        seu_read_stats.op_state = -EIO;
        return seu_read_stats;
    }
    DEBUG("SEU read stats command: sector addr: %x", sec_addr);
    DEBUG("SEU read stats args: %x", mbox_arg[0]);
    cache_force_write_back(&mbox_arg[0], sizeof(mbox_arg));
    cache_force_invalidate(read_stat_resp_buff, SEU_READ_STAT_RESP);

    ret = mbox_send_command(seu_descriptor.pseu_handle, SEU_READ_STATS,
            mbox_arg, SEU_READ_STATS_LEN, read_stat_resp_buff,
            SEU_READ_STAT_RESP, smc_resp, sizeof(smc_resp));
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        seu_read_stats.op_state = -EIO;
        return seu_read_stats;
    }
    if (osal_semaphore_wait(seu_descriptor.seu_semphr, COMMAND_TIMEOUT) != true)
    {
        ERROR("SDM mailbox error");
        seu_read_stats.op_state = -EIO;
        return seu_read_stats;
    }
    DEBUG("SEU read stats response: x1: %x x2: %x", smc_resp[0], smc_resp[1]);
    if (smc_resp[SEU_MBOX_STATUS] != 0UL)
    {
        ERROR("SEU read stats failed");
        seu_read_stats.op_state = -EIO;
        return seu_read_stats;
    }
    cache_force_invalidate(read_stat_resp_buff, SEU_READ_STAT_RESP);
    /* Prepare and structure read error stats */
    seu_read_stats.op_state = 0;
    seu_read_stats.t_seu_cycle = read_stat_resp_buff[0];
    seu_read_stats.t_seu_detect = read_stat_resp_buff[1];
    seu_read_stats.t_seu_correct = read_stat_resp_buff[2];
    seu_read_stats.t_seu_inject_detect = read_stat_resp_buff[3];
    seu_read_stats.t_sdm_seu_poll_interval = read_stat_resp_buff[4];

    return seu_read_stats;
}

int32_t seu_insert_ecc_err(uint8_t err_type, uint8_t ram_id, uint8_t
        sector_addr)
{
    int ret;
    uint32_t mbox_arg[SEU_BUFFER_SIZE] __attribute__((aligned(64)));
    uint64_t smc_resp[2] =
    {
        0
    };

    if (seu_descriptor.pseu_handle == NULL)
    {
        ERROR("error opening mailbox client");
        return -EIO;
    }
    /* Preserve bit fields of other params */
    if ((err_type > 2U) || (ram_id > 0x1FU))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    /* Prepare parameter set for error injection */
    mbox_arg[0] = ((uint32_t)sector_addr << 16) | ((uint32_t)ram_id << 2) |
            ((uint32_t)err_type);
    ret = mbox_set_callback(seu_descriptor.pseu_handle, seu_mailbox_complete);
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        return -EIO;
    }
    cache_force_write_back(&mbox_arg[0], sizeof(mbox_arg));

    ret = mbox_send_command(seu_descriptor.pseu_handle, SEU_INSERT_ECC_ERR_CMD,
            mbox_arg, SEU_INSERT_ECC_ERR_LEN, NULL, 0, smc_resp,
            sizeof(smc_resp));
    if (ret != 0)
    {
        ERROR("SDM mailbox error");
        return -EIO;
    }
    if (osal_semaphore_wait(seu_descriptor.seu_semphr, COMMAND_TIMEOUT) != true)
    {
        return -EIO;
    }
    DEBUG("SEU insert ECC error response: x1: %x x2: %x", smc_resp[0],
            smc_resp[1]);
    if (smc_resp[SEU_MBOX_STATUS] != 0UL)
    {
        ERROR("SEU ECC error injection failed");
        return -EIO;
    }
    return 0;
}

int32_t seu_deinit(void)
{
    int32_t ret;
    ret = mbox_close_client(seu_descriptor.pseu_handle);
    if (ret != 0)
    {
        ERROR("Failed to close MBOX client");
        return -EIO;
    }
    ret = mbox_deinit();
    if (ret != 0)
    {
        ERROR("Failed to close MBOX");
        return -EIO;
    }

    return 0;
}

void seu_irq_handler(void *param)
{
    (void)param;
    if (seu_descriptor.seu_call_back != NULL)
    {
        seu_descriptor.seu_call_back();
    }

    (void)interrupt_spi_disable(SDM_HPS_SPARE_INTR0);
}

void seu_mailbox_complete(uint64_t *resp_data)
{
    (void)resp_data;
    (void)osal_semaphore_post(seu_descriptor.seu_semphr);
}
