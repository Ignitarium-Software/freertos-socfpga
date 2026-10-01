/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 */

#include <string.h>
#include <errno.h>

#include "libfcs_filesys_ops.h"
#include "libfcs_logging.h"

#include "ff_sddisk.h"
#include "FreeRTOS.h"
#include "ff_sys.h"
#include "ff_stdio.h"
#include "ff_headers.h"

#define MOUNT_SD_CARD       -1
#define MOUNT_USB       0

static const char *sdmmc_mount = "/sd/";
static const char *usb_mount = "/usb/";

static FF_Disk_t *sd_disk_obj = NULL;
static FF_Disk_t *usb_disk_obj = NULL;
static uint32_t sd_count = 0;
static uint32_t usb_count = 0;

static int get_disk_type(const char *filename)
{
    if (strncmp(filename, sdmmc_mount, 4) == 0)
        return 0;

    if (strncmp(filename, usb_mount, 5) == 0)
        return 1;

    return -1;
}

static int mount_disk(const char *filename)
{
    int disk = get_disk_type(filename);
    if (disk < 0) {
        FCS_LOG_ERR("invalid mount point\n");
        return -1;
    }

    if (disk == 0) {
        if (sd_count > 0) {
            sd_count++;
            return 0;
        }

        sd_disk_obj = FF_SDDiskInit("root", MOUNT_SD_CARD);
        if (sd_disk_obj == NULL) {
            FCS_LOG_ERR("SD card init failed\n");
            return -1;
        }

        sd_count = 1;
    } else {
        if (usb_count > 0) {
            usb_count++;
            return 0;
        }

        usb_disk_obj = FF_SDDiskInit("usb", MOUNT_USB);
        if (usb_disk_obj == NULL) {
            FCS_LOG_ERR("USB mount init failed\n");
            return -1;
        }

        usb_count = 1;
    }

    return 0;
}

static void unmount_disk(int disk)
{
    if (disk == 0) {
        if (sd_count > 1) {
            sd_count--;
            return;
        }

        if (sd_disk_obj != NULL) {
            FF_Unmount(sd_disk_obj);
            FF_SDDiskDelete(sd_disk_obj);
            sd_disk_obj = NULL;
        }
        sd_count = 0;
    } else {
        if (usb_count > 1) {
            usb_count--;
            return;
        }

        if (usb_disk_obj != NULL) {
            FF_Unmount(usb_disk_obj);
            FF_SDDiskDelete(usb_disk_obj);
            usb_disk_obj = NULL;
        }
        usb_count = 0;
    }
}

static FCS_OSAL_FILE *fcs_freertos_fs_open(FCS_OSAL_CHAR *filename,
                         fcs_filesys_flags_t flag)
{
    FF_Error_t err;
    FF_FILE *file;
    int disk;

    if (!filename) {
        FCS_LOG_ERR("invalid argument\n");
        return NULL;
    }

    disk = get_disk_type(filename);
    if (disk < 0) {
        FCS_LOG_ERR("invalid mount point in filename\n");
        return NULL;
    }

    if (mount_disk(filename) != 0) {
        return NULL;
    }

    if (flag == FCS_FILE_READ) {
        if (disk == 0)
            file = FF_Open(sd_disk_obj->pxIOManager, filename + 3,
                       FF_MODE_READ, &err);
        else
            file = FF_Open(usb_disk_obj->pxIOManager, filename + 4,
                       FF_MODE_READ, &err);
    } else if (flag == FCS_FILE_WRITE) {
        if (disk == 0)
            file = FF_Open(sd_disk_obj->pxIOManager, filename + 3,
                       FF_MODE_CREATE | FF_MODE_WRITE | FF_MODE_TRUNCATE,
                       &err);
        else
            file = FF_Open(usb_disk_obj->pxIOManager, filename + 4,
                       FF_MODE_CREATE | FF_MODE_WRITE | FF_MODE_TRUNCATE,
                       &err);
    } else if (flag == FCS_FILE_APPEND) {
        if (disk == 0)
            file = FF_Open(sd_disk_obj->pxIOManager, filename + 3,
                       FF_MODE_APPEND | FF_MODE_CREATE | FF_MODE_WRITE,
                       &err);
        else
            file = FF_Open(usb_disk_obj->pxIOManager, filename + 4,
                       FF_MODE_APPEND | FF_MODE_CREATE | FF_MODE_WRITE,
                       &err);
    } else {
        FCS_LOG_ERR("invalid argument in flag\n");
        unmount_disk(disk);
        return NULL;
    }

    if (file == NULL) {
        FCS_LOG_ERR("error in opening file\n");
        unmount_disk(disk);
        return NULL;
    }

    return (FCS_OSAL_FILE *)file;
}

static FCS_OSAL_INT fcs_freertos_fs_read(FCS_OSAL_VOID *buf,
                       FCS_OSAL_SIZE len,
                       FCS_OSAL_FILE *file)
{
    FF_FILE *ffile = (FF_FILE *)file;

    if (!buf || len <= 0 || !ffile) {
        return -EINVAL;
    }

    return FF_Read(ffile, 1, len, buf);
}

static FCS_OSAL_INT fcs_freertos_fs_write(FCS_OSAL_VOID *buf,
                        FCS_OSAL_SIZE len,
                        FCS_OSAL_FILE *file)
{
    FF_FILE *ffile = (FF_FILE *)file;

    if (!buf || len <= 0 || !ffile) {
        return -EINVAL;
    }

    return FF_Write(ffile, 1, len, buf);
}

static FCS_OSAL_INT fcs_freertos_fs_fgets(FCS_OSAL_CHAR *str,
                        FCS_OSAL_SIZE len,
                        FCS_OSAL_FILE *file)
{
    FF_FILE *ffile = (FF_FILE *)file;

    if (!str || len == 0 || !ffile) {
        return -EINVAL;
    }

    int32_t ret;
    ret = FF_GetLine(ffile, str, (uint32_t)len);

    if (ret == 0 && str == NULL) {
        return -EFAULT;
    } else if (ret != 0) {
        return ret;
    } else {
        return 0;
    }
}

static FCS_OSAL_INT fcs_freertos_fs_fseek(FCS_OSAL_OFFSET offset,
                        fcs_filesys_whence_t whence,
                        FCS_OSAL_FILE *file)
{
    FF_FILE *ffile = (FF_FILE *)file;

    if (!ffile) {
        return -EINVAL;
    }

    FF_Error_t err;

    if (whence == FCS_SEEK_SET) {
        err = FF_Seek(ffile, offset, FF_SEEK_SET);
    } else if (whence == FCS_SEEK_CUR) {
        err = FF_Seek(ffile, offset, FF_SEEK_CUR);
    } else if (whence == FCS_SEEK_END) {
        err = FF_Seek(ffile, offset, FF_SEEK_END);
    } else {
        FCS_LOG_ERR("invalid whence %u\n", whence);
        return -EINVAL;
    }

    if (err != FF_ERR_NONE) {
        FCS_LOG_ERR("error in fseek\n");
        return -EFAULT;
    }
    return 0;
}

static FCS_OSAL_INT fcs_freertos_fs_close(FCS_OSAL_FILE *file)
{
    FF_FILE *ffile = (FF_FILE *)file;
    int disk = -1;

    if (!ffile) {
        return -EINVAL;
    }

    FF_IOManager_t *ioman = ffile->pxIOManager;

    if (sd_disk_obj != NULL && sd_disk_obj->pxIOManager == ioman)
        disk = 0;
    else if (usb_disk_obj != NULL && usb_disk_obj->pxIOManager == ioman)
        disk = 1;

    FF_Close(ffile);

    if (disk >= 0)
        unmount_disk(disk);

    return 0;
}

static FCS_OSAL_INT fcs_freertos_fs_get_size(FCS_OSAL_FILE *file,
                           FCS_OSAL_SIZE *size)
{
    FF_FILE *ffile = (FF_FILE *)file;

    if (!ffile || !size) {
        FCS_LOG_ERR("invalid file or size pointer\n");
        return -EINVAL;
    }

    *size = ff_filelength(ffile);
    if (*size == 0) {
        FCS_LOG_ERR("failed to get file size\n");
        return -EFAULT;
    }

    return 0;
}

FCS_OSAL_INT fcs_filesys_init(struct fcs_filesys_intf *fs_intf)
{
    if (!fs_intf) {
        return -EINVAL;
    }

    fs_intf->open = fcs_freertos_fs_open;
    fs_intf->read = fcs_freertos_fs_read;
    fs_intf->fgets = fcs_freertos_fs_fgets;
    fs_intf->write = fcs_freertos_fs_write;
    fs_intf->fseek = fcs_freertos_fs_fseek;
    fs_intf->close = fcs_freertos_fs_close;
    fs_intf->get_size = fcs_freertos_fs_get_size;
    return 0;
}
