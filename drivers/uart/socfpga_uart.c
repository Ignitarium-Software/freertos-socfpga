/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for UART
 */
#include <string.h>
#include <errno.h>
#include "socfpga_uart.h"
#include "socfpga_uart_ll.h"
#include "socfpga_uart_reg.h"
#include "socfpga_interrupt.h"
#include "osal.h"

#define GET_INT_ID(instance)    ((instance == 1U) ? (UART1IRQ) : (UART0IRQ))
struct uart_descriptor
{
    BaseType_t is_open;
    uint32_t instance;
    BaseType_t tx_is_busy;
    BaseType_t rx_is_busy;
    BaseType_t tx_is_async;
    BaseType_t rx_is_async;
    uint32_t base_address;
    size_t tx_bytes_left;
    size_t rx_bytes_left;
    size_t tx_size;
    size_t rx_size;
    uint8_t *tx_buf;
    uint8_t *rx_buf;
    uart_callback_t callback_fn;
    void *cb_user_context;
    osal_mutex_def_t mutex_mem;
    osal_semaphore_def_t wr_sem_mem;
    osal_semaphore_def_t rd_sem_mem;
    osal_mutex_t mutex;
    osal_semaphore_t rd_sem;
    osal_semaphore_t wr_sem;
};

static struct uart_descriptor uart_descriptors[UART_MAX_INSTANCE];

void uart_isr(void *param);

/**
 * @brief Check if the UART handle is valid
 */
static BaseType_t uart_is_handle_valid(uart_handle_t handle)
{
    if ((handle == &uart_descriptors[0]) || (handle == &uart_descriptors[1]))
    {
        return 1;
    }
    return 0;
}

uart_handle_t uart_open(uint32_t instance)
{
    uart_handle_t handle;
    socfpga_hpu_interrupt_t int_id;
    socfpga_interrupt_err_t int_ret;

    if (instance >= UART_MAX_INSTANCE)
    {
        return NULL;
    }

    handle = &(uart_descriptors[instance]);

    if (handle->is_open == true)
    {
        return NULL;
    }

    (void)memset(handle, 0, sizeof(struct uart_descriptor));
    handle->is_open = 1;
    handle->instance = instance;
    handle->base_address = GET_UART_BASE_ADDRESS(instance);

    int_id = GET_INT_ID(instance);
    int_ret = interrupt_register_isr(int_id, uart_isr, handle);
    if (int_ret != ERR_OK)
    {
        return NULL;
    }

    int_ret = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_UART);
    if (int_ret != ERR_OK)
    {
        return NULL;
    }

    handle->mutex = osal_mutex_create(&handle->mutex_mem);
    handle->rd_sem = osal_semaphore_create(&handle->rd_sem_mem);
    handle->wr_sem = osal_semaphore_create(&handle->wr_sem_mem);
    if ((handle->mutex == NULL) || (handle->rd_sem == NULL) || (handle->wr_sem == NULL))
    {
        handle->is_open = 0;
        return NULL;
    }
    uart_init(instance);
    return handle;
}

int32_t uart_ioctl(uart_handle_t const huart, uart_ioctl_t cmd, void *const buf)
{
    uart_config_t config =
    {
        0
    };
    int32_t res = 0;
    uint16_t bytes_left;

    if (uart_is_handle_valid(huart) == 0)
    {
        return -EINVAL;
    }
    if (buf == NULL)
    {
        return -EINVAL;
    }

    switch (cmd)
    {
        case UART_SET_CONFIG:
            if (huart->tx_is_busy || huart->rx_is_busy)
            {
                res = -EBUSY;
                break;
            }
            config = *(uart_config_t *)buf;

            uart_set_config(huart->base_address, config.parity,
                    config.stop_bits, config.wlen);

            if (uart_set_baud(huart->base_address, config.baud) != 1U)
            {
                res = -EINVAL;
            }
            break;

        case UART_GET_CONFIG:
            uart_get_config(huart->base_address, &config.baud, &config.parity,
                    &config.stop_bits, &config.wlen);
            *(uart_config_t *)buf = config;
            break;

        case UART_GET_TX_NBYTES:
            bytes_left = (uint16_t)((huart->tx_size - huart->tx_bytes_left) &
                    0xFFFFU);
            *(uint16_t *)buf = bytes_left;
            break;

        case UART_GET_RX_NBYTES:
            bytes_left = (uint16_t)((huart->rx_size - huart->rx_bytes_left) &
                    0xFFFFU);
            *(uint16_t *)buf = bytes_left;
            break;

        case UART_GET_TX_STATE:
            *(int32_t *)buf = 0;
            if (huart->tx_is_busy != 0)
            {
                *(int32_t *)buf = -EBUSY;
            }
            break;

        case UART_GET_RX_STATE:
            *(int32_t *)buf = 0;
            if (huart->rx_is_busy != 0)
            {
                *(int32_t *)buf = -EBUSY;
            }
            break;

        default:
            res = -EINVAL;
            break;
    }
    return res;
}

int32_t uart_set_callback(uart_handle_t const huart, uart_callback_t callback,
        void *param)
{
    if (huart == NULL)
    {
        return -EINVAL;
    }

    if (huart->tx_is_busy || huart->rx_is_busy)
    {
        return -EBUSY;
    }

    huart->callback_fn = callback;
    huart->cb_user_context = param;

    return 0;
}

/**
 * @brief Start writing data to transmit FIFO and enable tx interrupt
 * Update the number of bytes
 */
static void uart_write(uart_handle_t huart, uint8_t *const buffer,
        uint32_t nbytes)
{
    uint16_t byte_count = 0U;
    huart->tx_bytes_left = nbytes;

    byte_count = uart_write_fifo(huart->base_address, buffer, nbytes);

    huart->tx_bytes_left = huart->tx_bytes_left - byte_count;
    huart->tx_buf += byte_count;

    uart_enable_interrupt(huart->base_address, INTERRUPT_TX);
}

/**
 * @brief Start reading data from receive FIFO
 * Enable rx interrupt if required
 */
static int32_t uart_read(uart_handle_t huart, uint8_t *const buffer,
        uint32_t nbytes)
{
    uint16_t byte_count = 0U;
    huart->rx_bytes_left = nbytes;

    byte_count = uart_read_fifo(huart->base_address, buffer, nbytes);

    huart->rx_bytes_left -= byte_count;
    huart->rx_buf += byte_count;

    if (huart->rx_bytes_left > 0U)
    {
        uart_enable_interrupt(huart->base_address, INTERRUPT_RX);
    }
    else
    {
        if (huart->rx_is_async == true)
        {

            if (huart->callback_fn != NULL)
            {
                huart->callback_fn(UART_RD_DONE, huart->cb_user_context);
            }
            huart->rx_is_async = false;
            huart->rx_is_busy = false;
        }
        else
        {
            if (osal_semaphore_post(huart->rd_sem) == false)
            {
                return -EIO;
            }
        }
    }

    return 0;
}

int32_t uart_write_async(uart_handle_t const huart, uint8_t *const buf,
        uint32_t nbytes)
{
    if ((uart_is_handle_valid(huart) == 0) || (buf == NULL) || (nbytes == 0U))
    {
        return -EINVAL;
    }

    if (osal_mutex_lock(huart->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (huart->is_open == 0)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EINVAL;
        }

        if (huart->tx_is_busy == true)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EBUSY;
        }

        huart->tx_is_busy = true;
        huart->tx_is_async = true;
        if (osal_mutex_unlock(huart->mutex) == false)
        {
            return -EIO;
        }
    }

    huart->tx_buf = buf;
    huart->tx_size = nbytes;

    uart_write(huart, buf, nbytes);

    return 0;
}

int32_t uart_write_sync(uart_handle_t const huart, uint8_t *const buf,
        uint32_t nbytes)
{
    BaseType_t tx_sem_return;
    if ((uart_is_handle_valid(huart) == 0) || (buf == NULL) || (nbytes == 0U))
    {
        return -EINVAL;
    }

    if (osal_mutex_lock(huart->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (huart->is_open == 0)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EINVAL;
        }

        if (huart->tx_is_busy == true)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EBUSY;
        }

        huart->tx_is_busy = true;
        huart->tx_is_async = false;
        if (osal_mutex_unlock(huart->mutex) == false)
        {
            return -EIO;
        }
    }

    huart->tx_buf = buf;
    huart->tx_size = nbytes;

    uart_write(huart, buf, nbytes);

    tx_sem_return =
            osal_semaphore_wait(huart->wr_sem, OSAL_TIMEOUT_WAIT_FOREVER);
    if (tx_sem_return == 1)
    {
        huart->tx_is_busy = false;
    }

    return 0;
}

int32_t uart_write_polling(uart_handle_t const huart, uint8_t *const buf,
        uint32_t nbytes)
{
    if (!(uart_is_handle_valid(huart)) || (buf == NULL) || (nbytes == 0U))
    {
        return -EINVAL;
    }

    if (!(huart->is_open))
    {
        return -EINVAL;
    }

    huart->tx_is_busy = true;
    huart->tx_is_async = false;
    uart_tx_polling(huart->base_address, buf, nbytes);
    huart->tx_is_busy = false;

    return 0;
}

int32_t uart_read_async(uart_handle_t const huart, uint8_t *const buf,
        uint32_t nbytes)
{
    if ((uart_is_handle_valid(huart) == 0) || (buf == NULL) || (nbytes == 0U))
    {
        return -EINVAL;
    }

    if (osal_mutex_lock(huart->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (huart->is_open == 0)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EINVAL;
        }

        if (huart->rx_is_busy == true)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EBUSY;
        }

        huart->rx_is_busy = true;
        huart->rx_is_async = true;
        if (osal_mutex_unlock(huart->mutex) == false)
        {
            return -EIO;
        }
    }

    huart->rx_buf = buf;
    huart->rx_size = nbytes;

    if (uart_read(huart, buf, nbytes) != 0)
    {
        return -EIO;
    }

    return 0;
}

int32_t uart_read_sync(uart_handle_t const huart, uint8_t *const buf,
        uint32_t nbytes)
{
    BaseType_t rx_sem_return;

    if ((uart_is_handle_valid(huart) == 0) || (buf == NULL) || (nbytes == 0U))
    {
        return -EINVAL;
    }
    if (osal_mutex_lock(huart->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (huart->is_open == 0)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EINVAL;
        }

        if (huart->rx_is_busy == true)
        {
            if (osal_mutex_unlock(huart->mutex) == false)
            {
                return -EIO;
            }
            return -EBUSY;
        }

        huart->rx_is_busy = true;
        huart->rx_is_async = false;
        if (osal_mutex_unlock(huart->mutex) == false)
        {
            return -EIO;
        }
    }
    huart->rx_buf = buf;
    huart->rx_size = nbytes;

    if (uart_read(huart, buf, nbytes) != 0)
    {
        return -EIO;
    }

    rx_sem_return =
            osal_semaphore_wait(huart->rd_sem, OSAL_TIMEOUT_WAIT_FOREVER);

    if (rx_sem_return == 0)
    {
        uart_disable_interrupt(huart->base_address, INTERRUPT_RX);
    }
    else
    {
        huart->rx_is_busy = false;
    }

    return 0;
}

int32_t uart_cancel(uart_handle_t const huart)
{
    if ((huart == NULL) || (uart_is_handle_valid(huart) == 0))
    {
        return -EINVAL;
    }
    return -ENOSYS;
}

int32_t uart_close(uart_handle_t const huart)
{

    if ((huart == NULL) || (huart->is_open == 0))
    {
        return -EINVAL;
    }

    if (osal_semaphore_delete(huart->rd_sem) == false)
    {
        return -EFAULT;
    }
    if (osal_semaphore_delete(huart->wr_sem) == false)
    {
        return -EFAULT;
    }

    uart_deinit(huart->instance);
    huart->is_open = 0;

    return 0;
}

/**
 * @brief Interrupt handler for UART
 */
void uart_isr(void *param)
{
    uart_handle_t huart;
    uint32_t id;
    uint16_t rx_byte_count;
    uint16_t tx_byte_count;

    huart = (uart_handle_t)param;
    if (uart_is_handle_valid(huart) == 0)
    {
        return;
    }

    id = get_int_status(huart->base_address);

    switch (id)
    {
        case UART_RXBUF_RDY_INT:
            if (huart->rx_bytes_left > 0U)
            {
                rx_byte_count = uart_read_fifo(huart->base_address,
                        huart->rx_buf,
                        huart->rx_bytes_left);

                huart->rx_bytes_left -= rx_byte_count;
                huart->rx_buf += rx_byte_count;
            }
            if (huart->rx_bytes_left == 0U)
            {
                uart_disable_interrupt(huart->base_address, INTERRUPT_RX);

                if (huart->rx_is_async == true)
                {

                    if (huart->callback_fn != NULL)
                    {
                        huart->callback_fn(UART_RD_DONE,
                                huart->cb_user_context);
                    }
                    huart->rx_is_async = false;
                    huart->rx_is_busy = false;
                }
                else
                {
                    (void)osal_semaphore_post(huart->rd_sem);
                }
            }

            break;

        case UART_TXBUF_EMPTY_INT:
            if (huart->tx_bytes_left > 0U)
            {
                tx_byte_count = uart_write_fifo(huart->base_address,
                        huart->tx_buf,
                        huart->tx_bytes_left);

                huart->tx_bytes_left = huart->tx_bytes_left - tx_byte_count;
                huart->tx_buf += tx_byte_count;
            }
            else
            {
                uart_disable_interrupt(huart->base_address, INTERRUPT_TX);
                if (huart->tx_is_async == true)
                {

                    if (huart->callback_fn != NULL)
                    {
                        huart->callback_fn(UART_WR_DONE,
                                huart->cb_user_context);
                    }
                    huart->tx_is_async = false;
                    huart->tx_is_busy = false;
                }
                else
                {
                    (void)osal_semaphore_post(huart->wr_sem);
                }
            }
            break;

        case UART_HW_ERR_INT:
            break;

        default:
            /* do nothing */
            break;
    }
}
