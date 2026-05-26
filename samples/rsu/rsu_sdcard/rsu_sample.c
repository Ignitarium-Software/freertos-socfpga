/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Sample application implementation for RSU
 */

#include <stdint.h>
#include "osal_log.h"

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
 * This is a simple program to demonstrate the use of librsu library. The sample application erases slot 1 and programs an application image from the SD card to that slot,
 * verifies it, and loads it. The sample also performs a ROS (Remote OS update) operation on the corresponding SSBL slot (i.e., it erases, programs, and verifies the SSBL slot
 * corresponding to slot 1, which is slot 3 for the pfg file that the sample uses). The user can refer to the <a href="https://altera-fpga.github.io/rel-24.3/
 * embedded-designs/agilex-5/e-series/premium/rsu/ug-rsu-agx5e-soc/#creating-the-initial-flash-image">Creating the Flash Image</a> to understand more about
 * creating the initial image needed for RSU (Remote System Update) and ROS (Remote OS update) in Linux-based systems. The pfg in the link, however,
 * requires some changes to be used for FreeRTOS. The bitstream's 'hps_path' should be updated with the path to 'bl2.hex' instead of 'u-boot-spl-dtb.hex'.
 * The user can also add additional slots to accommodate fip (SSBL) binaries or additional images (for reference, refer to the pfg in the rsu samples folder).
 * For the steps to create the application image, the user can refer to the link <a href="https://altera-fpga.github.io/rel-24.3/embedded-designs/agilex-5/
 * e-series/premium/rsu/ug-rsu-agx5e-soc/#creating-the-application-image">Creating the Application Image</a>. To create an application image that
 * supports FreeRTOS, the user must replace the argument 'hps_path' with the path to 'bl2.hex' instead of the path 'u-boot-spl-dtb.hex'. When the user creates
 * the QSPI image, the creation of the application image is automatically taken care of. The user can copy 'app2.rpd' from the samples/build/build/qspi-atf-binaries/
 * folder to the SD card. For ROS update, the user can copy a valid 'fip.bin' (with the same ATF version) to the SD card. For this sample, it is assumed that a
 * valid fip binary named 'fip2.bin' is used. Both the fip binary and application image must be present in the FAT partition of the SD card.
 *
 * @section rsu_prerequisites Prerequisites
 * - Make sure to use a RSU supported image having  at least 2 slots with ATF version 2.12.0 or later
 * - Ensure that a rpd file is stored on the SD card, and its name matches the variable(file name shall be 8 characters or below) 'rsu_file_name' <br>
 * - Ensure that a fip.bin file is stored on the SD card, and its name matches the variable(file name shall be 8 characters or below) 'ros_file_name' <br>
 * - The sample pfg file used to create the initial RSU jic image is available at @c drivers/samples/rsu/initial_image.pfg which creates 6 slots(0 to 5).
 *
 * @section rsu_param Configurable Parameters
 * - The application image slot to be updated is defined with @c RSU_SLOT macro.
 * - The fip binary (SSBL) slot to be updated is defined with @c ROS_SLOT macro(if P1 slot is updated update SSBL.P1).
 * - The name of the application image is stored in the variable @c rsu_file_name.
 * - The name of the fip binary(SSBL) is stored in the variable @c ros_file_name.
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

/* SSBL/fip bianry slot to be updated */
#define ROS_SLOT    3

/* Application image used by RSU sample. */
char *rsu_file_name = "/app2.rpd";

/* Raw binary used by ROS sample. */
char *ros_file_name = "/fip2.bin";

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
        if (RSU_VERSION_DCMF_VERSION(info.version) && RSU_VERSION_ACMF_VERSION(
                info.version))
        {
            PRINT("RETRY COUNTER: 0x%08X", (int)info.retry_counter);
        }
    }
    return ret;
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
 * @brief Add app image or raw binary
 *
 *    Function to flash an application image or raw binary to a slot. The
 *    application image and fip binary to be flashed should be present in the
 *    SD card. For RSU operation file is not in raw format and for ROS it is in
 *    raw format.
 */
static int rsu_client_add_app_image(char *image_name, int slot_num,
        int raw)
{
    if (raw)
    {
        return rsu_slot_program_file_raw(slot_num, image_name);
    }

    return rsu_slot_program_file(slot_num, image_name);
}

/*
 * @brief Verify flashed image or raw binary
 *
 *    Function to compare flashed image or raw binary with actual file. The image and
 *    the binary to be verified shall be present in the sd card.
 */
static int rsu_client_verify_data(char *file_name, int slot_num, int raw)
{
    if (raw)
    {
        return rsu_slot_verify_file_raw(slot_num, file_name);
    }

    return rsu_slot_verify_file(slot_num, file_name);
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
    int ret, slot_count, idx;

    PRINT("Starting RSU-ROS sample application");

    ret = librsu_init("");
    if (ret != 0)
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

    PRINT("Erasing the slot %d ....", RSU_SLOT);
    ret = rsu_client_erase_image(RSU_SLOT);
    if (ret != 0)
    {
        ERROR("Failed to erase the slot");
        return;
    }
    PRINT("Done.");

    PRINT("Programming image to slot %d ...", RSU_SLOT);
    ret = rsu_client_add_app_image(rsu_file_name, RSU_SLOT, 0);
    if (ret != 0)
    {
        ERROR("Failed to program the image");
        return;
    }

    PRINT("Done.");

    PRINT("Verifying the image in slot %d...", RSU_SLOT);
    ret = rsu_client_verify_data(rsu_file_name, RSU_SLOT, 0);
    if (ret != 0)
    {
        ERROR("Slot verification failed");
        return;
    }
    PRINT("Done.");

    PRINT("Erasing the slot %d ...", ROS_SLOT);
    ret = rsu_client_erase_image(ROS_SLOT);
    if (ret != 0)
    {
        ERROR("Failed to erase the slot");
        return;
    }
    PRINT("Done.");

    PRINT("Programming binary to slot %d ...", ROS_SLOT);
    ret = rsu_client_add_app_image(ros_file_name, ROS_SLOT, 1);
    if (ret != 0)
    {
        ERROR("Failed to program the image");
        return;
    }

    PRINT("Done.");

    PRINT("Verifying the binary in slot %d...", ROS_SLOT);
    ret = rsu_client_verify_data(ros_file_name, ROS_SLOT, 1);
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
    PRINT("Done.");
    PRINT("RSU-ROS sample application completed.");
}
