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
#define SDMMC_CMD_SWITCH                (6U)     /*!< SDMMC switch bus width command */
#define SDMMC_CMD_SELECT_CARD           (7U)     /*!< SDMMC select card command */
#define SDMMC_CMD_SEND_IF_COND          (8U)     /*!< SDMMC send IF condition command */
#define SDMMC_CMD_SEND_EXT_CSD          (8U)     /*!< SDMMC send EXT CSD command */
#define SDMMC_CMD_READ_CID              (9U)     /*!< SDMMC read CID command */
#define SDMMC_CMD_SEND_CSD              (9U)     /*!< SDMMC send CSD command */
#define SDMMC_CMD_STOP_TRANSMISSION     (12U)     /*!< SDMMC stop xfer command */
#define SDMMC_CMD_SET_BLOCK_LEN         (16U)     /*!< SDMMC set block length command */
#define SDMMC_CMD_READ_SINGLE_BLOCK     (17U)     /*!< SDMMC read single block command */
#define SDMMC_CMD_READ_MULT_BLOCK       (18U)     /*!< SDMMC read multi block command */
#define SDMMC_CMD_WRITE_SINGLE_BLOCK    (24U)     /*!< SDMMC write single block command */
#define SDMMC_CMD_WRITE_MULT_BLOCK      (25U)     /*!< SDMMC write multi block command */
#define SDMMC_CMD_READ_OCR              (41U)     /*!< SDMMC read OCR command */
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

/**
 * @brief The SDMMC response type as defined by the protocol.
 */
#define SDMMC_NO_RESPONSE            (0U)        /*!< Used for no response commands*/
#define SDMMC_LONG_RESPONSE          (1U)        /*!< Used for long response commands*/
#define SDMMC_SHORT_RESPONSE         (2U)        /*!< Used for short response commands*/
#define SDMMC_SHORT_RESPONSE_BUSY    (3U)        /*!< Used for short response with busy flag commands*/

/**
 * @brief generic macros to support the driver.
 */
#define SDMMC_DATA_XFER_NOT_PST    (0U)            /*!< Used for commands with no data xfer*/
#define SDMMC_CMD_ID_CHECK_DI      (0U)          /*!< Used for responses with no command id*/
#define SDMMC_CMD_CRC_CHECK_DI     (0U)          /*!< Used for responses with no crc*/
#define SDMMC_DATA_XFER_PST        (1U)            /*!< Used for commands with data xfer*/
#define SDMMC_IS_CARD_EMMC         (1U)          /*!< Check if the device is eMMC*/
#define SDMMC_CMD_CRC_CHECK_EN     (1U)          /*!< Used for responses with crc*/
#define SDMMC_CMD_ID_CHECK_EN      (1U)          /*!< Used for responses with cmd id*/
#define SDMMC_REL_CARD_ADDRESS     (1U)          /*!< Default relative card address*/
#define SDMMC_SINGLE_BLOCK         (1U)          /*!< Used for single block transaction*/
#define SDMMC_EXT_CSD_SEC_NUM      (212U)          /*!< Used to extract number of sectors from csd */
#define SDMMC_BLOCK_SIZE           (512U)          /*!< Size of block for each transaction*/

/**
 * @brief Types of interrupts raised by the host controller.
 */
#define SDMMC_CMD_CPT_INT_LOG        (0x1U)        /*!< Command completion interrupt*/
#define SDMMC_XFER_CPT_INT_LOG       (0x2U)          /*!< Transfer complete interrupt*/
#define SDMMC_CMD_TIMOUT_INT_LOG     (0x18000U)        /*!< Command timeout interrupt*/
#define SDMMC_IS_CARD_DET            (1U)     /*!< Check the card detection state*/
#define SDMMC_XFER_TIMOUT_INT_LOG    (0x108000U)          /*!< transfer timeout interrupt*/
/**
 * @}
 */
/* end of group sdmmc_macros */


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
 * @brief Performs a single or multi-block read from a specified address.
 * @warning If the input handle is invalid, this function silently takes no action.
 *
 * @param[in] pread_buffer     Pointer to the buffer where the read data will be stored.
 * @param[in] read_addr        The SD/eMMC address from which the data should be read.
 * @param[in] block_size       The size (in bytes) of each block to be read.
 * @param[in] number_of_blocks The number of blocks to read from the card.
 *
 * @return
 * - 0:       Read operation was successful.
 * - -EIO:    Read operation failed.
 * - -EIO:    Command execution failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_read_block_sync(uint64_t *pread_buffer, uint64_t read_addr,
        uint32_t block_size, uint32_t number_of_blocks);

/**
 * @brief Performs a single or multi-block write to a specified address.
 * @warning If the input handle is invalid, this function silently takes no action.
 *
 * @param[in] pwrite_buffer    Pointer to the buffer containing the data to be written.
 * @param[in] write_addr       The SD/eMMC address to which the data should be written.
 * @param[in] block_size       The size (in bytes) of each block to be written.
 * @param[in] number_of_blocks The number of blocks to write to the card.
 *
 * @return
 * -  0:      Write operation was successful.
 * - -EIO:    Write operation failed.
 * - -EIO:    Command execution failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_write_block_sync(uint64_t *pwrite_buffer, uint64_t write_addr,
        uint32_t block_size, uint32_t number_of_blocks);

/**
 * @brief Performs a single or multi-block read from a specified address.
 * @warning If the input handle is invalid, this function silently takes no action.
 *
 * @param[in] pread_buffer        Pointer to the buffer where the read data will be stored.
 * @param[in] read_addr           The SD/eMMC address from which the data should be read.
 * @param[in] block_size          The size (in bytes) of each block to be read.
 * @param[in] number_of_blocks    The number of blocks to read from the card.
 * @param[in] xfer_done_call_back Callback function to be triggered once the transfer is complete.
 *
 * @return
 * -  0:      Read operation was successful.
 * - -EIO:    Read operation failed.
 * - -EIO:    Command execution failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_read_block_async(uint64_t *pread_buffer, uint64_t read_addr,
        uint32_t block_size, uint32_t number_of_blocks, sdmmc_cb_fun
        xfer_done_call_back);

/**
 * @brief Performs a single or multi-block write to a specified address.
 * @warning If the input handle is invalid, this function silently takes no action.
 *
 * @param[in] pwrite_buffer    Pointer to the buffer containing the data to be written.
 * @param[in] write_addr       The SD/eMMC address to which the data should be written.
 * @param[in] block_size       The size (in bytes) of each block to be written.
 * @param[in] number_of_blocks The number of blocks to write to the card.
 * @param[in] xfer_done_call_back        Callback function to be triggered once the transfer is complete.
 *
 * @return
 * -  0:      Write operation was successful.
 * - -EIO:    Write operation failed.
 * - -EIO:    Command execution failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_write_block_async(uint64_t *pwrite_buffer, uint64_t write_addr,
        uint32_t block_size, uint32_t number_of_blocks, sdmmc_cb_fun
        xfer_done_call_back);

/**
 * @brief Performs the initialization sequence on the card.
 * @warning If the input handle is invalid, this function silently takes no action.
 *
 * @param[in] ptr_sec_num Pointer to the variable where the sector number will be stored.
 *
 * @return
 * - 0:       Card initialization was successful.
 * - -EIO:    Card initialization failed.
 * - -EINVAL: One or more arguments are invalid.
 */
int32_t sdmmc_init_card(uint64_t *ptr_sec_num);

/**
 * @brief Checks whether a card is detected.
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
