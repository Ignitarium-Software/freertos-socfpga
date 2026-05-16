/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA USB3.1 XHCI controller.
 * Capabilities sub-module
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "xhci.h"
#include "osal_log.h"

/*
 * @brief read the host controller capability registers
 */
int get_xhc_cap_params(struct xhci_data *xhci)
{
    uint32_t caplength = RD_REG32(USBBASE + USB3_CAPLENGTH);
    uint32_t dboff = RD_REG32(USBBASE + USB3_DBOFF);
    uint32_t rtsoff = RD_REG32(USBBASE + USB3_RTSOFF);

    xhci->xhc_cap_ptr = (xhci_cap_reg_t *)USBBASE;

    DEBUG("CAPLENGTH : %x",
            (caplength & USB3_CAPLENGTH_CAPLENGTH_MASK));
    DEBUG("HCSPARAMS1 : %x",
            RD_REG32(USBBASE + USB3_HCSPARAMS1));
    DEBUG("HCSPARAMS2 : %x",
            RD_REG32(USBBASE + USB3_HCSPARAMS2));
    DEBUG("HCSPARAMS3 : %x",
            RD_REG32(USBBASE + USB3_HCSPARAMS3));
    DEBUG("HCCPARAMS1 : %x",
            RD_REG32(USBBASE + USB3_HCCPARAMS1));
    DEBUG("DBOFF : %x",
            (dboff & USB3_DBOFF_DOORBELL_ARRAY_OFFSET_MASK));
    DEBUG("RTOFF : %x",
            (rtsoff & USB3_RTSOFF_RUNTIME_REG_SPACE_OFFSET_MASK));
    DEBUG("HCCPARAMS2 : %x",
            RD_REG32(USBBASE + USB3_HCCPARAMS2));

    xhci->op_regs.xhci_op_base = (uint64_t)USBBASE +
            (uint64_t)(caplength & USB3_CAPLENGTH_CAPLENGTH_MASK);
    xhci->op_regs.xhci_runtime_base = (uint64_t)USBBASE +
            (uint64_t)(rtsoff & USB3_RTSOFF_RUNTIME_REG_SPACE_OFFSET_MASK);
    xhci->op_regs.xhci_db_base = (uint64_t)USBBASE +
            (uint64_t)(dboff & USB3_DBOFF_DOORBELL_ARRAY_OFFSET_MASK);

    DEBUG("Operational base : %lx ",
            xhci->op_regs.xhci_op_base);
    DEBUG("Runtime  base : %lx ",
            xhci->op_regs.xhci_runtime_base);
    DEBUG("Doorbell base : %lx ",
            xhci->op_regs.xhci_db_base);

    return 0;
}

xhci_oper_reg_params_t get_xhci_op_registers(void)
{
    xhci_oper_reg_params_t op_reg = {0};
    uint32_t cap = RD_REG32(USBBASE + offsetof(xhci_cap_reg_t, caplength));
    uint32_t dboff = RD_REG32(USBBASE + offsetof(xhci_cap_reg_t, dboff));
    uint32_t rtsoff = RD_REG32(USBBASE + offsetof(xhci_cap_reg_t, rtsoff));
    /* CAPLENGTH[7:0] */
    op_reg.xhci_op_base = (uint64_t)USBBASE + (uint64_t)(cap & 0xFFU);
    /* DBOFF[31:2] */
    op_reg.xhci_db_base = (uint64_t)USBBASE + (uint64_t)(dboff & ~0x3U);
    /* RSTOFF[31:5] */
    op_reg.xhci_runtime_base = (uint64_t)USBBASE + (uint64_t)(rtsoff & ~0x1FU);
    return op_reg;
}
