/*
 * FreeRTOS+TCP V3.1.0
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 *
 * HAL driver implementation for XGMAC. Modified for SoC FPGA
 */

/*
 * This is the driver implementation for the XGMAC Ethernet controller. The
 * application normally does not use the driver directly. It is normally used
 * along with a TCP/IP stack. The below block diagram shows how this driver is
 * integrated with the stack.
 *
 *          +----------------------------+
 *          |        Application         |
 *          +----------------------------+
 *                        |
 *          +----------------------------+
 *          |   FreeRTOS-Plus-TCP Stack  |
 *          +----------------------------+
 *                        |
 *          +----------------------------+
 *          |   Network Interface Layer  |
 *          +----------------------------+
 *                        |
 *         +-------------------------------+
 *         | Ethernet Driver + PHY Driver  |
 *         |                               |
 *         |  +-----------+ +-----------+  |
 *         |  | Ethernet  | | DMA buf   |  |
 *         |  | MAC layer | | Handling  |  |
 *         |  +-----------+ +-----------+  |
 *         |                               |
 *         |    +--------------------+     |
 *         |    | Ethernet PHY Layer |     |
 *         |    +--------------------+     |
 *         +-------------------------------+
 *
 * The interface layer provides the functions required by the stack and
 * translates them to driver calls.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "socfpga_defines.h"
#include "socfpga_xgmac.h"
#include "socfpga_xgmac_phy_ll.h"
#include "socfpga_xgmac_ll.h"
#include "socfpga_interrupt.h"
#include "socfpga_xgmac_configs.h"
#include "osal.h"
#include "osal_log.h"

#define XGMAC_EMAC_STARTED     0x1
#define XGMAC_EMAC_STOPPED     0x0
#define XGMAC_CSUM_BY_HW       1U
#define XGMAC_DMA_DESC_SIZE    16UL

#define SOCFGPA_CACHE_LINE_WIDTH    64U

struct xgmac_desc_t
{
    xgmac_base_addr_t xgmac_inst_base_addr;       /*!< XGMAC instance base address */
    xgmac_base_addr_t xgmac_inst_dma_base_addr;    /*!< XGMAC DMA base address */
    int32_t instance;                           /*!< Instance number */
    uint8_t csum_mode;                      /*!< Checksum mode */
    uint8_t phy_type;                           /*!< PHY type */

    xgmac_buf_desc_t *tx_bd_ring;        /*!< Transmit buffer descriptor ring */
    xgmac_buf_desc_t *rx_bd_ring;        /*!< Receive buffer descriptor ring */

    uint8_t *ptx_dma_buf1_ap[XGMAC_NUM_TX_DESC]; /*!< Transmit DMA buffer pointers */
    uint8_t *prx_dma_buf1_ap[XGMAC_NUM_RX_DESC]; /*!< Receive DMA buffer pointers */

    /*!< Aligned transmit buffer descriptors */
    xgmac_buf_desc_t axBufferDescTx[XGMAC_NUM_TX_DESC] __attribute__((aligned(XGMAC_DMA_ALIGN_BYTES)));
    /*!< Aligned receive buffer descriptors */
    xgmac_buf_desc_t axBufferDescRx[XGMAC_NUM_RX_DESC] __attribute__((aligned(XGMAC_DMA_ALIGN_BYTES)));

    volatile int32_t tx_desc_head;                /*!< Transmit descriptor head index */
    volatile int32_t tx_desc_tail;                /*!< Transmit descriptor tail index */
    volatile int32_t rx_desc_head;                /*!< Receive descriptor head index */
    volatile int32_t rx_desc_tail;                /*!< Receive descriptor tail index */

    xgmac_callback_t callback;           /*!< Callback function */
    void *pcntxt;                          /*!< User context pointer */
    xgmac_err_info_t err_info;          /*!< Interrupt error data */

    int8_t is_initialized;                        /*!< Indicates if device handle is initialized */
    int8_t is_started;                            /*!< Indicates if device is started */
    int8_t is_ready;                              /*!< Indicates if device is ready */

    osal_semaphore_t tx_sem;           /*!< Transmit synchronization semaphore */
    osal_semaphore_t tx_mutex;           /*!< Transmit synchronization mutex */

};

static struct xgmac_desc_t *xgmac_descriptors = NULL;

/* Static functions */
static Basetype_t dma_soft_reset(xgmac_base_addr_t emac_base_addr);
static Basetype_t dma_set_descriptors(xgmac_handle_t hxgmac);
static void dma_channel_init(xgmac_handle_t hxgmac, const
        xgmac_dev_config_str_t *xgmac_dev_config);
void dma_setup_tx_descriptor_list(xgmac_handle_t hxgmac);
void dma_setup_rx_descriptor_list(xgmac_handle_t hxgmac);
static Basetype_t dma_enable_interrupts(xgmac_handle_t hxgmac, const
        xgmac_dev_config_str_t *xgmac_dev_config);
static Basetype_t dma_register_isr(xgmac_handle_t hxgmac);
void socfpga_xgmac_dma_isr(void *param);

static socfpga_hpu_interrupt_t get_emac_intr_id(int32_t instance)
{
    socfpga_hpu_interrupt_t intr_id;

    switch (instance)
    {
        case 0:
            intr_id = EMAC0IRQ;
            break;
        case 1:
            intr_id = EMAC1IRQ;
            break;
        case 2:
            intr_id = EMAC2IRQ;
            break;
        default:
            intr_id = MAX_SPI_HPU_INTERRUPT;
            break;
    }
    return intr_id;
}

xgmac_handle_t xgmac_emac_init(xgmac_config_t *cfg)
{
    int32_t xgmac_instance = cfg->instance;
    xgmac_handle_t hxgmac;

    if (xgmac_descriptors == NULL)
    {
        xgmac_descriptors = (struct xgmac_desc_t *)pvPortMallocCoherent(
                sizeof(struct xgmac_desc_t) * XGMAC_MAX_INSTANCE);
        if (xgmac_descriptors == NULL)
        {
            return NULL;
        }

    }
    memset(xgmac_descriptors, 0, sizeof(struct xgmac_desc_t) * XGMAC_MAX_INSTANCE);

    /* Check if Handle is valid */
    if ((xgmac_instance >= XGMAC_MAX_INSTANCE) || (xgmac_instance < 0))
    {
        return NULL;
    }

    hxgmac = (xgmac_handle_t)&(xgmac_descriptors[xgmac_instance]);
    if (hxgmac == NULL)
    {
        return NULL;
    }

    /* Check if Handle is already initialized */
    if (hxgmac->is_initialized == TRUE)
    {
        return NULL;
    }

    /* Initialize to all 0's */
    (void)memset(hxgmac, 0, sizeof(struct xgmac_desc_t));
    hxgmac->instance = xgmac_instance;

    /* Update the EMAC Instance Core Base Address */
    hxgmac->xgmac_inst_base_addr = XGMAC_GET_BASE_ADDRESS(xgmac_instance);

    /* Update the EMAC Instance DMA Base Address */
    hxgmac->xgmac_inst_dma_base_addr = XGMAC_GET_DMA_BASE_ADDRESS(
            xgmac_instance);

    /* Initialize the XGMAC Instance Parameters */
    hxgmac->phy_type = cfg->phy_type;

    /* Enable Hardware checksum */
    hxgmac->csum_mode = XGMAC_CSUM_BY_HW;

    /* Initialize Tx and Rx BD Ring to zero */
    hxgmac->tx_bd_ring = NULL;
    hxgmac->rx_bd_ring = NULL;

    /* Initialize TransmitSemaphore and TransmitMutex to NULL */
    hxgmac->tx_sem = NULL;
    hxgmac->tx_mutex = NULL;

    hxgmac->is_initialized = TRUE;

    /* Return the initialized handle */
    return hxgmac;
}

int32_t xgmac_emac_start(xgmac_handle_t hxgmac)
{
    int32_t xgmac_instance = hxgmac->instance;
    xgmac_base_addr_t emac_base_addr;
    xgmac_base_addr_t mtl_base_addr;

    uint8_t bytes[24] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, (uint8_t)0x45, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 128, 17, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
    };


    emac_base_addr = hxgmac->xgmac_inst_base_addr;
    mtl_base_addr = XGMAC_GET_MTL_BASE_ADDRESS(
            xgmac_instance);
    if ((emac_base_addr == 0U) || (mtl_base_addr == 0U))
    {
        return -EINVAL;
    }

    /* Program MAC register configurations */
    xgmac_mac_init(emac_base_addr, &xgmac_dev_config_str);

    /* Program MTL configuration registers for Tx and Rx */
    xgmac_mtl_init(mtl_base_addr, &xgmac_dev_config_str);

    /* Setup the MAC Address in MAC High and MAC Low Registers */
    xgmac_set_macaddress(emac_base_addr, (void *)bytes, MAC_ADRRESS_INDEX1);

    if (emac_base_addr == 0U)
    {
        return -EINVAL;
    }
    /* Start the MAC Tx and Rx */
    xgmac_dev_start(emac_base_addr);

    hxgmac->is_started = XGMAC_EMAC_STARTED;

    return 0;
}

int32_t xgmac_dma_initialize(xgmac_handle_t hxgmac)
{
    xgmac_base_addr_t emac_base_addr;
    xgmac_base_addr_t dma_base_addr;
    Basetype_t ret_val;

    emac_base_addr = hxgmac->xgmac_inst_base_addr;
    dma_base_addr = hxgmac->xgmac_inst_dma_base_addr;
    if ((emac_base_addr == 0U) || (dma_base_addr == 0U))
    {
        return -EINVAL;
    }

    /* Wait for completion of software reset */
    if (dma_soft_reset(emac_base_addr) != true)
    {
        ERROR("SOCFPGA_XGMAC: XGMAC DMA Reset Failed.");
        return -EIO;
    }

    /* Program fields of DMA_SysBus_Mode Register */
    xgmac_dma_init(emac_base_addr, &xgmac_dev_config_str);

    /* Set DMA Tx/Rx Descriptors */
    ret_val = dma_set_descriptors(hxgmac);
    if (ret_val != true)
    {
        ERROR("SOCFPGA_XGMAC: XGMAC DMA Set Descriptors Failed.");
        return -EIO;
    }


    /* Initialize DMA channel */
    dma_channel_init(hxgmac, &xgmac_dev_config_str);

    /* Start Receive and Transmit DMA */
    xgmac_start_dma_dev(dma_base_addr, &xgmac_dev_config_str);

    /* Program  DMA Interrupt Enable Register */
    ret_val = dma_enable_interrupts(hxgmac, &xgmac_dev_config_str);
    if (ret_val != true)
    {
        ERROR("SOCFPGA_XGMAC: XGMAC DMA Enable Interrupt Failed.");
        return -EIO;
    }

    return 0;
}

int32_t xgmac_get_inst_base_addr(xgmac_handle_t hxgmac)
{
    if (hxgmac == NULL)
    {
        return 0;
    }
    return hxgmac->xgmac_inst_base_addr;
}

xgmac_err_info_t *xgmac_get_err_info(xgmac_handle_t hxgmac)
{
    if (hxgmac == NULL)
    {
        return NULL;
    }
    return &(hxgmac->err_info);
}

int32_t xgmac_dma_transmit(xgmac_handle_t hxgmac, xgmac_tx_buf_t *dma_tx_buf)
{
    TickType_t block_time_ticks = pdMS_TO_TICKS(5000U);
    xgmac_buf_desc_t *pdma_tx_desc;
    int32_t head_indx = hxgmac->tx_desc_head;
    uint32_t data_length;
    uint32_t last_tx_desc;
    xgmac_base_addr_t dma_base_addr;
    uint8_t ckecksum_insertion;
    uint32_t low_addr, high_addr;
    uintptr_t buffer_addr;
    int32_t ret_status = 0;

    dma_base_addr = hxgmac->xgmac_inst_dma_base_addr;

    /* Do while once and break in case of error */
    do
    {
        if (hxgmac->tx_sem == NULL)
        {
            ret_status = -EIO;
            break;
        }

        if (osal_semaphore_wait(hxgmac->tx_sem, block_time_ticks) != pdPASS)
        {
            ERROR("xgmac_dma_transmit: Time-out TX buffer not available.");
            ret_status = -EIO;
            break;
        }

        if (osal_semaphore_wait(hxgmac->tx_mutex, block_time_ticks) != pdFAIL)
        {

            pdma_tx_desc = &(hxgmac->tx_bd_ring[head_indx]);
            if (pdma_tx_desc == NULL)
            {
                return -EINVAL;
            }

            /* Assign NW Buffer address to Desc0 address */
            buffer_addr = (uintptr_t)dma_tx_buf->buf;
            low_addr = (uint32_t)buffer_addr;
            high_addr = (uint32_t)(buffer_addr >> 32);

            /* The Application will set this flag if it wants to release buffers after Tx done.
             * Hence copy the buffer address later to be used in TX Done function */
            if (dma_tx_buf->release_buf != 0U)
            {
                hxgmac->ptx_dma_buf1_ap[head_indx] = (uint8_t *)buffer_addr;
            }
            else
            {
                hxgmac->ptx_dma_buf1_ap[head_indx] = NULL;
            }

            /* Assign BufferAP address to Desc0 and Desc1  */
            pdma_tx_desc->des0 = low_addr;
            pdma_tx_desc->des1 = high_addr;

            data_length = dma_tx_buf->size;
            xgmac_flush_buffer((void *)(uintptr_t)buffer_addr, data_length);

            /* Set Buffer-1 Length */
            pdma_tx_desc->des2 = (data_length & TDES2_NORM_RD_HL_B1L_MASL);

            /* Enable Interrupt On Completion */
            pdma_tx_desc->des2 |= TDES2_NORM_RD_IOC_MASK;

            /* Prepare transmit descriptors to give to DMA. */

            /* Set the IPv4 checksum */
            ckecksum_insertion = hxgmac->csum_mode;
            if (ckecksum_insertion == XGMAC_CSUM_BY_HW)
            {
                pdma_tx_desc->des3 |= TDES3_NORM_RD_CIC_TPL_MASK;
            }

            /* Set first and last descriptor mask */
            /* Set Own bit of the Tx descriptor to give the buffer back to DMA */
            pdma_tx_desc->des3 |= TDES3_NORM_RD_FD_MASK |
                    TDES3_NORM_RD_LD_MASK | TDES3_NORM_RD_OWN_MASK;

            /* Issue synchronization barrier instruction */
            __asm volatile ("DSB SY");

            /* Point to next descriptor */
            head_indx++;
            if (head_indx == XGMAC_NUM_TX_DESC)
            {
                head_indx = 0;
            }

            /* Update the TX-head index */
            hxgmac->tx_desc_head = head_indx;

            /* Program the  Tx Tail Pointer Register */
            last_tx_desc = (uint32_t)(uintptr_t)&(hxgmac->tx_bd_ring[head_indx]);
            WR_DMA_CHNL_REG32(dma_base_addr, XGMAC_DMA_CH0, XGMAC_DMA_CH_TXDESC_TAIL_LPOINTER,
                    last_tx_desc);

            /* Release the Mutex. */
            if (osal_semaphore_post(hxgmac->tx_mutex) == false)
            {
                ret_status = -EIO;
                break;
            }
        }
        else
        {
            ret_status = -EIO;
            break;
        }
    } while(false);

    return ret_status;
}

int32_t xgmac_dma_receive(xgmac_handle_t hxgmac, xgmac_rx_buf_t *dma_rx_buf)
{
    xgmac_buf_desc_t *pdma_rx_desc;
    int32_t head_indx = hxgmac->rx_desc_head;
    uint8_t *pethernet_buffer;
    BaseType_t received_packet_length;
    BaseType_t dma_inv_length;
    int32_t ret_status;

    ret_status = 0;

    pdma_rx_desc = &(hxgmac->rx_bd_ring[head_indx]);
    if (pdma_rx_desc == NULL)
    {
        return -EINVAL;
    }

    if ((pdma_rx_desc->des3 & XGMAC_RDES3_OWN) == 0u)
    {
        /* Parse the buffer address from RxBufAP Array  */
        pethernet_buffer = hxgmac->prx_dma_buf1_ap[head_indx];
        if (pethernet_buffer == NULL)
        {
            return -EINVAL;
        }
        received_packet_length = pdma_rx_desc->des3 & RDES3_NORM_WR_PL_MASK;
        if(received_packet_length & 0x3f )
        {
            dma_inv_length = ((received_packet_length + (SOCFGPA_CACHE_LINE_WIDTH)) & ~(SOCFGPA_CACHE_LINE_WIDTH - 1U));
            xgmac_invalidate_buffer((void *)(uintptr_t)pethernet_buffer, dma_inv_length);
        }
        else
        {
            xgmac_invalidate_buffer((void *)(uintptr_t)pethernet_buffer, received_packet_length);
        }

        dma_rx_buf->buf = pethernet_buffer;
        dma_rx_buf->size = received_packet_length;
        dma_rx_buf->packet_status = pdma_rx_desc->des3;

    }
    else
    {
        ret_status = -EAGAIN;
    }

    return ret_status;
}

int32_t xgmac_refill_rx_descriptor(xgmac_handle_t hxgmac, uint8_t *buf)
{
    xgmac_buf_desc_t *pdma_rx_desc;
    int32_t head_indx = hxgmac->rx_desc_head;
    xgmac_base_addr_t dma_base_addr;
    uint32_t last_rx_desc;

    dma_base_addr = hxgmac->xgmac_inst_dma_base_addr;
    pdma_rx_desc = &(hxgmac->rx_bd_ring[head_indx]);
    if ((dma_base_addr == 0U) || (pdma_rx_desc == NULL))
    {
        return -1;
    }
    if (buf != NULL)
    {
        pdma_rx_desc->des0 = (uint32_t)(uintptr_t)buf;
        pdma_rx_desc->des1 = (uint32_t)((uintptr_t)buf >> 32);

        hxgmac->prx_dma_buf1_ap[head_indx] = (uint8_t *)buf;
    }
    /*
     * There is a possibility that the buffer is cached in L1 but not in L4.
     * Since the peripherals use L4 cache, it could see stale data
     */
     xgmac_flush_buffer(buf, XGMAC_MAX_PACKET_SIZE);

    /* Release descriptors to DMA */
    pdma_rx_desc->des2 = 0;
    pdma_rx_desc->des3 = 0;

    /* Set Own bit and IOC bit of of the Rx descriptor Status */
    pdma_rx_desc->des3 = XGMAC_RDES3_OWN | XGMAC_RDES3_IOC;

    head_indx = (head_indx + 1) % XGMAC_NUM_RX_DESC;

    hxgmac->rx_desc_head = head_indx;

    /* Update the tail pointer register */
    /*
     * Tail pointer should be always ahead of current pointer
     * Setting the tail to recently processed descriptor gives better chance
     * to avoid current catching up to tail (since it is a ring buffer)
     */
    last_rx_desc = (uint32_t)(uintptr_t)pdma_rx_desc;
    WR_DMA_CHNL_REG32(dma_base_addr, XGMAC_DMA_CH0, XGMAC_DMA_CH_RXDESC_TAIL_LPOINTER,
            last_rx_desc);

    return 0;
}

int32_t xgmac_set_callback(xgmac_handle_t hxgmac, xgmac_callback_t callback, void *pcntxt)
{
    if (hxgmac == NULL)
    {
        return -EINVAL;
    }

    if (hxgmac->is_started != 0)
    {
        return -EBUSY;
    }

    hxgmac->callback = callback;
    hxgmac->pcntxt = pcntxt;

    return 0;
}

int32_t xgmac_dma_tx_done(xgmac_handle_t hxgmac, uint8_t **release_buffer)
{
    int tail_indx = hxgmac->tx_desc_tail;
    int head_indx = hxgmac->tx_desc_head;
    TickType_t block_time_ticks = pdMS_TO_TICKS(5000U);
    int32_t ret_status;

    ret_status = 0;

    xgmac_buf_desc_t *pdma_tx_desc;

    size_t ux_count = ((UBaseType_t)XGMAC_NUM_TX_DESC) -
            uxSemaphoreGetCount(hxgmac->tx_sem);

    if (osal_semaphore_wait(hxgmac->tx_mutex, block_time_ticks) != pdFAIL)
    {
        pdma_tx_desc = &(hxgmac->tx_bd_ring[tail_indx]);
        if (pdma_tx_desc == NULL)
        {
            return -EINVAL;
        }

        if (ux_count > 0UL)
        {
            if ((tail_indx == head_indx) && (ux_count !=
                    (uint32_t)XGMAC_NUM_TX_DESC))
            {
                return -EINVAL;
            }

            *release_buffer = (uint8_t *)hxgmac->ptx_dma_buf1_ap[tail_indx];

            /* Reset all descriptor values */
            pdma_tx_desc->des0 = 0;
            pdma_tx_desc->des1 = 0;
            pdma_tx_desc->des2 = 0;
            pdma_tx_desc->des3 = 0;

            /* Issue synchronization barrier instruction */
            __asm volatile ("DSB SY");

            ux_count--;

            /* Give back counting semaphore */
            if (osal_semaphore_post(hxgmac->tx_sem) == false)
            {
                ret_status = -EIO;
            }

            ++tail_indx;
            if (tail_indx == XGMAC_NUM_TX_DESC)
            {
                tail_indx = 0;
            }

            hxgmac->tx_desc_tail = tail_indx;
        }
        else
        {
            ret_status = -EAGAIN;
        }

        if (osal_semaphore_post(hxgmac->tx_mutex) == false)
        {
            ret_status = -EIO;
        }
    }
    else
    {
        ret_status = -EINVAL;
    }

    return ret_status;
}

static Basetype_t dma_soft_reset(xgmac_base_addr_t emac_base_addr)
{
    uint8_t elapsed_time = 0;
    uint8_t timeout = 100;

    if (xgmac_dma_software_reset(emac_base_addr) != XGMAC_LL_RETVAL_SUCCESS)
    {
        return false;
    }
    /* Poll for reset for completion */
    while (!xgmac_is_dma_reset_done(emac_base_addr))
    {
        if (elapsed_time >= timeout)
        {
            return false;
        }
        /* Wait for some delay */
        DELAY_MS(1);
        elapsed_time += 1U;
    }
    return true;
}
static Basetype_t dma_set_descriptors(xgmac_handle_t hxgmac)

{
    /* Initialize the Tx and Rx Head and Tail to 0*/
    hxgmac->tx_desc_head = 0;
    hxgmac->rx_desc_head = 0;

    hxgmac->tx_desc_tail = 0;
    hxgmac->rx_desc_tail = 0;

    /* Create Descriptor list for Tx and Rx */
    if (hxgmac->tx_bd_ring == NULL)
    {
        hxgmac->tx_bd_ring = hxgmac->axBufferDescTx;
    }

    if (hxgmac->rx_bd_ring == NULL)
    {
        hxgmac->rx_bd_ring = hxgmac->axBufferDescRx;
    }

    /* Set all field values to zero */
    (void)memset(hxgmac->tx_bd_ring, '\0', sizeof(xgmac_buf_desc_t));
    (void)memset(hxgmac->rx_bd_ring, '\0', sizeof(xgmac_buf_desc_t));

    /* Setup Tx Descriptor Table Parameters */
    dma_setup_tx_descriptor_list(hxgmac);

    /* Create the Tx Buffer Descriptor Semaphore  */
    if (hxgmac->tx_sem == NULL)
    {
        hxgmac->tx_sem =
                osal_semaphore_counting_create(NULL, (UBaseType_t)XGMAC_NUM_TX_DESC,
                (UBaseType_t)XGMAC_NUM_TX_DESC);
        configASSERT(hxgmac->tx_sem != NULL);
    }

    /* Create the Tx Descriptor Mutex   */
    if (hxgmac->tx_mutex == NULL)
    {
        hxgmac->tx_mutex = osal_mutex_create(NULL);
        configASSERT(hxgmac->tx_mutex != NULL);
    }

    /* Setup Rx Descriptor Table Parameters */
    dma_setup_rx_descriptor_list(hxgmac);

    return true;
}

void dma_setup_tx_descriptor_list(xgmac_handle_t hxgmac)
{
    xgmac_buf_desc_t *pdma_descriptor;
    int16_t index;

    /* Initialize the Tx buffer descriptor pointer */
    pdma_descriptor = hxgmac->tx_bd_ring;

    /* Initialize the Tx buffer descriptor parameters - Desc0, Desc1, Desc2, Desc3 */
    for (index = 0; index < XGMAC_NUM_TX_DESC; index++)
    {
        /* Initialize Buffer1 address pointer in Handle TxDMA Buffer Pointer Array to NULL */
        hxgmac->ptx_dma_buf1_ap[index] = NULL;

        /* Initialize all Descriptors to 0 */
        pdma_descriptor[index].des0 = 0;
        pdma_descriptor[index].des1 = 0;
        pdma_descriptor[index].des2 = 0;
        pdma_descriptor[index].des3 = 0;
    }
}

void dma_setup_rx_descriptor_list(xgmac_handle_t hxgmac)
{
    xgmac_buf_desc_t *pdma_descriptor;
    int16_t index;

    /* Initialize the Rx buffer descriptor pointer */
    pdma_descriptor = hxgmac->rx_bd_ring;

    /* Initialize the Rx buffer descriptor parameters - Desc0, Desc1, Desc2, Desc3 */
    for (index = 0; index < XGMAC_NUM_RX_DESC; index++)
    {
        /* Initialize Buffer1 address pointer in Handle RxDMA Buffer Pointer Array to NULL */
        hxgmac->prx_dma_buf1_ap[index] = NULL;

        /* Initialize all Descriptors to 0. These will be set in Refill Descriptor */
        pdma_descriptor[index].des0 = 0;
        pdma_descriptor[index].des1 = 0;
        pdma_descriptor[index].des2 = 0;
        pdma_descriptor[index].des3 = 0;
    }
}
static void dma_channel_init(xgmac_handle_t hxgmac, const
        xgmac_dev_config_str_t *xgmac_dev_config)
{
    xgmac_base_addr_t dma_base_addr;
    xgmac_dma_desc_addr_t dma_desc_addr_params;
    xgmac_buf_desc_t *pax_buffer_desc_tx;
    xgmac_buf_desc_t *pax_buffer_desc_rx;
    uint8_t dma_ch_index = XGMAC_DMA_CH0;

    dma_base_addr = hxgmac->xgmac_inst_dma_base_addr;
    if (dma_base_addr == 0U)
    {
        return;
    }

    /* Program the DMA channel Descriptor Registers */
    pax_buffer_desc_tx = hxgmac->tx_bd_ring;
    pax_buffer_desc_rx = hxgmac->rx_bd_ring;

    dma_desc_addr_params.tx_ring_len = (uint32_t)(XGMAC_NUM_TX_DESC - 1);
    dma_desc_addr_params.rx_ring_len = (uint32_t)(XGMAC_NUM_RX_DESC - 1);
    dma_desc_addr_params.tx_desc_low_addr = (uint32_t)(uintptr_t)pax_buffer_desc_tx;
    dma_desc_addr_params.tx_desc_high_addr = (uint32_t)((uintptr_t)pax_buffer_desc_tx >> 32);
    dma_desc_addr_params.rx_desc_low_addr = (uint32_t)(uintptr_t)pax_buffer_desc_rx;
    dma_desc_addr_params.rx_desc_high_addr = (uint32_t)((uintptr_t)pax_buffer_desc_rx >> 32);
    dma_desc_addr_params.tx_last_desc_addr =
            (uint32_t)(uintptr_t)&(pax_buffer_desc_tx[XGMAC_NUM_TX_DESC -
            1]);
    dma_desc_addr_params.rx_last_desc_addr =
            (uint32_t)(uintptr_t)&(pax_buffer_desc_rx[XGMAC_NUM_RX_DESC -
            1]);

    /* Set DMA Tx/Rx Descriptor Address */
    xgmac_init_dma_channel_desc_reg(dma_base_addr, dma_ch_index, &dma_desc_addr_params);

    if (dma_base_addr == 0U)
    {
        return;
    }
    /* Program  DMA channel Control Settings */
    xgmac_config_dma_channel_control(dma_base_addr, dma_ch_index, (const
            xgmacdma_chanl_config_t *)(uintptr_t)xgmac_dev_config->
            dma_channel_config);
}

static Basetype_t dma_enable_interrupts(xgmac_handle_t hxgmac, const
        xgmac_dev_config_str_t *xgmac_dev_config)
{
    xgmac_base_addr_t dma_base_addr;
    uint8_t dma_chnl_index;
    uint8_t dma_num_chnls;

    dma_num_chnls = xgmac_dev_config->mac_dev_config->nofdmachannels;
    dma_base_addr = hxgmac->xgmac_inst_dma_base_addr;

    for (dma_chnl_index = 0; dma_chnl_index < dma_num_chnls;
            dma_chnl_index++)
    {
        if (dma_base_addr == 0U)
        {
            return false;
        }
        /* Enable Normal Interrupt Summary and associated interrupt mask bits */
        if (xgmac_enable_dma_interrupt(dma_base_addr, dma_chnl_index,
                INTERRUPT_NIS) != XGMAC_LL_RETVAL_SUCCESS)
        {
            return false;
        }

        if (dma_base_addr == 0U)
        {
            return false;
        }
        /* Enable Error Interrupt Summary and associated interrupt mask bits */
        if (xgmac_enable_dma_interrupt(dma_base_addr, dma_chnl_index,
                INTERRUPT_AIS) != XGMAC_LL_RETVAL_SUCCESS)
        {
            return false;
        }
    }

    /* Register IRQ Handler with GIC for XGMAC */
    if (dma_register_isr(hxgmac) != true)
    {
        ERROR("SOCFPGA_XGMAC Init: ISR Registration Failed.");
        return false;
    }
    return true;
}

static Basetype_t dma_register_isr(xgmac_handle_t hxgmac)
{
    int32_t instance;
    socfpga_hpu_interrupt_t emac_int_id;
    socfpga_interrupt_err_t err_ret;

    instance = hxgmac->instance;
    emac_int_id = get_emac_intr_id(instance);

    /* Clear the MAC based interrupts */
    xgmac_disable_interrupt(hxgmac->xgmac_inst_base_addr);

    /* Register the ISR */
    err_ret = interrupt_register_isr(emac_int_id, socfpga_xgmac_dma_isr, hxgmac);
    if (err_ret != ERR_OK)
    {
        return false;
    }

    /* Enable Shared peripheral interrupt */
    err_ret = interrupt_enable(emac_int_id, GIC_INTERRUPT_PRIORITY_ENET);
    if (err_ret != ERR_OK)
    {
        return false;
    }

    return true;
}

void socfpga_xgmac_dma_isr(void *param)
{
    xgmac_handle_t hxgmac = (xgmac_handle_t)param;
    xgmac_base_addr_t dma_base_addr;
    xgmac_dma_interrupt_id_t id;
    xgmac_err_t err_type = XGMAC_ERR_UNHANDLED;
    xgmac_int_status_t int_status = XGMAC_ERR_EVENT;
    uint32_t base_dma_chnl_address;
    uint8_t dmachnum;

    dma_base_addr = hxgmac->xgmac_inst_dma_base_addr;
    dmachnum = XGMAC_DMA_CH0;
    base_dma_chnl_address = ((uint32_t)(uintptr_t)dma_base_addr +
            XGMAC_DMA_CHANNEL_BASE +
            ((uint32_t)dmachnum * XGMAC_DMA_CHANNEL_INC));

    id = check_and_clear_xgmac_interrupt_status(base_dma_chnl_address);

    switch (id)
    {
        case INTERRUPT_TI:
            int_status = XGMAC_TX_DONE_EVENT;
            break;

        case INTERRUPT_RI:
            int_status = XGMAC_RX_EVENT;
            break;

        case INTERRUPT_FBE:
        case INTERRUPT_TXS:
        case INTERRUPT_RBU:
        case INTERRUPT_RS:
        case INTERRUPT_DDE:
        case INTERRUPT_UNHANDLED:
            {
                xgmac_err_info_t *pIntData = (xgmac_err_info_t *)hxgmac->pcntxt;
                int_status = XGMAC_ERR_EVENT;

                /* Re-mapping the error type to handle in the Network Interface Layer */
                switch (id)
                {
                    case INTERRUPT_FBE:
                        err_type = XGMAC_ERR_FATAL_BUS;
                        break;
                    case INTERRUPT_TXS:
                        err_type = XGMAC_ERR_TX_STOPPED;
                        break;
                    case INTERRUPT_RBU:
                        err_type = XGMAC_ERR_RX_BUF_UNAVAILABLE;
                        break;
                    case INTERRUPT_RS:
                        err_type = XGMAC_ERR_RX_STOPPED;
                        break;
                    case INTERRUPT_DDE:
                        err_type = XGMAC_ERR_DESC_DEFINE;
                        break;
                    default: /*do nothing*/
                        break;
                }

                pIntData->err_ch = dmachnum;
                pIntData->err_type = (uint8_t)err_type;
                break;
            }

        default: /*do nothing*/
            break;
    }

    if (hxgmac->callback != NULL)
    {
        hxgmac->callback(int_status, hxgmac->pcntxt);
        check_and_clear_link_interrupt_status((uint32_t)((uintptr_t)hxgmac
                ->xgmac_inst_base_addr));
    }
}
