/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * This contains the register definitions for SoC FPGA clock manager
 */


#ifndef __SOCFPGA_CLK_MNGR_REG_H__
#define __SOCFPGA_CLK_MNGR_REG_H__

#define CLK_MNGR_BASE_ADDR                           0x10D10000U

/* Control Register */
#define CLK_MNGR_CTRL                                0x0000U
/* Status Register */
#define CLK_MNGR_STAT                                0x0004U
/* Test IO Control Register */
#define CLK_MNGR_TESTIOCTRL                          0x0008U
/* intrgen */
#define CLK_MNGR_INTRGEN                             0x000CU
/* intrmsk */
#define CLK_MNGR_INTRMSK                             0x0010U
/* intrclr */
#define CLK_MNGR_INTRCLR                             0x0014U
/* intrsts */
#define CLK_MNGR_INTRSTS                             0x0018U
/* intrstk */
#define CLK_MNGR_INTRSTK                             0x001CU
/* intrraw */
#define CLK_MNGR_INTRRAW                             0x0020U
/* Enable Register */
#define MAINPLL_CLK_MNGR_EN                          0x0024U
/* Enable MAINPLL_Set Register */
#define MAINPLL_CLK_MNGR_ENS                         0x0028U
/* Enable Reset Register */
#define MAINPLL_CLK_MNGR_ENR                         0x002CU
/* Bypass Register */
#define MAINPLL_CLK_MNGR_BYPASS                      0x0030U
/* Bypass Set Register */
#define MAINPLL_CLK_MNGR_BYPASSS                     0x0034U
/* Bypass Reset Register */
#define MAINPLL_CLK_MNGR_BYPASSR                     0x0038U
/* Main PLL Control Register for NOC Clock Group. */
#define MAINPLL_CLK_MNGR_NOCCLK                      0x0040U
/* NoC Divide Register */
#define MAINPLL_CLK_MNGR_NOCDIV                      0x0044U
/* pllglob */
#define MAINPLL_CLK_MNGR_PLLGLOB                     0x0048U
/* fdbck */
#define MAINPLL_CLK_MNGR_FDBCK                       0x004CU
/* mem */
#define MAINPLL_CLK_MNGR_MEM                         0x0050U
/* memstat */
#define MAINPLL_CLK_MNGR_MEMSTAT                     0x0054U
/* vcocalib */
#define MAINPLL_CLK_MNGR_VCOCALIB                    0x0058U
/* pllc0 */
#define MAINPLL_CLK_MNGR_PLLC0                       0x005CU
/* pllc1 */
#define MAINPLL_CLK_MNGR_PLLC1                       0x0060U
/* pllc2 */
#define MAINPLL_CLK_MNGR_PLLC2                       0x0064U
/* pllc3 */
#define MAINPLL_CLK_MNGR_PLLC3                       0x0068U
/* pllm */
#define MAINPLL_CLK_MNGR_PLLM                        0x006CU
/* fhop */
#define MAINPLL_CLK_MNGR_FHOP                        0x0070U
/* ssc */
#define MAINPLL_CLK_MNGR_SSC                         0x0074U
/* lostlock */
#define MAINPLL_CLK_MNGR_LOSTLOCK                    0x0078U
/* Enable Register */
#define PERPLL_CLK_MNGR_EN                           0x007CU
/* Enable Set Register */
#define PERPLL_CLK_MNGR_ENS                          0x0080U
/* Enable Reset Register */
#define PERPLL_CLK_MNGR_ENR                          0x0084U
/* Bypass Register */
#define PERPLL_CLK_MNGR_BYPASS                       0x0088U
/* Bypass Set Register */
#define PERPLL_CLK_MNGR_BYPASSS                      0x008CU
/* Bypass Reset Register */
#define PERPLL_CLK_MNGR_BYPASSR                      0x0090U
/* Main Divide Register */
#define PERPLL_CLK_MNGR_EMACCTL                      0x0094U
/* GPIO Divide Register */
#define PERPLL_CLK_MNGR_GPIODIV                      0x0098U
/* pllglob */
#define PERPLL_CLK_MNGR_PLLGLOB                      0x009CU
/* fdbck */
#define PERPLL_CLK_MNGR_FDBCK                        0x00A0U
/* mem */
#define PERPLL_CLK_MNGR_MEM                          0x00A4U
/* memstat */
#define PERPLL_CLK_MNGR_MEMSTAT                      0x00A8U
/* vcocalib */
#define PERPLL_CLK_MNGR_VCOCALIB                     0x00ACU
/* pllc0 */
#define PERPLL_CLK_MNGR_PLLC0                        0x00B0U
/* pllc1 */
#define PERPLL_CLK_MNGR_PLLC1                        0x00B4U
/* pllc2 */
#define PERPLL_CLK_MNGR_PLLC2                        0x00B8U
/* pllc3 */
#define PERPLL_CLK_MNGR_PLLC3                        0x00BCU
/* pllm */
#define PERPLL_CLK_MNGR_PLLM                         0x00C0U
/* fhop */
#define PERPLL_CLK_MNGR_FHOP                         0x00C4U
/* ssc */
#define PERPLL_CLK_MNGR_SSC                          0x00C8U
/* lostlock */
#define PERPLL_CLK_MNGR_LOSTLOCK                     0x00CCU
/* jtag */
#define CLK_MNGR_JTAG                                0x00D0U
/* Main PLL Control Register for emaca_free_clk */
#define CLK_MNGR_EMACACTR                            0x00D4U
/* MAin PLL Control Register for emacb_free_clk */
#define CLK_MNGR_EMACBCTR                            0x00D8U
/* Main PLL Control Register for emac_ptp_free_clk */
#define CLK_MNGR_EMACPTPCTR                          0x00DCU
/* Peripheral PLL Control Register for gpio_db_free_clk */
#define CLK_MNGR_GPIODBCTR                           0x00E0U
/* Control Register for s2f_user0_free_clk registers */
#define CLK_MNGR_S2FUSER0CTR                         0x00E8U
/* Main PLL Control Register for s2f_user1_free_clk */
#define CLK_MNGR_S2FUSER1CTR                         0x00ECU
/* Main PLL Control Register for psi_ref_free_clk */
#define CLK_MNGR_PSIREFCTR                           0x00F0U
/* Pingpong or External Counter reset control */
#define CLK_MNGR_EXTCNTRST                           0x00F4U
/* Control Register for usb31 clk */
#define CLK_MNGR_USB31CTR                            0x00F8U
/* Control Register for dsu_clk */
#define CLK_MNGR_DSUCTR                              0x00FCU
/* Control Register for core01_clk */
#define CLK_MNGR_CORE01CTR                           0x0100U
/* Main PLL Control Register for core23_clk */
#define CLK_MNGR_CORE23CTR                           0x0104U
/* Control Register for core2_clk */
#define CLK_MNGR_CORE2CTR                            0x0108U
/* Main PLL Control Register for core3_clk */
#define CLK_MNGR_CORE3CTR                            0x010CU
/* serial_con_pll_ctr */
#define CLK_MNGR_SERIAL_CON_PLL_CTR                  0x0110U

/* Bit fields of ctrl register */
#define CLK_MNGR_CTRL_SWCTRLBTCLKSEL_MASK            0x00000200U
#define CLK_MNGR_CTRL_SWCTRLBTCLKSEL_POS             9U
#define CLK_MNGR_CTRL_SWCTRLBTCLKEN_MASK             0x00000100U
#define CLK_MNGR_CTRL_SWCTRLBTCLKEN_POS              8U
#define CLK_MNGR_CTRL_BOOTMODE_MASK                  0x00000001U
#define CLK_MNGR_CTRL_BOOTMODE_POS                   0U

/* Bit fields of stat register */
#define CLK_MNGR_STAT_BOOTCLKSRC_MASK                0x02000000U
#define CLK_MNGR_STAT_BOOTCLKSRC_POS                 25U
#define CLK_MNGR_STAT_BOOTMODE_MASK                  0x01000000U
#define CLK_MNGR_STAT_BOOTMODE_POS                   24U
#define CLK_MNGR_STAT_PERF_TRANS_MASK                0x00020000U
#define CLK_MNGR_STAT_PERF_TRANS_POS                 17U
#define CLK_MNGR_STAT_PERPLLLOCKED_MASK              0x00010000U
#define CLK_MNGR_STAT_PERPLLLOCKED_POS               16U
#define CLK_MNGR_STAT_MAIN_TRANS_MASK                0x00000200U
#define CLK_MNGR_STAT_MAIN_TRANS_POS                 9U
#define CLK_MNGR_STAT_MAINPLLLOCKED_MASK             0x00000100U
#define CLK_MNGR_STAT_MAINPLLLOCKED_POS              8U
#define CLK_MNGR_STAT_BUSY_MASK                      0x00000001U
#define CLK_MNGR_STAT_BUSY_POS                       0U

/* Bit fields of testioctrl register */
#define CLK_MNGR_TESTIOCTRL_DEBUGCLKSEL_MASK         0x00010000U
#define CLK_MNGR_TESTIOCTRL_DEBUGCLKSEL_POS          16U
#define CLK_MNGR_TESTIOCTRL_PERICLKSEL_MASK          0x00000300U
#define CLK_MNGR_TESTIOCTRL_PERICLKSEL_POS           8U
#define CLK_MNGR_TESTIOCTRL_MAINCLKSEL_MASK          0x00000003U
#define CLK_MNGR_TESTIOCTRL_MAINCLKSEL_POS           0U

/* Bit fields of intrgen register */
#define CLK_MNGR_INTRGEN_EN_MASK                     0x00000001U
#define CLK_MNGR_INTRGEN_EN_POS                      0U

/* Bit fields of intrmsk register */
#define CLK_MNGR_INTRMSK_PERLOCKLOST_MASK            0x00000008U
#define CLK_MNGR_INTRMSK_PERLOCKLOST_POS             3U
#define CLK_MNGR_INTRMSK_MAINLOCKLOST_MASK           0x00000004U
#define CLK_MNGR_INTRMSK_MAINLOCKLOST_POS            2U
#define CLK_MNGR_INTRMSK_PERLOCKACHIEVED_MASK        0x00000002U
#define CLK_MNGR_INTRMSK_PERLOCKACHIEVED_POS         1U
#define CLK_MNGR_INTRMSK_MAINLOCKACHIEVED_MASK       0x00000001U
#define CLK_MNGR_INTRMSK_MAINLOCKACHIEVED_POS        0U

/* Bit fields of intrclr register */
#define CLK_MNGR_INTRCLR_PERLOCKLOST_MASK            0x00000008U
#define CLK_MNGR_INTRCLR_PERLOCKLOST_POS             3U
#define CLK_MNGR_INTRCLR_MAINLOCKLOST_MASK           0x00000004U
#define CLK_MNGR_INTRCLR_MAINLOCKLOST_POS            2U
#define CLK_MNGR_INTRCLR_PERLOCKACHIEVED_MASK        0x00000002U
#define CLK_MNGR_INTRCLR_PERLOCKACHIEVED_POS         1U
#define CLK_MNGR_INTRCLR_MAINLOCKACHIEVED_MASK       0x00000001U
#define CLK_MNGR_INTRCLR_MAINLOCKACHIEVED_POS        0U

/* Bit fields of intrsts register */
#define CLK_MNGR_INTRSTS_PERLOCKLOST_MASK            0x00000008U
#define CLK_MNGR_INTRSTS_PERLOCKLOST_POS             3U
#define CLK_MNGR_INTRSTS_MAINLOCKLOST_MASK           0x00000004U
#define CLK_MNGR_INTRSTS_MAINLOCKLOST_POS            2U
#define CLK_MNGR_INTRSTS_PERLOCKACHIEVED_MASK        0x00000002U
#define CLK_MNGR_INTRSTS_PERLOCKACHIEVED_POS         1U
#define CLK_MNGR_INTRSTS_MAINLOCKACHIEVED_MASK       0x00000001U
#define CLK_MNGR_INTRSTS_MAINLOCKACHIEVED_POS        0U

/* Bit fields of intrstk register */
#define CLK_MNGR_INTRSTK_PERLOCKLOST_MASK            0x00000008U
#define CLK_MNGR_INTRSTK_PERLOCKLOST_POS             3U
#define CLK_MNGR_INTRSTK_MAINLOCKLOST_MASK           0x00000004U
#define CLK_MNGR_INTRSTK_MAINLOCKLOST_POS            2U
#define CLK_MNGR_INTRSTK_PERLOCKACHIEVED_MASK        0x00000002U
#define CLK_MNGR_INTRSTK_PERLOCKACHIEVED_POS         1U
#define CLK_MNGR_INTRSTK_MAINLOCKACHIEVED_MASK       0x00000001U
#define CLK_MNGR_INTRSTK_MAINLOCKACHIEVED_POS        0U

/* Bit fields of intrraw register */
#define CLK_MNGR_INTRRAW_PERLOCKLOST_MASK            0x00000008U
#define CLK_MNGR_INTRRAW_PERLOCKLOST_POS             3U
#define CLK_MNGR_INTRRAW_MAINLOCKLOST_MASK           0x00000004U
#define CLK_MNGR_INTRRAW_MAINLOCKLOST_POS            2U
#define CLK_MNGR_INTRRAW_PERLOCKACHIEVED_MASK        0x00000002U
#define CLK_MNGR_INTRRAW_PERLOCKACHIEVED_POS         1U
#define CLK_MNGR_INTRRAW_MAINLOCKACHIEVED_MASK       0x00000001U
#define CLK_MNGR_INTRRAW_MAINLOCKACHIEVED_POS        0U

/* Bit fields of en register */
#define CLK_MNGR_EN_CORE3EN_MASK                     0x00000800U
#define CLK_MNGR_EN_CORE3EN_POS                      11U
#define CLK_MNGR_EN_CORE2EN_MASK                     0x00000400U
#define CLK_MNGR_EN_CORE2EN_POS                      10U
#define CLK_MNGR_EN_CORE1EN_MASK                     0x00000200U
#define CLK_MNGR_EN_CORE1EN_POS                      9U
#define CLK_MNGR_EN_CORE0EN_MASK                     0x00000100U
#define CLK_MNGR_EN_CORE0EN_POS                      8U
#define CLK_MNGR_EN_S2FUSER0CLKEN_MASK               0x00000040U
#define CLK_MNGR_EN_S2FUSER0CLKEN_POS                6U
#define CLK_MNGR_EN_CSCLKEN_MASK                     0x00000010U
#define CLK_MNGR_EN_CSCLKEN_POS                      4U
#define CLK_MNGR_EN_L4SPCLKEN_MASK                   0x00000008U
#define CLK_MNGR_EN_L4SPCLKEN_POS                    3U
#define CLK_MNGR_EN_L4MPCLKEN_MASK                   0x00000004U
#define CLK_MNGR_EN_L4MPCLKEN_POS                    2U
#define CLK_MNGR_EN_L4MAINCLKEN_MASK                 0x00000002U
#define CLK_MNGR_EN_L4MAINCLKEN_POS                  1U

/* Bit fields of ens register */
#define CLK_MNGR_ENS_CORE3EN_MASK                    0x00000800U
#define CLK_MNGR_ENS_CORE3EN_POS                     11U
#define CLK_MNGR_ENS_CORE2EN_MASK                    0x00000400U
#define CLK_MNGR_ENS_CORE2EN_POS                     10U
#define CLK_MNGR_ENS_CORE1EN_MASK                    0x00000200U
#define CLK_MNGR_ENS_CORE1EN_POS                     9U
#define CLK_MNGR_ENS_CORE0EN_MASK                    0x00000100U
#define CLK_MNGR_ENS_CORE0EN_POS                     8U
#define CLK_MNGR_ENS_S2FUSER0CLKEN_MASK              0x00000040U
#define CLK_MNGR_ENS_S2FUSER0CLKEN_POS               6U
#define CLK_MNGR_ENS_CSCLKEN_MASK                    0x00000010U
#define CLK_MNGR_ENS_CSCLKEN_POS                     4U
#define CLK_MNGR_ENS_L4SPCLKEN_MASK                  0x00000008U
#define CLK_MNGR_ENS_L4SPCLKEN_POS                   3U
#define CLK_MNGR_ENS_L4MPCLKEN_MASK                  0x00000004U
#define CLK_MNGR_ENS_L4MPCLKEN_POS                   2U
#define CLK_MNGR_ENS_L4MAINCLKEN_MASK                0x00000002U
#define CLK_MNGR_ENS_L4MAINCLKEN_POS                 1U

/* Bit fields of enr register */
#define CLK_MNGR_ENR_CORE3EN_MASK                    0x00000800U
#define CLK_MNGR_ENR_CORE3EN_POS                     11U
#define CLK_MNGR_ENR_CORE2EN_MASK                    0x00000400U
#define CLK_MNGR_ENR_CORE2EN_POS                     10U
#define CLK_MNGR_ENR_CORE1EN_MASK                    0x00000200U
#define CLK_MNGR_ENR_CORE1EN_POS                     9U
#define CLK_MNGR_ENR_CORE0EN_MASK                    0x00000100U
#define CLK_MNGR_ENR_CORE0EN_POS                     8U
#define CLK_MNGR_ENR_S2FUSER0CLKEN_MASK              0x00000040U
#define CLK_MNGR_ENR_S2FUSER0CLKEN_POS               6U
#define CLK_MNGR_ENR_CSCLKEN_MASK                    0x00000010U
#define CLK_MNGR_ENR_CSCLKEN_POS                     4U
#define CLK_MNGR_ENR_L4SPCLKEN_MASK                  0x00000008U
#define CLK_MNGR_ENR_L4SPCLKEN_POS                   3U
#define CLK_MNGR_ENR_L4MPCLKEN_MASK                  0x00000004U
#define CLK_MNGR_ENR_L4MPCLKEN_POS                   2U
#define CLK_MNGR_ENR_L4MAINCLKEN_MASK                0x00000002U
#define CLK_MNGR_ENR_L4MAINCLKEN_POS                 1U

/* Bit fields of bypass register */
#define CLK_MNGR_BYPASS_CORE3_MASK                   0x00000080U
#define CLK_MNGR_BYPASS_CORE3_POS                    7U
#define CLK_MNGR_BYPASS_CORE2_MASK                   0x00000040U
#define CLK_MNGR_BYPASS_CORE2_POS                    6U
#define CLK_MNGR_BYPASS_CORE01_MASK                  0x00000020U
#define CLK_MNGR_BYPASS_CORE01_POS                   5U
#define CLK_MNGR_BYPASS_DSU_MASK                     0x00000010U
#define CLK_MNGR_BYPASS_DSU_POS                      4U
#define CLK_MNGR_BYPASS_S2FUSER0_MASK                0x00000004U
#define CLK_MNGR_BYPASS_S2FUSER0_POS                 2U
#define CLK_MNGR_BYPASS_NOC_MASK                     0x00000002U
#define CLK_MNGR_BYPASS_NOC_POS                      1U

/* Bit fields of bypasss register */
#define CLK_MNGR_BYPASSS_CORE3_MASK                  0x00000080U
#define CLK_MNGR_BYPASSS_CORE3_POS                   7U
#define CLK_MNGR_BYPASSS_CORE2_MASK                  0x00000040U
#define CLK_MNGR_BYPASSS_CORE2_POS                   6U
#define CLK_MNGR_BYPASSS_CORE01_MASK                 0x00000020U
#define CLK_MNGR_BYPASSS_CORE01_POS                  5U
#define CLK_MNGR_BYPASSS_DSU_MASK                    0x00000010U
#define CLK_MNGR_BYPASSS_DSU_POS                     4U
#define CLK_MNGR_BYPASSS_S2FUSER0_MASK               0x00000004U
#define CLK_MNGR_BYPASSS_S2FUSER0_POS                2U
#define CLK_MNGR_BYPASSS_NOC_MASK                    0x00000002U
#define CLK_MNGR_BYPASSS_NOC_POS                     1U

/* Bit fields of bypassr register */
#define CLK_MNGR_BYPASSR_CORE3_MASK                  0x00000080U
#define CLK_MNGR_BYPASSR_CORE3_POS                   7U
#define CLK_MNGR_BYPASSR_CORE2_MASK                  0x00000040U
#define CLK_MNGR_BYPASSR_CORE2_POS                   6U
#define CLK_MNGR_BYPASSR_CORE01_MASK                 0x00000020U
#define CLK_MNGR_BYPASSR_CORE01_POS                  5U
#define CLK_MNGR_BYPASSR_DSU_MASK                    0x00000010U
#define CLK_MNGR_BYPASSR_DSU_POS                     4U
#define CLK_MNGR_BYPASSR_S2FUSER0_MASK               0x00000004U
#define CLK_MNGR_BYPASSR_S2FUSER0_POS                2U
#define CLK_MNGR_BYPASSR_NOC_MASK                    0x00000002U
#define CLK_MNGR_BYPASSR_NOC_POS                     1U

/* Bit fields of nocclk register */
#define CLK_MNGR_NOCCLK_SRC_MASK                     0x00070000U
#define CLK_MNGR_NOCCLK_SRC_POS                      16U

/* Bit fields of nocdiv register */
#define CLK_MNGR_NOCDIV_CSPDBGCLK_MASK               0x30000000U
#define CLK_MNGR_NOCDIV_CSPDBGCLK_POS                28U
#define CLK_MNGR_NOCDIV_CSTRACECLK_MASK              0x0C000000U
#define CLK_MNGR_NOCDIV_CSTRACECLK_POS               26U
#define CLK_MNGR_NOCDIV_CSCLK_MASK                   0x03000000U
#define CLK_MNGR_NOCDIV_CSCLK_POS                    24U
#define CLK_MNGR_NOCDIV_MPUPERIPHDIV_MASK            0x00300000U
#define CLK_MNGR_NOCDIV_MPUPERIPHDIV_POS             20U
#define CLK_MNGR_NOCDIV_CCUDIV_MASK                  0x000C0000U
#define CLK_MNGR_NOCDIV_CCUDIV_POS                   18U
#define CLK_MNGR_NOCDIV_SOFTPHYDIV_MASK              0x00030000U
#define CLK_MNGR_NOCDIV_SOFTPHYDIV_POS               16U
#define CLK_MNGR_NOCDIV_L4SPCLK_MASK                 0x000000C0U
#define CLK_MNGR_NOCDIV_L4SPCLK_POS                  6U
#define CLK_MNGR_NOCDIV_L4MPCLK_MASK                 0x00000030U
#define CLK_MNGR_NOCDIV_L4MPCLK_POS                  4U
#define CLK_MNGR_NOCDIV_L4SYSFREECLK_MASK            0x0000000CU
#define CLK_MNGR_NOCDIV_L4SYSFREECLK_POS             2U

/* Bit fields of pllglob register */
#define CLK_MNGR_PLLGLOB_CLR_LOSTLOCK_BYPASS_MASK    0x20000000U
#define CLK_MNGR_PLLGLOB_CLR_LOSTLOCK_BYPASS_POS     29U
#define CLK_MNGR_PLLGLOB_LOSTLOCK_BYPASS_EN_MASK     0x10000000U
#define CLK_MNGR_PLLGLOB_LOSTLOCK_BYPASS_EN_POS      28U
#define CLK_MNGR_PLLGLOB_MODCLKDIV_MASK              0x0F000000U
#define CLK_MNGR_PLLGLOB_MODCLKDIV_POS               24U
#define CLK_MNGR_PLLGLOB_FASTREFCLK_MASK             0x00200000U
#define CLK_MNGR_PLLGLOB_FASTREFCLK_POS              21U
#define CLK_MNGR_PLLGLOB_DISCTRL_MASK                0x00100000U
#define CLK_MNGR_PLLGLOB_DISCTRL_POS                 20U
#define CLK_MNGR_PLLGLOB_CLKSYNC_MASK                0x00080000U
#define CLK_MNGR_PLLGLOB_CLKSYNC_POS                 19U
#define CLK_MNGR_PLLGLOB_PWRGATECTRL_MASK            0x00040000U
#define CLK_MNGR_PLLGLOB_PWRGATECTRL_POS             18U
#define CLK_MNGR_PLLGLOB_PSRC_MASK                   0x00030000U
#define CLK_MNGR_PLLGLOB_PSRC_POS                    16U
#define CLK_MNGR_PLLGLOB_DREFCLKDIV_MASK             0x00003000U
#define CLK_MNGR_PLLGLOB_DREFCLKDIV_POS              12U
#define CLK_MNGR_PLLGLOB_AREFCLKDIV_MASK             0x00000F00U
#define CLK_MNGR_PLLGLOB_AREFCLKDIV_POS              8U
#define CLK_MNGR_PLLGLOB_RST_N_MASK                  0x00000002U
#define CLK_MNGR_PLLGLOB_RST_N_POS                   1U
#define CLK_MNGR_PLLGLOB_PD_N_MASK                   0x00000001U
#define CLK_MNGR_PLLGLOB_PD_N_POS                    0U

/* Bit fields of fdbck register */
#define CLK_MNGR_FDBCK_FDIV_MASK                     0x00FFFFFFU
#define CLK_MNGR_FDBCK_FDIV_POS                      0U

/* Bit fields of mem register */
#define CLK_MNGR_MEM_ERR_MASK                        0x04000000U
#define CLK_MNGR_MEM_ERR_POS                         26U
#define CLK_MNGR_MEM_WR_MASK                         0x02000000U
#define CLK_MNGR_MEM_WR_POS                          25U
#define CLK_MNGR_MEM_REQ_MASK                        0x01000000U
#define CLK_MNGR_MEM_REQ_POS                         24U
#define CLK_MNGR_MEM_WDAT_MASK                       0x00FF0000U
#define CLK_MNGR_MEM_WDAT_POS                        16U
#define CLK_MNGR_MEM_ADDR_MASK                       0x0000FFFFU
#define CLK_MNGR_MEM_ADDR_POS                        0U

/* Bit fields of memstat register */
#define CLK_MNGR_MEMSTAT_RDATA_MASK                  0x000000FFU
#define CLK_MNGR_MEMSTAT_RDATA_POS                   0U

/* Bit fields of vcocalib register */
#define CLK_MNGR_VCOCALIB_CLR_MASK                   0x08000000U
#define CLK_MNGR_VCOCALIB_CLR_POS                    27U
#define CLK_MNGR_VCOCALIB_BANKSEL_MASK               0x06000000U
#define CLK_MNGR_VCOCALIB_BANKSEL_POS                25U
#define CLK_MNGR_VCOCALIB_MSCNT_MASK                 0x00FF0000U
#define CLK_MNGR_VCOCALIB_MSCNT_POS                  16U
#define CLK_MNGR_VCOCALIB_HSCNT_MASK                 0x000003FFU
#define CLK_MNGR_VCOCALIB_HSCNT_POS                  0U

/* Bit fields of pllc0 register */
#define CLK_MNGR_PLLC0_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC0_STAT_POS                      29U
#define CLK_MNGR_PLLC0_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC0_MUTE_POS                      28U
#define CLK_MNGR_PLLC0_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC0_EN_POS                        27U
#define CLK_MNGR_PLLC0_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC0_BYPAS_POS                     26U
#define CLK_MNGR_PLLC0_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC0_DIV_POS                       0U

/* Bit fields of pllc1 register */
#define CLK_MNGR_PLLC1_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC1_STAT_POS                      29U
#define CLK_MNGR_PLLC1_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC1_MUTE_POS                      28U
#define CLK_MNGR_PLLC1_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC1_EN_POS                        27U
#define CLK_MNGR_PLLC1_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC1_BYPAS_POS                     26U
#define CLK_MNGR_PLLC1_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC1_DIV_POS                       0U

/* Bit fields of pllc2 register */
#define CLK_MNGR_PLLC2_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC2_STAT_POS                      29U
#define CLK_MNGR_PLLC2_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC2_MUTE_POS                      28U
#define CLK_MNGR_PLLC2_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC2_EN_POS                        27U
#define CLK_MNGR_PLLC2_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC2_BYPAS_POS                     26U
#define CLK_MNGR_PLLC2_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC2_DIV_POS                       0U

/* Bit fields of pllc3 register */
#define CLK_MNGR_PLLC3_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC3_STAT_POS                      29U
#define CLK_MNGR_PLLC3_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC3_MUTE_POS                      28U
#define CLK_MNGR_PLLC3_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC3_EN_POS                        27U
#define CLK_MNGR_PLLC3_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC3_BYPAS_POS                     26U
#define CLK_MNGR_PLLC3_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC3_DIV_POS                       0U

/* Bit fields of pllm register */
#define CLK_MNGR_PLLM_MDIV_MASK                      0x000003FFU
#define CLK_MNGR_PLLM_MDIV_POS                       0U

/* Bit fields of fhop register */
#define CLK_MNGR_FHOP_ACK_MASK                       0x00010000U
#define CLK_MNGR_FHOP_ACK_POS                        16U
#define CLK_MNGR_FHOP_REQ_MASK                       0x00000100U
#define CLK_MNGR_FHOP_REQ_POS                        8U
#define CLK_MNGR_FHOP_DIR_MASK                       0x00000003U
#define CLK_MNGR_FHOP_DIR_POS                        0U

/* Bit fields of ssc register */
#define CLK_MNGR_SSC_STAT_MASK                       0x00000100U
#define CLK_MNGR_SSC_STAT_POS                        8U
#define CLK_MNGR_SSC_EN_MASK                         0x00000001U
#define CLK_MNGR_SSC_EN_POS                          0U

/* Bit fields of lostlock register */
#define CLK_MNGR_LOSTLOCK_BYPASS_CLEARED_MASK        0x00000001U
#define CLK_MNGR_LOSTLOCK_BYPASS_CLEARED_POS         0U

/* Bit fields of en register */
#define CLK_MNGR_EN_I2C_EMAC2_CLKEN_MASK             0x08000000U
#define CLK_MNGR_EN_I2C_EMAC2_CLKEN_POS              27U
#define CLK_MNGR_EN_SOFTPHYCLKEN_MASK                0x04000000U
#define CLK_MNGR_EN_SOFTPHYCLKEN_POS                 26U
#define CLK_MNGR_EN_USB31CLKEN_MASK                  0x02000000U
#define CLK_MNGR_EN_USB31CLKEN_POS                   25U
#define CLK_MNGR_EN_SPTIMER_1_CLKEN_MASK             0x01000000U
#define CLK_MNGR_EN_SPTIMER_1_CLKEN_POS              24U
#define CLK_MNGR_EN_SPTIMER_0_CLKEN_MASK             0x00800000U
#define CLK_MNGR_EN_SPTIMER_0_CLKEN_POS              23U
#define CLK_MNGR_EN_I2C_EMAC1_CLKEN_MASK             0x00400000U
#define CLK_MNGR_EN_I2C_EMAC1_CLKEN_POS              22U
#define CLK_MNGR_EN_UART_1_CLKEN_MASK                0x00200000U
#define CLK_MNGR_EN_UART_1_CLKEN_POS                 21U
#define CLK_MNGR_EN_UART_0_CLKEN_MASK                0x00100000U
#define CLK_MNGR_EN_UART_0_CLKEN_POS                 20U
#define CLK_MNGR_EN_I3C_1_CLKEN_MASK                 0x00080000U
#define CLK_MNGR_EN_I3C_1_CLKEN_POS                  19U
#define CLK_MNGR_EN_I3C_0_CLKEN_MASK                 0x00040000U
#define CLK_MNGR_EN_I3C_0_CLKEN_POS                  18U
#define CLK_MNGR_EN_I2C_EMAC0_CLKEN_MASK             0x00020000U
#define CLK_MNGR_EN_I2C_EMAC0_CLKEN_POS              17U
#define CLK_MNGR_EN_I2C_1_CLKEN_MASK                 0x00010000U
#define CLK_MNGR_EN_I2C_1_CLKEN_POS                  16U
#define CLK_MNGR_EN_I2C_0_CLKEN_MASK                 0x00008000U
#define CLK_MNGR_EN_I2C_0_CLKEN_POS                  15U
#define CLK_MNGR_EN_DMACLKEN_MASK                    0x00004000U
#define CLK_MNGR_EN_DMACLKEN_POS                     14U
#define CLK_MNGR_EN_SPIS_1_CLKEN_MASK                0x00002000U
#define CLK_MNGR_EN_SPIS_1_CLKEN_POS                 13U
#define CLK_MNGR_EN_SPIS_0_CLKEN_MASK                0x00001000U
#define CLK_MNGR_EN_SPIS_0_CLKEN_POS                 12U
#define CLK_MNGR_EN_SPIM_1_CLKEN_MASK                0x00000800U
#define CLK_MNGR_EN_SPIM_1_CLKEN_POS                 11U
#define CLK_MNGR_EN_NANDCLKEN_MASK                   0x00000400U
#define CLK_MNGR_EN_NANDCLKEN_POS                    10U
#define CLK_MNGR_EN_SPIM_0_CLKEN_MASK                0x00000200U
#define CLK_MNGR_EN_SPIM_0_CLKEN_POS                 9U
#define CLK_MNGR_EN_USB2CLKEN_MASK                   0x00000100U
#define CLK_MNGR_EN_USB2CLKEN_POS                    8U
#define CLK_MNGR_EN_PSICLKEN_MASK                    0x00000080U
#define CLK_MNGR_EN_PSICLKEN_POS                     7U
#define CLK_MNGR_EN_S2FUSER1CLKEN_MASK               0x00000040U
#define CLK_MNGR_EN_S2FUSER1CLKEN_POS                6U
#define CLK_MNGR_EN_SDMMCCLKEN_MASK                  0x00000020U
#define CLK_MNGR_EN_SDMMCCLKEN_POS                   5U
#define CLK_MNGR_EN_GPIODBEN_MASK                    0x00000010U
#define CLK_MNGR_EN_GPIODBEN_POS                     4U
#define CLK_MNGR_EN_EMACPTPEN_MASK                   0x00000008U
#define CLK_MNGR_EN_EMACPTPEN_POS                    3U
#define CLK_MNGR_EN_EMAC2EN_MASK                     0x00000004U
#define CLK_MNGR_EN_EMAC2EN_POS                      2U
#define CLK_MNGR_EN_EMAC1EN_MASK                     0x00000002U
#define CLK_MNGR_EN_EMAC1EN_POS                      1U
#define CLK_MNGR_EN_EMAC0EN_MASK                     0x00000001U
#define CLK_MNGR_EN_EMAC0EN_POS                      0U

/* Bit fields of ens register */
#define CLK_MNGR_ENS_I2C_EMAC2_CLKEN_MASK            0x08000000U
#define CLK_MNGR_ENS_I2C_EMAC2_CLKEN_POS             27U
#define CLK_MNGR_ENS_SOFTPHYCLKEN_MASK               0x04000000U
#define CLK_MNGR_ENS_SOFTPHYCLKEN_POS                26U
#define CLK_MNGR_ENS_USB31CLKEN_MASK                 0x02000000U
#define CLK_MNGR_ENS_USB31CLKEN_POS                  25U
#define CLK_MNGR_ENS_SPTIMER_1_CLKEN_MASK            0x01000000U
#define CLK_MNGR_ENS_SPTIMER_1_CLKEN_POS             24U
#define CLK_MNGR_ENS_SPTIMER_0_CLKEN_MASK            0x00800000U
#define CLK_MNGR_ENS_SPTIMER_0_CLKEN_POS             23U
#define CLK_MNGR_ENS_I2C_EMAC1_CLKEN_MASK            0x00400000U
#define CLK_MNGR_ENS_I2C_EMAC1_CLKEN_POS             22U
#define CLK_MNGR_ENS_UART_1_CLKEN_MASK               0x00200000U
#define CLK_MNGR_ENS_UART_1_CLKEN_POS                21U
#define CLK_MNGR_ENS_UART_0_CLKEN_MASK               0x00100000U
#define CLK_MNGR_ENS_UART_0_CLKEN_POS                20U
#define CLK_MNGR_ENS_I3C_1_CLKEN_MASK                0x00080000U
#define CLK_MNGR_ENS_I3C_1_CLKEN_POS                 19U
#define CLK_MNGR_ENS_I3C_0_CLKEN_MASK                0x00040000U
#define CLK_MNGR_ENS_I3C_0_CLKEN_POS                 18U
#define CLK_MNGR_ENS_I2C_EMAC0_CLKEN_MASK            0x00020000U
#define CLK_MNGR_ENS_I2C_EMAC0_CLKEN_POS             17U
#define CLK_MNGR_ENS_I2C_1_CLKEN_MASK                0x00010000U
#define CLK_MNGR_ENS_I2C_1_CLKEN_POS                 16U
#define CLK_MNGR_ENS_I2C_0_CLKEN_MASK                0x00008000U
#define CLK_MNGR_ENS_I2C_0_CLKEN_POS                 15U
#define CLK_MNGR_ENS_DMACLKEN_MASK                   0x00004000U
#define CLK_MNGR_ENS_DMACLKEN_POS                    14U
#define CLK_MNGR_ENS_SPIS_1_CLKEN_MASK               0x00002000U
#define CLK_MNGR_ENS_SPIS_1_CLKEN_POS                13U
#define CLK_MNGR_ENS_SPIS_0_CLKEN_MASK               0x00001000U
#define CLK_MNGR_ENS_SPIS_0_CLKEN_POS                12U
#define CLK_MNGR_ENS_SPIM_1_CLKEN_MASK               0x00000800U
#define CLK_MNGR_ENS_SPIM_1_CLKEN_POS                11U
#define CLK_MNGR_ENS_NANDCLKEN_MASK                  0x00000400U
#define CLK_MNGR_ENS_NANDCLKEN_POS                   10U
#define CLK_MNGR_ENS_SPIM_0_CLKEN_MASK               0x00000200U
#define CLK_MNGR_ENS_SPIM_0_CLKEN_POS                9U
#define CLK_MNGR_ENS_USB2CLKEN_MASK                  0x00000100U
#define CLK_MNGR_ENS_USB2CLKEN_POS                   8U
#define CLK_MNGR_ENS_PSICLKEN_MASK                   0x00000080U
#define CLK_MNGR_ENS_PSICLKEN_POS                    7U
#define CLK_MNGR_ENS_S2FUSER1CLKEN_MASK              0x00000040U
#define CLK_MNGR_ENS_S2FUSER1CLKEN_POS               6U
#define CLK_MNGR_ENS_SDMMCCLKEN_MASK                 0x00000020U
#define CLK_MNGR_ENS_SDMMCCLKEN_POS                  5U
#define CLK_MNGR_ENS_GPIODBEN_MASK                   0x00000010U
#define CLK_MNGR_ENS_GPIODBEN_POS                    4U
#define CLK_MNGR_ENS_EMACPTPEN_MASK                  0x00000008U
#define CLK_MNGR_ENS_EMACPTPEN_POS                   3U
#define CLK_MNGR_ENS_EMAC2EN_MASK                    0x00000004U
#define CLK_MNGR_ENS_EMAC2EN_POS                     2U
#define CLK_MNGR_ENS_EMAC1EN_MASK                    0x00000002U
#define CLK_MNGR_ENS_EMAC1EN_POS                     1U
#define CLK_MNGR_ENS_EMAC0EN_MASK                    0x00000001U
#define CLK_MNGR_ENS_EMAC0EN_POS                     0U

/* Bit fields of enr register */
#define CLK_MNGR_ENR_I2C_EMAC2_CLKEN_MASK            0x08000000U
#define CLK_MNGR_ENR_I2C_EMAC2_CLKEN_POS             27U
#define CLK_MNGR_ENR_SOFTPHYCLKEN_MASK               0x04000000U
#define CLK_MNGR_ENR_SOFTPHYCLKEN_POS                26U
#define CLK_MNGR_ENR_USB31CLKEN_MASK                 0x02000000U
#define CLK_MNGR_ENR_USB31CLKEN_POS                  25U
#define CLK_MNGR_ENR_SPTIMER_1_CLKEN_MASK            0x01000000U
#define CLK_MNGR_ENR_SPTIMER_1_CLKEN_POS             24U
#define CLK_MNGR_ENR_SPTIMER_0_CLKEN_MASK            0x00800000U
#define CLK_MNGR_ENR_SPTIMER_0_CLKEN_POS             23U
#define CLK_MNGR_ENR_I2C_EMAC1_CLKEN_MASK            0x00400000U
#define CLK_MNGR_ENR_I2C_EMAC1_CLKEN_POS             22U
#define CLK_MNGR_ENR_UART_1_CLKEN_MASK               0x00200000U
#define CLK_MNGR_ENR_UART_1_CLKEN_POS                21U
#define CLK_MNGR_ENR_UART_0_CLKEN_MASK               0x00100000U
#define CLK_MNGR_ENR_UART_0_CLKEN_POS                20U
#define CLK_MNGR_ENR_I3C_1_CLKEN_MASK                0x00080000U
#define CLK_MNGR_ENR_I3C_1_CLKEN_POS                 19U
#define CLK_MNGR_ENR_I3C_0_CLKEN_MASK                0x00040000U
#define CLK_MNGR_ENR_I3C_0_CLKEN_POS                 18U
#define CLK_MNGR_ENR_I2C_EMAC0_CLKEN_MASK            0x00020000U
#define CLK_MNGR_ENR_I2C_EMAC0_CLKEN_POS             17U
#define CLK_MNGR_ENR_I2C_1_CLKEN_MASK                0x00010000U
#define CLK_MNGR_ENR_I2C_1_CLKEN_POS                 16U
#define CLK_MNGR_ENR_I2C_0_CLKEN_MASK                0x00008000U
#define CLK_MNGR_ENR_I2C_0_CLKEN_POS                 15U
#define CLK_MNGR_ENR_DMACLKEN_MASK                   0x00004000U
#define CLK_MNGR_ENR_DMACLKEN_POS                    14U
#define CLK_MNGR_ENR_SPIS_1_CLKEN_MASK               0x00002000U
#define CLK_MNGR_ENR_SPIS_1_CLKEN_POS                13U
#define CLK_MNGR_ENR_SPIS_0_CLKEN_MASK               0x00001000U
#define CLK_MNGR_ENR_SPIS_0_CLKEN_POS                12U
#define CLK_MNGR_ENR_SPIM_1_CLKEN_MASK               0x00000800U
#define CLK_MNGR_ENR_SPIM_1_CLKEN_POS                11U
#define CLK_MNGR_ENR_NANDCLKEN_MASK                  0x00000400U
#define CLK_MNGR_ENR_NANDCLKEN_POS                   10U
#define CLK_MNGR_ENR_SPIM_0_CLKEN_MASK               0x00000200U
#define CLK_MNGR_ENR_SPIM_0_CLKEN_POS                9U
#define CLK_MNGR_ENR_USB2CLKEN_MASK                  0x00000100U
#define CLK_MNGR_ENR_USB2CLKEN_POS                   8U
#define CLK_MNGR_ENR_PSICLKEN_MASK                   0x00000080U
#define CLK_MNGR_ENR_PSICLKEN_POS                    7U
#define CLK_MNGR_ENR_S2FUSER1CLKEN_MASK              0x00000040U
#define CLK_MNGR_ENR_S2FUSER1CLKEN_POS               6U
#define CLK_MNGR_ENR_SDMMCCLKEN_MASK                 0x00000020U
#define CLK_MNGR_ENR_SDMMCCLKEN_POS                  5U
#define CLK_MNGR_ENR_GPIODBEN_MASK                   0x00000010U
#define CLK_MNGR_ENR_GPIODBEN_POS                    4U
#define CLK_MNGR_ENR_EMACPTPEN_MASK                  0x00000008U
#define CLK_MNGR_ENR_EMACPTPEN_POS                   3U
#define CLK_MNGR_ENR_EMAC2EN_MASK                    0x00000004U
#define CLK_MNGR_ENR_EMAC2EN_POS                     2U
#define CLK_MNGR_ENR_EMAC1EN_MASK                    0x00000002U
#define CLK_MNGR_ENR_EMAC1EN_POS                     1U
#define CLK_MNGR_ENR_EMAC0EN_MASK                    0x00000001U
#define CLK_MNGR_ENR_EMAC0EN_POS                     0U

/* Bit fields of bypass register */
#define CLK_MNGR_BYPASS_USB31_MASK                   0x00000080U
#define CLK_MNGR_BYPASS_USB31_POS                    7U
#define CLK_MNGR_BYPASS_PSIREF_MASK                  0x00000040U
#define CLK_MNGR_BYPASS_PSIREF_POS                   6U
#define CLK_MNGR_BYPASS_S2FUSER1_MASK                0x00000020U
#define CLK_MNGR_BYPASS_S2FUSER1_POS                 5U
#define CLK_MNGR_BYPASS_GPIODB_MASK                  0x00000008U
#define CLK_MNGR_BYPASS_GPIODB_POS                   3U
#define CLK_MNGR_BYPASS_EMACPTP_MASK                 0x00000004U
#define CLK_MNGR_BYPASS_EMACPTP_POS                  2U
#define CLK_MNGR_BYPASS_EMACB_MASK                   0x00000002U
#define CLK_MNGR_BYPASS_EMACB_POS                    1U
#define CLK_MNGR_BYPASS_EMACA_MASK                   0x00000001U
#define CLK_MNGR_BYPASS_EMACA_POS                    0U

/* Bit fields of bypasss register */
#define CLK_MNGR_BYPASSS_USB31_MASK                  0x00000080U
#define CLK_MNGR_BYPASSS_USB31_POS                   7U
#define CLK_MNGR_BYPASSS_PSIREF_MASK                 0x00000040U
#define CLK_MNGR_BYPASSS_PSIREF_POS                  6U
#define CLK_MNGR_BYPASSS_S2FUSER1_MASK               0x00000020U
#define CLK_MNGR_BYPASSS_S2FUSER1_POS                5U
#define CLK_MNGR_BYPASSS_GPIODB_MASK                 0x00000008U
#define CLK_MNGR_BYPASSS_GPIODB_POS                  3U
#define CLK_MNGR_BYPASSS_EMACPTP_MASK                0x00000004U
#define CLK_MNGR_BYPASSS_EMACPTP_POS                 2U
#define CLK_MNGR_BYPASSS_EMACB_MASK                  0x00000002U
#define CLK_MNGR_BYPASSS_EMACB_POS                   1U
#define CLK_MNGR_BYPASSS_EMACA_MASK                  0x00000001U
#define CLK_MNGR_BYPASSS_EMACA_POS                   0U

/* Bit fields of bypassr register */
#define CLK_MNGR_BYPASSR_USB31_MASK                  0x00000080U
#define CLK_MNGR_BYPASSR_USB31_POS                   7U
#define CLK_MNGR_BYPASSR_PSIREF_MASK                 0x00000040U
#define CLK_MNGR_BYPASSR_PSIREF_POS                  6U
#define CLK_MNGR_BYPASSR_S2FUSER1_MASK               0x00000020U
#define CLK_MNGR_BYPASSR_S2FUSER1_POS                5U
#define CLK_MNGR_BYPASSR_GPIODB_MASK                 0x00000008U
#define CLK_MNGR_BYPASSR_GPIODB_POS                  3U
#define CLK_MNGR_BYPASSR_EMACPTP_MASK                0x00000004U
#define CLK_MNGR_BYPASSR_EMACPTP_POS                 2U
#define CLK_MNGR_BYPASSR_EMACB_MASK                  0x00000002U
#define CLK_MNGR_BYPASSR_EMACB_POS                   1U
#define CLK_MNGR_BYPASSR_EMACA_MASK                  0x00000001U
#define CLK_MNGR_BYPASSR_EMACA_POS                   0U

/* Bit fields of emacctl register */
#define CLK_MNGR_EMACCTL_EMAC2SEL_MASK               0x10000000U
#define CLK_MNGR_EMACCTL_EMAC2SEL_POS                28U
#define CLK_MNGR_EMACCTL_EMAC1SEL_MASK               0x08000000U
#define CLK_MNGR_EMACCTL_EMAC1SEL_POS                27U
#define CLK_MNGR_EMACCTL_EMAC0SEL_MASK               0x04000000U
#define CLK_MNGR_EMACCTL_EMAC0SEL_POS                26U

/* Bit fields of gpiodiv register */
#define CLK_MNGR_GPIODIV_GPIODBCLK_MASK              0x0000FFFFU
#define CLK_MNGR_GPIODIV_GPIODBCLK_POS               0U

/* Bit fields of pllglob register */
#define CLK_MNGR_PLLGLOB_CLR_LOSTLOCK_BYPASS_MASK    0x20000000U
#define CLK_MNGR_PLLGLOB_CLR_LOSTLOCK_BYPASS_POS     29U
#define CLK_MNGR_PLLGLOB_LOSTLOCK_BYPASS_EN_MASK     0x10000000U
#define CLK_MNGR_PLLGLOB_LOSTLOCK_BYPASS_EN_POS      28U
#define CLK_MNGR_PLLGLOB_MODCLKDIV_MASK              0x0F000000U
#define CLK_MNGR_PLLGLOB_MODCLKDIV_POS               24U
#define CLK_MNGR_PLLGLOB_FASTREFCLK_MASK             0x00200000U
#define CLK_MNGR_PLLGLOB_FASTREFCLK_POS              21U
#define CLK_MNGR_PLLGLOB_DISCTRL_MASK                0x00100000U
#define CLK_MNGR_PLLGLOB_DISCTRL_POS                 20U
#define CLK_MNGR_PLLGLOB_CLKSYNC_MASK                0x00080000U
#define CLK_MNGR_PLLGLOB_CLKSYNC_POS                 19U
#define CLK_MNGR_PLLGLOB_PWRGATECTRL_MASK            0x00040000U
#define CLK_MNGR_PLLGLOB_PWRGATECTRL_POS             18U
#define CLK_MNGR_PLLGLOB_PSRC_MASK                   0x00030000U
#define CLK_MNGR_PLLGLOB_PSRC_POS                    16U
#define CLK_MNGR_PLLGLOB_DREFCLKDIV_MASK             0x00003000U
#define CLK_MNGR_PLLGLOB_DREFCLKDIV_POS              12U
#define CLK_MNGR_PLLGLOB_AREFCLKDIV_MASK             0x00000F00U
#define CLK_MNGR_PLLGLOB_AREFCLKDIV_POS              8U
#define CLK_MNGR_PLLGLOB_RST_N_MASK                  0x00000002U
#define CLK_MNGR_PLLGLOB_RST_N_POS                   1U
#define CLK_MNGR_PLLGLOB_PD_N_MASK                   0x00000001U
#define CLK_MNGR_PLLGLOB_PD_N_POS                    0U

/* Bit fields of fdbck register */
#define CLK_MNGR_FDBCK_FDIV_MASK                     0x00FFFFFFU
#define CLK_MNGR_FDBCK_FDIV_POS                      0U

/* Bit fields of mem register */
#define CLK_MNGR_MEM_ERR_MASK                        0x04000000U
#define CLK_MNGR_MEM_ERR_POS                         26U
#define CLK_MNGR_MEM_WR_MASK                         0x02000000U
#define CLK_MNGR_MEM_WR_POS                          25U
#define CLK_MNGR_MEM_REQ_MASK                        0x01000000U
#define CLK_MNGR_MEM_REQ_POS                         24U
#define CLK_MNGR_MEM_WDAT_MASK                       0x00FF0000U
#define CLK_MNGR_MEM_WDAT_POS                        16U
#define CLK_MNGR_MEM_ADDR_MASK                       0x0000FFFFU
#define CLK_MNGR_MEM_ADDR_POS                        0U

/* Bit fields of memstat register */
#define CLK_MNGR_MEMSTAT_RDATA_MASK                  0x000000FFU
#define CLK_MNGR_MEMSTAT_RDATA_POS                   0U

/* Bit fields of vcocalib register */
#define CLK_MNGR_VCOCALIB_CLR_MASK                   0x08000000U
#define CLK_MNGR_VCOCALIB_CLR_POS                    27U
#define CLK_MNGR_VCOCALIB_BANKSEL_MASK               0x06000000U
#define CLK_MNGR_VCOCALIB_BANKSEL_POS                25U
#define CLK_MNGR_VCOCALIB_MSCNT_MASK                 0x00FF0000U
#define CLK_MNGR_VCOCALIB_MSCNT_POS                  16U
#define CLK_MNGR_VCOCALIB_HSCNT_MASK                 0x000003FFU
#define CLK_MNGR_VCOCALIB_HSCNT_POS                  0U

/* Bit fields of pllc0 register */
#define CLK_MNGR_PLLC0_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC0_STAT_POS                      29U
#define CLK_MNGR_PLLC0_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC0_MUTE_POS                      28U
#define CLK_MNGR_PLLC0_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC0_EN_POS                        27U
#define CLK_MNGR_PLLC0_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC0_BYPAS_POS                     26U
#define CLK_MNGR_PLLC0_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC0_DIV_POS                       0U

/* Bit fields of pllc1 register */
#define CLK_MNGR_PLLC1_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC1_STAT_POS                      29U
#define CLK_MNGR_PLLC1_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC1_MUTE_POS                      28U
#define CLK_MNGR_PLLC1_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC1_EN_POS                        27U
#define CLK_MNGR_PLLC1_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC1_BYPAS_POS                     26U
#define CLK_MNGR_PLLC1_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC1_DIV_POS                       0U

/* Bit fields of pllc2 register */
#define CLK_MNGR_PLLC2_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC2_STAT_POS                      29U
#define CLK_MNGR_PLLC2_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC2_MUTE_POS                      28U
#define CLK_MNGR_PLLC2_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC2_EN_POS                        27U
#define CLK_MNGR_PLLC2_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC2_BYPAS_POS                     26U
#define CLK_MNGR_PLLC2_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC2_DIV_POS                       0U

/* Bit fields of pllc3 register */
#define CLK_MNGR_PLLC3_STAT_MASK                     0x20000000U
#define CLK_MNGR_PLLC3_STAT_POS                      29U
#define CLK_MNGR_PLLC3_MUTE_MASK                     0x10000000U
#define CLK_MNGR_PLLC3_MUTE_POS                      28U
#define CLK_MNGR_PLLC3_EN_MASK                       0x08000000U
#define CLK_MNGR_PLLC3_EN_POS                        27U
#define CLK_MNGR_PLLC3_BYPAS_MASK                    0x04000000U
#define CLK_MNGR_PLLC3_BYPAS_POS                     26U
#define CLK_MNGR_PLLC3_DIV_MASK                      0x000007FFU
#define CLK_MNGR_PLLC3_DIV_POS                       0U

/* Bit fields of pllm register */
#define CLK_MNGR_PLLM_MDIV_MASK                      0x000003FFU
#define CLK_MNGR_PLLM_MDIV_POS                       0U

/* Bit fields of fhop register */
#define CLK_MNGR_FHOP_ACK_MASK                       0x00010000U
#define CLK_MNGR_FHOP_ACK_POS                        16U
#define CLK_MNGR_FHOP_REQ_MASK                       0x00000100U
#define CLK_MNGR_FHOP_REQ_POS                        8U
#define CLK_MNGR_FHOP_DIR_MASK                       0x00000003U
#define CLK_MNGR_FHOP_DIR_POS                        0U

/* Bit fields of ssc register */
#define CLK_MNGR_SSC_STAT_MASK                       0x00000100U
#define CLK_MNGR_SSC_STAT_POS                        8U
#define CLK_MNGR_SSC_EN_MASK                         0x00000001U
#define CLK_MNGR_SSC_EN_POS                          0U

/* Bit fields of lostlock register */
#define CLK_MNGR_LOSTLOCK_BYPASS_CLEARED_MASK        0x00000001U
#define CLK_MNGR_LOSTLOCK_BYPASS_CLEARED_POS         0U

/* Bit fields of jtag register */
#define CLK_MNGR_JTAG_RST_MASK                       0x00000100U
#define CLK_MNGR_JTAG_RST_POS                        8U
#define CLK_MNGR_JTAG_ID_MASK                        0x000000FFU
#define CLK_MNGR_JTAG_ID_POS                         0U

/* Bit fields of emacactr register */
#define CLK_MNGR_EMACACTR_SRC_MASK                   0x00070000U
#define CLK_MNGR_EMACACTR_SRC_POS                    16U
#define CLK_MNGR_EMACACTR_CNT_MASK                   0x000007FFU
#define CLK_MNGR_EMACACTR_CNT_POS                    0U

/* Bit fields of emacbctr register */
#define CLK_MNGR_EMACBCTR_CNT_MASK                   0x000007FFU
#define CLK_MNGR_EMACBCTR_CNT_POS                    0U

/* Bit fields of emacptpctr register */
#define CLK_MNGR_EMACPTPCTR_SRC_MASK                 0x00070000U
#define CLK_MNGR_EMACPTPCTR_SRC_POS                  16U
#define CLK_MNGR_EMACPTPCTR_CNT_MASK                 0x000007FFU
#define CLK_MNGR_EMACPTPCTR_CNT_POS                  0U

/* Bit fields of gpiodbctr register */
#define CLK_MNGR_GPIODBCTR_SRC_MASK                  0x00070000U
#define CLK_MNGR_GPIODBCTR_SRC_POS                   16U
#define CLK_MNGR_GPIODBCTR_CNT_MASK                  0x000007FFU
#define CLK_MNGR_GPIODBCTR_CNT_POS                   0U

/* Bit fields of s2fuser0ctr register */
#define CLK_MNGR_S2FUSER0CTR_SRC_MASK                0x00070000U
#define CLK_MNGR_S2FUSER0CTR_SRC_POS                 16U
#define CLK_MNGR_S2FUSER0CTR_CNT_MASK                0x000007FFU
#define CLK_MNGR_S2FUSER0CTR_CNT_POS                 0U

/* Bit fields of s2fuser1ctr register */
#define CLK_MNGR_S2FUSER1CTR_SRC_MASK                0x00070000U
#define CLK_MNGR_S2FUSER1CTR_SRC_POS                 16U
#define CLK_MNGR_S2FUSER1CTR_CNT_MASK                0x000007FFU
#define CLK_MNGR_S2FUSER1CTR_CNT_POS                 0U

/* Bit fields of psirefctr register */
#define CLK_MNGR_PSIREFCTR_SRC_MASK                  0x00070000U
#define CLK_MNGR_PSIREFCTR_SRC_POS                   16U
#define CLK_MNGR_PSIREFCTR_CNT_MASK                  0x000007FFU
#define CLK_MNGR_PSIREFCTR_CNT_POS                   0U

/* Bit fields of extcntrst register */
#define CLK_MNGR_EXTCNTRST_CORE3CNTRST_MASK          0x00002000U
#define CLK_MNGR_EXTCNTRST_CORE3CNTRST_POS           13U
#define CLK_MNGR_EXTCNTRST_CORE2CNTRST_MASK          0x00001000U
#define CLK_MNGR_EXTCNTRST_CORE2CNTRST_POS           12U
#define CLK_MNGR_EXTCNTRST_CORE01CNTRST_MASK         0x00000800U
#define CLK_MNGR_EXTCNTRST_CORE01CNTRST_POS          11U
#define CLK_MNGR_EXTCNTRST_DSUCNTRST_MASK            0x00000400U
#define CLK_MNGR_EXTCNTRST_DSUCNTRST_POS             10U
#define CLK_MNGR_EXTCNTRST_USB31REFCNTRST_MASK       0x00000100U
#define CLK_MNGR_EXTCNTRST_USB31REFCNTRST_POS        8U
#define CLK_MNGR_EXTCNTRST_PSIREFCNTRST_MASK         0x00000080U
#define CLK_MNGR_EXTCNTRST_PSIREFCNTRST_POS          7U
#define CLK_MNGR_EXTCNTRST_S2FUSER1CNTRST_MASK       0x00000040U
#define CLK_MNGR_EXTCNTRST_S2FUSER1CNTRST_POS        6U
#define CLK_MNGR_EXTCNTRST_S2FUSER0CNTRST_MASK       0x00000020U
#define CLK_MNGR_EXTCNTRST_S2FUSER0CNTRST_POS        5U
#define CLK_MNGR_EXTCNTRST_GPIODBCNTRST_MASK         0x00000008U
#define CLK_MNGR_EXTCNTRST_GPIODBCNTRST_POS          3U
#define CLK_MNGR_EXTCNTRST_EMACPTPCNTRST_MASK        0x00000004U
#define CLK_MNGR_EXTCNTRST_EMACPTPCNTRST_POS         2U
#define CLK_MNGR_EXTCNTRST_EMACBCNTRST_MASK          0x00000002U
#define CLK_MNGR_EXTCNTRST_EMACBCNTRST_POS           1U
#define CLK_MNGR_EXTCNTRST_EMACACNTRST_MASK          0x00000001U
#define CLK_MNGR_EXTCNTRST_EMACACNTRST_POS           0U

/* Bit fields of usb31ctr register */
#define CLK_MNGR_USB31CTR_SRC_MASK                   0x00070000U
#define CLK_MNGR_USB31CTR_SRC_POS                    16U
#define CLK_MNGR_USB31CTR_CNT_MASK                   0x000007FFU
#define CLK_MNGR_USB31CTR_CNT_POS                    0U

/* Bit fields of dsuctr register */
#define CLK_MNGR_DSUCTR_SRC_MASK                     0x00070000U
#define CLK_MNGR_DSUCTR_SRC_POS                      16U
#define CLK_MNGR_DSUCTR_CNT_MASK                     0x000007FFU
#define CLK_MNGR_DSUCTR_CNT_POS                      0U

/* Bit fields of core01ctr register */
#define CLK_MNGR_CORE01CTR_SRC_MASK                  0x00070000U
#define CLK_MNGR_CORE01CTR_SRC_POS                   16U
#define CLK_MNGR_CORE01CTR_CNT_MASK                  0x000007FFU
#define CLK_MNGR_CORE01CTR_CNT_POS                   0U

/* Bit fields of core23ctr register */
#define CLK_MNGR_CORE23CTR_SRC_MASK                  0x00070000U
#define CLK_MNGR_CORE23CTR_SRC_POS                   16U

/* Bit fields of core2ctr register */
#define CLK_MNGR_CORE2CTR_CNT_MASK                   0x000007FFU
#define CLK_MNGR_CORE2CTR_CNT_POS                    0U

/* Bit fields of core3ctr register */
#define CLK_MNGR_CORE3CTR_CNT_MASK                   0x000007FFU
#define CLK_MNGR_CORE3CTR_CNT_POS                    0U

/* Bit fields of serial_con_pll_ctr register */
#define CLK_MNGR_SERIAL_CON_PLL_CTR_EN_MASK          0x00000001U
#define CLK_MNGR_SERIAL_CON_PLL_CTR_EN_POS           0U


#endif
