/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for FCS driver
 */

#ifndef _SOCFPGA_FCS_H_
#define _SOCFPGA_FCS_H_

#include <stdint.h>

/**
 * @file socfpga_fcs.h
 * @brief Header file for FCS HAL driver
 */

/**
 * @defgroup fcs FCS
 * @ingroup drivers
 * @brief APIs for the SoC FPGA FCS driver.
 * @details
 * The FCS driver is implemented as a HAL for libfcs. This driver is not to be
 * used by the applicaion but rather is used by the libfcs library. It provide
 * user with a high-level API for cryptographic operations. The libfcs uses a
 * wrapper layer to interact with the FCS driver, which in turn communicates
 * with SIP_SVC subsystem and provides the response to the corresponding
 * cryptographic operation.
 * To see example usage, see @ref fcs_sample "FCS Sample Application".
 * @{
 */

/**
 * @defgroup fcs_fns Functions
 * @ingroup fcs
 * FCS HAL APIs
 */

/**
 * @defgroup fcs_structs Structures
 * @ingroup fcs
 * FCS Specific Structures
 */

/**
 * @defgroup fcs_macros Macros
 * @ingroup fcs
 * FCS Specific Macros
 */

/**
 * @addtogroup fcs_macros
 * @{
 */

 #define FCS_RNG_MAX_SIZE              4084U    /*!< Maximum size for RNG */
 #define FCS_UUID_SIZE                 16U       /*!< Size of UUID */
 #define FCS_MAX_KEY_SIZE              240U     /*!< Maximum size for keys */
 #define FCS_SDOS_MIN_INPUT            32U      /*!< Minimum SDOS input size */
 #define FCS_SDOS_MAX_INPUT            32672U   /*!< Maximum SDOS input size */
 #define FCS_SDOS_ENC_MAX_RESP         32760U   /*!< Maximum SDOS encryption response size */
 #define FCS_SDOS_DEC_MAX_RESP         32712U   /*!< Maximum SDOS decryption response size */
 #define FCS_SDOS_MAGIC_NUMBER         0xACBDBDEDU   /*!< SDOS magic number identifier */
 #define FCS_SDOS_HEADER_PADDING       0x01020304U   /*!< SDOS header padding value */
 #define FCS_ATTEST_CERT_MAX_SIZE      4096U    /*!< Maximum size for attestation certificate */
 #define FCS_MCTP_MAX_SIZE             4096U    /*!< Maximum MCTP message size */
 #define FCS_PROV_DATA_MAX_SIZE        1024U    /*!< Maximum provisioning data size */
 #define FCS_DEV_IDENTITY_RESP_SIZE    32       /*!< Size of device identity response */
 #define FCS_ECDH_MAX_RESP             120U     /*!< Maximum ECDH response size */
 #define FCS_SDOS_ENCRYPT_MODE         0x1U     /*!< SDOS encryption mode identifier */
 #define FCS_SDOS_DECRYPT_MODE         0x0U     /*!< SDOS decryption mode identifier */
 #define FCS_DIGEST_OPMODE_SHA2        0x1U     /*!< Digest operation mode: SHA2 */
 #define FCS_DIGEST_OPMODE_HMAC        0x2U     /*!< Digest operation mode: HMAC */
 #define FCS_DIGEST_SIZE_256           0x0U     /*!< Digest size: 256 bits */
 #define FCS_DIGEST_SIZE_384           0x1U     /*!< Digest size: 384 bits */
 #define FCS_DIGEST_SIZE_512           0x2U     /*!< Digest size: 512 bits */
 #define FCS_AES_ENCRYPT_MODE          0x0U     /*!< AES encryption mode identifier */
 #define FCS_AES_DECRYPT_MODE          0x1U     /*!< AES decryption mode identifier */
 #define FCS_AES_ECB                   0x0U     /*!< AES mode: ECB */
 #define FCS_AES_CBC                   0x1U     /*!< AES mode: CBC */
 #define FCS_AES_CTR                   0x2U     /*!< AES mode: CTR */
 #define FCS_AES_GCM                   0x3U     /*!< AES mode: GCM */
 #define FCS_AES_GCM_GHASH             0x4U     /*!< AES GCM with GHASH */
 #define FCS_AES_TAG_32                0x0U     /*!< AES authentication tag size: 32 bits */
 #define FCS_AES_TAG_64                0x1U     /*!< AES authentication tag size: 64 bits */
 #define FCS_AES_TAG_96                0x2U     /*!< AES authentication tag size: 96 bits */
 #define FCS_AES_TAG_128               0x3U     /*!< AES authentication tag size: 128 bits */
 #define FCS_GCM_TAG_SIZE              16U      /*!< GCM tag size in bytes */
 #define FCS_IV_EXTERNAL               0x0U     /*!< Use external IV */
 #define FCS_IV_INTERNAL_DRNG          0x1U     /*!< Use internal IV from DRNG */
 #define FCS_IV_INTERNAL_BASE          0x2U     /*!< Use internal base IV */
 #define FCS_ECC_NISTP_256             0x1U     /*!< ECC curve: NIST P-256 */
 #define FCS_ECC_NISTP_384             0x2U     /*!< ECC curve: NIST P-384 */
 #define FCS_ECC_BRAINPOOL_256         0x3U     /*!< ECC curve: Brainpool P-256 */
 #define FCS_ECC_BRAINPOOL_384         0x4U     /*!< ECC curve: Brainpool P-384 */
 #define FCS_EXTRACT_AND_EXPAND        0x0      /*!< HKDF mode: Extract and expand */
 #define FCS_EXPAND                    0x1      /*!< HKDF mode: Expand only */
 #define FCS_EXPAND_GCM                0x2      /*!< HKDF mode: Expand with GCM context */
 #define FCS_HKDF_256                  0x0      /*!< HKDF with SHA-256 */
 #define FCS_HKDF_384                  0x1      /*!< HKDF with SHA-384 */
 #define FCS_HKDF_512                  0x2      /*!< HKDF with SHA-512 */
 #define FCS_SHARED_SEC_SIZE           48U      /*!< Size of shared secret */
 #define FCS_MAX_SIG_SIZE              96U      /*!< Maximum signature size */
 #define FCS_MAX_PUBKEY_SIZE           96U      /*!< Maximum public key size */


/**
 * @}
 */

/**
 * @addtogroup fcs_structs
 * @{
 *
 */
struct fcs_sdos_crypt_header
{
    uint32_t magic_number;
    uint32_t data_len;
    uint8_t pad;
    uint8_t srk_indx;
    uint16_t app_spec_obj_info;
    uint8_t owner_id[8];
    uint32_t hdr_pad;
    uint8_t iv_field[16];
};
/**
 * @}
 */

/**
 * @addtogroup fcs_fns
 * @{
 */

/**
 * @brief   Initialise the mailbox and other resources required by the FCS driver.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int fcs_init(void);

/**
 * @brief   de-initialise the mailbox and reset resources used by FCS
 *
 * @return
 * - 0: on success
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int fcs_deinit(void);

/**
 * @brief   Open a cryptographic session and associate it with a
 *          mailbox client and a UUID
 *
 * @param[out] uuid Pointer to store session UUID
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_open_service_session(char *uuid);

/**
 * @brief   Close the cryptographic session and its mailbox client
 *
 * @param[in] uuid Session UUID
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_close_service_session(char *uuid);

/**
 * @brief Generate a random number using the FCS service.
 *
 * This function requests the FCS service to generate a random number of the specified size.
 * The random number is written to the provided buffer.
 *
 * @param[out] rand_buf  Pointer to the buffer where the random number will be stored.
 *                       The buffer must be large enough to hold `rand_size` bytes.
 * @param[in] uuid       The UUID associated with the FCS service session.
 * @param[in] context_id The context ID for the random number generation operation.
 * @param[in] rand_size  The size of the random number to generate, in bytes.
 *                       should not exceed FCS_RNG_MAX_SIZE.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_random_number_ext(char *rand_buf, char *uuid, uint32_t context_id,
        uint32_t rand_size);


/**
 * @brief Import a service key into the FCS service.
 *
 * This function reads a key from the specified file and imports it into the FCS service.
 *
 * @param[in]  uuid  The session ID associated with the FCS service.
 * @param[in]  key         Buffer containing key data.
 * @param[in]  key_size    Size of key data.
 * @param[out] status      Buffer containing the status of key import.
 * @param[out] status_size Size of response data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_import_service_key(char *uuid, char *key, uint32_t key_size,
        char *status, unsigned int *status_size);

/**
 * @brief Export a service key from the FCS service.
 *
 * This function retrieves a key associated with the specified key ID and writes it to the provided buffer.
 *
 * @param[in]  uuid The session ID associated with the FCS service.
 * @param[in]  key_id     The ID of the key to be exported.
 * @param[out] key_dest   Pointer to the buffer where the exported key will be stored.
 * @param[out] key_size   Pointer to an integer to store the length of the exported key.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_export_service_key(char *uuid, uint32_t key_id, char *key_dest,
        unsigned int *key_size);

/**
 * @brief Remove a service key from the FCS service.
 *
 * This function deletes a key associated with the specified key ID from the FCS service.
 *
 * @param[in] uuid The session ID associated with the FCS service.
 * @param[in] key_id     The ID of the key to be removed.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */

int run_fcs_remove_service_key(char *uuid, uint32_t key_id);

/**
 * @brief Retrieve provisioning data from the FCS service.
 *
 * This function fetches provisioning data and writes it to the provided buffer.
 *
 * @param[in]  uuid          The session ID associated with the FCS service.
 * @param[in]  key_id        The ID of the key for which to retrieve provisioning data.
 * @param[out] key_info      Pointer to the buffer where the provisioning data will be stored.
 * @param[out] key_info_size Pointer to an integer to store the size of the provisioning data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_get_service_key_info(char *uuid, uint32_t key_id, char *key_info,
        unsigned int *key_info_size);


/**
 * @brief Create a service key.
 *
 * This function reads a key from the specified file and imports it into the FCS service.
 *
 * @param[in]  uuid The session ID associated with the FCS service.
 * @param[in]  key        Buffer containing key data.
 * @param[in]  key_size   Size of key data.
 * @param[out] status     Buffer containing the status of key import.
 * @param[out] status_size  Size of response data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_create_service_key(char *uuid, char *key, uint32_t key_size,
        char *status, unsigned int *status_size);


/**
 * @brief Retrieve provisioning data from the FCS service.
 *
 * This function fetches provisioning data and writes it to the provided buffer.
 *
 * @param[out] prov_data      Pointer to the buffer where the provisioning data will be stored.
 * @param[out] prov_data_size Pointer to an integer to store the size of the provisioning data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_service_get_provision_data(char *prov_data,
        uint32_t *prov_data_size);


/**
 * @brief Send a signed certificate to the device.
 *
 * This function sends a certificate to the SDM.
 *
 * @param[in]  cert_data Pointer to the buffer containing the certificate details.
 * @param[in]  cert_size Length of the certificate buffer.
 * @param[out] status    Pointer to the buffer where the certificate status will be stored.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_send_certificate(char *cert_data, uint32_t cert_size,
        uint32_t *status);

/**
 * @brief Set one selected counter to specified value without signed certificate
 *
 * This function sets a preauthorized service counter with the specified type and value.
 *
 * @param[in] type  The type of the counter.
 * @param[in] value The value to set for the counter.
 * @param[in] test  Test flag for the operation.

 * * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_service_counter_set_preauthorized(uint8_t type, uint32_t value,
        uint32_t test);

/**
 * @brief Compute a digest using the FCS service.
 *
 * This function computes a digest for the provided source data using the specified parameters.
 *
 * @param[in]  uuid  The session ID associated with the FCS service.
 * @param[in]  context_id  The context ID for the digest operation.
 * @param[in]  key_id      The ID of the key to be used for the operation.
 * @param[in]  op_mode The SHA operation mode (e.g., SHA-256, SHA-384).
 * @param[in]  dig_size    The size of the digest to compute.
 * @param[in]  src_data    Pointer to the source data.
 * @param[in]  src_size    Length of the source data.
 * @param[out] digest_data      Pointer to the buffer where the computed digest will be stored.
 * @param[out] digest_size  Pointer to an integer to store the length of the computed digest.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_get_digest(char *uuid, uint32_t context_id, uint32_t key_id,
        uint32_t op_mode, uint32_t dig_size, char *src_data, uint32_t src_size,
        char *digest_data, uint32_t *digest_size);

/**
 * @brief Verify a MAC using the FCS service.
 *
 * This function verifies a MAC for the provided source data using the specified parameters.
 *
 * @param[in]  uuid     The session ID associated with the FCS service.
 * @param[in]  context_id     The context ID for the MAC verification operation.
 * @param[in]  key_id         The ID of the key to be used for the operation.
 * @param[in]  dig_size       The size of the digest to verify.
 * @param[in]  src_data       Pointer to the source data.
 * @param[in]  src_size       Length of the source data.
 * @param[in]  dest_data      Pointer to the MAC data to verify.
 * @param[out] dest_size      Pointer to an integer to store the size of the verified MAC.
 * @param[in]  user_data_size Size of the user data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_mac_verify(char *uuid, uint32_t context_id, uint32_t key_id,
        uint32_t dig_size, char *src_data, uint32_t src_size, char *dest_data,
        uint32_t *dest_size, uint32_t user_data_size);


/**
 * @brief Encrypt data using the FCS service.
 *
 * This function encrypts the provided source data using the specified operation mode and writes the encrypted data to the response buffer.
 *
 * @param[in]  uuid    The session ID associated with the FCS service.
 * @param[in]  context_id    The context ID for the encryption operation.
 * @param[in]  src_data      Pointer to the source data to be encrypted.
 * @param[in]  src_size       Length of the source data.
 * @param[out] resp_data     Pointer to the buffer where the encrypted data will be stored.
 * @param[out] resp_size Pointer to an integer to store the length of the encrypted data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_sdos_encrypt(char *uuid, uint32_t context_id, char *src_data,
        uint32_t src_size, char *resp_data, uint32_t *resp_size);

/**
 * @brief Decrypt data using the FCS service.
 *
 * This function decrypts the provided source data using the specified operation mode and writes the decrypted data to the response buffer.
 *
 * @param[in]  uuid    The session ID associated with the FCS service.
 * @param[in]  context_id    The context ID for the decryption operation.
 * @param[in]  src_data      Pointer to the source data to be decrypted.
 * @param[in]  src_size       Length of the source data.
 * @param[out] resp_data     Pointer to the buffer where the decrypted data will be stored.
 * @param[out] resp_size Pointer to an integer to store the length of the decrypted data.
 * @param[in]  owner_flag  Owner flag for the decryption operation.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_sdos_decrypt(char *uuid, uint32_t context_id, char *src_data,
        uint32_t src_size, char *resp_data, uint32_t *resp_size, uint64_t
        owner_flag);

/**
 * @brief Request HKDF operation.
 *
 * This function requests the HKDF operation for the specified session UUID,
 * key ID, step type, MAC mode, IKM, and IKM length.
 *
 * @param[in] uuid      Pointer to the session UUID.
 * @param[in] key_id          Key ID
 * @param[in] step_type       Extract and expand or just expand
 * @param[in] mac_mode        SHA-256, SHA-384, SHA-512
 * @param[in] input_buffer    Buffer containg input data
 * @param[in] output_key_size Length of passed key object
 * @param[out] hkdf_status    Status of the HKDF request
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_hkdf_request(char *uuid, uint32_t key_id, uint32_t step_type,
        uint32_t mac_mode, char *input_buffer, uint32_t output_key_size,
        uint32_t *hkdf_status);

/**
 * @brief Retrieve the chip ID using the FCS service.
 *
 * This function fetches the chip ID and writes it to the provided low and high parts.
 *
 * @param[out] chip_low  Pointer to an integer to store the lower part of the chip ID.
 * @param[out] chip_high Pointer to an integer to store the higher part of the chip ID.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_get_chip_id(uint32_t *chip_low, uint32_t *chip_high);

/**
 * @brief Request to get the certificate.
 *
 * This function requests the certificate for the specified certificate request.
 *
 * @param[in]   cert_req The desired certificate request.
 * @param[out]  cert_data         Pointer to the certificate.
 * @param[out]  cert_size    Pointer to the certificate size.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_attestation_get_certificate(int cert_req, char *cert_data,
        uint32_t *cert_size);

/**
 * @brief Reloads the attestation certificate.
 *
 * This function reloads the attestation certificate for the specified
 * certificate request.
 *
 * @param[in] cert_req The desired certificate request.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_attestation_certificate_reload(int cert_req);

/**
 * @brief Sends an MCTP command using the FCS service.
 *
 * This function sends an MCTP command with the provided source data and retrieves the response data.
 *
 * @param[in]  src_data  Pointer to the source data for the MCTP command.
 * @param[in]  src_size   Length of the source data.
 * @param[out] resp_data Pointer to the buffer where the response data will be stored.
 * @param[out] resp_size  Pointer to an integer to store the length of the response data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_mctp_cmd_send(char *src_data, uint32_t src_size, char *resp_data,
        uint32_t *resp_size);

/**
 * @brief Retrieves the JTAG ID code using the FCS service.
 *
 * This function fetches the JTAG ID code and writes it to the provided pointer.
 *
 * @param[out] jtag_id_code Pointer to an integer to store the JTAG ID code.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_get_jtag_idcode(uint32_t *jtag_id_code);

/**
 * @brief Retrieves the device identity using the FCS service.
 *
 * This function fetches the device identity and writes it to the provided buffer.
 *
 * @param[out] dev_identity Pointer to the buffer where the device identity will be stored.
 * @param[out] dev_id_size   Pointer to an integer to store the length of the device identity.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_get_device_identity(char *dev_identity, uint32_t *dev_id_size);

/**
 * @brief Perform AES encryption or decryption using the FCS service.
 *
 * This function performs AES encryption or decryption on the provided input data using the specified parameters.
 *
 * @param[in]  uuid        The session ID associated with the FCS service.
 * @param[in]  key_id      The ID of the key to be used for the operation.
 * @param[in]  context_id  The context ID for the AES operation.
 * @param[in]  crypt_mode  The cryptographic mode (e.g., encrypt or decrypt).
 * @param[in]  block_mode  The block mode for AES (e.g., ECB, CBC, GCM).
 * @param[in]  iv_src      The source of the initialization vector (IV).
 * @param[in]  iv_data     Pointer to the IV data.
 * @param[in]  tag_size    Length of the authentication tag (for GCM mode).
 * @param[in]  aad_size    Length of the additional authenticated data (AAD).
 * @param[in]  aad_data    Pointer to the AAD data.
 * @param[in]  tag_data    Pointer to the buffer where the authentication tag will be stored.
 * @param[in]  input_data  Pointer to the input data to be encrypted or decrypted.
 * @param[in]  input_size  Length of the input data.
 * @param[out] output_data Pointer to the buffer where the output data will be stored.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_aes_cryption(char *uuid, uint32_t key_id, uint32_t context_id,
        uint32_t crypt_mode, uint32_t block_mode, uint32_t iv_src,
        char *iv_data, uint32_t tag_size, uint32_t aad_size, char *aad_data,
        char *tag_data, char *input_data, uint32_t input_size,
        char *output_data);

/**
 * @brief Sign a hash using ECDSA with the FCS service.
 *
 * This function generates an ECDSA signature for the provided hash data using the specified parameters.
 *
 * @param[in]  uuid             The session ID associated with the FCS service.
 * @param[in]  context_id       The context ID for the ECDSA operation.
 * @param[in]  key_id           The ID of the key to be used for signing.
 * @param[in]  ecc_algo         The ECC algorithm to use (e.g., NISTP-256, NISTP-384).
 * @param[in]  hash_data        Pointer to the hash data to be signed.
 * @param[in]  hash_data_size   Length of the hash data.
 * @param[out] signed_data      Pointer to the buffer where the signed data will be stored.
 * @param[out] signed_data_size Pointer to an integer to store the size of the signed data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_ecdsa_hash_sign(
    char *uuid, uint32_t context_id, uint32_t key_id, uint32_t ecc_algo,
    char *hash_data, uint32_t hash_data_size, char *signed_data,
    uint32_t *signed_data_size);


/**
 * @brief Verify an ECDSA signature using the FCS service.
 *
 * This function verifies an ECDSA signature for the provided hash data using the specified parameters.
 *
 * @param[in]  uuid           The session ID associated with the FCS service.
 * @param[in]  context_id     The context ID for the ECDSA operation.
 * @param[in]  key_id         The ID of the key to be used for verification.
 * @param[in]  ecc_algo       The ECC algorithm to use (e.g., NISTP-256, NISTP-384).
 * @param[in]  hash_data      Pointer to the hash data to verify.
 * @param[in]  hash_data_size Length of the hash data.
 * @param[in]  sig_data       Pointer to the signature data to verify.
 * @param[in]  sig_size       Length of the signature data.
 * @param[in]  pub_key_data   Pointer to the public key data.
 * @param[in]  pub_key_size   Length of the public key data.
 * @param[out] dest_data      Pointer to the buffer where the verification result will be stored.
 * @param[out] dest_size      Pointer to an integer to store the size of the verification result.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_ecdsa_hash_verify(char *uuid, uint32_t context_id, uint32_t key_id,
        uint32_t ecc_algo, char *hash_data, uint32_t hash_data_size,
        char *sig_data, uint32_t sig_size, char *pub_key_data, uint32_t
        pub_key_size, char *dest_data, uint32_t *dest_size);

/**
 * @brief Sign data using ECDSA with SHA-2 hashing.
 *
 * This function generates an ECDSA signature for the provided data using SHA-2 hashing.
 *
 * @param[in]  uuid       The session ID associated with the FCS service.
 * @param[in]  context_id The context ID for the ECDSA operation.
 * @param[in]  key_id     The ID of the key to be used for signing.
 * @param[in]  ecc_algo   The ECC algorithm to use (e.g., NISTP-256, NISTP-384).
 * @param[in]  src_data   Pointer to the source data to be signed.
 * @param[in]  src_size   Length of the source data.
 * @param[out] dest_data  Pointer to the buffer where the signed data will be stored.
 * @param[out] dest_size  Pointer to an integer to store the size of the signed data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_ecdsa_sha2_data_sign(char *uuid, uint32_t context_id, uint32_t
        key_id, uint32_t ecc_algo, char *src_data, uint32_t src_size,
        char *dest_data, uint32_t *dest_size);

/**
 * @brief Verify ECDSA signatures for data using SHA-2 hashing.
 *
 * This function verifies ECDSA signatures for the provided data using SHA-2 hashing.
 *
 * @param[in]  uuid         The session ID associated with the FCS service.
 * @param[in]  context_id   The context ID for the ECDSA operation.
 * @param[in]  key_id       The ID of the key to be used for verification.
 * @param[in]  ecc_algo     The ECC algorithm to use (e.g., NISTP-256, NISTP-384).
 * @param[in]  src_data     Pointer to the source data to verify.
 * @param[in]  src_size     Length of the source data.
 * @param[in]  signed_data  Pointer to the signed data.
 * @param[in]  sig_size     Length of the signed data.
 * @param[in]  pub_key_data Pointer to the public key data.
 * @param[in]  pub_key_size Length of the public key data.
 * @param[out] dest_data    Pointer to the buffer where the verification result will be stored.
 * @param[out] dest_size    Pointer to an integer to store the size of the verification result.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_ecdsa_sha2_data_sign_verify(char *uuid, uint32_t context_id,
        uint32_t key_id, uint32_t ecc_algo, char *src_data, uint32_t src_size,
        char *signed_data, uint32_t sig_size, char *pub_key_data, uint32_t
        pub_key_size, char *dest_data, uint32_t *dest_size);

/**
 * @brief Retrieve the public key for an ECC key using the FCS service.
 *
 * This function retrieves the public key for the specified ECC key.
 *
 * @param[in]  uuid         The session ID associated with the FCS service.
 * @param[in]  context_id   The context ID for the operation.
 * @param[in]  key_id       The ID of the ECC key.
 * @param[in]  ecc_algo     The ECC algorithm to use (e.g., NISTP-256, NISTP-384).
 * @param[out] pub_key_data Pointer to the buffer where the public key will be stored.
 * @param[out] pub_key_size Pointer to an integer to store the length of the public key.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_ecdsa_get_public_key(char *uuid, uint32_t context_id, uint32_t
        key_id, uint32_t ecc_algo, char *pub_key_data, uint32_t *pub_key_size);

/**
 * @brief Perform an ECDH key exchange using the FCS service.
 *
 * This function performs an ECDH key exchange and retrieves the shared secret.
 *
 * @param[in]  uuid            The session ID associated with the FCS service.
 * @param[in]  key_id          The ID of the ECC key to use for the key exchange.
 * @param[in]  context_id      The context ID for the operation.
 * @param[in]  ecc_algo        The ECC algorithm to use (e.g., NISTP-256, NISTP-384).
 * @param[in]  pub_key_data    Pointer to the public key data of the other party.
 * @param[in]  pub_key_size    Length of the public key data.
 * @param[out] shared_sec_data Pointer to the buffer where the shared secret will be stored.
 * @param[out] shared_sec_size Pointer to an integer to store the length of the shared secret.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_ecdh_request(char *uuid, uint32_t key_id, uint32_t context_id,
        uint32_t ecc_algo, char *pub_key_data, uint32_t pub_key_size,
        char *shared_sec_data, uint32_t *shared_sec_size);

/**
 * @brief Request exclusive access to the QSPI interface
 *
 * This function requests exclusive access to the QSPI interface.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_qspi_open(void);

/**
 * @brief Close the exculsive access to the QSPI interface
 *
 * This function closes the QSPI interface.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_qspi_close(void);

/**
 * @brief configure the chip select lines for the QSPI interface
 *
 * This function selects the chip select lines
 *
 * @param[in] chip_sel_info The chip select value.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO: If the FCS service is not initialized or other internal errors occur.
 */
int run_fcs_qspi_set_cs(uint32_t chip_sel_info);

/**
 * @brief Reads data from the QSPI interface
 *
 * This function reads data from QSPI
 *
 * @param[in]  qspi_addr The QSPI address.
 * @param[in]  data_len  The length of the data to read.
 * @param[out] buffer    The buffer to store the data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_qspi_read(uint32_t qspi_addr, uint32_t data_len, char *buffer);

/**
 * @brief Writes data to the QSPI interface
 *
 * This function writes data to QSPI
 *
 * @param[in] qspi_addr The QSPI address.
 * @param[in] data_len  The length of the data to write in words.
 * @param[in] buffer    The buffer to store the data.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_qspi_write(uint32_t qspi_addr, uint32_t data_len, char *buffer);

/**
 * @brief Erases data from the QSPI interface
 *
 * This function erases data from QSPI
 *
 * @param[in] qspi_addr The QSPI address, must be 4KB aligned
 * @param[in] data_len  Bytes to erase, must be multiple of 4K.
 *
 * @return
 * - 0: on success, or an error code on failure:
 * - -EIO:    If the FCS service is not initialized or other internal errors occur.
 * - -EINVAL: If invalid parameters are provided.
 */
int run_fcs_qspi_erase(uint32_t qspi_addr, uint32_t data_len);

/**
 * @}
 */
/* end of group fcs_fns */

/**
 * @}
 */
/* end of group fcs */

#endif /* __SOCFPGA_FCS_H__ */
