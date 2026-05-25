/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 */

#ifndef __SOCFPGA_PLATFORM_H__
#define __SOCFPGA_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PLAT_AGILEX3 = 0,
    PLAT_AGILEX5_REVA,
    PLAT_AGILEX5_REVB,
} socfpga_variant_t;

/*
 * Returns the SoC/silicon variant based on build target and silicon revision.
 *
 * Policy:
 * - AGILEX3 builds always report PLAT_AGILEX3.
 * - Non-AGILEX3 builds report Agilex5 revision based on SYS_MNGR scratch.
 */
socfpga_variant_t socfpga_platform_get_variant(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOCFPGA_PLATFORM_H__ */
