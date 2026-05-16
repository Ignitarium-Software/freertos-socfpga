/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA USB3.1 XHCI controller.
 * Events sub-module
 */

#include <stdio.h>
#include "xhci_events.h"
#include "socfpga_defines.h"
#include "socfpga_cache.h"
#include "osal_log.h"

static void erdp_register_update();

void display_event_trbs(struct xhci_data *xhci)
{
    cache_force_invalidate((void *) xhci->xer_ring.xer_enqueue_ptr,
            (size_t) (XHCI_EVENT_RING_SEG_LENGTH * sizeof(xhci_trb_t)));
    for (int i = 0; i < 10; i++)
    {
        DEBUG(" ID --> %d", i);
        DEBUG(" Address : %p", &xhci->xer_ring.xer_enqueue_ptr[i]);
        DEBUG(" Buffer : %lx", xhci->xer_ring.xer_enqueue_ptr[i].buffer);
        DEBUG(" Status : %x", xhci->xer_ring.xer_enqueue_ptr[i].status);
        DEBUG(" Control : %x", xhci->xer_ring.xer_enqueue_ptr[i].control);
        DEBUG(" TRB Value is : %d",
                ((xhci->xer_ring.xer_enqueue_ptr[i].control) & 0xfc00U) >> 10);
    }
}

uint8_t get_xhc_port_speed(uint8_t rhport)
{
    uint32_t portsc_reg_val, portsc_reg;
    uint8_t port_speed;

    xhci_oper_reg_params_t op_reg = get_xhci_op_registers();
    portsc_reg = (uint32_t) ((op_reg.xhci_op_base + PORTSC_REG_OFFSET +
            (0x10U * ((uint64_t) rhport - 1UL))) & 0x00000000ffffffffUL);

    portsc_reg_val = RD_REG32(portsc_reg);
    port_speed = (uint8_t) ((portsc_reg_val & USB3_PORTSC_20_PORTSPEED_MASK) >>
            USB3_PORTSC_20_PORTSPEED_POS);

    return port_speed;
}

/*
 * @note : Bit 0 of PORTSC register returns the device connect/disconnect
 * status
 */
bool xhci_port_status(uint8_t rhport)
{
    uint32_t reg_val;
    uint32_t portsc_reg;

    xhci_oper_reg_params_t op_reg = get_xhci_op_registers();
    portsc_reg = (uint32_t) ((op_reg.xhci_op_base + PORTSC_REG_OFFSET +
            (0x10U * ((uint64_t) rhport - 1UL))) & 0x00000000ffffffffUL);

    /*Bit 0 gives attach/detach status */
    reg_val = RD_REG32(portsc_reg);
    if ((reg_val & 1U) != 0U)
    {
        return true;
    }
    return false;
}

/* reset the RH port */
void reset_xhci_port(uint8_t rhport)
{
    uint32_t reg_val;
    xhci_oper_reg_params_t op_reg = get_xhci_op_registers();
    uint32_t portsc_reg = (uint32_t) ((op_reg.xhci_op_base + PORTSC_REG_OFFSET +
            (0x10U * ((uint64_t) rhport - 1UL))) & 0x00000000ffffffffUL);

    reg_val = RD_REG32(portsc_reg);
    reg_val |= (USB3_PORTSC_20_PR_MASK);
    WR_REG32(portsc_reg, reg_val);
    INFO("xHCI port reset initiated on port : %d", rhport);
}

bool is_xhci_port_reset_end(uint8_t rhport)
{
    volatile uint32_t reg_val, portsc_reg;
    uint32_t loop = 100U;

    xhci_oper_reg_params_t op_reg = get_xhci_op_registers();
    portsc_reg = ((op_reg.xhci_op_base + PORTSC_REG_OFFSET +
            (0x10U * ((uint64_t) rhport - 1UL))) & 0x00000000ffffffffUL);
    do
    {
        reg_val = RD_REG32(portsc_reg);
        if ((reg_val & (1U << 4)) == 0U)
        {
            break;
        }

        loop--;
        osal_task_delay(10);
    }while(loop > 0U);

    if (loop == 0U)
    {
        ERROR("usb3 port reset failure");
        return false;
    }

    INFO("xHCI port reset completede on port : %d", rhport);
    return true;
}

xhci_event_trb_t get_xhc_event(struct xhci_data *xhci_ptr)
{
    uint64_t erdp_addr;
    xhci_event_trb_type_t trb_id;
    xhci_event_trb_t event = {0};

    /* decrement the event ring trb count */
    xhci_ptr->xer_ring.trb_count -= 1U;

    erdp_addr = RD_REG32(ERDP);
    erdp_addr &= 0xfffffff0UL;

    cache_force_invalidate((void *)erdp_addr, sizeof(xhci_trb_t));

    xhci_trb_t *event_trb = (xhci_trb_t *)(uint64_t)erdp_addr;

    trb_id = (xhci_event_trb_type_t)((event_trb->control & 0xfc00U) >> 10);

    if (trb_id == TRANSFER_EVENT)
    {
        event.tr_event.tr_trb_ptr = event_trb->buffer;
        event.tr_event.status_field = event_trb->status;
        event.tr_event.control_field = event_trb->control;
    }
    else if (trb_id == COMMAND_COMPLETION_EVENT)
    {
        event.cc_event.cmd_trb_ptr = event_trb->buffer;
        event.cc_event.status_field = event_trb->status;
        event.cc_event.control_field = event_trb->control;
    }
    else if (trb_id == PORT_STATUS_CHANGE_EVENT)
    {
        event.psc_event.buf = event_trb->buffer;
        event.psc_event.status_field = event_trb->status;
        event.psc_event.control_field = event_trb->control;
    }
    else
    {
        /* error TRB type */
        configASSERT(trb_id >= 0);
    }
    erdp_register_update(&xhci_ptr->xer_ring);
    return event;
}

static void erdp_register_update(xer_event_ring_t *xer_ring)
{
    uint32_t trb_count;
    uint64_t erdp_addr;

    trb_count = xer_ring->trb_count;

    if (trb_count == 0U)
    {
        erdp_addr = RD_REG32(ERDP);
        erdp_addr = (uint64_t)xer_ring->xer_enqueue_ptr;
        /* EHB flag */
        erdp_addr |= 8UL;
        /* reload the event ring trb count to initial value of EP_TRB_SEG_LENGTH */
        xer_ring->trb_count = XHCI_EVENT_RING_SEG_LENGTH;
    }
    else
    {
        erdp_addr = RD_REG32(ERDP);
        erdp_addr += 0x10U;
    }
    WR_REG64(ERDP, erdp_addr);
}

xhci_psceg_params_t handle_psceg_event(xpsc_event_t psc_event)
{
    uint32_t portsc_reg, portsc_register, temp = 0U;
    xhci_psceg_params_t port_params = { 0 };

    uint8_t rhport = (uint8_t) ((psc_event.buf & 0xff000000UL) >> 24);

    xhci_oper_reg_params_t op_reg = get_xhci_op_registers();
    portsc_register = (uint32_t) ((op_reg.xhci_op_base + PORTSC_REG_OFFSET +
            (0x10U * ((uint64_t) rhport - 1UL))) & 0x00000000ffffffffUL);
    portsc_reg = RD_REG32(portsc_register);

    if ((portsc_reg & USB3_PORTSC_20_CSC_MASK) > 0U)
    {
        /* device connected to RH PORT */
        if ((portsc_reg & USB3_PORTSC_20_CCS_MASK) == 1U)
        {
            /* acknowledge the status bit change */
            temp |= (USB3_PORTSC_20_CSC_MASK);
            port_params.rhport = rhport;
            port_params.dev_attach_flag = 1;
        }
        else
        /* device detach event from RH PORT */
        {
            /* acknowledge the status bit change */
            temp |= (USB3_PORTSC_20_CSC_MASK);
            port_params.rhport = rhport;
            port_params.dev_attach_flag = -1;
        }
    }
    if ((portsc_reg & USB3_PORTSC_20_PRC_MASK) > 0U)
    {
        /*Port Reset Event */
        temp |= USB3_PORTSC_20_PRC_MASK;
    }
    if ((portsc_reg & USB3_PORTSC_30_WRC_MASK) > 0U)
    {
        /*Warm PR Event */
        temp |= USB3_PORTSC_30_WRC_MASK;
    }

    portsc_reg &= ~PORTSC_CLEAR_FLAGS;
    portsc_reg |= temp;
    WR_REG32(portsc_register, portsc_reg);

    return port_params;
}

void xhci_warm_reset(uint8_t rhport)
{
    uint32_t reg_val, portsc_register;
    xhci_oper_reg_params_t op_reg = get_xhci_op_registers();

    portsc_register = (uint32_t) ((op_reg.xhci_op_base + PORTSC_REG_OFFSET +
            (0x10U * ((uint64_t) rhport - 1UL))) & 0x00000000ffffffffUL);

    reg_val = RD_REG32(portsc_register);
    reg_val |= USB3_PORTSC_30_WPR_MASK;
    WR_REG32(portsc_register, reg_val);
}
