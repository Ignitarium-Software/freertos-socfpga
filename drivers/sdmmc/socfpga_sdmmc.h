/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SDMMC HAL driver
 */


#ifndef __SOCFPGA_SDMMC_H__
#define __SOCFPGA_SDMMC_H__

/**
 * @file socfpga_sdmmc.h
 * @brief File for the HAL APIs of SDMMC called by application layer.
 *
 */

/**
 * @defgroup sdmmc SD/eMMC
 * @ingroup drivers
 * @brief APIs for SoC FPGA SD/eMMC driver.
 * @details
 * The SD/eMMC driver supports both High-Speed and Default-Speed modes
 * over the SD and eMMC protocols.
 *
 * SD cards support up to 25 MBps with a 4-bit data bus.
 * eMMC supports up to 50 MBps with an 8-bit data bus.
 *
 * To see example usage, see @ref sdmmc_rw_sample  "SDMMC Sample Application".
 * @{
 *
 */

/**
 * @defgroup sdmmc_fns Functions
 * @ingroup sdmmc
 * SDMMC HAL APIs
 */

/**
 * @defgroup sdmmc_structs Structures
 * @ingroup sdmmc
 * SDMMC Specific Structures
 */

/**
 * @defgroup sdmmc_enums Enumerations
 * @ingroup sdmmc
 * SDMMC Specific Enumerations
 */

/**
 * @defgroup sdmmc_macros Macros
 * @ingroup sdmmc
 * SDMMC Specific Macros
 */

/**
 * @addtogroup sdmmc_macros
 * @{
 */
/**
 * @brief The SDMMC command types as defined by the protocol.
 */
#define SDMMC_CMD_GO_IDLE_STATE         (0U)     /*!< SDMMC go idle state command */
#define SDMMC_CMD_CHECK_OCR             (1U)     /*!< SDMMC check OP COND REG command */
#define SDMMC_CMD_ALL_SEND_CID          (2U)     /*!< SDMMC send card ID command */
#define SDMMC_CMD_SND_REL_ADDR          (3U)     /*!< SDMMC send rel card address command */
#define SDMMC_CMD_SET_REL_ADD           (3U)     /*!< SDMMC set rel card address command */
#define SDMMC_CMD_SWITCH                (6U)     /*!< SDMMC switch function command (CMD6) */
#define SDMMC_CMD_SELECT_CARD           (7U)     /*!< SDMMC select card command */
#define SDMMC_CMD_SEND_IF_COND          (8U)     /*!< SDMMC send IF condition command */
#define SDMMC_CMD_SEND_EXT_CSD          (8U)     /*!< SDMMC send EXT CSD command */
#define SDMMC_CMD_READ_CID              (9U)     /*!< SDMMC read CID command */
#define SDMMC_CMD_SEND_CSD              (9U)     /*!< SDMMC send CSD command */
#define SDMMC_CMD_SEND_STATUS           (13U)     /*!< SDMMC send status command */
#define SDMMC_CMD_STOP_TRANSMISSION     (12U)     /*!< SDMMC stop xfer command */
#define SDMMC_CMD_VOLTAGE_SWITCH        (11U)     /*!< SDMMC voltage switch command */
#define SDMMC_CMD_SET_BLOCK_LEN         (16U)     /*!< SDMMC set block length command */
#define SDMMC_CMD_READ_SINGLE_BLOCK     (17U)     /*!< SDMMC read single block command */
#define SDMMC_CMD_READ_MULT_BLOCK       (18U)     /*!< SDMMC read multi block command */
#define SDMMC_CMD_WRITE_SINGLE_BLOCK    (24U)     /*!< SDMMC write single block command */
#define SDMMC_CMD_WRITE_MULT_BLOCK      (25U)     /*!< SDMMC write multi block command */
#define SDMMC_CMD_READ_OCR              (41U)     /*!< SDMMC read OCR command */
#define SDMMC_CMD_SEND_SCR              (51U)     /*!< SDMMC send SCR command */
#define SDMMC_CMD_SEND_APP              (55U)     /*!< SDMMC send application specific command */

/**
 * @brief The SDMMC arguments as defined by the protocol.
 */
#define SDMMC_NO_CMD_ARG           (0U)          /*!< No argument*/
#define SDMMC_SET_ADDR             (1U)          /*!< Argument to set relative address as 1*/
#define SDMMC_ARG_BUS_WIDTH        (2U)          /*!< Argument to set bus width as 4*/
#define SDMMC_ARG_MASK_REL_ADD     (16U)          /*!< Argument mask for relative card address*/
#define SDMMC_ARG_CHECK_PATTERN    (0x000001AAU)          /*!< Check pattern for echo back*/
#define SDMMC_ARG_SDHC_OCR         (0x40010000U)          /*!< Argument for voltage negotiation*/
#define SDMMC_SET_EXT_BUS_WIDTH    (0x03B70200U)          /*!< Argument to set the bus width as 8*/
#define SDMMC_ARG_SWITCH_HS        (0x80FFFFF1U)          /*!< CMD6 argument to switch to HS mode */
#define SDMMC_ARG_SWITCH_SDR12     (0x80FFFFF0U)          /*!< CMD6 argument to switch to SDR12 mode */
#define SDMMC_ARG_SWITCH_SDR25     (0x80FFFFF1U)          /*!< CMD6 argument to switch to SDR25 mode */
#define SDMMC_ARG_SWITCH_SDR50     (0x80FFFFF2U)          /*!< CMD6 argument to switch to SDR50 mode */
#define SDMMC_ARG_SWITCH_CHECK_GRP1 (0x00FFFFF0U)         /*!< CMD6 check argument for group1 support */
#define SDMMC_ARG_SWITCH_SET_GRP2_TYPE_B (0x80FFFF0FU)    /*!< CMD6 set argument for group2 driver strength type-B */
#define SDMMC_ARG_SWITCH_SET_GRP3_200MA  (0x80FFF0FFU)    /*!< CMD6 set argument for group3 current limit 200mA */
#define SDMMC_CMD8_VHS_CHECK_PATTERN      (0x1AAU)
#define SDMMC_CMD8_VHS_CHECK_MASK         (0xFFFU)
#define SDMMC_CCC_SWITCH_MASK             (1U << 10U)
#define SDMMC_SCR_BUS_WIDTH_4             (0x4U)

#define SDMMC_OCR_S18R_MASK        (1U << 24U)            /*!< ACMD41 S18R request bit */
#define SDMMC_OCR_S18A_MASK        (1U << 24U)            /*!< ACMD41 response S18A accept bit */
#define SDMMC_OCR_XPC_MASK         (1U << 28U)            /*!< ACMD41 XPC request bit */
#define SDMMC_OCR_2T_MASK          (1U << 29U)            /*!< ACMD41 2T request bit */
#define SDMMC_OCR_CCS_MASK         (1U << 30U)            /*!< ACMD41 CCS request bit */

#define SDMMC_R1_ERROR_MASK        (1U << 19U)            /*!< R1 generic error bit */

/**
 * @brief The SDMMC response type as defined by the protocol.
 */
#define SDMMC_NO_RESPONSE            (0U)        /*!< Used for no response commands*/
#define SDMMC_LONG_RESPONSE          (1U)        /*!< Used for long response commands*/
#define SDMMC_SHORT_RESPONSE         (2U)        /*!< Used for short response commands*/
#define SDMMC_SHORT_RESPONSE_BUSY    (3U)        /*!< Used for short response with busy flag commands*/

/**
 * @brief Generic command-field helper macros.
 */
#define SDMMC_DATA_XFER_NOT_PST    (0U)          /*!< Used for commands with no data xfer */
#define SDMMC_CMD_ID_CHECK_DI      (0U)          /*!< Used for responses with no command id */
#define SDMMC_CMD_CRC_CHECK_DI     (0U)          /*!< Used for responses with no crc */
#define SDMMC_DATA_XFER_PST        (1U)          /*!< Used for commands with data xfer */
#define SDMMC_CMD_CRC_CHECK_EN     (1U)          /*!< Used for responses with crc */
#define SDMMC_CMD_ID_CHECK_EN      (1U)          /*!< Used for responses with cmd id */
#define SDMMC_REL_CARD_ADDRESS     (1U)          /*!< Default relative card address*/
#define SDMMC_SINGLE_BLOCK         (1U)          /*!< Used for single block transaction*/
#define SDMMC_EXT_CSD_SEC_NUM      (212U)          /*!< Used to extract number of sectors from csd */
#define SDMMC_BLOCK_SIZE           (512U)          /*!< Size of block for each transaction*/
#define SDMMC_BUS_WIDTH_1          (1U)            /*!< 1-bit SD bus width */
#define SDMMC_BUS_WIDTH_4          (4U)            /*!< 4-bit SD bus width */
#define SDMMC_SWITCH_STATUS_SIZE   (64U)          /*!< CMD6 switch status size */
#define SDMMC_SCR_STATUS_SIZE      (8U)          /*!< ACMD51 SCR data size */
#define SDMMC_SWITCH_GRP1_SUP_IDX  (13U)          /*!< CMD6 group1 support status index */
#define SDMMC_SWITCH_GRP1_SEL_IDX  (16U)          /*!< CMD6 switch status group1 index */
#define SDMMC_SWITCH_GRP1_MASK     (0x0FU)          /*!< CMD6 group1 function mask */
#define SDMMC_SWITCH_GRP3_MASK     (0x0FU)          /*!< CMD6 group3 function mask */
#define SDMMC_SWITCH_GRP1_HS       (0x01U)          /*!< CMD6 group1 high speed select value */
#define SDMMC_SWITCH_GRP1_SDR12    (0x00U)          /*!< CMD6 group1 SDR12 select value */
#define SDMMC_SWITCH_GRP1_SDR25    (0x01U)          /*!< CMD6 group1 SDR25 select value */
#define SDMMC_SWITCH_GRP1_SDR50    (0x02U)          /*!< CMD6 group1 SDR50 select value */
#define SDMMC_SWITCH_GRP3_200MA    (0x00U)          /*!< CMD6 group3 200mA current limit */

#define SDMMC_SWITCH_SUP_SDR12     (1U << SDMMC_SWITCH_GRP1_SDR12)
#define SDMMC_SWITCH_SUP_SDR25     (1U << SDMMC_SWITCH_GRP1_SDR25)
#define SDMMC_SWITCH_SUP_SDR50     (1U << SDMMC_SWITCH_GRP1_SDR50)

#define SDMMC_SWITCH_CURR_200MA    (1U << 0U)  /*!< CMD6 card current-limit support 200mA */

#ifndef SDMMC_HOST_UHS_MODE_CAPS
#define SDMMC_HOST_UHS_MODE_CAPS    (SDMMC_SWITCH_SUP_SDR50 | SDMMC_SWITCH_SUP_SDR25 | SDMMC_SWITCH_SUP_SDR12)
#endif

/**
 * @brief Types of interrupts raised by the host controller.
 */
#define SDMMC_CMD_CPT_INT_LOG      (0x1U)      /*!< Command completion interrupt */
#define SDMMC_XFER_CPT_INT_LOG     (0x2U)      /*!< Transfer complete interrupt */
#define SDMMC_CMD_TIMOUT_INT_LOG   (0x18000U)  /*!< Command timeout interrupt */
#define SDMMC_IS_CARD_DET          (1U)        /*!< Check the card detection state */
#define SDMMC_XFER_TIMOUT_INT_LOG  (0x108000U) /*!< Transfer timeout interrupt */

/**
 * @}
 */
/* end of group sdmmc_macros */

/**
 * @addtogroup sdmmc_enums
 * @{
 */
typedef enum
{
    SDMMC_SPEED_MODE_DS = 0,
    SDMMC_SPEED_MODE_HS,
    SDMMC_SPEED_MODE_UHS_SDR12,
    SDMMC_SPEED_MODE_UHS_SDR25,
    SDMMC_SPEED_MODE_UHS_SDR50
} sdmmc_speed_mode_t;

/**
 * @}
 */
/* end of group sdmmc_enums */


/**
 * @brief SDMMC context structure
 * @ingroup sdmmc_structs
 */
struct sdmmc_context;

/**
 * @addtogroup sdmmc_fns
 * @{
 */

/**
 * @brief Callback function type
 *
 * @param[in] xfer_flag Flag indicating the type of transfer completion.
 */
typedef void (*sdmmc_cb_fun)(int32_t xfer_flag);

/**
 * @brief Perform a single or multi-block read from a specified address.
 *
 * @param[in] addr             The SD/eMMC addr from which the data should be read.
 * @param[in] buf              Pointer to the buffer where the read data will be stored.
 * @param[in] block_size       The size (in bytes) of each block to be read.
 * @param[in] nblocks          The number of blocks to read from the card.
 *
 * @return
 * - 0:       Read operation was successful.
 * - -EIO:    Read operation failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_read_block_sync(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks);

/**
 * @brief Perform a single or multi-block write to a specified address.
 *
 * @param[in] addr             The SD/eMMC addr to which the data should be written.
 * @param[in] buf              Pointer to the buffer containing the data to be written.
 * @param[in] block_size       The size (in bytes) of each block to be written.
 * @param[in] nblocks          The number of blocks to write to the card.
 *
 * @return
 * -  0:      Write operation was successful.
 * - -EIO:    Write operation failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_write_block_sync(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks);

/**
 * @brief Perform a single or multi-block read from a specified address.
 *
 * @param[in] addr                The SD/eMMC addr from which the data should be read.
 * @param[in] buf                 Pointer to the buffer where the read data will be stored.
 * @param[in] block_size          The size (in bytes) of each block to be read.
 * @param[in] nblocks             The number of blocks to read from the card.
 * @param[in] xfer_done_call_back Callback function to be triggered once the transfer is complete.
 *
 * @return
 * -  0:      Read operation was successful.
 * - -EIO:    Read operation failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_read_block_async(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks, sdmmc_cb_fun
        xfer_done_call_back);

/**
 * @brief Perform a single or multi-block write to a specified address.
 *
 * @param[in] addr             The SD/eMMC addr to which the data should be written.
 * @param[in] buf              Pointer to the buffer containing the data to be written.
 * @param[in] block_size       The size (in bytes) of each block to be written.
 * @param[in] nblocks          The number of blocks to write to the card.
 * @param[in] xfer_done_call_back        Callback function to be triggered once the transfer is complete.
 *
 * @return
 * -  0:      Write operation was successful.
 * - -EIO:    Write operation failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_write_block_async(uint64_t addr, uint64_t *buf,
        uint32_t block_size, uint32_t nblocks, sdmmc_cb_fun
        xfer_done_call_back);

/**
 * @brief Perform the initialization sequence on the card.
 *
 * @param[out] n_sector Pointer to the variable where the sector number will be stored.
 *
 * @return
 * - 0:       Card initialization was successful.
 * - -EIO:    Card initialization failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_init_card(uint64_t *n_sector);

/**
 * @brief Check whether a card is detected.
 *
 * @return
 * - 1: Card is detected.
 * - 0: Card is not detected.
 */
uint32_t sdmmc_is_card_present(void);

/**
 * @}
 */
/* end of group sdmmc fns */

/**
 * @}
 */
/* end of group sdmmc */

#endif/* __SOCFPGA_SDMMC__ */
