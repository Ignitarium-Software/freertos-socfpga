/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for gpio
 */

#include <stdbool.h>
#include <string.h>
#include "socfpga_gpio.h"
#include "socfpga_interrupt.h"
#include "socfpga_gpio_reg.h"
#include "socfpga_defines.h"
#include "socfpga_rst_mngr.h"
#include "osal_log.h"

struct gpio_descriptor
{
    BaseType_t is_open;
    gpio_pin_t instance;
    gpio_callback_t callback_fn;
    void *cb_usercontext;
};

static struct gpio_descriptor gpio_descriptors[GPIO_MAX_INSTANCE];

void gpio_irq_handler(void *data);

/*
 * @brief Check if the pin is configured as gpio in pinmux
 */
static bool is_pin_gpio(uint32_t pin)
{
    uint32_t val;
    val = RD_REG32(PINMUX_REG(pin));
    val &= PINMUX_MASK;
    return (val == PINMUX_GPIO);
}

/*
 * @brief Set the bit value in the given gpio block register
 */
static void set_gpio_reg_bit(uint32_t reg, uint32_t pin, uint8_t val)
{
    uint32_t reg_val;
    uint32_t base_add = GET_GPIO_BASE_ADDR(pin);
    if (pin >= GPIO_PINS_PER_REG)
    {
        pin -= GPIO_PINS_PER_REG;
    }
    reg_val = RD_REG32(base_add + reg);
    reg_val &= ~(1U << pin);
    reg_val |= ((uint32_t)val << pin);
    WR_REG32(base_add + reg, reg_val);
}

/*
 * @brief Get the bit value in the given gpio block register
 */
static uint8_t get_gpio_reg_bit(uint32_t reg, uint8_t pin)
{
    uint32_t val;
    val = RD_REG32(GET_GPIO_BASE_ADDR(pin) + reg);
    if (pin >= GPIO_PINS_PER_REG)
    {
        pin -= GPIO_PINS_PER_REG;
    }
    val = (val >> pin) & 0x01U;
    return ((uint8_t)val);
}

/*
 * @brief set the direction of gpio pin
 */
static void gpio_set_direction(gpio_pin_t pin, gpio_dir_t dir)
{
    if (dir == GPIO_DIR_IN)
    {
        set_gpio_reg_bit(GPIO_SWPORTA_DDR, (uint32_t)pin, 0U);
    }
    else
    {
        set_gpio_reg_bit(GPIO_SWPORTA_DDR, (uint32_t)pin, 1U);
    }
}

/*
 * @brief handle the interrupt
 */
void gpio_irq_handler(void *data)
{
    uint8_t gpio_inst = *(uint8_t *)data;
    uint8_t pin_state;
    uint8_t pin_offset;
    uint32_t int_stat;
    uint32_t gpio_base;
    uint8_t pin;
    uint8_t i;

    gpio_base = (gpio_inst == GPIO_INSTANCE1)
            ? GPIO0_BASE_ADDR
            : GPIO1_BASE_ADDR;

    /* get the active interrupts */
    int_stat = RD_REG32(gpio_base + GPIO_INTSTATUS);

    /* clear all the edge interrupts */
    WR_REG32(gpio_base + GPIO_PORTA_EOI, int_stat);

    pin_offset = ((gpio_inst == GPIO_INSTANCE1) ? 0U : GPIO_PINS_PER_REG);

    /* Invoke callback for all active interrupts if it is registered */
    for (i = 0; i < GPIO_PINS_PER_REG; i++)
    {
        if ((int_stat & ((uint32_t)1 << i)) == ((uint32_t)1U << i))
        {
            pin = i + pin_offset;
            if (gpio_descriptors[pin].callback_fn != NULL)
            {
                pin_state = get_gpio_reg_bit(GPIO_SWPORTA_DR, pin);
                gpio_descriptors[pin].callback_fn(pin_state, gpio_descriptors[pin].cb_usercontext);
            }
        }
    }
}

/*
 * @brief set the interrupt configuration
 */
static bool gpio_set_interrupt(gpio_pin_t pin, gpio_int_t type)
{
    socfpga_hpu_interrupt_t id;
    uint8_t gpio_inst;
    socfpga_interrupt_err_t int_ret;
    if (pin < GPIO_NPIN_PER_REG)
    {
        id = GPIO0IRQ;
        gpio_inst = 0;
    }
    else
    {
        id = GPIO1IRQ;
        gpio_inst = 1;
    }
    int_ret = interrupt_register_isr(id, gpio_irq_handler, &gpio_inst);
    if (int_ret != ERR_OK)
    {
        return false;
    }

    /* clear the edge interrupt if set already */
    set_gpio_reg_bit(GPIO_PORTA_EOI, (uint32_t)pin, 1U);

    /* Configure the level and polarity and enable the interrupt */
    switch (type)
    {
        case GPIO_INT_NONE:
            set_gpio_reg_bit(GPIO_INTEN, (uint32_t)pin, 0U);
            break;
        case GPIO_INT_RISING:
            set_gpio_reg_bit(GPIO_INTTYPE_LEVEL, (uint32_t)pin, 1U);
            set_gpio_reg_bit(GPIO_INT_POLARITY, (uint32_t)pin, 1U);
            set_gpio_reg_bit(GPIO_INTEN, (uint32_t)pin, 1U);
            break;
        case GPIO_INT_FALLING:
            set_gpio_reg_bit(GPIO_INTTYPE_LEVEL, (uint32_t)pin, 1U);
            set_gpio_reg_bit(GPIO_INT_POLARITY, (uint32_t)pin, 0U);
            set_gpio_reg_bit(GPIO_INTEN, (uint32_t)pin, 1U);
            break;
        case GPIO_INT_LOW:
            set_gpio_reg_bit(GPIO_INTTYPE_LEVEL, (uint32_t)pin, 0U);
            set_gpio_reg_bit(GPIO_INT_POLARITY, (uint32_t)pin, 0U);
            set_gpio_reg_bit(GPIO_INTEN, (uint32_t)pin, 1U);
            break;
        case GPIO_INT_HIGH:
            set_gpio_reg_bit(GPIO_INTTYPE_LEVEL, (uint32_t)pin, 0U);
            set_gpio_reg_bit(GPIO_INT_POLARITY, (uint32_t)pin, 1U);
            set_gpio_reg_bit(GPIO_INTEN, (uint32_t)pin, 1U);
            break;

        default:
            /* do nothing */
            break;
    }
    if (type != GPIO_INT_NONE)
    {
        int_ret = interrupt_enable(id, GIC_INTERRUPT_PRIORITY_GPIO);
        if (int_ret != ERR_OK)
        {
            return false;
        }
    }
    return true;
}

/*
 * @brief get the direction of the gpio pin
 */
static uint8_t gpio_get_direction(gpio_pin_t pin)
{
    uint8_t ret_val;
    ret_val = get_gpio_reg_bit(GPIO_SWPORTA_DDR, pin);
    return ret_val;
}

/*
 * @brief get the interrupt configuration of the gpio pin
 */

static gpio_int_t gpio_get_interrupt(gpio_pin_t pin)
{
    gpio_int_t ret_val = GPIO_INT_NONE;
    uint8_t res1;
    uint8_t res2;

    /* get the level/edge and polarity */
    res1 = get_gpio_reg_bit(GPIO_INTTYPE_LEVEL, pin);
    res2 = get_gpio_reg_bit(GPIO_INT_POLARITY, pin);
    res1 = (res1 << 1U) | res2;

    switch (res1)
    {
        case 0:
            ret_val = GPIO_INT_LOW;
            break;
        case 1:
            ret_val = GPIO_INT_HIGH;
            break;
        case 2:
            ret_val = GPIO_INT_FALLING;
            break;
        case 3:
            ret_val = GPIO_INT_RISING;
            break;
        default:
            ret_val = GPIO_INT_NONE;
            break;
    }
    res1 = get_gpio_reg_bit(GPIO_INTEN, pin);
    if (res1 == 0U)
    {
        ret_val = GPIO_INT_NONE;
    }
    return ret_val;
}

/**
 * @brief Get the reset instance for the GPIO peripheral
 */
static reset_periphrl_t gpio_get_rst_instance(gpio_pin_t pin)
{
    reset_periphrl_t rst_instance = RST_PERIPHERAL_END;
    if (pin <= GPIO0_PIN23)
    {
        rst_instance = RST_GPIO0;
    }
    else if (pin <= GPIO1_PIN23)
    {
        rst_instance = RST_GPIO1;
    }
    else
    {
        rst_instance = RST_GPIO1;
    }
    return rst_instance;
}

/*
 * @brief read the current state of the gpio pin
 */
static uint8_t gpio_read_state(gpio_pin_t pin)
{
    uint8_t ret_val;
    ret_val = get_gpio_reg_bit(GPIO_EXT_PORTA, pin);
    return ret_val;
}

/*
 * @brief write the sate of the gpio pin
 */
static void gpio_write_state(gpio_pin_t pin, uint8_t state)
{
    set_gpio_reg_bit(GPIO_SWPORTA_DR, (uint32_t)pin, state);
}

gpio_handle_t gpio_open(gpio_pin_t pin)
{
    gpio_handle_t handle;
    reset_periphrl_t rst_instance;
    uint8_t reset_status;
    int32_t status;

    if ((pin >= GPIO_NUM_PINS) || (pin < GPIO0_PIN0))
    {
        return NULL;
    }
    if (is_pin_gpio((uint32_t)pin) == false)
    {
        ERROR(
                "The pin trying to open is allocated for different interface in Pinmux REG");
        return NULL;
    }

    handle = &(gpio_descriptors[pin]);
    if (handle->is_open == true)
    {
        return NULL;
    }
    rst_instance = gpio_get_rst_instance(pin);
    status = rstmgr_get_reset_status(rst_instance, &reset_status);
    if (status != 0)
    {
        ERROR("Failed to get GPIO reset status. ");
        return NULL;
    }
    if (reset_status != 0U)
    {
        status = rstmgr_toggle_reset(rst_instance);
        if (status != 0)
        {
            ERROR("Failed to reset release GPIO block. ");
            return NULL;
        }
    }

    (void)memset(handle, 0, sizeof(struct gpio_descriptor));
    handle->is_open = 1;
    handle->instance = pin;
    return handle;
}

int32_t gpio_ioctl(gpio_handle_t const hgpio, gpio_ioctl_t cmd, void *const buf)
{
    gpio_dir_t dir_config;
    gpio_int_t intr_config;
    int32_t ret = 0;

    if (hgpio == NULL)
    {
        return -EINVAL;
    }
    if (buf == NULL)
    {
        return -EINVAL;
    }
    switch (cmd)
    {
        case SET_GPIO_DIR:
            dir_config = *(gpio_dir_t *)buf;
            gpio_set_direction(hgpio->instance, dir_config);
            break;
        case SET_GPIO_INT:
            intr_config = *(gpio_int_t *)buf;
            if (gpio_set_interrupt(hgpio->instance, intr_config) == false)
            {
                ret = -EINVAL;
            }
            break;
        case GET_GPIO_DIRECTION:
            *(uint8_t *)buf = gpio_get_direction(hgpio->instance);
            break;
        case GET_GPIO_INT:
            *(gpio_int_t *)buf = gpio_get_interrupt(hgpio->instance);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

void gpio_set_callback(gpio_handle_t const hgpio, gpio_callback_t gpio_callback, void *param)
{
    if (hgpio == NULL)
    {
        return;
    }
    hgpio->callback_fn = gpio_callback;
    hgpio->cb_usercontext = param;
}

int32_t gpio_read_sync(gpio_handle_t const hgpio, uint8_t *pstate)
{
    if ((hgpio == NULL) || (pstate == NULL))
    {
        return -EINVAL;
    }
    if (hgpio->is_open == 0)
    {
        return -EINVAL;
    }
    *pstate = gpio_read_state(hgpio->instance);
    return 0;
}

int32_t gpio_write_sync(gpio_handle_t const hgpio, uint8_t state)
{
    if (hgpio == NULL)
    {
        return -EINVAL;
    }
    if (hgpio->is_open == 0)
    {
        return -EINVAL;
    }
    gpio_write_state(hgpio->instance, state);
    return 0;
}

int32_t gpio_close(gpio_handle_t const hgpio)
{
    if ((hgpio == NULL) || (hgpio->is_open == 0))
    {
        return -EINVAL;
    }
    if (gpio_get_interrupt(hgpio->instance) != GPIO_INT_NONE)
    {
        if (gpio_set_interrupt(hgpio->instance, GPIO_INT_NONE) == false)
        {
            return -EINVAL;
        }
        hgpio->callback_fn = NULL;
    }
    hgpio->is_open = 0;

    return 0;
}
