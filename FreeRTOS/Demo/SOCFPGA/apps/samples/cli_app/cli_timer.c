/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Implementation of CLI commands for timer
 */

/**
 * @defgroup cli_timer Timer
 * @ingroup cli
 *
 * Perform operations on timer
 *
 * @details
 * It supports the following commands:
 * - timer config &lt;instance&gt; &lt;mode&gt; &lt;period&gt;
 * - timer start
 * - timer stop
 * - timer close
 * - timer ticks
 * - timer help
 *
 * Typical usage:
 * - Use 'timer config' to configure the timer instance
 * - Use 'timer start' to start the timer
 * - Use 'timer ticks' to get the current timer ticks while timer is running
 * - Use 'timer stop' to stop the timer
 * - Use 'timer close' to close the timer instance
 *
 * @section tim_commands Commands
 * @subsection timer_config timer config
 * Configure timer instance <br>
 *
 * Usage: <br>
 * timer config &lt;instance&gt; &lt;mode&gt; &lt;period&gt; <br>
 *
 * It requires the following arguments:
 * - instance The instance of the timer. Valid values: 0 to 3.
 * - mode The mode of the timer. 1 = one-shot, 2 = free-running.
 * - period The period in microseconds for one-shot mode. Hex value 0x0 to 0xFFFFFFFF.
 *
 * @subsection timer_start timer start
 * Start timer instance <br>
 *
 * Usage: <br>
 * timer start <br>
 *
 * @subsection timer_ticks timer ticks
 * Get the current counter value of the timer instance <br>
 *
 * Usage: <br>
 * timer ticks <br>
 *
 * @subsection timer_stop timer stop
 * Stop timer instance <br>
 *
 * Usage: <br>
 * timer stop <br>
 *
 * @subsection timer_close timer close
 * Close timer instance <br>
 *
 * Usage: <br>
 * timer close <br>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <socfpga_uart.h>
#include <socfpga_timer.h>
#include "FreeRTOS_CLI.h"
#include "cli_app.h"
#include "cli_utils.h"
#include "osal_log.h"
#include "semphr.h"
#include <socfpga_gpio.h>

#define ONE_SHOT               1
#define FREE_RUNNING           2
#define FREE_RUNNING_PERIOD    0xFFFFFFFFU

uint8_t target;
extern SemaphoreHandle_t print_semaphore;
/* timer (cmd_timer) command parameters starts */
timer_handle_t timer_handle;
uint8_t is_timer_open = 0, is_timer_running = 0;

void timer_callback(void *usr_cntxt)
{
    /*
     * Runs in ISR context, htimer comes from usr_cntxt (captured at
     * timer_set_callback() registration) instead of the global timer_handle,
     * since the global can be reassigned/cleared from CLI task context
     * (timer config/close) concurrently with this ISR
     */
    timer_handle_t htimer = (timer_handle_t)usr_cntxt;
    if (htimer == NULL)
    {
        htimer = timer_handle;
    }
    timer_stop(htimer);
    timer_close(htimer);
    is_timer_running = 0;
    is_timer_open = 0;
    htimer = NULL;
    xSemaphoreGiveFromISR(print_semaphore,NULL);
}

void print_status(void *data)
{
    uint32_t *count = (uint32_t*)data;
    timer_set_callback(timer_handle, timer_callback, timer_handle);
    timer_set_period_us(timer_handle, *count);
    osal_semaphore_wait(print_semaphore,portMAX_DELAY);
    PRINT("One shot mode completed for %d ms", *count);
    PRINT("Closed timer instance.");
    vTaskDelete(NULL);
}

BaseType_t cmd_timer( char *write_buffer, size_t write_buffer_len,
        const char *command_string )
{
    const char *parameter1, *parameter2, *parameter3, *parameter4;
    uint32_t mode, get_count;
    static uint32_t count;
    uint32_t status;
    int ret_val;
    BaseType_t parameter1_str_len, parameter2_str_len,
            parameter3_str_len, parameter4_str_len, ret;
    ret = pdFALSE;
    parameter1 = FreeRTOS_CLIGetParameter(command_string, 1,
            &parameter1_str_len);

    if (parameter1 == NULL)
    {
        ret = pdTRUE;
    }
    else if (!strncmp(parameter1, "config", strlen("config")))
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        parameter3 = FreeRTOS_CLIGetParameter(command_string, 3,
                &parameter3_str_len);
        parameter4 = FreeRTOS_CLIGetParameter(command_string, 4,
                &parameter4_str_len);
        if (!strncmp(parameter2, "help", strlen("help")))
        {
            printf("\r\nConfigure timer instance"
                    "\r\n\nUsage:"
                    "\r\n  timer config <instance> <mode> <period>"
                    "\r\n\nIt requires the following arguments:"
                    "\r\n  instance    The instance of the timer. Valid values range from 0 to 3."
                    "\r\n  mode        The mode of the timer. Valid values are 1 for one shot and 2 for free running mode."
                    "\r\n  period      The period in microseconds for one shot mode. It is a hexadecimal value in the range 0x0 to 0xFFFFFFFF.");
        }
        else if ((parameter2 != NULL) || (parameter3 != NULL)
                || (parameter4 != NULL))
        {
            if ((parameter2 == NULL) || (parameter3 == NULL)
                    || (parameter4 == NULL))
            {
                ERROR("Invalid arguments.");
                return pdFAIL;
            }
            uint32_t t_id;
            ret_val = cli_get_decimal("timer config","instance", parameter2, 0, 3,
                    &t_id);
            if (ret_val != 0)
            {
                return pdFAIL;
            }
            timer_instance_t timer_id = (timer_instance_t)t_id;
            ret_val =  cli_get_decimal("timer config","mode", parameter3, 1, 2,
                    &mode);
            if (ret_val != 0)
            {
                return pdFAIL;
            }
            ret_val = cli_get_hex("timer config","period", parameter4, 0x0,
                    0xFFFFFFFF, &count);
            if (ret_val != 0)
            {
                return pdFAIL;
            }
            timer_handle_t htimer;
            /* set mode to free running if user entered value is freerunning period*/
            if (count == FREE_RUNNING_PERIOD)
            {
                mode = 2;
            }
            if (is_timer_open != 0)
            {
                printf("\r\nERROR: Timer already configured."
                        "\r\nOnly one instance supported at a time. Please run 'timer close' before a new configuration.");
                return pdFAIL;
            }
            htimer = timer_open(timer_id);
            if (htimer != NULL)
            {
                is_timer_open = 1;
                is_timer_running = 0;
                timer_handle = htimer;
                switch (mode)
                {
                case ONE_SHOT:
                    xTaskCreate(print_status,"print_status",
                            configMINIMAL_STACK_SIZE,&count,
                            tskIDLE_PRIORITY + 1,NULL);
                    PRINT("Configured timer in one shot mode.");
                    break;
                case FREE_RUNNING:
                    timer_set_callback(timer_handle, NULL, NULL);
                    timer_set_period_us(timer_handle, 0xFFFFFFFF);
                    PRINT("Configured timer in free running mode.");
                    break;
                default:
                    ERROR("Invalid arguments.");
                    timer_close(timer_handle);
                    timer_handle = NULL;
                    is_timer_open = 0;
                    ret = pdTRUE;
                    break;
                }
            }
        }
        else
        {
            ret = pdTRUE;
        }
    }
    else if (!strncmp(parameter1, "start", strlen("start")))
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        if (!strncmp(parameter2, "help", strlen("help")))
        {
            printf("\r\nStart the timer instance"
                    "\r\n\nUsage:"
                    "\r\n  timer start");

        }
        else
        {
            if (is_timer_open == 0)
            {
                ERROR("Timer not configured.");
                return pdFAIL;
            }
            is_timer_running = 1;
            timer_start(timer_handle);
        }
    }
    else if (!strncmp(parameter1, "stop", strlen("stop")))
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        if (!strncmp(parameter2, "help", strlen("help")))
        {
            printf("\r\nStop the timer instance"
                    "\r\n\nUsage:"
                    "\r\n  timer stop");
        }
        else
        {
            if (is_timer_open == 0)
            {
                printf("\r\nERROR: Timer not configured.");
                return pdFAIL;
            }
            timer_stop(timer_handle);
            is_timer_running = 0;
            PRINT("Timer stopped.");
        }
    }
    else if (!strncmp(parameter1, "close", strlen("close")))
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        if (!strncmp(parameter2, "help", strlen("help")))
        {
            printf("\r\nClose the timer instance"
                    "\r\n\nUsage:"
                    "\r\n  timer close");
        }
        else
        {
            if (is_timer_open != 0)
            {
                timer_close(timer_handle);
                is_timer_open = 0;
                timer_handle = NULL;
                PRINT("Closed timer instance");
            }
            else
            {
                PRINT("Timer not configured");
            }
        }
    }
    else if (!strncmp(parameter1, "ticks", strlen("ticks")))
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        if (!strncmp(parameter2, "help", strlen("help")))
        {
            printf("\r\nGet the current counter value of the timer instance"
                    "\r\n\nUsage:"
                    "\r\n  timer ticks");
        }
        else
        {
            if (is_timer_open == 0)
            {
                ERROR("Timer not configured.");
                return pdFAIL;
            }
            status = timer_get_value_raw(timer_handle, &get_count);
            if (status == 0)
            {
                PRINT("Current counter value: %u ticks", get_count);
            }
            else
            {
                PRINT("Timer get value raw error:  %d ", status);
            }
        }
    }
    else if (!strncmp(parameter1, "help", strlen("help")))
    {
        printf("\rPerform operations on timer"
                "\r\n\nIt supports the following commands:"
                "\r\n  timer config <instance> <mode> <period>"
                "\r\n  timer start"
                "\r\n  timer stop"
                "\r\n  timer close"
                "\r\n  timer ticks"
                "\r\n  timer help"
                "\r\n\nTypical usage:"
                "\r\n- Use 'timer config' to configure the timer instance"
                "\r\n- Use 'timer start' to start the timer"
                "\r\n- Use 'timer ticks' to get the current timer ticks while timer is running"
                "\r\n- Use 'timer stop' to stop the timer"
                "\r\n- Use 'timer close' to close the timer instance"
                "\r\n\nFor command specific help, try:"
                "\r\n  timer <command> help");
    }

    else
    {
        ERROR("Invalid arguments.");
        ret = pdTRUE;
    }
    (void) write_buffer;
    (void) write_buffer_len;

    if (ret != pdFALSE)
    {
        printf("Use 'timer help' for more information.\r\n"
                "For command specific help, try:\r\n"
                "  timer <command> help\r\n");
        ret = pdFALSE;
    }
    return ret;
}
