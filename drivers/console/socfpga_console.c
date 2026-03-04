/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Console driver implementation using UART driver
 */

#include <stdio.h>
#include <stdint.h>
#include <socfpga_uart.h>
#include "socfpga_console.h"
#include "osal.h"

#define RETRY_MAX_COUNT    10

#define CONSOLE_FLAG_TASK_LOOP 0x1

#define MAX_PIPE_SIZE        4096
#define MAX_INT_BUFF_SIZE    128

#ifndef configCONSOLE_MAKE_FLUSH_TASK
#define configCONSOLE_MAKE_FLUSH_TASK 0
#endif
#ifndef configCONSOLE_TASK_FUNCTION
#define configCONSOLE_TASK_FUNCTION console_clear_pending
#endif

static osal_pipe_t buffer_pipe;
static uint8_t intermediate_buffer[MAX_INT_BUFF_SIZE];
static osal_semaphore_t signal_bytes_available;
uart_handle_t hconsole_uart = NULL;
uart_config_t console_config;

static void console_flush_task(void *flags)
{
    while ((((uintptr_t)flags) & CONSOLE_FLAG_TASK_LOOP) != 0U)
    {
        bool err = osal_semaphore_wait(signal_bytes_available,
                OSAL_TIMEOUT_WAIT_FOREVER);
        /* semaphore returns TRUE if signaled success
         * Assert if we get a time out as it is not a valid state here
         * */
        osal_assert(err);
        configCONSOLE_TASK_FUNCTION();
    }
    osal_task_delete();
}

/*
 * Defining console_write_complete and console_read_complete just to prevent
 * context switch here
 */
static void console_write_complete(osal_pipe_t pipe, long is_isr,
                                   long *need_ctx_switch)
{
    (void)pipe;
    (void)is_isr;
    (void)need_ctx_switch;
}

static void console_read_complete(osal_pipe_t pipe, BaseType_t is_isr,
                                   BaseType_t *need_ctx_switch)
{
    (void)pipe;
    (void)is_isr;
    (void)need_ctx_switch;
}

int console_init(int id, const char *config_str)
{
    int ret = 0;
    int baudrate;
    int word_length;
    char parity;
    int num_stop_bits;

    if (config_str == NULL)
    {
        return -EINVAL;
    }

    /* example: 115200-8N1 */
    if (sscanf(config_str, "%d-%d%c%d", &baudrate, &word_length, &parity, &num_stop_bits) < 4)
    {
        ret = -EINVAL;
    }
    else
    {
        hconsole_uart = uart_open(id);
        if (hconsole_uart == NULL)
        {
            ret = -EBUSY;
        }


        console_config.baud = baudrate;
        /* Set word length (default value is 8) */
        switch (word_length) {
            case 5:
                console_config.wlen = 5;
                break;
            case 6:
                console_config.wlen = 6;
                break;
            case 7:
                console_config.wlen = 7;
                break;
            case 8:
                console_config.wlen = 8;
                break;
            default:
                console_config.wlen = 8;
                break;
        }
        /* Set parity (default is None) */
        switch (parity) {
            case 'n':
            case 'N':
                console_config.parity = UART_PARITY_NONE;
                break;
            case 'o':
            case 'O':
                console_config.parity = UART_PARITY_ODD;
                break;
            case 'e':
            case 'E':
                console_config.parity = UART_PARITY_EVEN;
                break;
            default:
                console_config.parity = UART_PARITY_NONE;
                break;
        }
        if (num_stop_bits > 1)
        {
            console_config.stop_bits = UART_STOP_BITS_1;
        }
        else
        {
            console_config.stop_bits = UART_STOP_BITS_2;
        }

        ret = uart_ioctl(hconsole_uart, UART_SET_CONFIG, &console_config);
    }

    if (ret == 0)
    {
        buffer_pipe = osal_pipe_create(MAX_PIPE_SIZE, console_read_complete,
                console_write_complete);
        if ((buffer_pipe == NULL))
        {
            (void)uart_close(hconsole_uart);
            hconsole_uart = NULL;
            return -ENOMEM;
        }
#if configCONSOLE_MAKE_FLUSH_TASK
        signal_bytes_available = osal_semaphore_create(NULL);
        if (signal_bytes_available == NULL)
        {
            return -ENOMEM;
        }
        if (osal_task_create(console_flush_task, "CONSOLE_FLUSH",
                (void * )(0x0 | CONSOLE_FLAG_TASK_LOOP),
                configCONSOLE_FLUSH_TASK_PRIORITY) < 0)
        {
            return -ENOMEM;
        }
#endif
    }

    return 0;
}

int console_fill_buffer(unsigned char *const buf, int length)
{
    int ret, bytes_read;
    uint32_t num_bytesin_pipe = 0;
    uint32_t bytes_written;
    int32_t console_state;
    uint32_t k_state = osal_get_kernel_state();

    if (k_state == OSAL_KERNEL_NOT_STARTED)
    {
        uart_write_polling(hconsole_uart, buf, length);
        return length;
    }

    if (k_state == OSAL_KERNEL_NOT_RUNNING)
    {
        /*
         * As pre the current freeRTOS code writing to stream buffer
         * if there is enough space is fine (Not allowed but the code looks safe)
         */
        if (length < osal_pipe_space_available(buffer_pipe))
        {
            return osal_pipe_send(buffer_pipe, buf, length);
        }
    }
    if (buf == NULL || length == 0)
    {
        return 0;
    }

    ret = osal_pipe_send(buffer_pipe, buf, length);

    return ret;
}

void console_signal_fill(void)
{
#if configCONSOLE_MAKE_FLUSH_TASK
    /*
     * If Sem fails, that means the reciever is already waiting
     * We can ignore that case as next pass will correct it
     */
    if (signal_bytes_available != NULL)
    {
        (void)osal_semaphore_post(signal_bytes_available);
    }
#endif
}

int console_write(unsigned char *const buf, int length)
{
    int ret = console_fill_buffer(buf, length);
    console_signal_fill();
    return ret;
}

void console_clear_pending(void)
{
    int bytes_read;
    int32_t console_state;
    uint32_t num_bytesin_pipe;

    if (buffer_pipe == NULL)
    {
        return;
    }
    num_bytesin_pipe = osal_pipe_bytes_available(buffer_pipe);
    if (num_bytesin_pipe > 0)
    {
        bytes_read = 0;
        do {
#if !configCONSOLE_MAKE_FLUSH_TASK
            /*
             * Check if lock could be acquired.
             * Use non blocking apis only to make this
             * usable from idle tasks.
             * As Idle task should not be blocked/suspended.
             * */
            (void)uart_ioctl(hconsole_uart, UART_GET_TX_STATE, &console_state);
            if (console_state == -EBUSY)
            {
                break;
            }
#endif
            bytes_read = osal_pipe_receive(buffer_pipe, intermediate_buffer, MAX_INT_BUFF_SIZE);
            uart_write_sync(hconsole_uart, intermediate_buffer, bytes_read);
        } while(bytes_read);
    }
}

int console_read(unsigned char *const buf, int length)
{
    return uart_read_sync(hconsole_uart, buf, length);
}

int console_deinit(void)
{
    int ret = uart_close(hconsole_uart);
    if (ret == 0)
    {
        hconsole_uart = NULL;
    }
    return ret;
}
