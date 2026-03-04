/*
 * Common IO - basic V1.0.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Copyright (C) 2025 Altera Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 *
 * Modified for SoC FPGA
 */

#ifndef __SOCFPGA_TIMER_H__
#define __SOCFPGA_TIMER_H__

/**
 * @file socfpga_timer.h
 * @brief Header file for timer HAL driver
 */

#include "socfpga_defines.h"

/**
 * @defgroup timer Timer
 * @ingroup drivers
 * @brief APIs for SoC FPGA timer.
 * @details This is the timer block implementation for SoC FPGA.
 * It provides APIs for reading the timer value, starting and stopping the timer,
 * configuring the timer in free-running or user mode, and setting a callback function
 * to be invoked on timer expiry. For example usage, refer to
 * @ref timer_sample "timer sample application".
 * @{
 */

/**
 * @defgroup timer_fns Functions
 * @ingroup timer
 * Timer HAL APIs
 */

/**
 * @defgroup timer_structs Structures
 * @ingroup timer
 * Timer Specific Structures
 */

/**
 * @defgroup timer_enums Enumerations
 * @ingroup timer
 * Timer Specific Enumerations
 */

/**
 * @defgroup timer_macros Macros
 * @ingroup timer
 * Timer Specific Macros
 */

/**
 * @addtogroup timer_macros
 * @{
 */
#define TIMER_NUM_INSTANCE    4      /*!<Number of instances for timer*/
/**
 * @}
 */

/**
 * @brief The timer descriptor type defined in the source file.
 * @ingroup timer_structs
 */
struct timer_context;

/**
 * @brief Timer handle returned by calling timer_open()
 * @ingroup timer_structs
 */
typedef struct timer_context *timer_handle_t;

/**
 * @brief Function pointer for user callback
 * @ingroup timer_fns
 */
typedef void (*timer_callback_t)(void *buf);

/**
 * @brief enum for timer instances
 * @ingroup timer_enums
 */
typedef enum
{
    TIMER_SYS0 = 0, /*!<Peripheral ID for SYS_TIMER instance 0*/
    TIMER_SYS1, /*!<Peripheral ID for SYS_TIMER instance 1*/
    TIMER_SP0, /*!<Peripheral ID for TIMER instance 0*/
    TIMER_SP1, /*!<Peripheral ID for TIMER instance 1*/
    TIMER_N_INSTANCE /*!<Number of timer instances*/
} timer_instance_t;

/**
 * @addtogroup timer_fns
 * @{
 */

/**
 * @brief Used to initialize the timer
 * Once a instance is opened, it needs to be closed before invoking open again.
 *
 *
 * @param[in] instance The Timer instance to open.
 *
 * @return
 * - Timer handle on success
 * - NULL if
 *     - Invalid timer instance
 *     - If same instance already opened
 *     - Failed to obtain clock
 *     - Failed to enable interrupt
 */
timer_handle_t timer_open(timer_instance_t instance);

/**
 * @brief Read the current counter value register
 *
 * @param[in] htimer       Timer handle returned by open API
 * @param[out] counter_val Counter value at given instance
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if timer handle is NULL
 * - -EPERM:  if timer is not running
 */
int32_t timer_get_value_raw(timer_handle_t const htimer, uint32_t *counter_val);

/**
 * @brief Start the timer instance.
 *
 * @param[in] htimer Timer handle returned by open API
 *
 * @return
 * - 0 :      on success
 * - -EINVAL: if timer handle is NULL
 * - -EBUSY:  if timer is already running
 */
int32_t timer_start(timer_handle_t const htimer);

/**
 * @brief Stop the timer instance.
 *
 * @param[in] htimer Timer handle returned by open API
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if timer handle is NULL
 * - -EPERM:  if timer is not running
 * - -EFAULT: if failed to disable interrupt
 */
int32_t timer_stop(timer_handle_t const htimer);

/**
 * @brief Close the timer instance.
 *
 * @param[in] htimer Timer handle returned by open API
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if timer handle is NULL
 */
int32_t timer_close(timer_handle_t const htimer);

/**
 * @brief Configure the timer instance in free running or in user mode.
 *
 * @param[in] htimer - Timer handle returned by open API
 * @param[in] period - The desired timer period in micro seconds.
 *	If period exceeds the maximum possible period, it is
 *	rounded to the maximum period. If period value is given as
     0xFFFFFFFFF, the timer will be set to free running mode.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if timer handle is NULL or period is too long
 * - -EBUSY:  if timer already running
 */
int32_t timer_set_period_us(timer_handle_t const htimer, uint32_t period);

/**
 * @brief Get remaining time in microseconds.
 *
 * In delay mode (user defined mode) this is the remaining time for next
 * interrupt. In free-running mode, this is the remaining time for next
 * roll over.
 *
 * @param[in] htimer  Timer handle returned by open API
 * @param[out] time_us Remaining time for timer overflow
 * @return
 * - 0        on success
 * - -EINVAL: if timer handle is NULL or time_us is NULL
 * - -EPERM:  if timer is not running
 */
int32_t timer_get_value_us(timer_handle_t const htimer, uint32_t *time_us);

/**
 * @brief Set the callback function
 *
 * Sets the callback function that is invoked on timer expiry. This
 * will be invoked periodically until the timer is stopped.
 *
 * @param[in] htimer   Timer handle returned by open API
 * @param[in] callback Call back function pointer
 * @param[in] param    User defined context variable.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if timer handle is NULL
 */
int32_t timer_set_callback(timer_handle_t const htimer, timer_callback_t
        callback, void *param);

/**
 * @}
 */
/* end of group timer_fns */
/**
 * @}
 */
/* end of group timer */
#endif
