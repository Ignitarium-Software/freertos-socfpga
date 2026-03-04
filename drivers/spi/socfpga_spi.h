/*
 * Common IO - basic V1.0.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Copyright (C) 2025 Altera Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
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

/**
 * @defgroup spi SPI
 * @ingroup drivers
 * @brief APIs for SoC FPGA SPI driver.
 * @details
 * This is the SPI driver implementation for SoC FPGA.
 * It supports the master mode operation.
 * It provides APIs for data transfer with an SPI slave.
 * The APIs are designed to be used in both synchronous and asynchronous modes.<br>
 * To see example usage, see @ref spi_sample "SPI sample application".
 * @{
 */

/**
 * @defgroup spi_fns Functions
 * @ingroup SPI
 * SPI HAL APIs
 */

/**
 * @defgroup spi_structs Structures
 * @ingroup SPI
 * SPI Specific Structures
 */

/**
 * @defgroup spi_enums Enumerations
 * @ingroup SPI
 * SPI Specific Enumerations
 */

/**
 * @brief The SPI return status from Async operations.
 * @ingroup spi_enums
 */
typedef enum
{
    SPI_SUCCESS,          /*!< SPI operation completed successfully. */
    SPI_WR_ERROR,    /*!< SPI driver returns error when performing write operation. */
    SPI_RD_ERROR,     /*!< SPI driver returns error when performing read operation. */
    SPI_XFER_ERROR, /*!< SPI driver returns error when performing transfer. */
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
 * @brief Ioctl request for SPI HAL.
 * @ingroup spi_enums
 */
typedef enum
{
    SPI_SET_CONFIG, /*!< Sets the configuration of the SPI master and the data type is spi_cfg_t. */
    SPI_GET_CONFIG, /*!< Gets the configuration of the SPI master and the data type is spi_cfg_t. */
    SPI_GET_TX_NBYTES,  /*!< Get the number of bytes sent in write operation and the data type is uint16_t. */
    SPI_GET_RX_NBYTES,  /*!< Get the number of bytes received in read operation and the data type is uint16_t. */
} spi_ioctl_t;

/**
 * @addtogroup spi_structs
 * @{
 */
/**
 * @brief The configuration parameters for SPI Master.
 *
 * @details The application will set the SPI master interface using the Ioctl
 * SPI_SET_CONFIG and send this structure.
 */
typedef struct
{
    uint32_t clk;      /*!< SPI frequency set for data transmission in Hz. */
    spi_mode_t mode; /*!< Mode selected as per enum spi_mode_t. */
} spi_cfg_t;

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
 * @brief Initializes SPI peripheral with default configuration.
 *
 * @warning Once opened, the same SPI instance must be closed before calling open again.
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
 * @brief Sets the application callback to be called on completion of an operation.
 *
 * The callback is guaranteed to be invoked when the current asynchronous operation completes, either successful or failed.
 * This simply provides a notification mechanism to user's application. It has no impact if the callback is not set.
 *
 * @note This callback will not be invoked when synchronous operation completes.
 * @note This callback is per handle. Each instance has its own callback.
 * @note Single callback is used for both read_async and write_async. Newly set callback overrides the one previously set.
 * @warning If the input handle is invalid, this function silently takes no action.
 *
 * @param[in] hspi The SPI peripheral handle returned in the open() call.
 * @param[in] callback The callback function to be called on completion of operation.
 * @param[in] pcntxt The user context to be passed back when callback is called.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 *     - pucBuffer is NULL with requests which needs buffer
 * - -EBUSY:  if the bus is busy
 */
int32_t spi_set_callback(spi_handle_t const hspi, spi_callback_t callback,
        void *pcntxt);

/**
 * @brief Configures the SPI port with user configuration.
 *
 *
 * @note SPI_SET_CONFIG sets the configurations for master.
 * This request expects the buffer with size of spi_cfg_t.
 *
 * @note SPI_GET_CONFIG gets the current configuration for SPI master.
 * This request expects the buffer with size of spi_cfg_t.
 *
 * @note SPI_GET_TX_NBYTES returns the number of written bytes in last operation.
 * This is supposed to be called in the caller task or application callback, right after last operation completes.
 * This request expects 2 bytes buffer (uint16_t).
 *
 * - If the last operation only did write, this returns the actual number of written bytes which might be smaller than the requested number (partial write).
 * - If the last operation only did read, this returns 0.
 * - If the last operation did both write and read, this returns the number of written bytes.
 *
 * @note SPI_GET_RX_NBYTES returns the number of read bytes in last operation.
 * This is supposed to be called in the caller task or application callback, right after last operation completes.
 * This request expects 2 bytes buffer (uint16_t).
 *
 * - If the last operation only did read, this returns the actual number of read bytes which might be smaller than the requested number (partial read).
 * - If the last operation only did write, this returns 0.
 * - If the last operation did both write and read, this returns the number of read bytes.
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
 *     - pucBuffer is NULL with requests which needs buffer
 * - -EBUSY:  if the bus is busy for only following requests:
 *     - SPI_SET_CONFIG
 */
int32_t spi_ioctl(spi_handle_t const hspi, spi_ioctl_t cmd, void *const buf);

/**
 * @brief The SPI master starts a synchronous transfer between master and the slave.
 *
 * This function attempts to read/write certain number of bytes from/to two pre-allocated buffers at the same time, in synchronous way.
 * This function does not return on partial read/write, unless there is an error.
 * And the number of bytes that have been actually read or written can be obtained by calling spi_ioctl.
 *
 * @param[in]  hspi   The SPI peripheral handle returned in open() call.
 * @param[in]  txbuf  The buffer to transmit data. For write operation txbuf contains the actual data
                      and for read operation the txbuf should contain dummy data.
 * @param[out] rxbuf  The buffer to recieve data. For write operations the rxbuf shoud be NULL.
 * @param[in]  nbytes The number of bytes to transfer.
 *
 * @return
 * - 0:       on success (all the requested bytes have been read/written)
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 *     - pucBuffer is NULL
 *     - nbytes is 0
 * - -EIO:    if there is some unknown driver error.
 * - -EBUSY:  if the bus is busy which means there is an ongoing operation.
 */
int32_t spi_transfer_sync(spi_handle_t const hspi, uint8_t *const txbuf,
        uint8_t *const rxbuf, uint16_t nbytes);

/**
 * @brief The SPI master starts a asynchronous transfer between master and the slave.
 *
 * This function attempts to read/write certain number of bytes from/to two pre-allocated buffers at the same time, in asynchronous way.
 * It returns immediately when the operation is started and the status can be check by calling spi_ioctl.
 *
 * Once the operation completes successfully, the user callback will be invoked.
 * If the operation encounters an error, the user callback will be invoked.
 * The callback is not invoked on partial read/write, unless there is an error.
 * And the number of bytes that have been actually read/write can be obtained by calling spi_ioctl.
 *
 * @param[in]  hspi   The SPI peripheral handle returned in open() call.
 * @param[in]  txbuf  The buffer to transmit data. For write operation txbuf contains the actual data
                      and for read operation the txbuf should contain dummy data.
 * @param[out] rxbuf  The buffer to recieve data. For write operations the rxbuf shoud be NULL.
 * @param[in]  nbytes The number of bytes to transfer.
 *
 * - 0:       on success (all the requested bytes have been read/written)
 * - -EINVAL: if
 *     - hspi is NULL
 *     - hspi is not opened yet
 *     - pucBuffer is NULL
 *     - nbytes is 0
 * - -EIO:    if there is some unknown driver error.
 * - -EBUSY:  if the bus is busy which means there is an ongoing operation.
 */
int32_t spi_transfer_async(spi_handle_t const hspi, uint8_t *const txbuf,
        uint8_t *const rxbuf, uint16_t nbytes);

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
 * @param[in] hspi The instance of the SPI driver to initialize.
 * @param[in] ss   Slave select number.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if
 *      - ss is invalid
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

#endif /* _SOCFPGA_SPI_H_ */
