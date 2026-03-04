/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for I3C
 */

#include <errno.h>
#include "osal_log.h"
#include "socfpga_defines.h"
#include "socfpga_i3c.h"
#include "socfpga_i3c_ll.h"

#define GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(addr)    (((addr) << 1U) % \
    NUM_BITS_PER_TABLE_ENTRY)
#define GET_ADDRESS_ALLOTMENT_TABLE_INDEX(addr)        (((addr) << 1U) / \
    NUM_BITS_PER_TABLE_ENTRY)

#define SET_ADDRESS_ALLOTMENT_ENTRY(idx, pos, status)    do {                                                                                                     \
        i3c_obj[(instance)].addr_allotment_table[(idx)] &= \
                ~((uint32_t)ADDRESS_ENTRY_STATUS_MAX << (pos)); \
        i3c_obj[(instance)].addr_allotment_table[(idx)] |= \
                ((uint32_t)(status) << (pos));                             \
}while(false)


#define GET_ADDRESS_ALLOTMENT_ENTRY(idx, pos) \
    ((i3c_obj[(instance)].addr_allotment_table[(idx)] >> (pos)) & \
    ADDRESS_ENTRY_STATUS_MAX)

/**
 * @brief I3C_CCC Common Command Codes (CCC)
 */
#define I3C_CCC_RSTDAA    (0x06U)
#define I3C_CCC_RSTACT(broadcast)    ((broadcast) ? 0x2AU : 0x9AU)
#define I3C_CCC_SETDASA    (0x87U)
#define I3C_CCC_ENTDAA     (0x07U)

/**
 * @brief I3C_CCC_Events Enable Events
 */
#define I3C_CCC_EVT_INTR    ((uint32_t)1)
#define I3C_CCC_EVT_CR      ((uint32_t)1 << 1U)
#define I3C_CCC_EVT_HJ      ((uint32_t)1 << 3U)

#define I3C_CCC_EVT_ALL    (I3C_CCC_EVT_INTR | I3C_CCC_EVT_CR | I3C_CCC_EVT_HJ)

#define I3C_CCC_DISEC(broadcast)    ((broadcast) ? 0x01U : 0x81U)

#define I3C_CCC_RSTACT_PERIPHERAL_ONLY       (0x01U)
#define I3C_CCC_RSTACT_RESET_WHOLE_TARGET    (0x02U)


/* I3C Controller Instance object  */
extern struct i3c_driver_obj i3c_obj[I3C_NUM_INSTANCES];


/**
 * @brief Get the status of the entry in the Address allotment table corresponding
 *        to the I3C address specified.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] address   Address for which the status is inquired.
 * @return uint8_t      Entry in the location corresponding to the address.
 */
static uint32_t get_address_allotment_table_entry(uint8_t instance, uint8_t
        address)
{
    uint8_t idx, bit_index;
    uint32_t status;

    idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(address);
    bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(address);

    status = GET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index);

    return status;

}


/**
 * @brief Get the status of the entry in the Address allotment table corresponding
 *        to the I3C address specified.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] address   Address for which the status is inquired.
 * @return uint8_t      Entry in the location corresponding to the address.
 */
static uint8_t get_next_free_address_allotment_table_entry(uint8_t instance)
{
    uint8_t addr;

    for (addr = 8U; addr < I3C_MAX_ADDR; addr++)
    {
        if (get_address_allotment_table_entry (instance, addr) ==
                ADDRESS_ENTRY_STATUS_FREE)
        {
            break;
        }
    }

    return addr;
}

/**
 * @brief Initialize address allotment table, reserving invalid and broadcast addresses, setting others
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static void i3c_init_address_table(uint8_t instance)
{
    uint8_t idx, bit_index;
    uint8_t addr, temp_addr;

    /* target address from 0 to 7 are reserved*/
    for (addr = 0U; addr <= 7U; addr++)
    {

        idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(addr);
        bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(addr);

        /* set the entry corresponding to the address 'addr'  as reserved */
        SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_RSVD);

        /* set the Addresses that are one bit different from the broadcast address
         * as reserved too.
         */
        temp_addr = I3C_BROADCAST_ADDR ^ (1U << addr);

        idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(temp_addr);
        bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(temp_addr);

        SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_RSVD);

    }

    /* Reserve the broadcast address as well */
    idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(I3C_BROADCAST_ADDR);
    bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(I3C_BROADCAST_ADDR);

    SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_RSVD);
    return;
}

/**
 * @brief Get initial address for the device, preferring static or preferred dynamic address, or find a free address.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] pdevice   Pointer to the device descriptor.
 * @return uint8_t      Initial (potential dynamic) address for the device.
 */
static uint8_t i3c_prepare_address(uint8_t instance,
        struct i3c_i3c_device *pdevice)
{
    uint8_t initial_address = 0U;

    if ((instance > I3C_NUM_INSTANCES) || (pdevice == NULL))
    {
        initial_address = -EINVAL;
    }

    if (pdevice->static_address != 0U)
    {
        if (get_address_allotment_table_entry(instance,
                pdevice->static_address) == ADDRESS_ENTRY_STATUS_FREE)
        {
            initial_address = pdevice->static_address;
        }
        else
        {
            initial_address = 0U;
        }
    }
    else
    {
        if ((pdevice->preferred_dynamic_address != 0U) &&
                (get_address_allotment_table_entry(instance,
                        pdevice->preferred_dynamic_address) ==
                ADDRESS_ENTRY_STATUS_FREE))
        {
            initial_address = pdevice->preferred_dynamic_address;
        }
        else
        {
            initial_address = get_next_free_address_allotment_table_entry(
                    instance);
        }
    }

    return initial_address;

}

/**
 * @brief Assign a free dynamic address to the controller.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return 0          If the operation was successful.
 */
static int32_t i3c_assign_own_da(uint8_t instance)
{
    uint8_t address, idx, bit_index;

    int32_t status = -EBUSY;

    address = get_next_free_address_allotment_table_entry(instance);
    if (address <= I3C_MAX_ADDR)
    {
        idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(address);
        bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(address);

        /* set the entry corresponding to the address 'addr'  as reserved */
        SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_I3C);

        i3c_obj[instance].own_da = address;

        status = 0;
    }

    return status;
}


/**
 * @brief Remove device from controller and update address allotment table.
 *
 * @param[in] instance     Instance of the I3C controller.
 * @param[in] pdevice_desc  Pointer to the device descriptor.
 * @param[in] address      Address of the device.
 * @return 0             If the operation was successful.
 */
static int32_t i3c_detach_device(uint8_t instance,
        struct i3c_device_desc *pdevice_desc,
        uint8_t address)
{
    uint8_t idx, bit_index;
    int32_t ret = 0;
    uint32_t status;

    /* use the address to check if the addressallotmenTable is set to I3C device*/
    idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(address);
    bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(address);

    /* set the entry corresponding to the address 'addr'  as reserved */
    status = GET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index);
    if (status == ADDRESS_ENTRY_STATUS_I3C)
    {
        /* set the entry to free*/
        SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_FREE);
    }
    /* Now remove the address from the device address table too*/
    ret = i3c_ll_detach_device(instance, pdevice_desc, address);

    return ret;
}

/**
 * @brief Add device to controller and update address allotment table.
 *
 * @param[in] instance     Instance of the I3C controller.
 * @param[in] pdevice_desc  Pointer to the device descriptor.
 * @param[in] address      Address of the device.
 * @return 0               If the operation was successful.
 */
static int32_t i3c_attach_i2c_device(uint8_t instance,
        struct i3c_device_desc *pdevice_desc,
        uint8_t address)
{

    uint8_t idx, bit_index;
    int32_t ret = 0;

    /* set the to entry corresponding to address as I2C device*/
    idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(address);
    bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(address);

    /* set the entry corresponding to the address 'addr'  as reserved */
    SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_I2C);

    ret = i3c_ll_attach_i2c_device(instance, pdevice_desc, address);

    return ret;
}

/**
 * @brief Add device to controller and update address allotment table.
 *
 * @param[in] instance     Instance of the I3C controller.
 * @param[in] pdevice_desc  Pointer to the device descriptor.
 * @param[in] address      Address of the device.
 * @return 0               If the operation was successful.
 */
static int32_t i3c_attach_device(uint8_t instance,
        struct i3c_device_desc *pdevice_desc,
        uint8_t address)
{
    uint8_t idx, bit_index;
    int32_t ret = 0;

    /* set the to entry corresponding to address as I3C device*/
    idx = GET_ADDRESS_ALLOTMENT_TABLE_INDEX(address);
    bit_index = GET_ADDRESS_ALLOTMENT_ENTRY_BIT_INDEX(address);

    /* set the entry corresponding to the address 'addr'  as reserved */
    SET_ADDRESS_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_I3C);

    ret = i3c_ll_attach_device(instance, pdevice_desc, address);

    return ret;
}

/**
 * @brief Add devices to the driver object for DASA and DAA checks.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] pdev_list  Pointer to the device list.
 * @return 0            If the operation was successful.
 * @return -EINVAL      If arguments were incorrect.
 */
static int32_t i3c_add_devices(uint8_t instance, struct i3c_dev_list *pdev_list)
{
    int32_t ret = 0;
    uint8_t dev_idx, i;

    if ((instance > I3C_NUM_INSTANCES) || (pdev_list == NULL))
    {
        ERROR("Invalid arguments");
        ret = -EINVAL;
    }

    for (dev_idx = 0; (ret == 0) && (dev_idx < pdev_list->num_devices);
            dev_idx++)
    {
        struct i3c_i3c_device *pdevice = &pdev_list->list[dev_idx];

        /* verify if the device is already attached to the controller*/
        for (i = 0; i < i3c_obj[instance].num_dev; i++)
        {
            /* Device ID is zero  means that it is a legacy I2C device*/
            if (i3c_obj[instance].i3c_dev_desc_list[i].device.device_id == 0U)
            {
                if (i3c_obj[instance].i3c_dev_desc_list[i].device.static_address
                        ==
                        pdevice->static_address)
                {
                    break;
                }
            }
            /*Non zero device IDs indicate that the device is an I3C device*/
            else
            {
                if (i3c_obj[instance].i3c_dev_desc_list[i].device.device_id ==
                        pdevice->device_id)
                {
                    break;
                }
            }
        }
        /* add the device to the attached list if it is a new device*/
        if (i >= i3c_obj[instance].num_dev)
        {

            /*Add the device to the attached device list in the controller object*/
            (void)memcpy(
                    (void *)&i3c_obj[instance].i3c_dev_desc_list[i3c_obj[
                        instance
                    ].
                    num_dev].device, pdevice, sizeof(struct i3c_i3c_device));
            i3c_obj[instance].num_dev++;

            /*Mark the address as taken in the address allotment table for I2C devices (Done inside the attach function)*/
            if (pdevice->device_id == 0U)
            {
                struct i3c_device_desc *pdevice_desc =
                        &i3c_obj[instance].i3c_dev_desc_list[i3c_obj[instance].
                                num_dev];
                ret = i3c_attach_i2c_device(instance, pdevice_desc,
                        pdevice->static_address);
            }
        }

    }

    return ret;
}


/**
 * @brief Perform DASA to assign dynamic addresses to devices with static addresses.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static void i3c_perform_dasa(uint8_t instance, bool *require_daa)
{
    uint8_t address, pos;
    struct i3c_device_desc *pdevice_desc;
    struct i3c_cmd_payload cmd_payload;
    int32_t ret;

    for (pos = 0U; pos < i3c_obj[instance].num_dev; pos++)
    {
        pdevice_desc = &i3c_obj[instance].i3c_dev_desc_list[pos];

        if ((pdevice_desc->device.static_address == 0U) ||
                (pdevice_desc->device.device_id == 0U))
        {
            *require_daa = true;
            continue;
        }
        /* prepare the address for the i3c device*/
        address = i3c_prepare_address(instance, &pdevice_desc->device);

        /* Prepare the DAT for the DASA command. For this we will attach the
         *  device to the controller with the initial address.
         * 1. add entry in the address allotment table (device address table [dev] = i3c Device;) /common part
         * 2. add the address in the free position in the device address table register (device address table [dev_idx] = address;) //LL part
         */
        ret = i3c_attach_device(instance, pdevice_desc, address);

        /* do dasa*/
        /* if both static address and preferred dynamicaddress is given,
         * DASA command is send to the target with preferred address
         * as dynamic address. If no preferred dynamic address
         * is given, dynamic address become same as static address.*/
        address = (pdevice_desc->device.preferred_dynamic_address != 0U)
                ? (pdevice_desc->
                device.
                preferred_dynamic_address)
                : (pdevice_desc->device.static_address);
        address = (address << 1U);    /* 7 bit address needs to left justified.*/

        /* set dynamic address for all this particular devices*/
        cmd_payload.cmd_id = I3C_CCC_SETDASA;              /* set DA using SA command*/
        cmd_payload.read = false;                        /* send(write) action*/
        cmd_payload.data = &address;
        cmd_payload.target_address = pdevice_desc->device.static_address;
        cmd_payload.data_length = 1U;

        /* Target address is used to get the index in the device address table, so
           prior to this the device address table should be populated with this static
           address */
        ret = i3c_ll_send_addr_assignment_command(instance, &cmd_payload,
                pdevice_desc->dat_index);
        if (ret == 0)
        {
            /* if successfully assigned the address, update it in the controller
               device list
             */
            pdevice_desc->device.dynamic_address = (address >> 1U);
        }
        if (pdevice_desc->device.dynamic_address !=
                pdevice_desc->device.static_address)
        {
            /* re-attach this device from static address position to the final
               dynamic Address position*/
            if (i3c_detach_device(instance, pdevice_desc, (address >> 1U)) != 0)
            {
                return;
            }

            /* attach back the device using the assigned dynamic address*/
            if (i3c_attach_device(instance, pdevice_desc,
                    pdevice_desc->device.dynamic_address) != 0)
            {
                return;
            }
        }
    }
    return;
}


/**
 * @brief Perform DAA to assign dynamic addresses to remaining devices after DASA.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static void i3c_perform_daa(uint8_t instance)
{
    uint8_t address, pos;
    struct i3c_device_desc *pdevice_desc;
    struct i3c_cmd_payload cmd_payload;
    uint32_t start_idx = 0U;
    uint8_t num_devices = 0U;
    int32_t ret = 0;

    for (pos = 0U; pos < i3c_obj[instance].num_dev; pos++)
    {
        pdevice_desc = &i3c_obj[instance].i3c_dev_desc_list[pos];

        if (pdevice_desc->device.static_address != 0U)
        {
            /* devices with static address would have been already
               assigned during DASA
             */
            continue;
        }
        /* prepare the address for the i3c device*/
        address = i3c_prepare_address(instance, &pdevice_desc->device);

        /* Prepare the DAT for the DAA command. For this we will attach the
         *  device to the controller with the initial address.
         * 1. add entry in the address allotment table (device address table [dev] = i3c Device;) /common part
         * 2. add the address in the free position in the device address table register (device address table [dev_idx] = address;) //LL part
         */
        ret = i3c_attach_device(instance, pdevice_desc, address);

        /* save the start index of the first device in the list for whch DAA is required.
           This is needed for sending the DAA command.
         */
        if (start_idx == 0U)
        {
            start_idx = pdevice_desc->dat_index;
        }

        /*storing the dynamic address, but should the actual command fails this needs to be cleared!!*/
        pdevice_desc->device.dynamic_address = address;

        num_devices++; /* number of devices that needs DAA*/
    }

    /* Prepare the device*/
    pdevice_desc = &i3c_obj[instance].i3c_dev_desc_list[pos];

    /* set dynamic address for all this particular devices*/
    cmd_payload.cmd_id = I3C_CCC_ENTDAA;              /* enter DAA command*/
    cmd_payload.read = false;                        /* send(write) action*/
    cmd_payload.data_length = num_devices;

    ret = i3c_ll_send_addr_assignment_command(instance, &cmd_payload,
            start_idx);
    if (ret != 0)
    {
        for (pos = 0U; pos < i3c_obj[instance].num_dev; pos++)
        {
            pdevice_desc = &i3c_obj[instance].i3c_dev_desc_list[pos];

            if (pdevice_desc->device.static_address == 0U)
            {
                /*remove the devices from the controller*/
                if (i3c_detach_device(instance, pdevice_desc,
                        pdevice_desc->device.dynamic_address) != 0)
                {
                    return;
                }
            }
        }
    }

    return;
}


/**
 * @brief Initialize I3C bus and assign dynamic addresses to connected devices.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static void i3c_init_bus(uint8_t instance)
{
    uint8_t def_byte = 0;
    bool require_daa = false;
    struct i3c_cmd_payload cmd_payload;
    bool is_i2c = false;
    bool is_async = false;

    /* reset all connected devices, send ccc broadcast */
    INFO("Resetting all connected devices");
    def_byte = I3C_CCC_RSTACT_RESET_WHOLE_TARGET;

    cmd_payload.cmd_id = I3C_CCC_RSTACT(true);            /* reset action command*/
    cmd_payload.read = false;                            /* send(write) action*/
    cmd_payload.data = &def_byte;
    cmd_payload.data_length = sizeof(def_byte);
    cmd_payload.target_address = 0;

    if (i3c_ll_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
    {
        /* if failed to reset the whole device, try resetting only
         * the peripheral*/
        def_byte = I3C_CCC_RSTACT_PERIPHERAL_ONLY;
        if (i3c_ll_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
        {
            return;
        }
    }

    /* reset current DAA assignments*/
    INFO("Resetting dynamic address assignments");
    cmd_payload.cmd_id = I3C_CCC_RSTDAA;             /* reset DAA command*/
    cmd_payload.read = false;                       /* send(write) action*/
    cmd_payload.data = NULL;
    cmd_payload.data_length = 0;

    if (i3c_ll_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
    {
        return;
    }

    /* disable all events*/
    INFO("Disabling all events");
    def_byte = I3C_CCC_EVT_ALL;

    cmd_payload.cmd_id = I3C_CCC_DISEC(true); /* disable all events broadcast command*/
    cmd_payload.read = false;                /* send(write) action*/
    cmd_payload.data = &def_byte;
    cmd_payload.data_length = sizeof(def_byte);

    if (i3c_ll_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
    {
        return;
    }


    /* perform DASA*/
    i3c_perform_dasa(instance, &require_daa);

    if (require_daa == true)
    {
        i3c_perform_daa(instance);
    }
}

/**
 * @brief Get the assigned dynamic address of the device identified by its PID.
 *
 * @param[in] instance     Instance of the I3C controller.
 * @param[in] pdevice      Pointer to the device object. dynamic_address field of this struct
 *                         would be updated if a matching deviceID is found in the controller.
 * @return 0               If success.
 * @return -EINVAL         If deviceID is not found in the list of devices managed by controller.
 */
static int32_t i3c_validate_i2c_device_address(uint8_t instance,
        struct i3c_i3c_device *pi3c_device)
{
    uint8_t idx;
    int32_t ret = 0;


    for (idx = 0; idx < i3c_obj[instance].num_attached_dev; idx++)
    {
        if (i3c_obj[instance].i3c_dev_desc_list[idx].device.device_id == 0U)
        {
            if (i3c_obj[instance].i3c_dev_desc_list[idx].device.static_address
                    ==
                    pi3c_device->static_address)
            {
                break;
            }
        }
    }
    if (idx >= i3c_obj[instance].num_attached_dev)
    {
        ERROR("Invalid device");
        ret = -EINVAL;
    }

    return ret;
}

/**
 * @brief Retrieve the dynamic address assigned to the device identified by its PID.
 *
 * @param[in] instance     Instance of the I3C controller.
 * @param[in] pdevice      Pointer to the device object. dynamic_address field of this struct
 *                         would be updated if a matching deviceID is found in the controller.
 * @return 0               If success.
 * @return -EINVAL         If deviceID is not found in the list of devices managed by controller.
 */
static int32_t i3c_get_device_dyn_address(uint8_t instance,
        struct i3c_i3c_device *pdevice)
{
    uint8_t idx;
    int32_t ret = 0;

    for (idx = 0; idx < i3c_obj[instance].num_attached_dev; idx++)
    {
        if (i3c_obj[instance].i3c_dev_desc_list[idx].device.device_id ==
                pdevice->device_id)
        {
            pdevice->dynamic_address =
                    i3c_obj[instance].i3c_dev_desc_list[idx].device.
                    dynamic_address;
            break;
        }

    }
    if (idx >= i3c_obj[instance].num_attached_dev)
    {
        ERROR("Invalid device");
        ret = -EINVAL;
    }

    return ret;
}


int32_t i3c_open(uint8_t instance)
{
    if (instance >= I3C_NUM_INSTANCES)
    {
        return -EINVAL;
    }

    /* initialize the controller instance( i3c_controller[instance]) */
    (void)memset((void *)&i3c_obj[instance], 0, sizeof(struct i3c_driver_obj));

    /* initialise the address allotment table */
    i3c_init_address_table(instance);

    /* initialise the Transfer semaphore*/
    i3c_obj[instance].xfer_complete = osal_semaphore_create(
            &(i3c_obj[instance].xfer_sem_mem));

    /* mutex for Mutual exclusion for controller operations */
    i3c_obj[instance].lock = osal_semaphore_create(
            &(i3c_obj[instance].lock_mem));

    if (i3c_assign_own_da(instance) != 0)
    {
        return -EBUSY;
    }

    /* set up the pin mux and other resources,
     * also enable power and clock to the controller*/
    i3c_ll_init(instance);

    return 0;
}

int32_t i3c_ioctl(uint8_t instance, enum i3c_ioctl_request ioctl, void *pargs)
{
    int32_t status = 0;

    switch (ioctl)
    {
        case I3C_IOCTL_TARGET_ATTACH:
            INFO("Attaching I3C devices");
            struct i3c_dev_list *pdevices = (struct i3c_dev_list *)pargs;

            status = i3c_add_devices(instance, pdevices);
            break;

        case I3C_IOCTL_BUS_INIT:
            INFO("Initializing the bus");
            i3c_init_bus(instance);
            break;

        case I3C_IOCTL_DO_DAA:
            INFO("Performing DAA");
            i3c_perform_daa(instance);
            break;

        case I3C_IOCTL_GET_DYNADDRESS:
            INFO("Fetching I3C dynamic address");
            struct i3c_i3c_device *pdevice = (struct i3c_i3c_device *)pargs;

            status = i3c_get_device_dyn_address(instance, pdevice);
            break;

        case I2C_IOCTL_ADDRESS_VALID:
            INFO("Vaildating I2C address");
            struct i3c_i3c_device *pi3c_device = (struct i3c_i3c_device *)pargs;
            status = i3c_validate_i2c_device_address(instance, pi3c_device);
            break;

        default:
            INFO("Invalid IOCTL command");
            status = -EINVAL;
            break;
    }

    return status;
}

void i3c_set_callback(uint8_t instance,
        i3c_callback_t callback, void *param)
{
    if (instance > I3C_NUM_INSTANCES)
    {
        ERROR("Invalid I3C instance");
        return;
    }
    i3c_obj[instance].callback_fn = callback;
    i3c_obj[instance].cb_usercontext = param;
}

int32_t i3c_transfer_sync(uint8_t instance, uint8_t address,
        struct i3c_xfer_request *pxfer_request, uint8_t num_xfers,
        bool is_i2c)
{
    int32_t ret = 0;
    uint16_t total_rx = 0, total_tx = 0;
    uint8_t idx,i;
    bool is_async = false;

    struct i3c_cmd_payload cmd_payload[I3C_MAX_XFER];

    if ((num_xfers > I3C_MAX_XFER) || (num_xfers >
            i3c_obj[instance].cmd_fifo_depth))
    {
        ERROR("Invalid transfer argument");
        ret = -EINVAL;
    }

    if (i3c_obj[instance].is_busy == true)
    {
        ERROR("I3C bus is busy");
        return -EBUSY;
    }
    i3c_obj[instance].is_busy = true;

    for (idx = 0; (ret == 0) && (idx < i3c_obj[instance].num_attached_dev);
            idx++)
    {
        if (i3c_obj[instance].attached_dev_addr_list[idx] == address)
        {
            break;
        }
    }

    /* if the device is part of the attach device of the controller, perform the transaction*/
    if ((ret == 0) && (idx <= i3c_obj[instance].num_attached_dev))
    {
        for (i = 0; i < num_xfers; i++)
        {
            /* prepare the command payload*/
            cmd_payload[i].cmd_id = 0;
            cmd_payload[i].target_address = address;
            cmd_payload[i].read = pxfer_request[i].read;                        /* read transfer if true, write otherwise*/
            cmd_payload[i].data = pxfer_request[i].buffer;
            cmd_payload[i].data_length = pxfer_request[i].length;

            if (pxfer_request[i].read == true)
            {
                total_rx += pxfer_request[i].length;
            }
            else
            {
                total_tx += pxfer_request[i].length;
            }
        }
        INFO("Data transfer started");
        ret = i3c_ll_send_xfer_command(instance, &cmd_payload[0],
                num_xfers /* number of commands*/, is_i2c, is_async);
        if (ret != 0)
        {
            ERROR("Data Transfer failed");
            return -EIO;
        }
        INFO("Data transfer completed");

        for (i = 0; i < num_xfers; i++)
        {
            if (cmd_payload[i].read == true)
            {
                /* update the actual legnth read */
                pxfer_request[i].length = cmd_payload[i].data_length;
            }
        }
    }
    i3c_obj[instance].is_busy = false;
    return ret;
}

int32_t i3c_transfer_async(uint8_t instance, uint8_t address,
        struct i3c_xfer_request *pxfer_request, uint8_t num_xfers,
        bool is_i2c )
{
    int32_t ret = 0;
    uint16_t total_rx = 0, total_tx = 0;
    uint8_t idx,i;
    bool is_async = true;

    struct i3c_cmd_payload cmd_payload[I3C_MAX_XFER];

    if ((num_xfers > I3C_MAX_XFER) ||
            (num_xfers > i3c_obj[instance].cmd_fifo_depth))
    {
        ERROR("Invalid transfer argument");
        ret = -EINVAL;
    }

    if (i3c_obj[instance].is_busy == true)
    {
        ERROR("I3C bus is busy");
        return -EBUSY;
    }
    i3c_obj[instance].is_busy = true;

    for (idx = 0; (ret == 0) && (idx < i3c_obj[instance].num_attached_dev);
            idx++)
    {
        if (i3c_obj[instance].attached_dev_addr_list[idx] == address)
        {
            break;
        }
    }

    /* if the device is part of the attach device of the controller, perform the transaction*/
    if ((ret == 0) && (idx <= i3c_obj[instance].num_attached_dev))
    {
        for (i = 0; i < num_xfers; i++)
        {
            /* prepare the command payload*/
            cmd_payload[i].cmd_id         = 0;
            cmd_payload[i].target_address = address;
            cmd_payload[i].read          = pxfer_request[i].read;               /* read transfer if true, write otherwise*/
            cmd_payload[i].data          = pxfer_request[i].buffer;
            cmd_payload[i].data_length    = pxfer_request[i].length;

            if (pxfer_request[i].read == true)
            {
                total_rx += pxfer_request[i].length;
            }
            else
            {
                total_tx += pxfer_request[i].length;
            }
        }
        INFO("Data transfer started");
        ret = i3c_ll_send_xfer_command(instance, &cmd_payload[0],
                num_xfers, is_i2c, is_async);
        if (ret != 0)
        {
            ERROR("Data Transfer failed");
            return -EIO;
        }
    }
    return ret;
}
/* end of file */
