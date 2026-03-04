/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * sample implementation for iossm
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "FreeRTOS.h"
#include "task.h"
#include "socfpga_iossm.h"
#include "iossm_sample.h"
#include "osal_log.h"

/**
 * @defgroup iossm_sample IOSSM
 * @ingroup samples
 * Sample Application for IOSSM
 * @details
 * @section iossm_desc Description
 * The Sample application demonstrates the use of the IOSSM driver.
 * It performs the following operations:
 * - Initializes the IOSSM module.
 * - Injects single-bit and double-bit ECC errors to simulate fault conditions.
 * - Demonstrates the handling and reporting of ECC error events.
 *
 * This sample can be used as a reference for understanding how to:
 * - Simulate ECC error scenarios for testing and validation.
 * - Integrate IOSSM callbacks for error reporting in embedded systems.
 * @section iossm_pre Prerequisites
 * - Requires IOSSM enabled jic image
 *
 * @section iossm_how_to How to Run
 * 1. Follow the common README for build and flashing instructions.
 * 2. Ensure IOSSM is enabled in the ATF
 * 3. Run the sample binary
 *
 * @section iossm_result Expected Results
 * - Demonstrates the handling and reporting of ECC error events.
 */

static iossm_context_t *handle;

void error_handler(void);
void iossm_task(void)
{
    int ret;
    uint32_t resp_state;
    int32_t ecc_status;

    PRINT("The IOSSM sample application for error injection starts");
    handle = iossm_open(IOSSM_INSTANCE_0);
    if (handle == NULL)
    {
        ERROR("Failed to open iossm instande");
        ERROR("Exiting from iossm sample");
        return;
    }

    ecc_status = iossm_read_ecc_status(handle);
    PRINT("Reading number of errors (%d)", ecc_status);

    ret = iossm_clear_ecc_buffer(handle);
    if (ret < 0)
    {
        ERROR("Unable to clear ecc buffer");
        ERROR("Exiting from iossm sample %d", ret);
        return;
    }

    ecc_status = iossm_read_ecc_status(handle);
    if (ecc_status < 0)
    {
        ERROR("Unable to read ecc status");
        ERROR("Exiting from iossm sample %d", ret);
        return;
    }
    PRINT("Clearing errors (%d)", ecc_status);

    iossm_set_callback(handle, error_handler);

    /* Inject double bit error*/

    resp_state = iossm_inject_dbit_err(handle);
    if (resp_state == 0)
    {
        PRINT("double bit error injected succesfully");
    }
    else
    {
        ERROR("double bit error injection failedn");
        return;
    }

    resp_state = iossm_ack_int(handle, IOSSM_ECC_UNCRCT_EVENT_DET);
    if (resp_state == 0)
    {
        PRINT("interrupt acknowledged succesfully");
    }
    else
    {
        ERROR("failed to acknowledge interrupt");
        return;
    }

    /* Inject single bit error*/

    resp_state = iossm_inject_sbit_err(handle);
    if (resp_state == 0)
    {
        PRINT("single bit error injected succesfully");
    }
    else
    {
        ERROR("single bit error injection failed");
        return;
    }

    resp_state = iossm_ack_int(handle, IOSSM_ECC_UNCRCT_EVENT_DET);
    if (resp_state == 0)
    {
        PRINT("interrupt acknowledged succesfully");
    }
    else
    {
        ERROR("failed to acknowledge interrupt");
        return;
    }

    PRINT("The IOSSM sample application for error injection completed");

    vTaskDelete(NULL);
}

void error_handler(void)
{
    uint32_t ecc_status;

    ecc_status = iossm_read_ecc_status(handle);
    PRINT("iossm error interrupt triggered (%d)\r\n", ecc_status);
}
