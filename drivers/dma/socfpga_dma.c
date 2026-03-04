/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for DMA
 */
#include <errno.h>
#include "socfpga_defines.h"
#include "socfpga_dma.h"
#include "socfpga_dma_reg.h"
#include "socfpga_cache.h"
#include "socfpga_interrupt.h"
#include "socfpga_rst_mngr.h"
#include "osal_log.h"


#define MULTI_BLK_LLI_MODE_ENABLED    1
#define DMA_MAX_INSTANCE              (2U)
#define MAX_CHANNEL_NUM               (4U)
#define MAX_LLI_PER_CHANNEL           128U
#define CH_SUSPEND_TIMEOUT_COUNT      (1000U)
/* Max block size available is 32767 */
#define MAX_BLOCK_SIZE    0x7FFFU

/* DMA channel registers */
struct dma_channel_reg_list
{
    uint64_t sar;
    uint64_t dar;
    uint64_t block_ts; /*Only 0-21 bits are used in this and other bits are reserved bits*/
    uint64_t llp; /*Only 5:63 bits are used; 0-5 bits are reserved*/
    uint64_t ctl;
    uint32_t chn_src_stat;
    uint32_t chn_dst_stat;
    uint64_t chn_llp_status;
    uint64_t reserved;
} __attribute__((packed, aligned(64)));

static uint32_t inst_base_addr[DMA_MAX_INSTANCE] =
{
    0x10DB0000U, 0x10DC0000U
};

static uint32_t chnl_offset_addr[MAX_CHANNEL_NUM] =
{
    0x00000100U, 0x00000200U, 0x00000300U, 0x00000400U
};

static socfpga_hpu_interrupt_t interrupt_id[DMA_MAX_INSTANCE][MAX_CHANNEL_NUM] =
{
    {
        DMA_IRQ0, DMA_IRQ1, DMA_IRQ2, DMA_IRQ3
    },
    {
        DMA1IRQ0, DMA1IRQ1, DMA1IRQ2, DMA1IRQ3
    }
};

struct dma_ch_cntxt
{
    BaseType_t is_open;
    uint32_t base_address;
    uint32_t ch_offset;
    uint32_t channel_num;
    dma_ch_state_t channel_state;
    socfpga_hpu_interrupt_t intr_id;
    /* dma_channel_reg_list descriptor base pointer */
    struct dma_channel_reg_list *linked_list_base;
    /* DMA transfer direction */
    dma_xfer_type_t direction;
    /* DMA channel config register  */
    uint64_t config;
    /* Interrupt config  */
    uint64_t interrupt_en;
    /* Callback function for interrupts */
    dma_callback_t xp_dma_callback;
};

static struct dma_ch_cntxt hdma_default[DMA_MAX_INSTANCE][MAX_CHANNEL_NUM];

/* Allocate memory for storing the descriptors */
static struct dma_channel_reg_list plinked_list_chain[DMA_MAX_INSTANCE]
[MAX_CHANNEL_NUM * MAX_LLI_PER_CHANNEL] __attribute__ ((aligned (64)));

void pdma_irq_handler(void *data);

dma_handle_t dma_open(uint32_t instance, uint32_t ch)
{
    int32_t status;
    uint8_t reset_status;
    dma_handle_t phandle = NULL;
    socfpga_interrupt_err_t int_ret;
    if ((instance >= DMA_MAX_INSTANCE) || (ch >= MAX_CHANNEL_NUM))
    {
        ERROR("Not a valid DMAC Instance or Channel");
        return NULL;
    }
    phandle = &hdma_default[instance][ch];
    if (phandle->is_open == 1)
    {
        ERROR("DMAC channel already opened please close it before re-opening");
        return NULL;
    }
    if (phandle->channel_state != DMA_CH_IDLE)
    {
        ERROR("DMAC Channel instance is in use");
        return NULL;
    }

    status = rstmgr_get_reset_status(RST_DMA, &reset_status);
    if (status != 0)
    {
        ERROR("DMAC block get reset status failed. ");
        return NULL;
    }
    if (reset_status == 1U)
    {
        status = rstmgr_toggle_reset(RST_DMA);
        if (status != 0)
        {
            ERROR("Failed to reset release DMAC block. ");
            return NULL;
        }
    }
    phandle = &hdma_default[instance][ch];
    phandle->base_address = inst_base_addr[instance];
    phandle->ch_offset = inst_base_addr[instance] + chnl_offset_addr[ch];
    phandle->intr_id = interrupt_id[instance][ch];
    phandle->channel_num = ch;
    phandle->linked_list_base = &plinked_list_chain[instance][(ch *
                    MAX_LLI_PER_CHANNEL)];
    phandle->is_open = 1;
    /*Setup and enable interrupts in GIC*/
    int_ret = interrupt_register_isr(phandle->intr_id, pdma_irq_handler, phandle);
    if (int_ret != ERR_OK)
    {
        return NULL;
    }
    int_ret = interrupt_enable(phandle->intr_id, GIC_INTERRUPT_PRIORITY_DMA);
    if (int_ret != ERR_OK)
    {
        return NULL;
    }
    return phandle;
}

/**
 * @brief Get the DMA burst length
 */
static void dma_get_burst_len(dma_handle_t const hdma, dma_burst_len_t *src_burst_len,
        dma_burst_len_t *dst_burst_len)
{

    if (hdma->direction == DMA_MEM_TO_MEM_DMAC)
    {
        *src_burst_len = DMA_BURST_LEN_16;
        *dst_burst_len = DMA_BURST_LEN_16;
    }
    else
    {
        *src_burst_len = DMA_BURST_LEN_4;
        *dst_burst_len = DMA_BURST_LEN_4;
    }
}

int32_t dma_config(dma_handle_t const hdma, dma_config_t *pcfg)
{

    if (hdma == NULL)
    {
        ERROR("DMAC handle cannot be NULL ");
        return -EINVAL;
    }
    if (hdma->is_open != 1)
    {
        ERROR("DMAC channel should be opened before config \n");
        return -EFAULT;
    }
    if (hdma->channel_state != DMA_CH_IDLE)
    {
        ERROR("DMAC Channel is in active state");
        return -EBUSY;
    }

    hdma->config = 0UL;
    hdma->interrupt_en = 0UL;

    hdma->direction = pcfg->ch_dir;
    hdma->config |= ((uint64_t)(pcfg->ch_dir) << DMA_CH_CFG2_TT_FC_POS);
    if (pcfg->ch_dir == DMA_MEM_TO_PERI_DMAC)
    {
        hdma->config |= ((uint64_t)pcfg->peri_id << DMA_CH_CFG2_DST_PER_POS);
    }
    if (pcfg->ch_dir == DMA_PERI_TO_MEM_DMAC)
    {
        hdma->config |= ((uint64_t)pcfg->peri_id << DMA_CH_CFG2_SRC_PER_POS);
    }
    hdma->config &= ~((DMA_CH_CFG2_DST_MULTBLK_TYPE_MASK |
            DMA_CH_CFG2_SRC_MULTBLK_TYPE_MASK));
#ifdef MULTI_BLK_LLI_MODE_ENABLED
    /* Data available in multiple blocks of memory */
    hdma->config |= (((uint32_t)DMA_MULTI_BLK_LL << DMA_CH_CFG2_DST_MULTBLK_TYPE_POS) |
            ((uint32_t)DMA_MULTI_BLK_LL << DMA_CH_CFG2_SRC_MULTBLK_TYPE_POS));
#else
    /* Data available in a single contiguous block memory */
    hdma->config |= ((DMA_MULTI_BLK_CONTIGUOUS << DMA_CH_CFG2_DST_MULTBLK_TYPE_POS) |
            (DMA_MULTI_BLK_CONTIGUOUS << DMA_CH_CFG2_SRC_MULTBLK_TYPE_POS));
#endif
    hdma->xp_dma_callback = pcfg->callback;
    return 0;
}

int32_t dma_setup_transfer(dma_handle_t const hdma, dma_xfer_cfg_t *xfer_list, uint32_t num_xfers,
                           dma_xfer_width_t src_width, dma_xfer_width_t dst_width)
{
    uint64_t val;
    uint64_t transfer_size;
    uint32_t i;
    dma_burst_len_t src_burst_len, dst_burst_len;
    struct dma_channel_reg_list *plinked_list;
    dma_xfer_cfg_t *ptransfer_cfg;
    if (hdma == NULL)
    {
        ERROR("DMAC handle cannot be NULL ");
        return -EINVAL;
    }
    if (num_xfers > MAX_LLI_PER_CHANNEL)
    {
        ERROR("Number of transfers exceeds current limit of %d", MAX_LLI_PER_CHANNEL);
        return -EINVAL;
    }
    if (xfer_list == NULL)
    {
        ERROR("Transfer list cannot be null");
        return -EFAULT;
    }
    if (hdma->is_open != 1)
    {
        ERROR("DMAC channel should be opened before setup transfer \n");
        return -EIO;
    }
    if (hdma->channel_state != DMA_CH_IDLE)
    {
        ERROR("DMAC Channel is in active state ");
        return -EBUSY;
    }
    (void)memset(hdma->linked_list_base, 0, (sizeof(struct
            dma_channel_reg_list) * MAX_LLI_PER_CHANNEL));
    plinked_list = hdma->linked_list_base;
    if (plinked_list == NULL)
    {
        ERROR("Linked list is null");
        return -EFAULT;
    }
    ptransfer_cfg = xfer_list;
    if (ptransfer_cfg == NULL)
    {
        ERROR("Transfer Cfg is NULL");
        return -EFAULT;
    }

    for (i = 0U; i < num_xfers; i++)
    {
        dma_get_burst_len(hdma, &src_burst_len, &dst_burst_len);
        transfer_size = (((uint64_t)src_width << DMA_CH_CTL_SRC_TR_WIDTH_POS) |
                ((uint64_t)dst_width << DMA_CH_CTL_DST_TR_WIDTH_POS));
        transfer_size |=
                (((uint64_t)src_burst_len <<
                DMA_CH_CTL_SRC_MSIZE_POS) | ((uint64_t)dst_burst_len << DMA_CH_CTL_DST_MSIZE_POS));
        transfer_size |= ((DMA_CH_CTL_DST_STAT_EN_MASK |
                DMA_CH_CTL_SRC_STAT_EN_MASK) | DMA_CH_CTL_IOC_BLKTFR_MASK);

        if (((1UL << (uint64_t)src_width) == 0U) || (ptransfer_cfg == NULL))
        {
            ERROR("Transfer width cannot be 0");
            return -EFAULT;
        }

        plinked_list->sar = ptransfer_cfg->src;
        plinked_list->dar = ptransfer_cfg->dst;
        plinked_list->ctl = transfer_size;
        plinked_list->block_ts =
                ((uint64_t)ptransfer_cfg->blk_size / (1UL << (uint64_t)src_width)) - 1UL;
        if (plinked_list->block_ts > MAX_BLOCK_SIZE)
        {
            ERROR("Transfer block size exceeding maximum size");
            return -EINVAL;
        }

        /*Set next descriptor address*/
        plinked_list->llp = ((uint64_t)(uintptr_t)(plinked_list +
                (uintptr_t)1U));

#ifdef MULTI_BLK_LLI_MODE_ENABLED
        plinked_list->ctl |= (1UL << DMA_CH_CTL_SHADOWREG_OR_LLI_VALID_POS);
        /*Last descriptor*/
        if ((num_xfers) == (i + 1U))
        {
            plinked_list->ctl |= (1UL << DMA_CH_CTL_SHADOWREG_OR_LLI_LAST_POS);
            plinked_list->llp = 0UL;
        }
#endif

        /* Next descriptor updates*/
        plinked_list++;
        ptransfer_cfg = ptransfer_cfg->next_trnsfr_cfg;
    }

    cache_force_write_back((void *)hdma->linked_list_base,
            ((MAX_LLI_PER_CHANNEL)*sizeof(plinked_list[0U])));

    hdma->interrupt_en = TFR_DONE_MASK;
    val = RD_REG64(hdma->base_address + DMA_DMAC_CFGREG);
    val |= (DMA_DMAC_CFGREG_INT_EN_MASK | DMA_DMAC_CFGREG_DMAC_EN_MASK);
    WR_REG64(hdma->base_address + DMA_DMAC_CFGREG, val);
    (void)RD_REG64(hdma->ch_offset + DMA_CH_CFG2);
    WR_REG64((hdma->ch_offset + DMA_CH_CFG2), hdma->config);
    WR_REG64(hdma->ch_offset + DMA_CH_INTSTATUS_ENABLEREG, hdma->interrupt_en);
    WR_REG64(hdma->ch_offset + DMA_CH_INTSIGNAL_ENABLEREG, hdma->interrupt_en);
    plinked_list = hdma->linked_list_base;
    if (plinked_list == NULL)
    {
        ERROR("Linked list is null");
        return -EFAULT;
    }
#ifdef MULTI_BLK_LLI_MODE_ENABLED
    (void)RD_REG64(hdma->ch_offset + DMA_CH_LLP);
    WR_REG64(hdma->ch_offset + DMA_CH_LLP, ((uint64_t)plinked_list));
    (void)RD_REG64(hdma->ch_offset + DMA_CH_LLP);
#else
    WR_REG64((hdma->ch_offset + DMA_CH_SAR), plinked_list->sar);
    WR_REG64((hdma->ch_offset + DMA_CH_DAR), plinked_list->dar);
    WR_REG64((hdma->ch_offset + DMA_CH_BLOCK_TS), (plinked_list->block_ts &
            DMA_CH_BLOCK_TS_BLOCK_TS_MASK));
    WR_REG64((hdma->ch_offset + DMA_CH_CTL), plinked_list->ctl);
#endif
    return 0;
}
int32_t dma_start_transfer(dma_handle_t const hdma)
{
    uint64_t val;
    if (hdma == NULL)
    {
        ERROR("DMAC handle cannot be NULL ");
        return -EINVAL;
    }
    if (hdma->is_open != 1)
    {
        ERROR("DMAC channel should be opened before start transfer \n");
        return -EIO;
    }

    if (hdma->channel_state != DMA_CH_IDLE)
    {
        ERROR("DMAC Channel is in active state ");
        return -EBUSY;
    }
    INFO("Starting the DMA transfer on channel %d", hdma->channel_num);
    val = RD_REG64(hdma->base_address + DMA_DMAC_CHENREG);
    val |= (1UL << (hdma->channel_num + CHENREG_CH_EN_POS));
    val |= (1UL << (hdma->channel_num + CHENREG_CH_EN_WE_POS));
    /*Start the channel for transfer */
    WR_REG64(hdma->base_address + DMA_DMAC_CHENREG, val);
    /*Set the channel state to active as the transfer is started */
    hdma->channel_state = DMA_CH_ACTIVE;
    return 0;
}
int32_t dma_stop_transfer(dma_handle_t const hdma)
{
    uint64_t val;
    uint32_t wait_count = 0U;

    if (hdma == NULL)
    {
        ERROR("DMAC handle cannot be NULL ");
        return -EINVAL;
    }
    if (hdma->channel_state == DMA_CH_IDLE)
    {
        ERROR("DMAC Channel not in active state ");
        return -EIO;
    }

    /* suspend the transfer */
    val = RD_REG64(hdma->base_address + DMA_DMAC_CHENREG);
    val |= (0x1UL << (hdma->channel_num + CHENREG_CH_SUSP_WE_POS)) |
            (0x1UL << (hdma->channel_num + CHENREG_CH_SUSP_POS));
    WR_REG64(hdma->base_address + DMA_DMAC_CHENREG, val);

    /* Disable the channel */
    val = RD_REG64(hdma->base_address + DMA_DMAC_CHENREG);
    val &= (~(1UL << hdma->channel_num));
    val |= (1UL << (hdma->channel_num + CHENREG_CH_EN_WE_POS));
    WR_REG64(hdma->base_address + DMA_DMAC_CHENREG, val);
    val = RD_REG64(hdma->base_address + DMA_DMAC_CHENREG);
    while (((val & (1UL << hdma->channel_num)) == (1UL << hdma->channel_num)) &&
            (wait_count < 100U))
    {
        val = RD_REG64(hdma->base_address + DMA_DMAC_CHENREG);
        wait_count++;
    }
    if (wait_count == 100U)
    {
        return -EIO;
    }
    /* Set the channel state to idle as the transfer is stopped */
    hdma->channel_state = DMA_CH_IDLE;
    return 0;
}


int32_t dma_close(dma_handle_t const hdma)
{
    if (hdma == NULL)
    {
        ERROR("DMAC handle cannot be NULL ");
        return -EINVAL;
    }
    (void)memset(hdma, 0, sizeof(struct dma_ch_cntxt));
    return 0;
}

/**
 * @brief Interrupt handler for DMA
 */
void pdma_irq_handler(void *data)
{
    uint64_t val;
    dma_handle_t phandle = (dma_handle_t)data;
    val = RD_REG64(phandle->ch_offset + DMA_CH_INTSTATUS);
    if ((val & TFR_DONE_MASK) == TFR_DONE_MASK)
    {
        WR_REG64((phandle->ch_offset + DMA_CH_INTCLEARREG), TFR_DONE_MASK);
        /* Set the channel state to idle once transfer completed*/
        phandle->channel_state = DMA_CH_IDLE;
        phandle->xp_dma_callback(phandle);
    }
}
