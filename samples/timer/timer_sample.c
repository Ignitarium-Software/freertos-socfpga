/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application for SoC FPGA timer
 */

/**
 * @file timer_sample.c
 * @brief Sample Application for Timer
 */

#include <stdlib.h>
#include <string.h>
#include "osal.h"
#include "osal_log.h"
#include <task.h>
#include "socfpga_timer.h"

/**
 * @defgroup timer_sample Timer
 * @ingroup samples
 *
 * Sample Application for Timer
 * @details
 * @section tim_desc Description
 * This sample application demonstrates the usage of the timer driver
 * in single shot mode. It configures a timer to trigger an interrupt
 * after a specified period and invokes a callback function when the
 * timer expires. The callback function stops the timer and posts a semaphore
 * to indicate that the timer callback has been executed.
 *
 * @section tim_param Configurable Parameters
 * - The timer period can be configured in @c TIMER_PERIOD_US macro.
 * - The timer instance can be changed in @c TIMER_INSTANCE macro.
 *
 * @section tim_how_to How to Run
 * 1. Follow the common README instructions to build and flash the application.
 * 2. Run the application on the board.
 * 3. Monitor the UART terminal for the output.
 *
 * @section tim_result Expected Results
 * - The timer starts and waits for the configured period.
 * - Once the timer expires, the callback is executed.
 * - The application prints a message indicating that the timer expired and the callback ran successfully.
 */

#define TIMER_INSTANCE     0
#define TIMER_PERIOD_US    (5 * 1000 * 1000)

osal_semaphore_def_t sem_mem;
osal_semaphore_t sem;

void timer_callback_sample(void *usr_cntxt)
{
    timer_handle_t htimer = (timer_handle_t)usr_cntxt;
    if (htimer == NULL)
    {
        /* Avoid blocking the sample if callback context is misconfigured. */
        osal_semaphore_post(sem);
        return;
    }
    /* for single shot usage stop the timer after fist interrupt */
    timer_stop(htimer);
    osal_semaphore_post(sem);
}

void timer_task(void)
{
    timer_handle_t htimer;
    BaseType_t l_ret_val;
    sem = osal_semaphore_create(&sem_mem);

    PRINT("Timer single shot usage example");

    htimer = timer_open(TIMER_INSTANCE);
    if (htimer != NULL)
    {
        timer_set_callback(htimer, timer_callback_sample, htimer);

        PRINT("Configuring the timer for 5 seconds");
        timer_set_period_us(htimer, TIMER_PERIOD_US);
        timer_start(htimer);

    }

    PRINT("Waiting for timer callback ...");
    /*
     * Wait for the timer callback to be invoked.
     * Keep a 20 msec margin to receive the callback
     */
    l_ret_val = osal_semaphore_wait(sem, 5 * 1000 + 20);
    if (l_ret_val == pdTRUE)
    {
        PRINT("Timer callback received");
    }
    else
    {
        ERROR("Failed to get timer callback");
    }

    timer_close(htimer);
    PRINT("Timer example completed");

}
