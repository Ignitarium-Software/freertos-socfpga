/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for Flash adapter layer
 */

#ifndef __SOCFPGA_FLASH_ADAPTER_H__
#define __SOCFPGA_FLASH_ADAPTER_H__

#include <stdint.h>

#define MAX_PARAM_HEADERS        4U
#define MAX_PARAM_TABLES         4U
#define SFDP_START_ADDR          0x0U
#define SFDP_PARAM_START_ADDR    0x8U
#define SFDP_HEADER_SIZE         8U
#define PARAM_HEADER_SIZE        8U

#define SFDP_HEADER_MSB_POS             32U
#define SFDP_HEADER_MASK                0xffffffffU
#define SFDP_SGN_START_POS              8U
#define SFDP_SGN_NUM_BYTES              4U
#define SFDP_MINREV_POS                 40U
#define SFDP_MINREV_MASK                0xffU
#define SFDP_MAJORREV_POS               48U
#define SFDP_MAJORREV_MASK              0xffU
#define SFDP_NUM_PARAM_TABLES_POS       56U
#define SFDP_NUM_PARAM_TABLES_MASK      0xffU
#define SFDP_PARAM_HEADER_MSB_POS       32U
#define SFDP_PARAM_LEN_POS              32U
#define SFDP_PARAM_LEN_MASK             0xffU
#define SFDP_PARAM_TABLE_OFFSET_POS     40U
#define SFDP_PARAM_TABLE_OFFSET_MASK    0xffffffU


#define SFDP_PARAM_FLASHSIZE_ADDR       0x34U
#define SFDP_PARAM_FLASHSIZE_SIZE       5U
#define SFDP_PARAM_FLASHSIZE_MSB_POS    32U
#define SFDP_PARAM_FLASHSIZE_POS        8U
#define SFDP_PARAM_FLASHSIZE_MASK       0xffffffffU
#define SFDP_PARAM_PAGESIZE_ADDR        0x58U
#define SFDP_PARAM_PAGESIZE_SIZE        5U
#define SFDP_PARAM_PAGESIZE_POS         12U
#define SFDP_PARAM_PAGESIZE_MASK        0x0fU


#define M25Q_INST_WIDTH        0U
#define M25Q_DATA_WIDTH        0U
#define M25Q_ADDR_WIDTH        0U
#define M25Q_BAUDDIV           0xfU
#define M25Q_SECTOR_SIZE       4096U
#define M25Q_CLOCK_FREQ        100000000U
#define M25Q_NSS_DEALY         0x14U
#define M25Q_INIT_DELAY        0xc8U
#define M25Q_BTWN_DELAY        0x14U
#define M25Q_AFTER_DELAY       0xffU
#define M25Q_NUM_ADDR_BYTES    2U
#define M25Q_DUMMY_CYCLES      0U
#define M25Q_QSPI_MODE         4U

/**
 * @brief The SFDP header structure
 */
struct sfdp_header
{
    uint32_t signature[4U];
    uint8_t min_rev;
    uint8_t major_rev;
    uint8_t num_parameter_tables;
    uint8_t reserved[1U];
};


/**
 * @brief The SFDP parameter header structure
 */
struct sfdp_param_header
{
    uint8_t param_id;
    uint8_t min_rev;
    uint8_t major_rev;
    uint8_t parameter_length;
    uint32_t parameter_table_offset;
    uint32_t reserved;
};

/**
 * @brief The SFDP parameter table structure
 */
struct sfdp_param_table
{
    uint8_t min_sector_erase_size;
    uint8_t address_bytes;
    uint8_t page_size;
    uint8_t erase_cmd4_k;
    uint8_t address_mode;
    uint8_t dtr_mode;
    uint8_t flash_density;
    uint8_t read_mode_interface;
    uint8_t mode_bits;
    uint8_t dummy_cycle;
    uint64_t flash_size;

};

/**
 * @brief The Flash Adapter structure
 */
struct sfdp_object
{
    struct sfdp_header std_header;
    struct sfdp_param_header param_header[MAX_PARAM_HEADERS];
    struct sfdp_param_table param_table[MAX_PARAM_TABLES];
};

/**
 * @brief Function pointer to the SFDP parsing logic
 */
typedef int (*parse_sfpd_pf)(void *pqspi, struct sfdp_object *sfdp);

/**
 * @brief The Flash Adapter structure
 */
typedef struct flash_adapter
{
    uint8_t device_id;
    struct sfdp_object sfdp;
    parse_sfpd_pf parse_sfdp;
} flash_adapter_t;

/**
 * @brief Read the SFDP parameters for the Micron M25Q flash chip.
 *
 * @param[in] phandle Void pointer which has to be type casted to flash handle.
 * @param[in,out] sfdp SFDP object to populate.
 *
 * @return
 * - FLASH_OK:    if the operation is successful.
 * - FLASH_ERROR: if the operation failed.
 */
int  parse_m25_q_parameters(void *phandle, struct sfdp_object *sfdp);


#endif
