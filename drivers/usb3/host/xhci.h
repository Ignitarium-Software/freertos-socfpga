/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for xHCI implementation
 */

#ifndef __XHCI_H__
#define __XHCI_H__

#include <errno.h>
#include <stdint.h>
#include "xhci_context.h"
#include "xhci_rings.h"

#define LINK_TRB                     (6U)     /* !< Link TRB id */
#define TRB_FIELD                    (10U)    /* !< TRB id field position */
#define SLOTID_FIELD                 (24U)    /* !< Slot id field position */
#define XHC_DEV_ADDR_MSK             (0xffU)   /* !< device address field mask */
#define XHCI_CR_TRB_LEN              (16)     /* !< xHCI command ring length   */
#define XHCI_CR_CCS_FLAG             (1U)     /* !< xHCI command ring CCS flag */
#define XHCI_EP_TR_RING_ALIGN        (16)     /* !< EP transfer ring alignment requirement */
#define EP_TRB_SEG_LENGTH            (512U)    /* !< EP TR segment length */
#define XHCI_EVENT_RING_SEG_LENGTH   (512U)   /* !< Event Ring segment length */
#define START_XHCI                   (1U)     /* !< Start the xHCI controller */

typedef enum
{
    ISO_OUT = 1,        /* !< Isochronous OUT EP */
    BULK_OUT,           /* !< Bulk OUT EP */
    INTERRUPT_OUT,      /* !< Interrupt OUT EP */
    CONTROL,            /* !< Control  EP */
    ISO_IN,             /* !< Isochronous IN EP */
    BULK_IN,            /* !< Bulk IN EP */
    INTERRUPT_IN,       /* !< Interrupt IN EP */
}xhci_ep_type_t;

/**
 * @brief USB Standard Endpoint Descriptor.
 *
 * Describes an endpoint’s characteristics such as type, address, maximum packet size,
 * and polling interval. It is used during USB enumeration to inform the host of
 * endpoint capabilities.
 */
typedef struct
{
    uint8_t bLength;            /*!< Size of this descriptor in bytes */
    uint8_t bDescriptorType;    /*!< ENDPOINT descriptor type (0x05) */
    uint8_t bEndpointAddress;   /*!< The address of the endpoint on the USB device (bit 7 indicates direction: 0 = OUT, 1 = IN) */
    uint8_t bmAttributes;       /*!< Endpoint attributes: transfer type (bits 0–1), synchronization type (bits 2–3), usage type (bits 4–5) */
    uint16_t wMaxPacketSize;    /*!< Maximum packet size this endpoint is capable of sending or receiving */
    uint8_t bInterval;          /*!< Interval for polling endpoint for data transfers (in frames or microframes, depending on transfer type) */
}/*! \cond */
__attribute__ ((packed))
/*! \endcond */ usb_endpoint_descriptor_t;

/**
 * @brief Represents a standard USB control transfer setup packet.
 *
 * This structure defines the setup packet used in USB control transfers.
 * It corresponds to the standard USB setup packet format defined by the USB specification.
 */
typedef struct
{
    uint8_t bmRequestType;   /*!< Characteristics of the request:
                                 - Direction (bit 7)
                                 - Type (bits 5..6)
                                 - Recipient (bits 0..4) */
    uint8_t bRequest;        /*!< Specific request code */
    uint16_t wValue;         /*!< Word-sized field that varies according to request */
    uint16_t wIndex;         /*!< Typically used to pass an index or offset (e.g., interface or endpoint) */
    uint16_t wLength;        /*!< Number of bytes to transfer if there is a data stage */
}/*! \cond */
__attribute__ ((packed))
/*! \endcond */ usb_control_request_t;

/*
 * @struct  xhci_cap_reg
 * @brief   structure to store xhci capability register parameters
 */
typedef struct __attribute__ ((packed))
{
    uint8_t caplength;                  /* !< caplength register */
    uint8_t rsvd;                       /* !< reserved */
    uint16_t hciversion;                /* !< hci version */
    union
    {
        struct hcsparams1_reg_
        {
            uint32_t max_dev_slots : 8;     /* !< max device slots supported by xHCI */
            uint32_t max_interrupters : 11; /* !< max interrupters */
            uint32_t rsvd : 5;              /* !< reserved */
            uint32_t max_ports : 8;         /* !< Max ports supported by the xHCI */

        } hcsparams1_params __attribute__ ((packed));
        uint32_t hcsparams1;
    };
    uint32_t hcsparams2;        /* !< xHCI hcsparams2 register  */
    uint32_t hcsparams3;        /* !< xHCI hcsparams3 register  */
    uint32_t hccparams1;        /* !< xHCI hccparams1 register  */
    union
    {
        struct dboff_reg
        {
            uint32_t db_array_offset : 30;  /* !< Doorbell register offset */
            uint32_t rsvd : 2;              /* !< reserved */
        } dboff_params __attribute__ ((packed));
        uint32_t dboff;
    };
    union
    {
        struct rtsoff_reg
        {
            uint32_t rts_offset : 27;       /* !< Runtime register space offset */
            uint32_t rsvd : 5;              /* !< reserved */

        } rtsoff_params __attribute__ ((packed));
        uint32_t rtsoff;
    };
    uint32_t hccparams2;
} xhci_cap_reg_t;

/*
 * @struct  xhci_oper_reg_params
 * @brief   stores xhci operational base, runtime tbase and doorbell base
 *           address
 */
typedef struct xhci_oper_reg_params
{
    uint64_t xhci_op_base;      /* !< operation base offset */
    uint64_t xhci_runtime_base; /* !< Runtime base offset */
    uint64_t xhci_db_base;      /* !< Doorbell register offset */
}xhci_oper_reg_params_t;

#define DEV_ADDRESS_FIELD    32
#define SLOT_STATE_FIELD     59

/*
 * @struct  ep_priv
 * @brief   stores endpoint specific parameters
 */
struct ep_priv
{
    xhci_trb_t *ep_tr_enq_ptr;          /* !< TR enqueue pointer */
    usb_endpoint_descriptor_t ep_desc; /* !< endpoint descriptor */
    uint8_t pcs_flag;                   /* !< pcs flag for EP TR */
    uint8_t ep_addr;                    /* !< endpoint address */
    uint8_t ep_index;                   /* !< endpoint index */
    xhci_ep_type_t ep_type;             /* !< endpoint type */
} __attribute__ ((packed));

/*
 * struct to store MSC specific endpoint data
 */
typedef struct
{
    struct ep_priv ep_in;      /* !< Endpoint IN specific parameters */
    struct ep_priv ep_out;     /* !< Endpoint OUT specific parameters */

}msc_eps_params_t;

struct xhci_device_data
{
    uint8_t slot_id;        /* !< slot id of the device */
    uint8_t dev_addr;       /* !< address of the device */
    uint8_t dev_speed;      /* !< speed of the device */
    uint8_t rh_port;        /* !< RH port of the device */
};

/*
 * @brief xhci controller structure
 */
struct xhci_data
{
    struct xhci_device_data dev_data;  /* !< device specific data */
    struct ep_priv ep0;                 /* !< EP 0 params */
    msc_eps_params_t msc_eps;           /* !< MSC specific endpoint params */

    xcr_command_ring_t *xcr_ring;       /* !< xHCI command ring */
    xer_event_ring_t xer_ring;          /* !< xHCI event ring */

    xhci_cap_reg_t *xhc_cap_ptr;        /* !< xHCI capability register pointer */
    xhci_oper_reg_params_t op_regs;    /* !< xHCI operational register */

    xhci_ip_device_context_t *ip_ctx;   /* !< xHCI input context */
    xhci_op_device_context_t *op_ctx;   /* !< xHCI output context */

    struct xhci_device_context_array *dcbaa; /* !< dcbaa pointer array */
};

/**
 * @brief  allocate xhci context memories
 * @param[in]  xhci_ptr -  reference of xhci hcd controller
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int alloc_xhci_contexts(struct xhci_data *xhci_ptr);

/**
 * @brief  update the output device context entry for the corresponding device
 * slot id
 * @param[in] xhci_ptr : reference to xhci hcd structure
 */
void update_dcbaa_entry(struct xhci_data *xhci_ptr);

/**
 * @brief  deallocate xhci context memories
 * @param[in]  xhci_ptr -  reference of xhci hcd controller
 */
void deallocate_xhci_context(struct xhci_data *xhci_ptr);

/**
 * @brief  allocate xhci input context
 * @param[in]  xhci_ptr -  reference of xhci hcd controller
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int alloc_input_device_context(struct xhci_data *xhci_ptr);

/**
 * @brief : allocate xhci output device context
 * @param[in]  xhci_ptr -  reference of xhci hcd controller
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int alloc_output_device_context(struct xhci_data *xhci_ptr);

/**
 * @brief initialize input device context data structure
 * @param[in]  xhci -  reference of xhci hcd controller
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int init_input_device_context(struct xhci_data *xhci_ptr);

/**
 * @brief allocate xhci device context base address array and store the base
 *         address array into DCBAA register
 * @param[in]  xhci_ptr -  reference of xhci hcd controller
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int alloc_dcbaa(struct xhci_data *xhci_ptr);

/**
 * @brief update xhci slot context data structure
 * @param[in]  xhci_ptr -  reference of xhci hcd controller
 */
void update_xhc_slot_context(struct xhci_data *xhci_ptr);

/**
 * @brief  initialize endpoint context data structure
 * @param[in]  xhci -  reference of xhci hcd controller
 * @param[in]  ep_type -  endpoint type  (bulk, control etc)
 * @return
 *  RET_SUCCESS, on successful allocation
 *  RET_FAIL, incase of failure
 */
void init_xhc_endpoint_context(struct xhci_data *xhci, xhci_ep_type_t ep_type);

/**
 * @brief  return the device address from the output context data structure
 * @param[in]  xhci -  reference of xhci hcd controller
 * @return device address
 */
uint8_t get_xhc_device_address(struct xhci_data *xhci_ptr);

/*XHCI capability register APIs*/

/**
 * @brief  function to return the operational register parameters of xhci
 *        controller
 * @return operational register parameters
 */
xhci_oper_reg_params_t get_xhci_op_registers(void);

/**
 * @brief  function to get the xhci capability parameters controller
 * @param[in]  xhci -  reference of xhci hcd controller
 * @return
 *  0, if operation is successful
 *  errno, on failure
 */
int get_xhc_cap_params(struct xhci_data *xhci);

/**
 * @brief  function to check if the pointer address passed is byte aligned
 * @param[in]  ptr -  reference to xhci strucute
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int init_xhc_event_ring(struct xhci_data *xhci_ptr);

/**
 * @brief Initialize xHCI context parameters.
 * @param[in] xhci_ptr reference xhci data structure
 * @return
 *  0, on successful allocation
 *  errno, incase of failure
 */
int init_xhci_context_params(struct xhci_data *xhci_ptr);

/* Generic xhci APIS */

/**
 * @brief  function to check if the pointer address passed is byte aligned
 * @param[in]  ptr -  pointer to be checked byte aligned or not
 * @param[in]  byte - pointer byte alignment size
 * @return
 *  0  on successful allocation
 *  errno, incase of failure
 */
int is_ptr_mem_aligned(uint64_t addr, uint32_t byte);

/**
 * @brief  parse the endpoint descriptors
 * @param[in] xhci_ptr reference to xhci hcd structure
 * @param[in] desc endpoint descriptor
 * @return
 *  0 on successful allocation
 *  errno, incase of failure
 */

int xhci_parse_endpoint_descriptor(struct xhci_data *xhci_ptr,
        usb_endpoint_descriptor_t *desc);

/**
 * @brief Display device info
 */
void display_xhci_device_params(struct xhci_device_data *dev_data);

/**
 * @brief  reset the xHCI controller
 * @return
 *  0, on success
 *  errno, on failure
 */
int xhci_reset(void);

/**
 * @brief  start the xHCI controller
 */
void start_xhci_controller(void);

/**
 * @brief Initialize the xHCI registers
 * @return
 *  true on successful initialization
 *  false incase of failure
 */
bool xhci_init(struct xhci_data *xhci);

#endif  /*__XHCI_H__ */
