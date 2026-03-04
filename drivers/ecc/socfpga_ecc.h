/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for ecc HAL driver
 */

 #ifndef __SOCFPGA_ECC_H__
 #define __SOCFPGA_ECC_H__

#include <stdint.h>

/**
 * @file socfpga_ecc.h
 * @brief Header file for ecc HAL driver
 *
 */

/**
 * @defgroup ecc ECC
 * @ingroup drivers
 *  @details
 * The ECC driver provides APIs to perform Single bit or Double bit error injection
 * in a specified ECC module. Once a single bit error is injected, an interrupt is
 * raised and the error is processed. The user may register a callback function to be
 * performed after processing a single bit error.
 *
 * For a double bit error, the user can see if the error has been injected. If a double
 * bit error is present, it is processed.
 * To see example usage, see @ref ecc_sample "ECC Sample Application".
 * @{
 */

/**
 * @defgroup ecc_fns Functions
 * @ingroup ECC
 * ECC HAL APIs
 */

/**
 * @defgroup ecc_macros Macros
 * @ingroup ECC
 * ECC Specific Macros
 */

/**
 * @addtogroup ecc_macros
 * @{
 */

#define ECC_OCRAM        1U          /*!< ECC OCRAM module id. */
#define ECC_USB0_RAM0    2U          /*!< ECC USB0 RAM0 module id. */
#define ECC_USB1_RAM2    3U          /*!< ECC USB1 RAM2 module id. */
#define ECC_EMAC0_RX     4U          /*!< ECC EMAC0 RX module id. */
#define ECC_EMAC0_TX     5U          /*!< ECC EMAC0 TX module id. */
#define ECC_EMAC1_RX     6U          /*!< ECC EMAC1 RX module id. */
#define ECC_EMAC1_TX     7U          /*!< ECC EMAC1 TX module id. */
#define ECC_EMAC2_RX     8U          /*!< ECC EMAC2 RX module id. */
#define ECC_EMAC2_TX     9U          /*!< ECC EMAC2 TX module id. */
#define ECC_QSPI         10U         /*!< ECC QSPI module id. */
#define ECC_USB1_RAM1    11U         /*!< ECC USB1 RAM1 module id. */
#define ECC_USB1_RAM0    12U         /*!< ECC USB1 RAM0 module id. */

#define ECC_SINGLE_BIT_ERROR    1U   /*!< Single bit error type. */
#define ECC_DOUBLE_BIT_ERROR    2U   /*!< Double bit error type. */

/**
 * @}
 */

/**
 * @brief  ecc callback function type
 * @ingroup ecc_fns
 */
typedef void (*ecc_callback_t)(uint32_t error_type);

/**
 * @addtogroup ecc_fns
 * @{
 */

/**
 * @brief  Used to initialise ECC modules.
 *
 * @return
 * - 0 on success.
 * - -EIO if error in setting interrupt.
 */
int ecc_init(void);

/**
 * @brief  Used to inject single or double bit error.
 *
 * @param[in]  ecc_module - ECC module id.
 * @param[in]  error_type - Type of error to inject (single or double bit).
 *
 * @return
 * - 0:       on success.
 * - -EINVAL: if invalid parameters are passed.
 */
int ecc_inject_error(uint32_t ecc_module, uint32_t error_type);

/**
 * @brief  Used to enable ECC modules.
 *
 * The argument passed is an OR'ed bitmask of modules to be enabled
 *
 * @param[in]  modules - ECC module id bitmask.
 *
 * @return
 * - 0:       on success.
 * - -EINVAL: if invalid parameters are passed.
 * - -EIO:    if error in enabling a module.
 */
int ecc_enable_modules(uint32_t modules);
/**
 * @brief  Used to set callback after processing a single bit error
 *
 * @return
 * - 0:       on success.
 * - -EINVAL: if invalid parameters are passed.
 */
int ecc_set_callback(ecc_callback_t user_callback);

/**
 * @brief  Used to get single bit error count for a specified module.
 *
 * @param[in]  ecc_module - ECC module id.
 *
 * @return
 * - >= 0:    on success, returns the single bit error count.
 * - -EINVAL: if invalid parameters are passed.
 */
int ecc_get_sbe_error_count(uint32_t ecc_module);

/**
 * @brief  Used to get double bit error count for a specified module.
 *
 * @param[in]  ecc_module - ECC module id.
 *
 * @return
 * - >= 0:    on success, returns the single bit error count.
 * - -EINVAL: if invalid parameters are passed.
 */
int ecc_get_dbe_error_count(uint32_t ecc_module);

/**
 * @}
 */
/* end of group ecc_fns */

/**
 * @}
 */
/* end of group ecc */
#endif /* __SOCFPGA_ECC_H__ */
