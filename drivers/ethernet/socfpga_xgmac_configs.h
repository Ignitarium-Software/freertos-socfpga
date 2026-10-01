/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * This contains the configuration initializations for SoC FPGA XGMAC DMA, MTL
 * and MAC registers
 */

#ifndef __SOCFPGA_XGMAC_CONFIGS_H__
#define __SOCFPGA_XGMAC_CONFIGS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "socfpga_xgmac.h"
#include "socfpga_xgmac_ll.h"

/**
 * @brief  Configuration parameters for XGMAC DMA Parameters
 */
static const xgmacdma_config_t dma_config = {

    .ubl = 0,
    .blen4 = 1,
    .blen8 = 1,
    .blen16 = 1,
    .blen32 = 1,
    .blen64 = 0,
    .blen128 = 0,
    .blen256 = 0,
    .aal = 0,
    .eame = 1,
    .rd_osr_lmt = 31,
    .wr_osr_lmt = 31,
};

/**
 * @brief  Configuration parameters for XGMAC DMA channel Parameters
 */

static const xgmacdma_ch_config_t dma_ch_config = {
    /* DMA channel Control register */
    .pblx8 = 1,
    .sph = 0,
    .dsl = 0,

    /* DMA channel Tx Control register */
    .tse = 0,
    .txpbl = 32,

    /* DMA channel Rx Control register */
    .rxpbl = 8,
    .rbsz = XGMAC_MAX_PACKET_SIZE,
};


/**
 * @brief  Configuration parameters for XGMAC MAC Rx parameters
 */
static const xgmacmac_rx_q_ctrl_config_t mac_rxq_ctrl_config = {
    /* MAC RxQ control0 register fields */
    .rxq0en = 2,

    /* rxq1end - rxq7en not supported in this version */

    /* MAC RxQ control1 register fields */
    .mcbcqen = 1,
};

/**
 * @brief   Configuration parameters for XGMAC MTL Tx Queue parameters
 */
static const xgmacmtl_tx_queue_config_t mtl_txq_config = {
    .tsf =  1,
    .txqen = 2,
};

/**
 * @brief   Configuration parameters for XGMAC MTL Rx Queue parameters
 */
static const xgmacmtl_rx_queue_config_t mtl_rxq_config = {
    .rsf = 1,
    .ehfc = 0,
};


/**
 * @brief  Configuration parameters for XGMAC MAC Tx Flow Control parameters
 */
static const xgmacmac_tx_flow_ctrl_config_t mac_tx_flow_ctrl_config = {
    .fcb = 0,
    .tfe = 0,
    .plt = 0,
    .dzpq = 0,
    .pt = 0xFFFF,
};

/**
 * @brief  Configuration parameters for XGMAC MAC Rx Flow Control parameters
 */
static const xgmacmac_rx_flow_ctrl_config_t mac_rx_flow_ctrl_config = {
    .rfe = 0,
    .up = 0,
    .pfce = 0,
};
/**
 * @brief  Configuration parameters for XGMAC MAC Tx parameters
 */
static const xgmacmac_tx_config_t mac_tx_config = {
    .jd = 0,
};

/**
 * @brief  Configuration parameters for XGMAC MAC Rx parameters
 */
static const xgmacmac_rx_config_t mac_rx_config = {
    .acs = 0,
    .cst = 0,
    .dcrcc =  0,
    .spen  =  0,
    .usp  =  0,
    .gpslce = 0,
    .wd = 0,
    .je = 0,
    .ipc = 1,
    .gpsl = 0,
    .arpen = 0,
};

/**
 * @brief  Configuration parameters for XGMAC MAC Packet Filter parameters
 */
static const xgmacmac_pkt_filter_config_t mac_pkt_filter_config = {
    .pr = 0,
    .huc = 0,
    .hmc = 0,
    .daif = 0,
    .pm = 0,
    .dbf = 0,
    .pcf = 0,
    .saif = 0,
    .saf = 0,
    .hpf = 0,
    .dhlfrs = 0,
    .vtfe = 0,
    .ipfe = 0,
    .dntu = 0,
    .ra = 1,
};

/**
 * @brief  Configuration parameters for XGMAC Device Parameters
 */
static const xgmac_dev_config_t mac_dev_config = {

    .dma_ch_num = 1,
    .tx_queue_num = 1,
    .rx_queue_num = 1,
};

/* Main XGMAC Configuration instance */
static const xgmac_dev_config_str_t xgmac_dev_config_str =
{
    .mac_dev_config = &mac_dev_config,
    .dma_config = &dma_config,
    .dma_ch_config = &dma_ch_config,
    .mtl_tx_q_config = &mtl_txq_config,
    .mtl_rx_q_config = &mtl_rxq_config,
    .mac_rx_q_ctrl_config = &mac_rxq_ctrl_config,
    .mac_tx_flow_ctrl_config = &mac_tx_flow_ctrl_config,
    .mac_rx_flow_ctrl_config = &mac_rx_flow_ctrl_config,
    .mac_tx_config = &mac_tx_config,
    .mac_rx_config = &mac_rx_config,
    .mac_pkt_filter_config = &mac_pkt_filter_config,
};

#endif /* __SOCFPGA_XGMAC_CONFIGS_H__ */
