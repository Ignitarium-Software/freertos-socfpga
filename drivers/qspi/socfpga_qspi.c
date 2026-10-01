/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Driver implementation for QSPI
 */

#include <stdint.h>
#include <stdio.h>
#include "socfpga_qspi.h"
#include "socfpga_interrupt.h"
#include "socfpga_qspi_reg.h"
#include "socfpga_qspi_ll.h"
#include "socfpga_flash.h"

#define DEFAULT_REMAP_ADDR    0U
static uint32_t prev_bank_addr = 0x00;

/**
 * @brief Enable interrupt
 */
void qspi_enable_int(uint32_t type)
{
    if (type == QSPI_INDDONE)
    {
        qspi_enable_interrupt(QSPI_IND_OPDONE);
    }
    else
    {
        qspi_enable_interrupt(QSPI_IND_OPDONE | QSPI_XFER_LVLBRCH);
    }
}

/**
 * @brief Disable interrupt
 */
void qspi_disable_int(uint32_t mask)
{
    qspi_disable_interrupt(mask);
}

/**
 * @brief Set QSPI callback
 */
int32_t  qspi_set_callback(qspi_descriptor_t *qspi_handle, qspi_callback_t
        callback, void *puser_context)
{
    qspi_handle->xqspi_callback = callback;
    qspi_handle->cb_usercontext = puser_context;
    return QSPI_OK;
}


/**
 * @brief QSPI ISR
 */
#if QSPI_ENABLE_INT_MODE
void qspi_isr(void *pparam){

    uint32_t status;
    qspi_descriptor_t *pqspi_peripheral;
    pqspi_peripheral = (qspi_descriptor_t *)pparam;

    status = qspi_get_int_status();
    qspi_set_int_status(status);

    if (((status & QSPI_IND_OPDONE) != 0U) || ((status & QSPI_XFER_LVLBRCH) !=
            0U))
    {
        if (pqspi_peripheral->is_wr_op != 0U)
        {
            if (((status & QSPI_XFER_LVLBRCH) != 0U) || ((status &
                    QSPI_IND_OPDONE) != 0U))
            {
                /*Write data to SRAM if bytes left*/
                if (pqspi_peripheral->bytes_left > 0U)
                {
                    uint32_t bytes_written = 0U;
                    (void)qspi_indirect_write(
                            pqspi_peripheral->start_addr,
                            pqspi_peripheral->buffer,
                            pqspi_peripheral->bytes_left, &bytes_written);
                    pqspi_peripheral->bytes_left -= bytes_written;
                    pqspi_peripheral->buffer += bytes_written;
                    pqspi_peripheral->start_addr += bytes_written;
                }
                else
                {
                    if ((status & QSPI_IND_OPDONE) != 0U)
                    {
                        qspi_clear_indwr_op_status();
                    }
                    if (pqspi_peripheral->is_async != 0U)
                    {
                        (*pqspi_peripheral->xqspi_callback)(QSPI_OK,
                                pqspi_peripheral->cb_usercontext);
                        qspi_disable_int(QSPI_XFER_LVLBRCH | QSPI_IND_OPDONE);
                        pqspi_peripheral->is_busy = false;
                    }
                    else
                    {
                        if (osal_semaphore_post(pqspi_peripheral->sem) == false)
                        {
                            return;
                        }
                    }
                }
            }
        }
        else
        {
            if (((status & QSPI_XFER_LVLBRCH) != 0U) || ((status &
                    QSPI_IND_OPDONE) != 0U))
            {
                if (pqspi_peripheral->bytes_left > 0U)
                {
                    uint32_t bytes_read = 0U;
                    (void)qspi_read_indirect(
                            pqspi_peripheral->start_addr,
                            pqspi_peripheral->buffer,
                            pqspi_peripheral->bytes_left, &bytes_read);

                    pqspi_peripheral->bytes_left -= bytes_read;
                    pqspi_peripheral->buffer += bytes_read;
                    pqspi_peripheral->start_addr += bytes_read;
                }
                else
                {
                    if ((status & QSPI_IND_OPDONE) != 0U)
                    {
                        qspi_clear_indrd_op_status();
                    }
                    if (pqspi_peripheral->is_async != 0U)
                    {
                        (*pqspi_peripheral->xqspi_callback)(QSPI_OK,
                                pqspi_peripheral->cb_usercontext);
                        qspi_disable_int(QSPI_XFER_LVLBRCH | QSPI_IND_OPDONE);
                        pqspi_peripheral->is_busy = false;
                    }
                    else
                    {
                        if (osal_semaphore_post(pqspi_peripheral->sem) == true)
                        {
                            return;
                        }
                    }
                }
            }
        }
    }
}
#endif

/**
 * @brief Initialize QSPI interface
 */
int32_t qspi_init(qspi_descriptor_t *qspi_handle)
{

    qspi_disable_int(QSPI_ALL_INT_MASK);
#if QSPI_ENABLE_INT_MODE
    socfpga_hpu_interrupt_t int_id;
    int err;
    int_id = SDM_QSPI_INTR;
    err = interrupt_register_isr(int_id, qspi_isr, qspi_handle);
    if (err != 0)
    {
        return QSPI_ERROR;
    }
    err = interrupt_enable(int_id, GIC_INTERRUPT_PRIORITY_QSPI);
    if (err != 0)
    {
        return QSPI_ERROR;
    }
#endif
    uint32_t status;

    if (qspi_is_busy() != 0U)
    {
        return QSPI_BUSY;
    }

    qspi_disable();

    qspi_set_nss_delay(qspi_handle->nss_delay);
    qspi_set_btwn_delay(qspi_handle->btwn_delay);
    qspi_set_after_delay(qspi_handle->after_delay);
    qspi_set_init_delay(qspi_handle->init_delay);
    qspi_set_remap_address(DEFAULT_REMAP_ADDR);
    qspi_set_bytes_per_page(qspi_handle->page_size);
    qspi_enable_fast_read_mode();
    qspi_set_instruction_width(qspi_handle->inst_width);
    qspi_set_addr_width(qspi_handle->addr_width);
    qspi_set_data_width(qspi_handle->data_width);
    qspi_enable_mode_bit(1);
    qspi_set_dummy_delay(qspi_handle->dummy_cycles);
    qspi_cfg_write_mode();
    qspi_set_baud_divisor(qspi_handle->baud_div);
    qspi_enable();

    status = qspi_get_int_status();
    qspi_set_int_status(status);

    return QSPI_OK;
}

/**
 * @brief Enable flash command send
 */
static int32_t qspi_flash_cmd_helper(void)
{
    uint32_t count = 0U;

    qspi_flashcmd_exec();
    while (count < QSPI_TIMEOUT)
    {
        if (qspi_get_flashcmd_stat() == 0U)
        {
            break;
        }
        count++;
    }

    if (count >= QSPI_TIMEOUT)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
}

/**
 * @brief Send flash command
 */
int32_t qspi_send_flashcmd(uint32_t cmd)
{
    uint32_t cs = 0U;
    int32_t ret = QSPI_OK;

    qspi_select_chip(cs);

    qspi_set_flashcmd(cmd);

    ret = qspi_flash_cmd_helper();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return ret;
}

/**
 * @brief Read SFDP parametes from flash
 */
int32_t qspi_read_sfdp(uint32_t address, uint8_t size, uint32_t *data)
{
    int32_t ret = QSPI_OK;

    qspi_set_nss_delay(SFDP_NSS_DELAY);
    qspi_set_btwn_delay(SFDP_BTWN_DELAY);
    qspi_set_after_delay(SFDP_AFTER_DELAY);
    qspi_set_init_delay(SFDP_INIT_DELAY);
    qspi_set_remap_address(DEFAULT_REMAP_ADDR);
    qspi_set_baud_divisor(SFDP_BAUDDIV);
    qspi_enable();

    qspi_enable_fast_read_mode();
    qspi_set_instruction_width(SFDP_INST_WIDTH);
    qspi_set_addr_width(SFDP_ADDR_WIDTH);
    qspi_set_data_width(SFDP_DATA_WIDTH);
    qspi_enable_mode_bit(1);
    qspi_set_dummy_delay(SFDP_DUMMY_DELAY);
    qspi_cfg_write_mode();

    qspi_select_chip(0);

    qspi_set_flashcmd(QSPI_READ_SFDP_CMD);
    qspi_set_flashcmdaddr(address);
    qspi_set_enablecmdaddr();
    qspi_set_flashcmdaddrbytes(3);
    qspi_flashcmd_read_data();
    qspi_flashcmd_read_bytes(size);

    ret = qspi_flash_cmd_helper();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    data[0] = qspi_flash_cmd_rddata0();
    if (size > 4U)
    {
        data[1] = qspi_flash_cmd_rddata1();
    }

    return ret;
}

/**
 * @brief Send flash write command
 */
int32_t qspi_send_flash_writecmd(uint8_t cmd, uint8_t numbytes, uint32_t *data)
{
    int32_t ret = QSPI_OK;
    qspi_select_chip(0);
    qspi_set_flashcmd(cmd);
    qspi_flashcmd_write_data();
    qspi_flashcmd_write_bytes(numbytes);

    qspi_flash_cmd_wrdata0(data[0]);

    if (numbytes > 4U)
    {
        qspi_flash_cmd_wrdata1(data[1]);
    }

    ret = qspi_flash_cmd_helper();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    return ret;
}

/**
 * @brief Send flash read command
 */
int32_t qspi_send_flash_readcmd(uint8_t cmd, uint8_t numbytes, uint32_t *data)
{
    int32_t ret = QSPI_OK;

    qspi_select_chip(0);

    qspi_set_flashcmd(cmd);
    qspi_flashcmd_read_data();
    qspi_flashcmd_read_bytes(numbytes);

    ret = qspi_flash_cmd_helper();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    data[0] = qspi_flash_cmd_rddata0();

    if (numbytes > 4U)
    {
        data[1] = qspi_flash_cmd_rddata1();
    }

    return ret;
}

/**
 * @brief Wait for flash erase or program
 */
int32_t qspi_wait_for_eraseand_program(void)
{
    int32_t ret;
    uint32_t status = 0, count = 0;

    /*Read the status of the  flash device*/
    while (count < QSPI_TIMEOUT)
    {
        ret = qspi_send_flash_readcmd(QSPI_READ_STATUS_CMD, 1, &status);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }
        if ((status & QSPI_READ_STATUS_POS) == 0U)
        {
            break;
        }
        count++;
    }

    if (count >= QSPI_TIMEOUT)
    {
        return QSPI_ERROR;
    }
    /*Read the flag status register*/
    count = 0;
    status = 0;
    while (count < QSPI_TIMEOUT)
    {
        ret = qspi_send_flash_readcmd(QSPI_READ_FLAG_STATUS_CMD, 1, &status);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }
        if ((status & QSPI_READ_FLAG_STATUS_POS) != 0U)
        {
            break;
        }
        count++;
    }
    if (count >= QSPI_TIMEOUT)
    {
        return QSPI_ERROR;
    }

    /*Clear the flag*/
    if ((status & QSPI_READ_FLAG_STATUS_POS) != 0U)
    {
        ret = qspi_send_flashcmd(QSPI_CLEAR_FLAG_STATUS_CMD);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }
    }
    return QSPI_OK;
}

/**
 * @brief Send command for QSPI sector erase
 */
int32_t qspi_erase(uint32_t address)
{

    int32_t ret = QSPI_OK;

    ret = qspi_send_flashcmd(QSPI_WRITE_ENABLE_CMD);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    /*Send the command to set the bank*/
    uint32_t bank_addr = (address >> QSPI_BANK_ADDR_POS);
    ret = qspi_send_flash_writecmd(QSPI_EXT_REG_CMD, 1, &bank_addr);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    ret = qspi_send_flashcmd(QSPI_WRITE_DISABLE_CMD);
    if (ret != QSPI_OK)
    {

        return QSPI_ERROR;
    }

    /*Start of sequence for 4k erase*/
    ret = qspi_send_flashcmd(QSPI_WRITE_ENABLE_CMD);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    qspi_select_chip(0);
    qspi_set_flashcmd(QSPI_SECTOR_ERASE_CMD);
    qspi_set_enablecmdaddr();
    qspi_set_flashcmdaddrbytes(3);
    qspi_set_flashcmdaddr(address);

    ret = qspi_flash_cmd_helper();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    ret = qspi_wait_for_eraseand_program();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    ret = qspi_send_flashcmd(QSPI_WRITE_DISABLE_CMD);

    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
}

/**
 * @brief Wait for QSPI erase and program
 */
int32_t qspi_write_finish(void)
{
    uint32_t count = 0U, status = 0U;

    while (count < QSPI_TIMEOUT)
    {
        status = qspi_get_indwr_multiop_status();
        if ((status & QSPI_INDONE_OPSTATUS_MASK) != 0U)
        {
            qspi_clear_indwr_op_status();
            break;
        }
        count++;
    }
    if (count >= QSPI_TIMEOUT)
    {
        return QSPI_ERROR;
    }

    int32_t ret = qspi_wait_for_eraseand_program();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    return QSPI_OK;
}

/**
 * @brief Select QPSI flash bank
 */
int32_t qspi_select_bank(uint32_t *bank_addr)
{
    int32_t ret = qspi_send_flashcmd(QSPI_WRITE_ENABLE_CMD);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    ret = qspi_send_flash_writecmd(QSPI_EXT_REG_CMD, 1, bank_addr);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    ret = qspi_send_flashcmd(QSPI_WRITE_DISABLE_CMD);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
}

/**
 * @brief Write indirectly to the QSPI flash page
 */
int32_t qspi_page_write_indirect(uint32_t offset, uint8_t *data, uint32_t size,
        uint32_t *count)
{
    uint32_t sram_partition = ((qspi_reg_get_data((uint32_t)QSPI_SRAMPART)) &
            QSPI_SRAM_RD_CAP_MASK);
    uint32_t w_capacity = (uint32_t)QSPI_TOTAL_SRAM_SIZE - sram_partition;
    volatile uint32_t *dest_addr = (uint32_t *)QSPI_DATA_BASE;
    volatile uint8_t *dest_addr_byte = (uint8_t *)QSPI_DATA_BASE;
    uint32_t w_fill_lvl, space;
    uint8_t *w_data_byte = NULL;
    uint32_t w_count = 0, *w_data = NULL;
    if (dest_addr == NULL)
    {
        return QSPI_ERROR;
    }

    qspi_set_indwrstaddr(offset);
    qspi_set_indwrcnt((uint32_t)size);
    qspi_start_indwr();

    while (w_count < size)
    {

        w_fill_lvl = ((qspi_get_sramfill() >> QSPI_SRAMFILL_WR_STATUS_POS) &
                QSPI_SRAMFILL_WR_STATUS_MASK);

        if ((w_capacity - w_fill_lvl) < 4U)
        {
            break;
        }
        space = ((w_capacity - w_fill_lvl) < (((uint32_t)size -
                (uint32_t)w_count) / (uint32_t)sizeof(uint32_t)))
                ? (w_capacity - w_fill_lvl)
                : (((uint32_t)size - (uint32_t)w_count) /
                (uint32_t)sizeof(uint32_t));

        w_data = (uint32_t *)(data + w_count);
        if (w_data == NULL)
        {
            return QSPI_ERROR;
        }

        for (uint32_t i = 0; i < space; ++i)
        {
            *dest_addr = *w_data;
            w_data++;
        }
        w_count += (space * sizeof(uint32_t));
        if ((size - w_count) < 4U)
        {
            w_data_byte = (uint8_t *)w_data;
            if (w_data_byte == NULL)
            {
                return QSPI_ERROR;
            }
            while (w_count != size)
            {
                *dest_addr_byte = *w_data_byte;
                w_data_byte++;
                w_count++;
            }
        }
    }
#if QSPI_ENABLE_INT_MODE
    *count = w_count;
    return QSPI_OK;
#else
    int32_t ret = 0;
    (void)count;
    ret = qspi_write_finish();
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    if (w_count != size)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
#endif
}

/**
 * @brief Write in indirect mode to QSPI flash bank
 */
int32_t qspi_bank_write_indirect(uint32_t bank_offset, uint8_t *data, uint32_t
        size, uint32_t *w_count)
{
    uint32_t page_offset = bank_offset & (QSPI_PAGE_SIZE - 1U);
    uint32_t w_size = size < (QSPI_PAGE_SIZE - page_offset)
            ? size : (QSPI_PAGE_SIZE - page_offset);
#if QSPI_ENABLE_INT_MODE
    uint32_t int_status = 0U;
    int32_t ret = QSPI_OK;

    int_status = qspi_get_int_status();
    qspi_set_int_status(int_status);

    /*set the water mark level*/
    if (size > QSPI_WRITE_WATER_LVL)
    {
        qspi_set_indwrwater(QSPI_WRITE_WATER_LVL);
    }
    else
    {
        qspi_set_indwrwater(size);
    }

    ret = qspi_page_write_indirect(bank_offset, data, w_size, w_count);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
#else
    int32_t status = 0;
    while (size > 0)
    {
        status = qspi_page_write_indirect(bank_offset, data, w_size, w_count);
        if (status != QSPI_OK)
        {
            break;
        }
        bank_offset += w_size;
        data += w_size;
        size -= w_size;
        w_size = size < QSPI_PAGE_SIZE ? size: QSPI_PAGE_SIZE;

    }
    return status;
#endif
}

/**
 * @brief Write indirectly to QSPI flash
 */
int32_t qspi_indirect_write(uint32_t address, uint8_t *data, uint32_t size,
        uint32_t *w_count)
{
    uint32_t bank_addr, bank_write_len, bank_offset;
    int32_t ret = 0;
    uint8_t *w_data = data;

    bank_addr = ((address & QSPI_BANK_ADDR_OFFSET) >> QSPI_BANK_ADDR_POS);
    bank_offset = address & (QSPI_BANK_SIZE - 1U);
    bank_write_len = size < (QSPI_BANK_SIZE - bank_offset)
            ? size : (QSPI_BANK_SIZE - bank_offset);

#if QSPI_ENABLE_INT_MODE
    ret = qspi_select_bank(&bank_addr);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    ret = qspi_bank_write_indirect(bank_offset, w_data, bank_write_len,
            w_count);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;

#else
    uint32_t bank_count = (((address + size - 1) >> QSPI_BANK_ADDR_POS) -
            ((address) >> QSPI_BANK_ADDR_POS)) + 1;

    for (uint32_t i = 0; i < bank_count; i++)
    {
        ret = qspi_select_bank(&bank_addr);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }

        ret = qspi_bank_write_indirect(bank_offset, w_data, bank_write_len,
                w_count);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }

        bank_addr += QSPI_BANK_SIZE;
        w_data += bank_write_len;
        size -= bank_write_len;
        bank_offset = 0;

        bank_write_len = size < (QSPI_BANK_SIZE) ? size: (QSPI_BANK_SIZE);
    }
    return QSPI_OK;
#endif
}

/**
 * @brief Read data in indirect mode from QSPI flash
 */
#if QSPI_ENABLE_INT_MODE
int32_t qspi_read_data(uint32_t size, uint8_t *data, uint32_t *count)
{
    uint32_t r_count = 0;
    uint32_t *r_data = (uint32_t *)data;
    uint32_t r_fill_lvl;
    volatile uint32_t *src_word = (uint32_t *)QSPI_DATA_BASE;
    volatile uint8_t *src_byte = (uint8_t *)QSPI_DATA_BASE;
    uint8_t *r_data_byte = (uint8_t *)data;
    uint32_t r_bytes;

    if (src_word == NULL)
    {
        return QSPI_ERROR;
    }
    if (size >= sizeof(uint32_t))
    {
        r_fill_lvl = ((qspi_get_sramfill() >> QSPI_SRAMFILL_RD_STATUS_POS) &
                QSPI_SRAMFILL_RD_STATUS_MASK);
        if (r_fill_lvl > 512U)
        {
            r_fill_lvl = 512U;
        }
        r_bytes = r_fill_lvl < (size / (sizeof(uint32_t))) ? r_fill_lvl: (size /
                (sizeof(uint32_t)));
        for (uint32_t i = 0; i < r_bytes; i++)
        {
            *r_data = *src_word;
            r_data++;
        }
        r_count = (r_bytes * (sizeof(uint32_t)));
        r_data_byte = (uint8_t *)r_data;
        if (r_data_byte == NULL)
        {
            return QSPI_ERROR;
        }
        size -= r_count;
    }
    if (size < 4U)
    {
        r_fill_lvl = ((qspi_get_sramfill() >> QSPI_SRAMFILL_RD_STATUS_POS) &
                QSPI_SRAMFILL_RD_STATUS_MASK);
        if (r_fill_lvl > 512U)
        {
            r_fill_lvl = 512U;
        }
        r_bytes = (r_fill_lvl * (sizeof(uint32_t))) < size ? (r_fill_lvl *
                (sizeof(uint32_t))): size;
        for (uint32_t i = 0; i < r_bytes; i++)
        {
            *r_data_byte = *src_byte;
            r_data_byte++;
        }
        r_count += r_bytes;
    }
    *count = r_count;
    return QSPI_OK;
}

/**
 * @brief Read data indirectly form QSPI flash(used in ISR)
 */
int32_t qspi_read_indirect(uint32_t address, uint8_t *data, uint32_t size,
        uint32_t *r_count)
{
    uint32_t bank_addr, bank_offset;
    uint32_t int_status;
    uint32_t sram_partition;
    int32_t ret = QSPI_OK;

    bank_addr = ((address & QSPI_BANK_ADDR_OFFSET) >> QSPI_BANK_ADDR_POS);

    /*New bank means new transfer*/
    if (prev_bank_addr != bank_addr)
    {
        size = size < QSPI_BANK_SIZE ? size: QSPI_BANK_SIZE;
        bank_offset = (address & (QSPI_BANK_SIZE - 1U));
        ret = qspi_select_bank(&bank_addr);
        if (ret != QSPI_OK)
        {
            return 0;
        }
        qspi_set_indrdstaddr(bank_offset);
        qspi_set_indrdcnt(size);

        int_status = qspi_get_int_status();
        qspi_set_int_status(int_status);
        sram_partition = ((qspi_reg_get_data((uint32_t)QSPI_SRAMPART)) &
                QSPI_SRAM_RD_CAP_MASK);

        if ((size / 4U) > sram_partition)
        {
            qspi_set_indrdwater((sram_partition * 4U));
        }
        else
        {
            qspi_set_indrdwater(size);
        }
        qspi_start_indrd();
        prev_bank_addr = bank_addr;
    }
    ret = qspi_read_data(size, data, r_count);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return ret;
}
#endif

/**
 * @brief Read data indirectly form QSPI flash(to be not used in ISR)
 */
int32_t qspi_bank_read_indirect(uint32_t bank_offset, uint8_t *data, uint32_t
        size, uint32_t *rd_count)
{

    qspi_set_indrdstaddr(bank_offset);
    qspi_set_indrdcnt(size);
#if QSPI_ENABLE_INT_MODE
    uint32_t int_status = qspi_get_int_status();
    int32_t ret = QSPI_OK;
    qspi_set_int_status(int_status);

    uint32_t sram_partition = ((qspi_reg_get_data(
            (uint32_t)QSPI_SRAMPART)) & QSPI_SRAM_RD_CAP_MASK);

    if ((size / 4U) > sram_partition)
    {
        qspi_set_indrdwater((sram_partition * 4U));
    }
    else
    {
        qspi_set_indrdwater(size);
    }

#endif
    qspi_start_indrd();
#if QSPI_ENABLE_INT_MODE
    ret = qspi_read_data(size, data, rd_count);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
#else
    (void)rd_count;
    uint32_t numr_byte = size % 4;
    uint32_t numr_word = size / 4;
    uint32_t r_count = 0;
    uint32_t *r_data = (uint32_t *)data;
    volatile uint32_t *src_word = (uint32_t *)QSPI_DATA_BASE;
    if (src_word == NULL)
    {
        return QSPI_ERROR;
    }
    uint32_t r_fill_lvl;
    while (r_count < size)
    {
        do
        {
            r_fill_lvl = ((qspi_get_sramfill() >> QSPI_SRAMFILL_RD_STATUS_POS) &
                    QSPI_SRAMFILL_RD_STATUS_MASK);
            r_data = (uint32_t *)(data + r_count);

            for (uint32_t i = 0; i < r_fill_lvl; ++i)
            {
                if (numr_word > 0)
                {
                    *r_data++ = *src_word;
                    numr_word--;
                }
                else
                {
                    uint32_t temp = *src_word;
                    memcpy(r_data, &temp, numr_byte);
                }
            }
            r_count += r_fill_lvl * sizeof(uint32_t);
        } while(r_fill_lvl > 0);
    }
    if (r_count != size)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
#endif
}

/**
 * @brief Read directly from QSPI flash
 */
int32_t qspi_indirect_read(uint32_t address, uint8_t *buffer, uint32_t size,
        uint32_t *r_count)
{
    uint32_t bank_read_len, bank_addr, bank_offset;
    int32_t ret = QSPI_OK;

    bank_addr = ((address & QSPI_BANK_ADDR_OFFSET) >> QSPI_BANK_ADDR_POS);
    prev_bank_addr = bank_addr;
    bank_offset = (address & (QSPI_BANK_SIZE - 1U));
    bank_read_len = size < (QSPI_BANK_SIZE - bank_offset)
            ? size : (QSPI_BANK_SIZE - bank_offset);
#if QSPI_ENABLE_INT_MODE
    ret = qspi_select_bank(&bank_addr);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }

    ret = qspi_bank_read_indirect(bank_offset, buffer, bank_read_len, r_count);
    if (ret != QSPI_OK)
    {
        return QSPI_ERROR;
    }
    return QSPI_OK;
#else

    uint32_t bank_count = (((address + size - 1) >> QSPI_BANK_ADDR_POS) -
            ((address) >> QSPI_BANK_ADDR_POS)) + 1;

    for (uint32_t i = 0; i < bank_count; i++)
    {
        ret = qspi_select_bank(&bank_addr);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }

        ret = qspi_bank_read_indirect(bank_offset, buffer, bank_read_len,
                r_count);
        if (ret != QSPI_OK)
        {
            return QSPI_ERROR;
        }

        bank_addr += QSPI_BANK_SIZE;
        buffer += bank_read_len;
        size -= bank_read_len;
        bank_offset = 0;

        bank_read_len = size < QSPI_BANK_SIZE ? size: QSPI_BANK_SIZE;
    }
    return QSPI_OK;
#endif
}

/**
 * @brief Deinitialize QSPI interface
 */
int32_t qspi_deinit(void)
{
#if QSPI_ENABLE_INT_MODE
    if (qspi_is_busy() != 0U)
    {
        return QSPI_BUSY;
    }

    qspi_disable_interrupt(QSPI_ALL_INT_MASK);
#endif

    qspi_disable();
    if (qspi_is_busy() != 0U)
    {
        return QSPI_BUSY;
    }
    return QSPI_OK;
}
