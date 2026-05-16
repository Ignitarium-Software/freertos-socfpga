/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Register definitions for SoC FPGA i2c block
 */

#ifndef __SOCFPGA_I2C_REG_H__
#define __SOCFPGA_I2C_REG_H__

/* Get base addr */
#define GET_I2C_BASE_ADDR(instance)       ((uint32_t)(0x10C02800U + \
    ((instance) * 0x100U)))

/* I2C Control Register */
#define I2C_CON                                           0x0000U
/* I2C Target Address Register */
#define I2C_TAR                                           0x0004U
/* I2C Slave Address Register */
#define I2C_SAR                                           0x0008U
/* I2C Rx/Tx Data Buffer and Command Register */
#define I2C_DATA_CMD                                      0x0010U
/* Standard Speed I2C Clock SCL High Count Register */
#define I2C_SS_SCL_HCNT                                   0x0014U
/* Standard Speed I2C Clock SCL Low Count Register */
#define I2C_SS_SCL_LCNT                                   0x0018U
/* Fast Mode or Fast Mode Plus I2C Clock SCL High Count Register */
#define I2C_FS_SCL_HCNT                                   0x001CU
/* Fast Mode or Fast Mode Plus I2C Clock SCL Low Count Register */
#define I2C_FS_SCL_LCNT                                   0x0020U
/* I2C Interrupt Status Register */
#define I2C_INTR_STAT                                     0x002CU
/* I2C Interrupt Mask Register */
#define I2C_INTR_MASK                                     0x0030U
/* I2C Raw Interrupt Status Register */
#define I2C_RAW_INTR_STAT                                 0x0034U
/* I2C Receive FIFO Threshold Register */
#define I2C_RX_TL                                         0x0038U
/* I2C Transmit FIFO Threshold Register */
#define I2C_TX_TL                                         0x003CU
/* Clear Combined and Individual Interrupt Register */
#define I2C_CLR_INTR                                      0x0040U
/* Clear RX_UNDER Interrupt Register */
#define I2C_CLR_RX_UNDER                                  0x0044U
/* Clear RX_OVER Interrupt Register */
#define I2C_CLR_RX_OVER                                   0x0048U
/* Clear TX_OVER Interrupt Register */
#define I2C_CLR_TX_OVER                                   0x004CU
/* Clear RD_REQ Interrupt Register */
#define I2C_CLR_RD_REQ                                    0x0050U
/* Clear TX_ABRT Interrupt Register */
#define I2C_CLR_TX_ABRT                                   0x0054U
/* Clear RX_DONE Interrupt Register */
#define I2C_CLR_RX_DONE                                   0x0058U
/* Clear ACTIVITY Interrupt Register */
#define I2C_CLR_ACTIVITY                                  0x005CU
/* Clear STOP_DET Interrupt Register */
#define I2C_CLR_STOP_DET                                  0x0060U
/* Clear START_DET Interrupt Register */
#define I2C_CLR_START_DET                                 0x0064U
/* Clear GEN_CALL Interrupt Register */
#define I2C_CLR_GEN_CALL                                  0x0068U
/* I2C ENABLE Register */
#define I2C_ENABLE                                        0x006CU
/* I2C STATUS Register */
#define I2C_STATUS                                        0x0070U
/* I2C Transmit FIFO Level Register */
#define I2C_TXFLR                                         0x0074U
/* I2C Receive FIFO Level Register */
#define I2C_RXFLR                                         0x0078U
/* I2C SDA Hold Time Length Register */
#define I2C_SDA_HOLD                                      0x007CU
/* I2C Transmit Abort Source Register */
#define I2C_TX_ABRT_SOURCE                                0x0080U
/* Generate Slave Data NACK Register */
#define I2C_SLV_DATA_NACK_ONLY                            0x0084U
/* DMA Control Register */
#define I2C_DMA_CR                                        0x0088U
/* DMA Transmit Data Level Register */
#define I2C_DMA_TDLR                                      0x008CU
/* DMA Receive Data Level Register */
#define I2C_DMA_RDLR                                      0x0090U
/* I2C SDA Setup Register */
#define I2C_SDA_SETUP                                     0x0094U
/* I2C ACK General Call Register */
#define I2C_ACK_GENERAL_CALL                              0x0098U
/* I2C Enable Status Register */
#define I2C_ENABLE_STATUS                                 0x009CU
/* I2C SS, FS or FM+  spike suppression limit */
#define I2C_FS_SPKLEN                                     0x00A0U
/* Clear RESTART_DET Interrupt Register */
#define I2C_CLR_RESTART_DET                               0x00A8U
/* Component Parameter Register 1 */
#define I2C_COMP_PARAM_1                                  0x00F4U
/* I2C Component Version Register */
#define I2C_COMP_VERSION                                  0x00F8U
/* I2C Component Type Register */
#define I2C_COMP_TYPE                                     0x00FCU

/* Bit fields of IC_CON register */
#define I2C_CON_STOP_DET_IF_MASTER_ACTIVE_MASK            0x00000400U
#define I2C_CON_STOP_DET_IF_MASTER_ACTIVE_POS             10U
#define I2C_CON_RX_FIFO_FULL_HLD_CTRL_MASK                0x00000200U
#define I2C_CON_RX_FIFO_FULL_HLD_CTRL_POS                 9U
#define I2C_CON_TX_EMPTY_CTRL_MASK                        0x00000100U
#define I2C_CON_TX_EMPTY_CTRL_POS                         8U
#define I2C_CON_STOP_DET_IFADDRESSED_MASK                 0x00000080U
#define I2C_CON_STOP_DET_IFADDRESSED_POS                  7U
#define I2C_CON_IC_SLAVE_DISABLE_MASK                     0x00000040U
#define I2C_CON_IC_SLAVE_DISABLE_POS                      6U
#define I2C_CON_IC_RESTART_EN_MASK                        0x00000020U
#define I2C_CON_IC_RESTART_EN_POS                         5U
#define I2C_CON_IC_10BITADDR_MASTER_RD_ONLY_MASK          0x00000010U
#define I2C_CON_IC_10BITADDR_MASTER_RD_ONLY_POS           4U
#define I2C_CON_IC_10BITADDR_SLAVE_MASK                   0x00000008U
#define I2C_CON_IC_10BITADDR_SLAVE_POS                    3U
#define I2C_CON_SPEED_MASK                                0x00000006U
#define I2C_CON_SPEED_POS                                 1U
#define I2C_CON_MASTER_MODE_MASK                          0x00000001U
#define I2C_CON_MASTER_MODE_POS                           0U

/* Bit fields of IC_TAR register */
#define I2C_TAR_IC_10BITADDR_MASTER_MASK                  0x00001000U
#define I2C_TAR_IC_10BITADDR_MASTER_POS                   12U
#define I2C_TAR_SPECIAL_MASK                              0x00000800U
#define I2C_TAR_SPECIAL_POS                               11U
#define I2C_TAR_GC_OR_START_MASK                          0x00000400U
#define I2C_TAR_GC_OR_START_POS                           10U
#define I2C_TAR_IC_TAR_MASK                               0x000003FFU
#define I2C_TAR_IC_TAR_POS                                0U

/* Bit fields of IC_SAR register */
#define I2C_SAR_IC_SAR_MASK                               0x000003FFU
#define I2C_SAR_IC_SAR_POS                                0U

/* Bit fields of IC_DATA_CMD register */
#define I2C_DATA_CMD_FIRST_DATA_BYTE_MASK                 0x00000800U
#define I2C_DATA_CMD_FIRST_DATA_BYTE_POS                  11U
#define I2C_DATA_CMD_RESTART_MASK                         0x00000400U
#define I2C_DATA_CMD_RESTART_POS                          10U
#define I2C_DATA_CMD_STOP_MASK                            0x00000200U
#define I2C_DATA_CMD_STOP_POS                             9U
#define I2C_DATA_CMD_CMD_MASK                             0x00000100U
#define I2C_DATA_CMD_CMD_POS                              8U
#define I2C_DATA_CMD_DAT_MASK                             0x000000FFU
#define I2C_DATA_CMD_DAT_POS                              0U

/* Bit fields of IC_SS_SCL_HCNT register */
#define I2C_SS_SCL_HCNT_IC_SS_SCL_HCNT_MASK               0x0000FFFFU
#define I2C_SS_SCL_HCNT_IC_SS_SCL_HCNT_POS                0U

/* Bit fields of IC_SS_SCL_LCNT register */
#define I2C_SS_SCL_LCNT_IC_SS_SCL_LCNT_MASK               0x0000FFFFU
#define I2C_SS_SCL_LCNT_IC_SS_SCL_LCNT_POS                0U

/* Bit fields of IC_FS_SCL_HCNT register */
#define I2C_FS_SCL_HCNT_IC_FS_SCL_HCNT_MASK               0x0000FFFFU
#define I2C_FS_SCL_HCNT_IC_FS_SCL_HCNT_POS                0U

/* Bit fields of IC_FS_SCL_LCNT register */
#define I2C_FS_SCL_LCNT_IC_FS_SCL_LCNT_MASK               0x0000FFFFU
#define I2C_FS_SCL_LCNT_IC_FS_SCL_LCNT_POS                0U

/* Bit fields of IC_INTR_STAT register */
#define I2C_INTR_STAT_R_MASTER_ON_HOLD_MASK               0x00002000U
#define I2C_INTR_STAT_R_MASTER_ON_HOLD_POS                13U
#define I2C_INTR_STAT_R_RESTART_DET_MASK                  0x00001000U
#define I2C_INTR_STAT_R_RESTART_DET_POS                   12U
#define I2C_INTR_STAT_R_GEN_CALL_MASK                     0x00000800U
#define I2C_INTR_STAT_R_GEN_CALL_POS                      11U
#define I2C_INTR_STAT_R_START_DET_MASK                    0x00000400U
#define I2C_INTR_STAT_R_START_DET_POS                     10U
#define I2C_INTR_STAT_R_STOP_DET_MASK                     0x00000200U
#define I2C_INTR_STAT_R_STOP_DET_POS                      9U
#define I2C_INTR_STAT_R_ACTIVITY_MASK                     0x00000100U
#define I2C_INTR_STAT_R_ACTIVITY_POS                      8U
#define I2C_INTR_STAT_R_RX_DONE_MASK                      0x00000080U
#define I2C_INTR_STAT_R_RX_DONE_POS                       7U
#define I2C_INTR_STAT_R_TX_ABRT_MASK                      0x00000040U
#define I2C_INTR_STAT_R_TX_ABRT_POS                       6U
#define I2C_INTR_STAT_R_RD_REQ_MASK                       0x00000020U
#define I2C_INTR_STAT_R_RD_REQ_POS                        5U
#define I2C_INTR_STAT_R_TX_EMPTY_MASK                     0x00000010U
#define I2C_INTR_STAT_R_TX_EMPTY_POS                      4U
#define I2C_INTR_STAT_R_TX_OVER_MASK                      0x00000008U
#define I2C_INTR_STAT_R_TX_OVER_POS                       3U
#define I2C_INTR_STAT_R_RX_FULL_MASK                      0x00000004U
#define I2C_INTR_STAT_R_RX_FULL_POS                       2U
#define I2C_INTR_STAT_R_RX_OVER_MASK                      0x00000002U
#define I2C_INTR_STAT_R_RX_OVER_POS                       1U
#define I2C_INTR_STAT_R_RX_UNDER_MASK                     0x00000001U
#define I2C_INTR_STAT_R_RX_UNDER_POS                      0U

/* Bit fields of IC_INTR_MASK register */
#define I2C_INTR_MASK_M_MASTER_ON_HOLD_MASK               0x00002000U
#define I2C_INTR_MASK_M_MASTER_ON_HOLD_POS                13U
#define I2C_INTR_MASK_M_RESTART_DET_MASK                  0x00001000U
#define I2C_INTR_MASK_M_RESTART_DET_POS                   12U
#define I2C_INTR_MASK_M_GEN_CALL_MASK                     0x00000800U
#define I2C_INTR_MASK_M_GEN_CALL_POS                      11U
#define I2C_INTR_MASK_M_START_DET_MASK                    0x00000400U
#define I2C_INTR_MASK_M_START_DET_POS                     10U
#define I2C_INTR_MASK_M_STOP_DET_MASK                     0x00000200U
#define I2C_INTR_MASK_M_STOP_DET_POS                      9U
#define I2C_INTR_MASK_M_ACTIVITY_MASK                     0x00000100U
#define I2C_INTR_MASK_M_ACTIVITY_POS                      8U
#define I2C_INTR_MASK_M_RX_DONE_MASK                      0x00000080U
#define I2C_INTR_MASK_M_RX_DONE_POS                       7U
#define I2C_INTR_MASK_M_TX_ABRT_MASK                      0x00000040U
#define I2C_INTR_MASK_M_TX_ABRT_POS                       6U
#define I2C_INTR_MASK_M_RD_REQ_MASK                       0x00000020U
#define I2C_INTR_MASK_M_RD_REQ_POS                        5U
#define I2C_INTR_MASK_M_TX_EMPTY_MASK                     0x00000010U
#define I2C_INTR_MASK_M_TX_EMPTY_POS                      4U
#define I2C_INTR_MASK_M_TX_OVER_MASK                      0x00000008U
#define I2C_INTR_MASK_M_TX_OVER_POS                       3U
#define I2C_INTR_MASK_M_RX_FULL_MASK                      0x00000004U
#define I2C_INTR_MASK_M_RX_FULL_POS                       2U
#define I2C_INTR_MASK_M_RX_OVER_MASK                      0x00000002U
#define I2C_INTR_MASK_M_RX_OVER_POS                       1U
#define I2C_INTR_MASK_M_RX_UNDER_MASK                     0x00000001U
#define I2C_INTR_MASK_M_RX_UNDER_POS                      0U

/* Bit fields of IC_RAW_INTR_STAT register */
#define I2C_RAW_INTR_STAT_MASTER_ON_HOLD_MASK             0x00002000U
#define I2C_RAW_INTR_STAT_MASTER_ON_HOLD_POS              13U
#define I2C_RAW_INTR_STAT_RESTART_DET_MASK                0x00001000U
#define I2C_RAW_INTR_STAT_RESTART_DET_POS                 12U
#define I2C_RAW_INTR_STAT_GEN_CALL_MASK                   0x00000800U
#define I2C_RAW_INTR_STAT_GEN_CALL_POS                    11U
#define I2C_RAW_INTR_STAT_START_DET_MASK                  0x00000400U
#define I2C_RAW_INTR_STAT_START_DET_POS                   10U
#define I2C_RAW_INTR_STAT_STOP_DET_MASK                   0x00000200U
#define I2C_RAW_INTR_STAT_STOP_DET_POS                    9U
#define I2C_RAW_INTR_STAT_RAW_INTR_ACTIVITY_MASK          0x00000100U
#define I2C_RAW_INTR_STAT_RAW_INTR_ACTIVITY_POS           8U
#define I2C_RAW_INTR_STAT_RX_DONE_MASK                    0x00000080U
#define I2C_RAW_INTR_STAT_RX_DONE_POS                     7U
#define I2C_RAW_INTR_STAT_TX_ABRT_MASK                    0x00000040U
#define I2C_RAW_INTR_STAT_TX_ABRT_POS                     6U
#define I2C_RAW_INTR_STAT_RD_REQ_MASK                     0x00000020U
#define I2C_RAW_INTR_STAT_RD_REQ_POS                      5U
#define I2C_RAW_INTR_STAT_TX_EMPTY_MASK                   0x00000010U
#define I2C_RAW_INTR_STAT_TX_EMPTY_POS                    4U
#define I2C_RAW_INTR_STAT_TX_OVER_MASK                    0x00000008U
#define I2C_RAW_INTR_STAT_TX_OVER_POS                     3U
#define I2C_RAW_INTR_STAT_RX_FULL_MASK                    0x00000004U
#define I2C_RAW_INTR_STAT_RX_FULL_POS                     2U
#define I2C_RAW_INTR_STAT_RX_OVER_MASK                    0x00000002U
#define I2C_RAW_INTR_STAT_RX_OVER_POS                     1U
#define I2C_RAW_INTR_STAT_RX_UNDER_MASK                   0x00000001U
#define I2C_RAW_INTR_STAT_RX_UNDER_POS                    0U

/* Bit fields of IC_RX_TL register */
#define I2C_RX_TL_RX_TL_MASK                              0x000000FFU
#define I2C_RX_TL_RX_TL_POS                               0U

/* Bit fields of IC_TX_TL register */
#define I2C_TX_TL_TX_TL_MASK                              0x000000FFU
#define I2C_TX_TL_TX_TL_POS                               0U

/* Bit fields of IC_CLR_INTR register */
#define I2C_CLR_INTR_CLR_INTR_MASK                        0x00000001U
#define I2C_CLR_INTR_CLR_INTR_POS                         0U

/* Bit fields of IC_CLR_RX_UNDER register */
#define I2C_CLR_RX_UNDER_CLR_RX_UNDER_MASK                0x00000001U
#define I2C_CLR_RX_UNDER_CLR_RX_UNDER_POS                 0U

/* Bit fields of IC_CLR_RX_OVER register */
#define I2C_CLR_RX_OVER_CLR_RX_OVER_MASK                  0x00000001U
#define I2C_CLR_RX_OVER_CLR_RX_OVER_POS                   0U

/* Bit fields of IC_CLR_TX_OVER register */
#define I2C_CLR_TX_OVER_CLR_TX_OVER_MASK                  0x00000001U
#define I2C_CLR_TX_OVER_CLR_TX_OVER_POS                   0U

/* Bit fields of IC_CLR_RD_REQ register */
#define I2C_CLR_RD_REQ_CLR_RD_REQ_MASK                    0x00000001U
#define I2C_CLR_RD_REQ_CLR_RD_REQ_POS                     0U

/* Bit fields of IC_CLR_TX_ABRT register */
#define I2C_CLR_TX_ABRT_CLR_TX_ABRT_MASK                  0x00000001U
#define I2C_CLR_TX_ABRT_CLR_TX_ABRT_POS                   0U

/* Bit fields of IC_CLR_RX_DONE register */
#define I2C_CLR_RX_DONE_CLR_RX_DONE_MASK                  0x00000001U
#define I2C_CLR_RX_DONE_CLR_RX_DONE_POS                   0U

/* Bit fields of IC_CLR_ACTIVITY register */
#define I2C_CLR_ACTIVITY_CLR_ACTIVITY_MASK                0x00000001U
#define I2C_CLR_ACTIVITY_CLR_ACTIVITY_POS                 0U

/* Bit fields of IC_CLR_STOP_DET register */
#define I2C_CLR_STOP_DET_CLR_STOP_DET_MASK                0x00000001U
#define I2C_CLR_STOP_DET_CLR_STOP_DET_POS                 0U

/* Bit fields of IC_CLR_START_DET register */
#define I2C_CLR_START_DET_CLR_START_DET_MASK              0x00000001U
#define I2C_CLR_START_DET_CLR_START_DET_POS               0U

/* Bit fields of IC_CLR_GEN_CALL register */
#define I2C_CLR_GEN_CALL_CLR_GEN_CALL_MASK                0x00000001U
#define I2C_CLR_GEN_CALL_CLR_GEN_CALL_POS                 0U

/* Bit fields of IC_ENABLE register */
#define I2C_ENABLE_TX_CMD_BLOCK_MASK                      0x00000004U
#define I2C_ENABLE_TX_CMD_BLOCK_POS                       2U
#define I2C_ENABLE_ABORT_MASK                             0x00000002U
#define I2C_ENABLE_ABORT_POS                              1U
#define I2C_ENABLE_ENABLE_MASK                            0x00000001U
#define I2C_ENABLE_ENABLE_POS                             0U

/* Bit fields of IC_STATUS register */
#define I2C_STATUS_SLV_ACTIVITY_MASK                      0x00000040U
#define I2C_STATUS_SLV_ACTIVITY_POS                       6U
#define I2C_STATUS_MST_ACTIVITY_MASK                      0x00000020U
#define I2C_STATUS_MST_ACTIVITY_POS                       5U
#define I2C_STATUS_RFF_MASK                               0x00000010U
#define I2C_STATUS_RFF_POS                                4U
#define I2C_STATUS_RFNE_MASK                              0x00000008U
#define I2C_STATUS_RFNE_POS                               3U
#define I2C_STATUS_TFE_MASK                               0x00000004U
#define I2C_STATUS_TFE_POS                                2U
#define I2C_STATUS_TFNF_MASK                              0x00000002U
#define I2C_STATUS_TFNF_POS                               1U
#define I2C_STATUS_IC_STATUS_ACTIVITY_MASK                0x00000001U
#define I2C_STATUS_IC_STATUS_ACTIVITY_POS                 0U

/* Bit fields of IC_TXFLR register */
#define I2C_TXFLR_TXFLR_MASK                              0x0000007FU
#define I2C_TXFLR_TXFLR_POS                               0U

/* Bit fields of IC_RXFLR register */
#define I2C_RXFLR_RXFLR_MASK                              0x0000007FU
#define I2C_RXFLR_RXFLR_POS                               0U

/* Bit fields of IC_SDA_HOLD register */
#define I2C_SDA_HOLD_IC_SDA_RX_HOLD_MASK                  0x00FF0000U
#define I2C_SDA_HOLD_IC_SDA_RX_HOLD_POS                   16U
#define I2C_SDA_HOLD_IC_SDA_TX_HOLD_MASK                  0x0000FFFFU
#define I2C_SDA_HOLD_IC_SDA_TX_HOLD_POS                   0U

/* Bit fields of IC_TX_ABRT_SOURCE register */
#define I2C_TX_ABRT_SOURCE_TX_FLUSH_CNT_MASK              0xFF800000U
#define I2C_TX_ABRT_SOURCE_TX_FLUSH_CNT_POS               23U
#define I2C_TX_ABRT_SOURCE_ABRT_USER_ABRT_MASK            0x00010000U
#define I2C_TX_ABRT_SOURCE_ABRT_USER_ABRT_POS             16U
#define I2C_TX_ABRT_SOURCE_ABRT_SLVRD_INTX_MASK           0x00008000U
#define I2C_TX_ABRT_SOURCE_ABRT_SLVRD_INTX_POS            15U
#define I2C_TX_ABRT_SOURCE_ABRT_SLV_ARBLOST_MASK          0x00004000U
#define I2C_TX_ABRT_SOURCE_ABRT_SLV_ARBLOST_POS           14U
#define I2C_TX_ABRT_SOURCE_ABRT_SLVFLUSH_TXFIFO_MASK      0x00002000U
#define I2C_TX_ABRT_SOURCE_ABRT_SLVFLUSH_TXFIFO_POS       13U
#define I2C_TX_ABRT_SOURCE_ARB_LOST_MASK                  0x00001000U
#define I2C_TX_ABRT_SOURCE_ARB_LOST_POS                   12U
#define I2C_TX_ABRT_SOURCE_ABRT_MASTER_DIS_MASK           0x00000800U
#define I2C_TX_ABRT_SOURCE_ABRT_MASTER_DIS_POS            11U
#define I2C_TX_ABRT_SOURCE_ABRT_10B_RD_NORSTRT_MASK       0x00000400U
#define I2C_TX_ABRT_SOURCE_ABRT_10B_RD_NORSTRT_POS        10U
#define I2C_TX_ABRT_SOURCE_ABRT_SBYTE_NORSTRT_MASK        0x00000200U
#define I2C_TX_ABRT_SOURCE_ABRT_SBYTE_NORSTRT_POS         9U
#define I2C_TX_ABRT_SOURCE_ABRT_HS_NORSTRT_MASK           0x00000100U
#define I2C_TX_ABRT_SOURCE_ABRT_HS_NORSTRT_POS            8U
#define I2C_TX_ABRT_SOURCE_ABRT_SBYTE_ACKDET_MASK         0x00000080U
#define I2C_TX_ABRT_SOURCE_ABRT_SBYTE_ACKDET_POS          7U
#define I2C_TX_ABRT_SOURCE_ABRT_HS_ACKDET_MASK            0x00000040U
#define I2C_TX_ABRT_SOURCE_ABRT_HS_ACKDET_POS             6U
#define I2C_TX_ABRT_SOURCE_ABRT_GCALL_READ_MASK           0x00000020U
#define I2C_TX_ABRT_SOURCE_ABRT_GCALL_READ_POS            5U
#define I2C_TX_ABRT_SOURCE_ABRT_GCALL_NOACK_MASK          0x00000010U
#define I2C_TX_ABRT_SOURCE_ABRT_GCALL_NOACK_POS           4U
#define I2C_TX_ABRT_SOURCE_ABRT_TXDATA_NOACK_MASK         0x00000008U
#define I2C_TX_ABRT_SOURCE_ABRT_TXDATA_NOACK_POS          3U
#define I2C_TX_ABRT_SOURCE_ABRT_10ADDR2_NOACK_MASK        0x00000004U
#define I2C_TX_ABRT_SOURCE_ABRT_10ADDR2_NOACK_POS         2U
#define I2C_TX_ABRT_SOURCE_ABRT_10ADDR1_NOACK_MASK        0x00000002U
#define I2C_TX_ABRT_SOURCE_ABRT_10ADDR1_NOACK_POS         1U
#define I2C_TX_ABRT_SOURCE_ABRT_7B_ADDR_NOACK_MASK        0x00000001U
#define I2C_TX_ABRT_SOURCE_ABRT_7B_ADDR_NOACK_POS         0U

/* Bit fields of IC_SLV_DATA_NACK_ONLY register */
#define I2C_SLV_DATA_NACK_ONLY_NACK_MASK                  0x00000001U
#define I2C_SLV_DATA_NACK_ONLY_NACK_POS                   0U

/* Bit fields of IC_DMA_CR register */
#define I2C_DMA_CR_TDMAE_MASK                             0x00000002U
#define I2C_DMA_CR_TDMAE_POS                              1U
#define I2C_DMA_CR_RDMAE_MASK                             0x00000001U
#define I2C_DMA_CR_RDMAE_POS                              0U

/* Bit fields of IC_DMA_TDLR register */
#define I2C_DMA_TDLR_DMATDL_MASK                          0x0000003FU
#define I2C_DMA_TDLR_DMATDL_POS                           0U

/* Bit fields of IC_DMA_RDLR register */
#define I2C_DMA_RDLR_DMARDL_MASK                          0x0000003FU
#define I2C_DMA_RDLR_DMARDL_POS                           0U

/* Bit fields of IC_SDA_SETUP register */
#define I2C_SDA_SETUP_SDA_SETUP_MASK                      0x000000FFU
#define I2C_SDA_SETUP_SDA_SETUP_POS                       0U

/* Bit fields of IC_ACK_GENERAL_CALL register */
#define I2C_ACK_GENERAL_CALL_ACK_GEN_CALL_MASK            0x00000001U
#define I2C_ACK_GENERAL_CALL_ACK_GEN_CALL_POS             0U

/* Bit fields of IC_ENABLE_STATUS register */
#define I2C_ENABLE_STATUS_SLV_RX_DATA_LOST_MASK           0x00000004U
#define I2C_ENABLE_STATUS_SLV_RX_DATA_LOST_POS            2U
#define I2C_ENABLE_STATUS_SLV_DISABLED_WHILE_BUSY_MASK    0x00000002U
#define I2C_ENABLE_STATUS_SLV_DISABLED_WHILE_BUSY_POS     1U
#define I2C_ENABLE_STATUS_IC_EN_MASK                      0x00000001U
#define I2C_ENABLE_STATUS_IC_EN_POS                       0U

/* Bit fields of IC_FS_SPKLEN register */
#define I2C_FS_SPKLEN_IC_FS_SPKLEN_MASK                   0x000000FFU
#define I2C_FS_SPKLEN_IC_FS_SPKLEN_POS                    0U

/* Bit fields of IC_CLR_RESTART_DET register */
#define I2C_CLR_RESTART_DET_CLR_RESTART_DET_MASK          0x00000001U
#define I2C_CLR_RESTART_DET_CLR_RESTART_DET_POS           0U

/* Bit fields of IC_COMP_PARAM_1 register */
#define I2C_COMP_PARAM_1_TX_BUFFER_DEPTH_MASK             0x00FF0000U
#define I2C_COMP_PARAM_1_TX_BUFFER_DEPTH_POS              16U
#define I2C_COMP_PARAM_1_RX_BUFFER_DEPTH_MASK             0x0000FF00U
#define I2C_COMP_PARAM_1_RX_BUFFER_DEPTH_POS              8U
#define I2C_COMP_PARAM_1_ADD_ENCODED_PARAMS_MASK          0x00000080U
#define I2C_COMP_PARAM_1_ADD_ENCODED_PARAMS_POS           7U
#define I2C_COMP_PARAM_1_HAS_DMA_MASK                     0x00000040U
#define I2C_COMP_PARAM_1_HAS_DMA_POS                      6U
#define I2C_COMP_PARAM_1_INTR_IO_MASK                     0x00000020U
#define I2C_COMP_PARAM_1_INTR_IO_POS                      5U
#define I2C_COMP_PARAM_1_HC_COUNT_VALUES_MASK             0x00000010U
#define I2C_COMP_PARAM_1_HC_COUNT_VALUES_POS              4U
#define I2C_COMP_PARAM_1_MAX_SPEED_MODE_MASK              0x0000000CU
#define I2C_COMP_PARAM_1_MAX_SPEED_MODE_POS               2U
#define I2C_COMP_PARAM_1_APB_DATA_WIDTH_MASK              0x00000003U
#define I2C_COMP_PARAM_1_APB_DATA_WIDTH_POS               0U

/* Bit fields of IC_COMP_VERSION register */
#define I2C_COMP_VERSION_IC_COMP_VERSION_MASK             0xFFFFFFFFU
#define I2C_COMP_VERSION_IC_COMP_VERSION_POS              0U

/* Bit fields of IC_COMP_TYPE register */
#define I2C_COMP_TYPE_IC_COMP_TYPE_MASK                   0xFFFFFFFFU
#define I2C_COMP_TYPE_IC_COMP_TYPE_POS                    0U

/*Reset Manager*/
#define SOCFPGA_RESET_MANAGER_BASE                        0x10D11000U
#define I2C_PER1MODRST_OFFSET                             0x28U
#define I2C0_RESET_BIT                                    8U
#define I2C1_RESET_BIT                                    9U
#define I2C2_RESET_BIT                                    10U
#define I2C3_RESET_BIT                                    11U
#define I2C4_RESET_BIT                                    12U

#endif   /* ifndef __SOCFPGA_I2C_REG_H__ */
