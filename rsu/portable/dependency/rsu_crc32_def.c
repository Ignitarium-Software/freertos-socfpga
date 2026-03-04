/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * CRC32 implementation for RSU
 */

#include <arm_acle.h>
#include "RSU_crc32_def.h"

unsigned long int calculate_crc32(unsigned long int ulCrc, void *vData,
        unsigned long ulDataSize)
{
    char *buf = (char *)vData;
    unsigned int val;
    const unsigned long int  *word;
    unsigned long int val0;
    unsigned long int i, num;

    if (vData == NULL)
    {
        return 0;
    }

    ulCrc = (~ulCrc) & 0xffffffff;

    word = (const unsigned long int *)buf;
    num = ulDataSize >> 3;
    ulDataSize &= 7;

    for (i = 0; i < num; i++)
    {
        val0 = word[i];
        ulCrc = __crc32d(ulCrc, val0);
    }
    word += num;

    buf = (char *)word;
    while (ulDataSize)
    {
        ulDataSize--;
        val = *buf++;
        ulCrc = __crc32b(ulCrc, val);
    }
    return ulCrc ^ 0xffffffff;
}

