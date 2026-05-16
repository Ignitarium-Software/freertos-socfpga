/*
 * Common IO - basic V1.0.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Copyright (C) 2025-2026 Altera Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 *
 * Modified for SoC FPGA
 */

#ifndef __SOCFPGA_SPI_H__
#define __SOCFPGA_SPI_H__

/**
 * @file socfpga_spi.h
 * @brief SoC FPGA SPI HAL driver
 */

/* Standard includes. */
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include "socfpga_dma.h"

/**
 * @defgroup spi SPI
 * @ingroup drivers
 * @brief APIs for SoC FPGA SPI driver.
 * @details
 * This is the SPI driver implementation for SoC FPGA.
 * It supports master and slave mode operation.
 * It provides APIs for data transfer with an SPI peer.
 * The APIs are designed to be used in both synchronous and asynchronous modes.
 * To see example usage, see @ref spi_sample "SPI sample application".
 * @{
 */

/**
 * @defgroup spi_fns Functions
 * @ingroup spi
 * SPI HAL APIs
 */

/**
 * @defgroup spi_structs Structures
 * @ingroup spi
 * SPI Specific Structures
 */

/**
 * @defgroup spi_enums Enumerations
 * @ingroup spi
 * SPI Specific Enumerations
 */

/**
 * @brief The SPI return status from Async operations.
 * @ingroup spi_enums
 */
typedef enum
{
    SPI_SUCCESS,        /*!< SPI operation completed successfully. */
    SPI_WR_ERROR,       /*!< Write operation error. */
    SPI_RD_ERROR,       /*!< Read operation error. */
    SPI_XFER_ERROR,     /*!< Full-duplex transfer error. */
    SPI_RX_OVERFLOW,    /*!< Slave RX FIFO overflow; bytes were lost (RXOI). */
} spi_xfer_status_t;

/**
 * @brief The SPI Modes denoting the clock polarity
 * and clock phase.
 * @ingroup spi_enums
 */
typedef enum
{
    SPI_MODE0, /*!< CPOL = 0 and CPHA = 0 */
    SPI_MODE1, /*!< CPOL = 0 and CPHA = 1 */
    SPI_MODE2, /*!< CPOL = 1 and CPHA = 0 */
    SPI_MODE3, /*!<CPOL = 1 and CPHA = 1 */
} spi_mode_t;

/**
 * @brief SPI controller role (master/slave).
 * @ingroup spi_enums
 */
typedef enum
{
    SPI_ROLE_MASTER = 0,
    SPI_ROLE_SLAVE = 1,
} spi_role_t;

/**
 * @brief Ioctl request for SPI HAL.
 * @ingroup spi_enums
 */
typedef enum
{
    SPI_SET_CONFIG,     /*!< Configure SPI using spi_cfg_t (input). */
    SPI_GET_CONFIG,     /*!< Read current config into spi_cfg_t (output). */
    SPI_GET_TX_NBYTES,  /*!< Get last TX byte count (uint16_t*, output). */
    SPI_GET_RX_NBYTES,  /*!< Get last RX byte count (uint16_t*, output). */
    SPI_ENABLE_DMA,     /*!< Enable DMA using spi_dma_config_t (input). */
    SPI_DISABLE_DMA,  /*!< Disable DMA mode; buf is ignored (pass NULL). */
} spi_ioctl_t;

/**
 * @addtogroup spi_structs
 * @{
 */
/**
 * @brief The configuration parameters for the SPI controller.
 *
 * @details
 * The application configures a handle using @ref spi_ioctl with
 * @ref SPI_SET_CONFIG.
 */
typedef struct
{
    uint32_t clk;      /*!< SPI frequency set for data transmission in Hz. */
    spi_mode_t mode; /*!< Mode selected as per enum spi_mode_t. */
    spi_role_t role;  /*!< SPI role: master or slave. */
} spi_cfg_t;

/**
 * @brief DMA configuration for SPI master transfers.
 *
 * @details
 * Used with @ref SPI_ENABLE_DMA.
 * The driver opens and configures two DMA channels:
 * - TX: memory to SPI (DMAC flow controller)
 * - RX: SPI to memory (DMAC flow controller)
 *
 * @note tx_prio/rx_prio are forwarded to the DMA driver as channel priority.
 * As of today, the DMA driver does not apply this field when programming the
 * DMAC registers.
 *
 * @note For SPI, RX DMA is sensitive to burst/threshold programming.
 * The driver selects burst length automatically per transfer based on nbytes,
 * with a maximum of @ref DMA_BURST_LEN_8.
 */
typedef struct
{
    uint32_t tx_instance; /*!< TX DMA instance (DMA_INSTANCE0/DMA_INSTANCE1). */
    uint32_t tx_channel;  /*!< DMA channel for TX (DMA_CH1..DMA_CH4). */
    uint8_t tx_prio;      /*!< TX DMA channel priority. */
    uint32_t rx_instance; /*!< RX DMA instance (DMA_INSTANCE0/DMA_INSTANCE1). */
    uint32_t rx_channel;  /*!< DMA channel for RX (DMA_CH1..DMA_CH4). */
    uint8_t rx_prio;      /*!< RX DMA channel priority. */
} spi_dma_config_t;

/**
 * @brief The SPI descriptor type defined in the source file.
 */
struct spi_handle;

/**
 * @brief spi_handle_t is the handle type returned by calling spi_open().
 * This is initialized in open and returned to caller. The caller must pass
 * this pointer to the rest of APIs.
 */
typedef struct spi_handle *spi_handle_t;
/**
 * @}
 */

/**
 * @brief The callback function for completion of SPI operation.
 * @ingroup spi_fns
 *
 * @param[in] status The status of the SPI operation.
 * @param[in] pparam The user context passed when setting the callback.
 */
typedef void (*spi_callback_t)(spi_xfer_status_t status, void *pparam);

/**
 * @addtogroup spi_fns
 * @{
 */
/**
 * @brief Initializes SPI master peripheral with default configuration.
 *
 * @warning Once opened, the same SPI instance must be closed before calling
 *          open again.
 *
 * @param[in] instance The instance of the SPI driver to initialize.
 *
 * @return
 * - 'the handle to the SPI port (not NULL)', on success.
 * - 'NULL', if
 *     - invalid instance number
 *     - open same instance more than once before closing it
 */
spi_handle_t spi_open(uint32_t instance);

/**
 * @brief Initializes SPI slave peripheral.
 *
 * @warning Once opened, the same SPI instance must be closed before calling
 *          open again.
 *
 * @param[in] instance The instance of the SPI driver to initialize.
 *
 * @return
 * - 'the handle to the SPI port (not NULL)', on success.
 * - 'NULL', if
 *     - invalid instance number
 *     - open same instance more than once before closing it
 */
spi_handle_t spi_slave_open(uint32_t instance);

/**
 * @brief Sets the application callback to be called on completion of an
 *        operation.
 *
 * The callback is invoked for asynchronous transfers started by
 * @ref spi_xfer_async.
 *
 * In slave mode, the callback may also be invoked to report RX overflow
 * (@ref SPI_RX_OVERFLOW).
 *
 * @note This callback will not be invoked when synchronous operation completes.
 * @note This callback is per handle. Each instance has its own callback.
 * @note Newly set callback overrides the one previously set.
 * @note Callback is invoked from interrupt context (SPI ISR / DMA ISR path).
 *       The callback must not call blocking APIs and must use only ISR-safe
 *       OSAL APIs.
 *
 * @param[in] hspi The SPI peripheral handle returned in the open() call.
 * @param[in] callback The callback to call on completion of operation.
 * @param[in] pcntxt The user context to be passed back when callback is called.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if
 *     - hspi is NULL
 * - -EBUSY:  if the bus is busy
 */
int32_t spi_set_callback(spi_handle_t const hspi, spi_callback_t callback,
        void *pcntxt);

/**
 * @brief Issues an ioctl request on the SPI handle.
 *
 *
 * @note SPI_SET_CONFIG: Set the SPI controller configuration.
 * buf points to a spi_cfg_t provided by the caller (driver reads it).
 *
 * @note SPI_GET_CONFIG: Get the current SPI controller configuration.
 * buf points to a spi_cfg_t provided by the caller (driver populates it).
 *
 * @note SPI_GET_TX_NBYTES: Get the number of bytes written by the last
 * operation. Call this in the caller task (sync) or in the application
 * callback (async), right after the last operation completes.
 * buf points to a uint16_t provided by the caller (driver populates it).
 * The value is limited to 65535 bytes (the maximum transfer size supported by
 * the current API).
 *
 * - If the last operation did not use a TX buffer (txbuf was NULL), this
 *   returns 0.
 * - Otherwise, this returns the number of bytes transmitted.
 *
 * @note SPI_GET_RX_NBYTES: Get the number of bytes read by the last
 * operation. Call this in the caller task (sync) or in the application
 * callback (async), right after the last operation completes.
 * buf points to a uint16_t provided by the caller (driver populates it).
 * The value is limited to 65535 bytes (the maximum transfer size supported by
 * the current API).
 *
 * - If the last operation did not use an RX buffer (rxbuf was NULL), this
 *   returns 0.
 * - Otherwise, this returns the number of bytes received.
 *
 * @note SPI_ENABLE_DMA: Enable DMA transfer mode.
 * buf points to a spi_dma_config_t provided by the caller (driver reads it).
 * This request is valid only when there is no ongoing transfer.
 *
 * @note SPI_DISABLE_DMA: Disable DMA transfer mode.
 * buf is ignored (pass NULL).
 * This request is valid only when there is no ongoing transfer.
 *
 * @param[in]     hspi The SPI peripheral handle returned in open() call.
 * @param[in]     cmd  The configuration request from one of the spi_ioctl_t.
 * @param[in,out] buf  The configuration values for the SPI port.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 *     - buf is NULL for requests which require a buffer
 * - -EBUSY:  if the bus is busy for requests which require an idle bus
 *            (SPI_SET_CONFIG/SPI_ENABLE_DMA/SPI_DISABLE_DMA)
 * - -EBUSY:  if DMA is already enabled for SPI_ENABLE_DMA
 */
int32_t spi_ioctl(spi_handle_t const hspi, spi_ioctl_t cmd, void *const buf);

/**
 * @brief Perform a synchronous SPI transfer.
 *
 * This function reads/writes @p nbytes bytes synchronously.
 * On success, the number of bytes actually transferred can be obtained via
 * @ref spi_ioctl (see @ref SPI_GET_TX_NBYTES and @ref SPI_GET_RX_NBYTES).
 *
 * @param[in]  hspi   SPI handle returned by @c spi_open() or
 *                    @c spi_slave_open().
 * @param[in]  txbuf  Transmit buffer pointer (may be NULL for RX-only transfers
 *                    in slave mode).
 *                    - Interrupt/PIO mode: treated as @c uint8_t[nbytes].
 *                    - DMA mode: treated as @c uint32_t[nbytes] where each
 *                      32-bit word represents one SPI byte in bits [7:0].
 * @param[out] rxbuf  Receive buffer pointer (may be NULL for TX-only
 *                    transfers).
 *                    - Interrupt/PIO mode: treated as @c uint8_t[nbytes].
 *                    - DMA mode: treated as @c uint8_t[nbytes].
 * @param[in]  nbytes Number of SPI bytes to clock.
 *
 * @note When DMA is enabled, the caller is responsible for DMA-safe buffer
 *       properties (for example, alignment and cache maintenance policy).
 *
 * @return
 * - 0:       on success (all the requested bytes have been read/written)
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 *     - txbuf is NULL for master transfers
 *     - txbuf and rxbuf are both NULL
 *     - nbytes is 0
 * - -EIO:    if there is some unknown driver error.
 * - -EBUSY:  if the bus is busy which means there is an ongoing operation.
 */
int32_t spi_xfer_sync(spi_handle_t const hspi, void *const txbuf,
        void *const rxbuf, uint16_t nbytes);

/**
 * @brief Perform an asynchronous SPI transfer.
 *
 * This function starts a transfer and returns immediately. Completion is
 * reported through the callback set by @ref spi_set_callback.
 * Buffer interpretation matches @ref spi_xfer_sync.
 *
 * @param[in]  hspi   SPI handle returned by @c spi_open() or
 *                    @c spi_slave_open().
 * @param[in]  txbuf  Transmit buffer pointer (see @ref spi_xfer_sync).
 * @param[out] rxbuf  Receive buffer pointer (see @ref spi_xfer_sync).
 * @param[in]  nbytes Number of SPI bytes to clock.
 *
 * @note When DMA is enabled, RX buffers may require cache invalidation after
 *       completion (depending on platform cache configuration).
 *
 * @return
 * - 0:       on success (all the requested bytes have been read/written)
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 *     - txbuf is NULL for master transfers
 *     - txbuf and rxbuf are both NULL
 *     - nbytes is 0
 * - -EIO:    if there is some unknown driver error.
 * - -EBUSY:  if the bus is busy which means there is an ongoing operation.
 */
int32_t spi_xfer_async(spi_handle_t const hspi, void *const txbuf,
        void *const rxbuf, uint16_t nbytes);

/**
 * @brief Closes the SPI instance.
 *
 * @param[in] hspi The SPI peripheral handle returned in open() call.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 * - -EIO:    if there is an internal driver error.
 */
int32_t spi_close(spi_handle_t const hspi);

/**
 * @brief Stops ongoing operation - Cancel is not supported for this driver.
 *
 * @param[in] hspi The SPI peripheral handle returned in open() call.
 *
 * @return
 * - -ENOSYS: always.
 */
int32_t spi_cancel(spi_handle_t const hspi);

/**
 * @brief This function is used to select spi slave.
 *
 * @param[in] hspi The SPI peripheral handle returned in open() call.
 * @param[in] ss   Slave select number.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if
 *      - ss is invalid
 *      - hspi is NULL
 *      - hspi is not opened yet
 * - -ENOTSUP: if called on a slave-mode handle
 */
int32_t spi_select_slave(spi_handle_t const hspi, uint32_t ss);
/**
 * @}
 */
/* end of group spi_fns */

/**
 * @}
 */
/* end of group spi */

#endif /* __SOCFPGA_SPI_H__ */
