/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for SPI
 */

#include <string.h>
#include "socfpga_defines.h"
#include "socfpga_spi.h"
#include "socfpga_spi_ll.h"
#include "socfpga_spi_reg.h"
#include "socfpga_interrupt.h"
#include "osal.h"
#include "osal_log.h"

#define MAX_INSTANCES    2U

struct spi_handle
{
    BaseType_t is_open;
    BaseType_t is_rx_busy;
    BaseType_t is_tx_busy;
    BaseType_t is_rx_async;
    BaseType_t is_tx_async;
    BaseType_t is_rx_on;
    BaseType_t is_tx_on;
    uint32_t instance;
    uint32_t base_address;
    uint32_t slave_id;
    uint16_t tx_size;
    uint16_t rx_size;
    uint16_t tx_bytes_left;
    uint16_t rx_bytes_left;
    uint8_t *tx_buf;
    uint8_t *rx_buf;
    spi_callback_t callback_fn;
    void *cb_user_context;
    osal_mutex_def_t mutex_mem;
    osal_semaphore_def_t sem_mem;
    osal_mutex_t mutex;
    osal_semaphore_t sem;
};

static struct spi_handle spi_descriptor[MAX_INSTANCES];

void spi_isr(void *param);

/**
 * @brief Check if the SPI handle is valid.
 */
static BaseType_t spi_is_handle_valid(spi_handle_t handle)
{
    if ((handle == &spi_descriptor[0]) || (handle == &spi_descriptor[1]))
    {
        return true;
    }
    return false;
}

spi_handle_t spi_open(uint32_t instance)
{
    spi_handle_t handle;
    socfpga_hpu_interrupt_t int_id;
    socfpga_interrupt_err_t int_ret;

    if (instance >= MAX_INSTANCES)
    {
        ERROR("Invalid SPI Instance");
        return NULL;
    }

    handle = &spi_descriptor[instance];
    if (handle->is_open != 0)    {
        ERROR("SPI instance already open");
        return NULL;
    }

    (void)memset(handle, 0, sizeof(struct spi_handle));
    handle->is_open = 1;
    handle->instance = instance;
    handle->base_address = GET_BASE_ADDRESS(instance);

    int_id = SPI_GET_INT_ID(instance);
    int_ret = interrupt_register_isr(int_id, spi_isr, handle);
    if (int_ret != ERR_OK)
    {
        ERROR("Failed to register SPI interrupt");
        return NULL;
    }
    int_ret = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_SPI);
    if (int_ret != ERR_OK)
    {
        ERROR("Failed to enable SPI interrupt");
        return NULL;
    }

    handle->mutex = osal_mutex_create(&handle->mutex_mem);
    handle->sem = osal_semaphore_create(&handle->sem_mem);

    if ((handle->mutex == NULL) || (handle->sem == NULL))
    {
        handle->is_open = 0;
        ERROR("Failed to create SPI synchronization primitives");
        return NULL;
    }

    spi_init(instance);

    spi_disable_interrupt(handle->base_address, SPI_ALL_INTERRUPTS);

    return handle;
}

int32_t spi_set_callback(spi_handle_t const hspi,
        spi_callback_t callback, void *pcntxt)
{
    if (hspi == NULL)
    {
        ERROR("Invalid SPI handle");
        return -EINVAL;
    }

    if (hspi->is_rx_busy || hspi->is_tx_busy)
    {
        ERROR("SPI bus is busy");
        return -EBUSY;
    }

    hspi->callback_fn = callback;
    hspi->cb_user_context = pcntxt;

    return 0;
}

int32_t spi_ioctl(spi_handle_t const hspi, spi_ioctl_t cmd, void *const buf)
{
    spi_cfg_t config =
    {
        0
    };
    int32_t result = 0;

    if (spi_is_handle_valid(hspi) == 0)
    {
        ERROR("SPI Handle is invalid");
        return -EINVAL;
    }

    switch (cmd)
    {
    case SPI_SET_CONFIG:
        if (hspi->is_tx_busy || hspi->is_rx_busy)
        {
            ERROR("SPI bus is busy");
            return -EBUSY;
        }
        if (buf == NULL)
        {
            ERROR("Buffer cannot be NULL");
            return -EINVAL;
        }

        config = *(spi_cfg_t *)buf;
        spi_disable(hspi->base_address);
        spi_set_config(hspi->base_address, config.clk, config.mode);
        spi_set_transfermode(hspi->base_address, SPI_TX_RX_MOD);
        spi_enable(hspi->base_address);
        break;

    case SPI_GET_CONFIG:
        if (buf == NULL)
        {
            ERROR("Buffer cannot be NULL");
            result = -EINVAL;
            break;
        }
        spi_get_config(hspi->base_address, &config.clk, &config.mode);
        *(spi_cfg_t *)buf = config;
        break;

    case SPI_GET_TX_NBYTES:
        if (buf == NULL)
        {
            ERROR("Buffer cannot be NULL");
            result = -EINVAL;
            break;
        }
        *(uint16_t *)buf = hspi->tx_size - hspi->tx_bytes_left;
        break;

    case SPI_GET_RX_NBYTES:
        if (buf == NULL)
        {
            ERROR("Buffer cannot be NULL");
            result = -EINVAL;
            break;
        }
        *(uint16_t *)buf = hspi->rx_size - hspi->rx_bytes_left;
        break;

    default:
        ERROR("Invalid IOCTL request");
        result = -EINVAL;
        break;
    }
    return result;
}

/**
 * @brief Start SPI transfer
 */
static void spi_transfer(spi_handle_t hspi, uint16_t nbytes)
{
    hspi->tx_bytes_left = nbytes;
    hspi->rx_bytes_left = nbytes;

    spi_enable_interrupt(hspi->base_address, SPI_RX_FULL_INT);

    spi_select_chip(hspi->instance, hspi->slave_id);

    spi_enable_interrupt(hspi->base_address, SPI_TX_EMPTY_INT);
}

int32_t spi_transfer_sync(spi_handle_t const hspi,
        uint8_t *const txbuf, uint8_t *const rxbuf, uint16_t nbytes)
{
    BaseType_t rx_sem_return;

    if ((spi_is_handle_valid(hspi) == 0) || (txbuf == NULL) || (nbytes == 0U))
    {
        ERROR("Invalid SPI handle or buffer");
        return -EINVAL;
    }

    if (osal_mutex_lock(hspi->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (hspi->is_open == 0)
        {
            if (osal_mutex_unlock(hspi->mutex) == false)
            {
                ERROR("Failed to unlock mutex");
                return -EIO;
            }
            ERROR("SPI instance not open");
            return -EINVAL;
        }
        if (hspi->is_tx_busy || hspi->is_rx_busy)
        {
            if (osal_mutex_unlock(hspi->mutex) == false)
            {
                ERROR("Failed to unlock mutex");
                return -EIO;
            }
            ERROR("SPI bus is busy");
            return -EBUSY;
        }
        hspi->is_tx_busy = true;
        hspi->is_tx_async = false;
        hspi->is_tx_on = true;
        hspi->is_rx_on = false;
        if (rxbuf != NULL)
        {
            hspi->is_rx_busy = true;
            hspi->is_rx_async = false;
            hspi->is_rx_on = true;
        }
        if (osal_mutex_unlock(hspi->mutex) == false)
        {
            ERROR("Failed to unlock mutex");
            return -EIO;
        }
    }

    hspi->tx_buf = txbuf;
    hspi->tx_size = nbytes;
    hspi->rx_size = 0;
    if (rxbuf != NULL)
    {
        hspi->rx_buf = rxbuf;
        hspi->rx_size = nbytes;
    }

    INFO("Starting SPI transfer in sync mode for %u bytes", nbytes);
    spi_transfer(hspi, nbytes);
    rx_sem_return = osal_semaphore_wait(hspi->sem, OSAL_TIMEOUT_WAIT_FOREVER);

    if (rx_sem_return != 0)
    {
        hspi->is_tx_busy = false;
        if (rxbuf != NULL)
        {
            hspi->is_rx_busy = false;
        }
    }

    INFO("Completed SPI transfer in sync mode. Transmitted %u bytes, Received %u bytes",
            nbytes - hspi->tx_bytes_left, nbytes - hspi->rx_bytes_left);
    return 0;
}

int32_t spi_transfer_async(spi_handle_t const hspi,
        uint8_t *const txbuf, uint8_t *const rxbuf, uint16_t nbytes)
{

    if ((spi_is_handle_valid(hspi) == 0) || (txbuf == NULL) || (rxbuf == NULL) ||
            (nbytes == 0U))
    {
        ERROR("Invalid SPI handle or buffer");
        return -EINVAL;
    }

    if (osal_mutex_lock(hspi->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (hspi->is_open == 0)
        {
            if (osal_mutex_unlock(hspi->mutex) == false)
            {
                ERROR("Failed to unlock mutex");
                return -EIO;
            }
            ERROR("SPI instance not open");
            return -EINVAL;
        }
        if (hspi->is_tx_busy || hspi->is_rx_busy)
        {
            if (osal_mutex_unlock(hspi->mutex) == false)
            {
                ERROR("Failed to unlock mutex");
                return -EIO;
            }
            ERROR("SPI bus is busy");
            return -EBUSY;
        }
        hspi->is_tx_busy = true;
        hspi->is_tx_async = true;
        hspi->is_tx_on = true;
        hspi->is_rx_on = false;
        if (rxbuf != NULL)
        {
            hspi->is_rx_busy = true;
            hspi->is_rx_async = true;
            hspi->is_rx_on = true;
        }
        if (osal_mutex_unlock(hspi->mutex) == false)
        {
            return -EIO;
        }
    }

    hspi->tx_buf = txbuf;
    hspi->tx_size = nbytes;
    hspi->rx_size = 0;
    if (rxbuf != NULL)
    {
        hspi->rx_buf = rxbuf;
        hspi->rx_size = nbytes;
    }

    INFO("Starting SPI transfer in async mode for %u bytes", nbytes);
    spi_set_transfermode(hspi->base_address, SPI_TX_RX_MOD);
    spi_transfer(hspi, nbytes);

    return 0;
}

int32_t spi_select_slave(spi_handle_t const hspi, uint32_t ss)
{
    if (spi_is_handle_valid(hspi) == 0)
    {
        ERROR("Invalid SPI handle");
        return -EINVAL;
    }

    if ((ss < 1U) || (ss > 4U))
    {
        ERROR("Invalid instance or slave ID");
        return -EINVAL;
    }

    hspi->slave_id = ss;

    return 0;
}

int32_t spi_cancel(spi_handle_t const hspi)
{
    (void)hspi;
    ERROR("Function not supported");
    return -ENOSYS;
}

int32_t spi_close(spi_handle_t const hspi)
{
    if (spi_is_handle_valid(hspi) == 0)
    {
        ERROR("Invalid SPI handle");
        return -EINVAL;
    }
    if (hspi->is_open == 0)
    {
        ERROR("SPI instance not open");
        return -EINVAL;
    }

    hspi->is_open = false;
    spi_deinit(hspi->instance);

    return 0;
}

/**
 * @brief Interrupt handler for SPI
 */
void spi_isr(void *param)
{
    spi_handle_t hspi;
    uint8_t id;
    uint16_t rx_byte_count;
    uint16_t tx_byte_count;

    hspi = (spi_handle_t)param;
    if (hspi == NULL)
    {
        return;
    }
    id = spi_get_interrupt_status(hspi->base_address);

    switch (id)
    {
    case SPI_RX_FULL_INT:
        if (hspi->rx_bytes_left > 0U)
        {
            if (hspi->is_rx_on == true)
            {
                rx_byte_count = spi_read_fifo(hspi->base_address,
                        hspi->rx_buf, hspi->rx_bytes_left);
                hspi->rx_bytes_left -= rx_byte_count;
                hspi->rx_buf += rx_byte_count;
            }
            else
            {
                rx_byte_count = spi_read_fifo(hspi->base_address, NULL,
                        hspi->rx_bytes_left);
                hspi->rx_bytes_left -= rx_byte_count;
            }
        }
        if (hspi->rx_bytes_left == 0U)
        {
            spi_disable_interrupt(hspi->base_address, SPI_RX_FULL_INT);
            spi_select_chip(hspi->instance, 0U);

            if ((hspi->is_rx_async == true) || (hspi->is_tx_async == true))
            {
                hspi->is_tx_async = false;
                hspi->is_rx_async = false;
                hspi->is_tx_busy = false;
                hspi->is_rx_busy = false;

                if (hspi->callback_fn != NULL)
                {
                    hspi->callback_fn(SPI_SUCCESS, hspi->cb_user_context);
                }
            }
            else
            {
                (void)osal_semaphore_post(hspi->sem);
            }
        }
        break;

    case SPI_TX_EMPTY_INT:
        if (hspi->tx_bytes_left > 0U)
        {
            if (hspi->is_tx_on == true)
            {
                tx_byte_count = spi_write_fifo(hspi->base_address,
                        hspi->tx_buf, hspi->tx_bytes_left);
                hspi->tx_bytes_left -= tx_byte_count;
                hspi->tx_buf += tx_byte_count;
            }
            else
            {
                tx_byte_count = spi_write_fifo(hspi->base_address, NULL,
                        hspi->tx_bytes_left);
                hspi->tx_bytes_left -= tx_byte_count;
            }
        }
        else
        {
            spi_disable_interrupt(hspi->base_address, SPI_TX_EMPTY_INT);
        }

        break;

    default:
        /* do nothing */
        break;
    }
}
