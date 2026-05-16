/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for DMA HAL driver
 */

#ifndef __SOCFPGA_DMA_H__
#define __SOCFPGA_DMA_H__

/**
 * @file socfpga_dma.h
 * @brief Header file for DMA HAL driver
 */

/**
 * @defgroup dma DMA
 * @brief APIs for the SoC FPGA DMA driver
 *
 * @details This driver provides methods to perform DMA operations.
 * The driver supports memory to memory DMA transfer and memory to
 * peripheral dma transfer. For example usage, refer to
 * @ref dma_sample "DMA sample application".
 * @ingroup drivers
 * @{
 */

/**
 * @defgroup dma_fns Functions
 * @ingroup dma
 * DMA HAL APIs
 */

/**
 * @defgroup dma_structs Structures
 * @ingroup dma
 * DMA Specific Structures
 */

/**
 * @defgroup dma_enums Enumerations
 * @ingroup dma
 * DMA Specific Enumerations
 */

/**
 * @defgroup dma_macros Macros
 * @ingroup dma
 * DMA Specific Macros
 */

/**
 * @addtogroup dma_macros
 * @{
 */

/**
 * @brief DMA Supported Instances
 */
#define DMA_INSTANCE0    0U /*!< DMA Instance 0 */
#define DMA_INSTANCE1    1U /*!< DMA Instance 1 */

/**
 * @brief DMA Channel IDs
 */
#define DMA_CH1    0U       /*!< DMA Channel 1 */
#define DMA_CH2    1U       /*!< DMA Channel 2 */
#define DMA_CH3    2U       /*!< DMA Channel 3 */
#define DMA_CH4    3U       /*!< DMA Channel 4 */
/**
 * @}
 */

/**
 * @addtogroup dma_structs
 * @{
 */
struct dma_ch_cntxt;

/**
 * @brief The handle type returned by calling dma_open().
 * This represents a DMA channel.
 * This is initialized in open() and returned to caller.
 * The caller must pass this pointer to the rest of APIs.
 */
typedef struct dma_ch_cntxt *dma_handle_t;
/**
 * @}
 */

/**
 * Function pointer for user callback. Accepts DMA handle as argument.
 * @ingroup dma_fns
 */
typedef void (*dma_callback_t)(void *usr_cntxt);

/**
 * @addtogroup dma_enums
 * @{
 */
/**
 * @brief DMA Supported Burst Transfer length for Source and destination
 */
typedef enum
{
    /*!< 1 data item per burst transfer */
    DMA_BURST_LEN_1 = 0,
    /*!< 4 data items per burst transfer */
    DMA_BURST_LEN_4,
    /*!< 8 data items per burst transfer */
    DMA_BURST_LEN_8,
    /*!< 16 data items per burst transfer */
    DMA_BURST_LEN_16,
    /*!< 32 data items per burst transfer */
    DMA_BURST_LEN_32,
    /*!< 64 data items per burst transfer */
    DMA_BURST_LEN_64,
    /*!< 128 data items per burst transfer */
    DMA_BURST_LEN_128,
    /*!< 256 data items per burst transfer */
    DMA_BURST_LEN_256,
    /*!< 512 data items per burst transfer */
    DMA_BURST_LEN_512,
    /*!< 1024 items per burst transfer */
    DMA_BURST_LEN_1024,
} dma_burst_len_t;

/**
 * @brief DMA Supported Transfer width for Source and destination
 */
typedef enum
{
    DMA_XFER_WIDTH1 = 0, /*!< Transfer width of 1 byte */
    DMA_XFER_WIDTH2, /*!< Transfer width of 2 bytes */
    DMA_XFER_WIDTH4, /*!< Transfer width of 4 bytes */
    DMA_XFER_WIDTH8, /*!< Transfer width of 8 bytes */
    DMA_XFER_WIDTH16, /*!< Transfer width of 16 bytes */
    DMA_XFER_WIDTH32, /*!< Transfer width of 32 bytes */
    DMA_XFER_WIDTH64, /*!< Transfer width of 64 bytes */
} dma_xfer_width_t;

/**
 * @brief DMA Multi-block transfer type
 */
typedef enum
{
    DMA_MULTI_BLK_CONTIGUOUS = 0, /*!< Multi-block contiguous transfer */
    DMA_MULTI_BLK_RELOAD, /*!< Multi-block reload transfer */
    DMA_MULTI_BLK_SHADOW_REG, /*!< Multi-block Shadow Register transfer */
    DMA_MULTI_BLK_LL, /*!< Multi-block linked list transfer */
} dma_multi_blk_xfer_type_t;

/**
 * @brief DMA supported transfer direction and flow controller options
 */
typedef enum
{
    DMA_MEM_TO_MEM_DMAC = 0, /*!< Memory to memory transfer, DMAC as flow controller */
    DMA_MEM_TO_PERI_DMAC, /*!< Memory to peripheral transfer, DMAC as flow controller */
    DMA_PERI_TO_MEM_DMAC, /*!< Peripheral to memory transfer, DMAC as flow controller */
    DMA_PERI_TO_PERI_DMAC, /*!< Peripheral to peripheral transfer, DMAC as flow controller */
    DMA_PERI_TO_MEM_SRC, /*!< Peripheral to memory transfer, source peripheral as flow controller */
    DMA_PERI_TO_PERI_SRC, /*!< Peripheral to peripheral transfer, source peripheral as flow controller */
    DMA_MEM_TO_PERI_DST, /*!< Memory to peripheral transfer, destination peripheral as flow controller */
    DMA_PERI_TO_PERI_DST, /*!< Peripheral to peripheral transfer, destination peripheral as flow controller */
} dma_xfer_type_t;

/**
 * @brief DMA Channel state
 */
typedef enum
{
    DMA_CH_IDLE, /*!< Channel is idle */
    DMA_CH_SUSPENDED, /*!< Channel is suspended */
    DMA_CH_ABORT, /*!< Channel is aborted */
    DMA_CH_ACTIVE, /*!< Channel is active */
} dma_ch_state_t;

/**
 * @brief DMA peripheral ID list
 */
typedef enum
{
    DMA_FPGA_PERI0, /*!< DMA peripheral ID for FPGA Peripheral 0 */
    DMA_FPGA_PERI1, /*!< DMA peripheral ID for FPGA Peripheral 1 */
    DMA_FPGA_PERI2, /*!< DMA peripheral ID for FPGA Peripheral 2 */
    DMA_FPGA_PERI3, /*!< DMA peripheral ID for FPGA Peripheral 3 */
    DMA_FPGA_PERI4, /*!< DMA peripheral ID for FPGA Peripheral 4 */
    DMA_FPGA_PERI5, /*!< DMA peripheral ID for FPGA Peripheral 5 */
    DMA_FPGA_PERI6, /*!< DMA peripheral ID for FPGA Peripheral 6 */
    DMA_FPGA_PERI7, /*!< DMA peripheral ID for FPGA Peripheral 7 */
    DMA_I2C0_TX, /*!< DMA peripheral ID for I2C instance 0 Tx */
    DMA_I2C0_RX, /*!< DMA peripheral ID for I2C instance 0 Rx */
    DMA_I2C1_TX, /*!< DMA peripheral ID for I2C instance 1 Tx */
    DMA_I2C1_RX, /*!< DMA peripheral ID for I2C instance 1 Rx */
    DMA_I2C_EMAC0_TX, /*!< DMA peripheral ID for EMAC instance 0 Tx */
    DMA_I2C_EMAC0_RX, /*!< DMA peripheral ID for EMAC instance 0 Rx */
    DMA_I2C_EMAC1_TX, /*!< DMA peripheral ID for EMAC instance 1 Tx */
    DMA_I2C_EMAC1_RX, /*!< DMA peripheral ID for EMAC instance 1 Rx */
    DMA_ID_SPI0_MASTER_TX, /*!< DMA peripheral ID for SPI instance 0 Master Tx */
    DMA_ID_SPI0_MASTER_RX, /*!< DMA peripheral ID for SPI instance 0 Master Rx */
    DMA_ID_SPI0_SLAVE_TX, /*!< DMA peripheral ID for SPI instance 0 Slave Tx */
    DMA_ID_SPI0_SLAVE_RX, /*!< DMA peripheral ID for SPI instance 0 Slave Rx */
    DMA_ID_SPI1_MASTER_TX, /*!< DMA peripheral ID for SPI instance 1 Master Tx */
    DMA_ID_SPI1_MASTER_RX, /*!< DMA peripheral ID for SPI instance 1 Master Rx */
    DMA_ID_SPI1_SLAVE_TX, /*!< DMA peripheral ID for SPI instance 1 Slave Tx */
    DMA_ID_SPI1_SLAVE_RX, /*!< DMA peripheral ID for SPI instance 1 Slave Rx */
    DMA_ID_STM = 26, /*!< DMA peripheral ID for System Trace Macrocell */
    DMA_ID_UART0_TX = 28, /*!< DMA peripheral ID for UART instance 0 Tx */
    DMA_ID_UART0_RX, /*!< DMA peripheral ID for UART instance 0 Rx */
    DMA_ID_UART1_TX, /*!< DMA peripheral ID for UART instance 1 Tx */
    DMA_ID_UART1_RX, /*!< DMA peripheral ID for UART instance 1 Rx */
    DMA_I2C_EMAC2_TX, /*!< DMA peripheral ID for EMAC instance 2 Tx */
    DMA_I2C_EMAC2_RX, /*!< DMA peripheral ID for EMAC instance 2 Rx */
    DMA_I3C0_TX = 35, /*!< DMA peripheral ID for I3C instance 0 Tx */
    DMA_I3C0_RX, /*!< DMA peripheral ID for I3C instance 0 Rx */
    DMA_I3C1_TX, /*!< DMA peripheral ID for I3C instance 1 Tx */
    DMA_I3C1_RX, /*!< DMA peripheral ID for I3C instance 1 Rx */
} dma_peri_id_t;
/**
 * @}
 */
/**
 * @addtogroup dma_structs
 * @{
 */
/**
 * @brief Configuration parameters for the DMA.
 *
 * The application will send the user configuration in the form of the
 * following structure in dma_config.
 */
typedef struct dma_cfg
{
    uint8_t instance; /*!< DMA controller instance number */
    dma_xfer_type_t ch_dir; /*!< DMA channel transfer direction */
    uint8_t ch_prio; /*!< DMA channel priority */
    dma_peri_id_t peri_id; /*!< Peripheral ID for the DMA channel */
    dma_callback_t callback; /*!< Callback function for DMA interrupts */
    void *usr_cntxt;
} dma_config_t;

/**
 * @brief Block transfer configuration parameters for the DMA.
 *
 * The application will send the block transfer configuration in the form of the
 * following structure in dma_setup_transfer.
 */
typedef struct dma_xfer_cfg
{
    uint64_t src; /*!< Source address for the DMA transfer */
    uint64_t dst; /*!< Destination address for the DMA transfer */
    uint32_t blk_size; /*!< Size of the block to be transferred in bytes */
    dma_burst_len_t src_burst_len; /*!< No of items read on each burst from source */
    dma_burst_len_t dst_burst_len; /*!< No of items written on each burst to destination */
    struct dma_xfer_cfg *next_xfer_cfg; /*!< Pointer to the next block transfer configuration */
} dma_xfer_cfg_t;
/**
 * @}
 */

/**
 * @addtogroup dma_fns
 * @{
 */
/**
 * @brief Initialize a DMA channel and return the handle to it
 *
 * The application should call this function to initialize the desired DMA channel.
 * The handle shall be passed while calling all other APIs
 *
 * @param[in] instance The desired DMA controller instance.
 *                     Select from the DmaInstance_t enum
 * @param[in] ch       The desired DMA channel instance.
 *                     Select from the DmaChannels_t enum
 *
 * @return
 * - 'the handle to the DMA channel (not NULL)', on success.
 * - 'NULL', if
 *      - invalid instance number
 *      - open same instance more than once before closing it
 *      - failed to enable the interrupt
 */
dma_handle_t dma_open(uint32_t instance, uint32_t ch);

/**
 * @brief Configure the DMA channel parameters
 *
 * This function configures channel properties that does not change every
 * transfer. That is properties like channel direction, peripheral id etc
 *
 * @param[in] hdma Handle to the channel returned by the Open()
 * @param[in] pcfg The channel configuration structure
 *
 * @return
 * - 0, on success
 * - -EINVAL: if hdma is NULL
 * - -EIO:    if the channel is not opened before invoking dma_config.
 * - -EBUSY:  if another transfer is in progress.
 *
 */
int32_t dma_config(dma_handle_t const hdma, dma_config_t *pcfg);

/**
 * @brief Setup a DMA data transfer
 *
 * This will setup a data transfer with given transfer parameters. It does
 * not start the data transfer.
 *
 * @param[in] hdma      Handle to the channel returned by the Open()
 * @param[in] xfer_list A linked list with transfer parameters of each
 *                      block in a multi-block transfer. For each block
 *                      the source address, destination address and
 *                      transfer size can be specified
 * @param[in] numxfers  The number of blocks in the linked list
 * @param[in] src_width  The source transfer width
 * @param[in] dst_width  The destination transfer width
 *
 * @return
 * - 0, on success
 * - -EINVAL: if hdma is NULL
 * - -EFAULT: if xfer_list is NULL or channel is not opened.
 * - -EBUSY:  if another transfer is in progress.
 *
 */
int32_t dma_setup_transfer(dma_handle_t const hdma, dma_xfer_cfg_t *xfer_list, uint32_t numxfers, dma_xfer_width_t src_width, dma_xfer_width_t dst_width);

/**
 * @brief Start the data transfer
 *
 * This will start the data transfer which is already set up by dma_setup_transfer.
 *
 * @param[in] hdma Handle to the channel returned by the Open()
 *
 * @return
 * - 0, on success
 * - -EINVAL: if hdma is NULL
 * - -EIO:    if channel is not open
 * - -EBUSY:  if another transfer is in progress.
 *
 */
int32_t dma_start_transfer(dma_handle_t const hdma);

/**
 * @brief Stop a data transfer in progress
 *
 * This will stop a data transfer which is in progress
 *
 * @param[in] hdma Handle to the channel returned by the Open()
 *
 * @return
 * - 0, on success
 * - -EINVAL: if hdma is NULL
 * - -EIO:    if no data transfer is in progress
 * - -EIO:    if it fails to stop the transfer
 */
int32_t dma_stop_transfer(dma_handle_t const hdma);


/**
 * @brief Close the dma channel
 *
 * This will close the dma channel
 *
 * @param[in] hdma Handle to the channel returned by the Open()
 *
 * @return
 * - 0: on success
 * - -EINVAL: if hdma is NULL
 */
int32_t dma_close(dma_handle_t const hdma);
/**
 * @}
 */
/**
 * @}
 */
#endif /* __SOCFPGA_DMA_H__ */
