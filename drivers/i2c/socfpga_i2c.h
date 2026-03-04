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

#ifndef __SOCFPGA_I2C_H__
#define __SOCFPGA_I2C_H__

/**
 * @file socfpga_i2c.h
 * @brief SoC FPGA I2C HAL driver
 */

#include <errno.h>
#include "socfpga_defines.h"
/**
 * @defgroup i2c I2C
 * @ingroup drivers
 * @brief APIs for Soc FPGA I2C driver.
 * @details This is the I2C driver implementation for SoC FPGA.
 * It provides APIs for configuring I2C as master, writing to and reading
 * from I2C slave devices. For example usage, see
 * @ref i2c_sample "I2C sample application".
 * @{
 */

/**
 * @defgroup i2c_fns Functions
 * @ingroup i2c
 * I2C HAL APIs
 */

/**
 * @defgroup i2c_structs Structures
 * @ingroup i2c
 * I2C Specific Structures
 */

/**
 * @defgroup i2c_enums Enumerations
 * @ingroup i2c
 * I2C Specific Enumerations
 */

/**
 * @defgroup i2c_macros Macros
 * @ingroup i2c
 * I2C Specific Macros
 */

/**
 * @addtogroup i2c_macros
 * @{
 */
/**
 * The speeds supported by I2C bus.
 */
#define I2C_STANDARD_MODE_BPS     (100000U)        /*!< Standard mode bits per second. */
#define I2C_FAST_MODE_BPS         (400000U)        /*!< Fast mode bits per second. */
#define I2C_FAST_MODE_PLUS_BPS    (1000000U)       /*!< Fast plus mode bits per second. */
#define I2C_HIGH_SPEED_BPS        (3400000U)       /*!< High speed mode bits per second. */
/**
 * @}
 */
/* end of group i2c_macros */

/**
 * @brief Data tranfer status.
 * Used while invoking the callback function.
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_SUCCESS = 0, /*!< I2C operation completed successfully. */
    I2C_OP_FAIL, /*!< I2C driver returns error during last operation. */
    I2C_NACK = -EIO, /*!< Unexpected NACK is caught. */
} i2c_op_status_t;

/**
 * @addtogroup i2c_structs
 * @{
 */
/**
 * @brief I2C bus configuration
 */
typedef struct i2c_config
{
    uint32_t clk; /*!< Bus frequency/baud rate */
} i2c_config_t;
/**
 * @}
 */
/* end of group i2c_structs */

/**
 * @brief Ioctl request types.
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_SEND_NO_STOP, /*!< Set flag to not send stop after the current transfer */
    /*!< Default is always stop for every transaction */
    /*!< Flag will auto reset to stop after one transaction if you set no stop */
    I2C_SET_SLAVE_ADDR, /*!< This can be either 7-bit address or 10-bit address. All the operations use this slave address after it is set. */
    I2C_SET_MASTER_CFG, /*!< Sets the I2C bus frequency and timeout using the struct #i2c_config_t, default speed is Standard mode. */
    I2C_GET_MASTER_CFG, /*!< Gets the I2C bus frequency and timeout set for the I2C master. */
    I2C_GET_BUS_STATE, /*!< Get the current I2C bus status. Returns eI2CBusIdle or eI2CBusy */
    I2C_GET_TX_NBYTES, /*!< Get the number of bytes sent in write operation. */
    I2C_GET_RX_NBYTES, /*!< Get the number of bytes received in read operation. */
} i2c_ioctl_t;

/**
 * @brief I2c baudrate types.
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_MODE_STANDARD = 1, /*!< Standard speed mode */
    I2C_MODE_FAST = 2, /*!< Fast or fast plus speed mode */
    I2C_MODE_HS = 3, /*!< High speed mode */
} i2c_mode_t;

/**
 * @brief The I2C descriptor type defined in the source file.
 * This is an anonymous struct that is vendor/driver specific.
 * @ingroup i2c_structs
 */
struct i2c_descriptor;

/**
 * @brief i2c_handle_t is the handle type returned by calling i2c_open().
 * This is initialized in open and returned to caller. The caller must pass
 * this pointer to the rest of APIs.
 * @ingroup i2c_structs
 */
typedef struct i2c_descriptor *i2c_handle_t;

/**
 * @addtogroup i2c_fns
 * @{
 */
/**
 * @brief The callback function for completion of I2C operation.
 *
 * @param[in] op_status I2C asynchronous operation status.
 * @param[in] param     User Context passed when setting the callback.
 *                      This is not used or modified by the driver. The context
 *                      is provided by the caller when setting the callback, and is
 *                      passed back to the caller in the callback.
 *
 */
typedef void (*i2c_callback_t)(i2c_op_status_t op_status, void *param);

/**
 * @brief Initiates and reserves an I2C instance as master.
 *
 * One instance can communicate with one or more slave devices.
 * Slave addresses need to be changed between actions to different slave devices.
 *
 * @warning Once opened, the same I2C instance must be closed before calling open again.
 *
 * @param[in] instance The instance of I2C to initialize. This is between 0 and the number of I2C instances on board - 1.
 *
 * @return
 * - 'the handle to the I2C port (not NULL)', on success.
 * - 'NULL', if
 *     - invalid instance number
 *     - open same instance more than once before closing it
 *     - failed to enable the interrupt
 */
i2c_handle_t i2c_open(uint32_t instance);

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
 * @param[in] hi2c     The I2C peripheral handle returned in the open() call.
 * @param[in] callback The callback function to be called on completion of transaction.
 * @param[in] param    The user context to be passed back when callback is called.
 */
void i2c_set_callback(i2c_handle_t const hi2c, i2c_callback_t callback, void *param);

/**
 * @brief Starts the I2C master read operation in synchronous mode.
 *
 * This function attempts to read certain number of bytes from slave device to a pre-allocated buffer, in synchronous way.
 * Partial read might happen, e.g. no more data is available.
 * And the number of bytes that have been actually read can be obtained by calling i2c_ioctl.
 *
 * @note Usually, the address of register needs to be written before calling this function.
 *
 * @warning Prior to this function, slave address must be already configured.
 * @warning None of other read or write functions shall be called while a transfer is in progress.
 *
 * @param[in]  hi2c   The I2C handle returned in open() call.
 * @param[out] buf    The receive buffer to read the data into. It must stay allocated by the caller.
 * @param[in]  nbytes The number of bytes to read.
 *
 * @return
 * - 0: on success (all the requested bytes have been read)
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 *     - buf is NULL
 *     - nbytes is 0
 * - -ENXIO: if slave address is not set yet
 * - -EIO:   if
 *     - the slave is unable to receive or transmit
 *     - the slave gets data or commands that it does not understand
 *     - there is some unknown driver error
 * - -EBUSY: if the bus is busy which means there is an ongoing transaction.
 *
 */
int32_t i2c_read_sync(i2c_handle_t const hi2c, uint8_t *const buf, size_t
        nbytes);

/**
 * @brief Starts the I2C master write operation in synchronous mode.
 *
 * This function attempts to write certain number of bytes from a pre-allocated buffer to a slave device, in synchronous way.
 * Partial write might happen, e.g. slave device unable to receive more data.
 * And the number of bytes that have been actually written can be obtained by calling i2c_ioctl.
 *
 * @warning Prior to this function, slave address must be already configured.
 * @warning None of other read or write functions shall be called while a transfer is in progress.
 *
 * @param[in] hi2c   The I2C handle returned in open() call.
 * @param[in] buf    The transmit buffer containing the data to be written. It must stay allocated until this function returns.
 * @param[in] nbytes The number of bytes to write.
 *
 * @return
 * - 0: on success (all the requested bytes have been written)
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 *     - buf is NULL
 *     - nbytes is 0
 * - -ENXIO: if slave address is not set yet
 * - -EIO:   if
 *     - the slave is unable to receive or transmit
 *     - the slave gets data or commands that it does not understand
 *     - there is some unknown driver error
 * - -EBUSY: if another transfer is in progress
 */
int32_t i2c_write_sync(i2c_handle_t const hi2c, uint8_t *const buf, size_t
        nbytes);

/**
 * @brief Starts the I2C master read operation in asynchronous mode.
 *
 * This function attempts to read certain number of bytes to a pre-allocated buffer, in asynchronous way.
 * It returns immediately when the operation is started and the status can be check by calling i2c_ioctl.
 * Once the operation completes, successful or not, the user callback will be invoked.
 *
 * Partial read might happen, e.g. slave device unable to receive more data.
 * And the number of bytes that have been actually read can be obtained by calling i2c_ioctl.
 *
 * @note Usually, the address of register needs to be written before calling this function.
 * @note In order to get notification when the asynchronous call is completed, i2c_set_callback must be called prior to this.
 *
 * @warning Prior to this function, slave address must be already configured.
 * @warning None of other read or write functions shall be called while a transfer is in progress.
 *
 * @param[in]  hi2c   The I2C handle returned in open() call.
 * @param[out] buf    The receive buffer to read the data into. Allocated by the caller.
 * @param[in]  nbytes The number of bytes to read.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 *     - buf is NULL
 *     - nbytes is 0
 * - -ENXIO: if slave address is not set yet
 * - -EIO:   if
 *     - there is some unknown driver error
 * - -EBUSY: if another transfer is in progress
 */
int32_t i2c_read_async(i2c_handle_t const hi2c, uint8_t *const buf, size_t
        nbytes);

/**
 * @brief Starts the I2C master write operation in asynchronous mode.
 *
 * This function attempts to write certain number of bytes from a pre-allocated buffer to a slave device, in asynchronous way.
 * It returns immediately when the operation is started and the status can be check by calling i2c_ioctl.
 * Once the operation completes, successful or not, the user callback will be invoked.
 *
 * Partial write might happen, e.g. slave device unable to receive more data.
 * And the number of bytes that have been actually written can be obtained by calling i2c_ioctl.
 *
 * @note In order to get notification when the asynchronous call is completed, i2c_set_callback must be called prior to this.
 *
 * @warning Prior to this function, slave address must be already configured.
 * @warning None of other read or write functions shall be called during this function.
 *
 * @param[in] hi2c   The I2C handle returned in open() call.
 * @param[in] buf    The transmit buffer containing the data to be written. It must stay allocated before this function returns.
 * @param[in] nbytes The number of bytes to write.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 *     - buf is NULL
 *     - nbytes is 0
 * - -ENXIO: if slave address is not set yet
 * - -EIO:   if
 *     - there is some unknown driver error
 * - -EBUSY: if another transfer is in progress
 */
int32_t i2c_write_async(i2c_handle_t const hi2c, uint8_t *const buf, size_t
        nbytes);

/**
 * @brief Configures the I2C master with user configuration.
 *
 * @param[in] hi2c       The I2C handle returned in open() call.
 * @param[in] cmd        Should be one of I2C_Ioctl_Request_t.
 * @param[in,out] pparam The configuration values for the IOCTL request.
 *
 * @note SetMasterConfig is expected only called once at beginning.
 * This request expects the buffer with size of i2c_config_t.
 *
 * @note I2C_GET_MASTER_CFG gets the current configuration for I2C master.
 * This request expects the buffer with size of i2c_config_t.
 *
 * @note I2C_GET_BUS_STATE gets the current bus state.
 * This request expects buffer with size of I2cBusStatus_t.
 *
 * @note I2C_SEND_NO_STOP is called at every operation you want to not send stop condition.
 *
 * @note I2C_SET_SLAVE_ADDR sets either 7-bit address or 10-bit address, according to hardware's capability.
 * This request expects 2 bytes buffer (uint16_t)
 *
 * @note I2C_GET_TX_NBYTES returns the number of written bytes in last transaction.
 * This is supposed to be called in the caller task or application callback, right after last transaction completes.
 * This request expects 2 bytes buffer (uint16_t).
 *
 * @note I2C_GET_RX_NBYTES returns the number of read bytes in last transaction.
 * This is supposed to be called in the caller task or application callback, right after last transaction completes.
 * This request expects 2 bytes buffer (uint16_t).
 *
 * @return
 * - 0: on success
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 *     - buf is NULL with requests which needs buffer
 */
int32_t i2c_ioctl(i2c_handle_t const hi2c, i2c_ioctl_t cmd, void *const pparam);

/**
 * @brief Stops the ongoing operation and de-initializes the I2C peripheral.
 *
 * @param[in] hi2c The I2C handle returned in open() call.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 */
int32_t i2c_close(i2c_handle_t const hi2c);

/**
 * @brief This function is used to cancel the current operation in progress, if possible.
 *
 * @param[in] hi2c The I2C handle returned in open() call.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if
 *     - hi2c is NULL
 *     - hi2c is not opened yet
 * - -EPERM:  if there is no on-going transaction.
 * - -ENOSYS: if this board doesn't support this operation.
 */
int32_t i2c_cancel(i2c_handle_t const hi2c);
/**
 * @}
 */
/* end of group i2c_fns */

/**
 * @}
 */
/* end of group i2c */
#endif /* _SOCFPGA_I2C_H_ */
