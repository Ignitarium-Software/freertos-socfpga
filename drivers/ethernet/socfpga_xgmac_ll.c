/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver implementation for SoC FPGA XGMAC
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "socfpga_defines.h"
#include "socfpga_xgmac_ll.h"
#include "socfpga_xgmac_reg.h"
#include "socfpga_cache.h"
#include "osal.h"

#define XGMAC_CLEAR_SPEED_MASK     0x1FFFFFFFU
/* Matches ETH_HALF_DUPLEX in socfpga_phy.h; kept local so the LL layer does not depend on a HAL header. */
#define XGMAC_LL_DUPLEX_HALF       2U
/* Local to the LL layer so it does not need to pull in the HAL header for a delay wrapper. */
#define DELAY_MS(ms)               osal_task_delay((ms))

static bool reset_dma(uint32_t emac_base_addr)
{
    uint8_t elapsed_time = 0;
    uint8_t timeout = 100;

    SET_BIT(emac_base_addr + XGMAC_DMA_MODE, XGMAC_DMA_MODE_SWR_MASK);
    /* Poll for reset for completion */
    while ((RD_REG32(emac_base_addr + XGMAC_DMA_MODE) & XGMAC_DMA_MODE_SWR_MASK) != 0U)
    {
        if (elapsed_time >= timeout)
        {
            return false;
        }
        /* Wait for some delay */
        DELAY_MS(1);
        elapsed_time += 1U;
    }
    return true;
}

static void xgmac_ll_mmc_setup(uint32_t base_address)
{
    /*
     * Disable all the Receive IPC statistics counter,
     * in the management counter
     *
     * MMC block is unused, the interrupt masking is done to avoid some
     * unwanted interrupts
     * */
    CLR_BIT(base_address + XGMAC_MMC_IPC_RX_INTERRUPT_MASK, XGMAC_MMC_IPC_RX_INTR_MASK_ALL);
}

static void xgmac_ll_config_mac_tx(uint32_t base_address, const
        xgmacmac_tx_config_t *mac_tx_config)
{
    /* Program JD MAC configuration to disable Jumbo frames in Tx */
    if (mac_tx_config->jd == 1)
    {
        SET_BIT(base_address + XGMAC_MAC_TX_CONFIGURATION, XGMAC_MAC_TX_CONFIGURATION_JD_MASK);
    }
    else
    {
        CLR_BIT(base_address + XGMAC_MAC_TX_CONFIGURATION, XGMAC_MAC_TX_CONFIGURATION_JD_MASK);
    }
}

static void xgmac_ll_config_mac_rx(uint32_t base_address, const
        xgmacmac_rx_config_t *mac_rx_config)
{
    uint32_t set_mask = 0, clr_mask = 0;
    uint16_t val;

    clr_mask |= XGMAC_MAC_RX_CONFIGURATION_ACS_MASK | XGMAC_MAC_RX_CONFIGURATION_CST_MASK |
            XGMAC_MAC_RX_CONFIGURATION_DCRCC_MASK | XGMAC_MAC_RX_CONFIGURATION_SPEN_MASK |
            XGMAC_MAC_RX_CONFIGURATION_USP_MASK | XGMAC_MAC_RX_CONFIGURATION_GPSLCE_MASK |
            XGMAC_MAC_RX_CONFIGURATION_WD_MASK | XGMAC_MAC_RX_CONFIGURATION_JE_MASK |
            XGMAC_MAC_RX_CONFIGURATION_IPC_MASK | XGMAC_MAC_RX_CONFIGURATION_ARPEN_MASK;

    CLR_BIT(base_address + XGMAC_MAC_RX_CONFIGURATION, clr_mask);

    /* Set/Clear mask bits for Automatic Pad or CRC Stripping */
    if (mac_rx_config->acs == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_ACS_MASK;
    }

    /* Set/Clear mask bits for CRC stripping for Type packets */
    if (mac_rx_config->cst == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_CST_MASK;
    }

    /* Set/Clear mask bits for Disable CRC Checking for Received Packets */
    if (mac_rx_config->dcrcc == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_DCRCC_MASK;
    }

    /* Set/Clear mask bits for Slow Protocol Detection Enable */
    if (mac_rx_config->spen == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_SPEN_MASK;
    }

    /* Set/Clear mask bits for Unicast Slow Protocol Packet Detect */
    if (mac_rx_config->usp == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_USP_MASK;
    }

    /* Set/Clear mask bits for Giant Packet Size Limit Control Enable */
    if (mac_rx_config->gpslce == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_GPSLCE_MASK;
    }

    /* Set/Clear mask bits for Watchdog Disable Enable */
    if (mac_rx_config->wd == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_WD_MASK;
    }

    /* Set/Clear mask bits for Jumbo Packet Enable */
    if (mac_rx_config->je == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_JE_MASK;
    }

    /* Set/Clear mask bits for Checksum Offload Enable */
    if (mac_rx_config->ipc == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_IPC_MASK;
    }

    /* Set/Clear mask bits for ARP enable */
    if (mac_rx_config->arpen == 1)
    {
        set_mask |= XGMAC_MAC_RX_CONFIGURATION_ARPEN_MASK;
    }
    SET_BIT(base_address + XGMAC_MAC_RX_CONFIGURATION, set_mask);

    /* Program Giant Packet Size Limit */
    val = (uint16_t)RD_REG32(base_address + XGMAC_MAC_RX_CONFIGURATION);
    val |= (uint16_t)((uint32_t)mac_rx_config->gpsl << XGMAC_MAC_RX_CONFIGURATION_GPSL_POS);
    WR_REG32(base_address + XGMAC_MAC_RX_CONFIGURATION, val);
}

static void xgmac_ll_enable_rx_flow_control(uint32_t base_address, const
        xgmacmac_rx_flow_ctrl_config_t *mac_rx_flow_ctrl_config)
{
    uint32_t set_mask = 0, clr_mask = 0;

    clr_mask |= XGMAC_MAC_RX_FLOW_CTRL_RFE_MASK | XGMAC_MAC_RX_FLOW_CTRL_UP_MASK |
            XGMAC_MAC_RX_FLOW_CTRL_PFCE_MASK;
    CLR_BIT(base_address + XGMAC_MAC_RX_FLOW_CTRL, clr_mask);

    /* Program Rx Flow Control */
    if (mac_rx_flow_ctrl_config->rfe == 1)
    {
        set_mask |= XGMAC_MAC_RX_FLOW_CTRL_RFE_MASK;
    }

    /* Program Unicast Pause Packet Detect */
    if (mac_rx_flow_ctrl_config->up == 1)
    {
        set_mask |= XGMAC_MAC_RX_FLOW_CTRL_UP_MASK;
    }

    /* Program Priority Based Flow Control Enable */
    if (mac_rx_flow_ctrl_config->pfce == 1)
    {
        set_mask |= XGMAC_MAC_RX_FLOW_CTRL_PFCE_MASK;
    }
    SET_BIT(base_address + XGMAC_MAC_RX_FLOW_CTRL, set_mask);
}

static void xgmac_ll_start_stop_mac_tx(uint32_t base_address, bool stflag)
{
    if (stflag == true)
    {
        SET_BIT(base_address + XGMAC_MAC_TX_CONFIGURATION, XGMAC_MAC_TX_CONFIGURATION_TE_MASK);
    }
    else
    {
        CLR_BIT(base_address + XGMAC_MAC_TX_CONFIGURATION, XGMAC_MAC_TX_CONFIGURATION_TE_MASK);
    }
}

static void xgmac_ll_start_stop_mac_rx(uint32_t base_address, bool stflag)
{
    if (stflag == true)
    {
        SET_BIT(base_address + XGMAC_MAC_RX_CONFIGURATION, XGMAC_MAC_RX_CONFIGURATION_RE_MASK);
    }
    else
    {
        CLR_BIT(base_address + XGMAC_MAC_RX_CONFIGURATION, XGMAC_MAC_RX_CONFIGURATION_RE_MASK);
    }
}

static void xgmac_ll_set_mtl_tx_regs(uint32_t base_address, uint8_t qindx, const
        xgmacmtl_tx_queue_config_t *mtl_txq_cfg_params)
{
    uint32_t val;
    uint32_t reg_val;
    uint32_t tqs;
    uint32_t set_mask = 0;

    /* Compute Tqs */
    reg_val = RD_REG32(base_address + XGMAC_MAC_HW_FEATURE1);
    tqs = XGMAC_MTL_TX_FIFO_BLK_CNT(reg_val);

    /* Enable Transmit Queue Store and Forward */
    CLR_MTL_QX_BIT(base_address + XGMAC_MTL_TXQ_OPERATION_MODE, qindx,
            XGMAC_MTL_TXQ0_OPERATION_MODE_TSF_MASK);
    if (mtl_txq_cfg_params->tsf == 1)
    {
        set_mask |= XGMAC_MTL_TXQ0_OPERATION_MODE_TSF_MASK;
    }

    /* Enable Tx Queue */
    set_mask |= mtl_txq_cfg_params->txqen << XGMAC_MTL_TXQ0_OPERATION_MODE_TXQEN_POS;

    SET_MTL_QX_BIT(base_address + XGMAC_MTL_TXQ_OPERATION_MODE, qindx, set_mask);

    /* Program Transmit Queue Size */
    val = RD_MTL_QX_REG32(base_address, qindx, XGMAC_MTL_TXQ_OPERATION_MODE);
    val |= tqs << XGMAC_MTL_TXQ0_OPERATION_MODE_TQS_POS;
    WR_MTL_QX_REG32(base_address, qindx, XGMAC_MTL_TXQ_OPERATION_MODE, val);
}

static void xgmac_ll_set_mtl_rx_regs(uint32_t base_address, uint8_t qindx, const
        xgmacmtl_rx_queue_config_t *mtl_rxq_cfg_params)
{
    uint32_t val;
    uint32_t reg_val;
    uint32_t rqs;
    uint32_t set_mask = 0, clr_mask = 0;

    /* Compute Rqs */
    reg_val = RD_REG32(base_address + XGMAC_MAC_HW_FEATURE1);
    rqs = XGMAC_MTL_RX_FIFO_BLK_CNT(reg_val);

    clr_mask |= XGMAC_MTL_RXQ_OPERATION_MODE_RSF_MASK | XGMAC_MTL_RXQ_OPERATION_MODE_EHFC_MASK;
    CLR_MTL_QX_BIT(base_address + XGMAC_MTL_RXQ_OPERATION_MODE, qindx, clr_mask);

    /* Enable Receive Queue Store and Forward */
    if (mtl_rxq_cfg_params->rsf == 1)
    {
        set_mask |= XGMAC_MTL_RXQ_OPERATION_MODE_RSF_MASK;
    }

    /* Enable Hardware Flow Control */
    if (mtl_rxq_cfg_params->ehfc == 1)
    {
        set_mask |= XGMAC_MTL_RXQ_OPERATION_MODE_EHFC_MASK;
    }
    SET_MTL_QX_BIT(base_address + XGMAC_MTL_RXQ_OPERATION_MODE, qindx, set_mask);

    /* Program Receive Queue Size */
    val = RD_MTL_QX_REG32(base_address, qindx, XGMAC_MTL_RXQ_OPERATION_MODE);
    val |= rqs << XGMAC_MTL_RXQ_OPERATION_MODE_RQS_POS;
    WR_MTL_QX_REG32(base_address, qindx, XGMAC_MTL_RXQ_OPERATION_MODE, val);
}

static void xgmac_ll_config_macrxqctrl_regs(uint32_t base_address, const
        xgmacmac_rx_q_ctrl_config_t *mac_rxq_ctrl_config)
{
    /* Clear and set the Receive Queue 0 */
    CLR_BIT(base_address + XGMAC_MAC_RXQ_CTRL0, XGMAC_MAC_RXQ_CTRL0_RXQ0EN_MASK);

    /* Enable for data Center Bridging/Generic */
    SET_BIT(base_address + XGMAC_MAC_RXQ_CTRL0,
            mac_rxq_ctrl_config->rxq0en << XGMAC_MAC_RXQ_CTRL0_RXQ0EN_POS);

    /* Enable/Disable Multicast and Broadcast Queue Enable */
    if (mac_rxq_ctrl_config->mcbcqen == 1)
    {
        SET_BIT(base_address + XGMAC_MAC_RXQ_CTRL1, XGMAC_MAC_RXQ_CTRL1_MCBCQEN_MASK);
    }
    else
    {
        CLR_BIT(base_address + XGMAC_MAC_RXQ_CTRL1, XGMAC_MAC_RXQ_CTRL1_MCBCQEN_MASK);
    }
}

static void xgmac_ll_config_mac_frame_filter(uint32_t base_address, const
        xgmacmac_pkt_filter_config_t *mac_pkt_filter_config)
{
    uint8_t val;
    uint32_t set_mask;
    uint32_t clr_mask;

    if (base_address == 0U)
    {
        return;
    }

    /* Program Promiscuous Mode, Hash Unicast/Multicast, DA Inverse Filtering, Pass All Multicast, Disable Broadcast */
    set_mask = 0;
    clr_mask = XGMAC_MAC_PACKET_FILTER_PR_MASK | XGMAC_MAC_PACKET_FILTER_HUC_MASK |
            XGMAC_MAC_PACKET_FILTER_HMC_MASK | XGMAC_MAC_PACKET_FILTER_DAIF_MASK |
            XGMAC_MAC_PACKET_FILTER_PM_MASK | XGMAC_MAC_PACKET_FILTER_DBF_MASK;
    CLR_BIT(base_address + XGMAC_MAC_PACKET_FILTER, clr_mask);

    if (mac_pkt_filter_config->pr == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_PR_MASK;
    }
    if (mac_pkt_filter_config->huc == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_HUC_MASK;
    }
    if (mac_pkt_filter_config->hmc == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_HMC_MASK;
    }
    if (mac_pkt_filter_config->daif == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_DAIF_MASK;
    }
    if (mac_pkt_filter_config->pm == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_PM_MASK;
    }
    if (mac_pkt_filter_config->dbf == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_DBF_MASK;
    }
    SET_BIT(base_address + XGMAC_MAC_PACKET_FILTER, set_mask);

    /* Program Pass Control Packets  */
    val = (uint8_t)RD_REG32(base_address + XGMAC_MAC_PACKET_FILTER);
    val |= mac_pkt_filter_config->pcf << XGMAC_MAC_PACKET_FILTER_PCF_POS;
    WR_REG32(base_address + XGMAC_MAC_PACKET_FILTER, val);

    /* Program SA Inverse Filtering, Source Address Filter Enable, Hash or Perfect Filter */
    set_mask = 0;
    clr_mask = XGMAC_MAC_PACKET_FILTER_SAIF_MASK | XGMAC_MAC_PACKET_FILTER_SAF_MASK |
            XGMAC_MAC_PACKET_FILTER_HPF_MASK;
    CLR_BIT(base_address + XGMAC_MAC_PACKET_FILTER, clr_mask);

    if (mac_pkt_filter_config->saif == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_SAIF_MASK;
    }
    if (mac_pkt_filter_config->saf == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_SAF_MASK;
    }
    if (mac_pkt_filter_config->hpf == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_HPF_MASK;
    }
    SET_BIT(base_address + XGMAC_MAC_PACKET_FILTER, set_mask);

    /* Program DA Hash Index or L3/L4 Filter Number in Receive  Filter */
    val = (uint8_t)RD_REG32(base_address + XGMAC_MAC_PACKET_FILTER);
    val |= (uint8_t)((uint32_t)mac_pkt_filter_config->dhlfrs << XGMAC_MAC_PACKET_FILTER_DHLFRS_POS);
    WR_REG32(base_address + XGMAC_MAC_PACKET_FILTER, val);

    /* Program VLAN Tag Filter Enable, Layer 3/4 Filter Enable, Drop Non-TCP/UDP over IP, Receive All */
    set_mask = 0;
    clr_mask = XGMAC_MAC_PACKET_FILTER_VTFE_MASK | XGMAC_MAC_PACKET_FILTER_IPFE_MASK |
            XGMAC_MAC_PACKET_FILTER_DNTU_MASK | XGMAC_MAC_PACKET_FILTER_RA_MASK;
    CLR_BIT(base_address + XGMAC_MAC_PACKET_FILTER, clr_mask);

    if (mac_pkt_filter_config->vtfe == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_VTFE_MASK;
    }
    if (mac_pkt_filter_config->ipfe == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_IPFE_MASK;
    }
    if (mac_pkt_filter_config->dntu == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_DNTU_MASK;
    }
    if (mac_pkt_filter_config->ra == 1)
    {
        set_mask |= XGMAC_MAC_PACKET_FILTER_RA_MASK;
    }
    SET_BIT(base_address + XGMAC_MAC_PACKET_FILTER, set_mask);
}

static void xgmac_ll_enable_tx_flow_control(uint32_t base_address, uint8_t qindx, const
        xgmacmac_tx_flow_ctrl_config_t *mac_tx_flow_ctrl_config)
{
    uint32_t set_mask;
    uint32_t clr_mask;

    if ((base_address == 0U) || (mac_tx_flow_ctrl_config == NULL))
    {
        return;
    }

    clr_mask = XGMAC_MAC_Q0_TX_FLOW_CTRL_TFE_MASK | XGMAC_MAC_Q0_TX_FLOW_CTRL_FCB_MASK |
            XGMAC_MAC_Q0_TX_FLOW_CTRL_DZPQ_MASK;
    CLR_MAC_FLOW_CNTL_BIT(base_address + XGMAC_MAC_Q0_TX_FLOW_CTRL, qindx, clr_mask);

    /* Set Pause frame */
    set_mask = (uint32_t)mac_tx_flow_ctrl_config->pt << XGMAC_MAC_Q6_TX_FLOW_CTRL_PT_POS;

    /* Set Pause Low Threshold */
    set_mask |= (uint32_t)mac_tx_flow_ctrl_config->plt << XGMAC_MAC_Q0_TX_FLOW_CTRL_PLT_POS;

    /* Enable Transmit Flow control */
    if (mac_tx_flow_ctrl_config->tfe == 1)
    {
        set_mask |= XGMAC_MAC_Q0_TX_FLOW_CTRL_TFE_MASK;
    }

    /* Enable Flow Control Busy */
    if (mac_tx_flow_ctrl_config->fcb == 1)
    {
        set_mask |= XGMAC_MAC_Q0_TX_FLOW_CTRL_FCB_MASK;
    }

    /* Disable Zero-Quanta Pause */
    if (mac_tx_flow_ctrl_config->dzpq == 1)
    {
        set_mask |= XGMAC_MAC_Q0_TX_FLOW_CTRL_DZPQ_MASK;
    }

    SET_MAC_FLOW_CNTL_BIT(base_address + XGMAC_MAC_Q0_TX_FLOW_CTRL, qindx, set_mask);
}

static void xgmac_ll_mtl_init (uint32_t base_address, const
        xgmac_dev_config_str_t *xgmac_dev_config)
{
    uint8_t qindex;
    uint8_t num_queues;
    const xgmac_dev_config_t *mac_dev_config = (const
            xgmac_dev_config_t *)(xgmac_dev_config->mac_dev_config);

    /* Program MTL configuration registers for Tx */
    num_queues = mac_dev_config->tx_queue_num;
    for (qindex = 0; qindex < num_queues; qindex++)
    {
        xgmac_ll_set_mtl_tx_regs(base_address, qindex, (const
                xgmacmtl_tx_queue_config_t *)
                xgmac_dev_config->mtl_tx_q_config);
    }

    /* Program MTL configuration registers for Rx */
    num_queues = mac_dev_config->rx_queue_num;
    for (qindex = 0; qindex < num_queues; qindex++)
    {
        if ((base_address == 0U) || (xgmac_dev_config == NULL))
        {
            return;
        }
        xgmac_ll_set_mtl_rx_regs(base_address, qindex, (const
                xgmacmtl_rx_queue_config_t *)
                xgmac_dev_config->mtl_rx_q_config);
    }
}

/* XGMAC Device start */
void xgmac_ll_mac_start(uint32_t base_address)
{
    /* Start the MAC Transmitter */
    xgmac_ll_start_stop_mac_tx(base_address, true);

    /* Start the MAC Receiver  */
    xgmac_ll_start_stop_mac_rx(base_address, true);
}

static void xgmac_ll_mac_config(uint32_t base_address, const
        xgmac_dev_config_str_t *xgmac_dev_config)
{
    uint8_t num_queues;
    const xgmac_dev_config_t *mac_dev_config = (const
            xgmac_dev_config_t *)(xgmac_dev_config->mac_dev_config);

    if ((base_address == 0U) || (xgmac_dev_config == NULL))
    {
        return;
    }
    /* Configure  MAC Rx Queue Control Register */
    xgmac_ll_config_macrxqctrl_regs(base_address, (const
            xgmacmac_rx_q_ctrl_config_t *)
            xgmac_dev_config->mac_rx_q_ctrl_config);

    /* Program MAC Frame Filter Register  for Promiscuous mode and Receive all */
    xgmac_ll_config_mac_frame_filter(base_address, (const
            xgmacmac_pkt_filter_config_t *)
            xgmac_dev_config->mac_pkt_filter_config);

    /* Program MAC Transmit Flow Control Register */
    num_queues = mac_dev_config->tx_queue_num;
    for (uint8_t qindex = 0; qindex < num_queues; qindex++)
    {
        xgmac_ll_enable_tx_flow_control(base_address, qindex, (const
                xgmacmac_tx_flow_ctrl_config_t *)
                xgmac_dev_config->mac_tx_flow_ctrl_config);
    }

    /* Program MAC Receive Flow Control Register */
    xgmac_ll_enable_rx_flow_control(base_address, (const
            xgmacmac_rx_flow_ctrl_config_t *)
            xgmac_dev_config->mac_rx_flow_ctrl_config);

    /* Program MAC Tx Configuration Registers */
    xgmac_ll_config_mac_tx(base_address, (const xgmacmac_tx_config_t *)
            xgmac_dev_config->mac_tx_config);


    /* Set Checksum Offload to COE for IPv4 Header & TCP, UDP, ICMP Payload  Rx packets */
    xgmac_ll_config_mac_rx(base_address, (const xgmacmac_rx_config_t *)
            xgmac_dev_config->mac_rx_config);

    /* MAC Management counter config */
    xgmac_ll_mmc_setup(base_address);
}

static void xgmac_ll_set_macaddress(uint32_t base_address, void *address_ptr, uint8_t
        index_val)
{
    uint32_t mac_addr;
    uint8_t *aptr = (uint8_t *)(void *)address_ptr;
    uint32_t addr_offset;

    /* Hardware MAc Address index ranges from 1 to 32,and index will start from 0 to 31 */
    index_val--;
    addr_offset = ((uint32_t)index_val * 8U);

    /* Set the MAC bits [31:0] in MAC_Addressx_Low register*/
    mac_addr = *(aptr);
    mac_addr |= ((uint32_t)(*(aptr + 1U)) << 8U);
    mac_addr |= ((uint32_t)(*(aptr + 2U)) << 16U);
    mac_addr |= ((uint32_t)(*(aptr + 3U)) << 24U);

    WR_REG32(base_address +
            ((uint32_t)XGMAC_MAC_ADDRESS0_LOW + (uint32_t)addr_offset), mac_addr);

    /* Read the MAC_Addressx_High register and clear address bits */
    mac_addr = RD_REG32(base_address +
            ((uint32_t)XGMAC_MAC_ADDRESS0_HIGH + (uint32_t)addr_offset));

    /* Clear the lower 16 bits of the MAC_Addressx_High register */
    mac_addr &= (uint32_t)(~XGMAC_MAC_ADDRESS0_HIGH_ADDRHI_MASK);


    /* Set MAC bits [47:32] from MAC_Addressx_High[15:0]  */
    mac_addr |= (uint32_t)(*(aptr + 4U));
    mac_addr |= ((uint32_t)(*(aptr + 5U)) << 8U);

    WR_REG32(base_address +
            ((uint32_t)XGMAC_MAC_ADDRESS0_HIGH + (uint32_t)addr_offset), mac_addr);
}

void xgmac_ll_mac_init(uint32_t mac_base_address, uint32_t mtl_base_address,
        const xgmac_dev_config_str_t *xgmac_dev_config)
{
    static const uint8_t mac_address[24] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, (uint8_t)0x45, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 128, 17, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
    };

    /* Program MAC register configurations */
    xgmac_ll_mac_config(mac_base_address, xgmac_dev_config);

    /* Program MTL configuration registers for Tx and Rx */
    xgmac_ll_mtl_init(mtl_base_address, xgmac_dev_config);

    /* Setup the MAC Address in MAC High and MAC Low Registers */
    xgmac_ll_set_macaddress(mac_base_address, (void *)(uintptr_t)mac_address,
            MAC_ADRRESS_INDEX1);

}

void xgmac_ll_set_duplex(uint32_t base_address, uint8_t duplex)
{
    /* Half Duplex */
    if (duplex == XGMAC_LL_DUPLEX_HALF)
    {
        SET_BIT(base_address + XGMAC_MAC_EXTENDED_CONFIGURATION, XGMAC_MAC_EXT_CONF_HD);
    }
    else
    {
        CLR_BIT(base_address + XGMAC_MAC_EXTENDED_CONFIGURATION, XGMAC_MAC_EXT_CONF_HD);
    }
}
void xgmac_ll_set_speed(uint32_t base_address, uint32_t speed)
{
    uint32_t reg_data = 0;

    reg_data = RD_REG32(base_address + XGMAC_MAC_TX_CONFIGURATION);
    reg_data &= (XGMAC_CLEAR_SPEED_MASK);
    /* 1Gbps */
    if (speed == 1000U)
    {
        reg_data |= ((uint32_t)XGMAC_MAC_CONF_SS_1G_GMII << XGMAC_MAC_TX_CONFIGURATION_SS_POS);
    }
    /* 100Mbps */
    else if (speed == 100U)
    {
        reg_data |= ((uint32_t)XGMAC_MAC_CONF_SS_100M_MII << XGMAC_MAC_TX_CONFIGURATION_SS_POS);
    }
    /* 10Mbps */
    else
    {
        reg_data |= ((uint32_t)XGMAC_MAC_CONF_SS_10M_MII << XGMAC_MAC_TX_CONFIGURATION_SS_POS);
    }

    WR_REG32(base_address + XGMAC_MAC_TX_CONFIGURATION, reg_data);
}

static void xgmac_ll_config_dma_sysbus_mode(uint32_t base_address, const
        xgmacdma_config_t *dma_config)
{
    uint32_t set_mask = 0, clr_mask = 0;

    clr_mask |= XGMAC_DMA_SYSBUS_MODE_UBL_MASK | XGMAC_DMA_SYSBUS_MODE_BLEN4_MASK |
            XGMAC_DMA_SYSBUS_MODE_BLEN8_MASK | XGMAC_DMA_SYSBUS_MODE_BLEN16_MASK |
            XGMAC_DMA_SYSBUS_MODE_BLEN32_MASK | XGMAC_DMA_SYSBUS_MODE_BLEN64_MASK |
            XGMAC_DMA_SYSBUS_MODE_BLEN128_MASK | XGMAC_DMA_SYSBUS_MODE_BLEN256_MASK |
            XGMAC_DMA_SYSBUS_MODE_EAME_MASK | XGMAC_DMA_SYSBUS_MODE_AAL_MASK;

    /* Clear DMA Sysbus Mode clr_mask bits */
    CLR_BIT(base_address + XGMAC_DMA_SYSBUS_MODE, clr_mask);

    /* Set/Clear mask bits for Undefined Burst Length */
    if (dma_config->ubl == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_UBL_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 4 */
    if (dma_config->blen4 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN4_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 8 */
    if (dma_config->blen8 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN8_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 16 */
    if (dma_config->blen16 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN16_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 32 */
    if (dma_config->blen32 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN32_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 64 */
    if (dma_config->blen64 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN64_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 128 */
    if (dma_config->blen128 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN128_MASK;
    }

    /* Set/Clear mask bits for AXI Burst Length 256 */
    if (dma_config->blen256 == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_BLEN256_MASK;
    }

    /* Set/Clear mask bits for Enhanced Address Mode Enable */
    if (dma_config->eame == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_EAME_MASK;
    }

    /* Set/Clear mask bits for Address-Aligned Beats */
    if (dma_config->aal == 1)
    {
        set_mask |= XGMAC_DMA_SYSBUS_MODE_AAL_MASK;
    }

    /* Configure Maximum Read Outstanding Request Limit */
    set_mask |= XGMAC_DMA_SYSBUS_MODE_RD_OSR_LMT_MASK;

    /* Configure Maximum Write Outstanding Request Limit */
    set_mask |= XGMAC_DMA_SYSBUS_MODE_WR_OSR_LMT_MASK;

    /* Set DMA Sysbus Mode set_mask bits */
    SET_BIT(base_address + XGMAC_DMA_SYSBUS_MODE, set_mask);
}

bool xgmac_ll_dma_init(uint32_t base_address, const
        xgmac_dev_config_str_t *xgmac_dev_config)
{
    if (reset_dma(base_address) != true)
    {
        return false;
    }
    xgmac_ll_config_dma_sysbus_mode(base_address, (const xgmacdma_config_t *)
            xgmac_dev_config->dma_config);
    return true;
}


static void xgmac_ll_start_dma(uint32_t base_address, uint8_t ch, uint8_t tx_rx_flag)
{
    uint32_t val;

    if (tx_rx_flag == XGMAC_DMA_TRANSMIT_START)
    {
        val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TX_CONTROL);
        val |= (1U << XGMAC_DMA_CH_TX_CONTROL_ST_POS);
        WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TX_CONTROL, val);
    }

    if (tx_rx_flag == XGMAC_DMA_RECEIVE_START)
    {
        val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL);
        val |= (1U << XGMAC_DMA_CH_RX_CONTROL_SR_POS);
        WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL, val);
    }
}

void xgmac_ll_init_dma_ch_desc_reg(uint32_t base_address, uint8_t ch,
        xgmac_dma_desc_addr_t *desc_params)
{
    uint32_t val;

    /* Program the transmit ring length registers */
    val = desc_params->tx_ring_len & XGMAC_DMA_CH0_TX_CONTROL2_TDRL_MASK;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TX_CONTROL2, val);

    /* Program the receive ring length registers */
    val = desc_params->rx_ring_len & XGMAC_DMA_CH0_RX_CONTROL2_RDRL_MASK;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL2, val);

    /* Program Tx List Address Registers with Base Address of Ring Descriptor */
    val = desc_params->tx_desc_high_addr;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TXDESC_LIST_HADDRESS, val);
    val = desc_params->tx_desc_low_addr;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TXDESC_LIST_LADDRESS, val);

    /* Program Rx List Address Registers with Base Address of Ring Descriptor */
    val = desc_params->rx_desc_high_addr;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RXDESC_LIST_HADDRESS, val);
    val = desc_params->rx_desc_low_addr;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RXDESC_LIST_LADDRESS, val);

    /* Program the  Tx Tail Pointer Register */
    val = desc_params->tx_last_desc_addr;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TXDESC_TAIL_LPOINTER, val);

    /* Program the  Rx Tail Pointer Register */
    val = desc_params->rx_last_desc_addr;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RXDESC_TAIL_LPOINTER, val);
}

void xgmac_ll_config_dma_ch_control(uint32_t base_address, uint8_t ch,
        const xgmacdma_ch_config_t *ch_config)
{
    uint32_t val;
    uint32_t set_mask = 0;
    uint32_t clr_mask;

    if (base_address == 0U)
    {
        return;
    }

    /* Program  DMA Control Settings - PBLx8 and SPH Enable */
    clr_mask = XGMAC_DMA_CH0_CONTROL_PBLX8_MASK | XGMAC_DMA_CH0_CONTROL_SPH_MASK;
    CLR_DMA_CH_BIT(base_address + XGMAC_DMA_CH_CONTROL, ch, clr_mask);

    if (ch_config->pblx8 == true)
    {
        set_mask |= XGMAC_DMA_CH0_CONTROL_PBLX8_MASK;
    }
    if (ch_config->sph == true)
    {
        set_mask |= XGMAC_DMA_CH0_CONTROL_SPH_MASK;
    }
    SET_DMA_CH_BIT(base_address + XGMAC_DMA_CH_CONTROL, ch, set_mask);

    /* Program  DMA Control Settings - DSL Descriptor Skip Length */
    val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_CONTROL);
    val |= (uint32_t)ch_config->dsl << XGMAC_DMA_CH_CONTROL_DSL_POS;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_CONTROL, val);

    /* Program  DMA Tx Control Settings - TSE Enable*/
    CLR_DMA_CH_BIT(base_address + XGMAC_DMA_CH_TX_CONTROL, ch,
            XGMAC_DMA_CH0_TX_CONTROL_TSE_MASK);
    if (ch_config->tse == true)
    {
        SET_DMA_CH_BIT(base_address + XGMAC_DMA_CH_TX_CONTROL, ch,
                XGMAC_DMA_CH0_TX_CONTROL_TSE_MASK);
    }

    /* Program  DMA Tx Control Settings - Write Txpbl */
    val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TX_CONTROL);
    val |= (uint32_t)ch_config->txpbl << XGMAC_DMA_CH_TX_CONTROL_TXPBL_POS;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_TX_CONTROL, val);

    /* Program  DMA Rx Control Settings - Write Rxpbl */
    val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL);
    val |= (uint32_t)ch_config->rxpbl << XGMAC_DMA_CH_RX_CONTROL_RXPBL_POS;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL, val);

    /* Program  DMA Rx Control Settings - RBSZ Receive Buffer Size*/
    val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL);
    val |= (uint32_t)ch_config->rbsz << XGMAC_DMA_CH_RX_CONTROL_RBSZ_POS;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_RX_CONTROL, val);

}

int32_t xgmac_ll_enable_dma_interrupt(uint32_t base_address, uint8_t ch,
        xgmac_dma_interrupt_id_t id)
{
    uint32_t val;
    uint32_t intr_mask = 0;

    /* Clear the DMA channel status register bits if set. Its a sticky bit hence write back to clear */
    val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_STATUS);
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_STATUS, val);

    switch (id)
    {
        case INTERRUPT_NIS:
            intr_mask = XGMAC_DMA_INTR_MASK_TI | XGMAC_DMA_INTR_MASK_RI |
                    XGMAC_DMA_INTR_MASK_NIS;
            break;

        case INTERRUPT_AIS:
            intr_mask = XGMAC_DMA_INTR_MASK_FBE | XGMAC_DMA_INTR_MASK_TXS |
                    XGMAC_DMA_INTR_MASK_RBU | XGMAC_DMA_INTR_MASK_RS |
                    XGMAC_DMA_INTR_MASK_DDE | XGMAC_DMA_INTR_MASK_AIS;
            break;

        default:
            return -EINVAL;
    }
    if (intr_mask == 0U)
    {
        return -EIO;
    }

    val = RD_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_INTERRUPT_ENABLE);
    val |= intr_mask;
    WR_DMA_CH_REG32(base_address, ch, XGMAC_DMA_CH_INTERRUPT_ENABLE, val);

    return 0;
}

void xgmac_ll_disable_interrupt(uint32_t base_address)
{
    /* Read the XGMAC interrupt status register */
    uint32_t mac_intr_status = RD_REG32(
            base_address + XGMAC_MAC_INTERRUPT_STATUS);

    /* Check for any active interrupt */
    mac_intr_status &= ~XGMAC_MAC_INTERRUPT_STATUS_LSI_MASK;

    /* Clear the interrupt */
    WR_REG32(base_address + XGMAC_MAC_INTERRUPT_STATUS, mac_intr_status);
}

void xgmac_ll_start_dma_dev(uint32_t base_address, const
        xgmac_dev_config_str_t *xgmac_dev_config)
{
    uint8_t ch;
    uint8_t num_ch;

    num_ch = xgmac_dev_config->mac_dev_config->dma_ch_num;

    for (ch = 0; ch < num_ch; ch++)
    {
        /* Start Receive and Transmit DMA */
        xgmac_ll_start_dma(base_address, ch, XGMAC_DMA_TRANSMIT_START);

        xgmac_ll_start_dma(base_address, ch, XGMAC_DMA_RECEIVE_START);
    }
}

xgmac_dma_interrupt_id_t xgmac_ll_check_and_clear_xgmac_interrupt_status(uint32_t
        base_address)
{
    uint32_t val;
    xgmac_dma_interrupt_id_t res;

    /* Read the status */
    val = RD_REG32(base_address + XGMAC_DMA_CH_STATUS);

    /* Clear the status */
    WR_REG32(base_address + XGMAC_DMA_CH_STATUS, val);

    if ((val & XGMAC_DMA_INTR_MASK_TI) != 0U)
    {
        res = INTERRUPT_TI;
    }
    else if ((val & XGMAC_DMA_INTR_MASK_RI) != 0U)
    {
        res = INTERRUPT_RI;
    }
    else if ((val & XGMAC_DMA_INTR_MASK_FBE) != 0U)
    {
        res = INTERRUPT_FBE;
    }
    else if ((val & XGMAC_DMA_INTR_MASK_TXS) != 0U)
    {
        res = INTERRUPT_TXS;
    }
    else if ((val & XGMAC_DMA_INTR_MASK_RBU) != 0U)
    {
        res = INTERRUPT_RBU;
    }
    else if ((val & XGMAC_DMA_INTR_MASK_RS) != 0U)
    {
        res = INTERRUPT_RS;
    }
    else
    {
        res = INTERRUPT_UNHANDLED;
    }

    return res;
}
void xgmac_ll_check_and_clear_link_interrupt_status(uint32_t base_address)
{
    uint32_t val;
    /*register reads ,clears the interrupts*/
    val = RD_REG32(base_address + XGMAC_MAC_RX_TX_STATUS);
    val = RD_REG32(base_address + XGMAC_MAC_INTERRUPT_STATUS);
    (void)val;
}
