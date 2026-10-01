/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA USB3.1 XHCI controller.
 * Interrupt sub-module.
 */

#include "xhci.h"
#include "xhci_interrupts.h"
#include "socfpga_interrupt.h"
#include "socfpga_usb3_reg.h"
#include "socfpga_defines.h"
#include "socfpga_interrupt_priority.h"
#include "osal_log.h"

void socfpga_usb3_handler(void *handler);

void enable_xhci_interrupts(void)
{
    volatile uint32_t reg_val;
    struct xhci_oper_reg_params op_regs;

    op_regs = get_xhci_op_registers();

    reg_val = RD_REG32((op_regs.xhci_runtime_base + USB3_IMAN));
    reg_val |= USB3_IMAN_IE_MASK;
    WR_REG32((op_regs.xhci_runtime_base + USB3_IMAN), reg_val);

    reg_val = RD_REG32((USBBASE + USB3_USBCMD));
    reg_val |= USB3_USBCMD_INTE_MASK;
    WR_REG32((USBBASE + USB3_USBCMD), reg_val);
}

void disable_xhci_interrupts(void)
{

    volatile uint32_t reg_val;
    struct xhci_oper_reg_params op_regs;

    op_regs = get_xhci_op_registers();

    reg_val = RD_REG32((op_regs.xhci_runtime_base + USB3_IMAN));
    reg_val &= ~(USB3_IMAN_IE_MASK);
    WR_REG32((op_regs.xhci_runtime_base + USB3_IMAN), reg_val);

    reg_val = RD_REG32((op_regs.xhci_runtime_base + USB3_IMAN));
    reg_val &= ~(USB3_IMAN_IP_MASK);
    WR_REG32((op_regs.xhci_runtime_base + USB3_IMAN), reg_val);

    reg_val = RD_REG32((USBBASE + USB3_USBCMD));
    reg_val &= ~(USB3_USBCMD_INTE_MASK);
    WR_REG32((USBBASE + USB3_USBCMD), reg_val);
}

bool register_usb3ISR(xhci_int_ptr_t int_handler)
{
    int err;

    err = interrupt_register_isr(USB1IRQ, socfpga_usb3_handler,
            int_handler);
    if (err != 0)
    {
        return false;
    }

    err = interrupt_enable(USB1IRQ, GIC_INTERRUPT_PRIORITY_USB3);
    if (err != 0)
    {
        ERROR("Failed to enable interrupt");
        return false;
    }

    INFO("Interrupt handler registered successfully");
    return true;
}

void socfpga_usb3_handler(void *handler)
{
    xhci_int_ptr_t usb3_int_handler;
    uint32_t reg_val;
    struct xhci_oper_reg_params op_regs;

    op_regs = get_xhci_op_registers();

    usb3_int_handler = (xhci_int_ptr_t) handler;

    reg_val = RD_REG32((USBBASE + USB3_USBSTS));

    if (((reg_val & USB3_USBSTS_HSE_MASK) != 0U) || ((reg_val &
            USB3_USBSTS_HCE_MASK) != 0U))
    {
        (void) xhci_reset();
        start_xhci_controller();
        return;
    }

    reg_val |= (USB3_USBSTS_EINT_MASK);
    WR_REG32((USBBASE + USB3_USBSTS), reg_val);

    reg_val = RD_REG32((op_regs.xhci_runtime_base + USB3_IMAN));
    reg_val |= USB3_IMAN_IP_MASK;
    WR_REG32((op_regs.xhci_runtime_base + USB3_IMAN), reg_val);

    /* rh_port = 1 -> RH port number as per tinyusb stack */
    /* in_isr = true, ISR flag for tinyusb stack */
    usb3_int_handler->int_handler(1, true);
}
