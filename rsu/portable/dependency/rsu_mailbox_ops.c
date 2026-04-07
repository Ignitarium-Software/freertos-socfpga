/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL layer for RSU mail box operations
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "socfpga_cache.h"

#include "RSU_OSAL_types.h"
#include "hal/RSU_plat_mailbox.h"
#include <utils/RSU_logging.h>
#include "socfpga_mbox_client.h"

#define RESP_SIZE                1U
#define RESP_STATUS              0U
#define RSU_GET_SPT_ADDR_RESP    16U
#define RSU_GET_STATUS_RESP      36U
#define RSU_UPDATE_ARG_SIZE      8U
#define RSU_NOTIFY_ARG_SIZE      4U
sdm_client_handle_t rsu_client = NULL;
osal_semaphore_t rsu_sem;

void rsu_callback(uint64_t *resp_data)
{
    (void)resp_data;
    osal_semaphore_post(rsu_sem);
}

static RSU_OSAL_INT plat_mbox_get_rsu_status(struct mbox_status_info *data)
{
    uint32_t *rsu_status_resp;
    uint64_t smc_resp[2] = {0};
    int ret = 0;
    if (rsu_client == NULL)
    {
        RSU_LOG_ERR("Mailbox is not initialized!!");
        return -1;
    }
    if (data == NULL)
    {
        return -EINVAL;
    }
    rsu_status_resp = pvPortMalloc(RSU_GET_STATUS_RESP);
    if (rsu_status_resp == NULL)
    {
        return -1;
    }
    cache_force_invalidate(rsu_status_resp, RSU_GET_STATUS_RESP);
    ret = mbox_send_command(rsu_client, 0x5B, NULL, 0, rsu_status_resp,
            RSU_GET_STATUS_RESP, smc_resp, sizeof(smc_resp));
    if (ret)
    {
        RSU_LOG_ERR("Failed to get the mailbox status");
        vPortFree(rsu_status_resp);
        return ret;
    }
    ret = osal_semaphore_wait(rsu_sem, OSAL_TIMEOUT_WAIT_FOREVER);
    if ((ret == pdTRUE) && (smc_resp[RESP_STATUS] == 0))
    {
        cache_force_invalidate(rsu_status_resp, smc_resp[RESP_SIZE]);
        data->current_image   =  (uint64_t)rsu_status_resp[0];
        data->fail_image      =  (uint64_t)rsu_status_resp[2];
        data->state           =  (uint64_t)rsu_status_resp[4];
        data->version         =  (uint64_t)rsu_status_resp[5];
        data->error_location  =  (uint64_t)rsu_status_resp[6];
        data->error_details   =  (uint64_t)rsu_status_resp[7];
        data->retry_counter   =  (uint64_t)rsu_status_resp[8];
    }

    vPortFree(rsu_status_resp);
    return 0;
}

static RSU_OSAL_INT plat_mbox_send_rsu_update(RSU_OSAL_U64 addr)
{
    int ret;
    uint32_t *rsu_update_args;
    uint64_t smc_resp[2] = {0};
    if (rsu_client == NULL)
    {
        RSU_LOG_ERR("Mailbox is not initialized!!");
        return -1;
    }
    rsu_update_args = pvPortMalloc(RSU_UPDATE_ARG_SIZE);
    if (rsu_update_args == NULL)
    {
        return -1;
    }
    rsu_update_args[0] = addr & 0XFFFFFFFF;
    rsu_update_args[1] = (addr >> 32) & 0xFFFFFFFF;
    cache_force_write_back(rsu_update_args, RSU_UPDATE_ARG_SIZE);
    ret = mbox_send_command(rsu_client, 0x5C, rsu_update_args,
            RSU_UPDATE_ARG_SIZE,
            NULL, 0, smc_resp, sizeof(smc_resp));
    if (ret)
    {
        RSU_LOG_ERR("Failed to send mailbox command");
        vPortFree(rsu_update_args);
        return ret;
    }
    ret = osal_semaphore_wait(rsu_sem, OSAL_TIMEOUT_WAIT_FOREVER);
    if ((ret == pdTRUE) && (smc_resp[RESP_STATUS] == 0))
    {
        vPortFree(rsu_update_args);
        return 0;
    }
    vPortFree(rsu_update_args);
    return ret;
}

static RSU_OSAL_INT plat_mbox_get_spt_addresses(
        struct mbox_data_rsu_spt_address *data)
{
    uint32_t *spt_addr_resp;
    uint64_t smc_resp[2] = {0};
    int ret = 0;
    if (rsu_client == NULL)
    {
        RSU_LOG_ERR("Mailbox is not initialized!!");
        return -1;
    }
    if (data == NULL)
    {
        return -EINVAL;
    }
    spt_addr_resp = pvPortMalloc(RSU_GET_SPT_ADDR_RESP);
    if (spt_addr_resp == NULL)
    {
        return -1;
    }
    cache_force_write_back(spt_addr_resp, RSU_GET_SPT_ADDR_RESP);
    ret = mbox_send_command(rsu_client, 0x5A, NULL, 0, spt_addr_resp,
            RSU_GET_SPT_ADDR_RESP, smc_resp, sizeof(smc_resp));
    if (ret)
    {
        RSU_LOG_ERR("Failed to get the mailbox status");
        vPortFree(spt_addr_resp);
        return ret;
    }
    ret = osal_semaphore_wait(rsu_sem, OSAL_TIMEOUT_WAIT_FOREVER);
    if ((ret == pdTRUE) && (smc_resp[0] == 0))
    {
        cache_force_invalidate(spt_addr_resp, smc_resp[RESP_SIZE]);
        data->spt0_address =  (uint64_t)((((uint64_t)spt_addr_resp[0]) <<
                32) | (uint64_t)spt_addr_resp[1]);
        data->spt1_address =  (uint64_t)((((uint64_t)spt_addr_resp[2]) <<
                32) | (uint64_t)spt_addr_resp[3]);
    }
    vPortFree(spt_addr_resp);
    return 0;
}

static RSU_OSAL_INT plat_mbox_rsu_notify(RSU_OSAL_U32 notify)
{
    int32_t ret;
    uint32_t *rsu_notify_arg;
    uint64_t smc_resp[2] = {0};
    if (rsu_client == NULL)
    {
        RSU_LOG_ERR("Mailbox is not initialized!!");
        return -1;
    }
    rsu_notify_arg = pvPortMalloc(RSU_NOTIFY_ARG_SIZE);
    if (rsu_notify_arg == NULL)
    {
        return -1;
    }
    *rsu_notify_arg = notify;
    cache_force_invalidate(rsu_notify_arg, RSU_NOTIFY_ARG_SIZE);
    ret = mbox_send_command(rsu_client, 0x5D, rsu_notify_arg,
            RSU_NOTIFY_ARG_SIZE,
            NULL, 0, smc_resp, sizeof(smc_resp));
    if (ret)
    {
        RSU_LOG_ERR("Failed to get the mailbox status");
        vPortFree(rsu_notify_arg);
        return ret;
    }
    ret = osal_semaphore_wait(rsu_sem, OSAL_TIMEOUT_WAIT_FOREVER);
    if ((ret == pdTRUE) && (smc_resp[RESP_STATUS] == 0))
    {
        vPortFree(rsu_notify_arg);
        return 0;
    }
    vPortFree(rsu_notify_arg);
    return -1;
}

static RSU_OSAL_INT plat_mbox_terminate(RSU_OSAL_VOID)
{
    int32_t ret;

    ret = mbox_close_client(rsu_client);
    if (ret)
    {
        RSU_LOG_ERR("Failed to close mailbox");
        return -1;
    }
    mbox_deinit();
    return 0;
}

RSU_OSAL_INT plat_mbox_init(struct mbox_ll_intf *mbox,
        RSU_OSAL_CHAR *config_file)
{
    (void)config_file;

    mbox_init();

    rsu_client = mbox_open_client();
    if (rsu_client == NULL)
    {
        RSU_LOG_ERR("Failure in opening mailbox client handle");
        return -EFAULT;
    }

    if (mbox_set_callback(rsu_client, rsu_callback ) != 0)
    {
        RSU_LOG_ERR("Failed to set mailbox callback");
        return -EFAULT;
    }
    rsu_sem = osal_semaphore_create(NULL);
    mbox->get_rsu_status = plat_mbox_get_rsu_status;
    mbox->send_rsu_update = plat_mbox_send_rsu_update;
    mbox->get_spt_addresses = plat_mbox_get_spt_addresses;
    mbox->rsu_notify = plat_mbox_rsu_notify;
    mbox->terminate = plat_mbox_terminate;

    return 0;
}
