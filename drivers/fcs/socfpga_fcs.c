/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL Implementation for FCS driver
 */
/*
 *
 * The FCS driver is implemented as a HAL for libfcs. This driver is not to be
 * used by the applicaion but rather is used by the libfcs library. It provide
 * user with a high-level API for cryptographic operations. The libfcs uses a
 * wrapper layer to interact with the FCS driver, which in turn communicates
 * with SIP_SVC subsystem and provides the response to the corresponding
 * cryptographic operation.
 *
 * The following diagram shows the general flow of a cryptographic
 * application.
 *
 *            +---------------------+
 *            |    Application      |
 *            +---------------------+
 *                 |          ^
 *                 v          |
 *            +---------------------+
 *            | libFCS (OS-indep +  |
 *            | dependent layer)    |
 *            +---------------------+
 *            |   Wrapper layer     |
 *            +---------------------+
 *     args from  |             ^   process
 *      user      v             |   response
 *            +---------------------+
 *            |     FCS driver      |
 *            +---------------------+
 *    sip_svc      |             ^
 *   func_id and   |             |   callback
 *     args        v             |   function
 *            +---------------------+
 *            |  SIP SVC Subsystem  |
 *            +---------------------+
 *    send      |   get     |      ^   response
 *    command   v  response v      |   ready
 *            +---------------------+
 *            |        EL3          |
 *            +---------------------+
 *            |        ATF          |
 *            +---------------------+
 *     mailbox   |               ^     poll
 * *     command   v               |   response
 *            +---------------------+
 *            |     Hardware        |
 *            +---------------------+
 *            |    SDM Mailbox      |
 *            +---------------------+
 *
 * The user application invokes cryptographic functions through the libfcs
 * library. This library has a freeRTOS wrapper that calls the freeRTOS
 * fcs driver functions. The driver performs the requested cryptographic
 * function through the SIP_SVC subsystem. This subsystem communicates
 * with the mailbox through the ATF using SMC calls.
 */

#include <string.h>
#include <errno.h>
#include "socfpga_cache.h"
#include "socfpga_mbox_client.h"
#include "socfpga_fcs.h"
#include "socfpga_fcs_ll.h"
#include "osal.h"
#include "osal_log.h"

#define FCS_STATUS_MASK           0x7FFU
#define FCS_BUFFER_SIZE           0x100000U
#define FCS_CRYPTO_BLOCK_SIZE     0x400000U
#define FCS_NON_GCM_BLOCK_SIZE    32U
typedef struct
{
    char uuid[FCS_UUID_SIZE];
    uint32_t session_id;
    sdm_client_handle_t crypto_handle;
} fcs_session_handle_t;

struct fcs_service_descriptor
{
    osal_semaphore_t fcs_sem;
    fcs_session_handle_t session_map[FCS_MAX_INSTANCES];
    sdm_client_handle_t security_handle;
    int session_count;
};

static struct fcs_service_descriptor *fcs_descriptor = NULL;
/** @cond DOXYGEN_IGNORE */
/* Statically allocating 4MB of data, 64 byte alignment for cache operations */
static uint32_t fcs_inp_ops_buffer[FCS_BUFFER_SIZE] __attribute__((aligned(64)));
static uint32_t fcs_out_ops_buffer[FCS_BUFFER_SIZE] __attribute__((aligned(64)));
/** @endcond */

void fcs_callback(uint64_t *resp_values);

int fcs_init(void)
{
    int ret;
    if (fcs_descriptor == NULL)
    {
        fcs_descriptor = pvPortMalloc(sizeof(struct fcs_service_descriptor));
        if (fcs_descriptor == NULL)
        {
            ERROR("Failed to initialise FCS");
            return -ENOMEM;
        }
        fcs_descriptor->session_count = 0;
        (void)memset(fcs_descriptor->session_map, 0,
                sizeof(fcs_descriptor->session_map));
        ret = mbox_init();
        if (ret != 0)
        {
            ERROR("Aborting FCS initialization");
            (void)memset(fcs_descriptor, 0, sizeof(struct
                    fcs_service_descriptor));
            vPortFree(fcs_descriptor);
            fcs_descriptor = NULL;
            return -EIO;
        }

        /* Opening a client to perform non session related operations */
        fcs_descriptor->security_handle = mbox_open_client();
        if (fcs_descriptor->security_handle == NULL)
        {
            ERROR("Failed to open mailbox client");
            (void)memset(fcs_descriptor, 0, sizeof(struct
                    fcs_service_descriptor));
            vPortFree(fcs_descriptor);
            fcs_descriptor = NULL;
            return -EIO;
        }
        ret = mbox_set_callback(fcs_descriptor->security_handle, fcs_callback);
        fcs_descriptor->fcs_sem = osal_semaphore_create(NULL);
        if ((fcs_descriptor->fcs_sem == NULL) || (ret != 0))
        {
            ERROR("Failed to initialise semaphore");
            (void)memset(fcs_descriptor, 0, sizeof(struct
                    fcs_service_descriptor));
            vPortFree(fcs_descriptor);
            fcs_descriptor = NULL;
            return -EIO;
        }
    }
    else
    {
        INFO("FCS already initialised");
    }
    return 0;
}
int fcs_deinit(void)
{
    int ret;
    if (fcs_descriptor == NULL)
    {
        ERROR("FCS not initialised");
        return -EIO;
    }
    else
    {
        if (fcs_descriptor->session_count != 0)
        {
            WARN("%d pending sessions to be closed",
                    fcs_descriptor->session_count);
        }
        ret = mbox_close_client(fcs_descriptor->security_handle);
        if (ret != 0)
        {
            ERROR("Failed to close malibox client");
            return -EIO;
        }
        ret = mbox_deinit();
        if (ret != 0)
        {
            WARN("Failed to free mailbox resources");
        }

        (void)memset(fcs_descriptor, 0, sizeof(struct fcs_service_descriptor));
        vPortFree(fcs_descriptor);
        fcs_descriptor = NULL;
    }
    return 0;
}

static sdm_client_handle_t get_client_handle(char *uuid, uint32_t *session_id)
{
    int i;
    sdm_client_handle_t fcs_handle = NULL;

    if (fcs_descriptor->session_count == 0)
    {
        ERROR("No opened sessions");
    }
    else
    {
        for (i = 0; i < (int)FCS_MAX_INSTANCES; i++)
        {
            if (memcmp(fcs_descriptor->session_map[i].uuid, uuid,
                    FCS_UUID_SIZE) == 0)
            {
                fcs_handle = fcs_descriptor->session_map[i].crypto_handle;
                *session_id = fcs_descriptor->session_map[i].session_id;
            }
        }
    }
    return fcs_handle;
}

/* @brief Use the random number generator to generate a UUID for the session ID */
static void generate_uuid(sdm_client_handle_t fcs_handle, uint32_t session_id,
        char *uuid)
{
    uint64_t rng_args[4], rng_resp[2] =
    {
        0
    };
    int ret;
    rng_args[0] = session_id;
    rng_args[1] = 1;
    rng_args[2] = (uint64_t)fcs_out_ops_buffer;
    rng_args[3] = FCS_UUID_SIZE;
    cache_force_invalidate(fcs_out_ops_buffer, FCS_UUID_SIZE +
            FCS_RESP_HEADER_SIZE);

    ret = sip_svc_send(fcs_handle, FCS_RANDOM_NUMBER, rng_args,
            sizeof(rng_args), rng_resp, sizeof(rng_resp));
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            if (rng_resp[FCS_RESP_STATUS] == 0UL)
            {
                cache_force_invalidate(fcs_out_ops_buffer, FCS_UUID_SIZE +
                        FCS_RESP_HEADER_SIZE);
                (void)memcpy((void *)uuid,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA],
                        FCS_UUID_SIZE);
            }
        }
    }
}
int run_fcs_open_service_session(char *uuid)
{
    int ret, i;
    uint16_t status;
    uint64_t open_session_resp[2] =
    {
        0
    };
    uint32_t session_id;
    sdm_client_handle_t fcs_handle;

    if (fcs_descriptor->session_count >= (int)FCS_MAX_INSTANCES)
    {
        ERROR("Session Limit Reaced");
        return -EIO;
    }
    if (uuid == NULL)
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }
    /* Session ID is tied with a client, find a client with no session*/
    for (i = 0; i < (int)FCS_MAX_INSTANCES; i++)
    {
        if (fcs_descriptor->session_map[i].session_id == 0U)
        {
            break;
        }
    }
    fcs_descriptor->session_map[i].crypto_handle = mbox_open_client();
    if (fcs_descriptor->session_map[i].crypto_handle == NULL)
    {
        ERROR("Failed to open Mailbox Client");
        return -EIO;
    }
    fcs_handle = fcs_descriptor->session_map[fcs_descriptor->session_count].
            crypto_handle;
    if (fcs_handle == NULL)
    {
        return -EINVAL;
    }
    ret = mbox_set_callback(fcs_handle, fcs_callback);
    if (ret != 0)
    {
        ERROR("Failed to allocate memory");
        return -ENOMEM;
    }

    DEBUG("Open_session: No args");
    ret = sip_svc_send(fcs_handle, FCS_OPEN_SESSION, NULL, 0, open_session_resp,
            sizeof(open_session_resp));
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", open_session_resp[0],
                    open_session_resp[1]);
            if (open_session_resp[FCS_RESP_STATUS] == 0UL)
            {
                session_id = (uint32_t)open_session_resp[FCS_RESP_SIZE];
                fcs_descriptor->session_map[i].session_id = session_id;
                INFO("Generating UUID");
                generate_uuid(fcs_handle, session_id, uuid);
                if (uuid == NULL)
                {
                    ERROR("Failed to generate UUID");
                    return -EIO;
                }
                /* UUID v4 formatting */
                uuid[6] = (char)(((uint8_t)uuid[6] & 0x0FU) | 0x40U);
                uuid[8] = (char)(((uint8_t)uuid[8] & 0x3FU) | 0x80U);
                (void)memcpy(fcs_descriptor->session_map[i].uuid, uuid,
                        FCS_UUID_SIZE);
                fcs_descriptor->session_count++;
            }
            status = (uint16_t)(open_session_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }
    else
    {
        (void)mbox_close_client(fcs_handle);
    }
    return ret;
}
int run_fcs_close_service_session(char *uuid)
{
    int ret, i;
    uint16_t status;
    uint64_t close_session_arg, resp_err = 0UL;
    uint32_t session_id = 0U;
    sdm_client_handle_t fcs_handle;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    close_session_arg = session_id;
    DEBUG("Close_session: session_id: %lu", close_session_arg);
    ret = sip_svc_send(fcs_handle, FCS_CLOSE_SESSION, &close_session_arg,
            sizeof(close_session_arg), &resp_err, sizeof(resp_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1: %lx", resp_err);
            ret = mbox_close_client(fcs_handle);
            if (resp_err == 0UL)
            {
                if (ret == 0)
                {
                    INFO("Client closed");
                }
                else
                {
                    ERROR("Failed to close client");
                    return ret;
                }
                for (i = 0; i < (int)FCS_MAX_INSTANCES; i++)
                {
                    if (fcs_descriptor->session_map[i].session_id == session_id)
                    {
                        /* Indicate that the client has no associated sesssion */
                        (void)memset(&fcs_descriptor->session_map[i], 0x0,
                                sizeof(fcs_descriptor->session_map[i]));
                        fcs_descriptor->session_count--;
                    }
                }
            }
            status = (uint16_t)resp_err;
            return (int)status;
        }
    }
    return ret;
}

int run_fcs_random_number_ext(char *rand_buf, char *uuid,
        uint32_t context_id, uint32_t rand_size)
{
    sdm_client_handle_t fcs_handle;
    uint64_t rng_args[4], rng_resp[2] =
    {
        0
    };
    int ret;
    uint16_t status;
    uint32_t session_id = 0U, extra_data = 0U;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Invalid session id");
        return -ENOMEM;
    }
    /* Pad to 4 bytes */
    if ((rand_size % 4U) != 0U)
    {
        extra_data = 4U - (rand_size % 4U);
    }
    if ((rand_size == 0U) || ((rand_size + extra_data) > FCS_RNG_MAX_SIZE))
    {
        ERROR("Invalid random number size");
        return -EINVAL;
    }
    rng_args[0] = session_id;
    rng_args[1] = context_id;
    rng_args[2] = (uint64_t)fcs_out_ops_buffer;
    rng_args[3] = (uint64_t)rand_size + (uint64_t)extra_data;
    cache_force_invalidate(fcs_out_ops_buffer, rand_size +
            FCS_RESP_HEADER_SIZE);

    DEBUG("Random_number_ext: rand_size: %lu", rand_size + extra_data);
    ret = sip_svc_send(fcs_handle, FCS_RANDOM_NUMBER, rng_args,
            sizeof(rng_args), rng_resp, sizeof(rng_resp));
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", rng_resp[0],
                    rng_resp[1]);
            if (rng_resp[FCS_RESP_STATUS] == 0UL)
            {
                cache_force_invalidate(fcs_out_ops_buffer, rand_size +
                        FCS_RESP_HEADER_SIZE);
                status = (uint16_t)rng_resp[FCS_RESP_STATUS];
                ret = (int)status;
                /* We dont copy the extra data */
                (void)memcpy((void *)rand_buf,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA], rand_size);
            }
        }
    }
    return ret;
}

int run_fcs_import_service_key(char *uuid, char *key,
        uint32_t key_size, char *status, unsigned int *status_size)
{
    sdm_client_handle_t fcs_handle;
    uint64_t import_key_args[2], import_key_resp[2] =
    {
        0
    };
    int ret;
    uint16_t resp_stat;
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        return -EIO;
    }
    if ((key == NULL) || (key_size == 0U))
    {
        return -EINVAL;
    }
    if (key_size > FCS_MAX_KEY_SIZE)
    {
        return -EINVAL;
    }
    (void)memset(fcs_inp_ops_buffer, 0, ((size_t)key_size +
            FCS_KEY_HEADER_SIZE));

    fcs_inp_ops_buffer[0] = session_id;
    /* The mailbox requires 8 bytes reserved after session id */
    (void)memcpy((void *)&fcs_inp_ops_buffer[3], (void *)key, key_size);

    import_key_args[0] = (uint64_t)fcs_inp_ops_buffer;
    import_key_args[1] = (uint64_t)key_size + FCS_KEY_HEADER_SIZE;
    cache_force_write_back((void *)fcs_inp_ops_buffer, ((size_t)key_size +
            FCS_KEY_HEADER_SIZE));

    DEBUG("Import_service_key: key_buffer: %lx, key_size: %lu", (uint64_t)key,
            key_size);
    ret = sip_svc_send(fcs_handle, FCS_IMPORT_SERVICE_KEY, import_key_args,
            sizeof(import_key_args), import_key_resp, sizeof(import_key_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", import_key_resp[0],
                    import_key_resp[1]);
            *status_size = MBOX_WORD_SIZE;
            (void)memcpy((void *)status,
                    (void *)&import_key_resp[FCS_RESP_KEY_STATUS],
                    *status_size);
            resp_stat = (uint16_t)import_key_resp[FCS_RESP_STATUS];
            ret = (int)resp_stat;
        }
    }
    return ret;
}

int run_fcs_export_service_key(char *uuid, uint32_t key_id,
        char *key_dest, unsigned int *key_size)
{
    sdm_client_handle_t fcs_handle;
    uint32_t session_id = 0U;
    uint64_t export_key_args[4], export_key_resp[2] =
    {
        0
    };
    int ret;
    uint16_t status;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((key_dest == NULL) || (key_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    export_key_args[0] = session_id;
    export_key_args[1] = key_id;
    export_key_args[2] = (uint64_t)fcs_out_ops_buffer;
    export_key_args[3] = *key_size;
    cache_force_invalidate(fcs_out_ops_buffer, FCS_EXPORT_KEY_MAX_SIZE);

    DEBUG("Export_service_key: key_id: %lu", key_id);
    ret = sip_svc_send(fcs_handle, FCS_EXPORT_SERVICE_KEY, export_key_args,
            sizeof(export_key_args), export_key_resp, sizeof(export_key_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", export_key_resp[0],
                    export_key_resp[1]);
            if (export_key_resp[FCS_RESP_STATUS] == 0UL)
            {
                cache_force_invalidate((void *)fcs_out_ops_buffer,
                        (size_t)export_key_resp[FCS_RESP_SIZE]);
                /* Ignoring the key status word of size 4 at the
                 * beginning of the response */
                *key_size = (uint32_t)export_key_resp[FCS_RESP_SIZE] -
                        MBOX_WORD_SIZE;
                (void)memcpy((void *)key_dest, (void *)&fcs_out_ops_buffer[1],
                        *key_size);
            }
            status = (uint16_t)export_key_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_remove_service_key(char *uuid, uint32_t key_id)
{
    sdm_client_handle_t fcs_handle;
    uint64_t remove_key_args[2], remove_key_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    int ret;
    uint16_t status;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    remove_key_args[0] = session_id;
    remove_key_args[1] = key_id;

    DEBUG("Remove_service_key: key_id: %lu", key_id);
    ret = sip_svc_send(fcs_handle, FCS_REMOVE_SERVICE_KEY, remove_key_args,
            sizeof(remove_key_args), remove_key_resp, sizeof(remove_key_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", remove_key_resp[0],
                    remove_key_resp[1]);
            status = (uint16_t)remove_key_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }

    return ret;
}
int run_fcs_get_service_key_info(char *uuid, uint32_t key_id,
        char *key_info, unsigned int *key_info_size)
{
    sdm_client_handle_t fcs_handle;
    uint64_t get_key_info_args[4], get_key_info_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    int ret;
    uint16_t status;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((key_info == NULL) || (key_info_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }

    get_key_info_args[0] = session_id;
    get_key_info_args[1] = key_id;
    get_key_info_args[2] = (uint64_t)key_info;
    get_key_info_args[3] = *key_info_size;
    cache_force_write_back(key_info, FCS_KEY_INFO_MAX_RESP);

    DEBUG("Get_service_key_info: key_id: %lu", key_id);
    ret = sip_svc_send(fcs_handle, FCS_GET_SERVICE_KEY_INFO, get_key_info_args,
            sizeof(get_key_info_args), get_key_info_resp,
            sizeof(get_key_info_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", get_key_info_resp[0],
                    get_key_info_resp[1]);
            if (get_key_info_resp[FCS_RESP_STATUS] == 0UL)
            {
                *key_info_size = (uint32_t)get_key_info_resp[FCS_RESP_SIZE];
                cache_force_invalidate(key_info, *key_info_size);
            }
            status = (uint16_t)(get_key_info_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }
    return ret;
}
int run_fcs_create_service_key(char *uuid, char *key,
        uint32_t key_size, char *status, unsigned int *status_size)
{
    sdm_client_handle_t fcs_handle;
    uint64_t create_key_args[2], create_key_resp[2] =
    {
        0
    };
    int ret;
    uint16_t resp_stat;
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((key == NULL) || (key_size == 0U))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    if (key_size > FCS_MAX_KEY_SIZE)
    {
        ERROR("Key size exceeds maximum limit");
        return -EINVAL;
    }
    (void)memset(fcs_inp_ops_buffer, 0, ((size_t)key_size +
            FCS_KEY_HEADER_SIZE));
    fcs_inp_ops_buffer[0] = session_id;
    /* The mailbox requires 8 bytes reserved after session id */
    (void)memcpy((void *)&fcs_inp_ops_buffer[3], (void *)key, key_size);

    create_key_args[0] = (uint64_t)fcs_inp_ops_buffer;
    create_key_args[1] = ((uint64_t)key_size + FCS_KEY_HEADER_SIZE);
    cache_force_write_back((void *)fcs_inp_ops_buffer, ((size_t)key_size +
            FCS_KEY_HEADER_SIZE));

    DEBUG("Create_service_key: key_buffer: %lx, key_size: %lu", (uint64_t)key,
            key_size);
    ret = sip_svc_send(fcs_handle, FCS_CREATE_SERVICE_KEY, create_key_args,
            sizeof(create_key_args), create_key_resp, sizeof(create_key_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", create_key_resp[0],
                    create_key_resp[1]);
            *status_size = MBOX_WORD_SIZE;
            (void)memcpy((void *)status,
                    (void *)&create_key_resp[FCS_RESP_KEY_STATUS],
                    *status_size);
            resp_stat = (uint16_t)(create_key_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)resp_stat;
        }
    }
    return ret;
}

int run_fcs_service_get_provision_data(char *prov_data,
        uint32_t *prov_data_size)
{
    int ret;
    uint16_t status;
    uint64_t prov_data_arg, prov_data_resp[2] =
    {
        0
    };
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    if ((prov_data == NULL) || (prov_data_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    cache_force_invalidate(prov_data, FCS_PROV_DATA_MAX_SIZE);

    prov_data_arg = (uint64_t)prov_data;
    DEBUG("Get_provision_data: prov_data_buffer: %x", (uint32_t)prov_data);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_GET_PROVISION_DATA,
            &prov_data_arg, sizeof(prov_data_arg), prov_data_resp,
            sizeof(prov_data_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", prov_data_resp[0],
                    prov_data_resp[1]);
            if (prov_data_resp[FCS_RESP_STATUS] == 0UL)
            {
                *prov_data_size = (uint32_t)prov_data_resp[FCS_RESP_SIZE];
                cache_force_invalidate(prov_data, *prov_data_size);
            }
            status = (uint16_t)prov_data_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_send_certificate(char *cert_data, uint32_t cert_size,
        uint32_t *status)
{
    uint64_t send_cert_args[3], send_cert_resp[2] =
    {
        0
    };
    int ret;
    uint16_t resp_stat;

    /* First 4 bytes are for test word(reserved in our case as its provided
     * in the certificate) */
    (void)memset(fcs_inp_ops_buffer, 0, cert_size + sizeof(uint32_t));
    (void)memcpy((void *)&fcs_inp_ops_buffer[1], (void *)cert_data, cert_size);
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }

    send_cert_args[0] = (uint64_t)fcs_inp_ops_buffer;
    send_cert_args[1] = (uint64_t)cert_size + sizeof(uint32_t);
    cache_force_write_back(fcs_inp_ops_buffer, cert_size + sizeof(uint32_t));

    DEBUG("Send_certificate: cert_buffer: %lx, cert_size: %lu",
            (uint64_t)cert_data, cert_size);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_SEND_CERTIFICATE,
            send_cert_args, sizeof(send_cert_args), send_cert_resp,
            sizeof(send_cert_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", send_cert_resp[0],
                    send_cert_resp[1]);
            if (send_cert_resp[FCS_RESP_STATUS] == 0UL)
            {
                (void)memcpy((void *)status,
                        (void *)&send_cert_resp[FCS_RESP_CERT_STATUS],
                        sizeof(uint32_t));
            }
            resp_stat = (uint16_t)(send_cert_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)resp_stat;
        }
    }
    return ret;
}
int run_fcs_service_counter_set_preauthorized(uint8_t type, uint32_t value,
        uint32_t test)
{
    uint64_t cntr_set_preauth_args[3], cntr_set_preauth_err =
    {
        0
    };
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }

    cntr_set_preauth_args[0] = type;
    cntr_set_preauth_args[1] = value;
    cntr_set_preauth_args[2] = test;

    DEBUG("Counter_set_preauthorized: type: %lu, value: %lu, test: %lu", type,
            value, test);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_CNTR_SET_PREAUTH,
            cntr_set_preauth_args, sizeof(cntr_set_preauth_args),
            &cntr_set_preauth_err, sizeof(cntr_set_preauth_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx", cntr_set_preauth_err);
            status = (uint16_t)(cntr_set_preauth_err & FCS_STATUS_MASK);
            return (int)status;
        }
    }
    return ret;
}

static int run_fcs_get_digest_init(char *uuid,
        uint32_t context_id, uint32_t key_id,
        uint32_t op_mode, uint32_t dig_size)
{
    uint64_t fcs_digest_init_args[5];
    uint32_t session_id = 0U, param_data;
    sdm_client_handle_t fcs_handle;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((op_mode != FCS_DIGEST_OPMODE_SHA2) && (op_mode !=
            FCS_DIGEST_OPMODE_HMAC))
    {
        ERROR("Invalid Operation Mode for DIGEST_REQ");
        return -EINVAL;
    }
    if ((dig_size != FCS_DIGEST_SIZE_256) && (dig_size !=
            FCS_DIGEST_SIZE_384) && (dig_size != FCS_DIGEST_SIZE_512))
    {
        ERROR("Invalid Digest Size for DIGEST_REQ");
        return -EINVAL;
    }
    if (op_mode == FCS_DIGEST_OPMODE_SHA2)
    {
        key_id = 0;
    }
    /* First 4 bytes if the parameter represents the operation mode
     * and the next 4 bytes represent the digest size */
    param_data = op_mode;
    param_data |= FCS_SET_DIGEST_SIZE(dig_size);

    fcs_digest_init_args[0] = session_id;
    fcs_digest_init_args[1] = context_id;
    fcs_digest_init_args[2] = key_id;
    fcs_digest_init_args[3] = FCS_DIGEST_PARAM_SIZE;
    fcs_digest_init_args[4] = param_data;

    DEBUG("Get_digest_init: key_id: %lu, "
            "op_mode: %lu, dig_size: %lu", key_id, op_mode, dig_size);
    return sip_svc_send(fcs_handle, FCS_GET_DIGEST_INIT, fcs_digest_init_args,
            sizeof(fcs_digest_init_args), NULL, 0);
}

static int run_fcs_get_digest_update(char *uuid, uint32_t context_id,
        char *src_data, uint32_t src_size,
        char *digest_data, uint32_t *digest_size,
        uint8_t final)
{
    int ret;
    uint16_t status;
    uint64_t fcs_digest_update_args[7], fcs_digest_update_smc_resp[2] =
    {
        0
    };
    sdm_client_handle_t fcs_handle;
    uint32_t session_id = 0U;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    fcs_digest_update_args[0] = session_id;
    fcs_digest_update_args[1] = context_id;
    /*
     * We shall provide the smmu address instead of the
     * regular source address for the SMC call
     */
    fcs_digest_update_args[2] = (uint64_t)src_data;
    fcs_digest_update_args[3] = src_size;
    fcs_digest_update_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_digest_update_args[5] = FCS_DIGEST_MAX_RESP;
    fcs_digest_update_args[6] = (uint64_t)FCS_SMMU_GET_ADDR(src_data);
    cache_force_invalidate(fcs_out_ops_buffer, FCS_DIGEST_MAX_RESP);
    cache_force_write_back(src_data, src_size);

    if (final == FCS_FINALIZE)
    {
        DEBUG("Get_digest_finalize: src_addrr: %x, src_size: %lu",
                (uint64_t)src_data, src_size);
        ret = sip_svc_send(fcs_handle, FCS_GET_DIGEST_FINALIZE,
                fcs_digest_update_args, sizeof(fcs_digest_update_args),
                fcs_digest_update_smc_resp, sizeof(fcs_digest_update_smc_resp));
    }
    else
    {
        DEBUG("Get_digest_update: src_addrr: %x, src_size: %lu",
                (uint64_t)src_data, src_size);
        ret = sip_svc_send(fcs_handle, FCS_GET_DIGEST_UPDATE,
                fcs_digest_update_args, sizeof(fcs_digest_update_args),
                fcs_digest_update_smc_resp, sizeof(fcs_digest_update_smc_resp));
    }
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx",
                    fcs_digest_update_smc_resp[0],
                    fcs_digest_update_smc_resp[1]);

            if ((fcs_digest_update_smc_resp[FCS_RESP_STATUS] == 0UL) &&
                    (final == FCS_FINALIZE))
            {
                cache_force_invalidate((void *)fcs_out_ops_buffer,
                        (size_t)fcs_digest_update_smc_resp[FCS_RESP_SIZE]);
                /* Ignore the FCS response header in the response,
                 * copy only the required data */
                *digest_size =
                        (uint32_t)fcs_digest_update_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)digest_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA],
                        *digest_size);
            }
            status = (uint16_t)fcs_digest_update_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_get_digest(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t op_mode, uint32_t dig_size,
        char *src_data, uint32_t src_size, char *digest_data,
        uint32_t *digest_size)
{
    int ret;
    uint32_t session_id = 0U, remaining_data = src_size, data_written;
    sdm_client_handle_t fcs_handle;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    /* check if 8 byte aligned */
    if ((((uint64_t)(uintptr_t)src_data % 8UL) != 0UL) ||
            (((uint64_t)(uintptr_t)digest_data % 8UL) != 0UL) || (digest_size ==
            NULL))
    {
        ERROR("Invalid address");
        return -EINVAL;
    }
    if ((uuid == NULL) || (src_data == NULL) || (digest_data == NULL))
    {
        return -EINVAL;
    }
    if ((src_size < 8U) || ((src_size % 8U) != 0U))
    {
        ERROR("Invalid Size");
        return -EINVAL;
    }

    ret = run_fcs_get_digest_init(uuid, context_id, key_id, op_mode, dig_size);
    if (ret != 0)
    {
        ERROR("Failed to initialise GET_DIGEST");
        return ret;
    }

    while (remaining_data > 0U)
    {
        if (remaining_data > FCS_CRYPTO_BLOCK_SIZE)
        {
            data_written = FCS_CRYPTO_BLOCK_SIZE;
            ret = run_fcs_get_digest_update(uuid, context_id, src_data,
                    FCS_CRYPTO_BLOCK_SIZE, digest_data, digest_size,
                    FCS_UPDATE);
        }
        else
        {
            data_written = remaining_data;
            ret = run_fcs_get_digest_update(uuid, context_id, src_data,
                    remaining_data, digest_data, digest_size, FCS_FINALIZE);
        }
        if (ret == 0)
        {
            remaining_data -= data_written;
            src_data += data_written;
        }
        else
        {
            ERROR("GET_DIGEST failed");
            return ret;
        }
    }
    return ret;
}
static int run_fcs_mac_verify_init(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t dig_size)
{
    uint64_t fcs_mac_verify_init_args[5];
    uint32_t session_id = 0U;
    sdm_client_handle_t fcs_handle;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    fcs_mac_verify_init_args[0] = session_id;
    fcs_mac_verify_init_args[1] = context_id;
    fcs_mac_verify_init_args[2] = key_id;
    fcs_mac_verify_init_args[3] = FCS_MAC_PARAM_SIZE;
    fcs_mac_verify_init_args[4] = FCS_SET_DIGEST_SIZE((uint64_t)dig_size);

    DEBUG("Mac_verify_init: key_id: %lu, dig_size: %lu", key_id, dig_size);
    return sip_svc_send(fcs_handle, FCS_MAC_VERIFY_INIT,
            fcs_mac_verify_init_args, sizeof(fcs_mac_verify_init_args), NULL,
            0);
}
static int run_fcs_mac_verify_update(char *uuid, uint32_t context_id,
        char *src_addr, uint32_t src_size,
        char *mac_data, uint32_t mac_data_size,
        char *dest_data, uint32_t *dest_size,
        uint8_t final)
{
    (void)mac_data;
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_mac_verify_args[8], mac_verify_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    *dest_size = 0;
    fcs_mac_verify_args[0] = session_id;
    fcs_mac_verify_args[1] = context_id;
    fcs_mac_verify_args[2] = (uint64_t)src_addr;
    fcs_mac_verify_args[3] = (uint64_t)src_size + (uint64_t)mac_data_size;
    fcs_mac_verify_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_mac_verify_args[5] = FCS_MAC_VERIFY_RESP;
    fcs_mac_verify_args[6] = src_size;
    fcs_mac_verify_args[7] = (uint64_t)FCS_SMMU_GET_ADDR(src_addr);

    cache_force_write_back(src_addr, src_size + mac_data_size);
    cache_force_invalidate(fcs_out_ops_buffer, FCS_MAC_VERIFY_RESP);

    if (final == FCS_UPDATE)
    {
        DEBUG("Mac_verify_update: src_addr: %lx, src_size: %lu, "
                "mac_data_size: %lu", (uint64_t)src_addr, src_size,
                mac_data_size);
        ret = sip_svc_send(fcs_handle, FCS_MAC_VERIFY_UPDATE,
                fcs_mac_verify_args, sizeof(fcs_mac_verify_args),
                mac_verify_smc_resp, sizeof(mac_verify_smc_resp));
    }
    else
    {
        DEBUG("Mac_verify_finalize: src_addr: %lx, src_size: %lu, "
                "mac_data_size: %lu", (uint64_t)src_addr, src_size,
                mac_data_size);
        ret = sip_svc_send(fcs_handle, FCS_MAC_VERIFY_FINALIZE,
                fcs_mac_verify_args, sizeof(fcs_mac_verify_args),
                mac_verify_smc_resp, sizeof(mac_verify_smc_resp));
    }
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", mac_verify_smc_resp[0],
                    mac_verify_smc_resp[1]);
            if (mac_verify_smc_resp[FCS_RESP_STATUS] == 0UL)
            {
                cache_flush((void *)fcs_out_ops_buffer,
                        (size_t)mac_verify_smc_resp[FCS_RESP_SIZE]);
                *dest_size = (uint32_t)mac_verify_smc_resp[FCS_RESP_SIZE];
                (void)memcpy((void *)dest_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA], *dest_size);
            }
        }
        status = (uint16_t)mac_verify_smc_resp[FCS_RESP_STATUS];
        ret = (int)status;
    }
    return ret;
}
int run_fcs_mac_verify(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t dig_size, char *src_data,
        uint32_t src_size, char *dest_data, uint32_t *dest_size,
        uint32_t user_data_size)
{
    uint32_t session_id = 0U, remaining_data = src_size, data_written,
            mac_size;
    int ret;
    char *mac_data = NULL;
    sdm_client_handle_t fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    if (key_id == 0U)
    {
        ERROR("Invalid Key ID");
        return -EINVAL;
    }
    if ((((uint64_t)(uintptr_t)src_data % 8UL) != 0UL) || (dest_data == NULL) ||
            (dest_size == NULL))
    {
        ERROR("Invalid address");
        return -EINVAL;
    }
    if ((src_size < 8U) || ((src_size % 8U) != 0U))
    {
        ERROR("Invalid Size");
        return -EINVAL;
    }
    if ((uuid == NULL) || (src_data == NULL) || (dest_data == NULL))
    {
        return -EINVAL;
    }

    ret = run_fcs_mac_verify_init(uuid, context_id, key_id, dig_size);
    if (ret != 0)
    {
        ERROR("Failed to initialise mac_verify");
        return ret;
    }
    mac_data = src_data + user_data_size;
    mac_size = src_size - user_data_size;
    while (remaining_data > 0U)
    {
        if (remaining_data > FCS_CRYPTO_BLOCK_SIZE)
        {
            /* minimum 8 bytes of data for final command*/
            if ((remaining_data - FCS_CRYPTO_BLOCK_SIZE) >= (8U + mac_size))
            {
                data_written = FCS_CRYPTO_BLOCK_SIZE;
                ret = run_fcs_mac_verify_update(uuid, context_id, src_data,
                        data_written, mac_data, 0U, dest_data, dest_size,
                        FCS_UPDATE);

            }
            else
            {
                data_written = remaining_data - (8U + mac_size);
                ret = run_fcs_mac_verify_update(uuid, context_id, src_data,
                        data_written, mac_data, 0U, dest_data, dest_size,
                        FCS_UPDATE);
            }
        }
        else
        {
            data_written = remaining_data;
            ret = run_fcs_mac_verify_update(uuid, context_id, src_data,
                    remaining_data - mac_size, mac_data, mac_size, dest_data,
                    dest_size, FCS_FINALIZE);
        }
        if (ret == 0)
        {
            remaining_data -= data_written;
            src_data += data_written;
        }
        else
        {
            ERROR("mac_verify failed");
            return ret;
        }
    }
    return ret;
}

int run_fcs_sdos_encrypt(char *uuid, uint32_t context_id,
        char *src_data, uint32_t src_size, char *resp_data,
        uint32_t *resp_size)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t sdos_encrypt_args[10], sdos_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((src_data == NULL) || (src_size == 0U))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    if ((resp_data == NULL) || (resp_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    sdos_encrypt_args[0] = session_id;
    sdos_encrypt_args[1] = context_id;
    sdos_encrypt_args[2] = FCS_SDOS_ENCRYPT_MODE;
    /*
     * We shall provide the smmu address instead of the
     * regular source address for the SMC call
     */
    sdos_encrypt_args[3] = (uint64_t)(uintptr_t)src_data;
    sdos_encrypt_args[4] = src_size;
    sdos_encrypt_args[5] = (uint64_t)(uintptr_t)resp_data;
    sdos_encrypt_args[6] = FCS_SDOS_ENC_MAX_RESP;
    /* No owner id required for encryption */
    sdos_encrypt_args[7] = 0;
    sdos_encrypt_args[8] = FCS_SMMU_GET_ADDR(src_data);
    sdos_encrypt_args[9] = FCS_SMMU_GET_ADDR(resp_data);
    cache_force_write_back(src_data, src_size);
    cache_force_invalidate(resp_data, FCS_SDOS_ENC_MAX_RESP);

    DEBUG("Sdos_encrypt: src_addr: %lx, src_size: %lu, resp_addr: %lx",
            (uint64_t)src_data, src_size, (uint64_t)resp_data);
    ret = sip_svc_send(fcs_handle, FCS_SDOS_CRYPTION, sdos_encrypt_args,
            sizeof(sdos_encrypt_args), sdos_smc_resp, sizeof(sdos_smc_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", sdos_smc_resp[0],
                    sdos_smc_resp[1]);
            if (sdos_smc_resp[FCS_RESP_STATUS] == 0UL)
            {
                *resp_size = (uint32_t)sdos_smc_resp[FCS_RESP_SIZE];
                cache_force_invalidate(resp_data, *resp_size);
            }
            status = (uint16_t)sdos_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}

int run_fcs_sdos_decrypt(char *uuid, uint32_t context_id,
        char *src_data, uint32_t src_size, char *resp_data,
        uint32_t *resp_size, uint64_t owner_flag)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t sdos_decrypt_args[10], sdos_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((src_data == NULL) || (src_size == 0U))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    if ((resp_data == NULL) || (resp_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }

    sdos_decrypt_args[0] = session_id;
    sdos_decrypt_args[1] = context_id;
    sdos_decrypt_args[2] = FCS_SDOS_DECRYPT_MODE;
    sdos_decrypt_args[3] = (uint64_t)(uintptr_t)src_data;
    sdos_decrypt_args[4] = src_size;
    sdos_decrypt_args[5] = (uint64_t)(uintptr_t)resp_data;
    sdos_decrypt_args[6] = FCS_SDOS_DEC_MAX_RESP;
    sdos_decrypt_args[7] = owner_flag;

    /* SMMU address is used by the mailbox */
    sdos_decrypt_args[8] = FCS_SMMU_GET_ADDR(src_data);
    sdos_decrypt_args[9] = FCS_SMMU_GET_ADDR(resp_data);
    cache_force_write_back(src_data, src_size);
    cache_force_invalidate(resp_data, FCS_SDOS_DEC_MAX_RESP);

    DEBUG(
            "Sdos_decrypt: owner_flag: %d, src_addr: %lx, src_size: %lu, resp_addr: %lx",
            owner_flag, (uint64_t)src_data, src_size, (uint64_t)resp_data,
            *resp_size);
    ret = sip_svc_send(fcs_handle, FCS_SDOS_CRYPTION, sdos_decrypt_args,
            sizeof(sdos_decrypt_args), sdos_smc_resp, sizeof(sdos_smc_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", sdos_smc_resp[0],
                    sdos_smc_resp[1]);
            if (sdos_smc_resp[FCS_RESP_STATUS] == 0UL)
            {
                *resp_size = (uint32_t)sdos_smc_resp[FCS_RESP_SIZE];
                cache_force_invalidate(resp_data, *resp_size);
            }
            status = (uint16_t)sdos_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}

int run_fcs_hkdf_request(char *uuid, uint32_t key_id,
        uint32_t step_type, uint32_t mac_mode, char *input_buffer,
        uint32_t output_key_size, uint32_t *hkdf_status)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t hkdf_args[6], hkdf_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if (input_buffer == NULL)
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    if (output_key_size == 0U)
    {
        ERROR("Invalid output key size");
        return -EINVAL;
    }

    /*
     * HKDF Input buffer data format
     * 1st input data size in bytes
     * 1st input data padded to 80 bytes
     * 2nd input data size in bytes
     * 2nd input data padded to 80 bytes
     * Output key object
     */
    hkdf_args[0] = session_id;
    hkdf_args[1] = step_type;
    hkdf_args[2] = mac_mode;
    hkdf_args[3] = (uint64_t)input_buffer;
    hkdf_args[4] = key_id;
    hkdf_args[5] = output_key_size;
    cache_force_write_back(input_buffer, 400);

    DEBUG("HKDF_request: key_id: %lu, step_type: %lu, "
            "mac_mode: %lu, input_buffer: %lx, output_key_size: %lu", key_id,
            step_type, mac_mode, (uint64_t)input_buffer, output_key_size);
    ret = sip_svc_send(fcs_handle, FCS_HKDF_REQUEST, hkdf_args,
            sizeof(hkdf_args), hkdf_resp, sizeof(hkdf_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", hkdf_resp[0],
                    hkdf_resp[1]);
            if (hkdf_resp[FCS_RESP_STATUS] == 0UL)
            {
                status = (uint16_t)(hkdf_resp[FCS_RESP_STATUS] &
                        FCS_STATUS_MASK);
                *hkdf_status = (uint32_t)(hkdf_resp[FCS_RESP_KEY_STATUS] &
                        FCS_STATUS_MASK);
                return (int)status;
            }
        }
    }
    return ret;
}

int run_fcs_get_chip_id(uint32_t *chip_low,
        uint32_t *chip_high)
{
    uint64_t get_chip_id_resp[FCS_RESP_DATA] =
    {
        0
    };
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }

    DEBUG("Get_chip_id: No args");
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_GET_CHIP_ID, NULL,
            0, get_chip_id_resp, sizeof(get_chip_id_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x0: %lx, x1: %lx, x2: %lx",
                    get_chip_id_resp[0], get_chip_id_resp[1],
                    get_chip_id_resp[2]);
            if (get_chip_id_resp[FCS_RESP_STATUS] == 0UL)
            {
                *chip_low = (uint32_t)get_chip_id_resp[1];
                *chip_high = (uint32_t)get_chip_id_resp[2];
            }
            status = (uint16_t)(get_chip_id_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }
    return ret;
}

int run_fcs_attestation_get_certificate(int cert_req, char *cert_data,
        uint32_t *cert_size)
{
    uint64_t get_attestation_cert_args[3], get_attestation_cert_resp[2] =
    {
        0
    };
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    if ((cert_data == NULL) || (cert_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    cache_force_invalidate(cert_data, FCS_ATTEST_CERT_MAX_SIZE);

    get_attestation_cert_args[0] = (uint64_t)cert_req;
    get_attestation_cert_args[1] = (uint64_t)cert_data;
    get_attestation_cert_args[2] = *cert_size;

    DEBUG("Get_attestation_certificate: cert_req: %d, "
            "cert_data: %lx", cert_req, (uint64_t)cert_data);
    ret = sip_svc_send(fcs_descriptor->security_handle,
            FCS_GET_ATTESTATION_CERT, get_attestation_cert_args,
            sizeof(get_attestation_cert_args), get_attestation_cert_resp,
            sizeof(get_attestation_cert_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx",
                    get_attestation_cert_resp[0], get_attestation_cert_resp[1]);
            if (get_attestation_cert_resp[FCS_RESP_STATUS] == 0UL)
            {
                *cert_size = (uint32_t)get_attestation_cert_resp[FCS_RESP_SIZE];
                cache_force_invalidate(cert_data, *cert_size);
            }
            status = (uint16_t)(get_attestation_cert_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }
    return ret;
}

int run_fcs_attestation_certificate_reload(int cert_req)
{
    uint64_t cert_on_reload_args[3], cert_reload_err = 0UL;
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    cert_on_reload_args[0] = (uint64_t)cert_req;

    DEBUG("Cert_on_reload: cert_req: %d", cert_req);
    ret = sip_svc_send(fcs_descriptor->security_handle,
            FCS_CREATE_CERT_ON_RELOAD, cert_on_reload_args,
            sizeof(cert_on_reload_args), &cert_reload_err,
            sizeof(cert_reload_err));
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx", cert_reload_err);
            status = (uint16_t)cert_reload_err;
            return (int)status;
        }
    }

    return ret;
}

int run_fcs_mctp_cmd_send(char *src_data, uint32_t src_size, char *resp_data,
        uint32_t *resp_size)
{
    uint64_t mctp_send_args[3], mctp_send_resp[2] =
    {
        0
    };
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    if ((src_data == NULL) || (src_size == 0U) || (resp_data == NULL) ||
            (resp_size == NULL))
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    cache_force_write_back(src_data, src_size);
    cache_force_invalidate(resp_data, FCS_MCTP_MAX_SIZE);

    mctp_send_args[0] = (uint64_t)src_data;
    mctp_send_args[1] = src_size;
    mctp_send_args[2] = (uint64_t)resp_data;

    DEBUG("Mctp_send: src_data: %lx, src_size: %lu, resp_data: %lx",
            (uint64_t)src_data, src_size, (uint64_t)resp_data);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_MCTP_SEND_MSG,
            mctp_send_args, sizeof(mctp_send_args), mctp_send_resp,
            sizeof(mctp_send_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", mctp_send_resp[0],
                    mctp_send_resp[1]);
            if (mctp_send_resp[FCS_RESP_STATUS] == 0UL)
            {
                *resp_size = (uint32_t)mctp_send_resp[FCS_RESP_SIZE];
                cache_force_invalidate(resp_data, *resp_size);
            }
            status = (uint16_t)(mctp_send_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }
    return ret;
}

int run_fcs_get_jtag_idcode(uint32_t *jtag_id_code)
{
    uint64_t get_idcode_resp[2] =
    {
        0
    };
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }

    DEBUG("Get_jtag_idcode: No args");
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_GET_IDCODE, NULL, 0,
            get_idcode_resp, sizeof(get_idcode_resp));
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", get_idcode_resp[0],
                    get_idcode_resp[1]);
            if (get_idcode_resp[FCS_RESP_STATUS] == 0UL)
            {
                *jtag_id_code = (uint32_t)get_idcode_resp[1];
            }
            status = (uint16_t)(get_idcode_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }

    return ret;
}

int run_fcs_get_device_identity(char *dev_identity, uint32_t *dev_id_size)
{
    uint64_t get_device_identity_args[1], get_device_identity_resp[2] =
    {
        0
    };
    int ret;
    uint16_t status;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    cache_force_invalidate(dev_identity, FCS_DEV_IDENTITY_RESP_SIZE);

    get_device_identity_args[0] = (uint64_t)dev_identity;

    DEBUG("Get_device_identity: dev_identity_buffer: %lx",
            (uint64_t)dev_identity);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_GET_DEVICE_IDENTITY,
            get_device_identity_args, sizeof(get_device_identity_args),
            get_device_identity_resp, sizeof(get_device_identity_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx",
                    get_device_identity_resp[0], get_device_identity_resp[1]);
            if (get_device_identity_resp[FCS_RESP_STATUS] == 0UL)
            {
                *dev_id_size =
                        (uint32_t)get_device_identity_resp[FCS_RESP_SIZE];
                cache_force_invalidate(dev_identity, *dev_id_size);
            }
            status = (uint16_t)(get_device_identity_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }
    return ret;
}

static int fcs_aes_set_params(uint32_t block_mode, uint32_t crypt_mode,
        uint32_t iv_src, char *iv_data, uint32_t tag_size, uint32_t aad_size,
        uint32_t *param_data, uint32_t *param_size)
{
    if ((block_mode != FCS_AES_ECB) && (block_mode != FCS_AES_CBC) &&
            (block_mode != FCS_AES_CTR) && (block_mode != FCS_AES_GCM) &&
            (block_mode != FCS_AES_GCM_GHASH))
    {
        ERROR("Invalid AES mode");
        return -EINVAL;
    }
    param_data[0] = block_mode;
    if (block_mode == FCS_AES_GCM_GHASH)
    {
        crypt_mode = 0;
    }
    *param_size = FCS_AES_MAX_PARAM_SIZE;
    param_data[0] |= FCS_SET_CRYPT_MODE(crypt_mode);
    if ((block_mode != FCS_AES_GCM) && (block_mode != FCS_AES_GCM_GHASH))
    {
        /*
         * tags not applicable for non GCM and GCM-GHASH modes
         * iv generation not applicable for non GCM and GCM-GHASH modes
         * aad data not applicable for non GCM and GCM-GHASH modes
         */
        tag_size = 0x0;
        iv_src = 0x0;
        param_data[1] = 0;
        param_data[2] = 0;
        if (FCS_AES_BLOCK_MODE(block_mode) == FCS_AES_ECB)
        {
            *param_size = FCS_AES_ECB_PARAM_SIZE;
        }
    }
    else
    {
        if (tag_size > FCS_AES_TAG_128)
        {
            ERROR("Invalid tag length");
            return -EINVAL;
        }
        param_data[0] |= FCS_SET_TAG_LEN(tag_size);
        if (iv_src > FCS_IV_INTERNAL_BASE)
        {
            ERROR("Invalid IV source");
            return -EINVAL;
        }
        param_data[1] = iv_src;
        if (aad_size > FCS_AAD_MAX_SIZE)
        {
            ERROR("Invalid AAD size");
            return -EINVAL;
        }
        param_data[2] = aad_size;

    }
    (void)memcpy((void *)&param_data[3], (void *)iv_data, FCS_AES_IV_SIZE);
    return 0;
}
static int run_fcs_aes_crypt_init(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t block_mode, uint32_t crypt_mode,
        uint32_t iv_src, char *iv_data, uint32_t tag_size,
        uint32_t aad_size)
{
    int ret;
    uint64_t fcs_aes_init_args[5];
    uint32_t session_id = 0U, param_size = 0;
    sdm_client_handle_t fcs_handle;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    ret = fcs_aes_set_params(block_mode, crypt_mode, iv_src, iv_data, tag_size,
            aad_size, fcs_inp_ops_buffer, &param_size);
    DEBUG("AES params: block_mode: %d, crypt_mode: %d, iv_src: %d, "
            "tag_size: %d, aad_size: %d", block_mode, crypt_mode, iv_src,
            tag_size, aad_size);
    if (ret != 0)
    {
        ERROR("Failed to set AES params");
        return -EINVAL;
    }
    cache_force_write_back(fcs_inp_ops_buffer, param_size);

    fcs_aes_init_args[0] = session_id;
    fcs_aes_init_args[1] = context_id;
    fcs_aes_init_args[2] = key_id;
    fcs_aes_init_args[3] = (uint64_t)fcs_inp_ops_buffer;
    fcs_aes_init_args[4] = param_size;

    ret = sip_svc_send(fcs_handle, FCS_AES_INIT, fcs_aes_init_args,
            sizeof(fcs_aes_init_args), NULL, 0);

    return ret;
}
static int run_fcs_aes_update(char *uuid, uint32_t context_id,
        char *src_addr, uint32_t src_size, char *dest_addr,
        uint32_t dest_size, uint32_t padding_size,
        uint8_t final)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_aes_update_args[9], fcs_aes_update_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    cache_force_write_back(src_addr, src_size);
    cache_force_invalidate(dest_addr, dest_size);

    fcs_aes_update_args[0] = session_id;
    fcs_aes_update_args[1] = context_id;
    fcs_aes_update_args[2] = (uint64_t)(uintptr_t)src_addr;
    fcs_aes_update_args[3] = src_size;
    fcs_aes_update_args[4] = (uint64_t)(uintptr_t)dest_addr;
    fcs_aes_update_args[5] = dest_size;
    fcs_aes_update_args[6] = padding_size;

    /* SMMU address is used by the mailbox */
    fcs_aes_update_args[7] = FCS_SMMU_GET_ADDR(src_addr);
    fcs_aes_update_args[8] = FCS_SMMU_GET_ADDR(dest_addr);
    if (final == FCS_UPDATE)
    {
        DEBUG("AES update: src_addr: %lx, src_size: %u, "
                "dest_addr: %lx, dest_size: %u, padding_size: %u",
                (uint64_t)src_addr, src_size, (uint64_t)dest_addr, dest_size,
                padding_size);
        ret = sip_svc_send(fcs_handle, FCS_AES_UPDATE, fcs_aes_update_args,
                sizeof(fcs_aes_update_args), fcs_aes_update_resp,
                sizeof(fcs_aes_update_resp));
    }
    else
    {
        DEBUG("AES finalize: src_addr: %lx, src_size: %u, "
                "dest_addr: %lx, dest_size: %u, padding_size: %u",
                (uint64_t)src_addr, src_size, (uint64_t)dest_addr, dest_size,
                padding_size);
        ret = sip_svc_send(fcs_handle, FCS_AES_FINALIZE, fcs_aes_update_args,
                sizeof(fcs_aes_update_args), fcs_aes_update_resp,
                sizeof(fcs_aes_update_resp));
    }
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", fcs_aes_update_resp[0],
                    fcs_aes_update_resp[1]);
            if (fcs_aes_update_resp[FCS_RESP_STATUS] == 0U)
            {
                cache_force_invalidate(dest_addr, dest_size);
            }
            status = (uint16_t)(fcs_aes_update_resp[FCS_RESP_STATUS] &
                    FCS_STATUS_MASK);
            return (int)status;
        }
    }

    return ret;
}

int run_fcs_aes_cryption(char *uuid, uint32_t key_id,
        uint32_t context_id, uint32_t crypt_mode, uint32_t block_mode,
        uint32_t iv_src, char *iv_data, uint32_t tag_size,
        uint32_t aad_size, char *aad_data, char *tag_data, char *input_data,
        uint32_t input_size, char *output_data)
{
    int ret;
    int finalized = 0;
    uint32_t session_id = 0U, data_rem, padding1,
            padding2, aes_input_size, aes_output_size, input_tag = 0U;
    char *aes_input_data, *aes_output_data, *temp = NULL;
    sdm_client_handle_t fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    padding1 = 0;
    padding2 = 0;
    aes_input_size = 0;
    input_tag = 0;
    if ((block_mode == FCS_AES_GCM) || (block_mode == FCS_AES_GCM_GHASH))
    {
        /* pad to make it multiple of 16 bytes */
        if ((aad_size % FCS_GCM_BLOCK_SIZE) != 0U)
        {
            padding1 = FCS_GCM_BLOCK_SIZE - (aad_size % FCS_GCM_BLOCK_SIZE);
        }
        aes_input_size = aad_size + padding1;
    }
    aes_input_size += input_size;
    if ((block_mode == FCS_AES_GCM) || (block_mode == FCS_AES_GCM_GHASH))
    {
        if ((input_size % FCS_GCM_BLOCK_SIZE) != 0U)
        {
            padding2 = FCS_GCM_BLOCK_SIZE - (input_size % FCS_GCM_BLOCK_SIZE);
        }
    }
    else
    {
        /* Non GCM modes have 32 byte blocks and aad data is not applicable*/
        if ((input_size % FCS_NON_GCM_BLOCK_SIZE) != 0U)
        {
            padding2 = FCS_NON_GCM_BLOCK_SIZE - (input_size %
                    FCS_NON_GCM_BLOCK_SIZE);
            aad_size = 0U;
        }
    }
    if (padding2 != 0U)
    {
        DEBUG("Padding input data with %u bytes", padding2);
    }
    aes_input_size += padding2;
    if (((crypt_mode == FCS_AES_DECRYPT_MODE) && (block_mode == FCS_AES_GCM)) ||
            (block_mode == FCS_AES_GCM_GHASH))
    {
        aes_input_size += FCS_GCM_TAG_SIZE;
        input_tag = FCS_GCM_TAG_SIZE;
    }
    ret = run_fcs_aes_crypt_init(uuid, context_id, key_id, block_mode,
            crypt_mode, iv_src, iv_data, tag_size, aad_size);
    if (ret != 0)
    {
        ERROR("Failed to initialise AES sequence");
        return ret;
    }
    /*
     * Mailbox requires the input data in the following format
     * AAD data(for GCM modes), padding1(for GCM modes), input data,
     * padding2(for GCM modes), tag data(if applicable)
     */

    /* Setting initial data */
    aes_input_data = (char *)fcs_inp_ops_buffer;
    aes_output_data = (char *)fcs_out_ops_buffer;
    (void)memcpy(aes_input_data, aad_data, aad_size);
    (void)memset(aes_input_data + aad_size, 0, padding1);

    data_rem = aes_input_size;
    while (data_rem > 0U)
    {
        if (data_rem > FCS_CRYPTO_BLOCK_SIZE)
        {
            aes_input_size = FCS_CRYPTO_BLOCK_SIZE;
            if (block_mode == FCS_AES_GCM_GHASH)
            {
                aes_output_size = 0;
            }
            else
            {
                /*
                 * padding1 and aad_size is 0 for non GCM modes so only
                 * input size is taken
                 */
                aes_output_size = aes_input_size - (aad_size + padding1);
            }
            (void)memcpy(aes_input_data + aad_size + padding1, input_data,
                    aes_input_size - (aad_size + padding1));
            ret = run_fcs_aes_update(uuid, context_id, aes_input_data,
                    aes_input_size, aes_output_data, aes_output_size, 0,
                    FCS_UPDATE);
            /* aad data and padding written to SDM if applicable */
            aad_size = 0;
            padding1 = 0;
        }
        else
        {
            if (((crypt_mode == FCS_AES_DECRYPT_MODE) && (block_mode ==
                    FCS_AES_GCM)) || (block_mode == FCS_AES_GCM_GHASH))
            {
                if (data_rem > (FCS_CRYPTO_BLOCK_SIZE - FCS_GCM_TAG_SIZE))
                {
                    /* Sending all data except the tag data in last update*/
                    aes_input_size = FCS_CRYPTO_BLOCK_SIZE - FCS_GCM_TAG_SIZE;
                    if (block_mode == FCS_AES_GCM_GHASH)
                    {
                        aes_output_size = 0;
                    }
                    else
                    {
                        aes_output_size = FCS_CRYPTO_BLOCK_SIZE -
                                (FCS_GCM_TAG_SIZE + aad_size + padding1);
                    }
                    (void)memcpy(aes_input_data + aad_size + padding1,
                            input_data, aes_input_size - (aad_size + padding1));

                    ret = run_fcs_aes_update(uuid, context_id, aes_input_data,
                            aes_input_size, aes_output_data, aes_output_size,
                            padding2, FCS_UPDATE);
                    if (ret == 0)
                    {
                        data_rem -= aes_input_size;
                        aes_input_data += aes_input_size;
                        output_data += aes_output_size;
                    }
                    else
                    {
                        ERROR("AES_CRYPTION failed");
                        break;
                    }
                }
                /* If false, we just send all the data at once */
            }
            /* doesn't affect non GCM modes */
            aes_input_size = data_rem;
            if (block_mode == FCS_AES_GCM_GHASH)
            {
                aes_output_size = 0;
            }
            else
            {
                aes_output_size = aes_input_size - (aad_size + padding1 +
                        input_tag);
            }
            if ((block_mode == FCS_AES_GCM) && (crypt_mode ==
                    FCS_AES_ENCRYPT_MODE))
            {
                aes_output_size += FCS_GCM_TAG_SIZE;
            }
            finalized = 1;
            /* Copy input data to the input buffer */
            (void)memcpy(aes_input_data + aad_size + padding1, input_data,
                    aes_input_size);
            (void)memset(aes_input_data + aad_size + padding1 + input_size
                    , 0, padding2);
            /* Copying tag data */
            (void)memcpy(
                    aes_input_data + aad_size + padding1 + input_size +
                    padding2, tag_data, input_tag);

            ret = run_fcs_aes_update(uuid, context_id, aes_input_data,
                    aes_input_size, aes_output_data, aes_output_size, padding2,
                    FCS_FINALIZE);
        }
        if (ret == 0)
        {
            if ((block_mode == FCS_AES_GCM) && (crypt_mode ==
                    FCS_AES_ENCRYPT_MODE) && (finalized == 1))
            {
                /* GCM mode, tag is appended to output data */
                (void)memcpy(output_data, aes_output_data, aes_output_size -
                        FCS_GCM_TAG_SIZE);
                (void)memcpy(tag_data, aes_output_data + aes_output_size -
                        FCS_GCM_TAG_SIZE, FCS_GCM_TAG_SIZE);
            }
            else
            {
                (void)memcpy(output_data, aes_output_data, aes_output_size);
            }
            /* Increment the block */
            data_rem -= aes_input_size;
            output_data += aes_output_size;
        }
        else
        {
            ERROR("AES_CRYPTION failed");
            break;
        }
        memset(fcs_inp_ops_buffer, 0, sizeof(fcs_inp_ops_buffer));
    }
    aes_input_data = temp;
    return ret;
}

int run_fcs_ecdsa_hash_sign(char *uuid, uint32_t context_id, uint32_t key_id,
        uint32_t ecc_algo, char *hash_data, uint32_t hash_data_size,
        char *signed_data, uint32_t *signed_data_size)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_hash_sign_args[6], hash_sign_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((ecc_algo != FCS_ECC_NISTP_256) && (ecc_algo != FCS_ECC_NISTP_384) &&
            (ecc_algo != FCS_ECC_BRAINPOOL_256) && (ecc_algo !=
            FCS_ECC_BRAINPOOL_384))
    {
        ERROR("Invalid ECC Algorithm");
        return -EINVAL;
    }
    fcs_hash_sign_args[0] = session_id;
    fcs_hash_sign_args[1] = context_id;
    fcs_hash_sign_args[2] = key_id;
    fcs_hash_sign_args[3] = FCS_ECDSA_PARAM_SIZE;
    fcs_hash_sign_args[4] = ecc_algo;
    DEBUG("Hash data sign init: Key ID: %d, Ecc Algo: %d", key_id, ecc_algo);
    ret = sip_svc_send(fcs_handle, FCS_ECDSA_HASH_SIGN_INIT, fcs_hash_sign_args,
            sizeof(fcs_hash_sign_args), NULL, 0);

    if (ret != 0)
    {
        return ret;
    }
    cache_force_write_back(hash_data, hash_data_size);
    cache_force_invalidate(fcs_out_ops_buffer, FCS_ECDSA_HASH_SIGN_MAX_RESP);

    fcs_hash_sign_args[0] = session_id;
    fcs_hash_sign_args[1] = context_id;
    fcs_hash_sign_args[2] = (uint64_t)hash_data;
    fcs_hash_sign_args[3] = hash_data_size;
    fcs_hash_sign_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_hash_sign_args[5] = FCS_ECDSA_HASH_SIGN_MAX_RESP;

    DEBUG("Hash data sign finalize: Hash data: %lx, Hash data size: %u",
            hash_data, hash_data_size);
    ret = sip_svc_send(fcs_handle, FCS_ECDSA_HASH_SIGN_FINALIZE,
            fcs_hash_sign_args, sizeof(fcs_hash_sign_args), hash_sign_smc_resp,
            sizeof(hash_sign_smc_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response x1: %lx, x2: %lx", hash_sign_smc_resp[0],
                    hash_sign_smc_resp[1]);
            if (hash_sign_smc_resp[FCS_RESP_STATUS] == 0U)
            {
                cache_force_invalidate((void *)fcs_out_ops_buffer,
                        (size_t)hash_sign_smc_resp[FCS_RESP_SIZE]);
                /* Ignoring the FCS response header */
                *signed_data_size =
                        (uint32_t)hash_sign_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)signed_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA],
                        *signed_data_size);
            }
            status = (uint16_t)hash_sign_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_ecdsa_hash_verify(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t ecc_algo, char *hash_data,
        uint32_t hash_data_size, char *sig_data, uint32_t sig_size,
        char *pub_key_data, uint32_t pub_key_size, char *dest_data,
        uint32_t *dest_size)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_hash_sign_verify_args[6], hash_sign_verify_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U, mbox_arg_size;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((ecc_algo != FCS_ECC_NISTP_256) && (ecc_algo != FCS_ECC_NISTP_384) &&
            (ecc_algo != FCS_ECC_BRAINPOOL_256) && (ecc_algo !=
            FCS_ECC_BRAINPOOL_384))
    {
        ERROR("Invalid ECC Algorithm");
        return -EINVAL;
    }

    fcs_hash_sign_verify_args[0] = session_id;
    fcs_hash_sign_verify_args[1] = context_id;
    fcs_hash_sign_verify_args[2] = key_id;
    fcs_hash_sign_verify_args[3] = FCS_ECDSA_PARAM_SIZE;
    fcs_hash_sign_verify_args[4] = ecc_algo;
    DEBUG("Hash data sign verify init: Key ID: %d, Ecc Algo: %d", key_id,
            ecc_algo);
    ret = sip_svc_send(fcs_handle, FCS_ECDSA_HASH_SIGN_VERIFY_INIT,
            fcs_hash_sign_verify_args, sizeof(fcs_hash_sign_verify_args), NULL,
            0);

    if (ret != 0)
    {
        return ret;
    }

    (void)memcpy(fcs_inp_ops_buffer, hash_data, hash_data_size);
    (void)memcpy(fcs_inp_ops_buffer + (hash_data_size / sizeof(uint32_t)),
            sig_data, sig_size);
    mbox_arg_size = hash_data_size + sig_size;
    if ((pub_key_data != NULL) && (pub_key_size != 0U))
    {
        (void)memcpy(fcs_inp_ops_buffer + ((hash_data_size + sig_size) /
                sizeof(uint32_t)), pub_key_data, pub_key_size);
        mbox_arg_size += pub_key_size;
    }
    cache_force_write_back(fcs_inp_ops_buffer, mbox_arg_size);
    cache_force_invalidate(fcs_out_ops_buffer, FCS_ECDSA_HASH_VERIFY_RESP);

    fcs_hash_sign_verify_args[0] = session_id;
    fcs_hash_sign_verify_args[1] = context_id;
    fcs_hash_sign_verify_args[2] = (uint64_t)fcs_inp_ops_buffer;
    fcs_hash_sign_verify_args[3] = mbox_arg_size;
    fcs_hash_sign_verify_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_hash_sign_verify_args[5] = FCS_ECDSA_HASH_VERIFY_RESP;

    DEBUG("Hash data sign verify finalize: Hash data: %lx, "
            "Hash data size: %u, Sig data: %lx, Sig size: %u,"
            "Pub key data: %lx, Pub key size: %u", (uint64_t)hash_data,
            hash_data_size, (uint64_t)sig_data, sig_size,
            (uint64_t)pub_key_data, pub_key_size);

    ret = sip_svc_send(fcs_handle, FCS_ECDSA_HASH_SIGN_VERIFY_FINALIZE,
            fcs_hash_sign_verify_args, sizeof(fcs_hash_sign_verify_args),
            hash_sign_verify_smc_resp, sizeof(hash_sign_verify_smc_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1: %lx, x2: %lx",
                    hash_sign_verify_smc_resp[0], hash_sign_verify_smc_resp[1]);
            if (hash_sign_verify_smc_resp[FCS_RESP_STATUS] == 0UL)
            {
                cache_force_invalidate((void *)fcs_out_ops_buffer,
                        (size_t)hash_sign_verify_smc_resp[FCS_RESP_SIZE]);
                /* Ignoring the FCS response header */
                *dest_size =
                        (uint32_t)hash_sign_verify_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)dest_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA], *dest_size);
            }
            status = (uint16_t)hash_sign_verify_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}

static int run_fcs_ecdsa_sha2_data_sign_init(char *uuid,
        uint32_t context_id, uint32_t key_id, uint32_t ecc_algo)
{
    int ret;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_ecdsa_sha2_init_args[5];
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((ecc_algo != FCS_ECC_NISTP_256) && (ecc_algo != FCS_ECC_NISTP_384) &&
            (ecc_algo != FCS_ECC_BRAINPOOL_256) && (ecc_algo !=
            FCS_ECC_BRAINPOOL_384))
    {
        ERROR("Invalid ECC Algorithm");
        return -EINVAL;
    }

    fcs_ecdsa_sha2_init_args[0] = session_id;
    fcs_ecdsa_sha2_init_args[1] = context_id;
    fcs_ecdsa_sha2_init_args[2] = key_id;
    fcs_ecdsa_sha2_init_args[3] = FCS_ECDSA_PARAM_SIZE;
    fcs_ecdsa_sha2_init_args[4] = ecc_algo;
    DEBUG("ECDSA SHA2 sign init: Key ID: %d, Ecc Algo: %d", key_id, ecc_algo);
    ret = sip_svc_send(fcs_handle, FCS_ECDSA_SHA2_SIGN_INIT,
            fcs_ecdsa_sha2_init_args, sizeof(fcs_ecdsa_sha2_init_args), NULL,
            0);

    return ret;
}

static int run_fcs_ecdsa_sha2_data_sign_update(char *uuid,
        uint32_t context_id, char *src_addr, uint32_t src_size,
        char *dest_data, uint32_t *dest_size, uint8_t final)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_sha2_sign_args[8], sha2_sign_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    cache_force_invalidate(fcs_out_ops_buffer,
            FCS_ECDSA_HASH_SHA2_SIGN_MAX_RESP);
    cache_force_write_back(src_addr, src_size);

    fcs_sha2_sign_args[0] = session_id;
    fcs_sha2_sign_args[1] = context_id;
    fcs_sha2_sign_args[2] = (uint64_t)src_addr;
    fcs_sha2_sign_args[3] = src_size;
    fcs_sha2_sign_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_sha2_sign_args[5] = FCS_ECDSA_HASH_SHA2_SIGN_MAX_RESP;
    fcs_sha2_sign_args[6] = FCS_SMMU_GET_ADDR(src_addr);

    if (final == FCS_UPDATE)
    {
        DEBUG("ECDSA SHA2 sign update: src_addr: %lx, "
                "src_size: %u", (uint64_t)src_addr, src_size);
        ret = sip_svc_send(fcs_handle, FCS_ECDSA_SHA2_SIGN_UPDATE,
                fcs_sha2_sign_args, sizeof(fcs_sha2_sign_args),
                sha2_sign_smc_resp, sizeof(sha2_sign_smc_resp));
    }
    else
    {
        DEBUG("ECDSA SHA2 sign finalize: src_addr: %lx, "
                "src_size: %u", (uint64_t)src_addr, src_size);
        ret = sip_svc_send(fcs_handle, FCS_ECDSA_SHA2_SIGN_FINALIZE,
                fcs_sha2_sign_args, sizeof(fcs_sha2_sign_args),
                sha2_sign_smc_resp, sizeof(sha2_sign_smc_resp));
    }
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1: %lx, x2: %lx", sha2_sign_smc_resp[0],
                    sha2_sign_smc_resp[1]);
            if ((sha2_sign_smc_resp[FCS_RESP_STATUS] == 0UL) && (final ==
                    FCS_FINALIZE))
            {
                cache_force_invalidate((void *)fcs_out_ops_buffer,
                        (size_t)sha2_sign_smc_resp[FCS_RESP_SIZE]);
                /* Ignoring the FCS response header */
                *dest_size = (uint32_t)sha2_sign_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)dest_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA], *dest_size);
            }
            status = (uint16_t)sha2_sign_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_ecdsa_sha2_data_sign(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t ecc_algo, char *src_data,
        uint32_t src_size, char *dest_data, uint32_t *dest_size)
{
    uint32_t session_id = 0U, remaining_data = src_size, data_written;
    int ret;
    sdm_client_handle_t fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    if (key_id == 0U)
    {
        ERROR("Invalid Key ID");
        return -EINVAL;
    }
    if ((uuid == NULL) || (src_data == NULL) || (dest_data == NULL))
    {
        return -EINVAL;
    }
    if ((((uint64_t)(uintptr_t)src_data % 8UL) != 0UL) || (dest_data == NULL) ||
            (dest_size == NULL))
    {
        ERROR("Invalid address");
        return -EINVAL;
    }
    if ((src_size < 8U) || ((src_size % 8U) != 0U))
    {
        ERROR("Invalid Size");
        return -EINVAL;
    }

    ret = run_fcs_ecdsa_sha2_data_sign_init(uuid, context_id, key_id, ecc_algo);
    if (ret != 0)
    {
        ERROR("Failed to initialise sha2_data_sign");
        return ret;
    }
    while (remaining_data > 0U)
    {
        /* Splitting the data into a maximum block size of 4MB */
        if (remaining_data > FCS_CRYPTO_BLOCK_SIZE)
        {
            data_written = FCS_CRYPTO_BLOCK_SIZE;
            ret = run_fcs_ecdsa_sha2_data_sign_update(uuid, context_id,
                    src_data, FCS_CRYPTO_BLOCK_SIZE, dest_data, dest_size,
                    FCS_UPDATE);
        }
        else
        {
            data_written = remaining_data;
            ret = run_fcs_ecdsa_sha2_data_sign_update(uuid, context_id,
                    src_data, remaining_data, dest_data, dest_size,
                    FCS_FINALIZE);
        }
        if (ret == 0)
        {
            remaining_data -= data_written;
            src_data += data_written;
        }
        else
        {
            return ret;
        }
    }
    return ret;
}
static int run_fcs_ecdsa_sha2_data_sign_verify_init(char *uuid,
        uint32_t context_id, uint32_t key_id, uint32_t ecc_algo)
{
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_ecdsa_sha2_init_args[5];
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((ecc_algo != FCS_ECC_NISTP_256) && (ecc_algo != FCS_ECC_NISTP_384) &&
            (ecc_algo != FCS_ECC_BRAINPOOL_256) && (ecc_algo !=
            FCS_ECC_BRAINPOOL_384))
    {
        ERROR("Invalid ECC Algorithm");
        return -EINVAL;
    }

    fcs_ecdsa_sha2_init_args[0] = session_id;
    fcs_ecdsa_sha2_init_args[1] = context_id;
    fcs_ecdsa_sha2_init_args[2] = key_id;
    fcs_ecdsa_sha2_init_args[3] = FCS_ECDSA_PARAM_SIZE;
    fcs_ecdsa_sha2_init_args[4] = ecc_algo;

    DEBUG("ECDSA SHA2 sign verify init: Key ID: %d, Ecc Algo: %d", key_id,
            ecc_algo);
    return sip_svc_send(fcs_handle, FCS_ECDSA_SHA2_SIGN_VERIFY_INIT,
            fcs_ecdsa_sha2_init_args, sizeof(fcs_ecdsa_sha2_init_args), NULL,
            0);
}
/*
 * Signature data should be stored after the input data
 * If pubkey is provided, ensure its after the signature to be verified
 */
static int run_fcs_ecdsa_sha2_data_sign_verify_update(char *uuid,
        uint32_t context_id, char *src_addr, uint32_t src_size,
        char *signed_data, uint32_t sig_size, char *pub_key_data,
        uint32_t pub_key_size, char *dest_data,
        uint32_t *dest_size, uint8_t final)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_sha2_sign_verify_args[8], sha2_sign_verify_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U, payload_size;

    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }

    (void)memcpy(fcs_inp_ops_buffer, (char *)(uintptr_t)src_addr, src_size);
    (void)memcpy(fcs_inp_ops_buffer + (src_size / sizeof(uint32_t)),
            signed_data, sig_size);
    payload_size = src_size + sig_size;
    if (pub_key_data != NULL)
    {
        (void)memcpy(fcs_inp_ops_buffer + (payload_size /
                sizeof(uint32_t)), pub_key_data, pub_key_size);
        payload_size += pub_key_size;
    }
    cache_force_invalidate(fcs_out_ops_buffer, FCS_ECDSA_HASH_SHA2_VERIFY_RESP);
    cache_force_write_back(fcs_inp_ops_buffer, payload_size);

    fcs_sha2_sign_verify_args[0] = session_id;
    fcs_sha2_sign_verify_args[1] = context_id;
    fcs_sha2_sign_verify_args[2] = (uint64_t)fcs_inp_ops_buffer;
    fcs_sha2_sign_verify_args[3] = payload_size;
    fcs_sha2_sign_verify_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_sha2_sign_verify_args[5] = FCS_ECDSA_HASH_SHA2_VERIFY_RESP;
    fcs_sha2_sign_verify_args[6] = src_size;
    fcs_sha2_sign_verify_args[7] = FCS_SMMU_GET_ADDR(
            (uint64_t)fcs_inp_ops_buffer);

    if (final == FCS_UPDATE)
    {
        DEBUG(
                "ECDSA SHA2 sign verify update: src_addr: %x, src_size: %u, sig_addr:%u,"
                "sig_size: %u", src_addr, src_size, signed_data, sig_size);
        if (pub_key_data != NULL)
        {
            printf("Pub key data: %lx, Pub key size: %u\n",
                    (uint64_t)pub_key_data, pub_key_size);
        }

        ret = sip_svc_send(fcs_handle, FCS_ECDSA_SHA2_SIGN_VERIFY_UPDATE,
                fcs_sha2_sign_verify_args, sizeof(fcs_sha2_sign_verify_args),
                sha2_sign_verify_smc_resp, sizeof(sha2_sign_verify_smc_resp));
    }
    else
    {
        ret = sip_svc_send(fcs_handle, FCS_ECDSA_SHA2_SIGN_VERIFY_FINALIZE,
                fcs_sha2_sign_verify_args, sizeof(fcs_sha2_sign_verify_args),
                sha2_sign_verify_smc_resp, sizeof(sha2_sign_verify_smc_resp));
    }
    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP SVC response: x1: %lx, x2: %lx",
                    sha2_sign_verify_smc_resp[0], sha2_sign_verify_smc_resp[1]);
            if ((sha2_sign_verify_smc_resp[FCS_RESP_STATUS] == 0UL) && (final ==
                    FCS_FINALIZE))
            {
                cache_flush((void *)fcs_out_ops_buffer,
                        (size_t)sha2_sign_verify_smc_resp[FCS_RESP_SIZE]);
                /* Ignoring the FCS response header */
                *dest_size =
                        (uint32_t)sha2_sign_verify_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)dest_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA], *dest_size);
            }
            status = (uint16_t)sha2_sign_verify_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_ecdsa_sha2_data_sign_verify(char *uuid,
        uint32_t context_id, uint32_t key_id, uint32_t ecc_algo,
        char *src_data, uint32_t src_size, char *signed_data,
        uint32_t sig_size, char *pub_key_data, uint32_t pub_key_size,
        char *dest_data, uint32_t *dest_size)
{
    uint32_t session_id = 0U,
            remaining_data = src_size + sig_size + pub_key_size, data_written;
    int ret;
    sdm_client_handle_t fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((uuid == NULL) || (src_data == NULL) || (signed_data == NULL) ||
            (dest_data == NULL))
    {
        return -EINVAL;
    }

    if ((key_id == 0U) && (pub_key_data == NULL))
    {
        ERROR("Invalid Key ID");
        return -EINVAL;
    }
    if ((((uint64_t)(uintptr_t)src_data % 8UL) != 0UL) || (dest_data == NULL) ||
            (dest_size == NULL))
    {
        ERROR("Invalid address");
        return -EINVAL;
    }
    if ((src_size < 8U) || ((src_size % 8U) != 0U))
    {
        ERROR("Invalid Size");
        return -EINVAL;
    }

    ret = run_fcs_ecdsa_sha2_data_sign_verify_init(uuid, context_id, key_id,
            ecc_algo);
    if (ret != 0)
    {
        ERROR("Failed to initialise sha_data_sign_verify");
        return ret;
    }
    while (remaining_data > 0U)
    {
        if (remaining_data > FCS_CRYPTO_BLOCK_SIZE)
        {
            /*
             * We shall send the 8bytes of input data, signature and the public key
             * in a finalize operation. Now we continue to send the input data
             * till enough data is available for a finalize operation
             */
            if ((remaining_data - FCS_CRYPTO_BLOCK_SIZE) >= (8U + sig_size +
                    pub_key_size))
            {
                data_written = FCS_CRYPTO_BLOCK_SIZE;
            }
            else
            {
                data_written = remaining_data - (8U + sig_size + pub_key_size);
            }
            ret = run_fcs_ecdsa_sha2_data_sign_verify_update(uuid, context_id,
                    src_data, data_written, signed_data, 0U, pub_key_data, 0U,
                    dest_data, dest_size, FCS_UPDATE);
        }
        else
        {
            data_written = remaining_data;
            src_size = data_written - (sig_size + pub_key_size);
            ret = run_fcs_ecdsa_sha2_data_sign_verify_update(uuid, context_id,
                    src_data, src_size, signed_data, sig_size, pub_key_data,
                    pub_key_size, dest_data, dest_size, FCS_FINALIZE);
        }
        if (ret == 0)
        {
            remaining_data -= data_written;
            src_data += data_written;
        }
        else
        {
            return ret;
        }
    }
    return ret;
}

int run_fcs_ecdsa_get_public_key(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t ecc_algo, char *pub_key_data,
        uint32_t *pub_key_size)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_get_pubkey_args[6], get_pubkey_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((ecc_algo != FCS_ECC_NISTP_256) && (ecc_algo != FCS_ECC_NISTP_384) &&
            (ecc_algo != FCS_ECC_BRAINPOOL_256) && (ecc_algo !=
            FCS_ECC_BRAINPOOL_384))
    {
        ERROR("Invalid ECC Algorithm");
        return -EINVAL;
    }

    fcs_get_pubkey_args[0] = session_id;
    fcs_get_pubkey_args[1] = context_id;
    fcs_get_pubkey_args[2] = key_id;
    fcs_get_pubkey_args[3] = FCS_ECDSA_PARAM_SIZE;
    fcs_get_pubkey_args[4] = ecc_algo;
    DEBUG("Get public key init: Key ID: %d, Ecc Algo: %d", key_id, ecc_algo);
    ret = sip_svc_send(fcs_handle, FCS_GET_PUBKEY_INIT, fcs_get_pubkey_args,
            sizeof(fcs_get_pubkey_args), NULL, 0);

    if (ret != 0)
    {
        return ret;
    }

    cache_force_invalidate(fcs_out_ops_buffer, FCS_GET_PUBKEY_RESP);

    fcs_get_pubkey_args[0] = session_id;
    fcs_get_pubkey_args[1] = context_id;
    fcs_get_pubkey_args[2] = (uint64_t)fcs_out_ops_buffer;
    fcs_get_pubkey_args[3] = FCS_GET_PUBKEY_RESP;
    ret = sip_svc_send(fcs_handle, FCS_GET_PUBKEY_FINALIZE, fcs_get_pubkey_args,
            sizeof(fcs_get_pubkey_args), get_pubkey_smc_resp,
            sizeof(get_pubkey_smc_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx, x2 %lx",
                    get_pubkey_smc_resp[FCS_RESP_STATUS],
                    get_pubkey_smc_resp[FCS_RESP_SIZE]);
            if (get_pubkey_smc_resp[FCS_RESP_STATUS] == 0UL)
            {
                /* Ignoring the FCS response header */
                cache_force_invalidate(fcs_out_ops_buffer,
                        (size_t)get_pubkey_smc_resp[FCS_RESP_SIZE]);
                *pub_key_size = (uint32_t)get_pubkey_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)pub_key_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA],
                        *pub_key_size);
            }
            status = (uint16_t)get_pubkey_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}

int run_fcs_ecdh_request(char *uuid, uint32_t key_id,
        uint32_t context_id, uint32_t ecc_algo, char *pub_key_data,
        uint32_t pub_key_size, char *shared_sec_data,
        uint32_t *shared_sec_size)
{
    int ret;
    uint16_t status;
    sdm_client_handle_t fcs_handle;
    uint64_t fcs_ecdh_args[6], ecdh_smc_resp[2] =
    {
        0
    };
    uint32_t session_id = 0U;
    fcs_handle = get_client_handle(uuid, &session_id);
    if (fcs_handle == NULL)
    {
        ERROR("Failed to locate client");
        return -EIO;
    }
    if ((ecc_algo != FCS_ECC_NISTP_256) && (ecc_algo != FCS_ECC_NISTP_384) &&
            (ecc_algo != FCS_ECC_BRAINPOOL_256) && (ecc_algo !=
            FCS_ECC_BRAINPOOL_384))
    {
        ERROR("Invalid ECC Algorithm");
        return -EINVAL;
    }
    if ((pub_key_data == NULL) || (pub_key_size == 0U))
    {
        ERROR("Invalid pubkey details");
        return -EINVAL;
    }
    fcs_ecdh_args[0] = session_id;
    fcs_ecdh_args[1] = context_id;
    fcs_ecdh_args[2] = key_id;
    fcs_ecdh_args[3] = FCS_ECDSA_PARAM_SIZE;
    fcs_ecdh_args[4] = ecc_algo;
    DEBUG("ECDH init: Key ID: %d, Ecc Algo: %d", key_id, ecc_algo);
    ret = sip_svc_send(fcs_handle, FCS_ECDH_INIT, fcs_ecdh_args,
            sizeof(fcs_ecdh_args), NULL, 0);

    if (ret != 0)
    {
        ERROR("ECDH_init failed");
        return ret;
    }

    cache_force_invalidate(fcs_out_ops_buffer, FCS_ECDH_MAX_RESP);
    cache_force_write_back(pub_key_data, pub_key_size);

    *shared_sec_size = FCS_ECDH_MAX_RESP;
    fcs_ecdh_args[0] = session_id;
    fcs_ecdh_args[1] = context_id;
    fcs_ecdh_args[2] = (uint64_t)pub_key_data;
    fcs_ecdh_args[3] = pub_key_size;
    fcs_ecdh_args[4] = (uint64_t)fcs_out_ops_buffer;
    fcs_ecdh_args[5] = FCS_ECDH_MAX_RESP;

    DEBUG("ECDH finalize: pub_key_addr: %x, pub_key_size: %u", pub_key_data,
            pub_key_size);
    ret = sip_svc_send(fcs_handle, FCS_ECDH_FINALIZE, fcs_ecdh_args,
            sizeof(fcs_ecdh_args), ecdh_smc_resp, sizeof(ecdh_smc_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx, x2 %lx",
                    ecdh_smc_resp[FCS_RESP_STATUS],
                    ecdh_smc_resp[FCS_RESP_SIZE]);
            if (ecdh_smc_resp[FCS_RESP_STATUS] == 0UL)
            {
                cache_force_invalidate((void *)fcs_out_ops_buffer,
                        (size_t)ecdh_smc_resp[FCS_RESP_SIZE]);
                /* Ignoring the FCS response header */
                *shared_sec_size = (uint32_t)ecdh_smc_resp[FCS_RESP_SIZE] -
                        FCS_RESP_HEADER_SIZE;
                (void)memcpy((void *)shared_sec_data,
                        (void *)&fcs_out_ops_buffer[FCS_RESP_DATA],
                        *shared_sec_size);
            }
            status = (uint16_t)ecdh_smc_resp[FCS_RESP_STATUS];
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_qspi_open(void)
{
    int ret;
    uint16_t status;
    uint64_t qspi_open_err = 0UL;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    DEBUG("QSPI open: No args");
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_QSPI_OPEN, NULL, 0,
            &qspi_open_err, sizeof(qspi_open_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx", qspi_open_err);
            status = (uint16_t)qspi_open_err;
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_qspi_close(void)
{
    int ret;
    uint16_t status;
    uint64_t qspi_close_err = 0UL;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    DEBUG("QSPI close: No args");
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_QSPI_CLOSE, NULL, 0,
            &qspi_close_err, sizeof(qspi_close_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx", qspi_close_err);
            status = (uint16_t)(qspi_close_err);
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_qspi_set_cs(uint32_t chip_sel_info)
{
    int ret;
    uint16_t status;
    uint64_t qspi_chip_sel_args[3], qspi_chip_sel_err = 0UL;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    qspi_chip_sel_args[0] = GET_QSPI_CHIP_SEL((uint64_t)chip_sel_info);
    qspi_chip_sel_args[1] = GET_QSPI_CA((uint64_t)chip_sel_info);
    qspi_chip_sel_args[2] = GET_QSPI_MODE((uint64_t)chip_sel_info);
    DEBUG(
            "QSPI chip select: Chip Select: %ld, Combined Address: %ld, Mode: %ld",
            qspi_chip_sel_args[0], qspi_chip_sel_args[1],
            qspi_chip_sel_args[2]);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_QSPI_CHIP_SELECT,
            qspi_chip_sel_args, sizeof(qspi_chip_sel_args), &qspi_chip_sel_err,
            sizeof(qspi_chip_sel_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx", qspi_chip_sel_err);
            status = (uint16_t)qspi_chip_sel_err;
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_qspi_read(uint32_t qspi_addr, uint32_t data_len, char *buffer)
{
    int ret;
    uint16_t status;
    uint64_t qspi_read_args[3], qspi_read_resp[2] =
    {
        0
    };
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    if (data_len > 1024U)
    {
        ERROR("Exceeding maximum size");
        return -EINVAL;
    }
    if (data_len == 0U)
    {
        ERROR("No size specified");
        return -EINVAL;
    }
    if (buffer == NULL)
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }

    qspi_read_args[0] = qspi_addr;
    qspi_read_args[1] = (uint64_t)buffer;
    qspi_read_args[2] = (uint64_t)data_len * MBOX_WORD_SIZE;
    cache_force_invalidate(buffer, data_len * MBOX_WORD_SIZE);
    DEBUG("QSPI read: QSPI Address: %lx, Size: %ld, Buffer: %lx",
            qspi_read_args[0], qspi_read_args[2], (uint64_t)buffer);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_QSPI_READ,
            qspi_read_args, sizeof(qspi_read_args), qspi_read_resp,
            sizeof(qspi_read_resp));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx, x2 %lx", qspi_read_resp[0],
                    qspi_read_resp[1]);
            status = (uint16_t)qspi_read_resp[FCS_RESP_STATUS];
            ret = (int)status;
            if (ret == 0)
            {
                cache_force_invalidate(buffer, qspi_read_resp[FCS_RESP_SIZE]);
                INFO("Read %ld bytes", qspi_read_resp[FCS_RESP_SIZE]);
            }
        }
    }
    return ret;
}
int run_fcs_qspi_write(uint32_t qspi_addr, uint32_t data_len, char *buffer)
{
    int ret;
    uint16_t status;
    uint64_t qspi_write_args[2], qspi_write_err;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("FCS is not initialised");
        return -EIO;
    }
    if (data_len > 1024U)
    {
        ERROR("Exceeding maximum size");
        return -EINVAL;
    }
    if (data_len == 0U)
    {
        ERROR("No size specified");
        return -EINVAL;
    }
    if (buffer == NULL)
    {
        ERROR("No buffer provided");
        return -EINVAL;
    }
    /*
     * Allocate memory to store the address, number of words and the words
     * to be written
     */
    /* Formatting the payload */
    fcs_inp_ops_buffer[0] = qspi_addr;
    fcs_inp_ops_buffer[1] = data_len;
    (void)memcpy((void *)&fcs_inp_ops_buffer[2], (void *)buffer, data_len *
            MBOX_WORD_SIZE);

    qspi_write_args[0] = (uint64_t)fcs_inp_ops_buffer;
    /* Adding two to account for the size of the qspi_addr and data_len */
    qspi_write_args[1] = MBOX_WORD_SIZE * ((uint64_t)data_len + 2UL);
    cache_force_write_back(fcs_inp_ops_buffer, data_len * MBOX_WORD_SIZE);

    INFO("QSPI write: Address: %lx, Size: %ld, Buffer: %lx", qspi_write_args[0],
            qspi_write_args[1], (uint64_t)buffer);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_QSPI_WRITE,
            qspi_write_args, sizeof(qspi_write_args), &qspi_write_err,
            sizeof(qspi_write_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx", qspi_write_err);
            status = (uint16_t)qspi_write_err;
            ret = (int)status;
        }
    }
    return ret;
}
int run_fcs_qspi_erase(uint32_t qspi_addr, uint32_t data_len)
{
    int ret;
    uint16_t status;
    uint64_t qspi_erase_args[2], qspi_erase_err = 0UL;
    if (fcs_descriptor->security_handle == NULL)
    {
        ERROR("Security driver not initialised");
        return -EIO;
    }
    if ((data_len % 1024U) != 0U)
    {
        ERROR("Length in words not a multiple of 1024");
        return -EINVAL;
    }
    if (data_len == 0U)
    {
        ERROR("No size specified");
        return -EINVAL;
    }

    qspi_erase_args[0] = qspi_addr;
    qspi_erase_args[1] = data_len;

    INFO("QSPI erase: Address: %lx, Size: %ld", qspi_erase_args[0],
            qspi_erase_args[1]);
    ret = sip_svc_send(fcs_descriptor->security_handle, FCS_QSPI_ERASE,
            qspi_erase_args, sizeof(qspi_erase_args), &qspi_erase_err,
            sizeof(qspi_erase_err));

    if (ret == 0)
    {
        if (osal_semaphore_wait(fcs_descriptor->fcs_sem,
                OSAL_TIMEOUT_WAIT_FOREVER) == true)
        {
            DEBUG("SIP_SVC response: x1 %lx", qspi_erase_err);
            status = (uint16_t)qspi_erase_err;
            ret = (int)status;
        }
    }
    return ret;
}

void fcs_callback(uint64_t *resp_values)
{
    (void)resp_values;
    (void)osal_semaphore_post(fcs_descriptor->fcs_sem);
}
