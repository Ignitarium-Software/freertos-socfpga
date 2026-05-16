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
#define CONSOLE_CFG_MAX_LEN  17
/*
 * If CONSOLE_DROP_MSG_FMT is changed, update CONSOLE_DROP_MSG_LEN to fit the
 * longest possible formatted string (max 10 digits for the byte count) + '\0'.
 */
#define CONSOLE_DROP_MSG_FMT "\r\n[console: %u bytes dropped]\r\n"
#define CONSOLE_DROP_MSG_LEN 48

#ifndef configCONSOLE_MAKE_FLUSH_TASK
#define configCONSOLE_MAKE_FLUSH_TASK 0
#endif
#ifndef configCONSOLE_TASK_FUNCTION
#define configCONSOLE_TASK_FUNCTION console_clear_pending
#endif

static osal_pipe_t buf_pipe;
static uint8_t int_buf[MAX_INT_BUFF_SIZE];
static char drop_msg[CONSOLE_DROP_MSG_LEN];
static uint32_t dropped_bytes = 0;
static osal_semaphore_t signal_bytes_available;
static osal_semaphore_def_t console_write_sem_mem;
static osal_semaphore_t console_write_sem;
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

int console_init(uint32_t id, const char *config_str)
{
    int baudrate;
    int word_length;
    char parity;
    int num_stop_bits;
    int ret;

    if (hconsole_uart != NULL)
    {
        return -EBUSY;
    }

    if (config_str == NULL ||
            strnlen(config_str, CONSOLE_CFG_MAX_LEN + 1) > CONSOLE_CFG_MAX_LEN)
    {
        return -EINVAL;
    }

    /* example: 115200-8N1 */
    if (sscanf(config_str, "%d-%d%c%d", &baudrate, &word_length, &parity,
            &num_stop_bits) < 4)
    {
        return -EINVAL;
    }
    /* Create lock first so early writes can be serialized. */
    if (console_write_sem == NULL)
    {
        console_write_sem = osal_semaphore_create(&console_write_sem_mem);
        if (console_write_sem == NULL)
        {
            return -ENOMEM;
        }
        if (!osal_semaphore_post(console_write_sem))
        {
            (void)osal_semaphore_delete(console_write_sem);
            console_write_sem = NULL;
            return -ENOMEM;
        }
    }

    hconsole_uart = uart_open(id);
    if (hconsole_uart == NULL)
    {
        (void)osal_semaphore_delete(console_write_sem);
        console_write_sem = NULL;
        return -EBUSY;
    }

    console_config.baud = baudrate;
    /* Set word length (default value is 8) */
    switch (word_length)
    {
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
    switch (parity)
    {
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
        console_config.stop_bits = UART_STOP_BITS_2;
    }
    else
    {
        console_config.stop_bits = UART_STOP_BITS_1;
    }

    ret = uart_ioctl(hconsole_uart, UART_SET_CONFIG, &console_config);
    if (ret != 0)
    {
        (void)uart_close(hconsole_uart);
        hconsole_uart = NULL;
        (void)osal_semaphore_delete(console_write_sem);
        console_write_sem = NULL;
        return ret;
    }

    buf_pipe = osal_pipe_create(MAX_PIPE_SIZE, console_read_complete,
            console_write_complete);
    if (buf_pipe == NULL)
    {
        (void)uart_close(hconsole_uart);
        hconsole_uart = NULL;
        (void)osal_semaphore_delete(console_write_sem);
        console_write_sem = NULL;
        return -ENOMEM;
    }
#if configCONSOLE_MAKE_FLUSH_TASK
    signal_bytes_available = osal_semaphore_create(NULL);
    if (signal_bytes_available == NULL)
    {
        (void)osal_pipe_delete(buf_pipe);
        buf_pipe = NULL;
        (void)uart_close(hconsole_uart);
        hconsole_uart = NULL;
        (void)osal_semaphore_delete(console_write_sem);
        console_write_sem = NULL;
        return -ENOMEM;
    }

    if (!osal_task_create(console_flush_task, "CONSOLE_FLUSH",
            (void * )(0x0 | CONSOLE_FLAG_TASK_LOOP),
            configCONSOLE_FLUSH_TASK_PRIORITY))
    {
        (void)uart_close(hconsole_uart);
        hconsole_uart = NULL;
        (void)osal_semaphore_delete(console_write_sem);
        console_write_sem = NULL;
        osal_semaphore_delete(signal_bytes_available);
        signal_bytes_available = NULL;
        (void)osal_pipe_delete(buf_pipe);
        buf_pipe = NULL;
        return -ENOMEM;
    }
#endif
    return 0;
}

int console_fill_buffer(unsigned char *const buf, int length)
{
    int ret;
    int32_t poll_ret;
    uint32_t k_state = osal_get_kernel_state();

    if ((hconsole_uart == NULL) || (buf_pipe == NULL))
    {
        return -EINVAL;
    }

    if (length <= 0)
    {
        return -EINVAL;
    }

    if (k_state == OSAL_KERNEL_NOT_STARTED)
    {
        poll_ret = uart_write_polling(hconsole_uart, buf, length);
        return (poll_ret == 0) ? length : poll_ret;
    }

    if (k_state == OSAL_KERNEL_NOT_RUNNING)
    {
        /*
         * As per the current freeRTOS code writing to stream buffer
         * if there is enough space is fine (Not allowed but the code looks safe)
         */
        if (length < osal_pipe_space_available(buf_pipe))
        {
            return osal_pipe_send(buf_pipe, buf, length);
        }
    }
    if (buf == NULL || length == 0)
    {
        return 0;
    }

    ret = osal_pipe_send(buf_pipe, buf, length);

    if (ret < length)
    {
        dropped_bytes += (uint32_t)(length - ret);
    }

    return ret;
}

void console_signal_fill(void)
{
#if configCONSOLE_MAKE_FLUSH_TASK
    /*
     * If Sem fails, that means the receiver is already waiting
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
    if ((hconsole_uart == NULL) || (buf_pipe == NULL))
    {
        return -EINVAL;
    }

    int ret = console_fill_buffer(buf, length);
    console_signal_fill();
    return ret;
}

void console_clear_pending(void)
{
    int bytes_read;
    uint32_t num_bytesin_pipe;
    uint32_t drops;
    int msg_len;
#if !configCONSOLE_MAKE_FLUSH_TASK
    int32_t console_state;
#endif

    if ((hconsole_uart == NULL) || (buf_pipe == NULL))
    {
        return;
    }
    num_bytesin_pipe = osal_pipe_bytes_available(buf_pipe);
    if (num_bytesin_pipe > 0)
    {
        bytes_read = 0;
        (void)console_lock();
        do {
#if configCONSOLE_MAKE_FLUSH_TASK
            bytes_read = osal_pipe_receive(buf_pipe, int_buf, MAX_INT_BUFF_SIZE);
            uart_write_sync(hconsole_uart, int_buf, bytes_read);
#else
            /*
             * Safe to call from the idle hook: uses polling write which does not
             * call any blocking RTOS primitives. UART busy state is checked first
             * to avoid interfering with an ongoing transfer.
             */
            (void)uart_ioctl(hconsole_uart, UART_GET_TX_STATE, &console_state);
            if (console_state == -EBUSY)
            {
                break;
            }
            bytes_read = osal_pipe_receive(buf_pipe, int_buf, MAX_INT_BUFF_SIZE);
            if (uart_write_polling(hconsole_uart, int_buf, bytes_read) != 0)
            {
                break;
            }
#endif
        } while(bytes_read);
        (void)console_unlock();
    }

    if (dropped_bytes > 0)
    {
        drops = dropped_bytes;
        dropped_bytes = 0;
        msg_len = snprintf(drop_msg, sizeof(drop_msg),
                CONSOLE_DROP_MSG_FMT, (unsigned int)drops);
        if (msg_len > 0)
        {
#if configCONSOLE_MAKE_FLUSH_TASK
            uart_write_sync(hconsole_uart, (uint8_t *)drop_msg, (uint32_t)msg_len);
#else
            (void)uart_write_polling(hconsole_uart, (uint8_t *)drop_msg, (uint32_t)msg_len);
#endif
        }
    }
}

int console_read(unsigned char *const buf, int length)
{
    return uart_read_sync(hconsole_uart, buf, length);
}

int console_lock(void)
{
    if (console_write_sem != NULL)
    {
        if (!osal_semaphore_wait(console_write_sem, OSAL_TIMEOUT_WAIT_FOREVER))
        {
            return -EBUSY;
        }
        return 0;
    }
    return -ENODEV;
}

int console_unlock(void)
{
    if (console_write_sem != NULL)
    {
        if (!osal_semaphore_post(console_write_sem))
        {
            return -EPERM;
        }
        return 0;
    }
    return -ENODEV;
}

int console_deinit(void)
{
    int ret = 0;

    if (buf_pipe != NULL)
    {
        (void)osal_pipe_delete(buf_pipe);
        buf_pipe = NULL;
    }

    if (hconsole_uart != NULL)
    {
        ret = uart_close(hconsole_uart);
        if (ret == 0)
        {
            hconsole_uart = NULL;
        }
    }

    if (console_write_sem != NULL)
    {
        (void)osal_semaphore_delete(console_write_sem);
        console_write_sem = NULL;
    }
    return ret;
}
