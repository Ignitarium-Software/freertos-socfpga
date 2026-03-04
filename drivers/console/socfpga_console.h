/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for console driver
 */


#ifndef __SOCFPGA_CONSOLE_H__
#define __SOCFPGA_CONSOLE_H__

#include <errno.h>

/**
 * @brief Initialize the console for system prints.
 *
 * @param id UART ID to use.
 * @param config_str UART configuration string.
 *                   Format: <baudrate>-<bit len><parity><number of stop bit>
 *                   Example: 115200-8N1
 *
 * @return 0 on success, -errno on failure.
 */
int console_init(int id, const char *config_str);

/**
 * @brief Write data to the console.
 *
 * @param buffer Pointer to the data to write.
 * @param length Number of bytes to write.
 *
 * @return Number of bytes written on success, -errno on failure.
 */
int console_write(unsigned char *const buffer, int length);

/**
 * @brief Read data from the console.
 *
 * @param buffer Pointer to the receive buffer.
 * @param length Maximum number of bytes to read.
 *
 * @return Number of bytes read on success, -errno on failure.
 */
int console_read(unsigned char *const buffer, int length);

/**
 * @brief Fill the console output buffer with provided data.
 *
 * The data remains in the buffer until console_signal_fill() is called to
 * signal it for write. Use this if the writes needs to be defered and dosent
 * invoke a context switch
 *
 * @param buffer Pointer to the data to copy into the read buffer.
 * @param length Number of bytes to copy.
 *
 * @return Number of bytes copied on success, -errno on failure.
 */
int console_fill_buffer(unsigned char *const buffer, int length);

/**
 * @brief Signal that the console buffer has been filled.
 *
 * This functon does nothing if configCONSOLE_MAKE_FLUSH_TASK if false.
 * In that case the console flush is the responsibility of the user.
 */
void console_signal_fill(void);

/**
 * @brief Clear any pending console events or state.
 */
void console_clear_pending(void);

/**
 * @brief Deinitialize the console.
 *
 * @return 0 on success, -errno on failure.
 */
int console_deinit();

#endif /* __SOCFPGA_CONSOLE_H__ */
