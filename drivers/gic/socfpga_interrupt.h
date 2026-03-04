/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for interrupt related APIs for SoC FPGA
 */


#ifndef __SOCFPGA_INTERRUPT_H__
#define __SOCFPGA_INTERRUPT_H__

/**
 * @file socfpga_interrupt.h
 * @brief File for the HAL APIs of Interrupt controller.
 */

#include <stdint.h>
#include "socfpga_interrupt_priority.h"

/**
 * @defgroup intr GIC
 * @ingroup drivers
 * @brief APIs for GIC interrupt controller.
 * @details
 * This is the GIC driver implementation for SoC FPGA.
 * It provides the APIs for enabling and disabling interrupts,
 * registering interrupt handlers, and configuring interrupt priorities for
 * both shared and private peripheral interrupts.
 * @{
 */

/**
 * @defgroup intr_fns Functions
 * @ingroup intr
 * GIC APIs
 */

/**
 * @defgroup intr_enums Enumerations
 * @ingroup intr
 * GIC specific Enumerations
 */

/**
 * @defgroup intr_macros Macros
 * @ingroup intr
 * GIC specific Macros
 */

/**
 * @addtogroup intr_macros
 * @{
 */
#define interrupt_min_interrupt_priority    14 /*!< Minimum interrupt priority for SoC FPGA.*/
#define MAX_SPI_HPU_INTERRUPT    274U /*!< Maximum number of interrupts*/
/** @} */

/**
 * @brief Interrupt IDs for SoC FPGA blocks.
 * @ingroup intr_enums
 */
typedef enum {
    /* System SGI */
    YIELD_CORE_INTR,
    /*System PPIs*/
    PPI_START = 22, /*!<Start of Private Peripheral Interface (PPI) interrupts*/
    EL1VIRT_TMR_INTR = 27, /*!< EL1 Phy Timer Interrupt */
    EL1PHY_TMR_INTR = 30, /*!< EL1 Phy Timer Interrupt */
    PPI_MAX = 30, /*!< Maximum PPI interrupts */

    /*SDM Block interrupts*/
    SDM_APS_MAILBOX_INTR = 32, /*!< SDM APS Mailbox Interrupt */
    SDM_I2C_INTR0, /*!< SDM I2C Interrupt 0 */
    SDM_I2C_INTR1, /*!< SDM I2C Interrupt 1 */
    SDM_QSPI_INTR, /*!< SDM QSPI Interrupt */

    SDM_PWR_ALERT_INTR = 38, /*!< SDM Power Alert Interrupt */
    SDM_HPS_SPARE_INTR0, /*!< SDM HPS Spare Interrupt 0 */
    SDM_HPS_SPARE_INTR1, /*!< SDM HPS Spare Interrupt 1 */
    SDM_HPS_SPARE_INTR2, /*!< SDM HPS Spare Interrupt 2 */

    /*Secure manager block interrupts*/
    SECMGR_TRIPLE_REDUN_ERROR = 46, /*!< Triple Redundancy Error Interrupt */
    SERR_GLOBAL, /*!< Global Secure Error Interrupt */

    /*CCU block interrupts*/
    INTERRUPT_CCU, /*!< Cache Coherency Unit Interrupt */

    /*FPGA to HPS interrupts*/
    FPGA2HPS_INTERRUPT0, /*!< FPGA to HPS Interrupt 0 */
    FPGA2HPS_INTERRUPT1, /*!< FPGA to HPS Interrupt 1 */
    FPGA2HPS_INTERRUPT2, /*!< FPGA to HPS Interrupt 2 */
    FPGA2HPS_INTERRUPT3, /*!< FPGA to HPS Interrupt 3 */
    FPGA2HPS_INTERRUPT4, /*!< FPGA to HPS Interrupt 4 */
    FPGA2HPS_INTERRUPT5, /*!< FPGA to HPS Interrupt 5 */
    FPGA2HPS_INTERRUPT6, /*!< FPGA to HPS Interrupt 6 */
    FPGA2HPS_INTERRUPT7, /*!< FPGA to HPS Interrupt 7 */
    FPGA2HPS_INTERRUPT8, /*!< FPGA to HPS Interrupt 8 */
    FPGA2HPS_INTERRUPT9, /*!< FPGA to HPS Interrupt 9 */
    FPGA2HPS_INTERRUPT10, /*!< FPGA to HPS Interrupt 10 */
    FPGA2HPS_INTERRUPT11, /*!< FPGA to HPS Interrupt 11 */
    FPGA2HPS_INTERRUPT12, /*!< FPGA to HPS Interrupt 12 */
    FPGA2HPS_INTERRUPT13, /*!< FPGA to HPS Interrupt 13 */
    FPGA2HPS_INTERRUPT14, /*!< FPGA to HPS Interrupt 14 */
    FPGA2HPS_INTERRUPT15, /*!< FPGA to HPS Interrupt 15 */
    FPGA2HPS_INTERRUPT16, /*!< FPGA to HPS Interrupt 16 */
    FPGA2HPS_INTERRUPT17, /*!< FPGA to HPS Interrupt 17 */
    FPGA2HPS_INTERRUPT18, /*!< FPGA to HPS Interrupt 18 */
    FPGA2HPS_INTERRUPT19, /*!< FPGA to HPS Interrupt 19 */
    FPGA2HPS_INTERRUPT20, /*!< FPGA to HPS Interrupt 20 */
    FPGA2HPS_INTERRUPT21, /*!< FPGA to HPS Interrupt 21 */
    FPGA2HPS_INTERRUPT22, /*!< FPGA to HPS Interrupt 22 */
    FPGA2HPS_INTERRUPT23, /*!< FPGA to HPS Interrupt 23 */
    FPGA2HPS_INTERRUPT24, /*!< FPGA to HPS Interrupt 24 */
    FPGA2HPS_INTERRUPT25, /*!< FPGA to HPS Interrupt 25 */
    FPGA2HPS_INTERRUPT26, /*!< FPGA to HPS Interrupt 26 */
    FPGA2HPS_INTERRUPT27, /*!< FPGA to HPS Interrupt 27 */
    FPGA2HPS_INTERRUPT28, /*!< FPGA to HPS Interrupt 28 */
    FPGA2HPS_INTERRUPT29, /*!< FPGA to HPS Interrupt 29 */
    FPGA2HPS_INTERRUPT30, /*!< FPGA to HPS Interrupt 30 */
    FPGA2HPS_INTERRUPT31, /*!< FPGA to HPS Interrupt 31 */
    FPGA2HPS_INTERRUPT32, /*!< FPGA to HPS Interrupt 32 */
    FPGA2HPS_INTERRUPT33, /*!< FPGA to HPS Interrupt 33 */
    FPGA2HPS_INTERRUPT34, /*!< FPGA to HPS Interrupt 34 */
    FPGA2HPS_INTERRUPT35, /*!< FPGA to HPS Interrupt 35 */
    FPGA2HPS_INTERRUPT36, /*!< FPGA to HPS Interrupt 36 */
    FPGA2HPS_INTERRUPT37, /*!< FPGA to HPS Interrupt 37 */
    FPGA2HPS_INTERRUPT38, /*!< FPGA to HPS Interrupt 38 */
    FPGA2HPS_INTERRUPT39, /*!< FPGA to HPS Interrupt 39 */
    FPGA2HPS_INTERRUPT40, /*!< FPGA to HPS Interrupt 40 */
    FPGA2HPS_INTERRUPT41, /*!< FPGA to HPS Interrupt 41 */
    FPGA2HPS_INTERRUPT42, /*!< FPGA to HPS Interrupt 42 */
    FPGA2HPS_INTERRUPT43, /*!< FPGA to HPS Interrupt 43 */
    FPGA2HPS_INTERRUPT44, /*!< FPGA to HPS Interrupt 44 */
    FPGA2HPS_INTERRUPT45, /*!< FPGA to HPS Interrupt 45 */
    FPGA2HPS_INTERRUPT46, /*!< FPGA to HPS Interrupt 46 */
    FPGA2HPS_INTERRUPT47, /*!< FPGA to HPS Interrupt 47 */
    FPGA2HPS_INTERRUPT48, /*!< FPGA to HPS Interrupt 48 */
    FPGA2HPS_INTERRUPT49, /*!< FPGA to HPS Interrupt 49 */
    FPGA2HPS_INTERRUPT50, /*!< FPGA to HPS Interrupt 50 */
    FPGA2HPS_INTERRUPT51, /*!< FPGA to HPS Interrupt 51 */
    FPGA2HPS_INTERRUPT52, /*!< FPGA to HPS Interrupt 52 */
    FPGA2HPS_INTERRUPT53, /*!< FPGA to HPS Interrupt 53 */
    FPGA2HPS_INTERRUPT54, /*!< FPGA to HPS Interrupt 54 */
    FPGA2HPS_INTERRUPT55, /*!< FPGA to HPS Interrupt 55 */
    FPGA2HPS_INTERRUPT56, /*!< FPGA to HPS Interrupt 56 */
    FPGA2HPS_INTERRUPT57, /*!< FPGA to HPS Interrupt 57 */
    FPGA2HPS_INTERRUPT58, /*!< FPGA to HPS Interrupt 58 */
    FPGA2HPS_INTERRUPT59, /*!< FPGA to HPS Interrupt 59 */
    FPGA2HPS_INTERRUPT60, /*!< FPGA to HPS Interrupt 60 */
    FPGA2HPS_INTERRUPT61, /*!< FPGA to HPS Interrupt 61 */
    FPGA2HPS_INTERRUPT62, /*!< FPGA to HPS Interrupt 62 */
    FPGA2HPS_INTERRUPT63, /*!< FPGA to HPS Interrupt 63 */

    /*DMA0 interrupts*/
    DMA_IRQ0, /*!< DMA0 Interrupt 0 */
    DMA_IRQ1, /*!< DMA0 Interrupt 1 */
    DMA_IRQ2, /*!< DMA0 Interrupt 2 */
    DMA_IRQ3, /*!< DMA0 Interrupt 3 */
    DMA0COMMON_IRQ, /*!< DMA0 Common Interrupt */
    DMA0COMBINED_IRQ, /*!< DMA0 Combined Interrupt */

    /*USB interrupts*/
    USB_HOST_SYSTEM_ERR_IRQ = 124, /*!< USB Host System Error Interrupt */
    USB0IRQ, /*!< USB0 Interrupt */
    USB1IRQ, /*!< USB1 Interrupt */

    /*MPFE interrupt*/
    IO96B0_DBE_IRQ, /*!< IO96B0 Double Bit Error Interrupt */

    /*SDMMC/NAND interrupts*/
    SDMMC_IRQ, /*!< SDMMC Interrupt */
    NAND_IRQ, /*!< NAND Interrupt */
    NAND_SYS_WAKE_IRQ, /*!< NAND System Wake Interrupt */

    /*SPI interrupts*/
    SPI0IRQ, /*!< SPI0 Interrupt */
    SPI1IRQ, /*!< SPI1 Interrupt */
    SPI2IRQ, /*!< SPI2 Interrupt */
    SPI3IRQ, /*!< SPI3 Interrupt */

    /*I2C interrupts*/
    I2C0IRQ, /*!< I2C0 Interrupt */
    I2C1IRQ, /*!< I2C1 Interrupt */
    I2C2IRQ, /*!< I2C2 Interrupt */
    I2C3IRQ, /*!< I2C3 Interrupt */
    I2C4IRQ, /*!< I2C4 Interrupt */

    /*UART interrupts*/
    UART0IRQ, /*!< UART0 Interrupt */
    UART1IRQ, /*!< UART1 Interrupt */

    /*GPIO interrupts*/
    GPIO0IRQ, /*!< GPIO0 Interrupt */
    GPIO1IRQ, /*!< GPIO1 Interrupt */

    /*Timer interrupts*/
    TIMER_L4SP0IRQ = 145, /*!< Timer L4SP0 Interrupt */
    TIMER_L4SP1IRQ, /*!< Timer L4SP1 Interrupt */
    TIMER_OSC10IRQ, /*!< Timer OSC10 Interrupt */
    TIMER_OSC11IRQ, /*!< Timer OSC11 Interrupt */

    /*Watchdog 0 interrupts*/
    WDOG0IRQ, /*!< Watchdog 0 Interrupt */
    WDOG1IRQ, /*!< Watchdog 1 Interrupt */

    /*Clock manager interrupt*/
    CLKMGR_IRQ, /*!< Clock Manager Interrupt */

    /*MPFE block interrupts*/
    IO96B1DBE_IRQ, /*!< IO96B1 Double Bit Error Interrupt */

    /*Watchdog 2/3 interrupts*/
    WDOG2IRQ = 157, /*!< Watchdog 2 Interrupt */
    WDOG3IRQ, /*!< Watchdog 3 Interrupt */

    /*SMMU interrupts*/
    SYS_TCU_GLOBAL_IRPT_S = 160, /*!< System TCU Global Interrupt Secure */
    SYS_TCU_GLOBAL_IRPT_NS, /*!< System TCU Global Interrupt Non-Secure */
    SYS_TCU_CMD_SYNC_IRPT_S, /*!< System TCU Command Sync Interrupt Secure */
    eesystcucmdsyncirptns, /*!< System TCU Command Sync Interrupt Non-Secure */
    SYS_TCU_PRI_Q_IRPT_NS, /*!< System TCU Priority Queue Interrupt Non-Secure */
    SYS_TCU_EVENT_Q_IRPT_S, /*!< System TCU Event Queue Interrupt Secure */
    SYS_TCU_EVENT_Q_IRPT_NS, /*!< System TCU Event Queue Interrupt Non-Secure */
    SYS_TCU_RAS_IRPT, /*!< System TCU RAS Interrupt */
    SYS_TCU_PMU_IRPT, /*!< System TCU PMU Interrupt */
    F2SOC_TBU_RAS_IRPT, /*!< FPGA to SoC TBU RAS Interrupt */
    F2SOC_TBU_PMU_IRPT, /*!< FPGA to SoC TBU PMU Interrupt */
    TSN_TBU_RAS_IRPT, /*!< TSN TBU RAS Interrupt */
    TSN_TBU_PMU_IRPT, /*!< TSN TBU PMU Interrupt */
    IO_TBU_RAS_IRPT, /*!< IO TBU RAS Interrupt */
    IO_TBU_PMU_IRPT, /*!< IO TBU PMU Interrupt */
    DMA_TBU_RAS_IRPT, /*!< DMA TBU RAS Interrupt */
    DMA_TBU_PMU_IRPT, /*!< DMA TBU PMU Interrupt */
    SDM_TBU_RAS_IRPT, /*!< SDM TBU RAS Interrupt */
    SDM_TBU_PMU_IRPT, /*!< SDM TBU PMU Interrupt */
    F2SDRAM_TBU_RAS_IRPT, /*!< FPGA to SDRAM TBU RAS Interrupt */
    F2SDRAM_TBU_PMU_IRPT, /*!< FPGA to SDRAM TBU PMU Interrupt */

    /*MPU block interrupt*/
    ETR_BUFINTR = 195, /*!< ETR Buffer Interrupt */

    /*I3C interrupts*/
    I3C0IRQ, /*!< I3C0 Interrupt */
    I3C1IRQ, /*!< I3C1 Interrupt */

    /*DMA interrupts*/
    DMA1IRQ0, /*!< DMA1 Interrupt 0 */
    DMA1IRQ1, /*!< DMA1 Interrupt 1 */
    DMA1IRQ2, /*!< DMA1 Interrupt 2 */
    DMA1IRQ3, /*!< DMA1 Interrupt 3 */
    DMA1COMMON_IRQ, /*!< DMA1 Common Interrupt */
    DMA1COMBINED_IRQ, /*!< DMA1 Combined Interrupt */

    /*Watchdog 4 interrupt*/
    WDOG4IRQ = 207, /*!< Watchdog 4 Interrupt */

    /*MPU block interrupt*/
    NCLUSTERPMUIRQ, /*!< Non-Cluster PMU Interrupt */
    NFAULTIRQ0, /*!< Non-Fault Interrupt 0 */
    NFAULTIRQ1, /*!< Non-Fault Interrupt 1 */
    NFAULTIRQ2, /*!< Non-Fault Interrupt 2 */
    NFAULTIRQ3, /*!< Non-Fault Interrupt 3 */
    NFAULTIRQ4, /*!< Non-Fault Interrupt 4 */
    NERRIRQ0, /*!< Non-Error Interrupt 0 */
    NERRIRQ1, /*!< Non-Error Interrupt 1 */
    NERRIRQ2, /*!< Non-Error Interrupt 2 */
    NERRIRQ3, /*!< Non-Error Interrupt 3 */
    NERRIRQ4, /*!< Non-Error Interrupt 4 */

    /*EMAC block interrupts*/
    EMAC0IRQ = 222, /*!< EMAC0 Interrupt */
    EMAC0TX_IRQ0, /*!< EMAC0 TX Interrupt 0 */
    EMAC0TX_IRQ1, /*!< EMAC0 TX Interrupt 1 */
    EMAC0TX_IRQ2, /*!< EMAC0 TX Interrupt 2 */
    EMAC0TX_IRQ3, /*!< EMAC0 TX Interrupt 3 */
    EMAC0TX_IRQ4, /*!< EMAC0 TX Interrupt 4 */
    EMAC0TX_IRQ5, /*!< EMAC0 TX Interrupt 5 */
    EMAC0TX_IRQ6, /*!< EMAC0 TX Interrupt 6 */
    EMAC0TX_IRQ7, /*!< EMAC0 TX Interrupt 7 */
    EMAC0RX_IRQ0, /*!< EMAC0 RX Interrupt 0 */
    EMAC0RX_IRQ1, /*!< EMAC0 RX Interrupt 1 */
    EMAC0RX_IRQ2, /*!< EMAC0 RX Interrupt 2 */
    EMAC0RX_IRQ3, /*!< EMAC0 RX Interrupt 3 */
    EMAC0RX_IRQ4, /*!< EMAC0 RX Interrupt 4 */
    EMAC0RX_IRQ5, /*!< EMAC0 RX Interrupt 5 */
    EMAC0RX_IRQ6, /*!< EMAC0 RX Interrupt 6 */
    EMAC0RX_IRQ7, /*!< EMAC0 RX Interrupt 7 */
    EMAC1IRQ, /*!< EMAC1 Interrupt */
    EMAC1TX_IRQ0, /*!< EMAC1 TX Interrupt 0 */
    EMAC1TX_IRQ1, /*!< EMAC1 TX Interrupt 1 */
    EMAC1TX_IRQ2, /*!< EMAC1 TX Interrupt 2 */
    EMAC1TX_IRQ3, /*!< EMAC1 TX Interrupt 3 */
    EMAC1TX_IRQ4, /*!< EMAC1 TX Interrupt 4 */
    EMAC1TX_IRQ5, /*!< EMAC1 TX Interrupt 5 */
    EMAC1TX_IRQ6, /*!< EMAC1 TX Interrupt 6 */
    EMAC1TX_IRQ7, /*!< EMAC1 TX Interrupt 7 */
    EMAC1RX_IRQ0, /*!< EMAC1 RX Interrupt 0 */
    EMAC1RX_IRQ1, /*!< EMAC1 RX Interrupt 1 */
    EMAC1RX_IRQ2, /*!< EMAC1 RX Interrupt 2 */
    EMAC1RX_IRQ3, /*!< EMAC1 RX Interrupt 3 */
    EMAC1RX_IRQ4, /*!< EMAC1 RX Interrupt 4 */
    EMAC1RX_IRQ5, /*!< EMAC1 RX Interrupt 5 */
    EMAC1RX_IRQ6, /*!< EMAC1 RX Interrupt 6 */
    EMAC1RX_IRQ7, /*!< EMAC1 RX Interrupt 7 */
    EMAC2IRQ, /*!< EMAC2 Interrupt */
    EMAC2TX_IRQ0, /*!< EMAC2 TX Interrupt 0 */
    EMAC2TX_IRQ1, /*!< EMAC2 TX Interrupt 1 */
    EMAC2TX_IRQ2, /*!< EMAC2 TX Interrupt 2 */
    EMAC2TX_IRQ3, /*!< EMAC2 TX Interrupt 3 */
    EMAC2TX_IRQ4, /*!< EMAC2 TX Interrupt 4 */
    EMAC2TX_IRQ5, /*!< EMAC2 TX Interrupt 5 */
    EMAC2TX_IRQ6, /*!< EMAC2 TX Interrupt 6 */
    EMAC2TX_IRQ7, /*!< EMAC2 TX Interrupt 7 */
    EMAC2RX_IRQ0, /*!< EMAC2 RX Interrupt 0 */
    EMAC2RX_IRQ1, /*!< EMAC2 RX Interrupt 1 */
    EMAC2RX_IRQ2, /*!< EMAC2 RX Interrupt 2 */
    EMAC2RX_IRQ3, /*!< EMAC2 RX Interrupt 3 */
    EMAC2RX_IRQ4, /*!< EMAC2 RX Interrupt 4 */
    EMAC2RX_IRQ5, /*!< EMAC2 RX Interrupt 5 */
    EMAC2RX_IRQ6, /*!< EMAC2 RX Interrupt 6 */
    EMAC2RX_IRQ7, /*!< EMAC2 RX Interrupt 7 */

    /*ECC block interrupt*/
    ECC_DERR_INTR_N, /*!< ECC Double Error Interrupt Non-secure */

    /*Maximum number of interrupts*/
    MAX_HPU_SPI_INTERRUPT /*!< Maximum number of HPU SPI interrupts */
} socfpga_hpu_interrupt_t;

/**
 * @brief Callback function for interrupt handlers.
 * @ingroup intr_fns
 *
 * @param[in] data User data passed to the callback.
 */
typedef void (*socfpga_interrupt_callback_t )(void*data);

/**
 * @brief Interrupt trigger type
 * @ingroup intr_enums
 */
typedef enum {
    SPI_INTERRUPT_TYPE_LEVEL = 0, /*!< Level triggered interrupt*/
    SPI_INTERRUPT_TYPE_EDGE = 2   /*!< Edge triggered interrupt*/
} socfpga_hpu_interrupt_type_t;

/**
 * @brief Interrupt mode
 * @ingroup intr_enums
 */
typedef enum {
    SPI_INTERRUPT_MODE_ANY = 0, /*!< Trigger any CPU core */
    SPI_INTERRUPT_MODE_TARGET = 2 /*!< Trigger targetted CPU core */
} socfpga_hpu_spi_interrupt_mode_t;

/**
 * @brief Interrupt error codes
 * @ingroup intr_enums
 */
typedef enum {
    ERR_OK = 0, /*!< No error */
    ERR_SPI_ID, /*!< Invalid SPI ID */
    ERR_SPI_TYPE, /*!< Invalid SPI type */
    ERR_SPI_MODE, /*!< Invalid SPI mode */
    ERR_SPI_TARGET, /*!< Invalid SPI target */
    ERR_INTERRUPT_CALLBACK, /*!< Invalid callback */
    ERR_PPI_ID /*!< Invalid PPI ID */
} socfpga_interrupt_err_t;

/**
 * @addtogroup intr_fns
 * @{
 */
/**
 * @brief Initializes the GIC interrupt controller.
 */
void interrupt_init_gic(void);

/**
 * @brief Initializes the GIC redistributor of the core
 */
void interrupt_enable_core_rdis(void);

/**
 * @brief Default interrupt handler for GIC.
 *
 * @param[in] data User data passed to the handler.
 */
void gic_default_interrupt_handler(void *data);

/**
 * @brief Enable shared peripheral interrupt.
 *
 * @param[in] id Block interrupt ID.
 * @param[in] interrupt_type Interrupt type.
 * @param[in] interrupt_mode Mode of interrupt.
 * @param[in] priority Priority of the interrupt.
 * @return
 * - ERR_OK on success
 * - ERR_SPI_ID if the interrupt ID is invalid
 */
socfpga_interrupt_err_t interrupt_spi_enable(socfpga_hpu_interrupt_t id,
        socfpga_hpu_interrupt_type_t interrupt_type,
        socfpga_hpu_spi_interrupt_mode_t interrupt_mode, uint8_t priority);

/**
 * @brief Disable shared peripheral interrupt.
 *
 * @param[in] id Block interrupt ID.
 * @return
 * - ERR_OK on success
 * - ERR_SPI_ID if the interrupt ID is invalid
 */
socfpga_interrupt_err_t interrupt_spi_disable(socfpga_hpu_interrupt_t id);

/**
 * @brief Register an ISR for a specific interrupt ID.
 *
 * @param[in] id Block interrupt ID.
 * @param[in] callback Callback function to be invoked when the interrupt occurs.
 * @param[in] user_data Data to be passed to the callback function.
 * @return
 * - ERR_OK on success
 * - ERR_SPI_ID if the interrupt ID is invalid
 * - ERR_INTERRUPT_CALLBACK if the callback is NULL
 */
socfpga_interrupt_err_t interrupt_register_isr(socfpga_hpu_interrupt_t id,
        socfpga_interrupt_callback_t callback, void *user_data);

/**
 * @brief Enable private peripheral interrupt.
 *
 * @param[in] id Block interrupt ID.
 * @param[in] interrupt_type Interrupt type.
 * @param[in] priority Priority of the interrupt.
 * @param[in] gic_redis_id Redistributor ID.
 * @return
 * - ERR_OK on success
 * - ERR_PPI_ID if the interrupt ID is invalid
 */
socfpga_interrupt_err_t interrupt_ppi_enable(socfpga_hpu_interrupt_t id,
        socfpga_hpu_interrupt_type_t interrupt_type, uint8_t priority,
        uint32_t gic_redis_id);

/**
 * @brief Enable interrupt for a specific block
 *
 * @param[in] id Block interrupt ID.
 * @param[in] priority Priority of the interrupt.
 * @return
 * - ERR_OK on success
 * - ERR_SPI_ID if the SPI interrupt ID is invalid
 * - ERR_PPI_ID if the PPI interrupt ID is invalid
 */
socfpga_interrupt_err_t interrupt_enable(socfpga_hpu_interrupt_t id,
        uint8_t priority);

/** @} */
/** @} */

#endif /*__SOCFPGA_INTERRUPT_H__*/
