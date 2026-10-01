/**
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for I3C
 */

#include <errno.h>
#include <string.h>
#include "osal_log.h"
#include "osal.h"
#include "socfpga_interrupt.h"
#include "socfpga_defines.h"
#include "socfpga_i3c.h"
#include "socfpga_i3c_ll.h"

#define GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr)    (((addr) << 1U) % \
    NUM_BITS_PER_TABLE_ENTRY)
#define GET_ADDR_ALLOTMENT_TABLE_INDEX(addr)        (((addr) << 1U) / \
    NUM_BITS_PER_TABLE_ENTRY)

#define SET_ADDR_ALLOTMENT_ENTRY(idx, pos, status)    do {                                                                                                     \
        i3c_desc[(instance)].addr_allotment_table[(idx)] &= \
                ~((uint32_t)ADDRESS_ENTRY_STATUS_MAX << (pos)); \
        i3c_desc[(instance)].addr_allotment_table[(idx)] |= \
                ((uint32_t)(status) << (pos)); \
}while(false)


#define GET_ADDR_ALLOTMENT_ENTRY(idx, pos) \
    ((i3c_desc[(instance)].addr_allotment_table[(idx)] >> (pos)) & \
    ADDRESS_ENTRY_STATUS_MAX)

/**
 * @brief I3C_CCC Common Command Codes (CCC)
 */
#define I3C_CCC_RSTDAA               (0x06U)
#define I3C_CCC_RSTACT(broadcast)    ((broadcast) ? 0x2AU : 0x9AU)
#define I3C_CCC_SETDASA              (0x87U)
#define I3C_CCC_ENTDAA               (0x07U)

/**
 * @brief I3C_CCC_Events Enable Events
 */
#define I3C_CCC_EVT_INTR    ((uint32_t)1 << 0U)
#define I3C_CCC_EVT_CR      ((uint32_t)1 << 1U)
#define I3C_CCC_EVT_HJ      ((uint32_t)1 << 3U)

#define I3C_CCC_EVT_ALL    (I3C_CCC_EVT_INTR | I3C_CCC_EVT_CR | I3C_CCC_EVT_HJ)

#define I3C_CCC_ENEC(broadcast)     ((broadcast) ? 0x00U : 0x80U)
#define I3C_CCC_DISEC(broadcast)    ((broadcast) ? 0x01U : 0x81U)

#define I3C_CCC_RSTACT_PERIPHERAL_ONLY       (0x01U)
#define I3C_CCC_RSTACT_RESET_WHOLE_TARGET    (0x02U)

#define I3C_HOT_JOIN_ADDR    (0x02U)

#define I3C_IBI_RNW_MASK         (0x01U)
#define I3C_IBI_NACK_STS_MASK    (0x08U)

#define I3C_DEVICE_ID_48_MASK    (0x0000FFFFFFFFFFFFULL)


/* I3C Controller Instance object  */
struct i3c_descriptor
{
    uint8_t instance;
    uint32_t base_addr;
    uint32_t dat_base;
    uint32_t dct_base;
    uint32_t cmd_fifo_depth;
    uint32_t data_fifo_depth;
    bool is_primary;
    uint8_t own_da;
    uint32_t *addr_allotment_table;
    uint16_t num_dev;
    struct i3c_device_desc *i3c_dev_desc_list;
    uint32_t num_xfers;
    bool is_async;
    int32_t xfer_result;
    bool is_busy;
    i3c_callback_t callback_fn;
    void *cb_usercontext;
    i3c_ibi_callback_t ibi_callback_fn;
    void *ibi_cb_usercontext;
    osal_mutex_def_t mutex_mem;
    osal_mutex_t mutex;
    osal_semaphore_def_t xfer_sem_mem;
    osal_semaphore_t xfer_complete;
    bool is_open;
};

static uint32_t i3c_addr_allotment_table[I3C_NUM_INSTANCES]
        [((I3C_MAX_ADDR + 1U) * 2U) / NUM_BITS_PER_TABLE_ENTRY];
static struct i3c_device_desc i3c_dev_desc_store[I3C_NUM_INSTANCES]
        [I3C_MAX_DEVICES];
static struct i3c_descriptor i3c_desc[I3C_NUM_INSTANCES] = { 0 };
static bool i3c_isr_registered[I3C_NUM_INSTANCES] = { false };

void i3c_isr(void *param);

/**
 * @brief Delete OSAL primitives created for an I3C instance.
 *
 * @param[in] hi3c I3C handle.
 */

static void i3c_delete_osal_primitives(i3c_handle_t hi3c)
{
    if (hi3c == NULL)
    {
        return;
    }

    if (hi3c->mutex != NULL)
    {
        osal_mutex_delete(hi3c->mutex);
        hi3c->mutex = NULL;
    }

    if (hi3c->xfer_complete != NULL)
    {
        osal_semaphore_delete(hi3c->xfer_complete);
        hi3c->xfer_complete = NULL;
    }
}

/**
 * @brief Detach a DAT entry for a device.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] pdevice_desc Device descriptor.
 * @param[in] addr Device address.
 * @return 0 on success, negative errno on failure.
 */

static int32_t i3c_detach_dat_entry(uint8_t instance,
        struct i3c_device_desc *pdevice_desc, uint8_t addr)
{
    uint32_t idx = pdevice_desc->dat_index;
    int32_t ret;

    ret = i3c_ll_detach_dat(instance, i3c_desc[instance].base_addr,
            i3c_desc[instance].dat_base, idx, addr);
    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

/**
 * @brief Process transfer response queue and aggregate status.
 *
 * @param[in] instance I3C controller instance.
 * @param[out] error Aggregated transfer status.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_process_xfer_response(uint8_t instance, int32_t *error)
{
    i3c_ll_cmd_response_t responses[I3C_MAX_XFER];
    i3c_ll_cmd_response_t status;
    uint8_t i;
    uint8_t num_response;
    uint8_t idx;

    if ((instance >= I3C_NUM_INSTANCES) || (error == NULL))
    {
        return -EINVAL;
    }

    *error = 0;
    (void)memset(responses, 0, sizeof(responses));
    num_response = i3c_ll_read_cmd_responses(instance,
            i3c_desc[instance].base_addr,
            responses, (uint8_t)i3c_desc[instance].num_xfers);
    for (i = 0U; i < num_response; i++)
    {
        status = responses[i];
        idx = status.tid;
        if (idx >= i3c_desc[instance].num_xfers)
        {
            *error = -EIO;
            continue;
        }

        switch (status.status)
        {
            case I3C_RESPONSE_OK:
                break;
            case I3C_RESPONSE_CRC_ERROR:
            case I3C_RESPONSE_PARITY_ERROR:
            case I3C_RESPONSE_FRAME_ERROR:
            case I3C_RESPONSE_BRAODCAST_NAK:
            case I3C_RESPONSE_XFER_ABORT:
                *error = -EIO;
                break;
            case I3C_RESPONSE_BUF_OVERFLOW:
                *error = -ENOMEM;
                break;
            default:
                *error = -EINVAL;
                break;
        }
    }

    return 0;
}

/**
 * @brief Handle transfer completion actions for sync/async flow.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] error Aggregated transfer status.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_handle_xfer_complete(uint8_t instance, int32_t error)
{
    if (instance >= I3C_NUM_INSTANCES)
    {
        return -EINVAL;
    }

    i3c_desc[instance].xfer_result = error;

    if (error != 0)
    {
        i3c_ll_reset_queues(i3c_desc[instance].base_addr);
        i3c_ll_resume(i3c_desc[instance].base_addr);
    }

    i3c_ll_disable_interrupt(i3c_desc[instance].base_addr,
            I3C_TX_THLD_STS_INTR);
    i3c_ll_complete_read_transfers(instance, i3c_desc[instance].base_addr,
            (uint8_t)i3c_desc[instance].num_xfers);

    if (i3c_desc[instance].is_async == false)
    {
        if (osal_semaphore_post(i3c_desc[instance].xfer_complete) == false)
        {
            return -EIO;
        }
    }
    else
    {
        i3c_desc[instance].is_busy = false;
        if (i3c_desc[instance].callback_fn != NULL)
        {
            i3c_desc[instance].callback_fn((error == 0) ? I3C_OK : error,
                    i3c_desc[instance].cb_usercontext);
        }
    }

    return 0;
}

/**
 * @brief Read transfer responses and complete sync/async transfer handling.
 *
 * @param[in] instance I3C controller instance.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_read_xfer_response(uint8_t instance)
{
    int32_t error;
    int32_t ret;

    error = 0;
    ret = i3c_process_xfer_response(instance, &error);
    if (ret != 0)
    {
        return ret;
    }

    ret = i3c_handle_xfer_complete(instance, error);
    if (ret != 0)
    {
        return ret;
    }

    return error;
}

/**
 * @brief Prepare transfer command metadata for sync/async operation.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] pcmd_payload Command payload array.
 * @param[in] num_cmds Number of commands.
 * @param[in] is_i2c True for I2C mode transfer.
 * @param[out] local_payload Local payload copy.
 * @param[out] dat_indices DAT indices for each command.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_prepare_transfer(uint8_t instance,
        struct i3c_cmd_payload *pcmd_payload, uint8_t num_cmds,
        bool is_i2c, struct i3c_cmd_payload *local_payload,
        uint8_t *dat_indices)
{
    struct i3c_cmd_payload *pcmd;
    struct i3c_device_desc *desc;
    uint8_t idx;
    uint8_t i;
    uint8_t match_addr;
    bool found;

    if ((instance >= I3C_NUM_INSTANCES) || (pcmd_payload == NULL) ||
            (local_payload == NULL) || (dat_indices == NULL) ||
            (num_cmds == 0U) || (num_cmds > I3C_MAX_XFER))
    {
        return -EINVAL;
    }

    pcmd = pcmd_payload;
    for (i = 0U; i < num_cmds; i++)
    {
        local_payload[i] = *pcmd;
        if (pcmd->target_addr == 0U)
        {
            local_payload[i].cmd_id &= (uint8_t)(~(1U << 7U));
            dat_indices[i] = 0U;
        }
        else
        {
            found = false;
            desc = NULL;
            match_addr = 0U;
            for (idx = 0U; idx < i3c_desc[instance].num_dev; idx++)
            {
                desc = &i3c_desc[instance].i3c_dev_desc_list[idx];
                if (desc->device.device_id == 0U)
                {
                    match_addr = desc->device.static_address;
                }
                else
                {
                    match_addr = desc->device.dynamic_address;
                }
                if (pcmd->target_addr == match_addr)
                {
                    idx = (uint8_t)desc->dat_index;
                    dat_indices[i] = idx;
                    found = true;
                    break;
                }
            }
            if ((found == false) || (idx >= I3C_MAX_DEVICES))
            {
                return -ENODEV;
            }
        }
        pcmd++;
    }

    return i3c_ll_prepare_transfer_batch(instance,
            local_payload, dat_indices, num_cmds, is_i2c);
}

/**
 * @brief Execute prepared transfer and collect sync results.
 *
 * @param[in] instance I3C controller instance.
 * @param[in,out] pcmd_payload Command payload array.
 * @param[in] num_cmds Number of commands.
 * @param[in] is_async True for async mode transfer.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_execute_transfer(uint8_t instance,
        struct i3c_cmd_payload *pcmd_payload, uint8_t num_cmds,
        bool is_async)
{
    int32_t ret;
    uint8_t i;

    i3c_desc[instance].num_xfers = num_cmds;
    i3c_desc[instance].is_async = is_async;
    i3c_desc[instance].xfer_result = 0;
    i3c_ll_start_transfer_batch(instance, i3c_desc[instance].base_addr,
            num_cmds);

    if (is_async)
    {
        return 0;
    }

    ret = osal_semaphore_wait(i3c_desc[instance].xfer_complete,
            (uint64_t)pdMS_TO_TICKS(10000U));
    i3c_ll_disable_interrupt(i3c_desc[instance].base_addr,
            I3C_TX_THLD_STS_INTR);

    if (ret == 0)
    {
        return -ETIMEDOUT;
    }

    ret = i3c_desc[instance].xfer_result;
    for (i = 0U; i < i3c_desc[instance].num_xfers; i++)
    {
            if (pcmd_payload[i].read == true)
        {
            pcmd_payload[i].data_length = i3c_ll_get_transfer_rx_length(instance,
                    i);
        }
    }

    return ret;
}

/**
 * @brief Build and submit transfer command(s) for sync or async operation.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] pcmd_payload Command payload array.
 * @param[in] num_cmds Number of commands.
 * @param[in] is_i2c True for I2C mode transfer.
 * @param[in] is_async True for async mode transfer.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_send_xfer_command(uint8_t instance,
        struct i3c_cmd_payload *pcmd_payload,
        uint8_t num_cmds, bool is_i2c, bool is_async)
{
    struct i3c_cmd_payload local_payload[I3C_MAX_XFER];
    uint8_t dat_indices[I3C_MAX_XFER];
    int32_t ret;

    ret = i3c_prepare_transfer(instance, pcmd_payload, num_cmds,
            is_i2c, local_payload, dat_indices);
    if (ret != 0)
    {
        return ret;
    }

    return i3c_execute_transfer(instance, pcmd_payload, num_cmds, is_async);
}

/**
 * @brief Prepare address assignment command for DASA/DAA flow.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] pcmd_payload Address assignment command payload.
 * @param[in] start_idx DAT start index for assignment flow.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_prepare_addr_assign(uint8_t instance,
        struct i3c_cmd_payload *pcmd_payload, uint32_t start_idx)
{
    i3c_ll_addr_assign_req_t req;
    int32_t ret;

    if ((instance >= I3C_NUM_INSTANCES) || (pcmd_payload == NULL) ||
            (start_idx >= I3C_MAX_DEVICES))
    {
        return -EINVAL;
    }

    req.cmd_id = pcmd_payload->cmd_id;
    req.start_dat_index = (uint8_t)start_idx;
    req.data = pcmd_payload->data;
    req.data_len = (uint8_t)pcmd_payload->data_length;
    ret = i3c_ll_submit_addr_assign(i3c_desc[instance].base_addr, instance,
            &req);

    if (ret != 0)
    {
        return ret;
    }

    i3c_desc[instance].num_xfers = 1U;

    return 0;
}

/**
 * @brief Complete address assignment command and collect response.
 *
 * @param[in] instance I3C controller instance.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_complete_addr_assign(uint8_t instance)
{
    int32_t ret;

    ret = osal_semaphore_wait(i3c_desc[instance].xfer_complete,
            (uint64_t)pdMS_TO_TICKS(10000U));
    if (ret == 0)
    {
        return -ETIMEDOUT;
    }

    return i3c_ll_complete_addr_assign(i3c_desc[instance].base_addr,
            instance,
            (uint8_t)i3c_desc[instance].num_xfers);
}

/**
 * @brief Send address assignment command for DASA/DAA flow.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] pcmd_payload Address assignment command payload.
 * @param[in] start_idx DAT start index for assignment flow.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_execute_addr_assign(uint8_t instance,
        struct i3c_cmd_payload *pcmd_payload, uint32_t start_idx)
{
    int32_t ret;

    ret = i3c_prepare_addr_assign(instance, pcmd_payload, start_idx);
    if (ret != 0)
    {
        return ret;
    }

    return i3c_complete_addr_assign(instance);
}


/**
 * @brief Get the status of the entry in the Address allotment table corresponding
 *        to the I3C address specified.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] address   Address for which the status is inquired.
 * @return uint8_t      Entry in the location corresponding to the address.
 */
static uint32_t get_addr_allotment_table_entry(uint8_t instance, uint8_t
        addr)
{
    uint8_t idx, bit_index;
    uint32_t status;

    idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(addr);
    bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr);

    status = GET_ADDR_ALLOTMENT_ENTRY(idx, bit_index);

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
static uint8_t get_next_free_addr_allotment_table_entry(uint8_t instance)
{
    uint8_t addr;

    for (addr = 8U; addr < I3C_BROADCAST_ADDR; addr++)
    {
        if (get_addr_allotment_table_entry(instance, addr) ==
                ADDRESS_ENTRY_STATUS_FREE)
        {
            break;
        }
    }

    if (addr >= I3C_BROADCAST_ADDR)
    {
        return 0U;
    }

    return addr;
}

/**
 * @brief Initialize address allotment table.
 *
 * Reserve addresses 0x00-0x07, broadcast address 0x7E and reserved address 0x7F.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static void i3c_init_addr_table(uint8_t instance)
{
    uint8_t idx, bit_index;
    uint8_t addr;

    /* target address from 0 to 7 are reserved */
    for (addr = 0U; addr <= 7U; addr++)
    {

        idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(addr);
        bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr);

        SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_RSVD);

    }

    /* Reserve the broadcast address and 0x7F as per 7-bit address map. */
    idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(I3C_BROADCAST_ADDR);
    bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(I3C_BROADCAST_ADDR);
    SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_RSVD);

    idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(I3C_MAX_ADDR);
    bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(I3C_MAX_ADDR);
    SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_RSVD);

    return;
}

/**
 * @brief Get initial address for the device, preferring static or preferred dynamic addr, or find a free address.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @param[in] pdevice   Pointer to the device descriptor.
 * @return uint8_t      Initial (potential dynamic) address for the device.
 */
static uint8_t i3c_prepare_addr(uint8_t instance,
        struct i3c_device *pdevice)
{
    uint8_t initial_addr = 0U;

    if ((instance >= I3C_NUM_INSTANCES) || (pdevice == NULL))
    {
        return 0U;
    }

    if (pdevice->static_address != 0U)
    {
        if (get_addr_allotment_table_entry(instance,
                pdevice->static_address) == ADDRESS_ENTRY_STATUS_FREE)
        {
            initial_addr = pdevice->static_address;
        }
        else
        {
            initial_addr = 0U;
        }
    }
    else
    {
        if ((pdevice->preferred_dynamic_address != 0U) &&
                (get_addr_allotment_table_entry(instance,
                        pdevice->preferred_dynamic_address) ==
                ADDRESS_ENTRY_STATUS_FREE))
        {
            initial_addr = pdevice->preferred_dynamic_address;
        }
        else
        {
            initial_addr = get_next_free_addr_allotment_table_entry(
                    instance);
        }
    }

    return initial_addr;

}

/**
 * @brief Assign a free dynamic address to the controller.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return 0          If the operation was successful.
 */
static int32_t i3c_assign_own_da(uint8_t instance)
{
    uint8_t addr, idx, bit_index;

    int32_t status = -EBUSY;

    addr = get_next_free_addr_allotment_table_entry(instance);
    if ((addr >= 8U) && (addr < I3C_BROADCAST_ADDR))
    {
        idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(addr);
        bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr);

        /* set the entry corresponding to the address 'addr'  as reserved */
        SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_I3C);

        i3c_desc[instance].own_da = addr;

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
        uint8_t addr)
{
    uint8_t idx, bit_index;
    int32_t ret = 0;
    uint32_t status;

    /* use the address to check if the addrallotmenTable is set to I3C device*/
    idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(addr);
    bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr);

    status = GET_ADDR_ALLOTMENT_ENTRY(idx, bit_index);
    if (status == ADDRESS_ENTRY_STATUS_I3C)
    {
        SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_FREE);
    }
    /* Now remove the address from the device address table too */
    ret = i3c_detach_dat_entry(instance, pdevice_desc, addr);

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
        uint8_t addr)
{

    uint8_t idx, bit_index;
    int32_t ret = 0;

    /* set the to entry corresponding to address as I2C device */
    idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(addr);
    bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr);

    SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_I2C);

    ret = i3c_ll_attach_dat_i2c(instance, i3c_desc[instance].base_addr,
            i3c_desc[instance].dat_base, addr, &pdevice_desc->dat_index);
    if (ret != 0)
    {
        SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_FREE);
        return ret;
    }

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
static int32_t i3c_attach_i3c_device(uint8_t instance,
        struct i3c_device_desc *pdevice_desc,
        uint8_t addr)
{
    uint8_t idx, bit_index;
    int32_t ret = 0;

    /* set the to entry corresponding to address as I3C device */
    idx = GET_ADDR_ALLOTMENT_TABLE_INDEX(addr);
    bit_index = GET_ADDR_ALLOTMENT_ENTRY_BIT_INDEX(addr);

    SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_I3C);

    ret = i3c_ll_attach_dat_i3c(instance, i3c_desc[instance].base_addr,
            i3c_desc[instance].dat_base, addr, &pdevice_desc->dat_index);
    if (ret != 0)
    {
        SET_ADDR_ALLOTMENT_ENTRY(idx, bit_index, ADDRESS_ENTRY_STATUS_FREE);
        return ret;
    }

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
    struct i3c_device_desc *pdevice_desc;

    if ((instance >= I3C_NUM_INSTANCES) || (pdev_list == NULL))
    {
        ERROR("Invalid arguments");
        return -EINVAL;
    }

    if ((pdev_list->num_devices > 0U) && (pdev_list->list == NULL))
    {
        ERROR("Invalid device list");
        return -EINVAL;
    }

    for (dev_idx = 0; (ret == 0) && (dev_idx < pdev_list->num_devices);
            dev_idx++)
    {
        struct i3c_device *pdevice = &pdev_list->list[dev_idx];
        uint64_t device_id = pdevice->device_id & I3C_DEVICE_ID_48_MASK;

        /* verify if the device is already attached to the controller */
        for (i = 0; i < i3c_desc[instance].num_dev; i++)
        {
            if (i3c_desc[instance].i3c_dev_desc_list[i].device.device_id == 0U)
            {
                if (i3c_desc[instance].i3c_dev_desc_list[i].device.static_address
                        ==
                        pdevice->static_address)
                {
                    break;
                }
            }
            else
            {
                if (i3c_desc[instance].i3c_dev_desc_list[i].device.device_id ==
                        device_id)
                {
                    break;
                }
            }
        }
        /* add the device to the attached list if it is a new device */
        if (i >= i3c_desc[instance].num_dev)
        {
            if (i3c_desc[instance].num_dev >= I3C_MAX_DEVICES)
            {
                return -ENOMEM;
            }

            pdevice_desc = &i3c_desc[instance].i3c_dev_desc_list[i3c_desc[instance].num_dev];
            (void)memset((void *)pdevice_desc, 0, sizeof(*pdevice_desc));

            /* Add the device to the attached device list in the controller object */
            (void)memcpy((void *)&pdevice_desc->device, pdevice,
                    sizeof(struct i3c_device));
            pdevice_desc->device.device_id &= I3C_DEVICE_ID_48_MASK;

            /* Mark the address as taken in the address allotment table for I2C devices */
            if (device_id == 0U)
            {
                ret = i3c_attach_i2c_device(instance, pdevice_desc,
                        pdevice->static_address);
            }

            if (ret == 0)
            {
                i3c_desc[instance].num_dev++;
            }
        }

    }

    return ret;
}

/**
 * @brief Select the DASA dynamic address for a static-address device.
 *
 * @param[in] desc I3C device descriptor.
 * @return Dynamic address candidate, or 0 if invalid input.
 */
static uint8_t i3c_select_dasa_dynamic_addr(const struct i3c_device_desc *desc)
{
    if ((desc == NULL) || (desc->device.static_address == 0U))
    {
        return 0U;
    }

    return desc->device.static_address;
}

/**
 * @brief Validate that a dynamic address is in valid I3C range.
 *
 * @param[in] addr Dynamic address.
 * @return true if valid, false otherwise.
 */
static bool i3c_is_valid_dynamic_addr(uint8_t addr)
{
    return ((addr >= 8U) && (addr < I3C_BROADCAST_ADDR));
}


/**
 * @brief Perform DASA to assign dynamic addresses to devices with static addresses.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static int32_t i3c_perform_dasa(uint8_t instance, bool *require_daa)
{
    uint8_t addr, pos;
    uint8_t initial_addr;
    uint8_t final_addr;
    struct i3c_device_desc *pdevice_desc;
    struct i3c_cmd_payload cmd_payload;
    int32_t ret;

    for (pos = 0U; pos < i3c_desc[instance].num_dev; pos++)
    {
        pdevice_desc = &i3c_desc[instance].i3c_dev_desc_list[pos];

        if ((pdevice_desc->device.static_address == 0U) ||
                (pdevice_desc->device.device_id == 0U))
        {
            *require_daa = true;
            continue;
        }
        /* prepare the address for the i3c device */
        initial_addr = i3c_prepare_addr(instance, &pdevice_desc->device);

        /* Attach device in DAT at initial address for DASA. */
        ret = i3c_attach_i3c_device(instance, pdevice_desc, initial_addr);
        if (ret != 0)
        {
            return ret;
        }

        /* Choose final DA for SETDASA. */
        final_addr = i3c_select_dasa_dynamic_addr(pdevice_desc);
        addr = (uint8_t)(final_addr << 1U);

        /* Set DA using SA command. */
        cmd_payload.cmd_id = I3C_CCC_SETDASA;
        cmd_payload.read = false;
        cmd_payload.data = &addr;
        cmd_payload.target_addr = pdevice_desc->device.static_address;
        cmd_payload.data_length = 1U;

        /* DAT must already contain the target static address. */
        ret = i3c_execute_addr_assign(instance, &cmd_payload,
                pdevice_desc->dat_index);
        if (ret == 0)
        {
            /* Update assigned dynamic address in descriptor. */
            pdevice_desc->device.dynamic_address = (addr >> 1U);
        }
        if (pdevice_desc->device.dynamic_address !=
                pdevice_desc->device.static_address)
        {
            /* Re-attach device from initial/static to final dynamic address. */
            if (i3c_detach_device(instance, pdevice_desc, initial_addr) != 0)
            {
                return -EIO;
            }

            /* attach back the device using the assigned dynamic address */
            if (i3c_attach_i3c_device(instance, pdevice_desc,
                    pdevice_desc->device.dynamic_address) != 0)
            {
                return -EIO;
            }
        }
    }
    return 0;
}


/**
 * @brief Perform DAA to assign dynamic addresses to remaining devices after DASA.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static int32_t i3c_perform_daa(uint8_t instance)
{
    uint8_t addr, pos, dev_idx;
    uint8_t daa_resp_buf[256];
    struct i3c_device_desc *pdevice_desc;
    struct i3c_cmd_payload cmd_payload;
    uint32_t start_idx = 0U;
    uint8_t num_devices = 0U;
    int32_t ret = 0;
    bool start_idx_valid = false;

    for (pos = 0U; pos < i3c_desc[instance].num_dev; pos++)
    {
        pdevice_desc = &i3c_desc[instance].i3c_dev_desc_list[pos];

        if (pdevice_desc->device.static_address != 0U)
        {
            /* Devices with static address are handled in DASA. */
            continue;
        }
        /* prepare the address for the i3c device */
        addr = i3c_prepare_addr(instance, &pdevice_desc->device);

        /* Attach candidate device in DAT before ENTDAA. */
        ret = i3c_attach_i3c_device(instance, pdevice_desc, addr);
        if (ret != 0)
        {
            return ret;
        }

        /* Save first DAT index for ENTDAA start. */
        if (start_idx_valid == false)
        {
            start_idx = pdevice_desc->dat_index;
            start_idx_valid = true;
        }

        pdevice_desc->device.dynamic_address = addr;
        num_devices++;
    }

    if (num_devices == 0U)
    {
        return 0;
    }

    /* Enter DAA command. */
    cmd_payload.cmd_id = I3C_CCC_ENTDAA;
    cmd_payload.read = false;
    cmd_payload.data = daa_resp_buf;
    cmd_payload.data_length = num_devices;

    ret = i3c_execute_addr_assign(instance, &cmd_payload,
            start_idx);
    if ((ret == 0) &&
            (i3c_ll_get_transfer_rx_length(instance, 0U) == num_devices))
    {
        dev_idx = 0U;
        for (pos = 0U; pos < i3c_desc[instance].num_dev; pos++)
        {
            pdevice_desc = &i3c_desc[instance].i3c_dev_desc_list[pos];
            if (pdevice_desc->device.static_address != 0U)
            {
                continue;
            }

            addr = (uint8_t)(daa_resp_buf[dev_idx] & I3C_MAX_ADDR);
            if (i3c_is_valid_dynamic_addr(addr) == false)
            {
                return -EIO;
            }

            if (pdevice_desc->device.dynamic_address != addr)
            {
                if (i3c_detach_device(instance, pdevice_desc,
                        pdevice_desc->device.dynamic_address) != 0)
                {
                    return -EIO;
                }

                pdevice_desc->device.dynamic_address = addr;
                if (i3c_attach_i3c_device(instance, pdevice_desc, addr) != 0)
                {
                    return -EIO;
                }
            }

            dev_idx++;
            if (dev_idx >= num_devices)
            {
                break;
            }
        }
    }

    if (ret != 0)
    {
        for (pos = 0U; pos < i3c_desc[instance].num_dev; pos++)
        {
            pdevice_desc = &i3c_desc[instance].i3c_dev_desc_list[pos];

            if (pdevice_desc->device.static_address == 0U)
            {
                /* remove the devices from the controller */
                if (i3c_detach_device(instance, pdevice_desc,
                        pdevice_desc->device.dynamic_address) != 0)
                {
                    return -EIO;
                }
            }
        }
    }

    return ret;
}


/**
 * @brief Initialize I3C bus and assign dynamic addresses to connected devices.
 *
 * @param[in] instance  Instance of the I3C controller.
 * @return None.
 */
static int32_t i3c_init_bus(uint8_t instance)
{
    uint8_t def_byte = 0;
    bool require_daa = false;
    struct i3c_cmd_payload cmd_payload;
    bool is_i2c = false;
    bool is_async = false;

    /* reset all connected devices, send ccc broadcast */
    INFO("Resetting all connected devices");
    def_byte = I3C_CCC_RSTACT_RESET_WHOLE_TARGET;

    cmd_payload.cmd_id = I3C_CCC_RSTACT(true);
    cmd_payload.read = false;
    cmd_payload.data = &def_byte;
    cmd_payload.data_length = sizeof(def_byte);
    cmd_payload.target_addr = 0;

    if (i3c_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
    {
        def_byte = I3C_CCC_RSTACT_PERIPHERAL_ONLY;
        if (i3c_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
        {
            return -EIO;
        }
    }

    /* reset current DAA assignments */
    INFO("Resetting dynamic address assignments");
    cmd_payload.cmd_id = I3C_CCC_RSTDAA;
    cmd_payload.read = false;
    cmd_payload.data = NULL;
    cmd_payload.data_length = 0;

    if (i3c_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
    {
        return -EIO;
    }

    /* disable all events */
    INFO("Disabling all events");
    def_byte = I3C_CCC_EVT_ALL;

    /* Disable all events broadcast command. */
    cmd_payload.cmd_id = I3C_CCC_DISEC(true);
    cmd_payload.read = false;
    cmd_payload.data = &def_byte;
    cmd_payload.data_length = sizeof(def_byte);

    if (i3c_send_xfer_command(instance, &cmd_payload, 1, is_i2c, is_async) != 0)
    {
        return -EIO;
    }


    /* perform DASA */
    if (i3c_perform_dasa(instance, &require_daa) != 0)
    {
        return -EIO;
    }

    if (require_daa == true)
    {
        return i3c_perform_daa(instance);
    }

    return 0;
}

/**
 * @brief Validate that an I2C static address belongs to an attached I2C target.
 *
 * @param[in] instance     Instance of the I3C controller.
 * @param[in] pi3c_device  I2C-style device descriptor containing static_address.
 * @return 0 on success, -EINVAL if the device is not attached.
 */
static int32_t i3c_validate_i2c_device_addr(uint8_t instance,
        struct i3c_device *pi3c_device)
{
    uint8_t idx;
    int32_t ret = 0;


    for (idx = 0; idx < i3c_desc[instance].num_dev; idx++)
    {
        if (i3c_desc[instance].i3c_dev_desc_list[idx].device.device_id == 0U)
        {
            if (i3c_desc[instance].i3c_dev_desc_list[idx].device.static_address ==
                    pi3c_device->static_address)
            {
                break;
            }
        }
    }
    if (idx >= i3c_desc[instance].num_dev)
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
static int32_t i3c_get_device_dyn_addr(uint8_t instance,
        struct i3c_device *pdevice)
{
    uint8_t idx;
    int32_t ret = 0;
    uint64_t device_id;

    if (pdevice == NULL)
    {
        return -EINVAL;
    }
    device_id = pdevice->device_id & I3C_DEVICE_ID_48_MASK;

    for (idx = 0; idx < i3c_desc[instance].num_dev; idx++)
    {
        if (i3c_desc[instance].i3c_dev_desc_list[idx].device.device_id ==
                device_id)
        {
            pdevice->dynamic_address =
                    i3c_desc[instance].i3c_dev_desc_list[idx].device.
                    dynamic_address;
            break;
        }

    }
    if (idx >= i3c_desc[instance].num_dev)
    {
        ERROR("Invalid device");
        ret = -EINVAL;
    }

    return ret;
}

/**
 *
 * @brief Lookup internal device descriptor for a target device object.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] pdevice Target device descriptor.
 * @return Pointer to device descriptor on success, NULL if not found.
 *
 */
static struct i3c_device_desc *i3c_get_device_desc(uint8_t instance,
        struct i3c_device *pdevice)
{
    uint8_t idx;
    uint64_t device_id;
    struct i3c_device_desc *desc;

    if (pdevice == NULL)
    {
        return NULL;
    }
    device_id = pdevice->device_id & I3C_DEVICE_ID_48_MASK;

    desc = NULL;
    for (idx = 0; idx < i3c_desc[instance].num_dev; idx++)
    {
        desc = &i3c_desc[instance].i3c_dev_desc_list[idx];
        if (device_id != 0U)
        {
            if (desc->device.device_id == device_id)
            {
                return desc;
            }
        }
        else
        {
            if (desc->device.static_address == pdevice->static_address)
            {
                return desc;
            }
        }
    }
    return NULL;
}

/**
 *
 * @brief Lookup internal descriptor by dynamic address for I3C device.
 *
 * @param[in] instance I3C controller instance.
 * @param[in] dynamic_addr Dynamic address from IBI source.
 * @return Pointer to descriptor on success, NULL if not found.
 *
 */
static struct i3c_device_desc *i3c_get_i3c_desc_by_dynamic_addr(uint8_t instance,
        uint8_t dynamic_addr)
{
    uint8_t idx;
    struct i3c_device_desc *desc;

    for (idx = 0U; idx < i3c_desc[instance].num_dev; idx++)
    {
        desc = &i3c_desc[instance].i3c_dev_desc_list[idx];
        if ((desc->device.device_id != 0U) &&
                (desc->device.dynamic_address == dynamic_addr))
        {
            return desc;
        }
    }

    return NULL;
}

/**
 *
 * @brief Read target BCR through directed CCC GETBCR.
 *
 * @param[in] instance I3C controller instance.
 * @param[in,out] pdevice Device object to update.
 * @return 0 on success, negative errno on failure.
 *
 */
static int32_t i3c_get_device_bcr(uint8_t instance,
        struct i3c_device *pdevice)
{
    if (pdevice == NULL)
    {
        return -EINVAL;
    }

    if (pdevice->dynamic_address == 0U)
    {
        return -EINVAL;
    }

    struct i3c_cmd_payload cmd_payload;
    bool is_i2c = false;
    bool is_async = false;
    uint8_t bcr = 0U;

    cmd_payload.cmd_id = 0x8EU;
    cmd_payload.read = true;
    cmd_payload.data = &bcr;
    cmd_payload.data_length = 1U;
    cmd_payload.target_addr = pdevice->dynamic_address;

    if (i3c_send_xfer_command(instance, &cmd_payload, 1, is_i2c,
            is_async) != 0)
    {
        return -EIO;
    }

    pdevice->bcr = bcr;
    return 0;
}

/**
 * @brief Validate an I3C handle and return its instance.
 *
 * @param[in] hi3c I3C handle.
 * @param[out] instance I3C instance.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_validate_handle(i3c_handle_t hi3c, uint8_t *instance)
{
    if ((hi3c == NULL) || (instance == NULL) || (hi3c->is_open == false))
    {
        return -EINVAL;
    }

    *instance = hi3c->instance;
    if (*instance >= I3C_NUM_INSTANCES)
    {
        return -EINVAL;
    }

    if (hi3c != &i3c_desc[*instance])
    {
        return -EINVAL;
    }

    return 0;
}

i3c_handle_t i3c_open(uint8_t instance)
{
    socfpga_hpu_interrupt_t int_id;
    int32_t ret;

    if (instance >= I3C_NUM_INSTANCES)
    {
        return NULL;
    }

    if (i3c_desc[instance].is_open == true)
    {
        return NULL;
    }

    /* initialize the controller instance( i3c_controller[instance]) */
    (void)memset((void *)&i3c_desc[instance], 0, sizeof(i3c_desc[instance]));
    i3c_desc[instance].instance = instance;
    i3c_desc[instance].addr_allotment_table = &i3c_addr_allotment_table[instance][0];
    i3c_desc[instance].i3c_dev_desc_list = &i3c_dev_desc_store[instance][0];

    (void)memset((void *)i3c_desc[instance].addr_allotment_table, 0,
            sizeof(i3c_addr_allotment_table[instance]));
    (void)memset((void *)i3c_desc[instance].i3c_dev_desc_list, 0,
            sizeof(i3c_dev_desc_store[instance]));
    i3c_ll_reset_dat_slots(instance);

    /* initialise the address allotment table */
    i3c_init_addr_table(instance);

    i3c_desc[instance].mutex = osal_mutex_create(
            &(i3c_desc[instance].mutex_mem));
    if (i3c_desc[instance].mutex == NULL)
    {
        return NULL;
    }

    /* initialise the Transfer semaphore*/
    i3c_desc[instance].xfer_complete = osal_semaphore_create(
            &(i3c_desc[instance].xfer_sem_mem));

    if (i3c_desc[instance].xfer_complete == NULL)
    {
        i3c_delete_osal_primitives(&i3c_desc[instance]);
        return NULL;
    }

    if (i3c_assign_own_da(instance) != 0)
    {
        i3c_delete_osal_primitives(&i3c_desc[instance]);
        return NULL;
    }

    /* Configure controller resources and enable clocks. */
    ret = i3c_ll_init(instance, i3c_desc[instance].own_da,
            &i3c_desc[instance].base_addr, &i3c_desc[instance].dat_base,
            &i3c_desc[instance].dct_base, &i3c_desc[instance].cmd_fifo_depth,
            &i3c_desc[instance].data_fifo_depth, &i3c_desc[instance].is_primary);
    if (ret != 0)
    {
        i3c_delete_osal_primitives(&i3c_desc[instance]);
        return NULL;
    }

    i3c_ll_set_default_data_thresholds(i3c_desc[instance].base_addr);
    i3c_ll_disable_interrupt(i3c_desc[instance].base_addr, I3C_ALL_STS_INTR);
    i3c_ll_clear_intr_status(i3c_desc[instance].base_addr, I3C_ALL_STS_INTR);
    i3c_ll_enable_interrupt(i3c_desc[instance].base_addr,
            (I3C_TRANSFER_ERR_STS_INTR | I3C_RESP_READY_STS_INTR |
            I3C_RX_THLD_STS_INTR | I3C_IBI_THLD_STS_INTR));
    i3c_ll_set_ibi_defaults(i3c_desc[instance].base_addr);

    int_id = GET_I3C_INTERRUPT_ID(instance);
    if (i3c_isr_registered[instance] == false)
    {
        if (interrupt_register_isr(int_id, i3c_isr,
                &i3c_desc[instance]) != 0)
        {
            (void)i3c_ll_deinit(instance, i3c_desc[instance].base_addr);
            i3c_delete_osal_primitives(&i3c_desc[instance]);
            return NULL;
        }
        i3c_isr_registered[instance] = true;
    }
    if (interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_I3C) != 0)
    {
        (void)interrupt_disable(int_id);
        (void)i3c_ll_deinit(instance, i3c_desc[instance].base_addr);
        i3c_delete_osal_primitives(&i3c_desc[instance]);
        return NULL;
    }

    i3c_desc[instance].is_open = true;
    i3c_desc[instance].is_busy = false;
    return &i3c_desc[instance];
}

int32_t i3c_close(i3c_handle_t hi3c)
{
    uint8_t instance;

    if (i3c_validate_handle(hi3c, &instance) != 0)
    {
        return -EINVAL;
    }

    (void)interrupt_disable(GET_I3C_INTERRUPT_ID(instance));
    i3c_ll_disable_interrupt(hi3c->base_addr, I3C_ALL_STS_INTR);
    i3c_ll_clear_intr_status(hi3c->base_addr, I3C_ALL_STS_INTR);
    i3c_ll_reset_queues(hi3c->base_addr);
    i3c_ll_resume(hi3c->base_addr);
    (void)i3c_ll_deinit(instance, hi3c->base_addr);
    hi3c->callback_fn = NULL;
    hi3c->cb_usercontext = NULL;
    hi3c->ibi_callback_fn = NULL;
    hi3c->ibi_cb_usercontext = NULL;
    hi3c->num_xfers = 0U;
    hi3c->is_async = false;
    hi3c->is_busy = false;
    hi3c->base_addr = 0U;
    hi3c->dat_base = 0U;
    hi3c->dct_base = 0U;
    hi3c->cmd_fifo_depth = 0U;
    hi3c->data_fifo_depth = 0U;
    hi3c->is_open = false;
    i3c_delete_osal_primitives(hi3c);

    return 0;
}

int32_t i3c_ioctl(i3c_handle_t hi3c, enum i3c_ioctl_request ioctl, void *pargs)
{
    int32_t status = 0;
    uint8_t instance;
    struct i3c_ibi_config *ibi_cfg;

    status = i3c_validate_handle(hi3c, &instance);
    if (status != 0)
    {
        return status;
    }

    switch (ioctl)
    {
        case I3C_IOCTL_TARGET_ATTACH:
            INFO("Attaching I3C devices");
            if ((hi3c->mutex == NULL) ||
                    (osal_mutex_lock(hi3c->mutex,
                    OSAL_TIMEOUT_WAIT_FOREVER) == false))
            {
                status = -EIO;
                break;
            }
            status = i3c_add_devices(instance, (struct i3c_dev_list *)pargs);
            if (osal_mutex_unlock(hi3c->mutex) == false)
            {
                status = -EIO;
            }
            break;

        case I3C_IOCTL_BUS_INIT:
            INFO("Initializing the bus");
            if ((hi3c->mutex == NULL) ||
                    (osal_mutex_lock(hi3c->mutex,
                    OSAL_TIMEOUT_WAIT_FOREVER) == false))
            {
                status = -EIO;
                break;
            }
            status = i3c_init_bus(instance);
            if (osal_mutex_unlock(hi3c->mutex) == false)
            {
                status = -EIO;
            }
            break;

        case I3C_IOCTL_DO_DAA:
            INFO("Performing DAA");
            if ((hi3c->mutex == NULL) ||
                    (osal_mutex_lock(hi3c->mutex,
                    OSAL_TIMEOUT_WAIT_FOREVER) == false))
            {
                status = -EIO;
                break;
            }
            status = i3c_perform_daa(instance);
            if (osal_mutex_unlock(hi3c->mutex) == false)
            {
                status = -EIO;
            }
            break;

        case I3C_IOCTL_GET_DYNADDRESS:
            INFO("Fetching I3C dynamic address");
            if (pargs == NULL)
            {
                status = -EINVAL;
                break;
            }
            status = i3c_get_device_dyn_addr(instance, (struct i3c_device *)pargs);
            break;

        case I2C_IOCTL_ADDRESS_VALID:
            INFO("Vaildating I2C address");
            if (pargs == NULL)
            {
                status = -EINVAL;
                break;
            }
            status = i3c_validate_i2c_device_addr(instance, (struct i3c_device *)pargs);
            break;

        case I3C_IOCTL_CONFIG_IBI:
            INFO("Configuring IBI");
            ibi_cfg = (struct i3c_ibi_config *)pargs;
            if ((ibi_cfg == NULL) || (ibi_cfg->dev == NULL))
            {
                status = -EINVAL;
                break;
            }
            if ((hi3c->mutex == NULL) ||
                    (osal_mutex_lock(hi3c->mutex,
                    OSAL_TIMEOUT_WAIT_FOREVER) == false))
            {
                status = -EIO;
                break;
            }
            if (ibi_cfg->enable)
            {
                status = i3c_enable_ibi(hi3c, ibi_cfg->dev);
            }
            else
            {
                status = i3c_disable_ibi(hi3c, ibi_cfg->dev);
            }
            if (osal_mutex_unlock(hi3c->mutex) == false)
            {
                status = -EIO;
            }
            break;

        default:
            INFO("Invalid IOCTL command");
            status = -EINVAL;
            break;
    }

    return status;
}

void i3c_set_callback(i3c_handle_t hi3c,
        i3c_callback_t callback, void *param)
{
    uint8_t instance;

    if (i3c_validate_handle(hi3c, &instance) != 0)
    {
        ERROR("Invalid I3C handle");
        return;
    }
    hi3c->callback_fn = callback;
    hi3c->cb_usercontext = param;
}

void i3c_set_ibi_callback(i3c_handle_t hi3c,
        i3c_ibi_callback_t callback, void *param)
{
    uint8_t instance;

    if (i3c_validate_handle(hi3c, &instance) != 0)
    {
        ERROR("Invalid I3C handle");
        return;
    }
    hi3c->ibi_callback_fn = callback;
    hi3c->ibi_cb_usercontext = param;
}

int32_t i3c_send_ccc(i3c_handle_t hi3c, uint8_t cmd_id,
        uint8_t target_addr, uint8_t *data, uint16_t data_length, bool read)
{
    struct i3c_cmd_payload cmd_payload;
    bool is_i2c = false;
    bool is_async = false;
    uint8_t instance;

    if (i3c_validate_handle(hi3c, &instance) != 0)
    {
        return -EINVAL;
    }

    cmd_payload.cmd_id = cmd_id;
    cmd_payload.read = read;
    cmd_payload.data = data;
    cmd_payload.data_length = data_length;
    cmd_payload.target_addr = target_addr;

    return i3c_send_xfer_command(instance, &cmd_payload, 1, is_i2c,
            is_async);
}

int32_t i3c_enable_ibi(i3c_handle_t hi3c, struct i3c_device *pdevice)
{
    int32_t ret;
    uint8_t enec;
    struct i3c_device_desc *desc;
    bool ibi_with_data;
    uint8_t instance;

    if (i3c_validate_handle(hi3c, &instance) != 0)
    {
        return -EINVAL;
    }

    if (pdevice == NULL)
    {
        return -EINVAL;
    }

    if (i3c_desc[instance].is_busy == true)
    {
        return -EBUSY;
    }

    ret = i3c_get_device_bcr(instance, pdevice);
    if (ret != 0)
    {
        return ret;
    }

    enec = I3C_CCC_EVT_INTR;
    ret = i3c_send_ccc(hi3c, I3C_CCC_ENEC(false), pdevice->dynamic_address,
            &enec, 1U, false);
    if (ret != 0)
    {
        return ret;
    }

    desc = i3c_get_device_desc(instance, pdevice);
    if (desc == NULL)
    {
        return -EINVAL;
    }

    desc->device.bcr = pdevice->bcr;
    desc->BCR = pdevice->bcr;
    ibi_with_data = ((pdevice->bcr & (1U << 2U)) != 0U);
    return i3c_ll_configure_ibi(i3c_desc[instance].base_addr,
            i3c_desc[instance].dat_base, desc->dat_index,
            pdevice->dynamic_address, true, ibi_with_data);
}

int32_t i3c_disable_ibi(i3c_handle_t hi3c, struct i3c_device *pdevice)
{
    int32_t ret;
    uint8_t disec;
    uint8_t instance;
    struct i3c_device_desc *desc;

    if (i3c_validate_handle(hi3c, &instance) != 0)
    {
        return -EINVAL;
    }

    if (pdevice == NULL)
    {
        return -EINVAL;
    }

    if (i3c_desc[instance].is_busy == true)
    {
        return -EBUSY;
    }

    desc = i3c_get_device_desc(instance, pdevice);
    if (desc == NULL)
    {
        return -EINVAL;
    }

    disec = I3C_CCC_EVT_INTR;
    ret = i3c_send_ccc(hi3c, I3C_CCC_DISEC(false), pdevice->dynamic_address,
            &disec, 1U, false);
    if (ret != 0)
    {
        return ret;
    }

    return i3c_ll_configure_ibi(i3c_desc[instance].base_addr,
            i3c_desc[instance].dat_base, desc->dat_index,
            pdevice->dynamic_address, false, false);
}

/**
 * @brief Validate and prepare transfer payload list for sync/async transfer.
 *
 * @param[in] hi3c I3C handle.
 * @param[in] addr Target address.
 * @param[in] pxfer_request Transfer request list.
 * @param[in] num_xfers Number of requests.
 * @param[out] cmd_payload Prepared command payloads.
 * @param[out] instance Controller instance.
 * @return 0 on success, negative errno on failure.
 */
static int32_t i3c_prepare_transfer_payload(i3c_handle_t hi3c, uint8_t addr,
        struct i3c_xfer_request *pxfer_request, uint8_t num_xfers,
        struct i3c_cmd_payload *cmd_payload, uint8_t *instance)
{
    int32_t ret;
    uint8_t idx;
    uint8_t i;
    struct i3c_device_desc *desc;

    ret = i3c_validate_handle(hi3c, instance);
    if (ret != 0)
    {
        return -EINVAL;
    }

    if ((hi3c->mutex == NULL) ||
            (osal_mutex_lock(hi3c->mutex, OSAL_TIMEOUT_WAIT_FOREVER) == false))
    {
        return -EIO;
    }

    if ((pxfer_request == NULL) || (cmd_payload == NULL) ||
            (num_xfers == 0U) || (num_xfers > I3C_MAX_XFER) ||
            (num_xfers > i3c_desc[*instance].cmd_fifo_depth))
    {
        (void)osal_mutex_unlock(hi3c->mutex);
        return -EINVAL;
    }

    if (i3c_desc[*instance].is_busy == true)
    {
        (void)osal_mutex_unlock(hi3c->mutex);
        return -EBUSY;
    }
    i3c_desc[*instance].is_busy = true;

    desc = NULL;
    for (idx = 0U; idx < i3c_desc[*instance].num_dev; idx++)
    {
        desc = &i3c_desc[*instance].i3c_dev_desc_list[idx];
        if (((desc->device.device_id == 0U) &&
                (desc->device.static_address == addr)) ||
                ((desc->device.device_id != 0U) &&
                (desc->device.dynamic_address == addr)))
        {
            break;
        }
    }

    if (idx >= i3c_desc[*instance].num_dev)
    {
        i3c_desc[*instance].is_busy = false;
        (void)osal_mutex_unlock(hi3c->mutex);
        return -ENODEV;
    }

    for (i = 0U; i < num_xfers; i++)
    {
        if ((pxfer_request[i].buffer == NULL) && (pxfer_request[i].length > 0U))
        {
            i3c_desc[*instance].is_busy = false;
            (void)osal_mutex_unlock(hi3c->mutex);
            return -EINVAL;
        }

        cmd_payload[i].cmd_id = 0U;
        cmd_payload[i].target_addr = addr;
        cmd_payload[i].read = pxfer_request[i].read;
        cmd_payload[i].data = pxfer_request[i].buffer;
        cmd_payload[i].data_length = pxfer_request[i].length;
    }

    if (osal_mutex_unlock(hi3c->mutex) == false)
    {
        return -EIO;
    }

    return 0;
}

int32_t i3c_transfer_sync(i3c_handle_t hi3c, uint8_t addr,
        struct i3c_xfer_request *pxfer_request, uint8_t num_xfers,
        bool is_i2c)
{
    int32_t ret;
    uint8_t i;
    bool is_async = false;
    uint8_t instance;
    struct i3c_cmd_payload cmd_payload[I3C_MAX_XFER];

    ret = i3c_prepare_transfer_payload(hi3c, addr, pxfer_request, num_xfers,
            cmd_payload, &instance);
    if (ret != 0)
    {
        return ret;
    }

    INFO("Data transfer started");
    ret = i3c_send_xfer_command(instance, &cmd_payload[0],
            num_xfers, is_i2c, is_async);
    if (ret != 0)
    {
        ERROR("Data Transfer failed");
        if ((hi3c->mutex != NULL) &&
                (osal_mutex_lock(hi3c->mutex, OSAL_TIMEOUT_WAIT_FOREVER) == true))
        {
            i3c_desc[instance].is_busy = false;
            (void)osal_mutex_unlock(hi3c->mutex);
        }
        return ret;
    }
    INFO("Data transfer completed");

    for (i = 0U; i < num_xfers; i++)
    {
        if (cmd_payload[i].read == true)
        {
            pxfer_request[i].length = cmd_payload[i].data_length;
        }
    }

    if ((hi3c->mutex == NULL) ||
            (osal_mutex_lock(hi3c->mutex, OSAL_TIMEOUT_WAIT_FOREVER) == false))
    {
        return -EIO;
    }
    i3c_desc[instance].is_busy = false;
    if (osal_mutex_unlock(hi3c->mutex) == false)
    {
        return -EIO;
    }
    return ret;
}

int32_t i3c_transfer_async(i3c_handle_t hi3c, uint8_t addr,
        struct i3c_xfer_request *pxfer_request, uint8_t num_xfers,
        bool is_i2c)
{
    int32_t ret;
    bool is_async = true;
    uint8_t instance;
    struct i3c_cmd_payload cmd_payload[I3C_MAX_XFER];

    ret = i3c_prepare_transfer_payload(hi3c, addr, pxfer_request, num_xfers,
            cmd_payload, &instance);
    if (ret != 0)
    {
        return ret;
    }

    INFO("Data transfer started");
    ret = i3c_send_xfer_command(instance, &cmd_payload[0],
            num_xfers, is_i2c, is_async);
    if (ret != 0)
    {
        ERROR("Data Transfer failed");
        if ((hi3c->mutex != NULL) &&
                (osal_mutex_lock(hi3c->mutex, OSAL_TIMEOUT_WAIT_FOREVER) == true))
        {
            i3c_desc[instance].is_busy = false;
            (void)osal_mutex_unlock(hi3c->mutex);
        }
    }

    return ret;
}

/**
 * @brief Interrupt handler for I3C transfer and IBI events.
 *
 * @param[in] param I3C handle pointer.
 */
void i3c_isr(void *param)
{
    i3c_handle_t i3c_handle = (i3c_handle_t)param;
    uint8_t instance = i3c_handle->instance;
    uint32_t status;
    uint32_t count;
    uint32_t ibi_status;
    uint8_t ibi_sts = 0U;
    uint8_t ibi_id = 0U;
    uint8_t data_len = 0U;
    uint8_t read_len = 0U;
    uint8_t ibi_addr;
    bool is_sir;
    bool is_ibi_acked;
    struct i3c_device_desc *desc;
    uint8_t payload[32];
    uint32_t clear_mask = 0U;

    status = i3c_ll_get_intr_status(i3c_handle->base_addr);

    if ((status & (I3C_TX_THLD_STS_INTR | I3C_RX_THLD_STS_INTR)) != 0U)
    {
        i3c_ll_service_transfer_thresholds(instance, i3c_handle->base_addr,
                (uint8_t)i3c_handle->num_xfers, status);
        clear_mask |= (status & (I3C_TX_THLD_STS_INTR | I3C_RX_THLD_STS_INTR));
    }

    if ((status & (I3C_TRANSFER_ERR_STS_INTR |
            I3C_RESP_READY_STS_INTR)) != 0U)
    {
        clear_mask |= I3C_RESP_READY_STS_INTR;
        clear_mask |= I3C_TRANSFER_ERR_STS_INTR;
        (void)i3c_read_xfer_response(instance);
    }

    if ((status & I3C_IBI_THLD_STS_INTR) != 0U)
    {
        clear_mask |= I3C_IBI_THLD_STS_INTR;
        count = i3c_ll_get_ibi_count(i3c_handle->base_addr);

        while (count > 0U)
        {
            (void)memset(payload, 0, sizeof(payload));
            read_len = 0U;
            ibi_status = i3c_ll_get_ibi_status(i3c_handle->base_addr);
            i3c_ll_get_ibi_fields(ibi_status, &ibi_sts, &ibi_id, &data_len);
            ibi_addr = (uint8_t)(ibi_id >> 1U);
            is_sir = ((ibi_addr != I3C_HOT_JOIN_ADDR) &&
                    ((ibi_id & I3C_IBI_RNW_MASK) != 0U));
            is_ibi_acked = ((ibi_sts & I3C_IBI_NACK_STS_MASK) == 0U);

            if ((is_sir == true) && (is_ibi_acked == true))
            {
                desc = i3c_get_i3c_desc_by_dynamic_addr(instance, ibi_addr);
                if (desc != NULL)
                {
                    read_len = i3c_ll_read_ibi_payload(i3c_handle->base_addr,
                            payload, data_len, (uint8_t)sizeof(payload));
                }
                else
                {
                    (void)i3c_ll_read_ibi_payload(i3c_handle->base_addr, NULL,
                            data_len, 0U);
                }
            }
            else
            {
                (void)i3c_ll_read_ibi_payload(i3c_handle->base_addr, NULL,
                        data_len, 0U);
            }

            if (i3c_handle->ibi_callback_fn != NULL)
            {
                i3c_handle->ibi_callback_fn(ibi_id, ibi_sts, payload,
                        read_len, i3c_handle->ibi_cb_usercontext);
            }

            count--;
        }
    }

    i3c_ll_clear_intr_status(i3c_handle->base_addr, clear_mask);
}
