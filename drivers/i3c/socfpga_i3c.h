/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for I3C HAL driver
 */

#ifndef __SOCFPGA_I3C_H__
#define __SOCFPGA_I3C_H__

/**
 * @file socfpga_i3c.h
 * @brief I3C HAL driver header file.
 *
 */

#include <errno.h>

/**
 * @defgroup i3c I3C
 * @ingroup drivers
 * @brief APIs for Soc FPGA I3C driver.
 * @details
 *
 * This driver provides methods to perform  operations with an I3C
 * or an I2C device connected to the SoC FPGA I3C bus.
 *
 * The driver supports DAA and DASA and can communicate with both I2C and I3C
 * devices connected to the bus in interrupt mode. For I3C devices the
 * I3C controller will assign an address if needed whereas for an I2C device its
 * static address is used to communicate with the controller.
 *
 * A typical usage flow is as below:
 *
 * 1. Initialize the driver using the `i3c_open` API.
 * 2. Prepare an array of type `struct i3c_i3c_device`. Each entry in the array should be
 *    filled with the properties of the I3C or I2C device present on the bus.
 * 3. Use the `I3C_IOCTL_TARGET_ATTACH` IOCTL to attach the devices. Use the above
 *    array as the parameter to this IOCTL.
 * 4. Use the `I3C_IOCTL_BUS_INIT` IOCTL to trigger dynamic address assignment.
 * 5. For I3C devices, use the `I3C_GET_DYNADDRESS` IOCTL to get the dynamic address.
 * 6. For I2C devices, use the `I2C_IOCTL_ADDRESS_VALID` to verify that the slave address is valid.
 * 7. Perform read/write operations as desired using the `i3c_transfer_sync` API.
 *
 * To see example usage, see @ref i3c_sample "I3C sample application"
 * @{
 */

/**
 * @defgroup i3c_fns Functions
 * @ingroup i3c
 * I3C HAL APIs
 */

/**
 * @defgroup i3c_structs Structures
 * @ingroup i3c
 * I3C Specific Structures
 */

/**
 * @defgroup i3c_enums Enumerations
 * @ingroup i3c
 * I3C Specific Enumerations
 */

/**
 * @defgroup i3c_macros Macros
 * @ingroup i3c
 * I3C Specific Macros
 */

/**
 * @addtogroup i3c_macros
 * @{
 */
/**
 * @brief I3C controller instance specification.
 */
#define I3C_INSTANCE1        0x0U                   /*!< I3C  instance number 1. */
#define I3C_INSTANCE2        0x1U                   /*!< I3C  instance number 2. */
#define I3C_NUM_INSTANCES    0x2U                   /*!< I3C  maximum number of instances */
/**
 * @}
 */

/**
 * @brief Specifies the different IOCTL requests allowed by the I3C controller driver.
 * @ingroup i3c_enums
 */
enum i3c_ioctl_request
{
    I3C_IOCTL_TARGET_ATTACH,          /*!< Command to attach the I3C device. */
    I3C_IOCTL_BUS_INIT,               /*!< Command for I3C bus initialization. */
    I3C_IOCTL_DO_DAA,                 /*!< command to start DAA. */
    I3C_IOCTL_GET_DYNADDRESS,         /*!< Command to get I3C device dynamic address. */
    I2C_IOCTL_ADDRESS_VALID           /*!< Command to check if I2C device is valid. */
};

/**
 * @addtogroup i3c_structs
 * @{
 */

/**
 * @brief Structure used to describe an I3C device that is connected to the I3C controller instance.
 */
struct i3c_i3c_device
{

    uint64_t device_id : 48;            /*!< 48 bit device provisioned ID.For i2c device mark it as zero*/

    uint8_t static_address;             /*!< static_address to be used for the target/device. If
                                         * it is set to 0, controller should access a dynamic
                                         * address through the DAA scheme
                                         */

    uint8_t preferred_dynamic_address;   /*!< preferred dynamic address for the device,
                                          * The final dynamic address assigned will be set in
                                          * the dynamic_address field.
                                          */

    uint8_t dynamic_address;            /*!< Final address of the device under the controller */
};


/**
 * @brief Structure used to pass the information of all the I3C devices that are connected to the I3C controller instance.
 */
struct i3c_dev_list
{
    uint16_t num_devices;               /*!< number of device objects in the pDeviceList */

    struct i3c_i3c_device *list;        /*!< List of i3c device connected to the controller*/
};

/**
 * @brief Callback function for i3c operations
 * @param[in] stat I3C instance
 * @param[in] param function param
 */
typedef void (*i3c_callback_t)(int stat,
        void *param);

/**
 * @brief Structure used by the user layer to make read/write from/to a connected device (after I3C_IOCTL_TARGET_ATTACH and I3C_IOCTL_BUS_INIT).
 */
struct i3c_xfer_request
{
    uint8_t *buffer;        /*!< Pointer to the location where data is present for write
                               or to where it should be copied for read */
    uint16_t length;        /*!< length in bytes of the data request */

    bool read;              /*!< read or write xfer*/
};
/**
 * @}
 */

/**
 * @addtogroup i3c_fns
 * @{
 */
/**
 * @brief Initialize the I3C controller instance.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return
 * - I3C_OK: if successful
 * - EINVAL: if incorrect instance
 * - EBUSY:  if bus is busy.
 */
extern int32_t i3c_open(uint8_t instance);

/**
 * @brief Configure I3C controller instance with user configuration.
 *
 * @note I3C_IOCTL_TARGET_ATTACH: Request for adding the devices connected to the controller.
 * Uses i3c_I3cDevList structure to pass the number of devices and their properties to the controller.
 *
 * @note I3C_IOCTL_BUS_INIT: Used for enumerating the I3C bus. The I3C_IOCTL_TARGET_ATTACH should
 * be called before calling this request. This ioctl will try to assign dynamic addresses to all the
 * connected devices. Once this is completed, read/write transactions can be performed with the devices.
 *
 * @note I3C_IOCTL_DO_DAA: Perform the dynamic address assignment to the connected devices
 * which do not have static addresses.
 *
 * @param[in] instance Instance of the I3C controller.
 * @param[in] ioctl    IOCTL request.
 * @param[in] pargs    Pointer to the arguments for the IOCTL request.
 *
 * @return
 * - I3C_OK:  if successful
 * - -EINVAL: if incorrect instance
 * - -EBUSY:  if busy
 */
extern int32_t i3c_ioctl(uint8_t instance, enum i3c_ioctl_request ioctl,
        void *pargs );

/**
 * @brief set I3C callback
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] callback  The callback to be registered.
 * @param[in] param    Parameters that are to be passed to the callback.
 */
extern void i3c_set_callback(uint8_t instance,
        i3c_callback_t callback, void *param);

/**
 * @brief Perform a data transfer between the controller and the target device.
 *
 * @param[in] instance      Instance of the I3C controller.
 * @param[in] address       Dynamic address of the target to which the transfer is requested.
 * @param[in] pxfer_request Pointer to the transfer request list.
 * @param[in] num_xfers     Number of transfers requested.
 * @param[in] is_i2c        Indicates if the transfer is for an I2C device.
 *
 * @return
 * - -EINVAL:         if invalid arguments were passed,
 * - -EINVAL:/-EBUSY: if transfer was not successful
 * - I3C_OK:          if transfer was successful.
 */
extern int32_t i3c_transfer_sync(uint8_t instance, uint8_t address, struct
        i3c_xfer_request *pxfer_request, uint8_t num_xfers, bool is_i2c);

/**
 * @brief Perform a data transfer between the controller and the target device.
 *
 * @param[in] instance      Instance of the I3C controller.
 * @param[in] address       Dynamic address of the target to which the transfer is requested.
 * @param[in] pxfer_request Pointer to the transfer request list.
 * @param[in] num_xfers     Number of transfers requested.
 * @param[in] is_i2c        Indicates if the transfer is for an I2C device.
 *
 * @return
 * - -EINVAL:         if invalid arguments were passed,
 * - -EINVAL:/-EBUSY: if transfer was not successful
 * - I3C_OK:          if transfer was successful.
 */
extern int32_t i3c_transfer_async(uint8_t instance, uint8_t address, struct
        i3c_xfer_request *pxfer_request, uint8_t num_xfers, bool is_i2c);

/**
 * @}
 */
/**
 * @}
 */

#endif /*__SOCFPGA_I3C_H__*/
