/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for I3C low level driver
 */
#ifndef __SOCFPGA_I3C_LL_H__
#define __SOCFPGA_I3C_LL_H__

#include "osal.h"

#define I3C_CORE_CLOCK    (200U * MHZ)                                /* 200 MHz : i3c core clock source is 14_mp_clk
                                                                         and according to Fig259 (TRM)14_mp_clk value is 200 MHz */


#define I3C_MAX_DEVICES    (8U)
#define I3C_MAX_XFER       (16U)

#define I3C_CONTROLLER_REGISTER_BASE(inst)    (((inst) == I3C_INSTANCE1) \
    ? 0x10DA0000   \
    : 0x10DA1000)

#define GET_I3C_INTERRUPT_ID(instance)    (((instance) == I3C_INSTANCE1) \
    ? I3C0IRQ : I3C1IRQ)

#define I3C_IBI_MR_REQ_REJECT     (0x2CU)
#define I3C_IBI_SIR_REQ_REJECT    (0x2CU)

/* IBI MR Req reject register*/
#define MR_REQ_REJECT_POS     (0U)
#define MR_REQ_REJECT_LEN     (32U)
#define MR_REQ_REJECT_MASK    (((1UL << (MR_REQ_REJECT_LEN)) - \
    1UL) << MR_REQ_REJECT_POS)

/* IBI SIR Req reject register*/
#define SIR_REQ_REJECT_POS     (0U)
#define SIR_REQ_REJECT_LEN     (32U)
#define SIR_REQ_REJECT_MASK    (((1UL << (SIR_REQ_REJECT_LEN)) - \
    1UL) << SIR_REQ_REJECT_POS)

/* interrupt signal enable register */
#define TRANSFER_ERR_SIGNAL_EN_POS       (9U)
#define TRANSFER_ABORT_SIGNAL_EN_POS     (5U)
#define RESP_READY_SIGNAL_EN_POS         (4U)
#define CMD_QUEUE_READY_SIGNAL_EN_POS    (3U)
#define IBI_THLD_SIGNAL_EN_POS           (2U)
#define RX_THLD_SIGNAL_EN_POS            (1U)
#define TX_THLD_SIGNAL_EN_POS            (0U)

#define TRANSFER_ERR_INTR       ((uint32_t)1 << TRANSFER_ERR_SIGNAL_EN_POS)
#define TRANSFER_ABORT_INTR     ((uint32_t)1 << TRANSFER_ABORT_SIGNAL_EN_POS)
#define RESP_READY_INTR         ((uint32_t)1 << RESP_READY_SIGNAL_EN_POS)
#define CMD_QUEUE_READY_INTR    ((uint32_t)1 << CMD_QUEUE_READY_SIGNAL_EN_POS)
#define IBI_THLD_INTR           ((uint32_t)1 << IBI_THLD_SIGNAL_EN_POS)
#define RX_THLD_INTR            ((uint32_t)1 << RX_THLD_SIGNAL_EN_POS)
#define TX_THLD_INTR            ((uint32_t)1 << TX_THLD_SIGNAL_EN_POS)

#define  RX_TX_DATA_PORT_SIZE    (4U)                            /* number of bytes*/

/*Reset manager peripheral reset register*/
#define PER1MODRST              (0x10D11028U)
#define PER1MODRST_I3C0_POS     (13U)
#define PER1MODRST_I3C0_MASK    ((0x1U << PER1MODRST_I3C0_POS))

#define PER1MODRST_I3C1_POS     (14U)
#define PER1MODRST_I3C1_MASK    ((0x1U << PER1MODRST_I3C1_POS))
#define LEGACY_I2C_MASK         (31U)
/* register read/write MACROS*/
#define HAL_REG_WRITE_FIELD(addr, pos, mask, value)    do {                                       \
        uint32_t temp_value;                   \
        temp_value = RD_REG32((addr));    \
        temp_value &= ~(mask);                   \
        temp_value |= ((uint32_t)(value) << (pos)) & (mask);   \
        WR_REG32((addr), temp_value);     \
}while(false)

#define HAL_REG_READ_FIELD(addr, pos, mask) \
    ((RD_REG32((addr)) & (mask)) >> (pos))


#define I3C_MAX_ADDR          (0x7FU)
#define I3C_BROADCAST_ADDR    (0x7EU)

#define NUM_BITS_PER_TABLE_ENTRY    ((sizeof(uint32_t) * 8U))

#define SCL_I3C_TIMING_CNT_MIN    (5U)

/* I3c Bus timing rates*/
#define I3C_BUS_SDR1_SCL_RATE          (8000000U)                      /* 8 MHz */
#define I3C_BUS_SDR2_SCL_RATE          (6000000U)                      /* 6 MHz */
#define I3C_BUS_SDR3_SCL_RATE          (4000000U)                      /* 4 MHz */
#define I3C_BUS_SDR4_SCL_RATE          (2000000U)                      /* 2 MHz */
#define I3C_BUS_I2C_FM_TLOW_MIN_NS     (1300U)
#define I3C_BUS_I2C_FMP_TLOW_MIN_NS    (500U)
#define I3C_BUS_THIGH_MAX_NS           (41U)

#define I3C_BUS_TYP_I3C_SCL_RATE        (12500000U)                     /* 12.5 MHz */
#define I3C_BUS_I2C_FM_PLUS_SCL_RATE    (1000000U)                     /*1 MHz*/
#define I3C_BUS_I2C_FM_SCL_RATE         (400000U)                     /* 400 KHz */
#define I3C_BUS_TLOW_OD_MIN_NS          (200U)

/* values specifying the entries in the Address allotment table*/
#define ADDRESS_ENTRY_STATUS_FREE    0U
#define ADDRESS_ENTRY_STATUS_I3C     1U
#define ADDRESS_ENTRY_STATUS_I2C     2U
#define ADDRESS_ENTRY_STATUS_RSVD    3U
#define ADDRESS_ENTRY_STATUS_MAX     3U

#define I3C_CONTROLLER_MASTER                      1U
#define I3C_CONTROLLER_PROGAMMABLE_MASTER_SLAVE    2U
#define I3C_CONTROLLER_SECONDARY                   3U
#define I3C_CONTROLLER_SLAVE                       4U

/*Internal error codes*/
#define I3C_OK          0
#define I3C_DENIED      1
#define I3C_TIMEDOUT    2
#define I3C_IO          3
#define I3C_NOMEM       4
#define I3C_BUSY        5
#define I3C_INVALID     6
#define I3C_PARAM       7

enum i3c_ccc_reset_action_def_byte
{
    I3C_CCC_RESET_ACTION_NO_RESET               = 0x00,
    I3C_CCC_RESET_ACTION_PERIPHERAL_ONLY        = 0x01,
    I3C_CCC_RESET_ACTION_RESET_WHOLE_TARGET     = 0x02,
    I3C_CCC_RESET_ACTION_DEBUG_NETWORK_ADAPTER  = 0x03,
    I3C_CCC_RESET_ACTION_VIRTUAL_TARGET_DETECT  = 0x04,
};

/* I3C command packet attributes*/
#define I3C_CCC_TRANSFER_CMD          0x0U
#define I3C_CCC_TRANSFER_ARG          0x1U
#define I3C_CCC_SHORT_DATA_ARG        0x2U
#define I3C_CCC_ADDRESS_ASSIGN_CMD    0x3U

/* I3C command response values*/
#define I3C_RESPONSE_OK                  0
#define I3C_RESPONSE_CRC_ERROR           1
#define I3C_RESPONSE_PARITY_ERROR        2
#define I3C_RESPONSE_FRAME_ERROR         3
#define I3C_RESPONSE_BRAODCAST_NAK       4
#define I3C_RESPONSE_ADDRESS_NAK         5
#define I3C_RESPONSE_BUF_OVERFLOW        6
#define I3C_RESPONSE_RESERVED_7          7
#define I3C_RESPONSE_XFER_ABORT          8
#define I3C_RESPONSE_SLAVE_WRITE_NACK    9
#define I3C_RESPONSE_RESERVED_10         10
#define I3C_RESPONSE_RESERVED_11         11
#define I3C_RESPONSE_PEC_ERROR           12
#define I3C_RESPONSE_RESERVED_13         13
#define I3C_RESPONSE_RESERVED_14         14
#define I3C_RESPONSE_RESERVED_15         15

/* structure used to reflect the response
   data structure defined in
   TRM section 5.8.6.5.7 (Master command data structures)
 */
union i3c_xfer_response {
    struct
    {
        uint16_t dl         : 16;
        uint32_t ccct       : 8;
        uint8_t tid        : 4;
        int32_t err_sts    : 4;
    } field;
    uint32_t value;
};

/* Data structure used to reflect the
   transfer command structure defined in
   TRM section 5.8.6.5.7 (Master command data structures)
 */
union i3c_xfer_cmd {
    struct
    {
        uint32_t cmd_attr   : 3;
        uint32_t tid        : 4;
        uint32_t cmd        : 8;
        uint32_t cp         : 1;
        uint32_t dev_index  : 5;
        uint32_t speed      : 3;
        uint32_t reserved_1 : 1;
        uint32_t dbp        : 1;
        uint32_t roc        : 1;
        uint32_t sdap       : 1;
        uint32_t rnw        : 1;
        uint32_t reserved_2 : 1;
        uint32_t toc        : 1;
        uint32_t pec        : 1;
    } field;
    uint32_t value;
};

/* Data structure used to reflect the address
   assignment command structure defined in
   TRM section 5.8.6.5.7 (Master command data structures)
 */
union i3c_addr_assign_cmd {
    struct
    {
        uint32_t cmd_attr   : 3;
        uint32_t tid        : 4;
        uint32_t cmd        : 8;
        uint32_t reserved_1 : 1;
        uint32_t dev_index  : 5;
        uint32_t dev_count  : 5;
        uint32_t roc        : 1;
        uint32_t reserved_2 : 3;
        uint32_t toc        : 1;
        uint32_t reserved_3 : 1;
    } field;
    uint32_t value;
};

union i3c_ccccmd
{
    union i3c_addr_assign_cmd addr;
    union i3c_xfer_cmd xfer;
};

/* Data structure used to reflect the command argument
   structure defined in TRM section 5.8.6.5.7
   (Master command data structures)
 */
union i3c_cmd_arg {
    struct
    {
        uint32_t cmd_attr   : 3;
        uint32_t reserved   : 5;
        uint32_t db         : 8;
        uint32_t dl         : 16;
    } field;
    uint32_t value;
};

/* command object used by the low level driver api to actually
   transmit the command. The structure reflects the master transfer
   command and address assignment command structures defined in the
   TRM section 5.8.6.5.7(Master command data structures)
 */
struct i3c_cmd_obj
{
    union i3c_ccccmd cmd;
    union i3c_cmd_arg arg;
    uint8_t *data;
    uint16_t rx_length;
    uint16_t tx_length;

    /*Additional variable for complete transfer*/
    uint16_t write_bytes_left;
    uint8_t *write_buffer;
    uint16_t read_bytes_left;
    uint8_t *read_buffer;
    int32_t status;
};

/* command payload structure.
   The I3C driver use this to pass CCC command information to the
   underlying Low level driver api to actually affect the command
   transfer.
 */
struct i3c_cmd_payload
{
    uint8_t cmd_id;              /* CCC command code*/
    bool read;                  /* true for read, false for write*/
    uint8_t *data;              /* pointer to the data */
    uint16_t data_length;        /* length of the data in bytes (pointed to by data field)*/
    uint8_t target_address;      /* address of the target, if set to 0, the command will be*/
};


/*
   Data structure which holds the context of the I3C controller instance.
   It contains the list of all the attached devices, configuration parameters
   role of the controller and other book keeping entries.
 */
struct i3c_device_desc
{

    struct i3c_i3c_device device;

    uint32_t dat_index;

    uint8_t BCR;    /* Bus characteristics register*/
    uint8_t DCR;    /* Device Characteristics register*/

    struct
    {
        uint8_t max_read;                /* maximum read speed*/
        uint8_t max_write;               /* maximum Write speed*/
        uint32_t max_read_turnaround;     /* maximum turnaround time for Read*/
    } data_speed;

    struct
    {
        uint16_t mrl;           /* Maximum Read Length */
        uint16_t mwl;           /* Maximum Write Length */
        uint8_t max_ibi;        /* Maximum IBI Payload Size. Valid only if BCR[2] is 1. */
    } data_length;
};

/* Callback function for i3c operations */
typedef void (*i3c_callback_t)(int stat,
        void *param);

/*
 * internal controller object, defines the controller driver instance.
 * maintains the list of all the connected targets, the addresses being
 * allotted, data and cmd fifo depths, and the memory mapped address of
 * controller instance.
 * This object defines the i3c controller instance.
 */
struct i3c_driver_obj
{
    uint32_t reg_base;                              /* register block memory mapped address */
    uint32_t *dev_address_table;                     /* memory address for device address table */
    uint32_t *dev_char_table;                        /* memory address for device characteristic table */

    uint32_t cmd_fifo_depth;                          /* maximum command FIFO depth*/
    uint32_t data_fifo_depth;                         /* maximum data fifo depth*/

    /* flag to indicate whether HDR is supported or not*/
    bool support_hdr;

    /* indicates the role of the controller,
     * true = primary, false = secondary
     */
    bool is_primary;

    uint8_t own_da;                              /*controllers own Dynamic Address */

    /* 2 bits for indicate whether that particular address is in use
     * 00b = free, 01b = I3C device , 10b = I2C device, 11b = reserved
     */
    uint32_t addr_allotment_table[((I3C_MAX_ADDR + 1U) * 2U) /
            NUM_BITS_PER_TABLE_ENTRY];

    uint32_t dat_free_mask;                           /* value at a bit position indicates the dat entry status
                                                         0 - indicate the dat entry is used
                                                         1 - indicates the dat entry is empty */
    uint16_t num_dev;
    struct i3c_device_desc i3c_dev_desc_list[I3C_MAX_DEVICES];

    uint16_t attached_dev_addr_list[I3C_MAX_DEVICES];                     /* contains the dynamic address of the devices successfuly
                                                                             attached to the controller.*/
    uint16_t num_attached_dev;                                            /* number of devices succesfully (with dynamic address assigned)
                                                                             attached to the controller*/
    struct i3c_cmd_obj cmd_obj[I3C_MAX_XFER];
    uint32_t num_xfers;
    BaseType_t is_async;
    BaseType_t is_busy;

    i3c_callback_t callback_fn;
    void *cb_usercontext;

    osal_semaphore_def_t xfer_sem_mem;
    osal_semaphore_def_t lock_mem;

    osal_semaphore_t xfer_complete;           /* signal to indicate the current xfer request is completed*/
    osal_semaphore_t lock;                   /* mutex to prevent concurrent access while an operation is ongoing */

};

extern int32_t i3c_ll_attach_i2c_device(uint8_t instance, struct
        i3c_device_desc *pdevice_desc, uint8_t address);

extern int32_t i3c_ll_attach_device(uint8_t instance, struct
        i3c_device_desc *pdevice_desc, uint8_t address);

extern int32_t i3c_ll_detach_device(uint8_t instance, struct
        i3c_device_desc *pdevice_desc, uint8_t address);

extern int32_t i3c_ll_send_addr_assignment_command(uint8_t instance, struct
        i3c_cmd_payload *pcmd_payload, uint32_t start_idx);

extern int32_t i3c_ll_send_xfer_command(uint8_t instance,
	 struct i3c_cmd_payload *pcmd_payload,
	 uint8_t num_cmds, bool is_i2c, bool is_async);

extern void i3c_ll_init(uint8_t instance);

#endif // __SOCFPGA_I3C_LL_H__
