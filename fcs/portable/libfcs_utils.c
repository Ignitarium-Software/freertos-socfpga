// SPDX-License-Identifier: MIT-0
/*
 * Copyright (C) 2025 Altera
 */

#include "libfcs_utils.h"
#include "libfcs_logging.h"
#include "ff_sddisk.h"

#define MOUNTED          1
#define UNMOUNTED        0
#define MOUNT_SD_CARD    -1
#define MOUNT_USB        0

/* Names of mount points */
static const char *sdmmc_mount = "/sd/";
static const char *usb_mount = "/usb/";
static const char fdt_name[] = "N/A";

FF_Disk_t *xDiskObj = NULL;
uint8_t ucMountStatus = UNMOUNTED;

static int fat_get_disk_type(const char *mptr)
{
    if (strncmp(mptr, sdmmc_mount, 4) == 0)
    {
        return 0;
    }
    else if (strncmp(mptr, usb_mount, 5) == 0)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

static int fat_mount( const char *MountName )
{
    int mount_type = fat_get_disk_type(MountName);
    if (mount_type == 0)
    {
        xDiskObj = FF_SDDiskInit(MountName, MOUNT_SD_CARD);
    }
    else if (mount_type == 1)
    {
        xDiskObj = FF_SDDiskInit(MountName, MOUNT_USB);
    }
    else
    {
        ERROR("Invalid mount point");
        return -1;
    }
    if (xDiskObj != NULL)
    {
        ucMountStatus = MOUNTED;
        return mount_type;
    }
    else
    {
        ERROR("Mounting Failed");
        return -1;
    }
}

static void fat_unmount( void )
{
    if (xDiskObj == NULL)
    {
        ERROR("No mounted devices");
        return;
    }
    (void) FF_Unmount(xDiskObj);
    (void) FF_SDDiskDelete(xDiskObj);
    xDiskObj = NULL;
    ucMountStatus = UNMOUNTED;
}
uint32_t fat_get_size( const char *file )
{
    int ret;
    uint32_t file_size;
    FF_Error_t xError;
    FF_FILE *pxFile;
    ret = fat_mount(file);
    if (xDiskObj == NULL)
    {
        ERROR("Failed to mount");
        return 0;
    }
    if (ret == 0)
    {
        pxFile =
                FF_Open(xDiskObj->pxIOManager, file + 3, FF_MODE_READ, &xError);
    }
    else
    {
        pxFile =
                FF_Open(xDiskObj->pxIOManager, file + 4, FF_MODE_READ, &xError);
    }
    if ((pxFile == NULL) || (xError != FF_ERR_NONE))
    {
        INFO("Failed to open file for reading\r\n");
        (void) FF_Unmount(xDiskObj);
        (void) FF_SDDiskDelete(xDiskObj);
        return 0;
    }

    ret = FF_GetFileSize(pxFile, &file_size);
    if (ret != 0)
    {
        ERROR("Error getting file size ");
        return 0;
    }
    (void) FF_Close(pxFile);
    return file_size;
}
uint32_t fat_read( const char *file, void *buffer )
{
    FF_Error_t xError;
    int32_t BytesRead = 0;
    FF_FILE *pxFile;
    int ret;
    ret = fat_mount(file);
    if (xDiskObj == NULL)
    {
        ERROR("Failed to mount");
        return 0;
    }
    if (ret == 0)
    {
        pxFile = FF_Open(xDiskObj->pxIOManager, file + 3,
                FF_MODE_READ, &xError);
    }
    else if (ret == 1)
    {
        pxFile = FF_Open(xDiskObj->pxIOManager, file + 4,
                FF_MODE_READ, &xError);
    }
    else
    {
        ERROR("Invalid mount point");
        return 0;
    }
    if (pxFile != NULL)
    {
        BytesRead = FF_Read(pxFile, 1, pxFile->ulFileSize, (uint8_t*)buffer);
        (void) FF_Close(pxFile);
    }
    fat_unmount();
    return (uint32_t) BytesRead;
}

const FCS_OSAL_CHAR *fcs_fdt_get_name(FCS_OSAL_CHAR *fit, FCS_OSAL_INT noffset, FCS_OSAL_INT *len)
{
	return fdt_name;
}

