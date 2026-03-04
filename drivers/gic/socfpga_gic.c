/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Driver implementation for GIC
 */

#include <string.h>
#include "osal_log.h"
#include "osal.h"
#include "socfpga_gic_registers.h"
#include "socfpga_interrupt.h"
#include "socfpga_gic.h"

#define INTERRUPT_DCTRL_ENG0      (1U << 0U)
#define INTERRUPT_DCTRL_ENG1NS    (1U << 1U)
#define INTERRUPT_DCTRL_ENG1S     (1U << 2U)
#define INTERRUPT_DCTRL_ARE_S     (1U << 4U)
#define INTERRUPT_DCTRL_ARE_NS    (1U << 5U)
#define INTERRUPT_DCTRL_DS        (1U << 6U)
#define INTERRUPT_DCTRL_E1NWF     (1U << 7U)

#define INTERRUPT_MAKE_PRIORITY(x)    (((uint32_t)(x) << portPRIORITY_SHIFT) & 0xFFU)

static struct gic_v3_dist_if *gic_dist;
static struct gic_v3_rdist_if *gic_rdist;

static uint32_t gic_max_rd = 0U;

/*
 * @func gic_enable_gic
 * @brief Function to enable the GIC and configure the base address of GIC controller
 */
int32_t gic_enable_gic(void)
{

    uint32_t index = 0U;

    gic_dist = (struct gic_v3_dist_if *)((void *)REGISTER_SOCFPGA_DIST_BASE_ADDR);
    gic_rdist = (struct gic_v3_rdist_if *)((void *)REGISTER_SOCFPGA_RD_BASE_ADDR);

    if (gic_dist == NULL)
    {
        return INTERRUPT_RETURN_ERROR;
    }

    while ((gic_rdist[index].lpis.GICR_TYPER[0] & (1U << 4U)) == 0U)      /* Keep incrementing until GICR_TYPER.Last reports no more RDs in block */

    {
        index++;
    }

    gic_max_rd = index;

    /* First set the ARE bits */
    gic_dist->GICD_CTLR = INTERRUPT_DCTRL_ARE_S | INTERRUPT_DCTRL_ARE_NS |
            INTERRUPT_DCTRL_DS;


    /* The split here is because the register layout is different once ARE==1 */

    /* Now set the rest of the options */
    gic_dist->GICD_CTLR = INTERRUPT_DCTRL_ENG0 | INTERRUPT_DCTRL_ENG1NS |
            INTERRUPT_DCTRL_ENG1S | INTERRUPT_DCTRL_ARE_S |
            INTERRUPT_DCTRL_ARE_NS | INTERRUPT_DCTRL_DS;

    return INTERRUPT_RETURN_SUCCESS;
}

/*
 * @brief
 */
int32_t gic_get_redist_id(uint32_t affinity)
{
    int32_t index = 0;

    if (gic_rdist == NULL)
    {
        return INTERRUPT_RETURN_ERROR;
    }

    do
    {
        if (gic_rdist[index].lpis.GICR_TYPER[1] == affinity)
        {
            return index;
        }
        index++;
    } while ((uint32_t)index <= gic_max_rd);

    return INTERRUPT_RETURN_ERROR; /* return -1 to signal not RD found */
}

/*
 * @brief
 */
int32_t gic_wakeup_redist(uint32_t rd)
{
    uint32_t tmp;

    if (gic_rdist == NULL)
    {
        return INTERRUPT_RETURN_ERROR;
    }

    /* Tell the Redistributor to wake-up by clearing ProcessorSleep bit */
    tmp = gic_rdist[rd].lpis.GICR_WAKER;
    tmp = tmp & ~0x2U;
    gic_rdist[rd].lpis.GICR_WAKER = tmp;


    /* Poll ChildrenAsleep bit until Redistributor wakes */
    do
    {
        tmp = gic_rdist[rd].lpis.GICR_WAKER;
    } while ((tmp & 0x4U) != 0U);

    return INTERRUPT_RETURN_SUCCESS;
}

static uint32_t gic_is_valid_ext_spi(uint32_t id)
{
    uint32_t max_spi;

    /* Check Ext SPI implemented */
    if (((gic_dist->GICD_TYPER >> 8U) & 0x1U) == 0U)
    {
#ifdef DEBUG
        /* put debug print here */
#endif
        return 0U;  /* GICD_TYPER.ESPI==0: Extended SPI range not present */
    }
    else
    {
        max_spi = ((gic_dist->GICD_TYPER >> 27U) & 0x1FU); /* Get field which reports the number ESPIs in blocks of 32, minus 1 */
        max_spi = (max_spi + 1U) * 32U;          /* Convert into number of ESPIs */
        max_spi = max_spi + 4096U;               /* Range starts at 4096 */

        if (id >= max_spi)
        {
#ifdef DEBUG
            /* put debug print here */
#endif
            return 0U;
        }
    }

    return 1U;
}

#define GIC_PRIORITY_MASK(offset)                 (uint32_t)(0xFFU << (offset))
#define GIC_PRIORITY_SHIFT_OFFSET(val, offset)    (uint32_t)((val) << (offset))
#define GIC_CLEAR_PRIORITY(dest, \
            offset)                               ((dest) &= (~GIC_PRIORITY_MASK(offset)))
#define GIC_SET_PRIORITY(dest, val, offset)       GIC_CLEAR_PRIORITY((dest), (offset)); \
    (dest) |= GIC_PRIORITY_SHIFT_OFFSET((val), (offset))

/*
 * @brief Set the GIC interrupt priority
 */
int32_t gic_set_int_priority(uint32_t id, uint32_t rd, uint8_t priority)
{
    /* Check if the priority is within the bounds */
    if ((priority & 0xF0U) == 0xF0U)
    {
        return INTERRUPT_RETURN_INVALID_PRIORITY;
    }

    if ((gic_rdist == NULL) || (gic_dist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

    if (id < 31U)
    {
        /* Check rd in range */
        if (rd > gic_max_rd)
        {
            return INTERRUPT_RETURN_INVALID_RDIST;
        }
        /* SGI or PPI */
        gic_rdist[rd].sgis.GICR_IPRIORITYR[id] = (uint8_t)INTERRUPT_MAKE_PRIORITY(priority);
    }
    else if (id < 1020U)
    {
        /* SPI */
        /* similar checks are to avoid numeric overflow violation in static analysis */
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */

        uint32_t index = id / 4U;
        uint32_t offset = (id & 0x03U) * 8U;
        uint32_t priority_temp = gic_dist->GICD_IPRIORITYR[index];
        GIC_SET_PRIORITY(priority_temp, INTERRUPT_MAKE_PRIORITY(priority), offset);
        gic_dist->GICD_IPRIORITYR[index] = priority_temp;

    }
    else if ((id > 1055U) && (id < 1120U))
    {
        return INTERRUPT_RETURN_INVALID_ID;
    }
    else if ((id > 4095U) && (id < 5120U))
    {
        return INTERRUPT_RETURN_INVALID_ID;
    }
    else
    {
        return INTERRUPT_RETURN_INVALID_ID;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

int32_t gic_set_int_group(uint32_t id, uint32_t rd, uint32_t security)
{
    uint32_t bank, group, mod, ret = 0U;

#ifdef DEBUG
    /* put debug print here */
#endif

    if ((gic_rdist == NULL) || (gic_dist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

    if (id < 31U)
    {
        /* Check rd in range */
        if (rd > gic_max_rd)
        {
            return INTERRUPT_RETURN_INVALID_RDIST;
        }

        /* SGI or PPI */
        id = 1U << id;

        /* Read current values */
        group = gic_rdist[rd].sgis.GICR_IGROUPR0;
        mod = gic_rdist[rd].sgis.GICR_IGRPMODR0;

        /* Update required bits */
        switch (security)
        {
            case GICV3_GROUP0:
                group = (group & ~id);
                mod = (mod & ~id);
                break;

            case GICV3_GROUP1_SECURE:
                group = (group & ~id);
                mod = (mod | id);
                break;

            case GICV3_GROUP1_NON_SECURE:
                group = (group | id);
                mod = (mod & ~id);
                break;

            default:
                ret = 1U;
                break;
        }
        if (ret == 1U)
        {
            return INTERRUPT_RETURN_INVALID_GROUP;
        }

        /* Write modified version back */
        gic_rdist[rd].sgis.GICR_IGROUPR0 = group;
        gic_rdist[rd].sgis.GICR_IGRPMODR0 = mod;
    }
    else if (id < 1020U)
    {
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */
        // SPI
        bank = id / 32U; /* There are 32 uIDs per register, need to work out which register to access */
        id = id & 0x1fU;    /* ... and which bit within the register */

        id = 1U << id;

        group = gic_dist->GICD_IGROUPR[bank];
        mod = gic_dist->GICD_IGRPMODR[bank];

        switch (security)
        {
            case GICV3_GROUP0:
                group = (group & ~id);
                mod = (mod & ~id);
                break;

            case GICV3_GROUP1_SECURE:
                group = (group & ~id);
                mod = (mod | id);
                break;

            case GICV3_GROUP1_NON_SECURE:
                group = (group | id);
                mod = (mod & ~id);
                break;

            default:
                ret = 2U;
                break;
        }
        if (ret == 2U)
        {
            return INTERRUPT_RETURN_INVALID_GROUP;
        }

        gic_dist->GICD_IGROUPR[bank] = group;
        gic_dist->GICD_IGRPMODR[bank] = mod;
    }
    else
    {
        /* Unknown or unsupported uID */
        return INTERRUPT_RETURN_INVALID_ID;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

#define GICV3_ROUTE_AFF3_SHIFT    (8)

/*
 * @brief Sets the target CPUs of the specified uID
 * For 'target' use one of the above defines
 * @param[in] id  INTuID of interrupt (id must be less than 1020)
 * @param[in] mode Routing mode
 * @param[in] affinity  Affinity co-ordinate of target
 */
int32_t gic_set_int_route(uint32_t id, uint32_t mode, uint32_t affinity)
{
    uint64_t tmp;

#ifdef DEBUG
    /* put debug print here */
#endif

    if (gic_dist == NULL)
    {
        return INTERRUPT_RETURN_ERROR;
    }

    /* Check for SPI ranges */
    if ((id <= 31U) || (id >= 1020U))
    {
        /* Not a GICv3.0 SPI */

        if ((id <= 4095U) || (id >= 5120U))
        {
            /* Not a GICv3.1 SPI either */
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        /* Check Ext SPI implemented */
        if (gic_is_valid_ext_spi(id) == 0U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }
    }

    id -= 32U; /* Adjust ID for Distributor registers */
    /* Combine routing in */
    tmp = (uint64_t)(affinity & 0x00FFFFFFU) |
            (((uint64_t)affinity & 0xFF000000U) << GICV3_ROUTE_AFF3_SHIFT) | (uint64_t)mode;

    if ((id > 31U) && (id < 1020U))
    {
        gic_dist->GICD_IROUTER[id] = tmp;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

/* Interrupt configuration */

int32_t gic_enable_int(uint32_t id, uint32_t rd)
{
    uint32_t bank;

    if ((gic_rdist == NULL) || (gic_dist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

    if (id < 32U)
    {
        /* Check rd in range */
        if (rd > gic_max_rd)
        {
            return INTERRUPT_RETURN_INVALID_RDIST;
        }

        if( id < 16 )
        {
            /*
             * Clearing any SGIs used to yield the core, as enabling
             * the interrupt may cause it to run a task prematurely
             */
            gic_rdist[rd].sgis.GICR_ICPENDR0 |= (1U << id);
        }
        gic_rdist[rd].sgis.GICR_ISENABLER0 = (1U << id);
    }
    else if (id < 1020U)
    {
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */
        /* SPI */
        bank = id / 32U; /* There are 32 uIDs per register, need to work out which register to access */
        id = id & 0x1fU; /* ... and which bit within the register */

        id = 1U << id;

        gic_dist->GICD_ISENABLER[bank] = id;
    }
    else
    {
#ifdef DEBUG
        ERROR("enableInt:: ERROR - Invalid or unsupported interrupt.");
#endif
        return INTERRUPT_RETURN_ERROR;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

int32_t gic_disable_int(uint32_t id, uint32_t rd)
{
    uint32_t bank;

    if ((gic_rdist == NULL) || (gic_dist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

    if (id < 31U)
    {
        //* Check rd in range */
        if (rd > gic_max_rd)
        {
            return 1;
        }
        /* SGI or PPI */
        id = id & 0x1fU;    /* ... and which bit within the register */
        id = 1U << id;      /* Move a '1' into the correct bit position */

        gic_rdist[rd].sgis.GICR_ICENABLER0 = id;
    }
    else if (id < 1020U)
    {
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */
        /* SPI */
        bank = id / 32U; /* There are 32 uIDs per register, need to work out which register to access */
        id = id & 0x1fU; /* ... and which bit within the register */

        id = 1U << id;

        gic_dist->GICD_ICENABLER[bank] = id;
    }
    else
    {
#ifdef DEBUG
        /* put debug print here */
#endif
        return INTERRUPT_RETURN_ERROR;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

int32_t gic_set_int_type(uint32_t id, uint32_t rd, uint32_t type)
{
    uint32_t bank, tmp, conf;

    if ((gic_dist == NULL) || (gic_rdist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

#ifdef DEBUG
    /* put debug print here */
#endif

    if (id < 31U)
    {
        if (id < 16U)
        {
            /* Not applicable to SGIs */
            return 0;
        }
        else
        {
            gic_rdist[rd].sgis.GICR_ICFGR[1] = (type & 0x3U) << ((id - 16U) << 1U);
        }
    }
    else if (id < 1020U)
    {
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */
        /* SPI */
        type = type & 0x3U;   /* Mask out unused bits */

        bank = id / 16U; /*There are 16 uIDs per register, need to work out which register to access */
        id = id & 0xFU;  /* ... and which field within the register */
        id = id * 2U;    /* Convert from which field to a bit offset (2-bits per field) */

        conf = type << id; /* Move configuration value into correct bit position */

        tmp = gic_dist->GICD_ICFGR[bank];       /* Read current value */
        tmp = tmp & ~(0x3U << id);              /* Clear the bits for the specified field */
        tmp = tmp | conf;                       /* OR in new configuration */
        gic_dist->GICD_ICFGR[bank] = tmp;       /* Write updated value back */
    }
    else
    {
        return INTERRUPT_RETURN_ERROR;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

int32_t gic_clear_int_pending(uint32_t id, uint32_t rd)
{
    uint32_t bank;

    if ((gic_dist == NULL) || (gic_rdist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

    if (id < 31U)
    {
        /* Check rd in range */
        if (rd > gic_max_rd)
        {
            return 1;
        }

        gic_rdist[rd].sgis.GICR_ICPENDR0 |= (1U << id);
    }
    else if (id < 1020U)
    {
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */
        /* SPI */
        bank = id / 32U; /* There are 32 uIDs per register, need to work out which register to access */
        id = id & 0x1fU; /* ... and which bit within the register */

        id = 1U << id;

        gic_dist->GICD_ICPENDR[bank] = id;
    }
    else
    {
        return INTERRUPT_RETURN_ERROR;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

/* Interrupt state */

int32_t gic_set_int_pending(uint32_t id, uint32_t rd)
{
    uint32_t bank;

    /* Adjust for SPI */
    id -= 32U;

    if ((gic_dist == NULL) || (gic_rdist == NULL))
    {
        return INTERRUPT_RETURN_ERROR;
    }

    if (id < 31U)
    {
        /* Check rd in range */
        if (rd > gic_max_rd)
        {
            return 1;
        }

        id = id & 0x1fU;    /* Find which bit within the register */
        id = 1U << id;      /* Move a '1' into the correct bit position */

        gic_rdist[rd].sgis.GICR_ISPENDR0 |= id;

    }
    else if (id < 1020U)
    {
        if (id == 31U)
        {
            return INTERRUPT_RETURN_INVALID_SPI;
        }

        id -= 32U; /* Adjust ID for Distributor registers */
        /* SPI */
        bank = id / 32U; /*There are 32 uIDs per register, need to work out which register to access */
        id = id & 0x1fU; /* ... and which bit within the register */

        id = 1U << id;    /* Move a '1' into the correct bit position */

        gic_dist->GICD_ISPENDR[bank] |= id;
    }
    else
    {
        return INTERRUPT_RETURN_ERROR;
    }

    return INTERRUPT_RETURN_SUCCESS;
}

void gic_enable_interrupts(void)
{
    __asm__ volatile ("msr DAIFClr, #0xF" ::: "memory");
}
