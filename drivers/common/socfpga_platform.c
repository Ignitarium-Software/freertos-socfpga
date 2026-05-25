/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 */

#include <stdint.h>

#include "socfpga_defines.h"
#include "socfpga_sys_mngr_reg.h"
#include "socfpga_platform.h"

/*
 * Agilex5 A0 workaround detection mask.
 */
#define ALT_SYSMGR_SCRATCH_REG_POR_1_REVA_WORKAROUND_MASK 0x2U

socfpga_variant_t socfpga_platform_get_variant(void)
{
#ifdef AGILEX3
    return PLAT_AGILEX3;
#else
    uint32_t reg_val = RD_REG32(SYS_MNGR_BASE_ADDR + SYS_MNGR_BOOT_SCRATCH_POR1);

    if ((reg_val & ALT_SYSMGR_SCRATCH_REG_POR_1_REVA_WORKAROUND_MASK) != 0U)
    {
        return PLAT_AGILEX5_REVA;
    }

    return PLAT_AGILEX5_REVB;
#endif
}
