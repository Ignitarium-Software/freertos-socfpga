/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SEU driver
 */

#ifndef __SOCFPGA_SEU_H__
#define __SOCFPGA_SEU_H__

/**
 * @file socfpga_seu.h
 * @brief Header file for SEU HAL driver
 */

/**
 * @defgroup seu SEU
 * @ingroup drivers
 * @brief APIs for SoC FPGA SEU driver.
 * @details
 * The SEU driver provides APIs to inject and read SEU-safe errors, read SEU statistics
 * and insert ECC errors. The driver communicates with the SIP SVC subsystem using the
 * mbox_send_command function to perform these operations.
 * To see example usage, see @ref seu_sample "SEU sample application".
 * @{
 */

/**
 * @defgroup seu_fns Functions
 * @ingroup seu
 * SEU HAL APIs
 */

/**
 * @defgroup seu_structs Structures
 * @ingroup seu
 * SEU Specific Structures
 */

#include <stdint.h>
#include <stdbool.h>

/**
 * @addtogroup seu_structs
 * @{
 *
 */

/**
 * @brief Seu error data parameters
 */
typedef struct
{
    uint8_t op_state;               /*!<seu output success/fail status*/
    uint8_t sector_addr;            /*!<Sector address at which the error was intentionally injected */
    uint8_t err_type;               /*!<type of the error injected*/
    uint16_t node_specific_status;  /*!<seu node specific status of the error injection*/
    uint32_t err_cnt;               /*!< no of errors injected*/
    bool correction_status;         /*!< Flag indicating if the injected error was corrected */

} read_err_data_t;

/**
 * @brief Seu parameters used to configure the error injection
 */
typedef struct
{
    uint8_t sector_addr;      /*!< Logical sector address for error injection */
    uint8_t cram_sel0;        /*!< CRAM select 0 for injection*/
    uint8_t cram_sel1;        /*!< CRAM select 1 for injection */
    uint8_t injection_cycle;  /*!< Cycle at which the error should be injected */
    uint8_t no_of_injection;  /*!< Number of errors to be injected */
} seu_err_params_t;

/**
 * @brief Seu safe error statistics
 */
typedef struct
{
    uint8_t op_state;                /*!< seu output success/fail status */
    uint32_t t_seu_cycle;            /*!< The time between the start of the most recent SEU detection cycle to the previous cycle*/
    uint32_t t_seu_detect;           /*!< The most recent time spent in performing the SEU detection within a SEU cycle*/
    uint32_t t_seu_correct;          /*!< The most recent time spent in performing the SEU correction*/
    uint32_t t_seu_inject_detect;    /*!< The most recent duration from the SEU error being injected to the time the SEU is detected.*/
    uint32_t t_sdm_seu_poll_interval;/*!< The time for SDM to toggle the SEU_ERROR pin after SDM detects error when polling the SEU error*/
} seu_stat_t;
/**
 * @}
 */

/**
 * @addtogroup seu_fns
 * @{
 */

/**
 * @brief SEU callback function type
 *
 */
typedef void (*seu_call_back_t)(void);

/**
 * @brief Inject an SEU-safe error.
 *
 * @param[in] err_params Parameters used to configure the SEU error injection.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if parameters are invalid
 * - -EIO:    if the mailbox client handle is invalid or the mailbox callback fails
 */
int32_t seu_insert_safe_err(seu_err_params_t err_params);

/**
 * @brief Set the SEU callback function.
 *
 * @param[in] call_back Callback function to be registered.
 */
void seu_set_call_back(seu_call_back_t call_back);

/**
 * @brief Initialize the SEU module.
 *
 * @return
 * -  0: on success
 * - -EIO:  if opening the mailbox client fails
 */
int32_t seu_init(void);

/**
 * @brief Read SEU safe error data.
 *
 * @return
 * - A read_err_data_t struct with op_state set to 0 on success
 * - -EINVAL: if the mailbox client handle is invalid
 * - -EIO:    if the mailbox callback reception fails
 */
read_err_data_t seu_read_err(void);

/**
 * @brief Read SEU safe error statistics.
 *
 * @param[in] sec_addr SEU sector address.
 *
 * @return
 * - A seu_stat handle with op_state set to 0 on success
 * - -EINVAL: if the mailbox client handle is invalid
 * - -EIO:    if receiving the mailbox callback fails
 */
seu_stat_t seu_read_stat(uint8_t sec_addr);

/**
 * @brief Inject an ECC error.
 *
 * @param[in] err_type     Type of ECC error to inject.
 * @param[in] ram_id       ID of the RAM where the error should be injected.
 * @param[in] sector_addr  Address of the sector to inject the error into.
 *
 * @return
 * - A read_data handle with op_state set to 0 on success
 * - -EINVAL: if parameters are invalid or the mailbox client handle is invalid
 * - -EIO:    if receiving the mailbox callback fails
 */

int32_t seu_insert_ecc_err(uint8_t err_type, uint8_t ram_id, uint8_t
        sector_addr);

/**
 * @brief De-initialize the SEU module.
 *
 * @return
 * - 0: on success
 * - -EIO: if closing the mailbox client fails
 */
int32_t seu_deinit(void);
/**
 * @}
 */
/* end of group seu_fns */

/**
 * @}
 */
/* end of group seu */

#endif
