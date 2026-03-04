// SPDX-License-Identifier: MIT-0
/*
 * Copyright (C) 2025 Altera
 */


#include <stdlib.h>
#include <libfcs_osal.h>
#include <string.h>
#include "osal.h"
#include "socfpga_fcs.h"
#include "libfcs_utils.h"
#include <libfcs_logging.h>
#include <errno.h>

#define SDOS_OWNER_ID_OFFSET    12U
#define SDOS_OWNER_ID_SIZE      8U
#define HKDF_INPUT_SIZE         80U
#define HKDF_MAX_SIZE           4096U


static FCS_OSAL_INT fcs_freertos_api_binding(struct libfcs_osal_intf *intf);
FCS_OSAL_INT fcs_freertos_open_service_session(struct fcs_cmd_context *ctx);
FCS_OSAL_INT
fcs_freertos_close_service_session(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_random_number_ext(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_import_service_key(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_export_service_key(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_remove_service_key(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_get_service_key_info(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_create_service_key(struct fcs_cmd_context *ctx);
FCS_OSAL_INT
fcs_freertos_service_get_provision_data(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_counter_set(struct fcs_cmd_context *ctx);
FCS_OSAL_INT
fcs_freertos_counter_set_preauthorized(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_hkdf_request(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_aes_crypt(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_ecdh_req(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_get_digest(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_mac_verify(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_sdos_encrypt(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_sdos_decrypt(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_get_chip_id(struct fcs_cmd_context *ctx);
FCS_OSAL_INT
fcs_freertos_attestation_get_certificate(struct fcs_cmd_context *ctx);
FCS_OSAL_INT
fcs_freertos_attestation_cert_reload(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_mctp_cmd_send(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_get_jtag_idcode(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_get_device_identity(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_qspi_open(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_qspi_close(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_qspi_set_cs(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_qspi_read(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_qspi_write(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_qspi_erase(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_ecdsa_get_pub_key(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_ecdsa_hash_sign(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_ecdsa_hash_verify(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_ecdsa_sha2_data_sign(struct fcs_cmd_context *ctx);
FCS_OSAL_INT
fcs_freertos_ecdsa_sha2_data_verify(struct fcs_cmd_context *ctx);
FCS_OSAL_INT fcs_freertos_hps_img_validate(struct fcs_cmd_context *ctx);

/**
 * @brief Allocate memory of given size.
 *
 * @param size allocate memory size.
 * @return Pointer to the allocated memory, or NULL if size is 0 or allocation
 * fails.
 */
FCS_OSAL_VOID *fcs_malloc(FCS_OSAL_SIZE size)
{
    if (size == 0) {
        FCS_LOG_ERR("Attempted to allocate zero size memory\n");
        return NULL;
    }
    FCS_OSAL_VOID *ptr = pvPortMalloc(size);

    if (!ptr)
    {
        FCS_LOG_ERR("Memory allocation failed\n");
    }

    return ptr;
}

/**
 * @brief Free allocated memory.
 *
 * @param ptr Pointer to the memory to free.
 */
FCS_OSAL_VOID fcs_osal_free(FCS_OSAL_VOID *ptr)
{
    if (ptr != NULL)
    {
        vPortFree(ptr);
    }
    else
    {
        FCS_LOG_WRN("Attempted to free a NULL pointer\n");
    }
}

/**
 * @brief Initialize a mutex.
 *
 * @param mutex Pointer to the mutex.
 *
 * @return 0 on success, negative value on error.
 */
FCS_OSAL_INT fcs_mutex_init(FCS_OSAL_MUTEX *mutex)
{
    if (!mutex) {
        FCS_LOG_ERR("Invalid mutex pointer provided to fcs mutex init\n");
        return -1;
    }

    *mutex = xSemaphoreCreateMutex();

    if (*mutex == NULL)
    {
        FCS_LOG_ERR("Mutex initialization failed\n");
        return -1;
    }
    return 0;
}

/**
 * @brief Lock the mutex with a timeout.
 *
 * @param mutex Pointer to the mutex.
 * @param time Timeout value in milliseconds.
 *
 * @return 0 on success, negative value on error.
 */
FCS_OSAL_INT fcs_mutex_timedlock(FCS_OSAL_MUTEX *mutex, FCS_OSAL_U32 const time)
{
    FCS_OSAL_INT ret;

    /* Check if the mutex pointer is NULL */
    if (!mutex) {
        FCS_LOG_ERR("Invalid mutex pointer provided to fcs mutex timedlock\n");
        return -1; // Return invalid argument error
    }

    ret = osal_mutex_lock(*mutex, time);
    if (ret == pdTRUE)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

/**
 * @brief Release the mutex.
 *
 * @param mutex Pointer to the mutex.
 *
 * @return 0 on success, negative value on error.
 */
FCS_OSAL_INT fcs_mutex_unlock(FCS_OSAL_MUTEX *mutex)
{
    /* Check if the mutex pointer is NULL */
    if (!mutex) {
        FCS_LOG_ERR("Invalid mutex pointer provided to fcs mutex unlock\n");
        return -1; /* Return invalid argument error */
    }
    if (osal_mutex_unlock(*mutex) == false)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Destroy the mutex.
 *
 * @param mutex Pointer to the mutex.
 *
 * @return 0 on success, negative value on error.
 */
FCS_OSAL_INT fcs_mutex_destroy(FCS_OSAL_MUTEX *mutex)
{
    /* Check if the mutex pointer is NULL */
    if (!mutex) {
        FCS_LOG_ERR("Invalid mutex pointer provided to fcs mutex destroy\n");
        return -1; /* Return invalid argument error */
    }

    /* Destroy the mutex */
    (void) osal_mutex_delete(*mutex);

    return 0;
}

/**
 * @brief allocate memory for the buffer and copy the file content to the buffer
 *
 * @param filename file name to read
 * @param buffer to copy the file content
 *
 * @return FCS_OSAL_INT
 */

FCS_OSAL_INT fcs_alloc_and_cpy_file_to_mem(const FCS_OSAL_CHAR *filename,
        FCS_OSAL_CHAR **buffer)
{
    uint32_t file_size, bytes_read;
    file_size = fat_get_size(filename);
    *buffer = pvPortMalloc(file_size);

    bytes_read = fat_read(filename, *buffer);
    if (buffer == NULL)
    {
        FCS_LOG_ERR("Failed to allocate memory for file: %s", filename);
        return -1; /* Memory allocation failed */
    }
    if (bytes_read != file_size)
    {
        FCS_LOG_ERR("Failed to read file");
        vPortFree(*buffer);
        *buffer = NULL;
        return -1; /* Error reading file */
    }
    return bytes_read;
}

/**
 * @brief returns fdt error message for the given error code
 */
const FCS_OSAL_CHAR *fcs_fit_strerror(FCS_OSAL_INT err)
{
    (void)err;
    return NULL;
}

/**
 * @brief Verify the FIT image header.
 *
 * @param fit Pointer to the FIT buffer.
 * @param size FIT size.
 *
 * @return 0 always return success
 */
FCS_OSAL_INT fcs_fit_verify_header(FCS_OSAL_CHAR *fit)
{
    (void)fit;
    return 0;
}

/**
 * @brief Get the position of the node in the FIT image
 *
 * @param fit - pointer to the FIT format image header
 * @param path - path to the node
 *
 * @return -EIO always return failure
 */
FCS_OSAL_INT fcs_fit_get_noffset(FCS_OSAL_CHAR *fit, const FCS_OSAL_CHAR *path)
{
    (void)fit;
    (void)path;
    return -EIO;
}

/**
 * @brief FIT stub for freeRTOS
 *
 * @param fit - pointer to the FIT format image header
 * @param offset - offset to the node
 * @param depth - pointer to store the depth
 *
 * @return -EIO always return failure
 */
FCS_OSAL_INT fcs_fit_next_node(FCS_OSAL_CHAR *fit, FCS_OSAL_INT offset,
        FCS_OSAL_INT *depth)
{
    (void)fit;
    (void)offset;
    (void)depth;
    return -EIO;
}

/**
 * @brief FIT stub for freeRTOS
 *
 * @param fit - pointer to the FIT format image header
 * @param noffset - offset to the image node
 * @param offset - pointer to store the data position
 * @param data - pointer to store the data pointer
 * @param size - pointer to store the data size
 *
 * @return -EIO always return failure
 */
FCS_OSAL_INT fcs_fit_image_get_data_and_size(FCS_OSAL_CHAR *fit,
        FCS_OSAL_INT noffset,
        FCS_OSAL_CHAR **data,
        FCS_OSAL_SIZE *size)
{
    (void)fit;
    (void)noffset;
    (void)data;
    (void)size;
    return -EIO;
}

/**
 * @brief Open a service session
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_open_service_session(struct fcs_cmd_context *ctx)
{
    char uuid[FCS_OSAL_UUID_SIZE] = {0};
    FCS_OSAL_INT ret = run_fcs_open_service_session(uuid);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        if (ret == 0)
        {
            (void) memcpy(ctx->open_session.suuid, uuid, FCS_OSAL_UUID_SIZE);
            *ctx->open_session.suuid_len = FCS_OSAL_UUID_SIZE;
        }
        else
        {
            (void) memset(ctx->open_session.suuid, 0U, FCS_OSAL_UUID_SIZE);
            *ctx->open_session.suuid_len = 0U;
        }
        return 0;
    }
    return ret;
}

/**
 * @brief Close service session
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_close_service_session(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_close_service_session(ctx->close_session.suuid);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get provisioned data inforamtion
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT
fcs_freertos_service_get_provision_data(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_service_get_provision_data(
            ctx->prov_data.data, ctx->prov_data.data_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Generate a random number
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_random_number_ext(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_random_number_ext(ctx->rng.rng,
            ctx->rng.suuid,
            ctx->rng.context_id,
            ctx->rng.rng_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Import a service key
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_import_service_key(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_import_service_key(ctx->import_key.suuid,
            ctx->import_key.key,
            ctx->import_key.key_len, ctx->import_key.status,
            ctx->import_key.status_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Export a service key object
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_export_service_key(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_export_service_key(ctx->export_key.suuid,
            ctx->export_key.key_id,
            ctx->export_key.key, ctx->export_key.key_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Remove service key
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_remove_service_key(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_remove_service_key(ctx->remove_key.suuid,
            ctx->remove_key.key_id);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get service key information
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_get_service_key_info(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_get_service_key_info(ctx->key_info.suuid,
            ctx->key_info.key_id,
            ctx->key_info.info, ctx->key_info.info_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }

    return ret;
}

/**
 * @brief Create a service key
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_create_service_key(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_create_service_key(ctx->create_key.suuid,
            ctx->create_key.key,
            ctx->create_key.key_len, ctx->create_key.status,
            ctx->create_key.status_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Perform counter set
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_counter_set(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_send_certificate(ctx->ctr_set.ccert,
            ctx->ctr_set.ccert_len, (uint32_t*)ctx->ctr_set.status);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    /* status size is always 4 bytes for counter set*/
    *ctx->ctr_set.status_len = sizeof(uint32_t);
    return ret;
}

/**
 * @brief Perform counter set preauth
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT
fcs_freertos_counter_set_preauthorized(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_service_counter_set_preauthorized(
            ctx->ctr_set_preauth.ctr_type,
            ctx->ctr_set_preauth.ctr_val, ctx->ctr_set_preauth.test);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief HKDF request
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_hkdf_request(struct fcs_cmd_context *ctx)
{
    char *temp, *input_data;
    input_data = (char*)(uintptr_t) pvPortMalloc(HKDF_MAX_SIZE);
    if (input_data == NULL)
    {
        return -ENOMEM;
    }
    /*
     * formatting input for HKDF request
     * 4 bytes - input_data_size
     * 80 bytes - input_data padded to 80 bytes with 0s
     * 4 bytes - second_input_size
     * 80 bytes - second_input_data padded to 80 bytes with 0
     * Output key object data
     */
    temp = input_data;
    (void) memset(temp, 0, HKDF_MAX_SIZE);
    (void) memcpy(temp, &ctx->hkdf_req.ikm_len, sizeof(ctx->hkdf_req.ikm_len));
    temp += sizeof(ctx->hkdf_req.ikm_len);
    (void) memcpy(temp, ctx->hkdf_req.ikm, ctx->hkdf_req.ikm_len);
    temp += HKDF_INPUT_SIZE;
    (void) memcpy(temp, &ctx->hkdf_req.info_len, sizeof(ctx->hkdf_req.info_len));
    temp += sizeof(ctx->hkdf_req.info_len);
    (void) memcpy(temp, ctx->hkdf_req.info, ctx->hkdf_req.info_len);
    temp += HKDF_INPUT_SIZE;
    (void) memcpy(temp, ctx->hkdf_req.output_key_obj,
            ctx->hkdf_req.output_key_obj_len);

    FCS_OSAL_INT ret = run_fcs_hkdf_request(ctx->hkdf_req.suuid,
            ctx->hkdf_req.key_id,
            ctx->hkdf_req.step_type, ctx->hkdf_req.mac_mode, input_data,
            ctx->hkdf_req.output_key_obj_len,
            ctx->hkdf_req.hkdf_resp);
    vPortFree(input_data);

    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get chip ID
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_get_chip_id(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_get_chip_id(ctx->chip_id.chip_id_lo,
            ctx->chip_id.chip_id_hi);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get an attestation certificate
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT
fcs_freertos_attestation_get_certificate(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_attestation_get_certificate(
            ctx->attestation_cert.cert_request,
            ctx->attestation_cert.cert, ctx->attestation_cert.cert_size);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Attestation certificate reload
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT
fcs_freertos_attestation_cert_reload(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_attestation_certificate_reload(
            ctx->attestation_cert_reload.cert_request);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }

    return ret;
}

/**
 * @brief Perform MCTP operation
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_mctp_cmd_send(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_mctp_cmd_send(ctx->mctp.mctp_req,
            ctx->mctp.mctp_req_len,
            ctx->mctp.mctp_resp, ctx->mctp.mctp_resp_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get JTAG ID code
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_get_jtag_idcode(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_get_jtag_idcode(ctx->jtag_id.jtag_idcode);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get device identity
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_get_device_identity(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_get_device_identity(
            ctx->device_identity.identity,
            ctx->device_identity.identity_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Open an access to QSPI interface
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_qspi_open(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_qspi_open();
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Close an access to QSPI interface
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_qspi_close(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_qspi_close();
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}


/**
 * @brief Chip select
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_qspi_set_cs(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_qspi_set_cs(ctx->qspi_cs.chipsel);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Read data from QSPI flash
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_qspi_read(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_qspi_read(ctx->qspi_write.qspi_addr,
            ctx->qspi_write.len,
            ctx->qspi_write.buffer);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}
/**
 * @brief Write data to QSPI flash
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_qspi_write(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_qspi_write(ctx->qspi_write.qspi_addr,
            ctx->qspi_write.len,
            ctx->qspi_write.buffer);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Erase QSPI flash
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_qspi_erase(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_qspi_erase(ctx->qspi_erase.qspi_addr,
            ctx->qspi_erase.len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Perform AES encryption/decryption operation
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_aes_crypt(struct fcs_cmd_context *ctx)
{
    if (ctx->aes.mode == FCS_AES_GCM_GHASH)
    {
        *ctx->aes.op_len = 0;
    }
    else
    {
        *ctx->aes.op_len = ctx->aes.ip_len;
    }
    FCS_OSAL_INT ret = run_fcs_aes_cryption(ctx->aes.suuid, ctx->aes.kid,
            ctx->aes.cid,
            ctx->aes.crypt,
            ctx->aes.mode, ctx->aes.iv_source, ctx->aes.iv,
            ctx->aes.tag_len,
            ctx->aes.aad_len,
            ctx->aes.aad,
            ctx->aes.tag,
            ctx->aes.input, ctx->aes.ip_len, ctx->aes.output);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

FCS_OSAL_INT fcs_freertos_ecdh_req(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_ecdh_request(ctx->ecdh_req.suuid,
            ctx->ecdh_req.kid,
            ctx->ecdh_req.cid,
            ctx->ecdh_req.ecc_curve, ctx->ecdh_req.pubkey,
            ctx->ecdh_req.pubkey_len,
            ctx->ecdh_req.sh_secret, ctx->ecdh_req.sh_secret_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Generate digest
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_get_digest(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_get_digest(ctx->dgst.suuid,
            ctx->dgst.context_id,
            ctx->dgst.key_id,
            ctx->dgst.sha_op_mode, ctx->dgst.sha_digest_sz, ctx->dgst.src,
            ctx->dgst.src_len, ctx->dgst.digest, ctx->dgst.digest_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Verify HMAC
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_mac_verify(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_mac_verify(ctx->mac_verify.suuid,
            ctx->mac_verify.context_id,
            ctx->mac_verify.key_id, ctx->mac_verify.sha_digest_sz,
            ctx->mac_verify.src,
            ctx->mac_verify.src_size, ctx->mac_verify.dst,
            ctx->mac_verify.dst_size,
            ctx->mac_verify.user_data_size);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Validate an HPS image
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_hps_img_validate(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_send_certificate(
            ctx->hps_img_validate.vab_cert,
            ctx->hps_img_validate.vab_cert_len, ctx->hps_img_validate.resp);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Perform SDOS encryption operation
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_sdos_encrypt(struct fcs_cmd_context *ctx)
{
    (void) memcpy(&ctx->sdos.own, ctx->sdos.src + SDOS_OWNER_ID_OFFSET,
            SDOS_OWNER_ID_SIZE);
    FCS_OSAL_INT ret = run_fcs_sdos_encrypt(ctx->sdos.suuid,
            ctx->sdos.context_id,
            ctx->sdos.src, ctx->sdos.src_size, ctx->sdos.dst,
            ctx->sdos.dst_size);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Perform SDOS decryption operation
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_sdos_decrypt(struct fcs_cmd_context *ctx)
{
    (void) memcpy(&ctx->sdos.own, ctx->sdos.src + SDOS_OWNER_ID_OFFSET,
            SDOS_OWNER_ID_SIZE);
    FCS_OSAL_INT ret = run_fcs_sdos_decrypt(ctx->sdos.suuid,
            ctx->sdos.context_id,
            ctx->sdos.src, ctx->sdos.src_size, ctx->sdos.dst,
            ctx->sdos.dst_size,
            ctx->sdos.own);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Get ECDSA public key
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_ecdsa_get_pub_key(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_ecdsa_get_public_key(ctx->ecdsa_pub_key.suuid,
            ctx->ecdsa_pub_key.context_id,
            ctx->ecdsa_pub_key.key_id, ctx->ecdsa_pub_key.ecc_curve,
            ctx->ecdsa_pub_key.pubkey, ctx->ecdsa_pub_key.pubkey_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Generate ECDSA hash signature
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_ecdsa_hash_sign(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_ecdsa_hash_sign(ctx->ecdsa_hash_sign.suuid,
            ctx->ecdsa_hash_sign.context_id,
            ctx->ecdsa_hash_sign.key_id, ctx->ecdsa_hash_sign.ecc_curve,
            ctx->ecdsa_hash_sign.src, ctx->ecdsa_hash_sign.src_len,
            ctx->ecdsa_hash_sign.dst, ctx->ecdsa_hash_sign.dst_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Verify ECDSA hash signature
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_ecdsa_hash_verify(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_ecdsa_hash_verify(ctx->ecdsa_hash_verify.suuid,
            ctx->ecdsa_hash_verify.context_id,
            ctx->ecdsa_hash_verify.key_id,
            ctx->ecdsa_hash_verify.ecc_curve, ctx->ecdsa_hash_verify.src,
            ctx->ecdsa_hash_verify.src_len,
            ctx->ecdsa_hash_verify.signature,
            ctx->ecdsa_hash_verify.signature_len,
            ctx->ecdsa_hash_verify.pubkey,
            ctx->ecdsa_hash_verify.pubkey_len, ctx->ecdsa_hash_verify.dst,
            ctx->ecdsa_hash_verify.dst_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Generate ECDSA SHA2 data signature
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_ecdsa_sha2_data_sign(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_ecdsa_sha2_data_sign(
            ctx->ecdsa_sha2_data_sign.suuid,
            ctx->ecdsa_sha2_data_sign.context_id,
            ctx->ecdsa_sha2_data_sign.key_id,
            ctx->ecdsa_sha2_data_sign.ecc_curve,
            ctx->ecdsa_sha2_data_sign.src,
            ctx->ecdsa_sha2_data_sign.src_len,
            ctx->ecdsa_sha2_data_sign.dst,
            ctx->ecdsa_sha2_data_sign.dst_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Verify ECDSA SHA2 data signature
 *
 * @param ctx Context pointer to the context of the command
 *
 * @return 0 on success, otherwise value on error.
 */
FCS_OSAL_INT fcs_freertos_ecdsa_sha2_data_verify(struct fcs_cmd_context *ctx)
{
    FCS_OSAL_INT ret = run_fcs_ecdsa_sha2_data_sign_verify(
            ctx->ecdsa_sha2_data_verify.suuid,
            ctx->ecdsa_sha2_data_verify.context_id,
            ctx->ecdsa_sha2_data_verify.key_id,
            ctx->ecdsa_sha2_data_verify.ecc_curve,
            ctx->ecdsa_sha2_data_verify.src,
            ctx->ecdsa_sha2_data_verify.src_len,
            ctx->ecdsa_sha2_data_verify.signature,
            ctx->ecdsa_sha2_data_verify.signature_len,
            ctx->ecdsa_sha2_data_verify.pubkey,
            ctx->ecdsa_sha2_data_verify.pubkey_len,
            ctx->ecdsa_sha2_data_verify.dst,
            ctx->ecdsa_sha2_data_verify.dst_len);
    if (ret >= 0)
    {
        *ctx->error_code_addr = ret;
        return 0;
    }
    return ret;
}

/**
 * @brief Bind the OSAL API to the interface.
 *
 * @param intf Pointer to the OSAL interface.
 *
 * @return 0 on success, negative value on error.
 */
static FCS_OSAL_INT fcs_freertos_api_binding(struct libfcs_osal_intf *intf)
{
    if (intf == NULL) {
        FCS_LOG_ERR("Invalid argument: intf is NULL\n");
        return -1;
    }

    intf->open_service_session = fcs_freertos_open_service_session;
    intf->close_service_session = fcs_freertos_close_service_session;
    intf->random_number_ext = fcs_freertos_random_number_ext;
    intf->import_service_key = fcs_freertos_import_service_key;
    intf->export_service_key = fcs_freertos_export_service_key;
    intf->remove_service_key = fcs_freertos_remove_service_key;
    intf->get_service_key_info = fcs_freertos_get_service_key_info;
    intf->create_service_key = fcs_freertos_create_service_key;
    intf->get_provision_data = fcs_freertos_service_get_provision_data;
    intf->counter_set = fcs_freertos_counter_set;
    intf->counter_set_preauthorized = fcs_freertos_counter_set_preauthorized;
    intf->hkdf_request = fcs_freertos_hkdf_request;
    intf->get_digest = fcs_freertos_get_digest;
    intf->mac_verify = fcs_freertos_mac_verify;
    intf->aes_crypt = fcs_freertos_aes_crypt;
    intf->ecdh_req = fcs_freertos_ecdh_req;
    intf->get_chip_id = fcs_freertos_get_chip_id;
    intf->attestation_get_certificate =
            fcs_freertos_attestation_get_certificate;
    intf->attestation_cert_reload = fcs_freertos_attestation_cert_reload;
    intf->mctp_cmd_send = fcs_freertos_mctp_cmd_send;
    intf->get_jtag_idcode = fcs_freertos_get_jtag_idcode;
    intf->get_device_identity = fcs_freertos_get_device_identity;
    intf->qspi_open = fcs_freertos_qspi_open;
    intf->qspi_close = fcs_freertos_qspi_close;
    intf->qspi_cs = fcs_freertos_qspi_set_cs;
    intf->qspi_read = fcs_freertos_qspi_read;
    intf->qspi_write = fcs_freertos_qspi_write;
    intf->qspi_erase = fcs_freertos_qspi_erase;
    intf->sdos_encrypt = fcs_freertos_sdos_encrypt;
    intf->sdos_decrypt = fcs_freertos_sdos_decrypt;
    intf->ecdsa_get_pub_key = fcs_freertos_ecdsa_get_pub_key;
    intf->ecdsa_hash_sign = fcs_freertos_ecdsa_hash_sign;
    intf->ecdsa_hash_verify = fcs_freertos_ecdsa_hash_verify;
    intf->ecdsa_sha2_data_sign = fcs_freertos_ecdsa_sha2_data_sign;
    intf->ecdsa_sha2_data_verify = fcs_freertos_ecdsa_sha2_data_verify;
    intf->hps_img_validate = fcs_freertos_hps_img_validate;

    return 0;
}

/**
 * @brief Initialize the OSAL.
 *
 * @param intf Pointer to the OSAL interface.
 * @param loglevel set log level
 *
 * @return 0 on success, negative value on error.
 */

FCS_OSAL_INT libfcs_osal_init(struct libfcs_osal_intf *intf, char *log_level)
{
    (void)log_level;
    FCS_OSAL_INT ret;

    /* Bind the OSAL APIs */
    ret = fcs_freertos_api_binding(intf);

    if (ret != 0) {
        FCS_LOG_ERR("Error in binding OSAL APIs\n");
        return ret;
    }

    ret = fcs_init();

    if (ret != 0) {
        FCS_LOG_ERR("Error in initialsing driver\n");
        return ret;
    }
    return 0;
}
