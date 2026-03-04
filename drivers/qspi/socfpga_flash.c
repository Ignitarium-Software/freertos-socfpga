/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for Flash
 */

/*
 * This the implementation of QSPI Flash driver for SoC FPGA. This flash driver has
 * a layered design as shown in the diagram below
 *
 *     +------------------------------+
 *     |        Flash Layer           |<---------
 *     +--------------+---------------+         |
 *                    ^                         V
 *                    |          +---------------------------+
 *                    |          |    Flash Adapt Layer      |
 *                    |          |  (SFDP Parse & Parameters)|
 *                    |          +--------------+------------+
 *                    |     --------------------|
 *                    |     |
 *                    v     v
 *     +------------------------------+
 *     |         QSPI Layer           |
 *     +--------------+---------------+
 *                    ^
 *                    |
 *                    v
 *     +------------------------------+
 *     |       QSPI LL Layer          |
 *     +--------------+---------------+
 *                    ^ ^
 *                    | |
 *                    | -----------------------
 *                    |                       |
 *                    v                       v
 *   +------------------------------------------------------+
 *   |  +--------------------------+ +--------------------+ |
 *   |  |  Flash Command Generator | |     QSPI SRAM      | |
 *   |  +--------------+-----------+ +--------------+-----+ |
 *   | (Hardware)                                           |
 *   +------------------------------------------------------+
 *
 *  - The flash layer provides the user APIs.
 *  - The SFDP parameters of the flash chip is retrieved by the flash adaptation layer.
 *  - The retrieved parameters are used by the flash layer to access the QSPI flash.
 *  - The QSPI layer implements the high level sequence for communicating with the
 *    QSPI device.
 *  - The LL (low level) driver implements the register sequences for programming the
 *    QPSI controller.
 *  - The LL driver talks to the Command Generator inside the controller to generate
 *    the QSPI commands.
 *  - The LL driver accesses the QSPI SRAM for transferring write / read data.
 */

#include <stdlib.h>
#include <stdio.h>
#include "osal_log.h"
#include "socfpga_flash_adapter.h"
#include "socfpga_flash.h"
#include "socfpga_qspi.h"

#define FLASH_MAX_WAIT_TIME    0xFFFFFFFFU        /*!< Maximum wait time for mutex lock. */

/*The Flash handle*/
struct flash_handle
{
    uint8_t slave_select;
    qspi_descriptor_t desc;
    flash_adapter_t *adapter;
    osal_mutex_def_t mutex_mem;
    osal_mutex_def_t sem_mem;
    BaseType_t is_open;
    flash_callback_t xflash_callback;
};

static struct flash_handle gflash_handle =
{
    .is_open = 0,
    .desc.is_busy = 0,
    .desc.sem = NULL,
    .desc.mutex = NULL,
};

static struct flash_adapter m25q_adapter =
{
    .device_id = 0,
    .sfdp = { { { 0 } } },
    .parse_sfdp = parse_m25_q_parameters,
};

static struct flash_adapter *adapter_list[] =
{
    &m25q_adapter
    /*Add other adapters*/
};

static int flash_read_sfdp(flash_handle_t pflash)
{
    uint64_t sfdp_header_raw, param_header_raw;
    uint32_t sfdp_header[2] =
    {
        0
    }, param_header[2] =
    {
        0
    }, signature;
    int32_t ret;
    struct sfdp_object *sfdp = &pflash->adapter->sfdp;

    ret = qspi_read_sfdp(SFDP_START_ADDR, SFDP_HEADER_SIZE, &sfdp_header[0]);

    if (ret != QSPI_OK)
    {
        ERROR("SFDP header read failed.");
        return -EIO;
    }

    sfdp_header_raw = (uint64_t)((((uint64_t)sfdp_header[1]) <<
            SFDP_HEADER_MSB_POS) | (uint64_t)sfdp_header[0]);
    signature = (uint32_t)((sfdp_header_raw >> SFDP_SGN_START_POS) &
            SFDP_HEADER_MASK);
    (void)memcpy(sfdp->std_header.signature, &signature, SFDP_SGN_NUM_BYTES);
    sfdp->std_header.min_rev = (uint8_t)(sfdp_header_raw >> (SFDP_MINREV_POS &
            SFDP_MINREV_MASK));
    sfdp->std_header.major_rev = (uint8_t)(sfdp_header_raw >>
            (SFDP_MAJORREV_POS & SFDP_MAJORREV_MASK));
    sfdp->std_header.num_parameter_tables = (uint8_t)(sfdp_header_raw >>
            (SFDP_NUM_PARAM_TABLES_POS & SFDP_NUM_PARAM_TABLES_MASK));


    ret = qspi_read_sfdp(SFDP_PARAM_START_ADDR, PARAM_HEADER_SIZE,
            &param_header[0]);

    if (ret != QSPI_OK)
    {
        ERROR("SFDP parameter header read failed");
        return -EIO;
    }

    param_header_raw = (uint64_t)((((uint64_t)param_header[1]) <<
            SFDP_PARAM_HEADER_MSB_POS) | (uint64_t)param_header[0]);
    for (uint8_t i = 0; i <  sfdp->std_header.num_parameter_tables; i++)
    {
        sfdp->param_header[i].parameter_length = (uint8_t)(param_header_raw >>
                (SFDP_PARAM_LEN_POS &
                SFDP_PARAM_LEN_MASK));
        sfdp->param_header[i].parameter_table_offset =
                (uint32_t)(param_header_raw >> (SFDP_PARAM_TABLE_OFFSET_POS &
                SFDP_PARAM_TABLE_OFFSET_MASK));
    }

    return 0;
}

flash_handle_t flash_open(uint32_t flash_num)
{

    int status;
    flash_handle_t flash_handle = &gflash_handle;
    if (flash_handle == NULL)
    {
        return NULL;
    }
    flash_handle->slave_select = (uint8_t)flash_num;

    if (flash_num > (MAX_FLASH_DEV - 1U))
    {
        ERROR("Invalid device number");
        return NULL;
    }

    if (flash_handle->is_open != 0)
    {
        ERROR("Device is already open");
        return NULL;
    }

    if (flash_num > ((sizeof(adapter_list) / sizeof(adapter_list[0])) - 1U))
    {
        ERROR("Invalid device number");
        return NULL;
    }

    flash_handle->adapter = adapter_list[flash_num];
    if (flash_handle->adapter == NULL)
    {
        ERROR("Adapter layer not available");
        return NULL;
    }

    status = flash_read_sfdp(flash_handle);

    if (status != QSPI_OK)
    {
        ERROR("SFDP parametes read failed");
        return NULL;
    }
    status = flash_handle->adapter->parse_sfdp(&flash_handle->desc,
            &flash_handle->adapter->sfdp);
    if (status != 0)
    {
        ERROR("SFDP parameters parsing failed");
        return NULL;
    }
    status = qspi_init(&flash_handle->desc);

    if (status != QSPI_OK)
    {
        ERROR("QSPI init failed");
        return NULL;
    }

    if (flash_handle->desc.mutex == NULL)
    {
        flash_handle->desc.mutex = osal_mutex_create(
                &flash_handle->mutex_mem);
        if (flash_handle->desc.mutex == NULL)
        {
            ERROR("Mutex create failed");
            return NULL;
        }
    }
    if (flash_handle->desc.sem == NULL)
    {
        flash_handle->desc.sem = osal_semaphore_create(&flash_handle->sem_mem);
        if (flash_handle->desc.sem == NULL)
        {
            ERROR("Semaphore create failed");
            return NULL;
        }
    }

    flash_handle->is_open = 1;

    return flash_handle;
}

int  flash_set_callback(flash_handle_t const flash_handle,
        flash_callback_t callback, void *puser_context)
{
    int ret;
    if (flash_handle == NULL)
    {
        ERROR("Invalid flash handle");
        return -EINVAL;
    }

    /*Register the call back in the Flash layer*/
    flash_handle->xflash_callback = callback;
    ret = qspi_set_callback(&flash_handle->desc, &flash_handle->xflash_callback,
            puser_context);

    if (ret != QSPI_OK)
    {
        ERROR("Failed to set callback");
        return -EINVAL;
    }

    return 0;

}

int flash_erase_sectors(flash_handle_t flash_handle, uint32_t address,
        uint32_t size)
{
    const uint32_t SECTOR_SIZE = 4096U;
    int32_t erase_count = 0;
    uint32_t sector_offset = address & (SECTOR_SIZE - 1U);
    uint32_t remain_len;

    if (flash_handle == NULL)
    {
        ERROR("Invalid flash handle");
        return -EINVAL;
    }
    INFO("Erasing %d bytes of data starting from offset 0x%x", size, address);
    /*If the size crosses to the next sector and the size is only 4k*/
    if (((sector_offset + flash_handle->desc.page_size) >= SECTOR_SIZE) &&
            (size <= SECTOR_SIZE))
    {
        remain_len = SECTOR_SIZE - sector_offset;
        if (qspi_erase(address) != 0)
        {
            ERROR("Erase failed");
            return -EIO;
        }
        address += remain_len;
        size += remain_len;
        erase_count++;
    }

    while (size > 0U)
    {
        if (qspi_erase(address) != 0)
        {
            ERROR("Erase failed");
            return -EIO;
        }
        if (size <= SECTOR_SIZE)
        {
            erase_count++;
            break;
        }
        /*Update the next sector size*/
        address += SECTOR_SIZE;
        size = (size > SECTOR_SIZE) ? size - SECTOR_SIZE : SECTOR_SIZE;
        erase_count++;
    }
    INFO("Erase completed");
    return erase_count;
}

int flash_write_sync(flash_handle_t flash_handle, uint32_t address,
        uint8_t *data, uint32_t size)
{

    int ret = 0;
    uint32_t w_count = 0;

    if ((flash_handle == NULL) || (data == NULL) || (size == 0U))
    {
        ERROR("Inavlid arguments");
        return -EINVAL;
    }
    if (osal_mutex_lock(flash_handle->desc.mutex, FLASH_MAX_WAIT_TIME))
    {
        if ((flash_handle->is_open) == 0)
        {
            ERROR("Device is not open");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EINVAL;
        }
        if (flash_handle->desc.is_busy != 0)
        {
            ERROR("Device is busy");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EBUSY;
        }
        flash_handle->desc.is_busy = true;
        if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
        {
            ERROR("Mutex unlock failed");
            return -ETIMEDOUT;
        }
    }
    flash_handle->desc.is_wr_op = true;
    flash_handle->desc.is_async = false;
#if QSPI_ENABLE_INT_MODE
    flash_handle->desc.buffer = data;
    flash_handle->desc.xfer_size = size;
    flash_handle->desc.bytes_left = size;
    flash_handle->desc.start_addr = address;

    INFO("Write sync started.Writing %d bytes of data starting from offset 0x%x",
            size, address);

    ret = qspi_indirect_write(address, data, size, &w_count);
    if (ret != QSPI_OK)
    {
        ERROR("Write failed");
        return -EIO;
    }

    flash_handle->desc.bytes_left -= w_count;
    flash_handle->desc.buffer += w_count;
    flash_handle->desc.start_addr += w_count;

    if (w_count < size)
    {
        if (size < QSPI_WRITE_WATER_LVL)
        {
            qspi_enable_int(QSPI_INDDONE);
        }
        else
        {
            qspi_enable_int(QSPI_INDDONE_AND_XFERBRCH);
        }
        if (osal_semaphore_wait(flash_handle->desc.sem, FLASH_MAX_WAIT_TIME) ==
                false)
        {
            ERROR("Write failed due to timeout");
            return -ETIMEDOUT;
        }
        qspi_disable_int(QSPI_XFER_LVLBRCH | QSPI_IND_OPDONE);
    }
    INFO("Write sync transfer completed");
#else
    INFO("Write sync transfer started");
    ret = qspi_indirect_write(address, data, size, &w_count);
    if (ret != QSPI_OK)
    {
        ERROR("Write failed");
        return -EIO;
    }
    INFO("Write sync transfer completed");
#endif
    flash_handle->desc.is_busy = false;
    return ret;
}

#if QSPI_ENABLE_INT_MODE
int flash_write_async(flash_handle_t flash_handle, uint32_t address,
        uint8_t *data, uint32_t size)
{
    uint32_t w_count = 0;
    int ret = 0;

    if ((flash_handle == NULL) || (data == NULL) || (size == 0U))
    {
        ERROR("Invalid arguments");
        return -EINVAL;
    }
    if (osal_mutex_lock(flash_handle->desc.mutex, FLASH_MAX_WAIT_TIME))
    {
        if (flash_handle->is_open == 0)
        {
            ERROR("Device is not open");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EINVAL;
        }
        if (flash_handle->desc.is_busy != 0)
        {
            ERROR("Devie is busy");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EBUSY;
        }
        flash_handle->desc.is_busy = true;
        if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
        {
            ERROR("Mutex unlock failed");
            return -ETIMEDOUT;
        }
    }

    flash_handle->desc.is_wr_op = true;
    flash_handle->desc.is_async = true;

    flash_handle->desc.buffer = data;
    flash_handle->desc.xfer_size = size;
    flash_handle->desc.bytes_left = size;
    flash_handle->desc.start_addr = address;

    INFO("Write async started. Writing %d bytes of data starting from offset 0x%x",
            size, address);

    ret = qspi_indirect_write(address, data, size, &w_count);
    if (ret != QSPI_OK)
    {
        ERROR("Write failed");
        return -EIO;
    }

    flash_handle->desc.bytes_left -= w_count;
    flash_handle->desc.buffer += w_count;
    flash_handle->desc.start_addr += w_count;

    if (w_count <= size)
    {
        if (size < QSPI_WRITE_WATER_LVL)
        {
            qspi_enable_int(QSPI_INDDONE);
        }
        else
        {
            qspi_enable_int(QSPI_INDDONE_AND_XFERBRCH);
        }
    }
    return 0;
}
#endif

int flash_read_sync(flash_handle_t flash_handle, uint32_t address,
        uint8_t *buffer, uint32_t size)
{

    int ret = 0;
    uint32_t r_count = 0;
    if ((flash_handle == NULL) || (size == 0U) || (buffer == NULL))
    {
        ERROR("Invalid arguments");
        return -EINVAL;
    }
    if (osal_mutex_lock(flash_handle->desc.mutex, FLASH_MAX_WAIT_TIME))
    {
        if ((flash_handle->is_open) == 0)
        {
            ERROR("Device is not open");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EINVAL;
        }
        if (flash_handle->desc.is_busy != 0)
        {
            ERROR("Device is busy");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EBUSY;
        }
        flash_handle->desc.is_busy = true;
        if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
        {
            ERROR("Failed to unlock mutex");
            return -ETIMEDOUT;
        }
    }
    flash_handle->desc.is_wr_op = false;
    flash_handle->desc.is_async = false;
#if QSPI_ENABLE_INT_MODE
    flash_handle->desc.buffer = buffer;
    flash_handle->desc.xfer_size = size;
    flash_handle->desc.bytes_left = size;
    flash_handle->desc.start_addr = address;

    INFO("Read sync transfer started. Reading %d bytes of data starting from the offset 0x%x",
            size, address);
    qspi_enable_int(QSPI_INDDONE_AND_XFERBRCH);
    ret = qspi_indirect_read(address, buffer, size, &r_count);
    if (ret != QSPI_OK)
    {
        ERROR("Read failed");
        return -EIO;
    }

    flash_handle->desc.bytes_left -= r_count;
    flash_handle->desc.buffer += r_count;
    flash_handle->desc.start_addr += r_count;

    if (r_count < size)
    {
        if (osal_semaphore_wait(flash_handle->desc.sem, FLASH_MAX_WAIT_TIME) ==
                false)
        {
            ERROR("Read failed due to timesout");
            return -ETIMEDOUT;
        }
    }
    qspi_disable_int(QSPI_XFER_LVLBRCH | QSPI_IND_OPDONE);
    INFO("Read sync transfer completed");
#else
    INFO("Read sync transfer started");
    ret = qspi_indirect_read(address, buffer, size, &r_count);
    if (ret != QSPI_OK)
    {
        ERROR("Read failed");
        return -EIO;
    }
    INFO("Read sync transfer completed");
#endif
    flash_handle->desc.is_busy = false;
    return ret;
}

#if QSPI_ENABLE_INT_MODE
int flash_read_async(flash_handle_t flash_handle, uint32_t address,
        uint8_t *buffer, uint32_t size)
{
    uint32_t r_count = 0;
    int ret;

    if ((flash_handle == NULL) || (size == 0U) || (buffer == NULL))
    {
        ERROR("Invalid arguments");
        return -EINVAL;
    }
    if (osal_mutex_lock(flash_handle->desc.mutex, FLASH_MAX_WAIT_TIME))
    {
        if (flash_handle->is_open == 0)
        {
            ERROR("Device is not open");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EINVAL;
        }
        if (flash_handle->desc.is_busy != 0)
        {
            ERROR("Device is busy");
            if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
            {
                ERROR("Mutex unlock failed");
                return -ETIMEDOUT;
            }
            return -EBUSY;
        }
        flash_handle->desc.is_busy = true;
        if (osal_mutex_unlock(flash_handle->desc.mutex) == false)
        {
            ERROR("Mutex lock failed");
            return -ETIMEDOUT;
        }
    }
    flash_handle->desc.is_wr_op = false;
    flash_handle->desc.is_async = true;
    flash_handle->desc.buffer = buffer;
    flash_handle->desc.xfer_size = size;
    flash_handle->desc.bytes_left = size;
    flash_handle->desc.start_addr = address;

    qspi_enable_int(QSPI_INDDONE_AND_XFERBRCH);

    INFO("Read ssync transfer started. Reading %d bytes of data starting from the offset 0x%x",
            size, address);
    ret = qspi_indirect_read(address, buffer, size, &r_count);
    if (ret != QSPI_OK)
    {
        ERROR("Read failed");
        return -EIO;
    }

    flash_handle->desc.bytes_left -= r_count;
    flash_handle->desc.buffer += r_count;
    flash_handle->desc.start_addr += r_count;

    return 0;
}
#endif

int flash_close(flash_handle_t flash_handle)
{
    if ((flash_handle == NULL))
    {
        ERROR("Invalid flash handle");
        return -EINVAL;
    }

    if (flash_handle->is_open == 0)
    {
        ERROR("Device is not open");
        return -EINVAL;
    }

    if (qspi_deinit() != QSPI_OK)
    {
        ERROR("QSPI deinit failed");
        return -EIO;
    }

    if (osal_mutex_delete(flash_handle->desc.mutex) == false)
    {
        ERROR("Mutex destroy failed");
        return -EFAULT;
    }
    if (osal_semaphore_delete(flash_handle->desc.sem) == false)
    {
        ERROR("Semaphore destroy failed");
        return -EFAULT;
    }
    flash_handle->adapter = NULL;
    (void)memset(flash_handle, 0, sizeof(*flash_handle));

    return 0;
}
