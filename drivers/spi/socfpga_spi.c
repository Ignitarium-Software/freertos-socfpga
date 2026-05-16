/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for SPI
 */

#include <string.h>
#include <stdbool.h>
#include "socfpga_defines.h"
#include "socfpga_spi.h"
#include "socfpga_spi_ll.h"
#include "socfpga_spi_reg.h"
#include "socfpga_dma.h"
#include "socfpga_cache.h"
#include "socfpga_interrupt.h"
#include "osal.h"
#include "osal_log.h"

#define SPI_MASTER_NUM_INSTANCES    2U
#define SPI_SLAVE_NUM_INSTANCES     2U
#define SPI_MAX_TRANSFER_SIZE       2048U
#define SPI_FIFO_DEPTH              256U
#define SPI_RXFTLR_DEFAULT          ((SPI_FIFO_DEPTH * 5U) / 8U)
/*
 * SPI DMA multi-block (LLI) block size in bytes.
 * Tune this value based on throughput/memory constraints.
 */
#define SPI_DMA_XFER_BLK_SIZE       512U
/*
 * Max LLI entries per DMA channel.
 * Keep in sync with drivers/dma/socfpga_dma.c (MAX_LLI_PER_CHANNEL).
 */
#define SPI_DMA_MAX_LLI_PER_CHANNEL 128U

/* Derive DMA burst length directly from transfer size. */
#define SPI_DMA_GET_BURST_LEN(nbytes) \
    (((nbytes) > 8U) ? DMA_BURST_LEN_8 : \
            (((nbytes) > 4U) ? DMA_BURST_LEN_4 : DMA_BURST_LEN_1))

/* Derive burst length in items (1/4/8) directly from transfer size. */
#define SPI_DMA_GET_BURST_ITEMS(nbytes) \
    (((nbytes) > 8U) ? 8U : (((nbytes) > 4U) ? 4U : 1U))

/* Context used to share a single DMA callback for TX and RX. */
typedef struct
{
    spi_handle_t hspi;
    bool is_tx;
} spi_dma_cb_ctx_t;

struct spi_handle
{
    bool is_open;
    bool is_rx_busy;
    bool is_tx_busy;
    bool is_rx_async;
    bool is_tx_async;
    bool is_rx_on;
    bool is_tx_on;
    uint32_t instance;
    spi_role_t role;
    uint32_t base_addr;
    uint32_t slave_id;
    uint16_t tx_size;
    uint16_t rx_size;
    uint16_t tx_bytes_left;
    uint16_t rx_bytes_left;
    uint8_t rx_threshold;
    uint8_t *tx_buf;
    uint8_t *rx_buf;
    spi_callback_t callback_fn;
    void *cb_user_context;
    osal_mutex_def_t mutex_mem;
    osal_semaphore_def_t sem_mem;
    osal_mutex_t mutex;
    osal_semaphore_t sem;
    bool dma_enabled;
    dma_handle_t h_dma_tx;
    dma_handle_t h_dma_rx;
    bool dma_tx_done;
    bool dma_rx_done;
    spi_dma_cb_ctx_t dma_cb_tx_ctx;
    spi_dma_cb_ctx_t dma_cb_rx_ctx;
};

static struct spi_handle spi_master_desc[SPI_MASTER_NUM_INSTANCES];
static struct spi_handle spi_slave_desc[SPI_SLAVE_NUM_INSTANCES];

void spi_isr(void *param);
static dma_peri_id_t spi_get_dma_tx_peri_id(uint32_t instance, spi_role_t role);
static dma_peri_id_t spi_get_dma_rx_peri_id(uint32_t instance, spi_role_t role);
static void spi_dma_callback(void *p_usr_cntxt);
static void spi_dma_close_channels(spi_handle_t hspi);
static void spi_setup_dma_xfer(spi_handle_t hspi, uint32_t *tx_src,
        uint8_t *rx_dst, uint16_t nbytes, uint32_t *pblk_cnt);
static int32_t spi_dma_init(spi_handle_t hspi, spi_dma_config_t *cfg);
static int32_t spi_dma_deinit(spi_handle_t hspi);

/*
 * Internal DMA buffers (cacheline aligned):
 * - Dummy TX buffers are used only when txbuf is NULL (slave RX-only path); TX
 *   DMA is not started in that case, but we keep valid pointer arithmetic.
 * - RX discard buffers are used only when rxbuf is NULL.
 */
static uint32_t spi_master_tx_dma_dummy_buf[SPI_MASTER_NUM_INSTANCES]
        [SPI_MAX_TRANSFER_SIZE] __attribute__((aligned(64)));
static uint32_t spi_slave_tx_dma_dummy_buf[SPI_SLAVE_NUM_INSTANCES]
        [SPI_MAX_TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t spi_master_rx_dma_discard_buf[SPI_MASTER_NUM_INSTANCES]
        [SPI_MAX_TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t spi_slave_rx_dma_discard_buf[SPI_SLAVE_NUM_INSTANCES]
        [SPI_MAX_TRANSFER_SIZE] __attribute__((aligned(64)));

/* Static LLI transfer lists (one TX and one RX chain per SPI instance). */
static dma_xfer_cfg_t spi_master_tx_dma_xfer_list[SPI_MASTER_NUM_INSTANCES]
        [SPI_DMA_MAX_LLI_PER_CHANNEL];
static dma_xfer_cfg_t spi_master_rx_dma_xfer_list[SPI_MASTER_NUM_INSTANCES]
        [SPI_DMA_MAX_LLI_PER_CHANNEL];
static dma_xfer_cfg_t spi_slave_tx_dma_xfer_list[SPI_SLAVE_NUM_INSTANCES]
        [SPI_DMA_MAX_LLI_PER_CHANNEL];
static dma_xfer_cfg_t spi_slave_rx_dma_xfer_list[SPI_SLAVE_NUM_INSTANCES]
        [SPI_DMA_MAX_LLI_PER_CHANNEL];

/**
 * @brief Check if the SPI handle is valid.
 */
static bool spi_is_handle_valid(spi_handle_t handle)
{
    bool in_master;
    bool in_slave;

    if (handle == NULL)
    {
        return false;
    }

    in_master = (handle >= &spi_master_desc[0]) &&
            (handle < &spi_master_desc[SPI_MASTER_NUM_INSTANCES]);
    in_slave = (handle >= &spi_slave_desc[0]) &&
            (handle < &spi_slave_desc[SPI_SLAVE_NUM_INSTANCES]);
    return in_master || in_slave;
}

static dma_peri_id_t spi_get_dma_tx_peri_id(uint32_t instance, spi_role_t role)
{
    if (role == SPI_ROLE_SLAVE)
    {
        return (instance == 1U) ? DMA_ID_SPI1_SLAVE_TX : DMA_ID_SPI0_SLAVE_TX;
    }
    return (instance == 1U) ? DMA_ID_SPI1_MASTER_TX : DMA_ID_SPI0_MASTER_TX;
}

static dma_peri_id_t spi_get_dma_rx_peri_id(uint32_t instance, spi_role_t role)
{
    if (role == SPI_ROLE_SLAVE)
    {
        return (instance == 1U) ? DMA_ID_SPI1_SLAVE_RX : DMA_ID_SPI0_SLAVE_RX;
    }
    return (instance == 1U) ? DMA_ID_SPI1_MASTER_RX : DMA_ID_SPI0_MASTER_RX;
}

static void spi_setup_dma_xfer(spi_handle_t hspi, uint32_t *tx_src,
        uint8_t *rx_dst, uint16_t nbytes, uint32_t *pblk_cnt)
{
    uint32_t blk_cnt;
    uint32_t blk_idx;
    uint32_t blk_off;
    uint32_t blk_bytes;
    dma_burst_len_t burst_len;
    dma_xfer_cfg_t *tx_list;
    dma_xfer_cfg_t *rx_list;

    if (hspi->role == SPI_ROLE_SLAVE)
    {
        tx_list = spi_slave_tx_dma_xfer_list[hspi->instance];
        rx_list = spi_slave_rx_dma_xfer_list[hspi->instance];
    }
    else
    {
        tx_list = spi_master_tx_dma_xfer_list[hspi->instance];
        rx_list = spi_master_rx_dma_xfer_list[hspi->instance];
    }

    /*
     * Preconditions:
     * Caller validates hspi, DMA channel handles, and overall transfer size.
     * This function only builds RX/TX linked lists based on nbytes.
     */
    blk_cnt = (nbytes + SPI_DMA_XFER_BLK_SIZE - 1U) / SPI_DMA_XFER_BLK_SIZE;

    burst_len = SPI_DMA_GET_BURST_LEN(nbytes);

    DEBUG("SPI DMA LLI setup: nbytes=%u blk_cnt=%lu burst_len=%u",
            nbytes,
            (unsigned long)blk_cnt,
            (unsigned)burst_len);

    for (blk_idx = 0U; blk_idx < blk_cnt; blk_idx++)
    {
        blk_off = blk_idx * SPI_DMA_XFER_BLK_SIZE;
        blk_bytes = nbytes - blk_off;
        if (blk_bytes > SPI_DMA_XFER_BLK_SIZE)
        {
            blk_bytes = SPI_DMA_XFER_BLK_SIZE;
        }

        /* RX: SPI DR0 -> memory (or discard buffer). */
        rx_list[blk_idx].src =
                (uint64_t)(uintptr_t)(hspi->base_addr + SPI_DR0);
        rx_list[blk_idx].dst =
                (uint64_t)(uintptr_t)(rx_dst + blk_off);
        rx_list[blk_idx].blk_size = blk_bytes;
        rx_list[blk_idx].src_burst_len = burst_len;
        rx_list[blk_idx].dst_burst_len = burst_len;
        rx_list[blk_idx].next_xfer_cfg =
                (blk_idx + 1U < blk_cnt) ? &rx_list[blk_idx + 1U] : NULL;

        /* TX: memory -> SPI DR0 (DMA TX uses 32-bit items). */
        tx_list[blk_idx].src =
                (uint64_t)(uintptr_t)(&tx_src[blk_off]);
        tx_list[blk_idx].dst =
                (uint64_t)(uintptr_t)(hspi->base_addr + SPI_DR0);
        tx_list[blk_idx].blk_size =
                (uint32_t)(sizeof(uint32_t) * blk_bytes);
        tx_list[blk_idx].src_burst_len = burst_len;
        tx_list[blk_idx].dst_burst_len = burst_len;
        tx_list[blk_idx].next_xfer_cfg =
                (blk_idx + 1U < blk_cnt) ? &tx_list[blk_idx + 1U] : NULL;

        DEBUG("SPI DMA LLI[%lu]: off=%lu bytes=%lu rx_dst=%p tx_src=%p",
                (unsigned long)blk_idx,
                (unsigned long)blk_off,
                (unsigned long)blk_bytes,
                (void *)(uintptr_t)(rx_dst + blk_off),
                (void *)(uintptr_t)(&tx_src[blk_off]));
    }

    *pblk_cnt = blk_cnt;
}

static void spi_dma_callback(void *p_usr_cntxt)
{
    spi_dma_cb_ctx_t *ctx = (spi_dma_cb_ctx_t *)p_usr_cntxt;

    if ((ctx == NULL) || (ctx->hspi == NULL))
    {
        return;
    }

    if (ctx->is_tx == true)
    {
        ctx->hspi->dma_tx_done = true;
    }
    else
    {
        ctx->hspi->dma_rx_done = true;
    }

    if ((ctx->hspi->dma_rx_done == true) && (ctx->hspi->dma_tx_done == true))
    {
        /* Mark the DMA transfer as complete for ioctl byte counters. */
        ctx->hspi->tx_bytes_left = 0U;
        ctx->hspi->rx_bytes_left = 0U;

        if ((ctx->hspi->is_rx_async == true)
                || (ctx->hspi->is_tx_async == true))
        {
            /* Deassert CS before notifying the application. */
            if (ctx->hspi->role == SPI_ROLE_MASTER)
            {
                spi_select_chip(ctx->hspi->instance, 0U);
            }

            ctx->hspi->is_tx_async = false;
            ctx->hspi->is_rx_async = false;
            ctx->hspi->is_tx_busy = false;
            ctx->hspi->is_rx_busy = false;

            if (ctx->hspi->callback_fn != NULL)
            {
                ctx->hspi->callback_fn(SPI_SUCCESS, ctx->hspi->cb_user_context);
            }
        }
        else
        {
            /* Use the same completion semaphore as interrupt-driven mode. */
            (void)osal_semaphore_post(ctx->hspi->sem);
        }
        ctx->hspi->dma_tx_done = false;
        ctx->hspi->dma_rx_done = false;
    }
}

static void spi_dma_close_channels(spi_handle_t hspi)
{
    if (hspi->h_dma_tx != NULL)
    {
        (void)dma_close(hspi->h_dma_tx);
        hspi->h_dma_tx = NULL;
    }
    if (hspi->h_dma_rx != NULL)
    {
        (void)dma_close(hspi->h_dma_rx);
        hspi->h_dma_rx = NULL;
    }
}

spi_handle_t spi_open(uint32_t instance)
{
    spi_handle_t handle;
    socfpga_hpu_interrupt_t int_id;
    socfpga_interrupt_err_t ret;

    if (instance >= SPI_MASTER_NUM_INSTANCES)
    {
        ERROR("Invalid SPI Instance");
        return NULL;
    }

    handle = &spi_master_desc[instance];
    if (handle->is_open)
    {
        ERROR("SPI instance already open");
        return NULL;
    }

    memset(handle, 0, sizeof(struct spi_handle));
    handle->instance = instance;
    handle->role = SPI_ROLE_MASTER;
    handle->base_addr = spi_get_base_addr(instance, handle->role);

    handle->mutex = osal_mutex_create(&handle->mutex_mem);
    handle->sem = osal_semaphore_create(&handle->sem_mem);

    if ((handle->mutex == NULL) || (handle->sem == NULL))
    {
        ERROR("Failed to create SPI synchronization primitives");
        return NULL;
    }

    int_id = SPI_GET_INT_ID_MASTER(instance);
    ret = interrupt_register_isr(int_id, spi_isr, handle);
    if (ret != ERR_OK)
    {
        osal_mutex_delete(handle->mutex);
        osal_semaphore_delete(handle->sem);
        ERROR("Failed to register SPI interrupt");
        return NULL;
    }
    ret = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_SPI);
    if (ret != ERR_OK)
    {
        osal_mutex_delete(handle->mutex);
        osal_semaphore_delete(handle->sem);
        ERROR("Failed to enable SPI interrupt");
        return NULL;
    }

    spi_init(instance, handle->role);

    spi_disable_interrupt(handle->base_addr, SPI_ALL_INTERRUPTS);

    handle->is_open = true;

    return handle;
}

spi_handle_t spi_slave_open(uint32_t instance)
{
    spi_handle_t handle;
    socfpga_hpu_interrupt_t int_id;
    socfpga_interrupt_err_t ret;

    if (instance >= SPI_SLAVE_NUM_INSTANCES)
    {
        ERROR("Invalid SPI Instance");
        return NULL;
    }

    handle = &spi_slave_desc[instance];
    if (handle->is_open)
    {
        ERROR("SPI instance already open");
        return NULL;
    }

    memset(handle, 0, sizeof(struct spi_handle));
    handle->instance = instance;
    handle->role = SPI_ROLE_SLAVE;
    handle->base_addr = spi_get_base_addr(instance, handle->role);

    handle->mutex = osal_mutex_create(&handle->mutex_mem);
    handle->sem = osal_semaphore_create(&handle->sem_mem);

    if ((handle->mutex == NULL) || (handle->sem == NULL))
    {
        ERROR("Failed to create SPI synchronization primitives");
        return NULL;
    }

    int_id = SPI_GET_INT_ID_SLAVE(instance);
    ret = interrupt_register_isr(int_id, spi_isr, handle);
    if (ret != ERR_OK)
    {
        osal_mutex_delete(handle->mutex);
        osal_semaphore_delete(handle->sem);
        ERROR("Failed to register SPI interrupt");
        return NULL;
    }
    ret = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_SPI);
    if (ret != ERR_OK)
    {
        osal_mutex_delete(handle->mutex);
        osal_semaphore_delete(handle->sem);
        ERROR("Failed to enable SPI interrupt");
        return NULL;
    }

    spi_init(instance, handle->role);

    spi_disable_interrupt(handle->base_addr, SPI_ALL_INTERRUPTS);
    spi_enable_interrupt(handle->base_addr, SPI_RX_OVERFLOW_INT);

    handle->is_open = true;

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

    if (spi_is_handle_valid(hspi) == false)
    {
        ERROR("SPI Handle is invalid");
        return -EINVAL;
    }

    if (hspi->is_open == false)
    {
        ERROR("SPI instance not open");
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

        spi_disable(hspi->base_addr);
        /* Keep SSI disabled if configuration fails. */
        result = spi_set_config(hspi->base_addr,
                ((spi_cfg_t *)buf)->clk,
                ((spi_cfg_t *)buf)->mode);
        if (result != 0)
        {
            return result;
        }
        spi_set_transfermode(hspi->base_addr, SPI_TX_RX_MOD);
        spi_enable(hspi->base_addr);
        break;

    case SPI_GET_CONFIG:
        if (buf == NULL)
        {
            ERROR("Buffer cannot be NULL");
            result = -EINVAL;
            break;
        }
        spi_get_config(hspi->base_addr, &config.clk, &config.mode);
        config.role = hspi->role;
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

    case SPI_ENABLE_DMA:
        if (hspi->is_tx_busy || hspi->is_rx_busy)
        {
            ERROR("SPI bus is busy");
            return -EBUSY;
        }
        if (hspi->dma_enabled == true)
        {
            ERROR("SPI DMA is already enabled");
            return -EBUSY;
        }
        if (buf == NULL)
        {
            ERROR("Config cannot be NULL");
            return -EINVAL;
        }
        if (spi_dma_init(hspi, (spi_dma_config_t *)buf) != 0)
        {
            ERROR("SPI DMA init failed");
            return -EIO;
        }

        spi_ll_enable_dma(hspi->base_addr, true, true);
        break;

    case SPI_DISABLE_DMA:
        if (hspi->is_tx_busy || hspi->is_rx_busy)
        {
            ERROR("SPI bus is busy");
            return -EBUSY;
        }
        if (hspi->dma_enabled == false)
        {
            ERROR("SPI DMA is not enabled");
            return -EINVAL;
        }
        if (spi_dma_deinit(hspi) != 0)
        {
            ERROR("SPI DMA deinit failed");
            return -EIO;
        }
        break;

    default:
        ERROR("Invalid IOCTL request");
        result = -EINVAL;
        break;
    }
    return result;
}

static int32_t spi_dma_init(spi_handle_t hspi, spi_dma_config_t *cfg)
{
    dma_config_t tx_cfg = { 0 };
    dma_config_t rx_cfg = { 0 };
    dma_peri_id_t tx_peri_id;
    dma_peri_id_t rx_peri_id;

    tx_peri_id = spi_get_dma_tx_peri_id(hspi->instance, hspi->role);
    rx_peri_id = spi_get_dma_rx_peri_id(hspi->instance, hspi->role);

    hspi->h_dma_tx = dma_open(cfg->tx_instance, cfg->tx_channel);
    hspi->h_dma_rx = dma_open(cfg->rx_instance, cfg->rx_channel);
    if ((hspi->h_dma_tx == NULL) || (hspi->h_dma_rx == NULL))
    {
        spi_dma_close_channels(hspi);
        return -EIO;
    }

    tx_cfg.instance = cfg->tx_instance;
    tx_cfg.ch_dir = DMA_MEM_TO_PERI_DMAC;
    tx_cfg.ch_prio = cfg->tx_prio;
    tx_cfg.peri_id = tx_peri_id;
    tx_cfg.callback = spi_dma_callback;
    hspi->dma_cb_tx_ctx.hspi = hspi;
    hspi->dma_cb_tx_ctx.is_tx = true;
    tx_cfg.usr_cntxt = &hspi->dma_cb_tx_ctx;

    rx_cfg.instance = cfg->rx_instance;
    rx_cfg.ch_dir = DMA_PERI_TO_MEM_DMAC;
    rx_cfg.ch_prio = cfg->rx_prio;
    rx_cfg.peri_id = rx_peri_id;
    rx_cfg.callback = spi_dma_callback;
    hspi->dma_cb_rx_ctx.hspi = hspi;
    hspi->dma_cb_rx_ctx.is_tx = false;
    rx_cfg.usr_cntxt = &hspi->dma_cb_rx_ctx;

    if (dma_config(hspi->h_dma_tx, &tx_cfg) != 0)
    {
        spi_dma_close_channels(hspi);
        return -EIO;
    }

    if (dma_config(hspi->h_dma_rx, &rx_cfg) != 0)
    {
        spi_dma_close_channels(hspi);
        return -EIO;
    }

    hspi->dma_tx_done = false;
    hspi->dma_rx_done = false;
    hspi->dma_enabled = true;
    return 0;
}

static int32_t spi_dma_deinit(spi_handle_t hspi)
{
    spi_ll_disable_dma(hspi->base_addr);
    spi_ll_set_dma_thresholds(hspi->base_addr, 0U, 0U);
    spi_dma_close_channels(hspi);

    hspi->dma_enabled = false;
    hspi->dma_tx_done = false;
    hspi->dma_rx_done = false;
    return 0;
}

/**
 * @brief Start SPI transfer
 */
static void spi_xfer(spi_handle_t hspi, uint16_t nbytes)
{
    uint32_t mode;

    hspi->tx_bytes_left = hspi->is_tx_on ? nbytes : 0U;
    hspi->rx_bytes_left = nbytes;

    if (hspi->role == SPI_ROLE_SLAVE)
    {
        if ((hspi->is_tx_on == true) && (hspi->is_rx_on == true))
        {
            mode = SPI_TX_RX_MOD;
        }
        else if (hspi->is_tx_on == true)
        {
            mode = SPI_TX_MOD;
        }
        else
        {
            mode = SPI_RX_MOD;
        }

        spi_set_transfermode(hspi->base_addr, mode);
        spi_set_slave_output(hspi->base_addr, hspi->is_tx_on);

        /*
         * Set RXFTLR so RXFIS fires before FIFO is full
         * Use a depth-based default and lower it as we near completion
         */
        {
            uint32_t thr = SPI_RXFTLR_DEFAULT;

            if (nbytes <= thr)
            {
                thr = (uint32_t)nbytes - 1U;
            }

            if (thr > (SPI_FIFO_DEPTH - 1U))
            {
                thr = SPI_FIFO_DEPTH - 1U;
            }

            hspi->rx_threshold = (uint8_t)thr;
            spi_ll_set_rx_threshold(hspi->base_addr, hspi->rx_threshold);
        }
    }

    spi_enable_interrupt(hspi->base_addr, SPI_RX_FULL_INT);

    if (hspi->role == SPI_ROLE_MASTER)
    {
        spi_select_chip(hspi->instance, hspi->slave_id);
    }

    if (hspi->is_tx_on == true)
    {
        spi_enable_interrupt(hspi->base_addr, SPI_TX_EMPTY_INT);
    }
}

/**
 * @brief Start SPI transfer through DMA
 */
static int32_t spi_dma_transfer(spi_handle_t const hspi, void *const txbuf,
        void *const rxbuf, uint16_t nbytes)
{
    uint32_t blk_cnt;
    dma_burst_len_t burst_len;
    uint8_t *rx_dst;
    bool start_tx_dma;
    bool start_rx_dma;
    uint32_t *tx_src;
    dma_xfer_cfg_t *tx_list;
    dma_xfer_cfg_t *rx_list;
    uint32_t mode;

    burst_len = SPI_DMA_GET_BURST_LEN(nbytes);

    INFO("SPI DMA transfer: nbytes=%u burst_len=%u", nbytes,
            (unsigned)burst_len);

    (void)burst_len;

    tx_src = (uint32_t *)txbuf;

    if (hspi->role == SPI_ROLE_SLAVE)
    {
        rx_dst = spi_slave_rx_dma_discard_buf[hspi->instance];
        if (tx_src == NULL)
        {
            tx_src = spi_slave_tx_dma_dummy_buf[hspi->instance];
        }
        tx_list = spi_slave_tx_dma_xfer_list[hspi->instance];
        rx_list = spi_slave_rx_dma_xfer_list[hspi->instance];
    }
    else
    {
        rx_dst = spi_master_rx_dma_discard_buf[hspi->instance];
        if (tx_src == NULL)
        {
            tx_src = spi_master_tx_dma_dummy_buf[hspi->instance];
        }
        tx_list = spi_master_tx_dma_xfer_list[hspi->instance];
        rx_list = spi_master_rx_dma_xfer_list[hspi->instance];
    }

    /* Always build RX DMA against a valid destination buffer. */
    if (rxbuf != NULL)
    {
        rx_dst = (uint8_t *)rxbuf;
    }

    spi_disable_interrupt(hspi->base_addr, SPI_ALL_INTERRUPTS);
    if (hspi->role == SPI_ROLE_SLAVE)
    {
        spi_enable_interrupt(hspi->base_addr, SPI_RX_OVERFLOW_INT);
    }

    /*
     * Transfer mode and slave output:
     *   Master always uses TX_RX (RX discards to internal buf when rxbuf=NULL).
     *   Slave mode is chosen per txbuf/rxbuf availability.
     */
    if (hspi->role == SPI_ROLE_SLAVE)
    {
        if ((txbuf != NULL) && (rxbuf != NULL))
        {
            mode = SPI_TX_RX_MOD;
        }
        else if (txbuf != NULL)
        {
            mode = SPI_TX_MOD;
        }
        else
        {
            mode = SPI_RX_MOD;
        }
        spi_set_transfermode(hspi->base_addr, mode);
        spi_set_slave_output(hspi->base_addr, (txbuf != NULL));
    }
    else
    {
        spi_set_transfermode(hspi->base_addr, SPI_TX_RX_MOD);
    }

    /* DMA TX uses uint32_t[nbytes] where the low byte is the SPI byte. */
    if (txbuf != NULL)
    {
        cache_force_write_back(tx_src, (sizeof(uint32_t) * nbytes));
    }

    /* Track progress for SPI_GET_TX_NBYTES / SPI_GET_RX_NBYTES. */
    hspi->tx_bytes_left = (txbuf != NULL) ? nbytes : 0U;
    hspi->rx_bytes_left = (rxbuf != NULL) ? nbytes : 0U;

    spi_ll_set_dma_thresholds(hspi->base_addr,
            (uint8_t)(SPI_FIFO_DEPTH - SPI_DMA_GET_BURST_ITEMS(nbytes)),
            (uint8_t)(SPI_DMA_GET_BURST_ITEMS(nbytes) - 1U));

    hspi->dma_tx_done = false;
    hspi->dma_rx_done = false;
    osal_semaphore_reset(hspi->sem);

    /*
     * Decide which DMA channels to start.
     *
     * Master: always both channels (RX discards when rxbuf is NULL).
     *
     * Slave RX-only (txbuf NULL, SPI_RX_MOD):
     *   The TRM requires at least one word in the TX FIFO before the master
     *   clocks. In SPI_RX_MOD the hardware shifts it as a constant MISO level
     *   and does not assert TXE. TX DMA is not started; dma_tx_done is
     *   pre-set so the shared callback fires on RX completion alone.
     *
     * Slave TX-only (rxbuf NULL, SPI_TX_MOD):
     *   Hardware discards incoming data; RX FIFO is not populated. RX DMA is
     *   not started; dma_rx_done is pre-set so the callback fires on TX
     *   completion alone.
     */
    start_tx_dma = true;
    start_rx_dma = true;

    if (hspi->role == SPI_ROLE_SLAVE)
    {
        if (txbuf == NULL)
        {
            WR_REG32((hspi->base_addr + SPI_DR0), 0U);
            hspi->dma_tx_done = true;
            start_tx_dma = false;
        }
        else if (rxbuf == NULL)
        {
            hspi->dma_rx_done = true;
            start_rx_dma = false;
        }
    }

    spi_setup_dma_xfer(hspi, tx_src, rx_dst, nbytes, &blk_cnt);

    INFO("SPI DMA transfer: using %lu LLI blocks", (unsigned long)blk_cnt);

    if (rxbuf != NULL)
    {
        cache_force_invalidate(rx_dst, nbytes);
    }

    if (start_rx_dma)
    {
        if (dma_setup_transfer(hspi->h_dma_rx,
                &rx_list[0U], blk_cnt,
                DMA_XFER_WIDTH1, DMA_XFER_WIDTH1) != 0)
        {
            ERROR("SPI RX DMA setup failed");
            spi_dma_close_channels((spi_handle_t)hspi);
            spi_ll_disable_dma(hspi->base_addr);
            spi_ll_set_dma_thresholds(hspi->base_addr, 0U, 0U);
            return -EIO;
        }
    }

    if (start_tx_dma)
    {
        if (dma_setup_transfer(hspi->h_dma_tx,
                &tx_list[0U], blk_cnt,
                DMA_XFER_WIDTH4, DMA_XFER_WIDTH4) != 0)
        {
            ERROR("SPI TX DMA setup failed");
            spi_dma_close_channels((spi_handle_t)hspi);
            spi_ll_disable_dma(hspi->base_addr);
            spi_ll_set_dma_thresholds(hspi->base_addr, 0U, 0U);
            return -EIO;
        }
    }

    if (hspi->role == SPI_ROLE_MASTER)
    {
        spi_select_chip(hspi->instance, hspi->slave_id);
    }

    if (start_rx_dma)
    {
        if (dma_start_transfer(hspi->h_dma_rx) != 0)
        {
            ERROR("SPI RX DMA start failed");
            if (hspi->role == SPI_ROLE_MASTER)
            {
                spi_select_chip(hspi->instance, 0U);
            }
            spi_dma_close_channels((spi_handle_t)hspi);
            spi_ll_disable_dma(hspi->base_addr);
            spi_ll_set_dma_thresholds(hspi->base_addr, 0U, 0U);
            return -EIO;
        }
    }

    if (start_tx_dma)
    {
        if (dma_start_transfer(hspi->h_dma_tx) != 0)
        {
            ERROR("SPI TX DMA start failed");
            if (hspi->role == SPI_ROLE_MASTER)
            {
                spi_select_chip(hspi->instance, 0U);
            }
            spi_dma_close_channels((spi_handle_t)hspi);
            spi_ll_disable_dma(hspi->base_addr);
            spi_ll_set_dma_thresholds(hspi->base_addr, 0U, 0U);
            return -EIO;
        }
    }

    return 0;
}

int32_t spi_xfer_sync(spi_handle_t const hspi,
        void *const txbuf, void *const rxbuf, uint16_t nbytes)
{
    bool ret;

    if ((spi_is_handle_valid(hspi) == false) || (nbytes == 0U)
            || ((hspi->role == SPI_ROLE_MASTER) && (txbuf == NULL))
            || ((txbuf == NULL) && (rxbuf == NULL)))
    {
        ERROR("Invalid SPI handle or buffer");
        return -EINVAL;
    }

    /* Caller is responsible for DMA buffer alignment. */

    if (osal_mutex_lock(hspi->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (hspi->is_open == false)
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
        hspi->is_tx_on = (txbuf != NULL);
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

    hspi->tx_buf = (uint8_t *)txbuf;
    hspi->tx_size = (txbuf != NULL) ? nbytes : 0U;
    hspi->rx_size = 0U;
    if (rxbuf != NULL)
    {
        hspi->rx_buf = (uint8_t *)rxbuf;
        hspi->rx_size = nbytes;
    }

    INFO("Starting SPI transfer in sync mode for %u bytes", nbytes);
    if (hspi->dma_enabled == false)
    {
        if (hspi->role == SPI_ROLE_MASTER)
        {
            spi_set_transfermode(hspi->base_addr, SPI_TX_RX_MOD);
        }
        spi_xfer(hspi, nbytes);
        ret = osal_semaphore_wait(hspi->sem, OSAL_TIMEOUT_WAIT_FOREVER);

        if (ret == true)
        {
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
        }
    }
    else
    {
        if ((hspi->h_dma_tx == NULL) || (hspi->h_dma_rx == NULL))
        {
            ERROR("SPI DMA not enabled");
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }
        if (nbytes > SPI_MAX_TRANSFER_SIZE)
        {
            ERROR("SPI DMA transfer size exceeds max");
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }
        if (spi_dma_transfer(hspi, txbuf, rxbuf, nbytes) != 0)
        {
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }

        ret = osal_semaphore_wait(hspi->sem, OSAL_TIMEOUT_WAIT_FOREVER);
        if (ret == false)
        {
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }
        if (rxbuf != NULL)
        {
            cache_force_invalidate(rxbuf, nbytes);
        }
        if (hspi->role == SPI_ROLE_MASTER)
        {
            spi_select_chip(hspi->instance, 0U);
        }
        hspi->tx_bytes_left = 0U;
        hspi->rx_bytes_left = 0U;
        hspi->is_tx_busy = false;
        hspi->is_rx_busy = false;
    }

    INFO("Completed SPI transfer in sync mode. "
            "Transmitted %u bytes, Received %u bytes",
            nbytes - hspi->tx_bytes_left,
            nbytes - hspi->rx_bytes_left);
    return 0;
}

int32_t spi_xfer_async(spi_handle_t const hspi,
        void *const txbuf, void *const rxbuf, uint16_t nbytes)
{

    if ((spi_is_handle_valid(hspi) == false) || (nbytes == 0U)
            || ((hspi->role == SPI_ROLE_MASTER) && (txbuf == NULL))
            || ((txbuf == NULL) && (rxbuf == NULL)))
    {
        ERROR("Invalid SPI handle or buffer");
        return -EINVAL;
    }

    /* Caller is responsible for DMA buffer alignment. */

    if (osal_mutex_lock(hspi->mutex, OSAL_TIMEOUT_WAIT_FOREVER))
    {
        if (hspi->is_open == false)
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
        hspi->is_tx_on = (txbuf != NULL);
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

    hspi->tx_buf = (uint8_t *)txbuf;
    hspi->tx_size = (txbuf != NULL) ? nbytes : 0U;
    hspi->rx_size = 0U;
    if (rxbuf != NULL)
    {
        hspi->rx_buf = (uint8_t *)rxbuf;
        hspi->rx_size = nbytes;
    }

    INFO("Starting SPI transfer in async mode for %u bytes", nbytes);
    if (hspi->dma_enabled == true)
    {
        if ((hspi->h_dma_tx == NULL) || (hspi->h_dma_rx == NULL))
        {
            ERROR("SPI DMA not configured");
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }
        if (nbytes > SPI_MAX_TRANSFER_SIZE)
        {
            ERROR("SPI DMA transfer size exceeds max");
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }
        if (spi_dma_transfer(hspi, txbuf, rxbuf, nbytes) != 0)
        {
            hspi->is_tx_busy = false;
            if (rxbuf != NULL)
            {
                hspi->is_rx_busy = false;
            }
            return -EIO;
        }
    }
    else
    {
        if (hspi->role == SPI_ROLE_MASTER)
        {
            spi_set_transfermode(hspi->base_addr, SPI_TX_RX_MOD);
        }
        spi_xfer(hspi, nbytes);
    }

    return 0;
}

int32_t spi_select_slave(spi_handle_t const hspi, uint32_t ss)
{
    if (spi_is_handle_valid(hspi) == false)
    {
        ERROR("Invalid SPI handle");
        return -EINVAL;
    }

    if (hspi->is_open == false)
    {
        ERROR("SPI instance not open");
        return -EINVAL;
    }

    if (hspi->role == SPI_ROLE_SLAVE)
    {
        ERROR("SPI slave select not supported in slave mode");
        return -ENOTSUP;
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
    if (spi_is_handle_valid(hspi) == false)
    {
        ERROR("Invalid SPI handle");
        return -EINVAL;
    }
    if (hspi->is_open == false)
    {
        ERROR("SPI instance not open");
        return -EINVAL;
    }

    if (hspi->dma_enabled == true)
    {
        if (spi_dma_deinit(hspi) != 0)
        {
            ERROR("SPI DMA deinit failed");
            return -EIO;
        }
    }

    osal_mutex_delete(hspi->mutex);
    osal_semaphore_delete(hspi->sem);
    hspi->is_open = false;
    spi_deinit(hspi->instance, hspi->role);

    return 0;
}

/**
 * @brief Interrupt handler for SPI
 */
void spi_isr(void *param)
{
    spi_handle_t hspi;
    uint32_t id;
    uint16_t rx_byte_count;
    uint16_t tx_byte_count;
    uint32_t new_thr;

    hspi = (spi_handle_t)param;
    if (hspi == NULL)
    {
        return;
    }

    id = spi_get_interrupt_status(hspi->base_addr);

    if ((id & SPI_RX_OVERFLOW_INT) != 0U)
    {
        spi_clear_rx_overflow(hspi->base_addr);


        /*
         * If no transfer is active the FIFO holds stale bytes from the
         * overflowing transaction.  Drain them so the next armed transfer
         * starts against a clean FIFO.
         */

        if (!hspi->is_rx_busy)
        {
            (void)spi_read_fifo(hspi->base_addr, NULL, SPI_FIFO_DEPTH);
        }

        if (hspi->callback_fn != NULL)
        {
            hspi->callback_fn(SPI_RX_OVERFLOW, hspi->cb_user_context);
        }
    }


    if ((id & SPI_RX_FULL_INT) != 0U)
    {
        if (hspi->rx_bytes_left > 0U)
        {
            if (hspi->is_rx_on == true)
            {
                rx_byte_count = spi_read_fifo(hspi->base_addr,
                        hspi->rx_buf, hspi->rx_bytes_left);
                hspi->rx_bytes_left -= rx_byte_count;
                hspi->rx_buf += rx_byte_count;
            }
            else
            {
                rx_byte_count = spi_read_fifo(hspi->base_addr, NULL,
                        hspi->rx_bytes_left);
                hspi->rx_bytes_left -= rx_byte_count;
            }

            if (hspi->role == SPI_ROLE_SLAVE)
            {
                if (hspi->rx_bytes_left > 0U)
                {
                    new_thr = (uint32_t)hspi->rx_bytes_left - 1U;

                    if (new_thr > (SPI_FIFO_DEPTH - 1U))
                    {
                        new_thr = SPI_FIFO_DEPTH - 1U;
                    }

                    if (new_thr < (uint32_t)hspi->rx_threshold)
                    {
                        hspi->rx_threshold = (uint8_t)new_thr;
                        spi_ll_set_rx_threshold(hspi->base_addr,
                                hspi->rx_threshold);
                    }
                }
            }
        }

        if (hspi->rx_bytes_left == 0U)
        {
            spi_disable_interrupt(hspi->base_addr, SPI_RX_FULL_INT);
            if (hspi->role == SPI_ROLE_MASTER)
            {
                spi_select_chip(hspi->instance, 0U);
            }

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
    }

    if ((id & SPI_TX_EMPTY_INT) != 0U)
    {
        if (hspi->tx_bytes_left > 0U)
        {
            if (hspi->is_tx_on == true)
            {
                tx_byte_count = spi_write_fifo(hspi->base_addr,
                        hspi->tx_buf, hspi->tx_bytes_left);
                hspi->tx_bytes_left -= tx_byte_count;
                hspi->tx_buf += tx_byte_count;
            }
            else
            {
                tx_byte_count = spi_write_fifo(hspi->base_addr, NULL,
                        hspi->tx_bytes_left);
                hspi->tx_bytes_left -= tx_byte_count;
            }
        }
        else
        {
            spi_disable_interrupt(hspi->base_addr, SPI_TX_EMPTY_INT);
        }
    }
}
