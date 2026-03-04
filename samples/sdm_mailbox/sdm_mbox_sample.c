/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SDM mailbox
 */

/**
 * @defgroup sdm_mbox_sample SDM Mailbox
 * @ingroup samples
 *
 * Sample Application for SDM Mailbox.
 *
 * @details
 * @section mbx_desc Description
 * This is a sample application to demonstrate reading the temperature and voltage using SDM mailbox through the SIP SVC calls in ATF.
 *
 * @section mbx_pre Prerequisites
 * ATF version 12 or above
 * @section mbx_howto How to Run
 * 1. Follow the common README for build and flashing instructions.
 * 2. Run the sample.
 *
 * @section mbx_res Expected Results
 * - The success/failure logs are displayed in the console along with the temperature and voltage.
 * @{
 */
/** @} */

#include <stdint.h>
#include <string.h>

#include "osal.h"
#include "osal_log.h"

#include "socfpga_mbox_client.h"

#define SIP_SMC_HWMON_TEMP       0x420000E8
#define SIP_SMC_HWMON_VOLT       0x420000E9U
#define VCC_CHANNEL              0x4U
#define READ_PEAK_TEMPERATURE    0x1U
#define MBOX_TIMEOUT             1000U

osal_semaphore_def_t mbox_sem_mem;
osal_semaphore_t mbox_sem;

void mbox_callback(uint64_t *resp_values)
{
    (void)resp_values;
    osal_semaphore_post(mbox_sem);
}
void mbox_sample_task(void)
{
    int ret;
    uint64_t channel, smc_fid, resp_data[2] = {0};
    float voltage = 0, temp = 0;
    sdm_client_handle_t mbox_handle;
    PRINT("Hardware monitor Voltage and Temperature readout sample");
    mbox_sem = osal_semaphore_create(&mbox_sem_mem);

    if (mbox_sem == NULL)
    {
        ERROR("Failed to create semaphore");
        return;
    }

    ret = mbox_init();
    if (ret != 0)
    {
        ERROR("Failed to initialize mailbox");
        return;
    }

    /*Open a Client to begin Mailbox operations*/
    mbox_handle = mbox_open_client();
    PRINT("Mailbox Client opened");
    if (mbox_handle != NULL)
    {
        /*Function ID to read voltage*/
        smc_fid = SIP_SMC_HWMON_VOLT;
        /*0x4 specifies which channel to read voltage from. We pass this value
         *  as an argument*/
        channel = VCC_CHANNEL;
        /* Register a callback function to know when response is ready */
        ret = mbox_set_callback(mbox_handle, mbox_callback);
        if (ret == 0)
        {
            PRINT("Mailbox Callback Registered");
            PRINT("Sending Mailbox Command");
            ret = sip_svc_send(mbox_handle, smc_fid, &channel, sizeof(channel),
                    resp_data, sizeof(resp_data));
            if (ret == 0)
            {
                /* Wait for the callback function */
                ret = osal_semaphore_wait(mbox_sem, MBOX_TIMEOUT);
                if (ret == pdTRUE)
                {
                    /*
                     * The mailbox response error is stored in resp_data[0] and the
                     * voltage value is stored in resp_data[1] if the mailbox
                     * command is a success. The value received has 16bits
                     * as the fractional value so we divide the response
                     * by 65536 to obtain the correct value
                     */
                    PRINT("Mailbox callback received");
                    if (resp_data[0] != 0)
                    {
                        ERROR("Error in mailbox command:%ld", resp_data[0]);
                        return;
                    }
                    voltage = (float)resp_data[1] / 65536;
                }
                else
                {
                    ERROR("Failed to get Mailbox callback");
                    return;
                }
            }
            else
            {
                ERROR("Failed to send command");
                return;
            }
        }
        /*Function ID to read temperature*/
        smc_fid = SIP_SMC_HWMON_TEMP;
        /*0x1 specifies to read highest temperature*/
        channel = READ_PEAK_TEMPERATURE;

        PRINT("Sending Mailbox Command");
        ret = sip_svc_send(mbox_handle, smc_fid, &channel, sizeof(channel),
                resp_data, sizeof(resp_data));
        if (ret == 0)
        {
            ret = osal_semaphore_wait(mbox_sem, MBOX_TIMEOUT);
            if (ret == pdTRUE)
            {
                PRINT("Mailbox response received");
                if (resp_data[0] != 0)
                {
                    ERROR("Error in mailbox command:%ld", resp_data[0]);
                    return;
                }
                /* For temperature readout, 8 bits are for representing decimal
                 * values, so we divide by 256 to get actual value */
                temp = (float)resp_data[1] / 256;
            }
            else
            {
                ERROR("Failed to get callback");
            }
        }
        else
        {
            ERROR("Failed to send mailbox command");
            return;
        }
        PRINT("Temperature: %0.2f C", temp);
        PRINT("Voltage: %0.4f V", voltage);
    }
    else
    {
        ERROR("Error opening client handle");
        return;
    }
    /*Close the Client Handle*/
    ret = mbox_close_client(mbox_handle);
    if (ret == 0)
    {
        PRINT("Mailbox Client Closed");
    }
    else
    {
        ERROR("Failed to close Mailbox Client");
    }
    (void)mbox_deinit();
    osal_semaphore_delete(mbox_sem);
    PRINT("SDM mailbox Voltage and Temperature Readout Sample completed");
}
