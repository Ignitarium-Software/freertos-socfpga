/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA USB3.1 XHCI controller.
 * Context sub-module.
 */

/*
 * xHCI are data structures that are associated with a device which stores the state information.
 *
 * Following are the xHCI context structures
 *
 *  -> Device context
 *  -> Slot context
 *  -> Endpoint context
 *  -> Input Context
 *      -> Input Control Context
 *  -> Output Context
 *
 * Each device has its own associated context structures. xHCI implements the DCBAA structure
 * to associate a device slot with the associated device context data structure.
 * The DCBAA structure can be indexed by the device slot ID.
 *
 *           DCBAA
 *       +-----------+
 *       |   Slot 0  |------------------->+------------------+
 *       +-----------+                    |  sractchpad buf  |
 *       |   Slot 1  |                    +------------------+
 *       +-----------+------------------->+------------------+
 *       |    ...    |                    |   Slot Context   |
 *       +-----------+                    +------------------+             +---------------+
 *       |   Slot n  |                    |  Endpoint 0 (bi) |------------>| transfer ring |
 *       +-----------+                    +------------------+             +---------------+
 *                                        | Endpoint 1 (out) |                    .
 *                                        +------------------+                    .
 *                                        | Endpoint 1 (in)  |                    .
 *                                        +------------------+                    .
 *                                        |       ...        |                    .
 *                                        +------------------+             +---------------+
 *                                        | Endpoint 15 (in) |------------>| transfer ring |
 *                                        +------------------+             +---------------+
 */

#include "xhci.h"
#include "socfpga_cache.h"
#include "socfpga_defines.h"
#include "xhci_endpoints.h"
#include "xhci_context.h"
#include "xhci_events.h"
#include "osal_log.h"

int alloc_xhci_contexts(struct xhci_data *xhci_ptr)
{
    int ret = 0;
    xhci_ptr->xcr_ring = alloc_command_ring();
    if (xhci_ptr->xcr_ring == NULL)
    {
        ERROR("Cannot allocate memory!!!");
        ret = -ENOMEM;
    }

    if (alloc_input_device_context(xhci_ptr) != 0)
    {
        ERROR("Cannot allocate memory!!!");
        ret = -ENOMEM;
    }

    if (alloc_output_device_context(xhci_ptr) != 0)
    {
        ERROR("Cannot allocate memory!!!");
        ret = -ENOMEM;
    }

    if (ret != 0)
    {
        if (xhci_ptr->xcr_ring != NULL)
        {
            vPortFree((void *)xhci_ptr->xcr_ring);
        }
        if (xhci_ptr->ip_ctx != NULL)
        {
            vPortFree((void *)xhci_ptr->ip_ctx);
        }
        if (xhci_ptr->op_ctx != NULL)
        {
            vPortFree((void *)xhci_ptr->op_ctx);
        }
    }
    INFO("xHCI contexts allocated successfully");
    return ret;
}

void deallocate_xhci_context(struct xhci_data *xhci_ptr)
{
    const uint32_t slotid = xhci_ptr->dev_data.slot_id;

    /* remove the output context address from dcbaa */
    xhci_ptr->dcbaa->dev_context_ptrs[slotid] = 0UL;

    /* free bulk endpoint tr rings */
    if (xhci_ptr->msc_eps.ep_out.ep_tr_enq_ptr != NULL)
    {
        vPortFree((void *)(uintptr_t)(xhci_ptr->msc_eps.ep_out.ep_tr_enq_ptr));
    }
    if (xhci_ptr->msc_eps.ep_in.ep_tr_enq_ptr != NULL)
    {
        vPortFree((void *)(uintptr_t)(xhci_ptr->msc_eps.ep_in.ep_tr_enq_ptr));
    }

    /* Free Control EP TR ring */
    if (xhci_ptr->ep0.ep_tr_enq_ptr != NULL)
    {
        vPortFree((void *)(uintptr_t)(xhci_ptr->ep0.ep_tr_enq_ptr));
    }

    /* clear input and output context data structures */
    bzero(xhci_ptr->op_ctx, sizeof(xhci_op_device_context_t));
    bzero(xhci_ptr->ip_ctx, sizeof(xhci_ip_device_context_t));

    xhci_ptr->dev_data.slot_id = 0U;
    xhci_ptr->dev_data.rh_port = 0U;
    INFO("xHCI contexts deallocated successfully");
}

int init_xhci_context_params(struct xhci_data *xhci_ptr)
{
    /* Initialize all device parameters to 0 */
    xhci_ptr->dev_data.slot_id = 0U;
    xhci_ptr->dev_data.dev_addr = 0U;
    xhci_ptr->dev_data.dev_speed = 0U;
    xhci_ptr->dev_data.rh_port = 0U;

    if (alloc_dcbaa(xhci_ptr) != 0)
    {
        return -ENOMEM;
    }

    if (xhci_crcr_init(xhci_ptr->xcr_ring) != 0)
    {
        return -ENOMEM;
    }

    if (init_xhc_event_ring(xhci_ptr) != 0)
    {
        return -ENOMEM;
    }

    return 0;
}

/**
 * @brief function to allocate memory for input context and initialize them to 0
 */
int alloc_input_device_context(struct xhci_data *xhci_ptr)
{
    xhci_ptr->ip_ctx = (xhci_ip_device_context_t *)pvPortMallocCoherent(
            sizeof(xhci_ip_device_context_t));
    if (xhci_ptr->ip_ctx == NULL)
    {
        ERROR("Cannot allocate memory!!!");
        return -ENOMEM;
    }

    bzero(xhci_ptr->ip_ctx, sizeof(xhci_ip_device_context_t));
    return 0;
}

int init_input_device_context(struct xhci_data *xhci_ptr)
{
    xhci_ip_device_context_t *input_ctx = xhci_ptr->ip_ctx;

    uint64_t ep0_max_pkt_size = 0UL;
    uint8_t dev_speed;
    uint8_t rh_port;
    uint8_t context_entries;
    uint64_t add_flags = 0UL;
    int ret = 0;
    xhci_trb_t *xep0_tr_ptr;

    /*
     * Input Control Context
     * Select A0, A1 flags
     */
    add_flags = (DCI_SLOT_CONTEXT << SLOT_CONTEXT_POS);
    add_flags |= (EP0_DCI <<  EP0_DCI_POS);
    input_ctx->xi_context.xic_context_flags = ((uint64_t)add_flags << 32);

    /*
     * Slot contexts
     * Context Entries -> 1
     * Speed -> 3 ( from PORTSC PLS bits )
     * RH port number -> 1
     */
    dev_speed = xhci_ptr->dev_data.dev_speed;
    rh_port = xhci_ptr->dev_data.rh_port;

    switch (dev_speed)
    {
        case 4:
            PRINT("xHCI - SS device attached");
            ep0_max_pkt_size = EP0_MAX_PKT_SIZE_SS;
            break;
        case 3:
            PRINT("xHCI - HS device attached");
            ep0_max_pkt_size = EP0_MAX_PKT_SIZE_HS;
            break;
        case 2:
            PRINT("xHCI - LS device attached");
            ep0_max_pkt_size = EP0_MAX_PKT_SIZE_FS;
            break;
        case 1:
            PRINT("xHCI - FS device attached");
            ep0_max_pkt_size = EP0_MAX_PKT_SIZE_LS;
            break;
        default:
            /* resource unavalable, try again */
            PRINT("XHCI - Device enumeration failed");
            ret = -EAGAIN;
            break;
    }

    if (ret != 0)
    {
        return ret;
    }

    /* Update the slot context fields */

    /*get the context entry field for the control endpoint */
    context_entries = get_ep_dci(0U);

    input_ctx->xs_context.xsc_info1 = ((uint64_t)dev_speed << 20);
    input_ctx->xs_context.xsc_info1 |= ((uint64_t)context_entries << 27);
    input_ctx->xs_context.xsc_info1 |= ((uint64_t)rh_port << 48);

    /*
     * Endpoint Contexts
     * EP Type -> 4 ( Control )
     * Max Packet Size -> 8 ( for LS )
     * Cerr -> 3
     * TR Dequeue pointer
     * DCS -> 1
     * Avg. TRB Length -> 8
     */
    xep0_tr_ptr = allocate_ring_segment(XHCI_EP_TR_RING_ALIGN,
            EP_TRB_SEG_LENGTH);
    if (xep0_tr_ptr == NULL)
    {
        ERROR("Cannot allocate memory!!!");
        return -ENOMEM;
    }

    input_ctx->xe_context[0].xec_info = ((uint64_t)CONTROL << 35);
    input_ctx->xe_context[0].xec_info |= ((uint64_t)ep0_max_pkt_size <<
            EP_CTX_MAX_PKT_SIZE_POS);
    input_ctx->xe_context[0].xec_info |= ((uint64_t)3U << 33);

    input_ctx->xe_context[0].xec_dequeue = (uint64_t)xep0_tr_ptr;
    input_ctx->xe_context[0].xec_dequeue |= EP0_CCS_FLAG;

    input_ctx->xe_context[0].xec_info2 = EP_DEFAULT_TRB_LEN;

    xhci_ptr->ep0.ep_addr = 0x00U;
    xhci_ptr->ep0.ep_index = 1U;
    xhci_ptr->ep0.pcs_flag = 1U;
    xhci_ptr->ep0.ep_type = CONTROL;
    xhci_ptr->ep0.ep_tr_enq_ptr = xep0_tr_ptr;

    return 0;
}

int alloc_output_device_context(struct xhci_data *xhci_ptr)
{
    xhci_ptr->op_ctx = (xhci_op_device_context_t *)pvPortMallocCoherent(
            sizeof(xhci_op_device_context_t));
    if (xhci_ptr->op_ctx == NULL)
    {
        ERROR("Canot allocate memory!!!");
        return -ENOMEM;
    }

    bzero(xhci_ptr->op_ctx, sizeof(xhci_op_device_context_t));

    return 0;
}

void init_xhc_endpoint_context(struct xhci_data *xhci, xhci_ep_type_t ep_type)
{
    usb_endpoint_descriptor_t *ep_desc;
    xhci_trb_t *tr_ptr;
    uint8_t pcs_flag;
    uint8_t ep_index;

    if (ep_type == BULK_OUT)
    {
        ep_desc = &xhci->msc_eps.ep_out.ep_desc;
        pcs_flag = xhci->msc_eps.ep_out.pcs_flag;
        ep_index = xhci->msc_eps.ep_out.ep_index;
        tr_ptr = xhci->msc_eps.ep_out.ep_tr_enq_ptr;
    }

    /* ep_type = BULK_IN */
    else
    {
        ep_desc = &xhci->msc_eps.ep_in.ep_desc;
        pcs_flag = xhci->msc_eps.ep_in.pcs_flag;
        ep_index = xhci->msc_eps.ep_in.ep_index;
        tr_ptr = xhci->msc_eps.ep_in.ep_tr_enq_ptr;
    }

    if (ep_desc == NULL)
    {
        return;
    }

    xhci->ip_ctx->xe_context[ep_index].xec_info = ((uint64_t)ep_type << 35);
    xhci->ip_ctx->xe_context[ep_index].xec_dequeue = (uint64_t)tr_ptr;
    xhci->ip_ctx->xe_context[ep_index].xec_dequeue |= pcs_flag;

    /* packet size */
    xhci->ip_ctx->xe_context[ep_index].xec_info |=
            ((uint64_t)ep_desc->wMaxPacketSize << EP_CTX_MAX_PKT_SIZE_POS);

    /* Cerr = 3 */
    xhci->ip_ctx->xe_context[ep_index].xec_info |= ((uint64_t)3U << 33);
}

int alloc_dcbaa(struct xhci_data *xhci_ptr)
{
    xhci_ptr->dcbaa = (struct xhci_device_context_array *)pvPortMallocCoherent(
            sizeof(struct xhci_device_context_array));
    if (xhci_ptr->dcbaa == NULL)
    {
        return -ENOMEM;
    }
    for (int i = 0; i < 256; i++)
    {
        xhci_ptr->dcbaa->dev_context_ptrs[i] = 0UL;
    }

    WR_REG64((USBBASE + USB3_DCBAAP_LO), (uint64_t)xhci_ptr->dcbaa);
    DEBUG("xHCI DCBAA register updated");

    return 0;
}

void update_dcbaa_entry(struct xhci_data *xhci_ptr)
{
    const uint32_t slotid = xhci_ptr->dev_data.slot_id;
    xhci_ptr->dcbaa->dev_context_ptrs[slotid] = (uint64_t)xhci_ptr->op_ctx;
}

void update_xhc_slot_context(struct xhci_data *xhci_ptr)
{
    const uint8_t dci_bulk_in = get_ep_dci(xhci_ptr->msc_eps.ep_in.ep_addr);
    const uint8_t dci_bulk_out = get_ep_dci(xhci_ptr->msc_eps.ep_out.ep_addr);
    const uint8_t context_entry_val = (dci_bulk_in > dci_bulk_out)
            ? dci_bulk_in : dci_bulk_out;
    uint64_t add_flags = 0UL;

    add_flags |= (DCI_SLOT_CONTEXT << SLOT_CONTEXT_POS);
    add_flags |= (uint64_t)(1UL) << (dci_bulk_out);
    add_flags |= (uint64_t)1UL << (dci_bulk_in);

    xhci_ptr->ip_ctx->xi_context.xic_context_flags = (add_flags << 32);

    xhci_ptr->ip_ctx->xs_context.xsc_info1 &= SLOT_CONTEXT_ENTRIES_MSK;
    xhci_ptr->ip_ctx->xs_context.xsc_info1 |= ((uint64_t)context_entry_val <<
            SLOT_CONTEXT_ENTRIES_POS);
}

void display_xhci_device_params(struct xhci_device_data *dev_data)
{
    char const *dev_speed[] = {"Null", "Low Speed", "Full Speed", "High Speed"};

    DEBUG("Device address : %d", dev_data->dev_addr);
    DEBUG("Device Slot: %d", dev_data->slot_id);
    DEBUG("Device rh port: %d", dev_data->rh_port);
    DEBUG("Device speed : %s", dev_speed[dev_data->dev_speed]);

    /* Suppressing unused variable warning when not using DEBUG logs. */
    (void)dev_data;
    (void)dev_speed;
}

void update_device_dev_speed(struct xhci_data *xhci_ptr)
{
    xhci_ptr->dev_data.dev_speed = get_xhc_port_speed(
            xhci_ptr->dev_data.rh_port);
}

void update_device_slot_id(struct xhci_device_data *dev_data, uint8_t slot_id)
{
    dev_data->slot_id = slot_id;
}

void update_device_rh_params(struct xhci_data *xhci_ptr,
        xhci_psceg_params_t rh_params)
{
    xhci_ptr->dev_data.rh_port = (uint8_t) rh_params.rhport;
}

void update_device_address(struct xhci_data *xhci_ptr)
{
    xhci_ptr->dev_data.dev_addr = get_xhc_device_address(xhci_ptr);
}

uint8_t get_xhc_device_address(struct xhci_data *xhci_ptr)
{
    uint8_t daddr = 0;

    daddr = (uint8_t) ((xhci_ptr->op_ctx->slot_context.xsc_info2 >>
            DEV_ADDRESS_FIELD) & XHC_DEV_ADDR_MSK);

    return daddr;
}

void update_endpoint_packetsize(xhci_ip_device_context_t *ip_ctx,
        uint16_t max_pkt_size)
{
    ip_ctx->xe_context[0].xec_info &= EP_CTX_MAX_PKT_SIZE_MSK;
    ip_ctx->xe_context[0].xec_info |= ((uint64_t)max_pkt_size <<
            EP_CTX_MAX_PKT_SIZE_POS);
}

void display_ip_context(xhci_ip_device_context_t *ip_ctx)
{
    DEBUG(" Input control Contexts ");
    DEBUG(" Field 1 : %lx", ip_ctx->xi_context.xic_context_flags);

    DEBUG(" Slot Contexts ");
    DEBUG(" Field 1 : %lx", ip_ctx->xs_context.xsc_info1);
    DEBUG(" Field 2 : %lx", ip_ctx->xs_context.xsc_info2);

    DEBUG(" Endpoint 0  Contexts ");
    DEBUG(" Field 1 : %lx", ip_ctx->xe_context[0].xec_info);
    DEBUG(" Field 2 : %lx", ip_ctx->xe_context[0].xec_dequeue);
    DEBUG(" Field 3 : %lx", ip_ctx->xe_context[0].xec_info2);

    (void)ip_ctx;
}

void display_op_context(xhci_op_device_context_t *op_ctx)
{
    DEBUG(" Slot Contexts ");
    DEBUG(" Field 1 : %lx", op_ctx->slot_context.xsc_info1);
    DEBUG(" Field 2 : %lx", op_ctx->slot_context.xsc_info2);

    DEBUG(" Endpoint 0  Contexts ");
    DEBUG(" Field 1 : %lx", op_ctx->EP_0.xec_info);
    DEBUG(" Field 2 : %lx", op_ctx->EP_0.xec_dequeue);
    DEBUG(" Field 3 : %lx", op_ctx->EP_0.xec_info2);

    DEBUG(" Endpoint 1 OUT  Contexts ");
    DEBUG(" Field 1 : %lx", op_ctx->EP[0].xec_info);
    DEBUG(" Field 2 : %lx", op_ctx->EP[0].xec_dequeue);
    DEBUG(" Field 3 : %lx", op_ctx->EP[0].xec_info2);

    DEBUG(" Endpoint 2 IN Contexts ");
    DEBUG(" Field 1 : %lx", op_ctx->EP[3].xec_info);
    DEBUG(" Field 2 : %lx", op_ctx->EP[3].xec_dequeue);
    DEBUG(" Field 3 : %lx \r\n", op_ctx->EP[3].xec_info2);

    (void)op_ctx;
}
