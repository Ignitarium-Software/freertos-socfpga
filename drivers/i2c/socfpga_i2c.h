/*
 * Common IO - basic V1.0.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Copyright (C) 2025-2026 Altera Corporation
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "socfpga_defines.h"
#include "socfpga_dma.h"
#include "socfpga_i2c_reg.h"
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
#define I2C_STANDARD_MODE_BPS     100000U        /*!< Standard mode bits per second. */
#define I2C_FAST_MODE_BPS         400000U        /*!< Fast mode bits per second. */
#define I2C_FAST_MODE_PLUS_BPS    1000000U       /*!< Fast plus mode bits per second. */
#define I2C_HIGH_SPEED_BPS        3400000U       /*!< High speed mode bits per second. */

/**
 * The DMA LLI params used by I2C driver
 *
 */
#ifndef MAX_I2C_BLK_XFERS
#define MAX_I2C_BLK_XFERS   8U
#endif

#ifndef I2C_DMA_XFER_BLK_SIZE
#define I2C_DMA_XFER_BLK_SIZE   512U
#endif

/*
 * DMA DATA_CMD helpers for 32-bit command/data words.
 * Callers must build these words for DMA-mode transfers.
 */
#define I2C_DMA_CMD_FLAG_STOP    (1U << I2C_DATA_CMD_STOP_POS)
#define I2C_DMA_CMD_FLAG_READ    (1U << I2C_DATA_CMD_CMD_POS)
#define I2C_DMA_CMD_FLAG_RESTART (1U << I2C_DATA_CMD_RESTART_POS)
#define I2C_DMA_CMD_FLAG_FIRST   (1U << I2C_DATA_CMD_FIRST_DATA_BYTE_POS)
#define I2C_DMA_CMD_DATA(data)   ((uint32_t)(data) & I2C_DATA_CMD_DAT_MASK)
#define I2C_DMA_CMD_WORD(data, flags) (I2C_DMA_CMD_DATA(data) | (flags))

/**
 * @}
 */
/* end of group i2c_macros */

/**
 * @brief Data transfer status.
 * Used while invoking the callback function.
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_SUCCESS = 0, /*!< I2C operation completed successfully. */
    I2C_OP_FAIL, /*!< I2C driver returns error during last operation. */
    I2C_NACK, /*!< Unexpected NACK is caught. */
} i2c_op_status_t;

/**
 * @addtogroup i2c_structs
 * @{
 */
/**
 * @brief I2C bus configuration
 *
 * clk: bus frequency in Hz.
 */
typedef struct i2c_config
{
    uint32_t clk;
} i2c_config_t;

/**
 * @brief I2C DMA channel configuration
 *
 * tx_instance/tx_channel/tx_prio: DMA settings for TX.
 * rx_instance/rx_channel/rx_prio: DMA settings for RX.
 */
typedef struct i2c_dma_config
{
    uint32_t tx_instance;
    uint32_t tx_channel;
    uint8_t tx_prio;
    uint32_t rx_instance;
    uint32_t rx_channel;
    uint8_t rx_prio;
} i2c_dma_config_t;

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
    I2C_OPEN_DMA,
    I2C_ENABLE_WDMA,
    I2C_ENABLE_RDMA,
    I2C_DISABLE_WDMA,
    I2C_DISABLE_RDMA,
    I2C_CLOSE_DMA,
    I2C_SLAVE_INIT,   /*!< Configure and enable slave mode using #i2c_slave_config_t. */
    I2C_SLAVE_DEINIT, /*!< Disable slave mode and restore master defaults. */
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
 * @brief I2C operations
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_READ = 1,
    I2C_WRITE = 2,
} i2c_operation_t;

/**
 * @brief I2C controller role.
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_ROLE_MASTER = 0,
    I2C_ROLE_SLAVE = 1,
} i2c_role_t;

/**
 * @brief I2C bus status
 * @ingroup i2c_enums
 */
typedef enum
{
    I2C_BUS_IDLE = 0,
    I2C_BUS_BUSY,
} i2c_bus_status_t;


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
 * @brief Callback invoked when an external master initiates a write to this slave.
 *
 * Called from ISR context before any data bytes are received. Allows the
 * application to prepare its receive buffer.
 *
 * @param[in] hi2c  I2C handle.
 * @param[in] param User context.
 */
typedef void (*i2c_slave_write_requested_cb_t)(i2c_handle_t hi2c, void *param);

/**
 * @brief Callback invoked for each byte received from an external master.
 *
 * Called from ISR context once per received byte.
 *
 * @param[in] hi2c  I2C handle.
 * @param[in] data  The received byte.
 * @param[in] param User context.
 */
typedef void (*i2c_slave_write_received_cb_t)(i2c_handle_t hi2c, uint8_t data,
        void *param);

/**
 * @brief Callback invoked when an external master requests to read from this slave.
 *
 * Called from ISR context on the first RD_REQ of a read transaction. The
 * callback must fill @p data with the first byte to transmit.
 *
 * @param[in]  hi2c  I2C handle.
 * @param[out] data  Pointer to store the byte to transmit.
 * @param[in]  param User context.
 */
typedef void (*i2c_slave_read_requested_cb_t)(i2c_handle_t hi2c, uint8_t *data,
        void *param);

/**
 * @brief Callback invoked after each byte has been transmitted to an external master.
 *
 * Called from ISR context after each byte write. Allows the application to
 * advance its transmit pointer. @p data may be filled with the next byte
 * as a hint, but the driver will call @c read_requested_cb again on the
 * next RD_REQ.
 *
 * @param[in]  hi2c  I2C handle.
 * @param[out] data  Pointer to optionally store the next byte.
 * @param[in]  param User context.
 */
typedef void (*i2c_slave_read_processed_cb_t)(i2c_handle_t hi2c, uint8_t *data,
        void *param);

/**
 * @brief Callback invoked when a STOP condition is detected on the bus.
 *
 * Called from ISR context. Signals the end of the current transaction.
 *
 * @param[in] hi2c  I2C handle.
 * @param[in] param User context.
 */
typedef void (*i2c_slave_stop_cb_t)(i2c_handle_t hi2c, void *param);

/**
 * @brief I2C slave mode configuration.
 *
 * Callbacks run in ISR context and follow a byte-oriented model.
 * Set @c rx_tl / @c tx_tl to 0 for per-byte ISR handling (default).
 */
typedef struct i2c_slave_config
{
    uint16_t slave_address;
    bool is_10bit_addr;
    bool stop_det_ifaddressed;
    bool ack_general_call;
    uint8_t tx_default_byte;
    uint8_t rx_tl;  /*!< RX FIFO threshold (I2C_RX_TL); 0 = fire after every byte. */
    uint8_t tx_tl;  /*!< TX FIFO threshold (I2C_TX_TL); 0 = fire when TX FIFO is empty. */

    i2c_slave_write_requested_cb_t  write_requested_cb;
    i2c_slave_write_received_cb_t   write_received_cb;
    i2c_slave_read_requested_cb_t   read_requested_cb;
    i2c_slave_read_processed_cb_t   read_processed_cb;
    i2c_slave_stop_cb_t             stop_cb;
    void *cb_usercontext;
} i2c_slave_config_t;

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
 * @param[out] buf    The receive buffer to read the data into.
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
int32_t i2c_read_sync(i2c_handle_t const hi2c, void *const buf, size_t
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
 * @param[in] buf    Transmit buffer containing the data/commands to be written.
 *
 *                   Interrupt mode (DMA disabled):
 *                   - @p buf is a byte buffer (uint8_t *) sized to @p nbytes bytes.
 *
 *                   DMA mode (WDMA enabled):
 *                   - @p buf is a 32-bit word buffer (uint32_t *) sized to
 *                     @p nbytes words.
 *                   - Each word is a pre-built IC_DATA_CMD value (data + STOP/READ/
 *                     RESTART/FIRST flags as required). See `I2C_DMA_CMD_*` helpers.
 *
 * @param[in] nbytes In interrupt mode this is a byte count; in DMA write mode this
 *                   is a word count.
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
int32_t i2c_write_sync(i2c_handle_t const hi2c, void *const buf, size_t
        nbytes);

/**
 * @brief Starts an I2C read operation in asynchronous mode.
 *
 * Works for both master and slave roles.
 *
 * Master: reads @p nbytes from the configured target slave device using
 * DMA (if enabled) or interrupt mode. Returns immediately; completion is
 * signalled via the callback set with i2c_set_callback().
 *
 * Slave: pre-arms RX DMA to receive @p nbytes from an external master.
 * DMA must be open and RX-enabled before calling. Completion is signalled
 * via the callback set with i2c_set_callback(). The slave must call this
 * before the master begins its write transaction.
 *
 * @note i2c_set_callback() must be called before this function.
 * @note For master role, slave address must already be configured.
 *
 * @param[in]  hi2c   The I2C handle returned in open() call.
 * @param[out] buf    The receive buffer to read the data into.
 * @param[in]  nbytes The number of bytes to read.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if parameters are invalid, or slave role with DMA not ready
 * - -ENXIO:  if master role and slave address is not set
 * - -EIO:    on DMA or transfer error
 * - -EBUSY:  if another transfer is in progress
 */
int32_t i2c_read_async(i2c_handle_t const hi2c, void *const buf, size_t
        nbytes);

/**
 * @brief Starts an I2C write operation in asynchronous mode.
 *
 * Works for both master and slave roles. @p buf is always a byte buffer;
 * the driver expands bytes to the required IC_DATA_CMD word format internally.
 *
 * Master: writes @p nbytes to the configured target slave device using
 * DMA (if enabled) or interrupt mode. Returns immediately; completion is
 * signalled via the callback set with i2c_set_callback().
 *
 * Slave: pre-arms TX DMA with @p nbytes to transmit when an external master
 * reads from this slave. DMA must be open and TX-enabled before calling.
 * Completion is signalled via the callback set with i2c_set_callback().
 * The slave must call this before the master begins its read transaction.
 *
 * @note i2c_set_callback() must be called before this function.
 * @note For master role, slave address must already be configured.
 *
 * @param[in] hi2c   The I2C handle returned in open() call.
 * @param[in] buf    Transmit buffer containing the data/commands to write.
 *
 *                 Interrupt mode (DMA disabled):
 *                 - @p buf is a byte buffer (uint8_t *) sized to @p nbytes bytes.
 *
 *                 DMA mode (WDMA enabled):
 *                 - @p buf is a 32-bit word buffer (uint32_t *) sized to
 *                   @p nbytes words.
 *                 - Each word is a pre-built IC_DATA_CMD value (data + STOP/READ/
 *                   RESTART/FIRST flags as required). See `I2C_DMA_CMD_*` helpers.
 *
 * @param[in] nbytes In interrupt mode this is a byte count; in DMA write mode this
 *                   is a word count.
 *
 * @return
 * - 0: on success
 * - -EINVAL: if parameters are invalid, or slave role with DMA not ready
 * - -ENXIO:  if master role and slave address is not set
 * - -EIO:    on DMA or transfer error
 * - -EBUSY:  if another transfer is in progress
 */
int32_t i2c_write_async(i2c_handle_t const hi2c, void *const buf, size_t
        nbytes);

/**
 * @brief Configures the I2C master with user configuration.
 *
 * @param[in] hi2c       The I2C handle returned in open() call.
 * @param[in] cmd        Should be one of i2c_ioctl_t.
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
 * @note I2C_SLAVE_INIT configures and enables slave mode using #i2c_slave_config_t.
 * @note I2C_SLAVE_DEINIT disables slave mode and restores master defaults.
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
