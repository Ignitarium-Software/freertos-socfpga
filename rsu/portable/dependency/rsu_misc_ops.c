/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL layer for RSU miscellaneous operations
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "socfpga_flash.h"
#include "socfpga_mbox_client.h"
#include "socfpga_cache.h"
#include "hal/RSU_plat_misc.h"
#include <utils/RSU_logging.h>
#include "RSU_OSAL_types.h"

#define FIELD_EXTRACT(reg, start, end) \
    ((reg >> start) & ((1U << (end - start + 1U)) - 1U))

#define RSU_VERSION_DCMF_IDX(val)    FIELD_EXTRACT(val, 28, 31)
#define RSU_GET_STATUS_RESP    36U
#define RSU_MAX_RETRY_LIMIT (3UL)
#define DCMF_SIZE               0x080000
#define DCMF0_VERSION_OFFSET    0x000420
#define DCMF1_VERSION_OFFSET    0x080420
#define DCMF2_VERSION_OFFSET    0x100420
#define DCMF3_VERSION_OFFSET    0x180420
#define DCIO_MAX_RETRY_OFFSET   0x20018C

char buf_a[DCMF_SIZE] = {0};
char buf_b[DCMF_SIZE] = {0};

extern sdm_client_handle_t rsu_client;
extern flash_handle_t rsu_rtos_qspi_handle;
extern osal_semaphore_t rsu_sem;

static RSU_OSAL_INT rsu_get_dcmf_status(struct rsu_dcmf_status *data)
{
    uint32_t *rsu_status_resp;
    uint64_t smc_resp[2] = {0};
    uint32_t crt_dcmf, idx;
    int ret;
    if (data == NULL)
    {
        return -EINVAL;
    }
    /*Use the mailbox driver to get the status info*/
    if (rsu_client == NULL)
    {
        RSU_LOG_ERR("Failure in opening RSU client handle\r\n");
        return -1;
    }
    rsu_status_resp = pvPortMalloc(RSU_GET_STATUS_RESP);
    if (rsu_status_resp == NULL)
    {
        return -EFAULT;
    }
    cache_force_invalidate(rsu_status_resp, RSU_GET_STATUS_RESP);
    ret = mbox_send_command(rsu_client, 0x5B, NULL, 0, rsu_status_resp,
            RSU_GET_STATUS_RESP, smc_resp, sizeof(smc_resp));
    if (ret != 0)
    {
        RSU_LOG_ERR("Failed to get the mailbox status");
        vPortFree(rsu_status_resp);
        return ret;
    }
    if (osal_semaphore_wait(rsu_sem, OSAL_TIMEOUT_WAIT_FOREVER) != pdTRUE)
    {
        RSU_LOG_ERR("Failed to get the semaphore");
        vPortFree(rsu_status_resp);
        return -EFAULT;
    }
    if (smc_resp[0] != 0UL)
    {
        return -EFAULT;
    }
    cache_force_invalidate(rsu_status_resp, RSU_GET_STATUS_RESP);
    crt_dcmf =  RSU_VERSION_DCMF_IDX(rsu_status_resp[4]);

    if (rsu_rtos_qspi_handle == NULL)
    {
        RSU_LOG_ERR("Failed to open the flash");
        return -EFAULT;
    }
    ret = flash_read_sync(rsu_rtos_qspi_handle, crt_dcmf * DCMF_SIZE,
            (uint8_t *)buf_a, DCMF_SIZE);
    if (ret != 0)
    {
        RSU_LOG_ERR("Failed to get DCMF status from flash");
        return -EFAULT;
    }

    for (idx = 0; idx < 4; idx++)
    {
        data->dcmf[idx] = 0;
        if (idx == crt_dcmf)
        {
            continue;
        }
        ret = flash_read_sync(rsu_rtos_qspi_handle, crt_dcmf * DCMF_SIZE,
                (uint8_t *)buf_b, DCMF_SIZE);
        if (ret != 0)
        {
            RSU_LOG_ERR("Failed to get DCMF status from flash");
            return -EFAULT;
        }
        for (int i = 0; i < DCMF_SIZE; i++)
        {
            if (buf_a[i] != buf_b[i])
            {
                data->dcmf[idx] = 1;
                break;
            }
        }
    }
    return 0;
}

static RSU_OSAL_INT rsu_get_dcmf_version(struct rsu_dcmf_version *version)
{
    int status;
    if (version == NULL)
    {
        return -EINVAL;
    }

    if (rsu_rtos_qspi_handle == NULL)
    {
        RSU_LOG_ERR("Failed to open the flash handle");
        return -EFAULT;
    }
    uint32_t value = 0;

    status = flash_read_sync(rsu_rtos_qspi_handle, DCMF0_VERSION_OFFSET,
            (uint8_t *)&value, 4);
    if (status != 0)
    {
        RSU_LOG_ERR("Failed to read dcmf0 from flash");
        return status;
    }
    version->dcmf[0] = (RSU_OSAL_U32)value;

    status = flash_read_sync(rsu_rtos_qspi_handle, DCMF1_VERSION_OFFSET,
            (uint8_t *)&value, 4);
    if (status != 0)
    {
        RSU_LOG_ERR("Failed to read dcmf1 from flash");
        return status;
    }
    version->dcmf[1] = (RSU_OSAL_U32)value;


    status = flash_read_sync(rsu_rtos_qspi_handle, DCMF2_VERSION_OFFSET,
            (uint8_t *)&value, 4);
    if (status != 0)
    {
        RSU_LOG_ERR("Failed to read dcmf2 from flash");
        return status;
    }
    version->dcmf[2] = (RSU_OSAL_U32)value;


    status = flash_read_sync(rsu_rtos_qspi_handle, DCMF3_VERSION_OFFSET,
            (uint8_t *)&value, 4);
    if (status != 0)
    {
        RSU_LOG_ERR("Failed to read dcmf3 from flash");
        return status;
    }
    version->dcmf[3] = (RSU_OSAL_U32)value;
    return 0;
}

static RSU_OSAL_INT rsu_get_max_retry_count(RSU_OSAL_U8 *rsu_max_retry)
{
    if (rsu_max_retry == NULL)
    {
        return -EINVAL;
    }

    int status;
    uint8_t max_retry_count;

    if (rsu_rtos_qspi_handle == NULL)
    {
        RSU_LOG_ERR("Failed to open the flash handle");
        return -EFAULT;
    }
    status = flash_read_sync(rsu_rtos_qspi_handle,DCIO_MAX_RETRY_OFFSET,
            &max_retry_count, 1);
    if (status != 0)
    {
        RSU_LOG_ERR("Failed to read from the flash");
        return -EFAULT;
    }

    *rsu_max_retry = max_retry_count;

    return 0;
}

static RSU_OSAL_INT terminate(RSU_OSAL_VOID)
{
    return 0;
}

RSU_OSAL_INT plat_rsu_misc_init(struct rsu_ll_misc *misc_intf,
        RSU_OSAL_CHAR *config_file)
{
    (void)config_file;

    if (!misc_intf)
    {
        return -EINVAL;
    }

    misc_intf->rsu_get_dcmf_status = rsu_get_dcmf_status;
    misc_intf->rsu_get_dcmf_version = rsu_get_dcmf_version;
    misc_intf->rsu_get_max_retry_count = rsu_get_max_retry_count;
    misc_intf->terminate = terminate;

    return 0;
}
