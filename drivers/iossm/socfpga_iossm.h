/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for iossm HAL driver
 */

#ifndef __SOCFPGA_IOSSM_H__
#define __SOCFPGA_IOSSM_H__

/**
 * @file socfpga_iossm.h
 * @brief Header file for IOSSM HAL driver
 *
 */

/**
 * @defgroup iossm IOSSM
 * @brief APIs for SoC FPGA IOSSM driver.
 * @details
 * IOSSM is a ECC engine for DDR error detection and correction.
 * It provides APIs to read ECC status,inject single and double
 * bit errors, acknowledge interrupts, andset callbacks
 * for error handling. For example usage,
 * see @ref iossm_sample "IOSSM sample application".
 * @ingroup drivers
 * @{
 */
/**
 * @defgroup iossm_fns Functions
 * @ingroup iossm
 * IOSSM HAL APIs
 */

/**
 * @defgroup iossm_structs Structures
 * @ingroup iossm
 * IOSSM Specific Structures
 */

/**
 * @defgroup iossm_enums Enumerations
 * @ingroup iossm
 * IOSSM Specific Enumerations
 */

/**
 * @defgroup iossm_macros Macros
 * @ingroup iossm
 * IOSSM Specific Macros
 */

/**
 * @addtogroup iossm_macros
 * @{
 */

#include <stdint.h>
#include <stdbool.h>

#define IOSSM_ECC_CRCT_EVENT_DET          (1U << 0U)   /*!< Single ECC correctable event detected. */
#define IOSSM_MUL_ECC_CRCT_EVENT_DET      (1U << 1U)   /*!< Multiple ECC correctable events detected. */
#define IOSSM_ECC_UNCRCT_EVENT_DET        (1U << 2U)   /*!< Single ECC uncorrectable event detected. */
#define IOSSM_MUL_ECC_UNCRCT_EVENT_DET    (1U << 3U)   /*!< Multiple ECC uncorrectable events detected. */
#define IOSSM_ECC_WRT_BACK_ERROR          (1U << 6U)   /*!< ECC write-back error occurred. */
#define IOSSM_SCRUB_OP_OK                 (1U << 7U)   /*!< Scrub operation completed successfully. */
#define IOSSM_ECC_CRCT_SCRB_EVENT_DET     (1U << 8U)   /*!< ECC correctable error detected during scrub operation. */
#define IOSSM_WR_ECC_SIN_BIT              (1U << 9U)   /*!< Single-bit ECC error during write. */
#define IOSSM_WR_ECC_DBL_BIT              (1U << 10U)  /*!< Double-bit ECC error during write. */
#define IOSSM_RD_ECC_SIN_BIT              (1U << 11U)  /*!< Single-bit ECC error during read. */
#define IOSSM_RD_ECC_DBL_BIT              (1U << 12U)  /*!< Double-bit ECC error during read. */
#define IOSSM_RMW_ECC_DBL_BIT             (1U << 13U)  /*!< Double-bit ECC error during read-modify-write. */

#define IOSSM_INSTANCE_OFFSET    (0x400000U)           /*!< Offset between IOSSM hardware instances. */
#define IOSSM_SINGLE_BIT_ERR     (0U)                  /*!< Indicator for single-bit ECC error. */
#define IOSSM_DOUBLE_BIT_ERR     (1U)                  /*!< Indicator for double-bit ECC error. */

#define IOSSM_OP_SUCCESS    0U               /*!< IOSSM operation completed successfully*/
#define IOSSM_OP_FAILED     1U               /*!< IOSSM operation failed*/

#define IOSSM_INSTANCE_0      0U                /*!< Identifier for IOSSM instance 0. */
#define IOSSM_INSTANCE_1      1U                /*!< Identifier for IOSSM instance 1. */
#define IOSSM_MAX_INSTANCE    2U                /*!< Maximum number of supported IOSSM instances. */

/**
 * @}
 */

/**
 * @brief  IOSSM callback function type
 * @ingroup iossm_fns
 */
typedef void (*iossm_cb_handler_t)(void);

/**
 * @addtogroup iossm_enums
 * @{
 */

/**
 * @brief IOSSM ECC modes
 */
typedef enum
{
    IOSSM_ECC_DI,              /*!< ECC disabled. */
    IOSSM_ECC_NO_TRIG,         /*!< ECC enabled but no trigger on error. */
    IOSSM_ECC_DETECT,          /*!< ECC enabled with error detection only. */
    IOSSM_ECC_DETECT_CORRECT   /*!< ECC enabled with error detection and correction. */
} iossm_ecc_mode_t;


/**
 * @brief IOSSM ECC types
 */
typedef enum
{
    IOSSM_OUT_OF_BAND, /*!< ECC is stored out-of-band (separately from the main data). */
    IOSSM_IN_LINE      /*!< ECC is stored in-line (alongside the main data). */
} iossm_ecc_type_t;

/**
 * @}
 */

/**
 * @addtogroup iossm_structs
 * @{
 */

/**
 * @brief IOSSM driver context information.
 */
typedef struct
{
    uint32_t iossm_base_addr;           /*!< Base address of the IOSSM instance. */
    uint8_t iossm_ip_type;              /*!< IP type identifier for the IOSSM instance. */
    uint8_t iossm_instance_id;          /*!< Unique instance ID of the IOSSM. */
    iossm_cb_handler_t iossm_cb_fun;    /*!< Callback function for IOSSM event handling. */
    bool iossm_instance_is_open;        /*!< Indicates whether the IOSSM instance is open (true) or closed (false). */
} iossm_context_t;

/**
 * @brief Hold error information reported by the IOSSM ECC handler.
 */
typedef struct
{
    uintptr_t iossm_err_info_ptr;   /*!< Pointer to additional error information or metadata (platform-dependent). */
    uint64_t iossm_err_addr;        /*!< Address at which the ECC error occurred. */
    uint64_t iossm_err_data;        /*!< Data value that caused or triggered the ECC error. */
    uint32_t iossm_err_dat_size;    /*!< Size of the erroneous data in bytes. */
    uint32_t iossm_err_id;          /*!< Identifier indicating the source or origin of the error. */
    uint32_t iossm_err_syndrome;    /*!< ECC syndrome value used for error detection and correction. */
    uint8_t iossm_err_type;         /*!< Type of ECC error (e.g., correctable, uncorrectable, single/double-bit). */
} err_info_t;

/** @} */

/**
 * @addtogroup iossm_fns
 * @{
 */

/**
 * @brief Used to initialize the iossm instance
 *     Once a instance is opened, it needs to be closed before invoking open again.
 *
 * @param[in] instance The iossm instance to open.
 *
 * @return
 * - iossm handle on success.
 * - NULL if Invalid iossm instance.
 * - NULL if same instance already opened.
 */
iossm_context_t *iossm_open(uint32_t instance);

/**
 * @brief Close the iossm instance.
 *
 * @param[in] instance Iossm instance to close.
 *
 * @return
 * - 0 on success.
 * - -EINVAL: if invalid instance id
 * - -EIO:    if the instance is not opened.
 */
int32_t iossm_close(uint32_t instance);

/**
 * @brief Read the ecc status.
 *
 * @param[in] xhandle iossm handle returned by open API.
 *
 * @return
 * - ecc status on success (number of errors).
 * - -EINVAL: if invalid instance id
 * - -EIO:    if the instance is not opened.
 */
int32_t iossm_read_ecc_status(const iossm_context_t *xhandle);

/**
 * @brief Mask the ecc interrupt on I096B.
 *
 * @param[in] xhandle    iossm handle returned by open API.
 * @param[in] interrupts interrupts to be masked.
 *
 * @return
 * - 0 on success
 * - -EINVAL: if iossm handle is NULL.
 * - -EIO:    if,
 *     - the instance is not opened.
 *     - if the response has failed.
 */
int32_t iossm_mask_int(const iossm_context_t *xhandle, uint32_t interrupts);

/**
 * @brief Inject single bit error.
 *
 * @param[in] xhandle iossm handle returned by open API.
 *
 * @return
 * - 0 on success
 * - -EINVAL: if iossm handle is NULL.
 * - -EIO:    if,
 *     - the instance is not opened.
 *     - if the response has failed.
 */
int32_t iossm_inject_sbit_err(const iossm_context_t *xhandle);

/**
 * @brief Inject double bit error.
 *
 * @param[in] xhandle iossm handle returned by open API.
 *
 * @return
 * - 0 on success
 * - -EINVAL: if iossm handle is NULL.
 * - -EIO:    if,
 *     - the instance is not opened.
 *     - if the response has failed.
 */
int32_t iossm_inject_dbit_err(const iossm_context_t *xhandle);

/**
 * @brief Acknowledge ecc interrupt status.
 *
 * @param[in] xhandle    iossm handle returned by open API.
 * @param[in] interrupts interrupts to be acknowledged.
 *
 * @return
 * - 0 on success
 * - -EINVAL: if iossm handle is NULL.
 * - -EIO:    if,
 *     - the instance is not opened.
 *     - if the response has failed.
 */
int32_t iossm_ack_int(const iossm_context_t *xhandle, uint32_t interrupts);

/**
 * @brief set iossm interrupt call back function.
 *
 * @param[in] xhandle       iossm handle returned by open API.
 * @param[in] call_back_fun Call back function pointer.
 *
 */
void iossm_set_callback(iossm_context_t *xhandle, iossm_cb_handler_t
        call_back_fun);


/**
 * @brief clear iossm error buffers
 *
 * @param[in] xhandle iossm handle returned by open API.
 *
 * @return
 * -  0 on success
 * - -EINVAL: if iossm handle is NULL.
 * - -EIO:    if,
 *     - the instance is not opened.
 *     - if the response has failed.
 */
int32_t iossm_clear_ecc_buffer(const iossm_context_t *xhandle);

/**
 * @brief get the error type from error index.
 *
 * @param[in] xhandle iossm handle returned by open API.
 * @param[in] index   index of the error (usually got from status register).
 *
 * @return
 * -  Error type on success
 * - -EINVAL: if iossm handle is NULL.
 * - -EIO:    if the instance is not opened.
 */
int32_t iossm_get_err_type(const iossm_context_t *xhandle, uint32_t index);

/**
 * @brief get the error address offset from error index.
 *
 * @param[in] xhandle iossm handle returned by open API.
 * @param[in] index   index of the error (usually got from status register)
 *
 */
int32_t iossm_get_err_addr_offset(const iossm_context_t *xhandle, uint32_t
        index);
/**
 * @}
 */
/** @} */
#endif /* __SOCFPGA_IOSSM_H__ */
