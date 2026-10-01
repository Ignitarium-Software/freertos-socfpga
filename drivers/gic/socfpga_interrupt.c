/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Implementation of interrupt APIs for SoC FPGA
 */


#include <errno.h>
#include "socfpga_interrupt.h"
#include "arm_gic.h"
#include "arm_gic_reg.h"
#include "osal_log.h"

#define AGX5_DIST_BASE_ADDR    (0x1D000000)
#define AGX5_RD_BASE_ADDR      (0x1D060000)


#define SOCFPGA_DEFAULT_INTERRUPT_SPIN

#define SOCFPGA_PPI_START    22
#define SOCFPGA_MAX_PPI      30
#define SOCFPGA_SPI_START    SDM_APS_MAILBOX_INTR
#define SOCFPGA_MAX_SPI      MAX_HPU_SPI_INTERRUPT

typedef struct
{
    socfpga_interrupt_callback_t callback;
    void *data;
} interrupt_handler_t;

static interrupt_handler_t interrupt_callbacks[MAX_SPI_HPU_INTERRUPT] =
{
    [0 ... MAX_SPI_HPU_INTERRUPT - 1U] = { gic_default_interrupt_handler, NULL }
};

void interrupt_irq_handler(unsigned int interrupt_id);

void gic_default_interrupt_handler(void *data) {
    (void)data;
#ifdef SOCFPGA_DEFAULT_INTERRUPT_SPIN
    while (1 == 1) {
    }
#else
    return;
#endif
}

void interrupt_enable_core_redis(void)
{
    /* Get the ID of the redistributor connected to this PE. */
    int32_t gic_redis_id = gic_get_redis_id(
            (uint32_t)gic_get_cpu_affinity());
    if (gic_redis_id < 0)
    {
        return;
    }

    /* Mark this core as being active. */
    if (gic_wakeup_redis((uint32_t)gic_redis_id) != 0)
    {
        return;
    }

    /* Set the interrupt mask. */
    gic_set_priority_mask(0xFFU);

    /* Enable group 1 interrupts (group 0 are secure interrupts). */
    gic_enable_group1_interrupts();
}

/**
 * @brief    Initializes the GIC600 interrupt controller.
 */
void interrupt_init_gic(void)
{
    /* Enable GIC. */
    if (gic_enable_gic() != 0)
    {
        return;
    }

    /* Enable the redistributor of the current core */
    interrupt_enable_core_redis();
}

int interrupt_ppi_enable(socfpga_hpu_interrupt_t id,
        socfpga_hpu_interrupt_type_t interrupt_type,
        uint8_t priority, uint32_t gic_redis_id)
{
    uint32_t type = GICV3_CONFIG_LEVEL;
    if ((id > PPI_MAX))
    {
        return -ERANGE;
    }
    if (interrupt_type == SPI_INTERRUPT_TYPE_EDGE)
    {
        type = GICV3_CONFIG_EDGE;
    }

    int err = gic_set_int_group((uint32_t)id, gic_redis_id,
            GICV3_GROUP1_NON_SECURE);
    if (err != 0)
    {
        return err;
    }
    err = gic_set_int_type((uint32_t)id, gic_redis_id, type);
    if (err != 0)
    {
        return err;
    }
    err = gic_set_int_priority((uint32_t)id, gic_redis_id, priority);
    if (err != 0)
    {
        return err;
    }
    err = gic_enable_int((uint32_t)id, gic_redis_id);
    if (err != 0)
    {
        return err;
    }
    return 0;
}

int interrupt_spi_enable(socfpga_hpu_interrupt_t id,
        socfpga_hpu_interrupt_type_t interrupt_type,
        socfpga_hpu_spi_interrupt_mode_t interrupt_mode,
        uint8_t priority)
{
    uint32_t mode = GICV3_ROUTE_MODE_ANY;
    uint32_t type = GICV3_CONFIG_LEVEL;
    uint32_t affinity = (uint32_t)gic_get_cpu_affinity();
    int gic_redis_id = gic_get_redis_id(affinity);

    if ((id > SOCFPGA_MAX_SPI) || (id < SOCFPGA_SPI_START))
    {
        return -ERANGE;
    }

    if (gic_redis_id < 0)
    {
        return gic_redis_id;
    }

    if (interrupt_type == SPI_INTERRUPT_TYPE_EDGE)
    {
        type = GICV3_CONFIG_EDGE;
    }
    if (interrupt_mode == SPI_INTERRUPT_MODE_TARGET)
    {
        mode = GICV3_ROUTE_MODE_COORDINATE;
    }

    int err = gic_set_int_priority((uint32_t)id, (uint32_t)gic_redis_id, priority);
    if (err != 0)
    {
        return err;
    }
    err = gic_set_int_group((uint32_t)id, (uint32_t)gic_redis_id, GICV3_GROUP1_NON_SECURE);
    if (err != 0)
    {
        return err;
    }
    err = gic_set_int_route((uint32_t)id, mode, affinity);
    if (err != 0)
    {
        return err;
    }
    err = gic_set_int_type((uint32_t)id, (uint32_t)gic_redis_id, type);
    if (err != 0)
    {
        return err;
    }
    err = gic_enable_int((uint32_t)id, (uint32_t)gic_redis_id);
    if (err != 0)
    {
        return err;
    }
    return 0;
}
int interrupt_enable(socfpga_hpu_interrupt_t id, uint8_t priority)
{
    int err = 0;
    int gic_redis_id;
    if (id < SOCFPGA_SPI_START)
    {
        gic_redis_id = gic_get_redis_id(
                (uint32_t)gic_get_cpu_affinity());
        if (gic_redis_id < 0)
        {
            return gic_redis_id;
        }
        err = interrupt_ppi_enable(id, SPI_INTERRUPT_TYPE_LEVEL, priority,
                (uint32_t)gic_redis_id);
    }
    else
    {
        err = interrupt_spi_enable(id, SPI_INTERRUPT_TYPE_LEVEL, SPI_INTERRUPT_MODE_TARGET,
                priority);
    }

    return err;
}

int interrupt_disable(socfpga_hpu_interrupt_t id)
{
    int gic_redis_id = gic_get_redis_id(
                (uint32_t)gic_get_cpu_affinity());
    if (gic_redis_id < 0)
    {
        return gic_redis_id;
    }

    return gic_disable_int((uint32_t)id, (uint32_t)gic_redis_id);
}

int interrupt_register_isr(socfpga_hpu_interrupt_t id,
        socfpga_interrupt_callback_t callback,
        void *user_data)
{
    if (id > MAX_HPU_SPI_INTERRUPT)
    {
        return -ERANGE;
    }
    if (callback == 0)
    {
        return -EINVAL;
    }
    interrupt_callbacks[id].callback = callback;
    interrupt_callbacks[id].data = user_data;
    return 0;
}

/*
 * @func  : vInterruptIRQHandler
   @brief : The IRQ interrupt handler. This function will determine the IRQ handler based on the interrupt ID.
   @param : interrupt_id -> interruptID
 */

void interrupt_irq_handler(unsigned int interrupt_id)
{
    /* Clear pending interrupts. */
    /*This is the Max ID for PPI and SPI*/
    if (interrupt_id < MAX_SPI_HPU_INTERRUPT)
    {
        interrupt_callbacks[interrupt_id].callback(interrupt_callbacks[
                    interrupt_id].data);
    }
    else
    {
        INFO("FIQ: Panic, unexpected INTID");
    }

    gic_enable_interrupts();

    return;
}
