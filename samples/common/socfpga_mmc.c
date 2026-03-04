/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Helper routines to load image files from storage
 */

#include "osal_log.h"
#include "ff_sddisk.h"
#include "socfpga_mmc.h"

uint8_t *mmc_read_file(media_source_t media_src, const char *file_name,
        uint32_t *file_size)
{
    const char *mmc_dev[4] = {"sdmmc", "usb3", "usb2", "invalid"};
    FF_Disk_t *pdisk;
    FF_FILE *file;
    FF_Error_t err;

    uint8_t *rbf_ptr;
    size_t bytes_read;
    int ret_val;
    int mount_drive_num;

    if (media_src == SOURCE_SDMMC)
    {
        mount_drive_num = DRIVE_NUM_SDMMC;
    }
    else if (media_src == SOURCE_USB3)
    {
        mount_drive_num = DRIVE_NUM_USB3;
    }
    else if (media_src == SOURCE_USB2)
    {
        /*need to define the value after USB2.0 is done*/
        ERROR("Invalid media source specified !!!");
        return NULL;
    }
    else
    {
        /*Invalid media source */
        ERROR("Invalid media source specified !!!");
        return NULL;
    }

    pdisk = FF_SDDiskInit(MOUNT_POINT, mount_drive_num);
    if (pdisk == NULL)
    {
        ERROR("Failed to initialize disk\n");
        return NULL;
    }

    err = FF_Mount(pdisk, 0);
    if (err != FF_ERR_NONE)
    {
        ERROR("Failed to mount filesystem\n");
        FF_SDDiskDelete(pdisk);
        return NULL;
    }

    file = FF_Open(pdisk->pxIOManager, file_name, FF_MODE_READ, &err);
    if ((file == NULL) || (err != FF_ERR_NONE))
    {
        ERROR("Failed to open file for reading\r\n");
        FF_Unmount(pdisk);
        FF_SDDiskDelete(pdisk);
        return NULL;
    }

    ret_val = FF_GetFileSize(file, file_size);
    if (ret_val != 0)
    {
        ERROR("Error getting file size ");
        return NULL;
    }

    rbf_ptr = (uint8_t *)pvPortMalloc(*file_size);
    if (rbf_ptr == NULL)
    {
        ERROR("Cannot allocate memory ");
        return NULL;
    }

    bytes_read = FF_Read(file, 1, *file_size, rbf_ptr);
    if ((bytes_read <= 0) || (err != FF_ERR_NONE))
    {
        ERROR("Failed to read data from file\n");
        vPortFree(rbf_ptr);
        FF_Close(file);
        FF_Unmount(pdisk);
        FF_SDDiskDelete(pdisk);
        return NULL;
    }
    FF_Close(file);

    err = FF_Unmount(pdisk);
    if (err != FF_ERR_NONE)
    {
        ERROR("Failed to unmount filesystem\n");
        vPortFree(rbf_ptr);
        FF_SDDiskDelete(pdisk);
        return NULL;
    }
    FF_SDDiskDelete(pdisk);

    PRINT("Read %x bytes from %s drive successfully", *file_size,
            mmc_dev[media_src]);

    /* return the fpga rbf ptr */
    return rbf_ptr;
}
