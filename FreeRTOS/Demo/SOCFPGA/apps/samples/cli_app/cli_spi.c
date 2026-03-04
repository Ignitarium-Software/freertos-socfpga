/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Implementation of CLI commands for SPI EEPROM
 */


/**
 * @defgroup cli_spi SPI
 * @ingroup cli
 *
 * Perform operations on EEPROM connected to SPI bus
 *
 * @details
 * It supports the following commands:
 * - spi read &lt;instance&gt; &lt;slave_select&gt; &lt;register_address&gt; &lt;data_size&gt;
 * - spi write &lt;instance&gt; &lt;slave_select&gt; &lt;register_address&gt; &lt;data&gt;...
 * - spi help
 *
 * Typical usage:
 * - Use 'spi write' command to write data to the SPI EEPROM.
 * - Use 'spi read' command to read data from the SPI EEPROM.
 *
 * @section spi_commands Commands
 * @subsection spi_read spi read
 * Read from SPI EEPROM memory <br>
 *
 * Usage: <br>
 * spi read &lt;instance&gt; &lt;slave_select&gt; &lt;register_address&gt; &lt;data_size&gt; <br>
 *
 * It requires the following arguments:
 * - instance - The instance of the SPI driver. Valid values are 0 and 1.
 * - slave_select - The number assigned to the slave device. Valid range is from 1 to 4.
 * - register_address - The address of the memory location. It is a hexadecimal value from 0x00 to 0x3FFF.
 * - data_size - Number of bytes to read.
 * @subsection spi_write spi write
 * Write to SPI EEPROM memory
 *
 * Usage: <br>
 * spi write &lt;instance&gt; &lt;slave_select&gt; &lt;register_address&gt; &lt;data&gt;... <br>
 *
 * It requires the following arguments:
 * - instance - The instance of the SPI driver. Valid values are 0 and 1.
 * - slave_select - The number assigned to the slave device. Valid range is from 1 to 4.
 * - register_address - The address of the memory location. It is a hexadecimal value from 0x00 to 0x3FFF.
 * - data - Data written to the EEPROM memory. User can provide multiple values.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include "semphr.h"
#include "FreeRTOS_CLI.h"
#include "cli_app.h"
#include "cli_utils.h"
#include "socfpga_spi.h"

#define EEPROM_READ         0x03
#define EEPROM_WRITE        0x02
#define EEPROM_WR_ENABLE    0x06
#define TRANSFER_SIZE       1
#define SPI_FREQ            500000U

BaseType_t cmd_spi( char *write_buffer, size_t write_buffer_len,
        const char *command_string );
int32_t eeprom_read( uint8_t *buf, size_t size, uint16_t mem_add );
int32_t eeprom_write( uint8_t *buf, size_t size, uint16_t mem_add );
int32_t eeprom_enable_write();

spi_handle_t spi_hdl;

/**
 * @func : cmd_spi
 * @brief : spi command line execution
 */
BaseType_t cmd_spi( char *write_buffer, size_t write_buffer_len,
        const char *command_string )
{
    (void)write_buffer;
    (void)write_buffer_len;
    const char *parameter1;
    const char *parameter2;
    const char *parameter3;
    const char *parameter4;
    const char *parameter5;
    char temp_string[ 6 ] = { 0 };
    char help_str[5] = {0};
    uint8_t rbuf[259];
    const char *read_cmd = "read";
    const char *write_cmd = "write";
    const char *help_cmd = "help";
    uint32_t spi_inst;
    uint32_t slave_sel;
    uint32_t reg_addr;
    uint8_t wbuf[256];
    uint32_t data_size;
    uint8_t size;
    uint8_t idx;
    int32_t ret;
    uint32_t i;
    spi_cfg_t config;
    BaseType_t parameter1_str_len;
    BaseType_t parameter2_str_len;
    BaseType_t parameter3_str_len;
    BaseType_t parameter4_str_len;
    BaseType_t parameter5_str_len;

    parameter1 = FreeRTOS_CLIGetParameter(command_string, 1,
            &parameter1_str_len);
    strncpy(temp_string, parameter1, parameter1_str_len);
    if (strcmp(temp_string, read_cmd) == 0)
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        parameter3 = FreeRTOS_CLIGetParameter(command_string, 3,
                &parameter3_str_len);
        parameter4 = FreeRTOS_CLIGetParameter(command_string, 4,
                &parameter4_str_len);
        parameter5 = FreeRTOS_CLIGetParameter(command_string, 5,
                &parameter5_str_len);

        strncpy(help_str, parameter2, parameter2_str_len);
        if (strcmp(help_str, help_cmd) == 0)
        {
            printf("\rRead from SPI EEPROM memory"
                    "\r\n\nUsage:"
                    "\r\n  spi read <instance> <slave_select> <register_address> <data_size>"
                    "\r\n\nIt requires the following arguments:"
                    "\r\n  instance            The instance of the SPI driver. Valid values are 0 and 1."
                    "\r\n  slave_select        The number assigned to the slave device. Valid range is from 1 to 4."
                    "\r\n  register_address    The address of the memory location. It is a hexadecimal value from 0x00 to x3fff."
                    "\r\n  data_size           Number of bytes to read.");
            return pdFALSE;
        }
        else if ((parameter2 == NULL) || (parameter3 == NULL) ||
                (parameter4 == NULL) || (parameter5 == NULL))
        {
            printf("\r\nERROR: Invalid arguments.");
            printf("\r\nEnter 'spi read help' for more information");
            return pdFAIL;
        }

        ret = cli_get_decimal("spi read","instance", parameter2, 0, 1,
                &spi_inst);
        if (ret  != 0)
        {
            return pdFAIL;
        }
        ret = cli_get_decimal("spi read","slave_select", parameter3, 1, 4,
                &slave_sel);
        if (ret  != 0)
        {
            return pdFAIL;
        }
        spi_hdl = spi_open(spi_inst);
        if (spi_hdl == NULL)
        {
            printf("\r\n SPI instance not initialized.");
            return pdFAIL;
        }
        config.mode = SPI_MODE3;
        config.clk = SPI_FREQ;
        ret = spi_ioctl(spi_hdl, SPI_SET_CONFIG, &config);
        if (ret != 0)
        {
            printf("Failed to configure SPI \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = spi_select_slave(spi_hdl, slave_sel);
        if (ret != 0)
        {
            printf("Failed to select slave \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = cli_get_hex("spi read","register_address", parameter4, 0x0,
                0x3FFF, &reg_addr);
        if (ret != 0)
        {
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = cli_get_decimal("spi read","data_size", parameter5, 0x1, 0x3FFF,
                &data_size);
        if (ret != 0)
        {
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = eeprom_read(rbuf, (size_t)data_size, (uint16_t)reg_addr);
        if (ret != 0)
        {
            printf("Failed to read \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = spi_close(spi_hdl);
        if (ret != 0)
        {
            printf("Failed to close SPI\n\r");
            return pdFAIL;
        }
        printf("Data in 0x%x is \n\r", reg_addr);
        /* valid data in memory starts from 3rd index */
        for (i=0; i < data_size; i++)
        {
            printf("0x%x ", rbuf[i + 3]);
        }
    }
    else if (strcmp(temp_string, write_cmd) == 0)
    {
        parameter2 = FreeRTOS_CLIGetParameter(command_string, 2,
                &parameter2_str_len);
        parameter3 = FreeRTOS_CLIGetParameter(command_string, 3,
                &parameter3_str_len);
        parameter4 = FreeRTOS_CLIGetParameter(command_string, 4,
                &parameter4_str_len);

        strncpy(help_str, parameter2, parameter2_str_len);
        if (strcmp(help_str, help_cmd) == 0)
        {
            printf("\rWrite to SPI EEPROM memory"
                    "\r\n\nUsage:"
                    "\r\n  spi write <instance> <slave_select> <register_address> <data>..."
                    "\r\n\nIt requires the following arguments:"
                    "\r\n  instance            The instance of the SPI driver. Valid values are 0 and 1."
                    "\r\n  slave_select        The number assigned to the slave device. Valid range is from 1 to 4."
                    "\r\n  register_address    The address of the memory location. It is a hexadecimal value from 0x00 to x3fff."
                    "\r\n  data                Data written to the EEPROM memory in hex . User can provide multiple values.");
            return pdFALSE;
        }
        else if ((parameter2 == NULL) || (parameter3 == NULL) ||
                (parameter4 == NULL))
        {
            printf("\r\nERROR: Invalid arguments");
            printf("\r\nEnter 'help' to view a list of available commands.");
            return pdFAIL;
        }

        ret = cli_get_decimal("spi write","instance", parameter2, 0, 1,
                &spi_inst);
        if (ret  != 0)
        {
            return pdFAIL;
        }
        ret = cli_get_decimal("spi write","slave_select", parameter3, 1, 4,
                &slave_sel);
        if (ret  != 0)
        {
            return pdFAIL;
        }
        spi_hdl = spi_open(spi_inst);
        if (spi_hdl == NULL)
        {
            printf("\r\n SPI init failed !!!");
            return pdFAIL;
        }
        config.mode = SPI_MODE3;
        config.clk = SPI_FREQ;
        ret = spi_ioctl(spi_hdl, SPI_SET_CONFIG, &config);
        if (ret != 0)
        {
            printf("Failed to configure SPI \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = spi_select_slave(spi_hdl, slave_sel);
        if (ret != 0)
        {
            printf("Failed to select slave \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = cli_get_hex("spi write","register_address", parameter4, 0x0,
                0x3FFF, &reg_addr);
        if (ret != 0)
        {
            spi_close(spi_hdl);
            return pdFAIL;
        }
        size = 0;
        idx = 5;
        while (pdTRUE)
        {
            parameter5 = FreeRTOS_CLIGetParameter(command_string, idx,
                    &parameter5_str_len);
            if (parameter5 == NULL)
            {
                break;
            }
            uint32_t write_val;
            ret = cli_get_hex("spi write","data", parameter5, 0, 255,
                    &write_val);
            if (ret != 0)
            {
                spi_close(spi_hdl);
                return pdFAIL;
            }
            wbuf[size] = write_val;
            size++;
            idx++;
        }
        if (size == 0)
        {
            printf("\r\nERROR: Invalid arguments");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = eeprom_enable_write();
        if (ret != 0)
        {
            printf("Failed to enable write \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = eeprom_write((uint8_t *)&wbuf, size, (uint16_t)reg_addr);
        if (ret != 0)
        {
            printf("Failed to write \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        /*Add delay for eeprom to execute internal write cycle*/
        vTaskDelay(10);
        ret = eeprom_read(rbuf, size, reg_addr);
        if (ret != 0)
        {
            printf("Failed to readback from EEPROM \n\r");
            spi_close(spi_hdl);
            return pdFAIL;
        }
        ret = spi_close(spi_hdl);
        if (ret != 0)
        {
            printf("Failed to close SPI \n\r");
            return pdFAIL;
        }
        if (strncmp((char*)wbuf, (char*)&rbuf[3], size) == 0)
        {
            printf("Write success \n\r");
        }
        else
        {
            printf("Write failed \n\r");
            return pdFAIL;
        }
    }
    else if (strcmp(temp_string, help_cmd) == 0)
    {
        printf("\rPerform operations on SPI bus"
                "\r\n\nIt supports the following commands:"
                "\r\n  spi read <instance> <slave_select> <register_address> <data_size>"
                "\r\n  spi write <instance> <slave_select> <register_address> <data>..."
                "\r\n  spi help"
                "\r\n\nTypical usage:"
                "\r\n- Use 'spi write' command to write data to the EEPROM."
                "\r\n- Use 'spi read' command to read data from the EEPROM."
                "\r\n\nFor command specific help, try:"
                "\r\n  spi <command> help");
        return pdFALSE;
    }
    else
    {
        printf("\r\nERROR: Invalid arguments"
                " \r\nEnter 'spi help' for more information.");
        return pdFAIL;
    }
    return pdFALSE;
}

int32_t eeprom_enable_write()
{
    uint8_t cmd = EEPROM_WR_ENABLE;
    int32_t ret;

    ret = spi_transfer_sync(spi_hdl, &cmd, NULL, 1);
    if (ret != 0)
    {
        return ret;
    }
    return 0;
}

int32_t eeprom_write( uint8_t *buf, size_t size, uint16_t mem_add )
{
    uint8_t cmd[ 150 ] = { 0 };
    uint8_t rx_count = 0, i = 0;
    int32_t ret;
    cmd[ 0 ] = EEPROM_WRITE;
    cmd[ 1 ] = (mem_add >> 8) & 0xFF;
    cmd[ 2 ] = mem_add & 0xFF;

    for (i = 0; i < size; i++)
    {
        cmd[ i + 3 ] = buf[ i ];
    }

    rx_count = size + 3;
    ret = spi_transfer_sync(spi_hdl, cmd, NULL,  rx_count);
    if (ret != 0)
    {
        return ret;
    }
    return 0;
}

int32_t eeprom_read( uint8_t *buf, size_t size, uint16_t mem_add )
{
    uint32_t i = 0;
    uint8_t cmd[ 20 ] = { 0 };
    int32_t ret;
    cmd[ 0 ] = EEPROM_READ;
    cmd[ 1 ] = (mem_add >> 8) & 0xFF;
    cmd[ 2 ] = mem_add & 0xFF;

    for (i = 0; i < size; i++)
    {
        cmd[i + 3] = i + 0x0F;
    }

    ret = spi_transfer_sync(spi_hdl, cmd, buf, (size + 3));
    if (ret != 0)
    {
        return ret;
    }
    return 0;
}
