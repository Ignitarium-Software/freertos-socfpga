/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for timer
 */

 #include <errno.h>
#include "socfpga_timer.h"
#include "socfpga_timer_reg.h"
#include "socfpga_interrupt.h"
#include "socfpga_clk_mngr.h"
#include "socfpga_rst_mngr.h"
#include "osal_log.h"

#define TIMER_DISABLE    0x0U
#define TIMER_ENABLE     0x1U

/*Macro to convert ticks to miro second*/
#define TIMER_TICKS_TO_US(clk_hz, ticks) \
    ((uint32_t)((((uint64_t)(ticks)) * ((uint64_t)(1000U * 1000U))) / \
    (clk_hz)))

#define TIMER_FREE_RUNNING_PERIOD    0xFFFFFFFFU

struct timer_context
{
    uint32_t base_address;
    BaseType_t is_running;
    BaseType_t is_open;
    BaseType_t is_configured;
    socfpga_hpu_interrupt_t interrupt_id;
    uint32_t clk_hz;
    timer_callback_t callback_func;
    void *param;

};

static struct timer_context timer_descriptor[TIMER_NUM_INSTANCE];

void timer_irq_handler(void *data);

static reset_periphrl_t timer_get_rst_instance (timer_instance_t instance)
{
    reset_periphrl_t rst_instance = RST_PERIPHERAL_END;
    switch (instance)
    {
        case TIMER_SYS0:
            rst_instance = RST_L4SYSTIMER0;
            break;
        case TIMER_SYS1:
            rst_instance = RST_L4SYSTIMER1;
            break;
        case TIMER_SP0:
            rst_instance = RST_SPTIMER0;
            break;
        case TIMER_SP1:
            rst_instance = RST_SPTIMER1;
            break;
        default:
            ERROR("Invalid Timer instance");
            break;
    }
    return rst_instance;
}

/* Convert microsecond delay to tick value */
static inline uint32_t timer_us_to_ticks(uint32_t clk_hz, uint32_t time_us)
{
    uint64_t ticks = (((uint64_t)clk_hz) * ((uint64_t)time_us));
    ticks /= 1000000ULL;

    if (ticks > 0xFFFFFFFFULL)
    {
        return TIMER_FREE_RUNNING_PERIOD;
    }
    return (uint32_t)ticks;
}

timer_handle_t timer_open(timer_instance_t instance)
{
    uint32_t clk_hz;
    uint32_t val;
    uint8_t res;
    int32_t status;
    clock_block_t clock_block_id = CLOCK_INVALID;
    socfpga_interrupt_err_t int_ret;
    reset_periphrl_t rst_instance;
    int32_t ret = 0;

    /* Check Instance Validity */
    if (instance >= TIMER_N_INSTANCE)
    {
        ERROR("Invalid Timer instance");
        return NULL;
    }
    timer_handle_t handle = &timer_descriptor[instance];
    if (handle->is_open == 1)
    {
        ERROR("Timer instance is already running");
        return NULL;
    }
    /* Update Timer base address to the context variable */
    switch (instance)
    {

        case TIMER_SYS0:
            handle->base_address = OSC1_TIMER0_BASE_ADDR;
            handle->interrupt_id = TIMER_OSC10IRQ;
            clock_block_id = CLOCK_SP_TIMER;
            break;
        case TIMER_SYS1:
            handle->base_address = OSC1_TIMER1_BASE_ADDR;
            handle->interrupt_id = TIMER_OSC11IRQ;
            clock_block_id = CLOCK_SP_TIMER;
            break;
        case TIMER_SP0:
            handle->base_address = SPTIMER0_BASE_ADDR;
            handle->interrupt_id = TIMER_L4SP0IRQ;
            clock_block_id = CLOCK_OSC1TIMER;
            break;
        case TIMER_SP1:
            handle->base_address = SPTIMER0_BASE_ADDR;
            handle->interrupt_id = TIMER_L4SP1IRQ;
            clock_block_id = CLOCK_OSC1TIMER;
            break;
        default:
            ERROR("Invalid Timer instance");
            ret = -EINVAL;
            break;

    }
    if (ret == -EINVAL)
    {
        return NULL;
    }

    rst_instance = timer_get_rst_instance(instance);
    if (rst_instance == RST_PERIPHERAL_END)
    {
        ERROR("Invalid reset instance");
        return NULL;
    }

    status = rstmgr_get_reset_status(rst_instance, &res);
    if (status != 0)
    {
        ERROR("Timer block get reset status failed. ");
        return NULL;
    }
    if (res != 0U)
    {
        status = rstmgr_toggle_reset(rst_instance);
        if (status != 0)
        {
            ERROR("Failed to reset release Timer block. ");
            return NULL;
        }
    }
    /* Get the source clock value in Hz */
    if (clk_mngr_get_clk(clock_block_id, &clk_hz) == 1U)
    {
        ERROR("error while getting clock");
        return NULL;
    }
    handle->clk_hz = clk_hz;

    /* Setup and enable interrupts in GIC */
    int_ret = interrupt_register_isr(handle->interrupt_id, timer_irq_handler,
            handle);
    if (int_ret != ERR_OK)
    {
        ERROR("error while registering ISR");
        return NULL;
    }
    int_ret = interrupt_enable(handle->interrupt_id,
            GIC_INTERRUPT_PRIORITY_TIMER);
    if (int_ret != ERR_OK)
    {
        ERROR("error while enabling ISR");
        return NULL;
    }

    /* Enable the interrupt */
    val = RD_REG32((handle->base_address + TIMER_TIMER1CONTROLREG));
    val &= ~TIMER_TIMER1CONTROLREG_TIMER_INTERRUPT_MASK_MASK;
    WR_REG32((handle->base_address + TIMER_TIMER1CONTROLREG), val);

    handle->is_open = 1;

    return handle;

}

int32_t timer_set_period_us(timer_handle_t const htimer, uint32_t period)
{
    uint32_t volatile val, count_val;
    if (htimer == NULL)
    {
        ERROR("Timer handle cannot be NULL");
        return -EINVAL;
    }
    if (htimer->is_running == 1)
    {
        ERROR("Timer instance already running");
        return -EBUSY;
    }
    /* Timer needs to be disabled to configure */
    WR_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG, TIMER_DISABLE);
    val = RD_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG);
    val |= TIMER_TIMER1CONTROLREG_TIMER_MODE_MASK;
    WR_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG, val);
    count_val = timer_us_to_ticks(htimer->clk_hz, period);
    WR_REG32(htimer->base_address + TIMER_TIMER1LOADCOUNT, count_val);
    htimer->is_configured = 1;

    return 0;
}

int32_t timer_start(timer_handle_t const htimer)
{
    uint32_t val;
    if (htimer == NULL)
    {
        ERROR("Timer handle cannot be NULL");
        return -EINVAL;
    }

    if (htimer->is_configured == 0)
    {
        ERROR("Timer needs to be configured before starting");
        return -EINVAL;
    }

    if ((htimer->is_running) == 1)
    {
        ERROR("Timer instance already running");
        return -EBUSY;
    }

    INFO("Starting timer instance");
    /* Enable the timer */
    val = RD_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG);
    val |= TIMER_ENABLE;
    WR_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG, val);

    htimer->is_running = 1;

    return 0;
}

int32_t timer_stop(timer_handle_t const htimer)
{

    if ((htimer == NULL))
    {
        ERROR("Timer Handle cannot be NULL");
        return -EINVAL;
    }
    if (htimer->is_running == 0)
    {
        ERROR("Timer instance not running");
        return -EPERM;
    }
    /*
     * Disable timer
     *
     * Write 0x0 directly without read modify as disabling timer
     * resets all associated registers
     */
    INFO("Stopping timer instance");
    WR_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG, TIMER_DISABLE);
    htimer->is_running = 0;
    htimer->is_configured = 0;
    return 0;
}

int32_t timer_close(timer_handle_t const htimer)
{
    socfpga_interrupt_err_t int_ret;

    if ((htimer == NULL))
    {
        ERROR("Timer Handle cannot be NULL");
        return -EINVAL;
    }
    if (htimer->is_open == 0)
    {
        ERROR("Invalid handle");
        return -EINVAL;
    }
    /*
     * Disable timer
     *
     * Write 0x0 directly without read modify as disabling timer
     * resets all associated registers
     */
    WR_REG32(htimer->base_address + TIMER_TIMER1CONTROLREG, TIMER_DISABLE);
    /* Disable timer interrupt at GIC level */
    int_ret = interrupt_spi_disable(htimer->interrupt_id);
    if (int_ret != ERR_OK)
    {
        ERROR("error while disabling ISR");
        return -EFAULT;
    }

    htimer->base_address = 0;
    htimer->is_running = 0;
    htimer->is_open = 0;
    htimer->is_configured = 0;
    /* setting unsupported interrupt id to prevent interrupts */
    htimer->interrupt_id = MAX_HPU_SPI_INTERRUPT;
    htimer->clk_hz = 0;
    htimer->callback_func = NULL;

    return 0;
}

int32_t timer_get_value_raw(timer_handle_t const htimer, uint32_t *counter_val)
{
    if ((htimer == NULL) || (counter_val == NULL))
    {
        ERROR("Timer Handle cannot be NULL");
        return -EINVAL;
    }
    if (htimer->is_running == 0)
    {
        ERROR("Timer instance not running");
        return -EPERM;
    }
    *counter_val = RD_REG32(
            htimer->base_address + TIMER_TIMER1CURRENTVAL);
    return 0;
}

int32_t timer_get_value_us(timer_handle_t const htimer, uint32_t *time_us)
{
    if ((htimer == NULL) || (time_us == NULL))
    {
        ERROR("Timer Handle cannot be NULL");
        return -EINVAL;
    }
    if (htimer->is_running == 0)
    {
        ERROR("Timer instance not running");
        return -EPERM;
    }

    uint32_t count_val = 0U;
    count_val = RD_REG32(
            htimer->base_address + TIMER_TIMER1CURRENTVAL);

    if (htimer->clk_hz == 0U)
    {
        ERROR("Denominator is 0");
        return -EINVAL;
    }
    *time_us = TIMER_TICKS_TO_US(htimer->clk_hz, count_val);
    return 0;
}

int32_t timer_set_callback(timer_handle_t const htimer,
        timer_callback_t callback, void *param)
{
    if (htimer == NULL)
    {
        ERROR("Timer handle cannot be NULL");
        return -EINVAL;
    }
    /* Assign user callback to context variable */
    htimer->callback_func = callback;
    htimer->param = param;
    return 0;
}

void timer_irq_handler(void *data)
{
    uint32_t val;
    timer_handle_t handle = (timer_handle_t)data;
    /* Reading TIMER_TIMER1EOI register clears the interrupt and it returns all zeros */
    val = RD_REG32(handle->base_address + TIMER_TIMER1EOI);
    (void)val;
    if (handle->callback_func != NULL)
    {
        handle->callback_func(data);
    }
}
