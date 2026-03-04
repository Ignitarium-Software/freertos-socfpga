/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * HAL driver implementation for ECC
 */

#include <stdint.h>
#include <errno.h>
#include "osal_log.h"
#include "socfpga_defines.h"
#include "socfpga_ecc.h"
#include "socfpga_sys_mngr_reg.h"
#include "socfpga_sip_handler.h"
#include "socfpga_interrupt.h"

#define ECC_OCRAM_BASE_ADDR        0x108CC000U
#define ECC_EMAC0_RX_BASE_ADDR     0x108C0000U
#define ECC_EMAC0_TX_BASE_ADDR     0x108C0400U
#define ECC_EMAC1_RX_BASE_ADDR     0x108C0800U
#define ECC_EMAC1_TX_BASE_ADDR     0x108C0C00U
#define ECC_EMAC2_RX_BASE_ADDR     0x108C1000U
#define ECC_EMAC2_TX_BASE_ADDR     0x108C1400U
#define ECC_USB0_RAM0_BASE_ADDR    0x108C4000U
#define ECC_USB1_RAM0_BASE_ADDR    0x108C4400U
#define ECC_USB1_RAM1_BASE_ADDR    0x108C4800U
#define ECC_USB1_RAM2_BASE_ADDR    0x108C4C00U
#define ECC_QSPI_BASE_ADDR         0x10A22000U

#define ECC_CTRL             0x8U
#define ECC_INIT_STAT        0xCU
#define ECC_ERR_INT_EN       0x10U
#define ECC_ERR_INT_SET      0x14U
#define ECC_ERR_INT_RESET    0x18U
#define ECC_INT_MODE         0x1CU
#define ECC_INT_STAT         0x20U
#define ECC_INT_TEST         0x24U
#define ECC_D_ERR_ADDR_A     0x2CU
#define ECC_S_ERR_ADDR_A     0x30U
#define ECC_D_ERR_ADDR_B     0x34U
#define ECC_S_ERR_ADDR_B     0x38U
#define ECC_S_ERR_CNT        0x3CU

#define ECC_PENDING_ERROR_A_MASK    ((1U << 0U) | (1U << 8U))
#define ECC_PENDING_ERROR_B_MASK    ((1U << 16U) | (1U << 24U))
#define ECC_PENDING_SB_ERR_MASK     ((1U << 0U) | (1U << 16U))
#define ECC_PENDING_DB_ERR_MASK     ((1U << 8U) | (1U << 24U))
#define ECC_SBERR_PORTA_MASK        (1U << 0U)
#define ECC_DBERR_PORTA_MASK        (1U << 8U)
/* List of supported modules */
#define ECC_MAX_INSTANCES       13U
#define ECC_MODULES_MASK        (((1U) << (ECC_MAX_INSTANCES)) - 2U)
#define ECC_MEM_WAIT_TIMEOUT    10000U

#define SMC_SECURE_REG_WR    0xC2000008U
#define SMC_SECURE_REG_RD    0xC2000007U

#define SMC_WR_REG32(address, val)    ecc_smc_io_write32(address, val)
#define SMC_RD_REG32(address)         ecc_smc_io_read32(address)

typedef struct
{
    uint32_t base_addr;
    uint32_t sbe_err_cnt;
    uint32_t dbe_err_cnt;
} ecc_blk_data_t;

typedef struct
{
    ecc_blk_data_t ecc_instances[ECC_MAX_INSTANCES];
    uint32_t module_init;
    ecc_callback_t user_cb;
} ecc_handle_t;

ecc_handle_t hecc;

static inline void ecc_smc_io_write32(uint32_t addr, uint32_t val)
{
    uint64_t smc_args[8] = {0};
    int ret;
    smc_args[0] = addr;
    smc_args[1] = val;
    ret = smc_call(SMC_SECURE_REG_WR, smc_args);
    if (ret != 0)
    {
        ERROR("SMC write failed (%d)", ret);
    }
}

static inline uint32_t ecc_smc_io_read32(uint32_t addr)
{
    uint64_t smc_args[8] = {0};
    smc_args[0] = addr;
    smc_call(SMC_SECURE_REG_RD, smc_args);
    return (uint32_t)smc_args[0];
}

static void ecc_process_sbe(uint32_t module_id)
{
    uint32_t err_addr;
    uint32_t sbe_status = 0U;
    uint32_t base_addr = hecc.ecc_instances[module_id].base_addr;

    sbe_status = RD_REG32(base_addr + ECC_INT_STAT);
    WR_REG32(base_addr + ECC_INT_STAT, (1U << 0U));
    if (sbe_status & ECC_PENDING_SB_ERR_MASK)
    {
        if (sbe_status & ECC_SBERR_PORTA_MASK)
        {
            err_addr = RD_REG32(base_addr + ECC_S_ERR_ADDR_A);
            DEBUG("Single bit error detected on PORTA");
            DEBUG("Module: %d", module_id);
            DEBUG("Error Address: 0x%08X", err_addr);
        }
#if ECC_DUAL_PORT
        if (sbe_status & (1U << 16))
        {
            WR_REG32(base_addr + ECC_INT_STAT, (1U << 16U));
            err_addr = RD_REG32(base_addr + ECC_S_ERR_ADDR_B);
            DEBUG("Single bit error detected on PORTB");
            DEBUG("Module: %d", module_id);
            DEBUG("Error Address: 0x%08X", err_addr);
        }
#endif
    /*
     * For log levels lower than DEBUG, this will throw an unused variable
     * error.
     */
    (void)err_addr;
    }
}
static void ecc_process_dbe(uint32_t module_id)
{
    uint32_t err_addr;
    uint32_t dbe_status = 0U;
    uint32_t base_addr = hecc.ecc_instances[module_id].base_addr;
    dbe_status = RD_REG32(base_addr + ECC_INT_STAT);
    WR_REG32(base_addr + ECC_INT_STAT, (1U << 8U));
    if (dbe_status & ECC_PENDING_DB_ERR_MASK)
    {
        if ((dbe_status & (1U << 8U)) != 0U)
        {
            err_addr = RD_REG32(base_addr + ECC_D_ERR_ADDR_A);
            DEBUG("Double bit error detected on PORTA");
            DEBUG("Module: %d", module_id);
            DEBUG("Error Address: 0x%08X", err_addr);
        }
#if ECC_DUAL_PORT
        if ((dbe_status & (1U << 24U)) != 0U)
        {
            WR_REG32(base_addr + ECC_INT_STAT, (1U << 24U));
            err_addr = RD_REG32(base_addr + ECC_D_ERR_ADDR_B);
            DEBUG("Double bit error detected on PORTB");
            DEBUG("Module: %d", module_id);
            DEBUG("Error Address: 0x%08X", err_addr);
        }
#endif
    }
    (void)err_addr;
}
/*
 * For QSPI ECC all register access is performed via SMC calls, these
 * registers are secure registers and can all be accessed through the
 * ATF using secure register operations.
 */
void ecc_qspi_irq_handler(void *param)
{
    (void)param;

    uint32_t err_status = 0U;
    uint32_t base_addr = hecc.ecc_instances[ECC_QSPI].base_addr;
    err_status = SMC_RD_REG32(base_addr + ECC_INT_STAT);
    if ((err_status & 1U) != 0U)
    {
        SMC_WR_REG32(base_addr + ECC_INT_STAT, ECC_SBERR_PORTA_MASK);
        if (hecc.user_cb != NULL)
        {
            hecc.user_cb(ECC_SINGLE_BIT_ERROR);
        }
        hecc.ecc_instances[ECC_QSPI].sbe_err_cnt++;
        return;
    }
    if ((err_status & (1U << 8U)) != 0U)
    {
        SMC_WR_REG32(base_addr + ECC_INT_STAT, ECC_DBERR_PORTA_MASK);
        if (hecc.user_cb != NULL)
        {
            hecc.user_cb(ECC_DOUBLE_BIT_ERROR);
        }
        hecc.ecc_instances[ECC_QSPI].dbe_err_cnt++;
        return;
    }
}
void ecc_irq_handler(void *param)
{
    (void)param;
    uint32_t serr_status = 0U, derr_status = 0U;

    derr_status = SMC_RD_REG32(
            SYS_MNGR_BASE_ADDR + SYS_MNGR_ECC_INTSTATUS_DERR);
    /* ignoring DDR errors */
    derr_status &= ECC_MODULES_MASK;
    if (derr_status != 0U)
    {
        PRINT("Double bit error detected: %x", derr_status);
        for (uint32_t i = 0U; i < ECC_MAX_INSTANCES; i++)
        {
            if ((derr_status & (1U << i)) != 0U)
            {
                ecc_process_dbe(i);
                hecc.ecc_instances[i].dbe_err_cnt++;
            }
        }
        if (hecc.user_cb != NULL)
        {
            hecc.user_cb(ECC_DOUBLE_BIT_ERROR);
        }
    }

    serr_status = RD_REG32(
            SYS_MNGR_BASE_ADDR + SYS_MNGR_ECC_INTSTATUS_SERR);
    /* ignoring DDR errors */
    serr_status &= ECC_MODULES_MASK;
    if (serr_status != 0U)
    {
        PRINT("Single bit error detected: %x", serr_status);
        for (uint32_t i = 0U; i < ECC_MAX_INSTANCES; i++)
        {
            if ((serr_status & (1U << i)) != 0U)
            {
                ecc_process_sbe(i);
                hecc.ecc_instances[i].sbe_err_cnt++;
            }
        }
        if (hecc.user_cb != NULL)
        {
            hecc.user_cb(ECC_SINGLE_BIT_ERROR);
        }
    }
}
/*
 * Providing a different implementation for enabling QSPI module as
 * the register access should be done using secure register read and
 * write
 */
static int ecc_enable_qspi(void)
{
    uint32_t reg_val, retry_count = 0;
    uint32_t base_addr = hecc.ecc_instances[ECC_QSPI].base_addr;
    /* Disable single bit error interrupt */
    SMC_WR_REG32(base_addr + ECC_ERR_INT_RESET, 1U);

    /* Disable ECC correction and detection */
    reg_val = SMC_RD_REG32(base_addr + ECC_CTRL);
    reg_val &= ~(1U << 0);
    SMC_WR_REG32(base_addr + ECC_CTRL, reg_val);

    /* Start hardware memory initialization PORTA */
    reg_val = SMC_RD_REG32(base_addr + ECC_CTRL);
    reg_val |= (1U << 16);
    SMC_WR_REG32(base_addr + ECC_CTRL, reg_val);

    /* Wait for PORTA memory initialization */
    while ((SMC_RD_REG32(base_addr + ECC_INIT_STAT) & 1U) == 0U)
    {
        retry_count++;
        if (retry_count >= ECC_MEM_WAIT_TIMEOUT)
        {
            ERROR("Module %x: PORT A timeout", ECC_QSPI);
            ERROR("Failed to initialize ECC module %x", ECC_QSPI);
            return -EIO;
        }
    }
    /* Enabling ECC detection and correction */
    reg_val = SMC_RD_REG32(base_addr + ECC_CTRL);
    reg_val |= (1U << 0U);
    SMC_WR_REG32(base_addr + ECC_CTRL, reg_val);
    reg_val = SMC_RD_REG32(base_addr + ECC_CTRL);

    /* Enable single bit error interrupt */
    SMC_WR_REG32(base_addr + ECC_ERR_INT_EN, 1U);

    hecc.module_init |= (1U << ECC_QSPI);
    return 0;
}
int ecc_enable_modules(uint32_t modules)
{
    uint32_t reg_val, base_addr;
    uint32_t retry_count = 0;

    if ((modules & ECC_MODULES_MASK) == 0U)
    {
        ERROR("Invalid ECC module list");
        return -EINVAL;
    }
    for (uint32_t i = 0U; i < ECC_MAX_INSTANCES; i++)
    {
        if ((modules & (1U << i)) == 0U)
        {
            /* Module not enabled */
            continue;
        }
        if ((hecc.module_init & (1U << i)) != 0U)
        {
            ERROR("ECC module %u already initialized", (unsigned int)i);
            return -EINVAL;
        }
        base_addr = hecc.ecc_instances[i].base_addr;
        if (base_addr == 0U)
        {
            ERROR("ECC not initialized");
            return -EINVAL;
        }
        if (i == ECC_QSPI)
        {
            return ecc_enable_qspi();
        }
        /* Disable single bit error interrupt */
        WR_REG32(base_addr + ECC_ERR_INT_RESET, 1U);

        /* Disable ECC correction and detection */
        reg_val = RD_REG32(base_addr + ECC_CTRL);
        reg_val &= ~(1U << 0U);
        WR_REG32(base_addr + ECC_CTRL, reg_val);

        /* Start hardware memory initialization PORTA */
        reg_val = RD_REG32(base_addr + ECC_CTRL);
        reg_val |= (1U << 16U);
        WR_REG32(base_addr + ECC_CTRL, reg_val);

        /* Wait for PORTA memory initialization */
        while ((RD_REG32(base_addr + ECC_INIT_STAT) & 1U) == 0U)
        {
            retry_count++;
            if (retry_count >= ECC_MEM_WAIT_TIMEOUT)
            {
                ERROR("Module %x: PORT A timeout", (1U << i));
                ERROR("Failed to initialize ECC module %x", (1U << i));
                return -EIO;
            }
        }
        /* Clear pending interrupt */
        WR_REG32(base_addr + ECC_INIT_STAT, ECC_PENDING_ERROR_A_MASK);

    #if ECC_DUAL_PORT
        /* Start hardware memory initialization PORTB */
        reg_val = SMC_RD_REG32(base_addr + ECC_CTRL);
        reg_val |= (1U << 24U);
        WR_REG32(base_addr + ECC_CTRL, reg_val);

        /* Wait for PORTA memory initialization */
        while ((RD_REG32(base_addr + ECC_INIT_STAT) & (1U << 8U)) == 0U)
        {
            retry_count++;
            if (retry_count >= ECC_MEM_WAIT_TIMEOUT)
            {
                ERROR("Module %x: PORT B timeout", (1U << i));
                return -EIO;
            }
        }
        /* Clear pending interrupt */
        WR_REG32(base_addr + ECC_INIT_STAT, ECC_PENDING_ERROR_B_MASK);
    #endif

        /*Set max error count value*/
        WR_REG32(base_addr + ECC_S_ERR_CNT, 1U);

        /* Enable interrupt on distinct error */
        reg_val = RD_REG32(base_addr + ECC_INT_MODE);
        reg_val |= (1U << 0U);
        WR_REG32(base_addr + ECC_INT_MODE, reg_val);

        /*Enabling ECC detection and correction*/
        reg_val = RD_REG32(base_addr + ECC_CTRL);
        reg_val |= (1U << 0U);
        WR_REG32(base_addr + ECC_CTRL, reg_val);

        /*Enable single bit error interrupt */
        WR_REG32(base_addr + ECC_ERR_INT_SET, 1U);

        /* Enable ECC interrupts for initialized modules */
        SMC_WR_REG32(SYS_MNGR_BASE_ADDR + SYS_MNGR_ECC_INTMASK_CLR, (1U << i));

        hecc.module_init |= (1U << i);
    }
    return 0;
}

int ecc_init(void)
{
    BaseType_t ret;

    /* Disabling ECC interrupts */
    SMC_WR_REG32(SYS_MNGR_BASE_ADDR + SYS_MNGR_ECC_INTMASK_SET, 0x7FFFEU);

    /* Single bit error interrupt */
    ret = interrupt_register_isr(SERR_GLOBAL, ecc_irq_handler, NULL);
    if (ret != 0)
    {
        return -EIO;
    }
    ret = interrupt_enable(SERR_GLOBAL, GIC_INTERRUPT_PRIORITY_EDAC);
    if (ret != 0)
    {
        return -EIO;
    }

    /* Single bit error interrupt for QSPI */
    ret = interrupt_register_isr(SDM_HPS_SPARE_INTR1, ecc_qspi_irq_handler, NULL);
    if (ret != 0)
    {
        return -EIO;
    }
    ret = interrupt_enable(SDM_HPS_SPARE_INTR1, GIC_INTERRUPT_PRIORITY_EDAC);
    if (ret != 0)
    {
        return -EIO;
    }

    /* Double bit error interrupt */
    ret = interrupt_register_isr(ECC_DERR_INTR_N, ecc_irq_handler, NULL);
    if (ret != 0)
    {
        return -EIO;
    }
    ret = interrupt_enable(ECC_DERR_INTR_N, GIC_INTERRUPT_PRIORITY_EDAC);
    if (ret != 0)
    {
        return -EIO;
    }

    /* Double bit error interrupt for QSPI */
    ret = interrupt_register_isr(SDM_HPS_SPARE_INTR2, ecc_qspi_irq_handler, NULL);
    if (ret != 0)
    {
        return -EIO;
    }
    ret = interrupt_enable(SDM_HPS_SPARE_INTR2, GIC_INTERRUPT_PRIORITY_EDAC);
    if (ret != 0)
    {
        return -EIO;
    }
    /* Initialising all ECC modules */
    hecc.ecc_instances[ECC_OCRAM].base_addr = ECC_OCRAM_BASE_ADDR;
    hecc.ecc_instances[ECC_EMAC0_RX].base_addr = ECC_EMAC0_RX_BASE_ADDR;
    hecc.ecc_instances[ECC_EMAC0_TX].base_addr = ECC_EMAC0_TX_BASE_ADDR;
    hecc.ecc_instances[ECC_EMAC1_RX].base_addr = ECC_EMAC1_RX_BASE_ADDR;
    hecc.ecc_instances[ECC_EMAC1_TX].base_addr = ECC_EMAC1_TX_BASE_ADDR;
    hecc.ecc_instances[ECC_EMAC2_RX].base_addr = ECC_EMAC2_RX_BASE_ADDR;
    hecc.ecc_instances[ECC_EMAC2_TX].base_addr = ECC_EMAC2_TX_BASE_ADDR;
    hecc.ecc_instances[ECC_QSPI].base_addr = ECC_QSPI_BASE_ADDR;
    hecc.ecc_instances[ECC_USB0_RAM0].base_addr = ECC_USB0_RAM0_BASE_ADDR;
    hecc.ecc_instances[ECC_USB1_RAM0].base_addr = ECC_USB1_RAM0_BASE_ADDR;
    hecc.ecc_instances[ECC_USB1_RAM1].base_addr = ECC_USB1_RAM1_BASE_ADDR;
    hecc.ecc_instances[ECC_USB1_RAM2].base_addr = ECC_USB1_RAM2_BASE_ADDR;

    return 0;
}

int ecc_set_callback(ecc_callback_t user_callback)
{
    if (user_callback == NULL)
    {
        ERROR("User callback cannot be NULL");
        return -EINVAL;
    }
    hecc.user_cb = user_callback;
    return 0;
}

int ecc_inject_error(uint32_t ecc_module, uint32_t error_type)
{
    uint32_t base_addr;

    if (ecc_module >= ECC_MAX_INSTANCES)
    {
        ERROR("Invalid ECC module %u", (unsigned int)ecc_module);
        return -EINVAL;
    }

    base_addr = hecc.ecc_instances[ecc_module].base_addr;
    if (base_addr == 0U)
    {
        ERROR("ECC module %d not initialized", ecc_module);
        return -EINVAL;
    }
    /* QSPI error injection should be done through secure register operations */
    if (ecc_module == ECC_QSPI)
    {
        switch (error_type)
        {
            case ECC_SINGLE_BIT_ERROR:
                SMC_WR_REG32((base_addr + ECC_INT_TEST), (1U << 0));
                break;

            case ECC_DOUBLE_BIT_ERROR:
                SMC_WR_REG32((base_addr + ECC_INT_TEST), (1U << 8));
                break;
            default:
                ERROR("Invalid error type: %d", error_type);
                return -EINVAL;
        }
        return 0;
    }
    switch (error_type)
    {
        case ECC_SINGLE_BIT_ERROR:
            WR_REG16((base_addr + ECC_INT_TEST), (1U << 0));
            break;

        case ECC_DOUBLE_BIT_ERROR:
            WR_REG16((base_addr + ECC_INT_TEST), (1U << 8));
            break;
        default:
            ERROR("Invalid error type: %d", error_type);
            return -EINVAL;
    }
    return 0;
}

int ecc_get_sbe_error_count(uint32_t ecc_module)
{
    if ((ecc_module >= ECC_MAX_INSTANCES) ||
            !(hecc.module_init & (1U << ecc_module)))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }
    return hecc.ecc_instances[ecc_module].sbe_err_cnt;
}
int ecc_get_dbe_error_count(uint32_t ecc_module)
{
    if ((ecc_module >= ECC_MAX_INSTANCES) ||
            !(hecc.module_init & (1U << ecc_module)))
    {
        ERROR("Invalid parameters");
        return -EINVAL;
    }
    return hecc.ecc_instances[ecc_module].dbe_err_cnt;
}
