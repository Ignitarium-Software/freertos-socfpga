/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
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
#include "socfpga_dma.h"
#include "socfpga_cache.h"
#include "osal_log.h"
#include "osal.h"

#define NUM_BLEN_OPTS   3U

struct i2c_descriptor
{
    uint32_t base_addr;
    uint32_t instance;
    i2c_role_t role;
    bool slave_is_10bit_addr;
    bool slave_stop_det_ifaddressed;
    bool slave_ack_general_call;
    uint8_t slave_rx_tl;
    uint8_t slave_tx_tl;
    uint8_t slave_tx_default_byte;
    uint16_t slave_own_address;
    i2c_slave_write_requested_cb_t  slave_write_requested_cb;
    i2c_slave_write_received_cb_t   slave_write_received_cb;
    i2c_slave_read_requested_cb_t   slave_read_requested_cb;
    i2c_slave_read_processed_cb_t   slave_read_processed_cb;
    i2c_slave_stop_cb_t             slave_stop_cb;
    void *slave_cb_usercontext;
    bool slave_write_started;
    uint32_t slave_stop_cnt;
    uint32_t slave_rx_over_cnt;
    uint32_t slave_tx_underflow_cnt;
    uint32_t slave_tx_abrt_cnt;
    uint16_t slave_address;
    size_t bytes_left;
    size_t rd_cmds_left;
    size_t xfer_size;
    uint8_t *buffer;
    i2c_operation_t operation;
    dma_handle_t wdma_handle;
    dma_handle_t rdma_handle;
    dma_config_t wdma_config;
    dma_config_t rdma_config;
    dma_xfer_cfg_t *wdma_xfer_cfg;
    dma_xfer_cfg_t *rdma_xfer_cfg;
    uint32_t *wdma_buf;
    bool is_dma_open;
    bool is_wdma_en;
    bool is_rdma_en;
    bool is_async;
    bool is_open;
    bool is_busy;
    bool no_stop_flag;
    bool is_xfer_abort;
    i2c_callback_t callback_fn;
    void *cb_usercontext;
    osal_mutex_def_t mutex_mem;
    osal_semaphore_def_t sem_mem;
    osal_mutex_t mutex;
    osal_semaphore_t sem;
};

static struct i2c_descriptor i2c_desc[MAX_I2C_INSTANCES] = {0};
static dma_xfer_cfg_t wdma_blk_xfer_list[MAX_I2C_INSTANCES][MAX_I2C_BLK_XFERS] = {0};
static dma_xfer_cfg_t rdma_blk_xfer_list[MAX_I2C_INSTANCES][MAX_I2C_BLK_XFERS] = {0};
static uint32_t i2c_transfer_buf[MAX_I2C_INSTANCES]
        [MAX_I2C_BLK_XFERS * I2C_DMA_XFER_BLK_SIZE] = {0};

static void i2c_delete_osal_primitives(i2c_handle_t hi2c)
{
    if (hi2c == NULL)
    {
        return;
    }

    if (hi2c->mutex != NULL)
    {
        osal_mutex_delete(hi2c->mutex);
        hi2c->mutex = NULL;
    }
    if (hi2c->sem != NULL)
    {
        osal_semaphore_delete(hi2c->sem);
        hi2c->sem = NULL;
    }
}

/**
 * @brief handle the interrupt
 */
void i2c_isr(void *data);
static void i2c_wdma_callback(void *data);
static void i2c_rdma_callback(void *data);

static void i2c_drain_rx_fifo(uint32_t base_addr)
{
    uint8_t discard;

    while (i2c_read_fifo(base_addr, &discard, 1U) == 1U)
    {
    }
}

static int32_t i2c_slave_init(i2c_handle_t const hi2c, const i2c_slave_config_t *cfg);
static int32_t i2c_slave_deinit(i2c_handle_t const hi2c);

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

static void i2c_get_dma_interface_id(uint32_t instance, uint32_t *i2c_tx_peri_id,
        uint32_t *i2c_rx_peri_id)
{
    if (instance >= MAX_I2C_INSTANCES)
    {
        ERROR("Invalid I2C instance");
        return;
    }

    switch (instance)
    {
        case 0U:
            *i2c_tx_peri_id = DMA_I2C0_TX;
            *i2c_rx_peri_id = DMA_I2C0_RX;
            break;
        case 1U:
            *i2c_tx_peri_id = DMA_I2C1_TX;
            *i2c_rx_peri_id = DMA_I2C1_RX;
            break;
        case 2U:
            *i2c_tx_peri_id = DMA_I2C_EMAC0_TX;
            *i2c_rx_peri_id = DMA_I2C_EMAC0_RX;
            break;
        case 3U:
            *i2c_tx_peri_id = DMA_I2C_EMAC1_TX;
            *i2c_rx_peri_id = DMA_I2C_EMAC1_RX;
            break;
        case 4U:
            *i2c_tx_peri_id = DMA_I2C_EMAC2_TX;
            *i2c_rx_peri_id = DMA_I2C_EMAC2_RX;
            break;
    }

}

static int32_t i2c_dma_config(i2c_handle_t hi2c, i2c_dma_config_t *pconfig)
{
    int32_t ret = 0;
    bool wdma_opened = false;
    bool rdma_opened = false;

    if (pconfig == NULL)
    {
        ERROR("Invalid I2C Handle and/or I2C DMA Config");
        return -EINVAL;
    }

    hi2c->wdma_handle = dma_open(pconfig->tx_instance, pconfig->tx_channel);
    if (hi2c->wdma_handle == NULL)
    {
        ERROR("Failed to open I2C Tx DMA channel");
        ret = -EIO;
    }
    else
    {
        wdma_opened = true;
    }

    if (ret == 0)
    {
        hi2c->rdma_handle = dma_open(pconfig->rx_instance, pconfig->rx_channel);
        if (hi2c->rdma_handle == NULL)
        {
            ERROR("Failed to open I2C Rx DMA channel");
            ret = -EIO;
        }
        else
        {
            rdma_opened = true;
        }
    }

    hi2c->wdma_config.instance = pconfig->tx_instance;
    hi2c->wdma_config.ch_dir = DMA_MEM_TO_PERI_DMAC;
    hi2c->wdma_config.ch_prio = pconfig->tx_prio;
    hi2c->wdma_config.callback = i2c_wdma_callback;
    hi2c->wdma_config.usr_cntxt = (void *)hi2c;

    hi2c->rdma_config.instance = pconfig->rx_instance;
    hi2c->rdma_config.ch_dir = DMA_PERI_TO_MEM_DMAC;
    hi2c->rdma_config.ch_prio = pconfig->rx_prio;
    hi2c->rdma_config.callback = i2c_rdma_callback;
    hi2c->rdma_config.usr_cntxt = (void *)hi2c;

    hi2c->wdma_xfer_cfg = wdma_blk_xfer_list[hi2c->instance];
    hi2c->rdma_xfer_cfg = rdma_blk_xfer_list[hi2c->instance];
    hi2c->wdma_buf = i2c_transfer_buf[hi2c->instance];

    (void)i2c_get_dma_interface_id(hi2c->instance,
            &hi2c->wdma_config.peri_id, &hi2c->rdma_config.peri_id);

    if ((ret == 0) && (dma_config(hi2c->wdma_handle, &(hi2c->wdma_config)) != 0))
    {
        ERROR("I2C Tx DMA channel configuration failed");
        ret = -EIO;
    }

    if ((ret == 0) && (dma_config(hi2c->rdma_handle, &(hi2c->rdma_config)) != 0))
    {
        ERROR("I2C Rx DMA channel configuration failed");
        ret = -EIO;
    }

    if (ret != 0)
    {
        if (rdma_opened)
        {
            dma_close(hi2c->rdma_handle);
        }
        if (wdma_opened)
        {
            dma_close(hi2c->wdma_handle);
        }
    }
    else
    {
        hi2c->is_dma_open = true;
    }

    return ret;
}

static int32_t i2c_dma_close(i2c_handle_t hi2c)
{
    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        ERROR("Invalid I2C instance and/or I2C instance not open");
        return -EINVAL;
    }

    if (hi2c->is_dma_open == false)
    {
        ERROR("I2C DMA channels are not configured");
        return -EINVAL;
    }

    dma_close(hi2c->wdma_handle);
    dma_close(hi2c->rdma_handle);

    hi2c->is_wdma_en = false;
    hi2c->is_rdma_en = false;
    hi2c->is_dma_open = false;

    return 0;
}

static void i2c_find_burst_params(uint32_t nbytes,
        dma_burst_len_t *burst_len, uint32_t *burst)
{
    int i = 0;
    uint8_t dma_burst_lens[NUM_BLEN_OPTS] = {1, 4, 8};
    dma_burst_len_t dma_burst_len_enums[NUM_BLEN_OPTS] =
            {DMA_BURST_LEN_1, DMA_BURST_LEN_4, DMA_BURST_LEN_8};

    *burst = 1U;
    *burst_len = dma_burst_len_enums[0];

    for (i = (NUM_BLEN_OPTS - 1U); i >= 0; i--)
    {
        if (dma_burst_lens[i] <= nbytes)
        {
            *burst = dma_burst_lens[i];
            *burst_len = dma_burst_len_enums[i];
            break;
        }
    }
}

static void i2c_slave_handle_tx_abort(i2c_handle_t pi2c_peripheral,
        uint32_t base_addr, uint32_t raw)
{
    if ((raw & I2C_RAW_INTR_STAT_TX_ABRT_MASK) == 0U)
    {
        return;
    }

    pi2c_peripheral->slave_tx_abrt_cnt++;
    i2c_ll_clear_tx_abrt(base_addr);

    if ((pi2c_peripheral->is_busy == true) &&
            (pi2c_peripheral->operation == I2C_WRITE))
    {
        if (pi2c_peripheral->is_wdma_en)
        {
            i2c_ll_tdma_disable(base_addr);
        }
        i2c_disable_interrupt(base_addr, I2C_RD_REQ_INT);
        pi2c_peripheral->is_busy = false;

        if (pi2c_peripheral->callback_fn != NULL)
        {
            pi2c_peripheral->callback_fn(I2C_OP_FAIL,
                    pi2c_peripheral->cb_usercontext);
        }
    }
}

static void i2c_slave_handle_stop_detect(i2c_handle_t pi2c_peripheral,
        uint32_t base_addr, uint32_t raw)
{
    bool irq_async_done;

    if ((raw & I2C_RAW_INTR_STAT_STOP_DET_MASK) == 0U)
    {
        return;
    }

    pi2c_peripheral->slave_stop_cnt++;
    i2c_ll_clear_stop_det(base_addr);

    pi2c_peripheral->slave_write_started = false;

    if (!((pi2c_peripheral->is_busy == true) &&
          (pi2c_peripheral->operation == I2C_READ) &&
          (pi2c_peripheral->is_rdma_en == true)))
    {
        i2c_drain_rx_fifo(base_addr);
        i2c_ll_clear_rx_over(base_addr);
    }

    if (pi2c_peripheral->slave_stop_cb != NULL)
    {
        pi2c_peripheral->slave_stop_cb(pi2c_peripheral,
                pi2c_peripheral->slave_cb_usercontext);
    }

    irq_async_done = (pi2c_peripheral->is_busy == true) &&
                          (pi2c_peripheral->is_async == true) &&
                          (pi2c_peripheral->is_rdma_en == false) &&
                          (pi2c_peripheral->is_wdma_en == false);
    if (irq_async_done)
    {
        pi2c_peripheral->is_busy = false;

        /*
         * Keep RD_REQ_INT and STOP_DET_INT enabled so that the byte-
         * callback model (read_requested_cb, stop_cb) continues to
         * work for subsequent master transactions after an IRQ-async
         * RX or TX completes.  Disabling them here would prevent the
         * slave from responding to master reads via the callback path.
         */
        if (pi2c_peripheral->callback_fn != NULL)
        {
            pi2c_peripheral->callback_fn(I2C_SUCCESS,
                    pi2c_peripheral->cb_usercontext);
        }
    }
}

static void i2c_slave_handle_read_request(i2c_handle_t pi2c_peripheral,
        uint32_t base_addr)
{
    uint8_t byte;
    bool slave_activity;
    bool dma_pre_armed;
    bool irq_tx_armed;
    size_t pos;
    uint16_t nwr;
    uint32_t raw;
    uint8_t default_byte;

    raw = i2c_ll_get_raw_interrupt_status(base_addr);
    if ((raw & I2C_RAW_INTR_STAT_RD_REQ_MASK) == 0U)
    {
        return;
    }

    /*
     * Guard RD_REQ handling with bus activity check
     */
    slave_activity =
            (i2c_read_status(base_addr) & I2C_STATUS_IC_STATUS_ACTIVITY_MASK) != 0U;

    /* Check if slave TX DMA was pre-armed by i2c_write_async(). */
    dma_pre_armed = (pi2c_peripheral->is_busy == true) &&
                         (pi2c_peripheral->is_wdma_en == true) &&
                         (pi2c_peripheral->operation == I2C_WRITE);
    irq_tx_armed = (pi2c_peripheral->is_busy == true) &&
                        (pi2c_peripheral->is_async == true) &&
                        (pi2c_peripheral->is_wdma_en == false) &&
                        (pi2c_peripheral->operation == I2C_WRITE);

    if (dma_pre_armed)
    {
        /*
         * Master has addressed us for reading. TX FIFO is empty here
         * (DMA was not started in i2c_write_async to avoid
         * ABRT_SLVFLUSH_TXFIFO). Clear RD_REQ before starting DMA
         * so the controller can accept new read requests.
         */
        i2c_disable_interrupt(base_addr, I2C_RD_REQ_INT);
        i2c_ll_clear_rd_req(base_addr);
        if (dma_start_transfer(pi2c_peripheral->wdma_handle) != 0)
        {
            pi2c_peripheral->is_busy = false;
            i2c_enable_interrupt(base_addr, I2C_RD_REQ_INT);
            if (pi2c_peripheral->callback_fn != NULL)
            {
                pi2c_peripheral->callback_fn(I2C_OP_FAIL,
                        pi2c_peripheral->cb_usercontext);
            }
            return;
        }
        i2c_ll_tdma_enable(base_addr);
    }
    else if (irq_tx_armed && slave_activity)
    {
        /*
         * Clear RD_REQ before writing to TX FIFO (DW databook
         * requires clearing IC_CLR_RD_REQ before responding).
         */
        i2c_ll_clear_rd_req(base_addr);

        /* Feed TX FIFO from the pre-armed buffer. */
        while (pi2c_peripheral->bytes_left > 0U)
        {
            pos = pi2c_peripheral->xfer_size - pi2c_peripheral->bytes_left;
            nwr = i2c_write_fifo(base_addr, &pi2c_peripheral->buffer[pos],
                    (uint32_t)pi2c_peripheral->bytes_left, true);
            if (nwr == 0U)
            {
                break;
            }
            pi2c_peripheral->bytes_left -= nwr;
        }

        if (pi2c_peripheral->bytes_left == 0U)
        {
            default_byte = pi2c_peripheral->slave_tx_default_byte;
            if (i2c_write_fifo(base_addr, &default_byte, 1U, true) == 1U)
            {
                pi2c_peripheral->slave_tx_underflow_cnt++;
            }
        }
    }
    else if (slave_activity)
    {
        /*
         * Byte-oriented callback mode: one byte per RD_REQ.
         * Clear RD_REQ before writing the byte
         */
        byte = pi2c_peripheral->slave_tx_default_byte;

        i2c_ll_clear_rd_req(base_addr);

        if (pi2c_peripheral->slave_read_requested_cb != NULL)
        {
            pi2c_peripheral->slave_read_requested_cb(pi2c_peripheral,
                    &byte, pi2c_peripheral->slave_cb_usercontext);
        }
        (void)i2c_write_fifo(base_addr, &byte, 1U, true);

        if (pi2c_peripheral->slave_read_processed_cb != NULL)
        {
            pi2c_peripheral->slave_read_processed_cb(pi2c_peripheral,
                    &byte, pi2c_peripheral->slave_cb_usercontext);
        }
    }
    else
    {
        /* Bus no longer active; discard stale RD_REQ. */
        i2c_ll_clear_rd_req(base_addr);
    }
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
    if ((handle->is_open) == true)
    {
        ERROR("I2C instance already open.");
        return NULL;
    }
    else
    {
        (void)memset(handle, 0, sizeof(struct i2c_descriptor));

        handle->base_addr = GET_I2C_BASE_ADDR(instance);
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
        i2c_init(handle->base_addr);
        handle->is_dma_open = false;
        handle->is_wdma_en = false;
        handle->is_rdma_en = false;
        handle->mutex = osal_mutex_create(&handle->mutex_mem);
        if (handle->mutex == NULL)
        {
            ERROR("Failed to create mutex for I2C instance");
            return NULL;
        }
        handle->sem = osal_semaphore_create(&handle->sem_mem);
        if ((handle->sem == NULL) || (handle->mutex == NULL))
        {
            ERROR("Failed to create semaphore for I2C instance. ");
            i2c_delete_osal_primitives(handle);
            return NULL;
        }
        int_id = i2c_get_interrupt_instance(instance);
        if (int_id == MAX_HPU_SPI_INTERRUPT)
        {
            ERROR("Invalid interrupt instance. ");
            i2c_delete_osal_primitives(handle);
            return NULL;
        }
        int_ret = interrupt_register_isr(int_id, i2c_isr, handle);
        if (int_ret != ERR_OK)
        {
            ERROR("Failed to register I2C interrupt handler. ");
            i2c_delete_osal_primitives(handle);
            return NULL;
        }
        int_ret = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_I2C);
        if (int_ret != ERR_OK)
        {
            ERROR("Failed to enable I2C interrupt");
            i2c_delete_osal_primitives(handle);
            return NULL;
        }
        handle->is_open = true;
        return handle;
    }
}


int32_t i2c_close(i2c_handle_t const hi2c)
{
    int32_t retval;

    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        ERROR("Invalid I2C handle or I2C instance not open.");
        return -EINVAL;
    }
    else
    {
        if (hi2c->is_dma_open == true)
        {
            retval = i2c_dma_close(hi2c);
            if (retval != 0)
            {
                ERROR("Failed to close I2C DMA channels");
                return -EIO;
            }
        }

        i2c_disable_interrupt(hi2c->base_addr, I2C_TX_EMPTY_INT);
        i2c_disable_interrupt(hi2c->base_addr, I2C_RX_FULL_INT);
        i2c_disable_interrupt(hi2c->base_addr, I2C_RD_REQ_INT);
        i2c_disable_interrupt(hi2c->base_addr, I2C_RX_OVER_INT);
        i2c_disable_interrupt(hi2c->base_addr, I2C_STOP_DET_INT);

        hi2c->callback_fn = NULL;
        hi2c->cb_usercontext = NULL;
        hi2c->slave_write_requested_cb = NULL;
        hi2c->slave_write_received_cb = NULL;
        hi2c->slave_read_requested_cb = NULL;
        hi2c->slave_read_processed_cb = NULL;
        hi2c->slave_stop_cb = NULL;
        hi2c->slave_cb_usercontext = NULL;

        i2c_delete_osal_primitives(hi2c);
        hi2c->is_open = false;
    }

    return 0;
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

/*
 * Configure the controller for slave role and enable the interrupt-driven
 * slave state machine.
 *
 * This:
 * - requires the bus to be idle (no ongoing activity)
 * - disables any I2C DMA enables (TDMAE/RDMAE) and clears per-transfer state
 * - programs slave address/10-bit/general-call/STOP_DET behavior and FIFO TLs
 * - unmasks the set of interrupts needed for slave operation
 */
static int32_t i2c_slave_init(i2c_handle_t const hi2c, const i2c_slave_config_t *cfg)
{
    uint32_t mask;

    if ((hi2c == NULL) || (cfg == NULL) || (hi2c->is_open == false))
    {
        return -EINVAL;
    }

    /* Recommended: enable slave only when bus is idle */
    if ((i2c_read_status(hi2c->base_addr) & I2C_STATUS_IC_STATUS_ACTIVITY_MASK) != 0U)
    {
        return -EBUSY;
    }

    if (osal_mutex_lock(hi2c->mutex, 0xFFFFFFFFU))
    {
        if (hi2c->is_busy == true)
        {
            (void)osal_mutex_unlock(hi2c->mutex);
            return -EBUSY;
        }
        (void)osal_mutex_unlock(hi2c->mutex);
    }

    hi2c->role = I2C_ROLE_SLAVE;

    /* Store slave configuration */
    hi2c->slave_own_address = cfg->slave_address;
    hi2c->slave_is_10bit_addr = cfg->is_10bit_addr;
    hi2c->slave_stop_det_ifaddressed = cfg->stop_det_ifaddressed;
    hi2c->slave_ack_general_call = cfg->ack_general_call;
    hi2c->slave_rx_tl = cfg->rx_tl;
    hi2c->slave_tx_tl = cfg->tx_tl;
    hi2c->slave_tx_default_byte = cfg->tx_default_byte;
    hi2c->slave_write_requested_cb = cfg->write_requested_cb;
    hi2c->slave_write_received_cb = cfg->write_received_cb;
    hi2c->slave_read_requested_cb = cfg->read_requested_cb;
    hi2c->slave_read_processed_cb = cfg->read_processed_cb;
    hi2c->slave_stop_cb = cfg->stop_cb;
    hi2c->slave_cb_usercontext = cfg->cb_usercontext;
    hi2c->slave_write_started = false;

    /* Reset stats */
    hi2c->slave_stop_cnt = 0U;
    hi2c->slave_rx_over_cnt = 0U;
    hi2c->slave_tx_underflow_cnt = 0U;
    hi2c->slave_tx_abrt_cnt = 0U;

    i2c_ll_tdma_disable(hi2c->base_addr);
    i2c_ll_rdma_disable(hi2c->base_addr);
    hi2c->is_wdma_en = false;
    hi2c->is_rdma_en = false;
    hi2c->is_busy = false;

    /* Mask all interrupts while reconfiguring */
    i2c_ll_set_interrupt_mask(hi2c->base_addr, 0U);

    i2c_ll_config_slave(hi2c->base_addr,
            hi2c->slave_own_address,
            hi2c->slave_is_10bit_addr,
            hi2c->slave_stop_det_ifaddressed,
            hi2c->slave_ack_general_call,
            hi2c->slave_rx_tl,
            hi2c->slave_tx_tl);

    /* Unmask required slave interrupts */
    mask = (I2C_INTR_MASK_M_RX_FULL_MASK |
            I2C_INTR_MASK_M_RD_REQ_MASK |
            I2C_INTR_MASK_M_TX_ABRT_MASK |
            I2C_INTR_MASK_M_STOP_DET_MASK |
            I2C_INTR_MASK_M_RX_OVER_MASK);
    i2c_ll_set_interrupt_mask(hi2c->base_addr, mask);

    return 0;
}

/*
 * Disable slave role and restore a master-default controller configuration.
 *
 * This masks all I2C interrupts, disables the controller, clears any TX abort
 * state, then calls i2c_init() so the instance is ready to be reconfigured as
 * a master (speed/target address set by subsequent IOCTLs).
 */
static int32_t i2c_slave_deinit(i2c_handle_t const hi2c)
{
    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        return -EINVAL;
    }

    if (hi2c->role != I2C_ROLE_SLAVE)
    {
        return 0;
    }

    /* Mask interrupts and disable controller */
    i2c_ll_set_interrupt_mask(hi2c->base_addr, 0U);
    i2c_ll_disable_controller(hi2c->base_addr);

    /* Release TX FIFO from flushed state */
    i2c_ll_clear_tx_abrt(hi2c->base_addr);

    /* Restore master defaults (caller may reconfigure speed/target) */
    i2c_init(hi2c->base_addr);
    hi2c->role = I2C_ROLE_MASTER;
    return 0;
}

int32_t i2c_ioctl(i2c_handle_t const hi2c, i2c_ioctl_t cmd, void *const pparam)
{
    i2c_config_t *config;
    i2c_dma_config_t *pconfig;
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
            if (hi2c->role != I2C_ROLE_MASTER)
            {
                return -EPERM;
            }
            hi2c->no_stop_flag = true;
            break;

        case I2C_SET_SLAVE_ADDR:
            if (hi2c->role != I2C_ROLE_MASTER)
            {
                return -EPERM;
            }
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            hi2c->slave_address = *((uint16_t *)pparam);
            i2c_set_target_addr(hi2c->base_addr, hi2c->slave_address);
            break;

        case I2C_SET_MASTER_CFG:
            if (hi2c->role != I2C_ROLE_MASTER)
            {
                return -EPERM;
            }
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            config = (i2c_config_t *)pparam;
            if (i2c_config_master(hi2c->base_addr, config->clk) == 0U)
            {
                ERROR("Failed to set I2C master configuration.");
                ret = -EIO;
            }
            break;

        case I2C_GET_MASTER_CFG:
            if (hi2c->role != I2C_ROLE_MASTER)
            {
                return -EPERM;
            }
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            config = (i2c_config_t *)pparam;
            config->clk = i2c_get_config(hi2c->base_addr);
            break;

        case I2C_GET_BUS_STATE:
            if ((pparam == NULL))
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }

            ret = i2c_read_status(hi2c->base_addr);
            if (ret & I2C_STATUS_IC_STATUS_ACTIVITY_MASK)
            {
                *(i2c_bus_status_t *)pparam = I2C_BUS_BUSY;
            }
            else
            {
                *(i2c_bus_status_t *)pparam = I2C_BUS_IDLE;
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

        case I2C_OPEN_DMA:
            if (pparam == NULL)
            {
                ERROR("Invalid config param");
                return -EINVAL;
            }
            if (hi2c->is_dma_open == true)
            {
                ERROR("DMA instance is already open");
                return -EIO;
            }

            pconfig = (i2c_dma_config_t *)pparam;
            if (i2c_dma_config(hi2c, pconfig) != 0U)
            {
                ERROR("DMA initialization failed");
                return -EIO;
            }
            break;

        case I2C_ENABLE_WDMA:
            if (hi2c->is_dma_open == false)
            {
                ERROR("DMA Channels not configured");
                return -EINVAL;
            }
            hi2c->is_wdma_en = true;
            break;

        case I2C_ENABLE_RDMA:
            if (hi2c->is_dma_open == false)
            {
                ERROR("DMA Channels not configured");
                return -EINVAL;
            }
            hi2c->is_rdma_en = true;
            break;

        case I2C_DISABLE_WDMA:
            hi2c->is_wdma_en = false;
            break;

        case I2C_DISABLE_RDMA:
            hi2c->is_rdma_en = false;
            break;

        case I2C_CLOSE_DMA:
            if (i2c_dma_close(hi2c) != 0U)
            {
                ERROR("DMA de-initialization failed");
                return -EINVAL;
            }
            break;

        case I2C_SLAVE_INIT:
            if (pparam == NULL)
            {
                ERROR("Buffer cannot be null");
                return -EINVAL;
            }
            ret = i2c_slave_init(hi2c, (const i2c_slave_config_t *)pparam);
            break;

        case I2C_SLAVE_DEINIT:
            ret = i2c_slave_deinit(hi2c);
            break;

        default:
            ERROR("Invalid IOCTL request");
            ret = -EINVAL;
            break;
    }
    return ret;
}

static int32_t i2c_start_xfer_int_mode(i2c_handle_t hi2c, i2c_operation_t op,
        uint8_t *buf, size_t nbytes)
{
    uint16_t nwr;
    uint16_t ncmd;

    if (op == I2C_WRITE)
    {
        hi2c->buffer = buf;
        hi2c->bytes_left = nbytes;

        nwr = i2c_write_fifo(hi2c->base_addr, buf, (uint32_t)nbytes,
                hi2c->no_stop_flag);
        hi2c->bytes_left -= nwr;
        hi2c->buffer += nwr;

        i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT |
                I2C_TX_EMPTY_INT);
    }
    else
    {
        hi2c->buffer = buf;
        hi2c->bytes_left = nbytes;
        hi2c->rd_cmds_left = nbytes;

        ncmd = i2c_enq_read_cmd(hi2c->base_addr, (uint32_t)nbytes,
                hi2c->no_stop_flag);
        hi2c->rd_cmds_left -= (size_t)ncmd;

        i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT |
                I2C_RX_FULL_INT | I2C_TX_EMPTY_INT);
    }
    return 0;
}

static int32_t i2c_dma_setup_wdma(i2c_handle_t hi2c, uint32_t *psrc_words,
        size_t nwords, uint32_t blk_cnt, dma_burst_len_t burst_len)
{
    int32_t ret;
    size_t chunk_words;
    size_t rem_words = nwords;
    dma_xfer_cfg_t *pcfg = hi2c->wdma_xfer_cfg;
    uint32_t i;

    /*
     * Build a linked list of DMA blocks that pushes DATA_CMD words to the I2C
     * TX FIFO. Each block is capped to I2C_DMA_XFER_BLK_SIZE words.
     */
    for (i = 0U; i < blk_cnt; i++)
    {
        chunk_words = rem_words;
        if (chunk_words > I2C_DMA_XFER_BLK_SIZE)
        {
            chunk_words = I2C_DMA_XFER_BLK_SIZE;
        }

        /* Program source/destination, byte size, burst, and next-block link. */
        pcfg[i].src = (uint64_t)(uintptr_t)
                &psrc_words[i * I2C_DMA_XFER_BLK_SIZE];
        pcfg[i].dst = (uint64_t)(hi2c->base_addr + I2C_DATA_CMD);
        pcfg[i].blk_size = (uint32_t)(chunk_words * 4U);
        pcfg[i].next_xfer_cfg = (i + 1U < blk_cnt) ? &pcfg[i + 1U] : NULL;
        pcfg[i].src_burst_len = burst_len;
        pcfg[i].dst_burst_len = burst_len;

        rem_words -= chunk_words;
    }

    ret = dma_setup_transfer(hi2c->wdma_handle, pcfg, blk_cnt,
            DMA_XFER_WIDTH4, DMA_XFER_WIDTH4);
    if (ret != 0)
    {
        return -EIO;
    }

    return 0;
}

static int32_t i2c_dma_setup_rdma(i2c_handle_t hi2c, uint8_t *pdst_bytes,
        size_t nbytes, uint32_t blk_cnt, dma_burst_len_t burst_len)
{
    int32_t ret;
    size_t chunk_bytes;
    size_t rem_bytes = nbytes;
    dma_xfer_cfg_t *pcfg = hi2c->rdma_xfer_cfg;
    uint32_t i;

    /*
     * Build a linked list of DMA blocks that reads bytes from the I2C RX FIFO
     * into the destination buffer, capped by I2C_DMA_XFER_BLK_SIZE.
     */
    for (i = 0U; i < blk_cnt; i++)
    {
        chunk_bytes = rem_bytes;
        if (chunk_bytes > I2C_DMA_XFER_BLK_SIZE)
        {
            chunk_bytes = I2C_DMA_XFER_BLK_SIZE;
        }

        /* Program source/destination, byte size, burst, and next-block link. */
        pcfg[i].src = (uint64_t)(hi2c->base_addr + I2C_DATA_CMD);
        pcfg[i].dst = (uint64_t)(uintptr_t)
                &pdst_bytes[i * I2C_DMA_XFER_BLK_SIZE];
        pcfg[i].blk_size = (uint32_t)chunk_bytes;
        pcfg[i].next_xfer_cfg = (i + 1U < blk_cnt) ? &pcfg[i + 1U] : NULL;
        pcfg[i].src_burst_len = burst_len;
        pcfg[i].dst_burst_len = burst_len;

        rem_bytes -= chunk_bytes;
    }

    ret = dma_setup_transfer(hi2c->rdma_handle, pcfg, blk_cnt,
            DMA_XFER_WIDTH1, DMA_XFER_WIDTH1);
    if (ret != 0)
    {
        return -EIO;
    }

    return 0;
}

static int32_t i2c_start_xfer_dma_mode(i2c_handle_t hi2c, i2c_operation_t op,
        void *buf, size_t nitems)
{
    int32_t ret;
    uint32_t blk_cnt;
    uint32_t burst;
    uint32_t burst_items;
    uint32_t *words;
    uint8_t *read_buf;
    dma_burst_len_t blk_burst_len;
    size_t blk_cnt_sz;
    size_t i;

    if (nitems == 0U)
    {
        return -EINVAL;
    }

    blk_cnt_sz = (nitems + I2C_DMA_XFER_BLK_SIZE - 1U) / I2C_DMA_XFER_BLK_SIZE;
    if ((blk_cnt_sz == 0U) || (blk_cnt_sz > MAX_I2C_BLK_XFERS))
    {
        return -EINVAL;
    }
    blk_cnt = (uint32_t)blk_cnt_sz;

   {
        burst_items = (uint32_t)(nitems % I2C_DMA_XFER_BLK_SIZE);

        if (burst_items == 0U)
        {
            burst_items = (nitems >= I2C_DMA_XFER_BLK_SIZE) ?
                    I2C_DMA_XFER_BLK_SIZE : (uint32_t)nitems;
        }
        i2c_find_burst_params(burst_items, &blk_burst_len, &burst);
    }

    if (op == I2C_WRITE)
    {
        words = (uint32_t *)buf;

        (void)i2c_ll_set_tdlr(hi2c->base_addr, burst);

        ret = i2c_dma_setup_wdma(hi2c, words, nitems, blk_cnt, blk_burst_len);
        if (ret != 0)
        {
            ERROR("I2C Tx DMA setup stage failed");
            return -EIO;
        }

        (void)cache_force_write_back((void *)words, (nitems * 4U));

        dma_start_transfer(hi2c->wdma_handle);
        i2c_ll_tdma_enable(hi2c->base_addr);
        i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT);
        return 0;
    }
    else
    {
        read_buf = (uint8_t *)buf;

        for (i = 0U; i < nitems; i++)
        {
            hi2c->wdma_buf[i] = (uint32_t)(1U << I2C_DATA_CMD_CMD_POS);
        }

        if ((nitems > 0U) && (hi2c->no_stop_flag == false))
        {
            hi2c->wdma_buf[nitems - 1U] =
                    (uint32_t)((1U << I2C_DATA_CMD_STOP_POS) |
                    (1U << I2C_DATA_CMD_CMD_POS));
        }

        (void)i2c_ll_set_tdlr(hi2c->base_addr, burst);
        (void)i2c_ll_set_rdlr(hi2c->base_addr, burst);

        ret = i2c_dma_setup_wdma(hi2c, hi2c->wdma_buf, nitems, blk_cnt,
                blk_burst_len);
        if (ret != 0)
        {
            ERROR("I2C Tx DMA setup stage failed");
            return -EIO;
        }

        ret = i2c_dma_setup_rdma(hi2c, read_buf, nitems, blk_cnt,
                blk_burst_len);
        if (ret != 0)
        {
            ERROR("I2C Rx DMA setup stage failed");
            return -EIO;
        }

        (void)cache_force_write_back((void *)(hi2c->wdma_buf), (nitems * 4U));
        (void)cache_force_invalidate((void *)read_buf, nitems);

        dma_start_transfer(hi2c->rdma_handle);
        dma_start_transfer(hi2c->wdma_handle);

        i2c_ll_rdma_enable(hi2c->base_addr);
        i2c_ll_tdma_enable(hi2c->base_addr);
        i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT);
        return 0;
    }
    return 0;
}

int32_t i2c_write_sync(i2c_handle_t const hi2c, void *const buf, size_t nbytes)
{
    int32_t ret;
    bool use_dma;
    uint8_t *wbuf = (uint8_t *)buf;

    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->role != I2C_ROLE_MASTER)
    {
        return -EPERM;
    }

    if (hi2c->slave_address == 0U)
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
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

        if (hi2c->is_busy == true)
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

    hi2c->operation = I2C_WRITE;

    hi2c->is_async = false;
    hi2c->xfer_size = nbytes;
    use_dma = hi2c->is_wdma_en;

    if (use_dma == false)
    {
        INFO("Starting I2C sync write (Interrupt mode)");
        ret = i2c_start_xfer_int_mode(hi2c, I2C_WRITE, wbuf, nbytes);
    }
    else
    {
        hi2c->bytes_left = 0U;
        INFO("Starting I2C sync write (DMA mode)");
        ret = i2c_start_xfer_dma_mode(hi2c, I2C_WRITE, buf, nbytes);
    }

    if (ret != 0)
    {
        return ret;
    }

    if (osal_semaphore_wait(hi2c->sem, OSAL_TIMEOUT_WAIT_FOREVER) == false)
    {
        return -EIO;
    }

    hi2c->is_busy = false;
    hi2c->no_stop_flag = false;

    if (hi2c->is_xfer_abort == true)
    {
        ERROR("Transfer aborted");
        return -EIO;
    }

    return 0;
}

int32_t i2c_write_async(i2c_handle_t const hi2c, void *const buf, size_t nbytes)
{
    int32_t ret;
    uint32_t blk_cnt = 0U;
    uint32_t burst = 0U;
    uint32_t burst_items;
    size_t blk_cnt_sz = 0U;
    dma_burst_len_t burst_len = DMA_BURST_LEN_1;
    uint32_t *wbuf32 = NULL;
    uint8_t *wbuf = (uint8_t *)buf;

    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if ((hi2c->role == I2C_ROLE_MASTER) && (hi2c->slave_address == 0U))
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
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

        if (hi2c->is_busy == true)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        if (hi2c->callback_fn == NULL)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Callback function is not set");
            return -EINVAL;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            return -EIO;
        }
    }

    hi2c->operation = I2C_WRITE;
    hi2c->is_async = true;
    hi2c->xfer_size = nbytes;

    if (hi2c->role == I2C_ROLE_SLAVE)
    {
        if (hi2c->is_dma_open && hi2c->is_wdma_en)
        {
            wbuf32 = (uint32_t *)buf;

            blk_cnt_sz = (nbytes + I2C_DMA_XFER_BLK_SIZE - 1U) / I2C_DMA_XFER_BLK_SIZE;
            if ((blk_cnt_sz == 0U) || (blk_cnt_sz > MAX_I2C_BLK_XFERS))
            {
                hi2c->is_busy = false;
                return -EINVAL;
            }
            blk_cnt = (uint32_t)blk_cnt_sz;
            hi2c->bytes_left = 0U;

            {
                burst_items = (uint32_t)(nbytes % I2C_DMA_XFER_BLK_SIZE);

                if (burst_items == 0U)
                {
                    burst_items = (nbytes >= I2C_DMA_XFER_BLK_SIZE) ?
                            I2C_DMA_XFER_BLK_SIZE : (uint32_t)nbytes;
                }
                i2c_find_burst_params(burst_items, &burst_len, &burst);
            }

            if (i2c_dma_setup_wdma(hi2c, wbuf32, nbytes, blk_cnt, burst_len) != 0)
            {
                hi2c->is_busy = false;
                ERROR("Slave TX DMA setup failed");
                return -EIO;
            }

            (void)cache_force_write_back((void *)wbuf32, nbytes * 4U);
            (void)i2c_ll_set_tdlr(hi2c->base_addr, burst);

            /*
             * Do NOT start DMA or enable TDMAE here. The DW I2C controller will
             * issue ABRT_SLVFLUSH_TXFIFO if the TX FIFO has data when the master
             * sends its READ address. DMA must start AFTER the RD_REQ event.
             * The RD_REQ ISR handler starts DMA when the master addresses us.
             */
            i2c_enable_interrupt(hi2c->base_addr, I2C_RD_REQ_INT);
            i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT);
            return 0;
        }

        /* IRQ fallback: arm buffer; ISR feeds TX FIFO from it on RD_REQ. */
        hi2c->buffer = (uint8_t *)buf;
        hi2c->bytes_left = nbytes;
        i2c_enable_interrupt(hi2c->base_addr, I2C_RD_REQ_INT);
        i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT);
        return 0;
    }

    if (hi2c->is_wdma_en == false)
    {
        INFO("Starting I2C async write (Interrupt mode)");
        ret = i2c_start_xfer_int_mode(hi2c, I2C_WRITE, wbuf, nbytes);
    }
    else
    {
        hi2c->bytes_left = 0U;
        INFO("Starting I2C master async write (DMA mode)");
        ret = i2c_start_xfer_dma_mode(hi2c, I2C_WRITE, buf, nbytes);
    }

    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

int32_t i2c_read_sync(i2c_handle_t const hi2c, void *const buf, size_t nbytes)
{
    int32_t ret;
    bool use_dma;
    uint8_t *rbuf = (uint8_t *)buf;

    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->role != I2C_ROLE_MASTER)
    {
        return -EPERM;
    }

    if (hi2c->slave_address == 0U)
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (hi2c->is_open == false)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                ERROR("Failed to acquire mutex");
                return -EIO;
            }
            ERROR("Instance is not open");
            return -EINVAL;
        }

        if (hi2c->is_busy == true)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                ERROR("Failed to acquire mutex");
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            ERROR("Failed to acquire mutex");
            return -EIO;
        }
    }

    hi2c->operation = I2C_READ;

    hi2c->is_async = false;
    hi2c->xfer_size = nbytes;
    hi2c->buffer = rbuf;
    use_dma = hi2c->is_rdma_en;

    if (use_dma == false)
    {
        INFO("Starting I2C sync read (Interrupt mode)");
        ret = i2c_start_xfer_int_mode(hi2c, I2C_READ, rbuf, nbytes);
    }
    else
    {
        hi2c->bytes_left = 0U;
        INFO("Starting I2C sync read (DMA mode)");
        ret = i2c_start_xfer_dma_mode(hi2c, I2C_READ, rbuf, nbytes);
    }

    if (ret != 0)
    {
        return ret;
    }

    if (osal_semaphore_wait(hi2c->sem, OSAL_TIMEOUT_WAIT_FOREVER) == false)
    {
        return -EIO;
    }

    hi2c->is_busy = false;
    hi2c->no_stop_flag = false;

    if (hi2c->is_xfer_abort == true)
    {
        ERROR("Transfer aborted");
        return -EIO;
    }

    return 0;
}

int32_t i2c_read_async(i2c_handle_t const hi2c, void *const buf, size_t nbytes)
{
    int32_t ret;
    uint8_t *rbuf = (uint8_t *)buf;
    uint32_t blk_cnt = 0U;
    uint32_t burst = 0U;
    uint32_t burst_items;
    uint32_t idle_wait;
    size_t blk_cnt_sz = 0U;
    dma_burst_len_t burst_len = DMA_BURST_LEN_1;

    if ((hi2c == NULL) || (buf == NULL) || (nbytes == 0U) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if ((hi2c->role == I2C_ROLE_MASTER) && (hi2c->slave_address == 0U))
    {
        ERROR("Slave address not set");
        return -ENXIO;
    }

    if (osal_mutex_lock(hi2c->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
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

        if (hi2c->is_busy == true)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Instance is busy");
            return -EBUSY;
        }

        if (hi2c->callback_fn == NULL)
        {
            if (osal_mutex_unlock(hi2c->mutex) == false)
            {
                return -EIO;
            }
            ERROR("Callback function is not set");
            return -EINVAL;
        }

        hi2c->is_busy = true;
        if (osal_mutex_unlock(hi2c->mutex) == false)
        {
            return -EIO;
        }
    }

    hi2c->operation = I2C_READ;
    hi2c->is_async = true;
    hi2c->xfer_size = nbytes;
    hi2c->buffer = rbuf;

    if (hi2c->role == I2C_ROLE_SLAVE)
    {
        if (hi2c->is_dma_open && hi2c->is_rdma_en)
        {
            idle_wait = 10000U;
            while (((i2c_read_status(hi2c->base_addr) &
                            I2C_STATUS_IC_STATUS_ACTIVITY_MASK) != 0U) &&
                    (idle_wait > 0U))
            {
                idle_wait--;
            }
            if ((i2c_read_status(hi2c->base_addr) &
                        I2C_STATUS_IC_STATUS_ACTIVITY_MASK) != 0U)
            {
                hi2c->is_busy = false;
                return -EBUSY;
            }

            i2c_drain_rx_fifo(hi2c->base_addr);
            i2c_ll_clear_rx_over(hi2c->base_addr);

            blk_cnt_sz = (nbytes + I2C_DMA_XFER_BLK_SIZE - 1U) / I2C_DMA_XFER_BLK_SIZE;
            if ((blk_cnt_sz == 0U) || (blk_cnt_sz > MAX_I2C_BLK_XFERS))
            {
                hi2c->is_busy = false;
                return -EINVAL;
            }
            blk_cnt = (uint32_t)blk_cnt_sz;
            hi2c->bytes_left = 0U;

            {
                burst_items = (uint32_t)(nbytes % I2C_DMA_XFER_BLK_SIZE);

                if (burst_items == 0U)
                {
                    burst_items = (nbytes >= I2C_DMA_XFER_BLK_SIZE) ?
                            I2C_DMA_XFER_BLK_SIZE : (uint32_t)nbytes;
                }
                i2c_find_burst_params(burst_items, &burst_len, &burst);
            }

            if (i2c_dma_setup_rdma(hi2c, rbuf, nbytes, blk_cnt, burst_len) != 0)
            {
                hi2c->is_busy = false;
                ERROR("Slave RX DMA setup failed");
                return -EIO;
            }

            (void)cache_force_invalidate((void *)rbuf, nbytes);
            (void)i2c_ll_set_rdlr(hi2c->base_addr, burst);
            i2c_disable_interrupt(hi2c->base_addr, I2C_RX_FULL_INT);

            if (dma_start_transfer(hi2c->rdma_handle) != 0)
            {
                hi2c->is_busy = false;
                i2c_enable_interrupt(hi2c->base_addr, I2C_RX_FULL_INT);
                ERROR("Slave RX DMA start failed");
                return -EIO;
            }

            i2c_ll_rdma_enable(hi2c->base_addr);
            i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT);
            return 0;
        }

        /* IRQ fallback: arm buffer; ISR copies RX FIFO data into it. */
        hi2c->bytes_left = nbytes;
        i2c_enable_interrupt(hi2c->base_addr, I2C_RX_FULL_INT);
        i2c_enable_interrupt(hi2c->base_addr, I2C_STOP_DET_INT);
        i2c_enable_interrupt(hi2c->base_addr, I2C_TX_ABORT_INT);
        return 0;
    }

    if (hi2c->is_rdma_en == false)
    {
        INFO("Starting I2C async read (Interrupt mode)");
        ret = i2c_start_xfer_int_mode(hi2c, I2C_READ, rbuf, nbytes);
    }
    else
    {
        hi2c->bytes_left = 0U;
        INFO("Starting I2C master async read (DMA mode)");
        ret = i2c_start_xfer_dma_mode(hi2c, I2C_READ, rbuf, nbytes);
    }

    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

int32_t i2c_cancel(i2c_handle_t const hi2c)
{
    if ((hi2c == NULL) || (hi2c->is_open == false))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }

    if (hi2c->role != I2C_ROLE_MASTER)
    {
        return -EPERM;
    }
    if (hi2c->is_busy == false)
    {
        ERROR("I2C instance is not busy");
        return -EPERM;
    }
    i2c_ll_cancel(hi2c->base_addr);
    hi2c->is_xfer_abort = false;
    hi2c->is_busy = false;
    hi2c->no_stop_flag = false;
    return 0;
}

void i2c_isr(void *data)
{
    uint8_t byte;
    uint16_t nbytes;
    uint32_t base_addr;
    bool no_stop_flag;
    bool irq_rx_armed;
    bool dma_rx_active;
    uint32_t status;
    uint32_t raw;
    size_t rx_len;
    size_t pos;

    i2c_handle_t pi2c_peripheral = (i2c_handle_t)data;
    if (pi2c_peripheral == NULL)
    {
        return;
    }

    if (pi2c_peripheral->role == I2C_ROLE_SLAVE)
    {
        base_addr = pi2c_peripheral->base_addr;
        raw = i2c_ll_get_raw_interrupt_status(base_addr);

        i2c_slave_handle_tx_abort(pi2c_peripheral, base_addr, raw);

        if ((raw & I2C_RAW_INTR_STAT_RX_OVER_MASK) != 0U)
        {
            pi2c_peripheral->slave_rx_over_cnt++;
            i2c_ll_clear_rx_over(base_addr);
        }

        /*
         * Process STOP_DET before RX_FULL/RD_REQ, match
         */
        raw = i2c_ll_get_raw_interrupt_status(base_addr);
        i2c_slave_handle_stop_detect(pi2c_peripheral, base_addr, raw);

        rx_len = 0U;
        dma_rx_active = (pi2c_peripheral->is_busy == true) &&
                            (pi2c_peripheral->operation == I2C_READ) &&
                            (pi2c_peripheral->is_rdma_en == true);
        irq_rx_armed = (pi2c_peripheral->is_busy == true) &&
                            (pi2c_peripheral->is_async == true) &&
                            (pi2c_peripheral->operation == I2C_READ) &&
                            (pi2c_peripheral->is_rdma_en == false);

        if (dma_rx_active)
        {
            i2c_slave_handle_read_request(pi2c_peripheral, base_addr);
            return;
        }

        while (i2c_read_fifo(base_addr, &byte, 1U) == 1U)
        {

            if (irq_rx_armed && (pi2c_peripheral->bytes_left > 0U))
            {
                pos = pi2c_peripheral->xfer_size - pi2c_peripheral->bytes_left;
                pi2c_peripheral->buffer[pos] = byte;
                pi2c_peripheral->bytes_left--;
            }
            else if (pi2c_peripheral->is_rdma_en)
            {
                /* RDMA enabled but no RX transfer armed: discard bytes. */
            }
            else
            {
                /* Byte-oriented callback mode */
                if (!pi2c_peripheral->slave_write_started)
                {
                    pi2c_peripheral->slave_write_started = true;
                    if (pi2c_peripheral->slave_write_requested_cb != NULL)
                    {
                        pi2c_peripheral->slave_write_requested_cb(pi2c_peripheral,
                                pi2c_peripheral->slave_cb_usercontext);
                    }
                }

                if (pi2c_peripheral->slave_write_received_cb != NULL)
                {
                    pi2c_peripheral->slave_write_received_cb(pi2c_peripheral,
                            byte,
                            pi2c_peripheral->slave_cb_usercontext);
                }
            }
            rx_len++;
        }

        (void)rx_len;

        i2c_slave_handle_read_request(pi2c_peripheral, base_addr);
        return;
    }

    if (pi2c_peripheral->role == I2C_ROLE_MASTER)
    {
        base_addr = pi2c_peripheral->base_addr;

        status = i2c_get_interrupt_status(base_addr);
        i2c_clear_interrupt(pi2c_peripheral->base_addr);
        no_stop_flag = pi2c_peripheral->no_stop_flag;

        if ((status & I2C_TX_ABORT_INT) == I2C_TX_ABORT_INT)
        {
            i2c_disable_interrupt(base_addr, I2C_TX_ABORT_INT | I2C_TX_EMPTY_INT |
                    I2C_RX_FULL_INT);

            pi2c_peripheral->is_xfer_abort = true;
            if ((pi2c_peripheral->is_async) == true)
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
                if (pi2c_peripheral->operation == I2C_READ)
                {
                    /* Master READ: TX_EMPTY means FIFO has room for more READ commands. */
                    if (pi2c_peripheral->rd_cmds_left > 0U)
                    {
                        pi2c_peripheral->rd_cmds_left -= i2c_enq_read_cmd(base_addr,
                                (uint32_t)pi2c_peripheral->rd_cmds_left, no_stop_flag);
                    }
                    else
                    {
                        i2c_disable_interrupt(base_addr, I2C_TX_EMPTY_INT);
                    }
                }
                else
                {
                    /* Master WRITE: TX_EMPTY means FIFO has room for more data bytes. */
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
                        if ((pi2c_peripheral->is_async) == true)
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

                    if (pi2c_peripheral->is_async == true)
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
}

static void i2c_wdma_callback(void *data)
{
    i2c_handle_t pi2c_peripheral;

    pi2c_peripheral = (i2c_handle_t)data;

    i2c_ll_tdma_disable(pi2c_peripheral->base_addr);

    /* Re-enable RD_REQ ISR path now that DMA TX is done (slave mode). */
    if (pi2c_peripheral->role == I2C_ROLE_SLAVE)
    {
        i2c_enable_interrupt(pi2c_peripheral->base_addr, I2C_RD_REQ_INT);
    }

    if (pi2c_peripheral->operation == I2C_WRITE)
    {
        if (pi2c_peripheral->is_async == true)
        {
            pi2c_peripheral->is_busy = false;
            pi2c_peripheral->no_stop_flag = false;
            if (pi2c_peripheral->is_xfer_abort == true)
            {
                pi2c_peripheral->callback_fn(I2C_NACK, pi2c_peripheral->cb_usercontext);
            }
            else
            {
                pi2c_peripheral->callback_fn(I2C_SUCCESS, pi2c_peripheral->cb_usercontext);
            }
        }
        else
        {
            (void)osal_semaphore_post(pi2c_peripheral->sem);
        }
    }
}


static void i2c_rdma_callback(void *data)
{
    i2c_handle_t pi2c_peripheral;

    pi2c_peripheral = (i2c_handle_t)data;

    i2c_ll_rdma_disable(pi2c_peripheral->base_addr);

    /* Re-enable RX_FULL ISR path now that DMA RX is done (slave mode). */
    if (pi2c_peripheral->role == I2C_ROLE_SLAVE)
    {
        i2c_enable_interrupt(pi2c_peripheral->base_addr, I2C_RX_FULL_INT);
    }

    if (pi2c_peripheral->operation == I2C_READ)
    {
        if (pi2c_peripheral->is_async == true)
        {
            pi2c_peripheral->is_busy = false;
            pi2c_peripheral->no_stop_flag = false;
            if (pi2c_peripheral->is_xfer_abort == true)
            {
                pi2c_peripheral->callback_fn(I2C_NACK, pi2c_peripheral->cb_usercontext);
            }
            else
            {
                pi2c_peripheral->callback_fn(I2C_SUCCESS, pi2c_peripheral->cb_usercontext);
            }
        }
        else
        {
            (void)osal_semaphore_post(pi2c_peripheral->sem);
        }
    }
}
