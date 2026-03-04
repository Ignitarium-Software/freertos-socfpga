/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for Rings sub-module of SoC FPGA USB3.1 XHCI low level driver
 */

#ifndef __XHCI_RINGS_H__
#define __XHCI_RINGS_H__

#include <stdio.h>
#include <stdint.h>

#define XHCI_EVENT_RING_TABLE_SZ    1U
#define ERDP                        0x11001038U
#define PORTSC                      0x11000420U

/*
 * @struct  xhci_trb_t
 * @brief   basic transfer request block for xhci controller
 * @note    refer to section 4.11.11 of xhci document
 */
typedef struct __attribute__ ((packed)){
    uint64_t buffer;        /*!< Bits 0-63: TRB-specific parameters (e.g., buffer pointer, device address, etc.) */
    uint32_t status;        /*!< Bits 64-95: Status information (e.g., transfer length, completion code, etc.)*/
    uint32_t control;       /*!< Bits 96-127: TRB type, flags, and Cycle Bit */
} xhci_trb_t;

/*
 *@struct  xhci_event_segment_t
 *@brief   Event ring segment table
 */
typedef struct __attribute__ ((packed)) {
    uint64_t xes_addr;          /*!< store the address of event ring segment */
    uint16_t xes_size;          /*!< number of TRB supported by the ring */
    uint16_t xes_rsvd0;
    uint32_t xes_rsvd1;
} xhci_event_segment_t;

/*
 *@struct  xcr_command_ring_t
 *@brief   Command Ring for xhci
 */
typedef struct __attribute__ ((packed))
{
    xhci_trb_t *xcr_enqueue_ptr;    /* !< xhci command ring enqueue pointer*/
    xhci_trb_t *xcr_dequeue_ptr;    /* !< xhci command ring dequeue pointer*/
    uint8_t xcr_ccs_flag;           /* !< xhci command ring consumer cycle state flag*/

} xcr_command_ring_t;

/*
 * @struct  xhci_erst_entry_t
 * @brief   Event Ring Segment table Entry ( ERST)
 */
typedef struct __attribute__ ((packed))
{
    uint64_t seg_addr;      /* !< xhci event ring segment address*/
    uint32_t seg_size;      /* !< xhci event ring segment size */
    uint32_t rsvd;          /* reserved */
} xhci_erst_entry_t;

/*
 * @struct  xer_event_ring_t
 * @brief   xhci event ring
 */
typedef struct __attribute__ ((packed))
{
    uint32_t trb_count;         /*!< Number of trbs in the event ring */
    xhci_trb_t *xer_enqueue_ptr;    /*!< xhci event ring enqueue pointer*/
    xhci_erst_entry_t *erst_ptr;      /* !< reference to xhci event ring segment table */

} xer_event_ring_t;


/* XHCI operational register APIS */

/**
 * @brief  initialize XHCI command ring
 * @params[in] xcr_ring reference to XHCI command ring structure
 * @return
 *  0 on successful command ring initialisation
 *  errno  on failure
 */
int xhci_crcr_init(xcr_command_ring_t *xcr_ring);

/**
 * @brief  allocate ring segment for command/transfer/events rings
 * @param[in] req_byte_align alignment of the trb
 * @param[in] req_trb_len length of the trb ring
 * @return
 *  xhci_trb_t* reference to Transfer ring structure
 *  NULL, if allocation is unsuccessful
 */
xhci_trb_t *allocate_ring_segment(int req_byte_align, int req_trb_len);

/**
 * @brief  allocate xhci command ring structure
 * @return
 *  xcr_command_ring_t* reference to command ring structure
 */
xcr_command_ring_t *alloc_command_ring(void);

/**
 * @brief  check whether the trb is a link trb or not
 * @params[in]  ptr reference to a trb structure
 * @return
 *   1 - if it is a link trb
 *   0 - if it is not a link trb
 */
int is_link_trb(xhci_trb_t *ptr);

/**
 * @brief  find the next segment of the transfer ring/command ring
 * @param[in] ptr reference to current trb segment
 * @param[in] pcs_flag reference to current segment pcs flag
 * @return xhci_trb_t* reference to new trb segment
 */
xhci_trb_t *get_next_trb_segment(xhci_trb_t *ptr, uint8_t *pcs_flag);

#endif /* __XHCI_RINGS_H__ */
