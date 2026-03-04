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

#ifndef __SOCFPGA_GPIO_H__
#define __SOCFPGA_GPIO_H__

/**
 * @file socfpga_gpio.h
 * @brief This file contains the GPIO HAL API definitions.
 */

#include "socfpga_defines.h"
#include <errno.h>

/**
 * @defgroup gpio GPIO
 * @brief APIs for Soc FPGA GPIO driver.
 * @details This is the GPIO driver implementation for SoC FPGA.
 * It provides APIs for configuring GPIO pins, setting pin direction,
 * reading pin values, and handling GPIO interrupts. For example usage,
 * see @ref gpio_sample "GPIO sample application".
 * @ingroup drivers
 * @{
 */
/**
 * @defgroup gpio_fns Functions
 * @ingroup gpio
 * GPIO HAL APIs
 */

/**
 * @defgroup gpio_structs Structures
 * @ingroup gpio
 * GPIO Specific Structures
 */

/**
 * @defgroup gpio_enums Enumerations
 * @ingroup gpio
 * GPIO Specific Enumerations
 */

/**
 * @defgroup gpio_macros Macros
 * @ingroup gpio
 * GPIO Specific Macros
 */

/**
 * @addtogroup gpio_macros
 * @{
 */
#define GPIO_MAX_INSTANCE    (48U)     /*!< Number of GPIO pins supported. */
#define GPIO_PINS_PER_REG    (24U)     /*!< Number of GPIO pins having same base address . */
#define GPIO_INSTANCE1       (0U)  /*!< GPIO instance 0. */
#define GPIO_INSTANCE0       (1U)   /*!< GPIO instance 1. */
#define PINMUX_REG(p)                                                 \
	(((p) < 40U) ?                                                    \
	 (0x10D13000U + (4U * (p))) :                                     \
	 (0x10D13100U + (4U * ((p) - 40U)))) /**< Returns pinmux register */
#define PINMUX_GPIO        (0x08U)       /*!< GPIO functionality bit. */
#define PINMUX_MASK        (0x0FU)       /*!< Pin functionality mask. */
#define GPIO0_BASE_ADDR    (0x10C03200U)             /*!< GPIO instance 0 base address. */
#define GPIO1_BASE_ADDR    (0x10C03300U)             /*!< GPIO instance 1 base address. */
#define GET_GPIO_BASE_ADDR(instance)    (((instance) < 24U) ? GPIO0_BASE_ADDR \
    : GPIO1_BASE_ADDR)                            /*!< Returns the base address of GPIO pin. */
/**
 * @}
 *
 */

/**
 * @addtogroup gpio_enums
 * @{
 */
/**
 * @brief Available gpio pins.
 * @ingroup gpio_enums
 */
typedef enum
{
    GPIO0_PIN0 = 0,       /*!< GPIO 0th instance pin 0. */
    GPIO0_PIN1,           /*!< GPIO 0th instance pin 1. */
    GPIO0_PIN2,           /*!< GPIO 0th instance pin 2. */
    GPIO0_PIN3,           /*!< GPIO 0th instance pin 3. */
    GPIO0_PIN4,           /*!< GPIO 0th instance pin 4. */
    GPIO0_PIN5,           /*!< GPIO 0th instance pin 5. */
    GPIO0_PIN6,           /*!< GPIO 0th instance pin 6. */
    GPIO0_PIN7,           /*!< GPIO 0th instance pin 7. */
    GPIO0_PIN8,           /*!< GPIO 0th instance pin 8. */
    GPIO0_PIN9,           /*!< GPIO 0th instance pin 9. */
    GPIO0_PIN10,          /*!< GPIO 0th instance pin 10. */
    GPIO0_PIN11,          /*!< GPIO 0th instance pin 11. */
    GPIO0_PIN12,          /*!< GPIO 0th instance pin 12. */
    GPIO0_PIN13,          /*!< GPIO 0th instance pin 13. */
    GPIO0_PIN14,          /*!< GPIO 0th instance pin 14. */
    GPIO0_PIN15,          /*!< GPIO 0th instance pin 15. */
    GPIO0_PIN16,          /*!< GPIO 0th instance pin 16. */
    GPIO0_PIN17,          /*!< GPIO 0th instance pin 17. */
    GPIO0_PIN18,          /*!< GPIO 0th instance pin 18. */
    GPIO0_PIN19,          /*!< GPIO 0th instance pin 19. */
    GPIO0_PIN20,          /*!< GPIO 0th instance pin 20. */
    GPIO0_PIN21,          /*!< GPIO 0th instance pin 21. */
    GPIO0_PIN22,          /*!< GPIO 0th instance pin 22. */
    GPIO0_PIN23,          /*!< GPIO 0th instance pin 23. */
    GPIO1_PIN0 = 24,           /*!< GPIO 1st instance pin 0. */
    GPIO1_PIN1,           /*!< GPIO 1st instance pin 1. */
    GPIO1_PIN2,           /*!< GPIO 1st instance pin 2. */
    GPIO1_PIN3,           /*!< GPIO 1st instance pin 3. */
    GPIO1_PIN4,           /*!< GPIO 1st instance pin 4. */
    GPIO1_PIN5,           /*!< GPIO 1st instance pin 5. */
    GPIO1_PIN6,           /*!< GPIO 1st instance pin 6. */
    GPIO1_PIN7,           /*!< GPIO 1st instance pin 7. */
    GPIO1_PIN8,           /*!< GPIO 1st instance pin 8. */
    GPIO1IN9,            /*!< GPIO 1st instance pin 9. */
    GPIO1_PIN10,          /*!< GPIO 1st instance pin 10. */
    GPIO1_PIN11,          /*!< GPIO 1st instance pin 11. */
    GPIO1_PIN12,          /*!< GPIO 1st instance pin 12. */
    GPIO1_PIN13,          /*!< GPIO 1st instance pin 13. */
    GPIO1_PIN14,          /*!< GPIO 1st instance pin 14. */
    GPIO1_PIN15,          /*!< GPIO 1st instance pin 15. */
    GPIO1_PIN16,          /*!< GPIO 1st instance pin 16. */
    GPIO1_PIN17,          /*!< GPIO 1st instance pin 17. */
    GPIO1_PIN18,          /*!< GPIO 1st instance pin 18. */
    GPIO1_PIN19,          /*!< GPIO 1st instance pin 19. */
    GPIO1_PIN20,          /*!< GPIO 1st instance pin 20. */
    GPIO1_PIN21,          /*!< GPIO 1st instance pin 21. */
    GPIO1_PIN22,          /*!< GPIO 1st instance pin 22. */
    GPIO1_PIN23,          /*!< GPIO 1st instance pin 23. */
    GPIO_NUM_PINS,         /*!< GPIO pins available. */
    GPIO_NPIN_PER_REG = 24  /*!< GPIO pins available per register . */
} gpio_pin_t;

/**
 * @brief GPIO operation direction.
 * @ingroup gpio_enums
 */
typedef enum
{
    GPIO_DIR_IN, /*!< Input direction. */
    GPIO_DIR_OUT /*!< Output direction. */
} gpio_dir_t;

/**
 * @brief GPIO interrupts.
 * @ingroup gpio_enums
 */
typedef enum
{
    GPIO_INT_NONE, /*!< No interrupt. */
    GPIO_INT_RISING, /*!< Interrupt on rising edge. */
    GPIO_INT_FALLING, /*!< Interrupt on falling edge. */
    GPIO_INT_LOW, /*!< Interrupt on low level. */
    GPIO_INT_HIGH, /*!< Interrupt on high level. */
} gpio_int_t;

/**
 * @brief Ioctl requests for GPIO.
 * @ingroup gpio_enums
 */
typedef enum
{
    SET_GPIO_DIR, /*!< Set GPIO Direction. Takes input type gpio_dir_t */
    SET_GPIO_INT, /*!< Set GPIO Interrupt type. Takes input type gpio_int_t */
    GET_GPIO_DIRECTION, /*!< Get GPIO Direction setting. Returns gpio_dir_t */
    GET_GPIO_INT, /*!< Get GPIO Interrupt config. Returns gpio_int_t type */
} gpio_ioctl_t;
/**
 * @}
 */

/**
 * @addtogroup gpio_structs
 * @{
 */
/**
 * @brief GPIO descriptor type defined in the source file.
 */
struct gpio_descriptor;

/**
 * @brief gpio_handle_t type is the GPIO handle returned by calling gpio_open()
 * this is initialized in open and returned to caller. Caller must pass this pointer
 * to the rest of the APIs.
 */
typedef struct gpio_descriptor *gpio_handle_t;
/**
 * @}
 */

/**
 * @addtogroup gpio_fns
 * @{
 */
/**
 * @brief GPIO interrupt callback type. This callback is passed to the
 * driver by using gpio_set_callback API.
 */
typedef void (*gpio_callback_t)(uint8_t state, void *param);

/**
 * @brief Initializes the GPIO pin instance.
 * The application must call this function to open the desired GPIO pin
 * before using it for any functionality.
 *
 * @param[in] pin The GPIO pin number to open. Shall use the constants
 *                defined by gpio_pin_t
 *
 * @return
 * - handle to the GPIO peripheral if everything succeeds
 * - NULL, if
 *     - invalid instance number
 *     - open same instance more than once before closing it.
 */
gpio_handle_t gpio_open(gpio_pin_t pin);

/**
 * @brief Sets the application callback to be invoked on interrupt trigger.
 * The callback is triggered by a hardware interrupt.
 * To receive the callback, the user must configure an interrupt type
 * for the GPIO pin using gpio_ioctl().
 *
 * @note This callback is per handle. Each instance has its own callback.
 *
 * @warning If input handle or if callback function is NULL, this function silently takes no action.
 *
 * @param[in] hgpio         The GPIO handle returned in the open() call.
 * @param[in] gpio_callback The callback function to be called on interrupt.
 * @param[in] param         The user context to be passed back when callback is called.
 */
void gpio_set_callback(gpio_handle_t const hgpio, gpio_callback_t gpio_callback, void *param);

/**
 * @brief Read the gpio pin in blocking mode
 *
 * @param[in]  hgpio  The GPIO handle returned in the open() call.
 * @param[out] pstate The pin state.
 *
 * @return
 * - 0:       on success
 * - -EINVAL: if hgpio or pstate are NULL or handle is not open
 */
int32_t gpio_read_sync(gpio_handle_t const hgpio, uint8_t *pstate);

/**
 * @brief Write the gpio pin in blocking mode
 *
 * @param[in] hgpio The GPIO handle returned in the open() call.
 * @param[in] state Desired pin state
 *
 * @return
 * - 0        on success
 * - -EINVAL: if hgpio is NULL or handle is not open
 */
int32_t gpio_write_sync(gpio_handle_t const hgpio, uint8_t state);

/**
 * @brief Deinitializes the GPIO pin and closes the handle.
 * The application should call this function to reset the GPIO pin to its default state,
 * disable any associated interrupts, and release the handle.
 *
 * @param[in] hgpio The GPIO handle returned in the open() call.
 *
 * @return
 * - 0:       on success (gpio pin is deinitialized)
 * - -EINVAL: if
 *     - hgpio handle is NULL
 *     - if is not in open state (already closed).
 */
int32_t gpio_close(gpio_handle_t const hgpio);

/**
 * @brief gpio_ioctl is used to configure GPIO pin options.
 * The application should call this function to configure various GPIO
 * pin options: I/O direction, interrupt options etc
 *
 * @param[in]     hgpio The GPIO handle returned in the open() call.
 * @param[in]     cmd   One of gpio_ioctl_t enum
 * @param[in,out] buf   Buffer holding IOCTL argument
 *
 * @return
 * - 0:       on success
 * - -EINVAL: on NULL handle, invalid request, or NULL buffer when required.
 */
int32_t gpio_ioctl(gpio_handle_t const hgpio, gpio_ioctl_t cmd, void *const
        buf);

/**
 * @}
 */
/* end of group gpio_fns */
/**
 * @}
 */
/* end of group gpio */

#endif /* __SOCFPGA_GPIO_H__ */
