/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for GIC driver
 */


#ifndef __ARM_GIC_H__
#define __ARM_GIC_H__

/**
 * @file arm_gic.h
 * @brief GIC driver APIs.
 */

#include <errno.h>
#include <stdint.h>

/**
 * @defgroup gic GIC
 * @ingroup drivers
 * @brief APIs for GIC interrupt controller.
 * @details This is the GIC driver implementation for SoC FPGA. It provides
 * APIs for configuring interrupt routing, groups, priorities, and types.
 * @{
 */

/**
 * @defgroup gic_macros Macros
 * @ingroup gic
 */

/** @addtogroup gic_macros
 * @{ */

#define REGISTER_SOCFPGA_DIST_BASE_ADDR      0x1D000000U
#define REGISTER_SOCFPGA_REDIS_BASE_ADDR     0x1D060000U

#define GICV3_GROUP0                        0U
#define GICV3_GROUP1_SECURE                 1U
#define GICV3_GROUP1_NON_SECURE             2U

#define GICV3_CONFIG_LEVEL                  0U
#define GICV3_CONFIG_EDGE                   2U
#define GICV3_ROUTE_MODE_ANY                0x80000000U
#define GICV3_ROUTE_MODE_COORDINATE         0U
/** @} */

/**
 * @defgroup gic_fns Functions
 * @ingroup gic
 */
/**
 * @brief Initialize and enable the GIC controller.
 *
 * @return
 * - 0 on success
 * - -ENODEV if the distributor base pointer is not available
 */
int gic_enable_gic(void);

/**
 * @brief Get the redistributor index for a CPU affinity.
 *
 * @param[in] affinity CPU affinity value.
 *
 * @return
 * - >= 0 on success (redistributor index)
 * - -ENODEV if the redistributor base pointer is not available
 * - -ENOENT if no redistributor matches the affinity
 */
int gic_get_redis_id(uint32_t affinity);

/**
 * @brief Wake up the redistributor.
 *
 * @param[in] idx Redistributor index.
 *
 * @return
 * - 0 on success
 * - -ENODEV if the redistributor base pointer is not available
 */
int gic_wakeup_redis(uint32_t idx);

/**
 * @brief Set interrupt priority.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for SGI/PPI IDs.
 * @param[in] priority Interrupt priority.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the priority is invalid
 * - -ERANGE if the interrupt ID or redistributor index is invalid
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_set_int_priority(uint32_t id, uint32_t idx, uint8_t priority);

/**
 * @brief Set interrupt group.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for SGI/PPI IDs.
 * @param[in] security Interrupt group setting.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the security setting is invalid
 * - -ERANGE if the interrupt ID or redistributor index is invalid
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_set_int_group(uint32_t id, uint32_t idx, uint32_t security);

/**
 * @brief Set interrupt routing.
 *
 * @param[in] id Interrupt ID.
 * @param[in] mode Routing mode.
 * @param[in] affinity Affinity coordinate of target.
 *
 * @return
 * - 0 on success
 * - -ERANGE if the interrupt ID is out of range
 * - -ENODEV if the distributor base pointer is not available
 */
int gic_set_int_route(uint32_t id, uint32_t mode, uint32_t affinity);

/**
 * @brief Enable an interrupt.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for SGI/PPI IDs.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the interrupt ID is unsupported
 * - -ERANGE if the interrupt ID or redistributor index is invalid
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_enable_int(uint32_t id, uint32_t idx);

/**
 * @brief Disable an interrupt.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for SGI/PPI IDs.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the interrupt ID is unsupported
 * - -ERANGE if the interrupt ID or redistributor index is invalid
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_disable_int(uint32_t id, uint32_t idx);

/**
 * @brief Set interrupt trigger type.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for PPI IDs.
 * @param[in] type Trigger type.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the interrupt ID is unsupported
 * - -ERANGE if the interrupt ID is out of range
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_set_int_type(uint32_t id, uint32_t idx, uint32_t type);

/**
 * @brief Clear interrupt pending state.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for SGI/PPI IDs.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the interrupt ID is unsupported
 * - -ERANGE if the interrupt ID or redistributor index is invalid
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_clear_int_pending(uint32_t id, uint32_t idx);

/**
 * @brief Set interrupt pending state.
 *
 * @param[in] id Interrupt ID.
 * @param[in] idx Redistributor index for SGI/PPI IDs.
 *
 * @return
 * - 0 on success
 * - -EINVAL if the interrupt ID is unsupported
 * - -ERANGE if the interrupt ID or redistributor index is invalid
 * - -ENODEV if the GIC base pointers are not available
 */
int gic_set_int_pending(uint32_t id, uint32_t idx);

/**
 * @brief Enable interrupts at the CPU interface.
 */
void gic_enable_interrupts(void);

/** @} */
#endif /* __ARM_GIC_H__ */
