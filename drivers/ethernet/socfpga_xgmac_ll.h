/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for SoC FPGA XGMAC low level driver
 */

#ifndef __SOCFPGA_XGMAC_LL_H__
#define __SOCFPGA_XGMAC_LL_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "socfpga_xgmac_reg.h"

#define BIT(nr)    (1UL << (nr))
#define XGMAC_MAC_CONF_SS_1G_GMII     3U
#define XGMAC_MAC_CONF_SS_100M_MII    4U
#define XGMAC_MAC_CONF_SS_10M_MII     7U

#define XGMAC_MAC_EXT_CONF_HD           BIT(24)

#define MAC_ADRRESS_INDEX1    1

#define XGMAC_DMA_TRANSMIT_START    1U
#define XGMAC_DMA_RECEIVE_START     2U

#define XGMAC_GET_BASE_ADDRESS(instance)         ((uint32_t)(XGMAC_EMAC_BASEADDR + \
    ((uint32_t)(instance) * 0x10000U)))
#define XGMAC_GET_DMA_BASE_ADDRESS(instance)     ((uint32_t)(XGMAC_EMAC_DMA_BASEADDR + \
    ((uint32_t)(instance) * 0x10000U)))
#define XGMAC_GET_MTL_BASE_ADDRESS(instance)     ((uint32_t)(XGMAC_EMAC_MTL_BASEADDR + \
    ((uint32_t)(instance) * 0x10000U)))

#define XGMAC_DMA_INTR_MASK_TI     0x00000001U
#define XGMAC_DMA_INTR_MASK_TXS    0x00000002U

#define XGMAC_DMA_INTR_MASK_RI     0x00000040U
#define XGMAC_DMA_INTR_MASK_RBU    0x00000080U
#define XGMAC_DMA_INTR_MASK_RS     0x00000100U
#define XGMAC_DMA_INTR_MASK_DDE    0x00000200U

#define XGMAC_DMA_INTR_MASK_FBE    0x00001000U
#define XGMAC_DMA_INTR_MASK_AIS    0x00004000U
#define XGMAC_DMA_INTR_MASK_NIS    0x00008000U

#define XGMAC_MMC_IPC_RX_INTR_MASK_ALL    0xFFFFFFFFU
/* Get the fifo size in bytes from Feature1 register. */
#define XGMAC_MTL_RX_FIFOSZ_BYTES(feature1_val)    (128U << (((feature1_val) & \
    XGMAC_MAC_HW_FEATURE1_RXFIFOSIZE_MASK) >> XGMAC_MAC_HW_FEATURE1_RXFIFOSIZE_POS))

#define XGMAC_MTL_TX_FIFOSZ_BYTES(feature1_val)    (128U << (((feature1_val) & \
    XGMAC_MAC_HW_FEATURE1_TXFIFOSIZE_MASK) >> XGMAC_MAC_HW_FEATURE1_TXFIFOSIZE_POS))

/*
 * Calculate block count from fifo size in bytes (256 bytes per block,
 * value 0 means 256 bytes).
 */
#define XGMAC_MTL_RX_FIFO_BLK_CNT(feature1_val)    ((XGMAC_MTL_RX_FIFOSZ_BYTES(feature1_val) >> 8) - \
    1U)
#define XGMAC_MTL_TX_FIFO_BLK_CNT(feature1_val)    ((XGMAC_MTL_TX_FIFOSZ_BYTES(feature1_val) >> 8) - \
    1U)

/* Common register bit set/clear macros */
#define SET_BIT(address, bit)     (*(uint32_t volatile *)((uintptr_t)(address)) |= (bit))
#define CLR_BIT(address, bit)     (*(uint32_t volatile *)((uintptr_t)(address)) &= ~(bit))

/* MAC register bit set/clear macros */
#define SET_MAC_FLOW_CNTL_BIT(addr, queue_idx, bit)    (*(uint32_t \
    volatile *)((uintptr_t)(addr) + ((queue_idx) * XGMAC_TX_FLOW_CONTROL_INC)) |= (bit))
#define CLR_MAC_FLOW_CNTL_BIT(addr, queue_idx, bit)    (*(uint32_t \
    volatile *)((uintptr_t)(addr) + ((queue_idx) * XGMAC_TX_FLOW_CONTROL_INC)) &= ~(bit))

/* MTL register bit set/clear macros */
#define SET_MTL_QX_BIT(addr, queue_idx, bit)    (*(uint32_t volatile *)((uintptr_t)(addr) + \
    XGMAC_MTL_TC_BASE + ((queue_idx) * XGMAC_MTL_TC_INC)) |= (bit))
#define CLR_MTL_QX_BIT(addr, queue_idx, bit)    (*(uint32_t volatile *)((uintptr_t)(addr) + \
    XGMAC_MTL_TC_BASE + ((queue_idx) * XGMAC_MTL_TC_INC)) &= ~(bit))

/* MTL register read/write macros */
#define RD_MTL_QX_REG32(base_address, queue_indx, reg_offset)          *(uint32_t \
    volatile *)((uintptr_t)(base_address) + XGMAC_MTL_TC_BASE + ((queue_indx) *   \
    XGMAC_MTL_TC_INC) + (reg_offset))
#define WR_MTL_QX_REG32(base_address, queue_indx, reg_offset, data)    (*(uint32_t \
    volatile *)((uintptr_t)(base_address) + XGMAC_MTL_TC_BASE + ((queue_indx) *    \
    XGMAC_MTL_TC_INC) + (reg_offset)) = (data))

/* DMA register bit set/clear macros */
#define SET_DMA_CH_BIT(addr, ch, bit)    (*(uint32_t volatile *)((uintptr_t)(addr) + \
    XGMAC_DMA_CHANNEL_BASE + ((ch) * XGMAC_DMA_CHANNEL_INC)) |= (bit))
#define CLR_DMA_CH_BIT(addr, ch, bit)    (*(uint32_t volatile *)((uintptr_t)(addr) + \
    XGMAC_DMA_CHANNEL_BASE + ((ch) * XGMAC_DMA_CHANNEL_INC)) &= ~(bit))

/* DMA register read/write macros */
#define RD_DMA_CH_REG32(base_address, ch, reg_offset)          *(uint32_t  \
    volatile *)((uintptr_t)(base_address) + XGMAC_DMA_CHANNEL_BASE + ((ch) * \
    XGMAC_DMA_CHANNEL_INC) + (reg_offset))
#define WR_DMA_CH_REG32(base_address, ch, reg_offset, data)    (*(uint32_t \
    volatile *)((uintptr_t)(base_address) + XGMAC_DMA_CHANNEL_BASE + ((ch) * \
    XGMAC_DMA_CHANNEL_INC) + (reg_offset)) = (data))

typedef enum
{
    INTERRUPT_TI,       /* Transmit Interrupt .*/
    INTERRUPT_TXS,      /* Transmit Stopped. */
    INTERRUPT_TBU,      /*Transmit Buffer Unavailable.*/
    INTERRUPT_RI,       /* Receive Interrupt */
    INTERRUPT_RBU,      /* Receive Buffer Unavailable */
    INTERRUPT_RS,       /* Receive Stopped */
    INTERRUPT_DDE,      /* Descriptor Definition Error  */
    INTERRUPT_FBE,      /* Fatal Bus Error  */
    INTERRUPT_CDE,      /* Context Descriptor Error  */
    INTERRUPT_AIS,      /* Abnormal Interrupt Summary */
    INTERRUPT_NIS,      /* Normal Interrupt Summary */
    INTERRUPT_UNHANDLED      /* Unhandled Interrupt  */
} xgmac_dma_interrupt_id_t;

typedef struct
{
    uint32_t tx_ring_len;
    uint32_t rx_ring_len;
    uint32_t tx_desc_high_addr;
    uint32_t tx_desc_low_addr;
    uint32_t rx_desc_high_addr;
    uint32_t rx_desc_low_addr;
    uint32_t tx_last_desc_addr;
    uint32_t rx_last_desc_addr;
} xgmac_dma_desc_addr_t;

/*
 * @brief  Configuration structure for XGMAC DMA Parameters
 */
typedef struct
{
    bool ubl;
    bool blen4;
    bool blen8;
    bool blen16;
    bool blen32;
    bool blen64;
    bool blen128;
    bool blen256;
    bool aal;
    bool eame;
    uint8_t rd_osr_lmt;
    uint8_t wr_osr_lmt;
} xgmacdma_config_t;


/*
 * @brief  Configuration structure for XGMAC DMA Channel Parameters
 */
typedef struct
{
    /* DMA channel Control register fields */
    bool pblx8;
    bool sph;
    uint8_t dsl;

    /* DMA channel Tx Control register fields */
    bool tse;
    uint8_t txpbl;
    uint8_t tqos;

    /* DMA channel Rx Control register fields */
    uint8_t rxpbl;
    uint8_t rqos;
    uint16_t rbsz;
} xgmacdma_ch_config_t;


/*
 * @brief  Configuration structure for XGMAC MAC Rx parameters
 */
typedef struct
{
    /* MAC RxQ control0 register fields */
    uint8_t rxq0en;
    uint8_t rxq1en;
    uint8_t rxq2en;
    uint8_t rxq3en;
    uint8_t rxq4en;
    uint8_t rxq5en;
    uint8_t rxq6en;
    uint8_t rxq7en;

    /* MAC RxQ control1 register fields */
    bool mcbcqen;
} xgmacmac_rx_q_ctrl_config_t;


/*
 * @brief   Configuration structure for XGMAC MTL Tx Queue parameters
 */
typedef struct
{
    bool tsf;
    uint8_t txqen;
    uint8_t tqs;
} xgmacmtl_tx_queue_config_t;


/*
 * @brief   Configuration structure for XGMAC MTL Rx Queue parameters
 */
typedef struct
{
    bool rsf;
    bool ehfc;
    uint8_t rqs;
} xgmacmtl_rx_queue_config_t;


/*
 * @brief  Configuration structure for XGMAC MAC Tx Flow Control parameters
 */
typedef struct
{
    bool fcb;
    bool tfe;
    uint8_t plt;
    bool dzpq;
    uint32_t pt;
} xgmacmac_tx_flow_ctrl_config_t;

/*
 * @brief  Configuration structure for XGMAC MAC Rx Flow Control parameters
 */
typedef struct
{
    bool rfe;
    bool up;
    bool pfce;
} xgmacmac_rx_flow_ctrl_config_t;


/*
 * @brief  Configuration structure for XGMAC MAC Tx parameters
 */
typedef struct
{
    bool jd;
} xgmacmac_tx_config_t;


/*
 * @brief  Configuration structure for XGMAC MAC Rx parameters
 */
typedef struct
{
    bool acs;
    bool cst;
    bool dcrcc;
    bool spen;
    bool usp;
    bool gpslce;
    bool wd;
    bool je;
    bool ipc;
    uint16_t gpsl;
    bool arpen;
} xgmacmac_rx_config_t;

/*
 * @brief  Configuration structure for XGMAC MAC Packet Filter parameters
 */
typedef struct
{
    bool pr;
    bool huc;
    bool hmc;
    bool daif;
    bool pm;
    bool dbf;
    uint8_t pcf;
    bool saif;
    bool saf;
    bool hpf;
    uint8_t dhlfrs;
    bool vtfe;
    bool ipfe;
    bool dntu;
    bool ra;
} xgmacmac_pkt_filter_config_t;

/*
 * @brief  Configuration structure for XGMAC Device Parameters
 */
typedef struct
{
    uint8_t dma_ch_num;
    uint8_t tx_queue_num;
    uint8_t rx_queue_num;
} xgmac_dev_config_t;

/*
 * @brief  Configuration structure for XGMAC DMA, MTL and MAC Parameters
 */
typedef struct
{
    const xgmac_dev_config_t *mac_dev_config;
    const xgmacdma_config_t *dma_config;
    const xgmacdma_ch_config_t *dma_ch_config;
    const xgmacmtl_tx_queue_config_t *mtl_tx_q_config;
    const xgmacmtl_rx_queue_config_t *mtl_rx_q_config;
    const xgmacmac_rx_q_ctrl_config_t *mac_rx_q_ctrl_config;
    const xgmacmac_tx_flow_ctrl_config_t *mac_tx_flow_ctrl_config;
    const xgmacmac_rx_flow_ctrl_config_t *mac_rx_flow_ctrl_config;
    const xgmacmac_tx_config_t *mac_tx_config;
    const xgmacmac_rx_config_t *mac_rx_config;
    const xgmacmac_pkt_filter_config_t *mac_pkt_filter_config;

} xgmac_dev_config_str_t;

void xgmac_ll_set_duplex(uint32_t base_address, uint8_t duplex);
void xgmac_ll_set_speed(uint32_t base_address, uint32_t speed);
bool xgmac_ll_dma_init(uint32_t base_address, const
        xgmac_dev_config_str_t *xgmac_dev_config);
void xgmac_ll_init_dma_ch_desc_reg(uint32_t base_address, uint8_t ch,
        xgmac_dma_desc_addr_t *desc_params);
void xgmac_ll_config_dma_ch_control(uint32_t base_address, uint8_t ch, const
        xgmacdma_ch_config_t *ch_config);
int32_t xgmac_ll_enable_dma_interrupt(uint32_t base_address, uint8_t ch,
        xgmac_dma_interrupt_id_t id);
void xgmac_ll_start_dma_dev(uint32_t base_address, const
        xgmac_dev_config_str_t *xgmac_dev_config);
void xgmac_ll_mac_start(uint32_t base_address);
void xgmac_ll_mac_init(uint32_t mac_base_address, uint32_t mtl_base_address,
        const xgmac_dev_config_str_t *xgmac_dev_config);
void xgmac_ll_disable_interrupt(uint32_t base_address);
xgmac_dma_interrupt_id_t xgmac_ll_check_and_clear_xgmac_interrupt_status(
    uint32_t base_address);
void xgmac_ll_check_and_clear_link_interrupt_status(uint32_t base_address);

#endif
