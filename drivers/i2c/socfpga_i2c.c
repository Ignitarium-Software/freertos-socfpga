/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for i2c
 */

#include "socfpga_interrupt.h"
#include "socfpga_i2c.h"
#include "socfpga_i2c_ll.h"
#include "socfpga_i2c_reg.h"
#include "socfpga_defines.h"
#include "socfpga_rst_mngr.h"
#include "osal_log.h"

struct i2c_descriptor
{
    uint32_t base_address;
    uint32_t instance;
    uint16_t slave_address;
    size_t bytes_left;
    size_t rd_cmds_left;
    size_t xfer_size;
    uint8_t *buffer;
    BaseType_t is_async;
    BaseType_t is_open;
    BaseType_t is_busy;
    BaseType_t no_stop_flag;
    BaseType_t is_xfer_abort;
    i2c_callback_t callback_fn;
    void *cb_usercontext;
    osal_mutex_def_t mutex_mem;
    osal_semaphore_def_t sem_mem;
    osal_mutex_t mutex;
    osal_semaphore_t sem;
};

static struct i2c_descriptor i2c_desc[MAX_I2C_INSTANCES];

/**
 * @brief handle the interrupt
 */
void i2c_isr(void *data);

/**
 * @brief Get the reset instance for the I2C peripheral
 */
static reset_periphrl_t i2c_get_rst_instance(uint32_t instance)
{
    reset_periphrl_t rst_instance = RST_PERIPHERAL_END;
    switch (instance)
    {
        case 0U:
            rst_instance = RST_I2C0;
            break;
        case 1U:
            rst_instance = RST_I2C1;
            break;
        case 2U:
            rst_instance = RST_I2C2;
            break;
        case 3U:
            rst_instance = RST_I2C3;
            break;
        case 4U:
            rst_instance = RST_I2C4;
            break;
        default:
            rst_instance = RST_PERIPHERAL_END;
            break;
    }
    return rst_instance;
}

/**
 * @brief Get the interrupt instance for the I2C peripheral
 */
static socfpga_hpu_interrupt_t i2c_get_interrupt_instance(uint32_t instance)
{
    socfpga_hpu_interrupt_t interrupt_instance = MAX_HPU_SPI_INTERRUPT;
    switch (instance)
    {
        case 0U:
            interrupt_instance = I2C0IRQ;
            break;
        case 1U:
            interrupt_instance = I2C1IRQ;
            break;
        case 2U:
            interrupt_instance = I2C2IRQ;
            break;
        case 3U:
            interrupt_instance = I2C3IRQ;
            break;
        case 4U:
            interrupt_instance = I2C4IRQ;
            break;
        default:
            interrupt_instance = MAX_HPU_SPI_INTERRUPT;
            break;
    }
    return interrupt_instance;
}

i2c_handle_t i2c_open(uint32_t instance)
{
    i2c_handle_t handle;
    socfpga_interrupt_err_t int_ret;
    socfpga_hpu_interrupt_t int_id;
    reset_periphrl_t rst_instance;
    uint8_t reset_status = 0U;
    int32_t status;

    if (instance >= MAX_I2C_INSTANCES)
    {
        ERROR("Invalid I2C instance.");
        return NULL;
    }

    handle = &i2c_desc[instance];
    if ((handle->is_open) == 1)
    {
        ERROR("I2C instance already open.");
        return NULL;
    }
    else
    {
        (void)memset(handle, 0, sizeof(struct i2c_descriptor));

        handle->base_address = GET_I2C_BASE_ADDRESS(instance);
        handle->instance = instance;
        rst_instance = i2c_get_rst_instance(instance);
        if (rst_instance == RST_PERIPHERAL_END)
        {
            ERROR("Invalid Reset Manager instance. ");
            return NULL;
        }
        status = rstmgr_get_reset_status(rst_instance, &reset_status);
        if (status != 0)
        {
            ERROR("I2C block get reset status failed. ");
            return NULL;
        }
        if (reset_status != 0U)
        {
            status = rstmgr_toggle_reset(rst_instance);
            if (status != 0)
            {
                ERROR("Failed to reset release I2C block. ");
                return NULL;
            }
        }
        i2c_init(handle->base_address);

        handle->is_open = true;
        handle->mutex = osal_mutex_create(&handle->mutex_mem);
        handle->sem = osal_semaphore_create(&handle->sem_mem);
        if (handle->sem == NULL)
        {
            ERROR("Failed to create semaphore for I2C instance. ");
            return NULL;
        }
        int_id = i2c_get_interrupt_instance(instance);
        if (int_id == MAX_HPU_SPI_INTERRUPT)
        {
            ERROR("Invalid interrupt instance. ");
            return NULL;
        }
        if (handle == NULL)
        {
            return NULL;
        }
        int_ret = interrupt_register_isr(int_id, i2c_isr, handle);
        if (int_ret != ERR_OK)
        {
            ERROR("Failed to register I2C interrupt handler. ");
            return NULL;
        }
        int_ret = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_I2C);
        if (int_ret != ERR_OK)
        {
            ERROR("Failed to enable I2C interrupt");
            return NULL;
        }

        return handle;
    }
}

int32_t i2c_close(i2c_handle_t const hi2c)
{

    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        ERROR("Invalid I2C handle or I2C instance not open.");
        return -EINVAL;
    }
    else
    {
        i2c_disable_interrupt(hi2c->base_address, I2C_TX_EMPTY_INT);
        i2c_disable_interrupt(hi2c->base_address, I2C_RX_FULL_INT);
        hi2c->is_open = false;
        return 0;
    }
}

void i2c_set_callback(i2c_handle_t const hi2c, i2c_callback_t callback, void *param)
{
    if (hi2c == NULL)
    {
        ERROR("Invalid I2C handle.");
        return;
    }

    hi2c->callback_fn = callback;
    hi2c->cb_usercontext = param;
}

int32_t i2c_ioctl(i2c_handle_t const hi2c, i2c_ioctl_t cmd, void *const pparam)
{
    i2c_config_t *config;
    int32_t ret = 0;
    uint16_t bytes_left;

    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        ERROR("Invalid I2C handle or I2C instance not open.");
        return -EINVAL;
    }

    switch (cmd)
    {
        case I2C_SEND_NO_STOP:
            hi2c->no_stop_flag = true;
            break;

        case I2C_SET_SLAVE_ADDR:
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            hi2c->slave_address = *((uint16_t *)pparam);
            i2c_set_target_addr(hi2c->base_address, hi2c->slave_address);
            break;

        case I2C_SET_MASTER_CFG:
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            config = (i2c_config_t *)pparam;
            if (i2c_config_master(hi2c->base_address, config->clk) == 0U)
            {
                ERROR("Failed to set I2C master configuration.");
                ret = -EINVAL;
            }
            break;

        case I2C_GET_MASTER_CFG:
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            config = (i2c_config_t *)pparam;
            config->clk = i2c_get_config(hi2c->base_address);
            break;

        case I2C_GET_BUS_STATE:
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            break;

        case I2C_GET_TX_NBYTES:
        /* fall through */
        case I2C_GET_RX_NBYTES:
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            bytes_left = (uint16_t)((hi2c->xfer_size - hi2c->bytes_left) &
                    0xFFFFU);
            *(uint16_t *)pparam = bytes_left;
            break;

        default:
            ERROR("Invalid IOCTL request");
            ret = -EINVAL;
            break;
    }
    return ret;
}

int32_t i2c_write_sync(i2c_handle_t const hi2c, uint8_t *const buf, size_t nbytes)
{
    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->slave_address == 0U)
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, 0xFFFFFFFFU))
    {
        if (hi2c->is_open == false)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is not open");
            return -EINVAL;
        }

        if (hi2c->is_busy == 1)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            return -EIO;
        }
    }

    hi2c->is_async = false;
    hi2c->buffer = buf;
    hi2c->xfer_size = nbytes;
    hi2c->bytes_left = nbytes;

    INFO("Starting I2C sync write");
    nbytes = i2c_write_fifo(hi2c->base_address, buf, hi2c->xfer_size, hi2c->no_stop_flag);

    hi2c->bytes_left -= nbytes;
    hi2c->buffer += nbytes;
    i2c_enable_interrupt(hi2c->base_address, I2C_TX_ABORT_INT |
            I2C_TX_EMPTY_INT);

    if (osal_semaphore_wait(hi2c->sem, 0xFFFFFFFFU) == false)
    {
        return -EIO;
    }

    if (hi2c->is_xfer_abort == 1)
    {
        ERROR("Transfer aborted");
        return -EIO;
    }
    hi2c->is_busy = false;
    hi2c->no_stop_flag = false;

    INFO("I2C write transfer completed");
    return 0;
}

int32_t i2c_write_async(i2c_handle_t const hi2c, uint8_t *const buf, size_t nbytes)
{
    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->slave_address == 0U)
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, 0xFFFFFFFFU))
    {
        if (hi2c->is_open == false)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is not open");
            return -EINVAL;
        }

        if (hi2c->is_busy == 1)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            return -EIO;
        }
    }
    hi2c->is_async = true;
    hi2c->buffer = buf;
    hi2c->xfer_size = nbytes;
    hi2c->bytes_left = nbytes;

    INFO("Starting I2C async write");
    nbytes = i2c_write_fifo(hi2c->base_address, buf, hi2c->xfer_size, hi2c->no_stop_flag);

    hi2c->bytes_left -= nbytes;
    hi2c->buffer += nbytes;

    i2c_enable_interrupt(hi2c->base_address, I2C_TX_ABORT_INT |
            I2C_TX_EMPTY_INT);

    return 0;
}

int32_t i2c_read_sync(i2c_handle_t const hi2c, uint8_t *const buf, size_t nbytes)
{
    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->slave_address == 0U)
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, 0xFFFFFFFFU))
    {
        if (hi2c->is_open == false)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is not open");
            return -EINVAL;
        }

        if (hi2c->is_busy == 1)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            return -EIO;
        }
    }

    hi2c->is_async = false;
    hi2c->buffer = buf;
    hi2c->xfer_size = nbytes;
    hi2c->bytes_left = nbytes;
    hi2c->rd_cmds_left = nbytes;

    INFO("Starting I2C sync read");

    /* Enqueue the read commands */
    hi2c->rd_cmds_left -= i2c_enq_read_cmd(
            hi2c->base_address, nbytes, hi2c->no_stop_flag);

    i2c_enable_interrupt(hi2c->base_address, I2C_TX_ABORT_INT |
            I2C_RX_FULL_INT);

    if (osal_semaphore_wait(hi2c->sem, 0xFFFFFFFFU) == false)
    {
        return -EIO;
    }
    if (hi2c->is_xfer_abort == 1)
    {
        ERROR("Transfer aborted");
        return -EIO;
    }
    hi2c->is_busy = false;
    hi2c->no_stop_flag = false;

    INFO("I2C read transfer completed");
    return 0;
}

int32_t i2c_read_async(i2c_handle_t const hi2c, uint8_t *const buf, size_t nbytes)
{
    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->slave_address == 0U)
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, 0xFFFFFFFFU))
    {
        if (hi2c->is_open == false)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is not open");
            return -EINVAL;
        }

        if (hi2c->is_busy == 1)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            return -EIO;
        }
    }

    hi2c->is_async = true;
    hi2c->buffer = buf;
    hi2c->xfer_size = nbytes;
    hi2c->bytes_left = nbytes;
    hi2c->rd_cmds_left = nbytes;

    INFO("Starting I2C async read");
    /* Enqueue the read commands */
    hi2c->rd_cmds_left -= i2c_enq_read_cmd(
            hi2c->base_address, nbytes, hi2c->no_stop_flag);

    i2c_enable_interrupt(hi2c->base_address, I2C_TX_ABORT_INT |
            I2C_RX_FULL_INT);

    return 0;
}

int32_t i2c_cancel(i2c_handle_t const hi2c)
{
    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }
    if (hi2c->is_busy == false)
    {
        ERROR("I2C instance is not busy");
        return -EPERM;
    }
    i2c_ll_cancel(hi2c->base_address);
    hi2c->is_xfer_abort = false;
    hi2c->is_busy = false;
    hi2c->no_stop_flag = false;
    return 0;
}

void i2c_isr(void *data)
{
    uint16_t nbytes;
    uint32_t base_addr;
    BaseType_t no_stop_flag;
    uint32_t status;

    i2c_handle_t pi2c_peripheral = (i2c_handle_t)data;
    if (pi2c_peripheral == NULL)
    {
        return;
    }

    base_addr = pi2c_peripheral->base_address;

    status = i2c_get_interrupt_status(base_addr);
    i2c_clear_interrupt(pi2c_peripheral->base_address);
    no_stop_flag = pi2c_peripheral->no_stop_flag;
    if ((status & I2C_TX_ABORT_INT) == I2C_TX_ABORT_INT)
    {
        i2c_disable_interrupt(base_addr, I2C_TX_ABORT_INT | I2C_TX_EMPTY_INT |
                I2C_RX_FULL_INT);

        pi2c_peripheral->is_xfer_abort = true;
        if ((pi2c_peripheral->is_async) == 1)
        {
            pi2c_peripheral->callback_fn(I2C_NACK, pi2c_peripheral->cb_usercontext);
        }
        else
        {
            (void)osal_semaphore_post(pi2c_peripheral->sem);
        }
    }
    else
    {
        if ((status & I2C_TX_EMPTY_INT) == 1U)
        {
            if (pi2c_peripheral->bytes_left > 0U)
            {
                nbytes = i2c_write_fifo(base_addr, pi2c_peripheral->buffer,
                        pi2c_peripheral->bytes_left, no_stop_flag);
                pi2c_peripheral->bytes_left -= nbytes;
                pi2c_peripheral->buffer += nbytes;
            }
            else
            {
                i2c_disable_interrupt(base_addr, I2C_TX_EMPTY_INT);
                if ((pi2c_peripheral->is_async) == 1)
                {
                    pi2c_peripheral->is_busy = false;
                    pi2c_peripheral->no_stop_flag = false;
                    pi2c_peripheral->callback_fn(I2C_SUCCESS, pi2c_peripheral->cb_usercontext);
                }
                else
                {
                    (void)osal_semaphore_post(pi2c_peripheral->sem);
                }
            }
        }
        if ((status & I2C_RX_FULL_INT) == I2C_RX_FULL_INT)
        {
            /*
             * Before reading the received byte(s), enqueue next set of
             * read commands if pending
             */
            if (pi2c_peripheral->rd_cmds_left > 0U)
            {
                pi2c_peripheral->rd_cmds_left -= i2c_enq_read_cmd(base_addr,
                        pi2c_peripheral->rd_cmds_left,
                        no_stop_flag);
            }

            /* read the received bytes */
            if (pi2c_peripheral->bytes_left > 0U)
            {
                nbytes = i2c_read_fifo(base_addr, pi2c_peripheral->buffer,
                        pi2c_peripheral->bytes_left);
                pi2c_peripheral->bytes_left -= nbytes;
                pi2c_peripheral->buffer += nbytes;
            }
            if (pi2c_peripheral->bytes_left == 0U)
            {
                i2c_disable_interrupt(base_addr, I2C_RX_FULL_INT);

                if (pi2c_peripheral->is_async == 1)
                {
                    pi2c_peripheral->is_busy = false;
                    pi2c_peripheral->no_stop_flag = false;
                    pi2c_peripheral->callback_fn(I2C_SUCCESS, pi2c_peripheral->cb_usercontext);
                }
                else
                {
                    (void)osal_semaphore_post(pi2c_peripheral->sem);
                }
            }
        }
    }
}
