/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application implementation for RSU
 */

#include <stdint.h>
#include "osal_log.h"
#include "socfpga_flash.h"
#include <libRSU.h>
#include <libRSU_OSAL.h>

/**
 * @defgroup rsu_sample RSU
 * @ingroup samples
 *
 * Sample Application for RSU
 *
 * @details
 * @section rsu_description Description
 * This is a simple program to demonstrate the use of librsu library with images sourced directly
 * from QSPI flash staging slots. The application image ('app2.rpd') and the SSBL image ('fip2.bin')
 * are pre-programmed into dedicated QSPI staging slots ('SSBL.P2' and 'SSBL.P3' respectively) as
 * part of the initial flash programming step using the provided 'initial_image.pfg'. No SD card or
 * file system access is required at runtime.
 * At runtime the sample reads the application image from the 'SSBL.P2' slot and the SSBL image from
 * the 'SSBL.P3' slot directly via the QSPI flash handle using 'flash_read_sync'. It then erases
 * slot @c RSU_SLOT and programs the application image using librsu, performs a ROS (Remote OS Update)
 * by erasing slot @c ROS_SLOT and programming the SSBL (fip) image in raw mode, verifies both, and
 * finally requests the updated slot to be loaded on the next warm reboot.
 * The user can refer to the <a href="https://altera-fpga.github.io/rel-24.3/
 * embedded-designs/agilex-5/e-series/premium/rsu/ug-rsu-agx5e-soc/#creating-the-initial-flash-image">Creating the Flash Image</a> to understand more about
 * creating the initial image needed for RSU (Remote System Update) and ROS (Remote OS update) in Linux-based systems. The pfg in the link, however,
 * requires some changes to be used for FreeRTOS. The bitstream's 'hps_path' should be updated with the path to 'bl2.hex' instead of 'u-boot-spl-dtb.hex'.
 * The user can also add additional slots to accommodate fip (SSBL) binaries or additional images (for reference, refer to the pfg in the rsu_qspi samples folder).
 *
 * @section rsu_prerequisites Prerequisites
 * - Make sure to use a RSU supported image having at least 2 slots with ATF version 2.12.0 or later.
 * - The QSPI flash must be programmed with the initial image generated from 'initial_image.pfg',
 *   with 'SSBL.P2' populated with 'app2.rpd' (as 'app2.bin') and 'SSBL.P3' with 'fip2.bin'.
 * - The source files 'app2.rpd' and 'fip2.bin' must be present in 'samples/rsu/rsu_qspi/bin/'
 *   before building. If either file is missing the build will fail.
 * - The sample pfg file used to create the initial RSU jic image is available at @c samples/rsu/rsu_qspi/initial_image.pfg.
 *
 * @section rsu_param Configurable Parameters
 * - The application image destination slot is defined with @c RSU_SLOT macro.
 * - The application image source slot (SSBL.P2) is defined with @c RSU_SRC_SLOT macro.
 * - The SSBL (fip) destination slot is defined with @c ROS_SLOT macro.
 * - The SSBL (fip) source slot (SSBL.P3) is defined with @c ROS_SRC_SLOT macro.
 *
 * @section rsu_how_to_run How to Run
 * 1. Follow the common README for build and flashing instructions.
 * 2. Run the application on the board with the appropriate RSU image.
 * 3. After the successful execution of the sample application the board goes for a warm reboot. Restart the board to load the new slot.
 *
 * @section rsu_expected_results Expected Results
 * - The board goes for a warm reboot. Then after a power cycle the board boots up in the new slot.
 * @{
 */
/** @} */


/* Application image slot to be updated */
#define RSU_SLOT    1
#define RSU_SRC_SLOT    4
#define RSU_SRC_PARTITION   "SSBL.P2"

/* SSBL/fip bianry slot to be updated */
#define ROS_SLOT    4
#define ROS_SRC_SLOT    3
#define ROS_SRC_PARTITION   "SSBL.P3"

/* QSPI flash handle for RSU operations */
extern flash_handle_t rsu_rtos_qspi_handle;

/*
 *  @brief Get slot count
 *
 *    Function to get the number of slots. This function fetches
 *    the number of slots that can be used to flash the application image
 *    and freeRTOS binaries(SSBL).
 */
static int rsu_client_get_slot_count(void)
{
    return rsu_slot_count();
}

/*
 * @brief Erase a slot
 *
 *    Function to erase a slot in the jic. The function erases the
 *    application image in the selected slot. The slot_num is zero based.
 */
static int rsu_client_erase_image(int slot_num)
{
    return rsu_slot_erase(slot_num);
}

/*
 *  @brief Print status of current slot.
 *
 *    Function to get status of current slot. This function fetches
 *    the details like version, current image of the currently active slot.
 */
static int rsu_client_copy_status_log(void)
{
    struct rsu_status_info info;
    int ret = -1;

    if (!rsu_status_log(&info))
    {
        ret = 0;
        PRINT("      VERSION: 0x%08X", (int)info.version);
        PRINT("        STATE: 0x%08X", (int)info.state);
        PRINT("CURRENT IMAGE: 0x%016lX", info.current_image);
        PRINT("   FAIL IMAGE: 0x%016lX", info.fail_image);
        PRINT("    ERROR LOC: 0x%08X", (int)info.error_location);
        PRINT("ERROR DETAILS: 0x%08X", (int)info.error_details);
        if (RSU_VERSION_DCMF_VERSION(info.version) &&
                RSU_VERSION_ACMF_VERSION(info.version))
        {
            PRINT("RETRY COUNTER: 0x%08X", (int)info.retry_counter);
        }
    }
    return ret;
}

/**
 * @brief Get slot offset
 *
 *    Function to get the offset of the specified slot.
 */
static uint64_t rsu_client_get_slot_offset(const char *slot_name)
{
    struct rsu_slot_info info;
    int slot_num = rsu_slot_by_name((char *)slot_name);
    if (slot_num < 0)
    {
        ERROR("Failed to get slot number for slot name: %s", slot_name);
        return 0;
    }
    if (rsu_slot_get_info(slot_num, &info) != 0)
    {
        ERROR("Failed to get slot info for slot number: %d", slot_num);
        return 0;
    }
    return info.offset;
}

/*
 * @brief Print slot attributes
 *
 *    Function to get slot info. Gets the details like partition name,
 *    partition offset, partition size and priority.
 */
static int rsu_client_list_slot_attribute(int slot_num)
{
    struct rsu_slot_info info;
    int ret = -1;

    if (!rsu_slot_get_info(slot_num, &info))
    {
        ret = 0;
        PRINT("      NAME: %s", info.name);
        PRINT("      SLOT: %d", slot_num);
        PRINT("    OFFSET: 0x%016lX", info.offset);
        PRINT("      SIZE: 0x%08X", info.size);

        if (info.priority)
        {
            PRINT("  PRIORITY: %i", info.priority);
        }
        else
        {
            PRINT("  PRIORITY: [disabled]");
        }
    }
    return ret;
}

/*
 * @brief Verify flashed image or raw binary
 *
 *    Function to compare flashed image or raw binary with actual file.
 *    The image and the binary to be verified shall be present in the RAM.
 */
static int rsu_client_verify_data_from_ddr(void *image_buf, int slot_num,
        int size, int raw)
{
    if (raw)
    {
        return rsu_slot_verify_buf_raw(slot_num, image_buf, size);
    }

    return rsu_slot_verify_buf(slot_num, image_buf, size);
}

/*
 * @brief Add app image or raw binary
 *
 *    Function to flash an application image or raw binary from DDR to a slot.
 *    The application image and fip binary to be flashed should be present in the
 *    sd card. For RSU operation file is not in raw format and for ROS it is in
 *    raw format.
 */
static int rsu_client_add_app_image_from_ddr(void *image_buf, int slot_num,
        int size, int raw)
{
    if (raw)
    {
        return rsu_slot_program_buf_raw(slot_num, image_buf, size);
    }

    return rsu_slot_program_buf(slot_num, image_buf, size);
}

/*
 * @brief Get the size of the source slot
 *
 *    Function to get the size of the specified source slot.
 */
static int rsu_client_get_source_slot_size(int slot_num)
{
    return rsu_slot_size(slot_num);
}

/**
 * @brief Allocate buffer for the source slot
 *
 *    Function to allocate memory for the source slot buffer.
 */
static void rsu_client_allocate_source_slot_buffer(int slot_num, void **buf,
        int *size)
{
    *size = rsu_client_get_source_slot_size(slot_num);
    *buf = malloc(*size);
    if (*buf == NULL)
    {
        ERROR("Failed to allocate memory for source slot buffer");
    }
}

/*
 * @brief Load a slot
 *    Function to load the requested slot. Once the slot is requested the
 *    board goes for a warm reboot. Power cycle the board to boot in the requested slot.
 */
static int rsu_client_request_slot_be_loaded(int slot_num)
{
    return rsu_slot_load_after_reboot(slot_num);
}

void rsu_task(void)
{
    int ret, slot_count;
    int idx = 0;
    void *app_buf = NULL;
    int app_size = 0;
    void *ssbl_buf = NULL;
    int ssbl_size = 0;
    uint64_t app_src_slot_offset = 0;
    uint64_t ssbl_src_slot_offset = 0;


    PRINT("Starting RSU-ROS sample application");

    ret = librsu_init("");
    if (ret)
    {
        ERROR("RSU initialization failed!!");
        return;
    }

    PRINT("Getting the number of slots in the image ...");
    slot_count = rsu_client_get_slot_count();
    if (slot_count < 0)
    {
        ERROR("No available slots");
        return;
    }
    PRINT("The number of slots available is :%d", slot_count);

    PRINT("Getting slot information ...");
    for (idx = 0; idx < slot_count; idx++)
    {
        ret = rsu_client_list_slot_attribute(idx);
        if (ret != 0)
        {
            ERROR("Failed to get attributes for slot: %d", idx);
            return;
        }
    }

    PRINT("Getting current slot status ...");
    ret = rsu_client_copy_status_log();
    if (ret != 0)
    {
        ERROR("Failed to get the status log");
        return;
    }

    rsu_client_allocate_source_slot_buffer(RSU_SRC_SLOT, &app_buf, &app_size);
    if (app_buf == NULL)
    {
        ERROR("Failed to allocate buffer for application image");
        return;
    }

    app_src_slot_offset = rsu_client_get_slot_offset(RSU_SRC_PARTITION);

    if (app_src_slot_offset == 0)
    {
        ERROR("Failed to get source slot offset for application image");
        return;
    }

    PRINT("Reading data from the offset 0x%lx...", app_src_slot_offset);
    ret = flash_read_sync(rsu_rtos_qspi_handle, app_src_slot_offset,
            app_buf, app_size);
    if (ret != 0)
    {
        ERROR("Read failed with error code %d", ret);
        return;
    }

    PRINT("Erasing the slot %d ....", RSU_SLOT);
    ret = rsu_client_erase_image(RSU_SLOT);
    if (ret != 0)
    {
        ERROR("Failed to erase the slot");
        return;
    }
    PRINT("Done.");

    PRINT("Programming image to slot %d ...", RSU_SLOT);
    ret = rsu_client_add_app_image_from_ddr(app_buf, RSU_SLOT, app_size, 0);
    if (ret != 0)
    {
        ERROR("Failed to program the image");
        return;
    }
    PRINT("Done.");

    PRINT("Verifying the image in slot %d...", RSU_SLOT);
    ret = rsu_client_verify_data_from_ddr(app_buf, RSU_SLOT, app_size, 0);
    if (ret != 0)
    {
        ERROR("Slot verification failed");
        return;
    }
    PRINT("Done.");

    /* Program fip2.bin */
    rsu_client_allocate_source_slot_buffer(ROS_SRC_SLOT, &ssbl_buf, &ssbl_size);
    if (ssbl_buf == NULL)
    {
        ERROR("Failed to allocate buffer for application image");
        return;
    }

    ssbl_src_slot_offset = rsu_client_get_slot_offset(ROS_SRC_PARTITION);
    if (ssbl_src_slot_offset == 0)
    {
        ERROR("Failed to get source slot offset for SSBL image");
        return;
    }

    PRINT("Reading data from the offset 0x%lx...", ssbl_src_slot_offset);
    ret = flash_read_sync(rsu_rtos_qspi_handle, ssbl_src_slot_offset,
            ssbl_buf, ssbl_size);
    if (ret != 0)
    {
        ERROR("Read failed with error code %d", ret);
        return;
    }

    PRINT("Erasing the slot %d ....", ROS_SLOT);
    ret = rsu_client_erase_image(ROS_SLOT);
    if (ret != 0)
    {
        ERROR("Failed to erase the slot");
        return;
    }
    PRINT("Done.");

    PRINT("Programming image to slot %d ...", ROS_SLOT);
    ret = rsu_client_add_app_image_from_ddr(ssbl_buf, ROS_SLOT, ssbl_size, 1);
    if (ret != 0)
    {
        ERROR("Failed to program the image");
        return;
    }
    PRINT("Done.");

    PRINT("Verifying the image in slot %d...", ROS_SLOT);
    ret = rsu_client_verify_data_from_ddr(ssbl_buf, ROS_SLOT, ssbl_size, 1);
    if (ret != 0)
    {
        ERROR("Slot verification failed");
        return;
    }
    PRINT("Done.");

    PRINT("Loading %dst slot ...", RSU_SLOT);
    ret = rsu_client_request_slot_be_loaded(RSU_SLOT);
    if (ret != 0)
    {
        ERROR("Failed to load the 1st slot");
        return;
    }

    free(app_buf);
    free(ssbl_buf);
    PRINT("Done.");
    PRINT("RSU-ROS sample application completed.");
}
