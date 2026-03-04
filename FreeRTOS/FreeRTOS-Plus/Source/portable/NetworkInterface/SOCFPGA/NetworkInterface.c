/*
 * FreeRTOS+TCP V3.1.0
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * Modifications to support Altera SoC FPGA
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_ARP.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"
#include "FreeRTOS_Routing.h"
#include <FreeRTOSIPConfig.h>

#include "socfpga_xgmac.h"
#include "socfpga_xgmac_phy.h"
/*-----------------------------------------------------------*/

/* If ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES is set to 1, then the Ethernet
 * driver will filter incoming packets and only pass the stack those packets it
 * considers need processing. */
#if ( ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES == 0 )
    #define ipCONSIDER_FRAME_FOR_PROCESSING( pucEthernetBuffer )    eProcessBuffer
#else
    #define ipCONSIDER_FRAME_FOR_PROCESSING( pucEthernetBuffer ) \
    eConsiderFrameForProcessing(                                 \
        ( pucEthernetBuffer ) )
#endif

#define EMAC_MAX_INSTANCE    3U

#ifndef niEMAC_HANDLER_TASK_PRIORITY
/* Define the priority of the task prvEMACHandlerTask(). */
    #define niEMAC_HANDLER_TASK_PRIORITY    configMAX_PRIORITIES - 1U
#endif

#define niBMSR_LINK_STATUS                  0x0004uL

#ifndef PHY_LS_HIGH_CHECK_TIME_MS

/* Check if the Link Status in the PHY is still high after 15 seconds of not
 * receiving packets. */
    #define PHY_LS_HIGH_CHECK_TIME_MS    15000
#endif

#ifndef PHY_LS_LOW_CHECK_TIME_MS
/* Check if the LinkSStatus in the PHY is still low every second. */
    #define PHY_LS_LOW_CHECK_TIME_MS    1000
#endif

/* Default the size of the stack used by the XGMAC deferred handler task to 4 times
 * the size of the stack used by the idle task. */
#ifndef configEMAC_TASK_STACK_SIZE
    #define configEMAC_TASK_STACK_SIZE    ( 4U * configMINIMAL_STACK_SIZE )
#endif

#if ( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM == 0 || \
      ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 0 )
    #warning \
    Please define both 'ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM' and 'ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM' as 1 to enable CRC offloading
#endif

/* Interrupt events to process.  Currently only the Rx event is processed
 * although code for other events is included to allow for possible future
 * expansion. */
#define XGMAC_IF_RX_EVENT     1U
#define XGMAC_IF_TX_EVENT     2U
#define XGMAC_IF_ERR_EVENT    4U
#define XGMAC_IF_ALL_EVENT                    \
    ( XGMAC_IF_RX_EVENT | XGMAC_IF_TX_EVENT | \
      XGMAC_IF_ERR_EVENT )

#define TX_BUFFER_COUNT       ( 512U )
#define TX_BUFFER_SIZE        XGMAC_MAX_PACKET_SIZE

#define RX_BUFFER_COUNT       ( 512U )
#define RX_BUFFER_SIZE        XGMAC_MAX_PACKET_SIZE


static NetworkInterface_t * AgxInterface = NULL;
/*-----------------------------------------------------------*/

/* Structure with TX data output buffer details*/
typedef struct
{
uint8_t * pTxBuffer[ TX_BUFFER_COUNT ];
uint16_t usHeadIndex;
uint16_t usTailIndex;
uint16_t usBufUsedCnt;
uint8_t ucIsInitiazed;
} TxBufferPool_t;
/*-----------------------------------------------------------*/

#if ( !( ipconfigZERO_COPY_TX_DRIVER ) )
static TxBufferPool_t TxBufferPool = { NULL };
static TxBufferPool_t * pxTxBufferPool = &TxBufferPool;
#endif

static SemaphoreHandle_t xTxBufSynchSem = NULL;

/* Structure with RX data output buffer details*/
typedef struct
{
uint8_t * pRxBuffer[ RX_BUFFER_COUNT ];
uint16_t usHeadIndex;
uint16_t usTailIndex;
uint16_t usBufUsedCnt;
uint8_t ucIsInitiazed;
} RxBufferPool_t;
/*-----------------------------------------------------------*/

#if ( !( ipconfigZERO_COPY_RX_DRIVER ) )
static RxBufferPool_t RxBufferPool = { NULL };
static RxBufferPool_t * pxRxBufferPool = &RxBufferPool;
#endif

static SemaphoreHandle_t xRxBufSynchSem = NULL;

BaseType_t prvUpdateRxDMADescriptors( uint8_t * pucRxBuffer,
                                      NetworkInterface_t * pxInterface );

/*
 * Create a Pool of DMA Tx and Rx Buffers in case Zero Copy disabled
 */
BaseType_t prvCreateTxBufferPool( TxBufferPool_t * pTxBufferPool );
BaseType_t prvCreateRxBufferPool( RxBufferPool_t * pRxBufferPool );

/*
 * Get Tx and Rx Buffers
 */
uint8_t * pucGetTXBuffer( TxBufferPool_t * pTxBufferPool,
                          size_t xWantedSize );
uint8_t * pucGetRXBuffer( RxBufferPool_t * pRxBufferPool,
                          size_t xWantedSize );

/*
 * Release Tx Buffers
 */
BaseType_t pucReleaseTXBuffer( TxBufferPool_t * pTxBufferPool,
                               void * pvBuffer );

/*
 * Reinit Tx and Rx Buffers in case Zero Copy disabled
 */
BaseType_t prvReinitTxBufferPool( TxBufferPool_t * pTxBufferPool );
BaseType_t prvReinitRxBufferPool( RxBufferPool_t * pRxBufferPool );

/*
 * A deferred interrupt handler for XGMAC DMA interrupt sources.
 */
void prvEMACHandlerTask( void * pvParameters );

static void prvHandleErrorEvents( uint8_t ucErrStatus,
                                  uint8_t ucErrChnlNum,
                                  NetworkInterface_t * pxInterface );

BaseType_t xNetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                    NetworkBufferDescriptor_t * const pxNetworkBuffer,
                                    BaseType_t bReleaseAfterSend );

/* Holds the handle of the task used as a deferred interrupt processor */
static TaskHandle_t xEMACTaskHandle = NULL;

void prvEMACIRQHanlderCallback( xgmac_int_status_t xIntrStatus,
                                void * pvIrqData );

/*
 * NetworkInterfaceInput function to receive a new packet and send it to IP_task
 */
BaseType_t prvNetworkInterfaceInput( NetworkInterface_t * pxInterface );

/*
 * prvNetworkInterfaceDown function to stop EMAC and DMA for recovery from error
 */
BaseType_t prvNetworkInterfaceOutDone( NetworkInterface_t * pxInterface );

/*
 * NetworkInterfaceInput function to receive a new packet and send it to IP_task
 */
void prvNetworkInterfaceDown( NetworkInterface_t * pxInterface );

static void prvPassEthMessages( NetworkBufferDescriptor_t * pxDescriptor );

static inline BaseType_t xReadPhyStatus( NetworkInterface_t * pxInterface );
BaseType_t GetPhyLinkStatus( struct xNetworkInterface * pxDescriptor );
BaseType_t prvPhyCheckLinkStatus( TickType_t xMaxTimeTicks,
                                  NetworkInterface_t * pxInterface );
/*-----------------------------------------------------------*/

/* A copy of PHY register 1: 'COPPER_STATUS_REG' */
static BaseType_t xPHYLinkStatus = pdFALSE;
/*-----------------------------------------------------------*/

/* The function xNetworkInterfaceInitialise() will be called as
 * long as it returns the value pdFAIL.
 * It will go through several stages as described in 'eEMACState'.
 */
typedef enum XGMAC_STATE
{
    XGMAC_EMACInit,
    XGMAC_PHYInit,
    XGMAC_DMAInit,
    XGMAC_EMACStart,
    XGMAC_PHYWait,
    XGMAC_Ready,
    XGMAC_Failed,
} XGMACState_t;
/*-----------------------------------------------------------*/

/**
 * @brief  Configuration parameters for XGMAC Device Config
 */

static xgmac_config_t xEmacConfig[ EMAC_MAX_INSTANCE ] __attribute__( ( aligned( 64 ) ) );
static SemaphoreHandle_t xSemaphoreCounterTx;
/*-----------------------------------------------------------*/

/* Initialize PHY parameters */
static xgmac_phy_config_t xPhyDev =
{
    .phy_address            = 0,
    .phy_identifier         = 0,
    .phy_interface          = ETH_PHY_IF_RGMII,
    .enable_autonegotiation = ETH_ENABLE_AUTONEG,
    .speed_mbps             = ETH_SPEED_1000_MBPS,
    .duplex                 = ETH_FULL_DUPLEX,
    .advertise              = ETH_ADVERTISE_ALL,
    .link_status            = 0,
};
/*-----------------------------------------------------------*/

static XGMACState_t eXGMACState = XGMAC_EMACInit;
/*-----------------------------------------------------------*/

static BaseType_t xNetworkInterfaceInitialise( NetworkInterface_t * pxInterface )
{
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
BaseType_t xReturn = pdFAIL;
BaseType_t xRetVal;
int32_t xStatus;
const TickType_t xWaitLinkDelay = pdMS_TO_TICKS( 1000U );
xgmac_handle_t pXGMACHandle = NULL;
uint8_t * pucBufferPool = NULL;

    eXGMACState = XGMAC_EMACInit;

    xSemaphoreCounterTx = xSemaphoreCreateCounting( 512, 0 );
    configASSERT( xSemaphoreCounterTx != NULL );

    AgxInterface = pxInterface;

    switch( eXGMACState )
    {
        case XGMAC_EMACInit:
            pXGMACHandle = xgmac_emac_init( ( xgmac_config_t * ) ( &( xEmacConfig[ instance ] ) ) );
            xEmacConfig[ instance ].hxgmac = pXGMACHandle;

            if( pXGMACHandle == NULL )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: EMAC Initialization Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            xStatus = xgmac_set_callback( pXGMACHandle, \
                                          prvEMACIRQHanlderCallback,
                                          xgmac_get_err_info( pXGMACHandle ) );

            if( xStatus != 0 )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: IRQ Callback Registration Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            /* Transition to PHY Init */
            eXGMACState = XGMAC_PHYInit;

        /* Fall through. */
        case XGMAC_PHYInit:

            /* Detect the PHY */
            xStatus = xgmac_phy_discover( pXGMACHandle, &xPhyDev );

            if( xStatus != 0 )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: PHY Detection Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            /* Initialize PHY */
            xStatus = xgmac_phy_initialize( pXGMACHandle, &xPhyDev );

            if( xStatus != 0 )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: PHY Initialization Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            /* Transition to DMA Init */
            eXGMACState = XGMAC_DMAInit;

        /* Fall through. */
        /* Initialize DMA and enable DMA interrupts */
        case XGMAC_DMAInit:
            /* Create DMA Buffer Pool for Tx */
            #if ( !( ipconfigZERO_COPY_TX_DRIVER != 0 ) )
                if( ( pxTxBufferPool->ucIsInitiazed ) == 0U )
                {
                    xRetVal = prvCreateTxBufferPool( pxTxBufferPool );

                    if( xRetVal != pdPASS )
                    {
                        FreeRTOS_printf( ( "SOCFPGA_XGMAC: Tx DMA Buffer Allocation Failed....\n" ) );
                        eXGMACState = XGMAC_Failed;
                        break;
                    }
                }
            #endif /* if ( !( ipconfigZERO_COPY_TX_DRIVER != 0 ) ) */
            /* Create DMA Buffer Pool for Rx */
            #if ( !( ipconfigZERO_COPY_RX_DRIVER != 0 ) )
                if( ( pxRxBufferPool->ucIsInitiazed ) == 0U )
                {
                    xRetVal = prvCreateRxBufferPool( pxRxBufferPool );

                    if( xRetVal != pdPASS )
                    {
                        FreeRTOS_printf( ( "SOCFPGA_XGMAC: Rx DMA Buffer Allocation Failed....\n" ) );
                        eXGMACState = XGMAC_Failed;
                        break;
                    }
                }

                pucBufferPool = ( uint8_t * ) pxRxBufferPool;

                if( pucBufferPool == NULL )
                {
                    FreeRTOS_printf( ( "SOCFPGA_XGMAC: Rx Buffer Pool is NULL....\n" ) );
                    eXGMACState = XGMAC_Failed;
                    break;
                }
            #endif /* if ( !( ipconfigZERO_COPY_RX_DRIVER != 0 ) ) */
            xStatus = xgmac_dma_initialize( pXGMACHandle );

            if( xStatus != 0 )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: DMA Initialization Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            /* ReFill DMA Rx Descriptors with Rx DMA Buffer Addresses */
            xRetVal = prvUpdateRxDMADescriptors( pucBufferPool, pxInterface );

            if( xRetVal != pdPASS )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: Update Rx Descriptors Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            /* Transition to EMAC Start */
            eXGMACState = XGMAC_EMACStart;

        /* Fall through. */
        /* Configure and start the EMAC */
        case XGMAC_EMACStart:
            xStatus = xgmac_cfg_speed_mode( pXGMACHandle, &xPhyDev );

            if( xStatus != 0 )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: Set EMAC operating Speed Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            xStatus = xgmac_emac_start( pXGMACHandle );

            if( xStatus != 0 )
            {
                FreeRTOS_printf( ( "SOCFPGA_XGMAC: EMAC Start Failed....\n" ) );
                eXGMACState = XGMAC_Failed;
                break;
            }

            /* Transition to Wait for PHY */
            eXGMACState = XGMAC_PHYWait;

        /* Fall through. */
        case XGMAC_PHYWait:
            ( void ) prvPhyCheckLinkStatus( xWaitLinkDelay, pxInterface );

            if( GetPhyLinkStatus( NULL ) == pdFALSE )
            {
                /* Link status is not yet high, stay in XGMAC_WaitPHY */
                break;
            }

            if( xEMACTaskHandle == NULL )
            {
                /* create task for deferred interrupt handler and initialize it's handle */
                ( void ) xTaskCreate( prvEMACHandlerTask, "EMAC",
                                      configEMAC_TASK_STACK_SIZE, pxInterface,
                                      niEMAC_HANDLER_TASK_PRIORITY, &xEMACTaskHandle );

                if( xEMACTaskHandle == NULL )
                {
                    eXGMACState = XGMAC_Failed;
                    break;
                }
            }

            /* Transition to EMAC Ready */
            eXGMACState = XGMAC_Ready;

        /* Fall through. */

        case XGMAC_Ready:
            /* The network driver is operational. */
            xReturn = pdPASS;
            break;

        case XGMAC_Failed:
            /* A fatal error has occurred, and the driver
             * can not start. */
            break;

        default:
            /* should not reach here */
            xReturn = pdFAIL;
            break;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xNetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                    NetworkBufferDescriptor_t * const pxNetworkBuffer,
                                    BaseType_t bReleaseAfterSend )
{
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
xgmac_handle_t pXGMACHandle = ( xgmac_handle_t ) xEmacConfig[ instance ].hxgmac;
int32_t xStatus;

xgmac_tx_buf_t xDMATxBuffer;
uint8_t * pucBuffer;
uint32_t ulDataLength = 0;

    if( pXGMACHandle == NULL )
    {
        FreeRTOS_printf( ( "XGMAC Handle is NULL, EMAC not initialized\n" ) );
        return pdFALSE;
    }

    /* Check Link Status and Call XGMAC transmit function */
    if( xPHYLinkStatus != pdFALSE )
    {
        iptraceNETWORK_INTERFACE_TRANSMIT();

        /* Check Buffer size and adjust to Max packet size */
        ulDataLength = pxNetworkBuffer->xDataLength;

        if( ulDataLength > XGMAC_MAX_PACKET_SIZE )
        {
            ulDataLength = XGMAC_MAX_PACKET_SIZE;
        }

        if( pxNetworkBuffer->pucEthernetBuffer == NULL )
        {
            FreeRTOS_printf( ( "Ethernet Buffer is NULL\n" ) );
            return pdFALSE;
        }

        #if ( ipconfigZERO_COPY_TX_DRIVER == 0 )
            /* Get Tx Buffer Index from DMA Tx Buffer Pool */
            pucBuffer = pucGetTXBuffer( pxTxBufferPool, XGMAC_MAX_PACKET_SIZE );
            configASSERT( pucBuffer != NULL );

            /* Copy the bytes from NW buffer to XGMAC Tx Buffer  */
            ( void ) memcpy( pucBuffer, pxNetworkBuffer->pucEthernetBuffer, ulDataLength );
        #else
            /* Pass the address of Network Buffer as-is */
            pucBuffer = ( uint8_t * ) pxNetworkBuffer->pucEthernetBuffer;
            bReleaseAfterSend = pdFALSE;
        #endif

        xDMATxBuffer.buf = pucBuffer;
        xDMATxBuffer.size = ulDataLength;

        /* The TCP/IP buffer should be released back to stack after tx done
         * hence set the flag. This will be used in DMA TRansmit Done */
        xDMATxBuffer.release_buf = 1;

        if( pXGMACHandle == NULL )
        {
            return pdFALSE;
        }

        /* XGMAC transmit function */
        xStatus = xgmac_dma_transmit( pXGMACHandle, &( xDMATxBuffer ) );

        if( xStatus != 0 )
        {
            bReleaseAfterSend = pdTRUE;
        }
    }

    if( bReleaseAfterSend != pdFALSE )
    {
        /* Release NW buffer if No link or failure in Transmit */
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
    }

    return pdTRUE;
}
/*-----------------------------------------------------------*/

BaseType_t prvPhyCheckLinkStatus( TickType_t xMaxTimeTicks,
                                  NetworkInterface_t * pxInterface )
{
TickType_t xStartTime, xEndTime;
const TickType_t xShortDelay = pdMS_TO_TICKS( 20UL );
BaseType_t xReturn;

    xStartTime = xTaskGetTickCount();

    for( ; ; )
    {
        xEndTime = xTaskGetTickCount();

        if( ( xEndTime - xStartTime ) > xMaxTimeTicks )
        {
            xReturn = pdFALSE;
            break;
        }

        xPHYLinkStatus = xReadPhyStatus( pxInterface );

        if( xPHYLinkStatus != pdFALSE )
        {
            xReturn = pdTRUE;
            break;
        }

        vTaskDelay( xShortDelay );
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

static void prvPassEthMessages( NetworkBufferDescriptor_t * pxDescriptor )
{
IPStackEvent_t xRxEvent;

    xRxEvent.eEventType = eNetworkRxEvent;
    xRxEvent.pvData = (void*) pxDescriptor;

    if( xSendEventStructToIPTask( &xRxEvent, ( TickType_t ) 1000U ) != pdPASS )
    {
        /* The buffer could not be sent to the stack so must be released again.
         * This is a deferred handler task, not a real interrupt, so it is ok to
         * use the task level function here. */
        #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
        {
            do
            {
            NetworkBufferDescriptor_t * pxNext = pxDescriptor->pxNextBuffer;
                vReleaseNetworkBufferAndDescriptor( pxDescriptor );
                pxDescriptor = pxNext;
            } while( pxDescriptor != NULL );
        }
        #else
        {
            vReleaseNetworkBufferAndDescriptor(pxDescriptor);
        }
        #endif /* ipconfigUSE_LINKED_RX_MESSAGES */

        iptraceETHERNET_RX_EVENT_LOST();
        FreeRTOS_printf( ( "prvPassEthMessages: Can not queue return packet!\n" ) );
    }
    else
    {
        iptraceNETWORK_INTERFACE_RECEIVE();
    }
}
/*-----------------------------------------------------------*/

BaseType_t prvNetworkInterfaceInput( NetworkInterface_t * pxInterface )
{
    #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
        NetworkBufferDescriptor_t * pxFirstDescriptor = NULL;
        NetworkBufferDescriptor_t * pxLastDescriptor = NULL;
    #endif
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
xgmac_handle_t pXGMACHandle = ( xgmac_handle_t ) xEmacConfig[ instance ].hxgmac;
size_t xReceivedPacketLength;
int32_t xStatus;
xgmac_rx_buf_t xDMARxBufferIn;
uint8_t * pucEthernetBuffer;
uint8_t * pucRefillRxBuffer;
uint32_t ulPacketStatus;
volatile int msgCount = 0;

    do
    {
    NetworkBufferDescriptor_t * pxCurrentBufferDesc, * pxNewBufferDesc = NULL;
    BaseType_t xSendPacket = pdTRUE;

        /* XGMAC receive function */
        xStatus = xgmac_dma_receive( pXGMACHandle, &( xDMARxBufferIn ) );

        if( xStatus != 0 )
        {
            break;
        }

        pucEthernetBuffer = xDMARxBufferIn.buf;
        xReceivedPacketLength = ( size_t ) ( xDMARxBufferIn.size );
        pucRefillRxBuffer = pucEthernetBuffer;

        if( pucEthernetBuffer == NULL )
        {
            break;
        }

        /* Check the validity of the received packet and send only if valid else discard here */
        /* Check if packet has errors and discard if true */
        ulPacketStatus = xDMARxBufferIn.packet_status;

        if( ( ulPacketStatus &
              ( RDES3_NORM_WR_LD_MASK | RDES3_NORM_WR_ES_MASK ) ) ==
            XGMAC_RX_PACKET_ERROR )
        {
            xSendPacket = pdFALSE;
        }
        else if( eConsiderFrameForProcessing( pucEthernetBuffer ) !=
                 eProcessBuffer )
        {
            xSendPacket = pdFALSE;
        }
        else
        {
            pxNewBufferDesc = pxGetNetworkBufferWithDescriptor(
                XGMAC_MAX_PACKET_SIZE, ( TickType_t ) 0 );

            if( pxNewBufferDesc == NULL )
            {
                /* A packet has been received, but there is no replacement buffer and hence packet
                 * will be dropped
                 */
                FreeRTOS_printf( ( "Unable to allocate a Network Buffer\n" ) );
                xSendPacket = pdFALSE;
            }
        }

        if( xSendPacket != pdFALSE )
        {
            #if ( ipconfigZERO_COPY_RX_DRIVER != 0 )
            {
                pxCurrentBufferDesc = pxPacketBuffer_to_NetworkBuffer(
                    pucEthernetBuffer );
                configASSERT( pxCurrentBufferDesc != NULL );


                pucRefillRxBuffer = pxNewBufferDesc->pucEthernetBuffer;
            }
            #else
            {
                /* In zero copy mode both descriptors are the same. */
                pxCurrentBufferDesc = pxNewBufferDesc;

                if( pxCurrentBufferDesc == NULL )
                {
                    break;
                }

                if( pxNewBufferDesc != NULL )
                {
                    /* Update the Buf1 address in Desc0 field as this replaces old buffer  */
                    ( void ) memcpy( pxNewBufferDesc->pucEthernetBuffer,
                                     pucEthernetBuffer,
                                     xReceivedPacketLength );
                }
            }
            #endif /* if ( ipconfigZERO_COPY_RX_DRIVER != 0 ) */
            /*Subtract 4 bytes of CRC from recieved packet*/
            pxCurrentBufferDesc->xDataLength = ( xReceivedPacketLength - 4U );

            pxCurrentBufferDesc->pxInterface = AgxInterface;
            pxCurrentBufferDesc->pxEndPoint = FreeRTOS_MatchingEndpoint(
                pxCurrentBufferDesc->pxInterface,
                pxCurrentBufferDesc->pucEthernetBuffer );


            #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
            {
                pxCurrentBufferDesc->pxNextBuffer = NULL;

                if( pxFirstDescriptor == NULL )
                {
                    /* Becomes the first message */
                    pxFirstDescriptor = pxCurrentBufferDesc;
                }
                else if( pxLastDescriptor != NULL )
                {
                    /* Add to the tail */
                    pxLastDescriptor->pxNextBuffer = pxCurrentBufferDesc;
                }
                else
                {
                    /*do nothing*/
                }

                pxLastDescriptor = pxCurrentBufferDesc;
            }
            #else /* if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 ) */
            {
                prvPassEthMessages(pxCurrentBufferDesc);
            }
            #endif /* if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 ) */
            msgCount++;
        }

        if( ( pXGMACHandle == NULL ) || ( pucRefillRxBuffer == NULL ) )
        {
            return 0;
        }

        /* Update the descriptor with new address and rest other fields */
        if( xgmac_refill_rx_descriptor( pXGMACHandle, pucRefillRxBuffer ) != 0 )
        {
            FreeRTOS_printf( ( "SOCFPGA_XGMAC: Refill Rx Descriptor Failed....\n" ) );
            break;
        }
    } while( true );

    #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
    {
        if( pxFirstDescriptor != NULL )
        {
            prvPassEthMessages( pxFirstDescriptor );
        }
    }
    #endif /* ipconfigUSE_LINKED_RX_MESSAGES */

    return msgCount;
}
/*-----------------------------------------------------------*/


BaseType_t GetPhyLinkStatus( struct xNetworkInterface * pxDescriptor )
{
    ( void ) pxDescriptor;
BaseType_t xReturn;

    if( xPHYLinkStatus == pdFALSE )
    {
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/


static inline BaseType_t xReadPhyStatus( NetworkInterface_t * pxInterface )
{
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
xgmac_handle_t pXGMACHandle = ( xgmac_handle_t ) xEmacConfig[ instance ].hxgmac;

    return phy_get_link_status( xgmac_get_inst_base_addr( pXGMACHandle ),
                                           &xPhyDev );
}
/*-----------------------------------------------------------*/

__attribute__( ( aligned( 64 ) ) )

static uint8_t ucNetworkPackets[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS *
                                 XGMAC_MAX_PACKET_SIZE ];

void vNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )

{
uint8_t * ucRAMBuffer = ucNetworkPackets;
uint32_t ul;

    for( ul = 0; ul < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; ul++ )
    {
        pxNetworkBuffers[ ul ].pucEthernetBuffer = ucRAMBuffer +
                                                   ipBUFFER_PADDING;

        *( ( uintptr_t * ) ucRAMBuffer ) =
            ( uintptr_t ) ( &( pxNetworkBuffers[ ul ] ) );

        ucRAMBuffer += XGMAC_MAX_PACKET_SIZE;
    }
}
/*-----------------------------------------------------------*/

static void prvHandleErrorEvents( uint8_t ucErrStatus,
                                  uint8_t ucErrChnlNum,
                                  NetworkInterface_t * pxInterface )
{
BaseType_t ucRemainInErrState = pdFALSE;
xgmac_err_t xErrType = ( xgmac_err_t ) ucErrStatus;
BaseType_t xStatus = xReadPhyStatus( pxInterface );

    if( xStatus != xPHYLinkStatus )
    {
        /* PHY state changed, mark as down for reconfigure */
        xPHYLinkStatus = 0;
    }
    switch( xErrType )
    {
        case XGMAC_ERR_FATAL_BUS:
            FreeRTOS_printf( ( "Fatal Bus Error on DMA Channel %d\n", ucErrChnlNum ) );
            break;

        case XGMAC_ERR_TX_STOPPED:
            FreeRTOS_printf( ( "Transmit Stopped on DMA Channel %d Re-init NetworkInterface\n", \
                               ucErrChnlNum ) );
            break;

        case XGMAC_ERR_RX_STOPPED:
            FreeRTOS_printf( ( "Receive Stopped on DMA Channel %d Re-init NetworkInterface\n", \
                               ucErrChnlNum ) );
            break;

        case XGMAC_ERR_TX_BUF_UNAVAILABLE:
            FreeRTOS_printf( ( "Transmit Buffer Unavailable Error on DMA Channel %d\n",
                               ucErrChnlNum ) );
            break;

        case XGMAC_ERR_RX_BUF_UNAVAILABLE:
            FreeRTOS_printf( ( "Receive Buffer Unavailable Error on DMA Channel %d\n", ucErrChnlNum ) );
            break;

        case XGMAC_ERR_CNTXT_DESC:
            FreeRTOS_printf( ( "Context Descriptor Error on DMA Channel %d\n",
                               ucErrChnlNum ) );
            break;

        case XGMAC_ERR_DESC_DEFINE:
            FreeRTOS_printf( ( "Descriptor Definition Error on DMA Channel %d\n",
                               ucErrChnlNum ) );
            break;

        case XGMAC_ERR_UNHANDLED:
            FreeRTOS_printf( ( "Unhandled Error on DMA Channel %d\n", ucErrChnlNum ) );
            break;

        default:
            ucRemainInErrState = pdTRUE;
            break;
    }

    configASSERT( ucRemainInErrState == pdFALSE );
}
/*-----------------------------------------------------------*/

BaseType_t prvUpdateRxDMADescriptors( uint8_t * pucRxBuffer,
                                      NetworkInterface_t * pxInterface )
{
uint8_t * pucBufAddr = NULL;
int32_t xStatus;

    ( void ) ( pucRxBuffer );
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
xgmac_handle_t pXGMACHandle = ( xgmac_handle_t ) xEmacConfig[ instance ].hxgmac;

    for( uint16_t usIndex = 0U; usIndex < ( uint32_t ) XGMAC_NUM_RX_DESC; usIndex++ )
    {
        #if ( ipconfigZERO_COPY_RX_DRIVER != 0 )
            TickType_t uxBlockTimeTicks = pdMS_TO_TICKS( 100UL );
            NetworkBufferDescriptor_t * pxBufferDescriptor;

            pxBufferDescriptor = pxGetNetworkBufferWithDescriptor(
                XGMAC_MAX_PACKET_SIZE, uxBlockTimeTicks );

            if( pxBufferDescriptor != NULL )
            {
                pucBufAddr = pxBufferDescriptor->pucEthernetBuffer;
            }
            else
            {
                return pdFAIL;
            }
        #else  /* if ( ipconfigZERO_COPY_RX_DRIVER != 0 ) */
            /* Get address of Rx Buffer to assign to Descriptor */
            pucBufAddr = pucGetRXBuffer( ( RxBufferPool_t * ) ( uintptr_t ) pucRxBuffer,
                                         XGMAC_MAX_PACKET_SIZE );

            if( pucBufAddr == NULL )
            {
                return pdFAIL;
            }
        #endif /* if ( ipconfigZERO_COPY_RX_DRIVER != 0 ) */
        xStatus = xgmac_refill_rx_descriptor( pXGMACHandle, pucBufAddr );

        if( xStatus != 0 )
        {
            return pdFAIL;
        }
    }

    return pdPASS;
}
/*-----------------------------------------------------------*/

BaseType_t prvNetworkInterfaceOutDone( NetworkInterface_t * pxInterface )
{
uint8_t * pucReleaseBuffer = NULL;
int32_t xStatus;
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
xgmac_handle_t pXGMACHandle = ( xgmac_handle_t ) xEmacConfig[ instance ].hxgmac;

    while( xSemaphoreTake( xSemaphoreCounterTx, 0U ) == pdTRUE )
    {
        xStatus = xgmac_dma_tx_done( pXGMACHandle, &pucReleaseBuffer );

        if( xStatus != 0 )
        {
            break;
        }

        if( pucReleaseBuffer != NULL )
        {
            #if ( ipconfigZERO_COPY_TX_DRIVER != 0 )
            {
            NetworkBufferDescriptor_t * pxBuffer;
            void * pvBuffer;

                pvBuffer = ( void * ) pucReleaseBuffer;

                if( pvBuffer != NULL )
                {
                    pxBuffer = pxPacketBuffer_to_NetworkBuffer( pvBuffer );

                    if( pxBuffer != NULL )
                    {
                        vReleaseNetworkBufferAndDescriptor( pxBuffer );
                    }
                    else
                    {
                        FreeRTOS_printf( ( "Tx Done Get Buff: Can not find network buffer\n" ) );
                    }
                }
            }
            #else  /* if ( ipconfigZERO_COPY_TX_DRIVER != 0 ) */
                BaseType_t xReturn;
                xReturn = pucReleaseTXBuffer( pxTxBufferPool, pucReleaseBuffer );

                if( xReturn != pdPASS )
                {
                    FreeRTOS_printf( ( "Tx Done Get Buff: Can not release pool buffer  \n" ) );
                }
            #endif /* ipconfigZERO_COPY_TX_DRIVER */
        }
    }

    return pdPASS;
}
/*-----------------------------------------------------------*/

BaseType_t prvCreateTxBufferPool( TxBufferPool_t * pTxBufferPool )
{
uint8_t * pTxBuf;
BaseType_t xReturn = pdPASS;

    /* Allocate Transmit Buffer Pool */
    pTxBuf = ( uint8_t * ) pvPortMalloc( ( size_t ) TX_BUFFER_COUNT * ( size_t ) TX_BUFFER_SIZE );

    /* Return failure if unable to allocate buffer */
    if( pTxBuf == NULL )
    {
        xReturn = pdFAIL;
        return xReturn;
    }

    /* Assign Tx Buffer Addresses to Tx Pool Structure  */
    for( uint32_t i = 0U; i < TX_BUFFER_COUNT; i++ )
    {
        pTxBufferPool->pTxBuffer[ i ] = ( uint8_t * ) &( pTxBuf[ i * TX_BUFFER_SIZE ] );
    }

    /* Assign Head and Tail to start of the location */
    pTxBufferPool->usHeadIndex = 0;
    pTxBufferPool->usTailIndex = 0;

    /* Initial available buffer count */
    pTxBufferPool->usBufUsedCnt = 0;

    pTxBufferPool->ucIsInitiazed = 1;

    xTxBufSynchSem = xSemaphoreCreateMutex();

    if( xTxBufSynchSem == NULL )
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t prvCreateRxBufferPool( RxBufferPool_t * pRxBufferPool )
{
uint8_t * pRxBuf;
BaseType_t xReturn = pdPASS;

    /* Allocate Receive Buffer Pool */
    pRxBuf = ( uint8_t * ) pvPortAlignedAlloc(64, ( size_t ) RX_BUFFER_COUNT * ( size_t ) RX_BUFFER_SIZE );

    /* Return failure if unable to allocate buffer */
    if( pRxBuf == NULL )
    {
        xReturn = pdFAIL;
        return xReturn;
    }

    /* Assign Rx Buffer Addresses to Rx Pool Structure  */
    for( uint32_t i = 0U; i < RX_BUFFER_COUNT; i++ )
    {
        pRxBufferPool->pRxBuffer[ i ] = ( uint8_t * ) &( pRxBuf[ i * RX_BUFFER_SIZE ] );
    }

    /* Assign Head and Tail to start of the location */
    pRxBufferPool->usHeadIndex = 0;
    pRxBufferPool->usTailIndex = 0;

    /* Initial available buffer count */
    pRxBufferPool->usBufUsedCnt = 0;

    pRxBufferPool->ucIsInitiazed = 1;

    xRxBufSynchSem = xSemaphoreCreateMutex();

    if( xRxBufSynchSem == NULL )
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

uint8_t * pucGetTXBuffer( TxBufferPool_t * pTxBufferPool,
                          size_t xWantedSize )
{
TickType_t xBlockTimeTicks = pdMS_TO_TICKS( 1000U );
uint8_t * pucBufAddr = NULL;
uint16_t usIndex;

    if( xWantedSize > TX_BUFFER_SIZE )
    {
        FreeRTOS_printf( ( "Tx Buffer Size more than the Poll Buffer size  \n" ) );
        return NULL;
    }

    if( xSemaphoreTake( xTxBufSynchSem, xBlockTimeTicks ) != pdFAIL )
    {
        if( pTxBufferPool->usBufUsedCnt == ( uint16_t ) TX_BUFFER_COUNT )
        {
            FreeRTOS_printf( ( "Tx Buffer Pool Fully Used \n" ) );
            return NULL;
        }

        /*Get the buffer from the head of the buffer pool */
        usIndex = pTxBufferPool->usHeadIndex;
        pTxBufferPool->usHeadIndex++;
        pucBufAddr = pTxBufferPool->pTxBuffer[ usIndex ];

        /* Update Head and Buffer Count in use */
        pTxBufferPool->usHeadIndex %= ( uint16_t ) TX_BUFFER_COUNT;
        pTxBufferPool->usBufUsedCnt++;

        /* Release the Mutex. */
        ( void ) xSemaphoreGive( xTxBufSynchSem );
    }

    return pucBufAddr;
}
/*-----------------------------------------------------------*/

uint8_t * pucGetRXBuffer( RxBufferPool_t * pRxBufferPool,
                          size_t xWantedSize )
{
TickType_t xBlockTimeTicks = pdMS_TO_TICKS( 1000U );
uint8_t * pucBufAddr = NULL;
uint16_t usIndex;

    if( xWantedSize > RX_BUFFER_SIZE )
    {
        FreeRTOS_printf( ( "Rx Buffer Size more than the Pool Buffer size  \n" ) );
        return NULL;
    }

    if( xSemaphoreTake( xRxBufSynchSem, xBlockTimeTicks ) != pdFAIL )
    {
        if( pRxBufferPool->usBufUsedCnt == ( uint16_t ) RX_BUFFER_COUNT )
        {
            FreeRTOS_printf( ( "Rx Buffer Pool Fully Used \n" ) );
            return NULL;
        }

        /*Get the buffer from the head of the buffer pool */
        usIndex = pRxBufferPool->usHeadIndex;
        pRxBufferPool->usHeadIndex++;
        pucBufAddr = pRxBufferPool->pRxBuffer[ usIndex ];

        /* Update Head and Buffer Count in use */
        pRxBufferPool->usHeadIndex %= ( uint16_t ) RX_BUFFER_COUNT;
        pRxBufferPool->usBufUsedCnt++;

        /* Release the Mutex. */
        ( void ) xSemaphoreGive( xRxBufSynchSem );
    }

    return pucBufAddr;
}
/*-----------------------------------------------------------*/

BaseType_t pucReleaseTXBuffer( TxBufferPool_t * pTxBufferPool,
                               void * pvBuffer )
{
TickType_t xBlockTimeTicks = pdMS_TO_TICKS( 1000U );
BaseType_t xReturn = pdFAIL;
uint16_t usIndex;

    if( xSemaphoreTake( xTxBufSynchSem, xBlockTimeTicks ) != pdFAIL )
    {
        if( pTxBufferPool->usBufUsedCnt == 0U )
        {
            FreeRTOS_printf( ( "Tx Buffer Pool is Already Empty \n" ) );
            return xReturn;
        }

        /* Add the buffer to the tail of the pool */
        ( void ) memset( pvBuffer, '\0', TX_BUFFER_SIZE );
        usIndex = pTxBufferPool->usTailIndex;
        pTxBufferPool->usTailIndex++;
        pTxBufferPool->pTxBuffer[ usIndex ] = ( uint8_t * ) pvBuffer;

        /* Update tail and free count */
        pTxBufferPool->usTailIndex %= ( uint16_t ) TX_BUFFER_COUNT;
        pTxBufferPool->usBufUsedCnt--;

        /* Release the Mutex. */
        ( void ) xSemaphoreGive( xTxBufSynchSem );

        xReturn = pdPASS;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

NetworkInterface_t * pxFillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                NetworkInterface_t * pxInterface )
{
    xEmacConfig[ xEMACIndex ].instance = ( int32_t ) ( xEMACIndex );
    xEmacConfig[ xEMACIndex ].phy_type = XGMAC_PHY_TYPE_RGMII;

    ( void ) memset( pxInterface, 0, sizeof( NetworkInterface_t ) );
    pxInterface->pcName = "socfpga";
    pxInterface->pvArgument = (void*) xEMACIndex;
    pxInterface->pfInitialise = xNetworkInterfaceInitialise;
    pxInterface->pfOutput = xNetworkInterfaceOutput;
    pxInterface->pfGetPhyLinkStatus = GetPhyLinkStatus;

    ( void ) FreeRTOS_AddNetworkInterface( pxInterface );

    return pxInterface;
}
/*-----------------------------------------------------------*/

void prvEMACIRQHanlderCallback( xgmac_int_status_t xIntrStatus,
                                void * pvIrqData )
{
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
uint32_t ulISREvent = 0U;
xgmac_err_info_t * pxIntData = ( xgmac_err_info_t * ) pvIrqData;

    if( xEMACTaskHandle != NULL )
    {
        if( xIntrStatus == XGMAC_RX_EVENT )
        {
            /* Notify prvEMACHandlerTask of Receive event */
            ulISREvent = XGMAC_IF_RX_EVENT;
        }
        else if( xIntrStatus == XGMAC_TX_DONE_EVENT )
        {
            /* Notify prvEMACHandlerTask of Transmit completion event */
            ulISREvent = XGMAC_IF_TX_EVENT;
            ( void ) xSemaphoreGiveFromISR( xSemaphoreCounterTx, NULL );
        }
/*-----------------------------------------------------------*/

        else if( xIntrStatus == XGMAC_ERR_EVENT )
        {
            /* Notify prvEMACHandlerTask of Error event with Error Type and Error Data */
            ulISREvent |= ( uint32_t ) pxIntData->err_type << 24;
            ulISREvent |= ( uint32_t ) pxIntData->err_ch << 16;
            ulISREvent |= XGMAC_IF_ERR_EVENT;
        }
/*-----------------------------------------------------------*/

        else
        {
            /*do nothing*/
        }

        ( void ) xTaskNotifyFromISR( xEMACTaskHandle, ulISREvent, eSetBits,
                                     &( xHigherPriorityTaskWoken ) );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}
/*-----------------------------------------------------------*/

void prvEMACHandlerTask( void * pvParameters )
{
NetworkInterface_t * pxInterface = ( NetworkInterface_t * ) pvParameters;
TimeOut_t xPhyTime;
int instance = ( int ) ( ( uintptr_t ) pxInterface->pvArgument );
TickType_t xPhyRemTime;
uint64_t xStatus;
const TickType_t ulMaxBlockTime = pdMS_TO_TICKS( 100UL );
uint32_t ulISREvents = 0U;
xgmac_handle_t pXGMACHandle = ( xgmac_handle_t ) xEmacConfig[ instance ].hxgmac;

    /* Remove compiler warnings about unused parameters. */

    vTaskSetTimeOutState(&xPhyTime);
    xPhyRemTime = pdMS_TO_TICKS(PHY_LS_LOW_CHECK_TIME_MS);

    for( ; ; )
    {
        #ifdef ENABLE_PRINTF
        #if ( ipconfigHAS_PRINTF != 0 )
            {
                /* Call a function that monitors resources: the amount of free network
                 * buffers and the amount of free space on the heap.  See FreeRTOS_IP.c
                 * for more detailed comments. */
                vPrintResourceStats();
            }
            #endif /* ( ipconfigHAS_PRINTF != 0 ) */
        #endif

        /* Wait for a new event or a time-out. */
        ( void ) xTaskNotifyWait( 0U,                 /* ulBitsToClearOnEntry */
                                  XGMAC_IF_ALL_EVENT, /* ulBitsToClearOnExit */
                                  &( ulISREvents ),   /* pulNotificationValue */
                                  ulMaxBlockTime );

        if( ( ulISREvents & XGMAC_IF_RX_EVENT ) != 0U )
        {
            ( void ) prvNetworkInterfaceInput( pxInterface );
        }

        if( ( ulISREvents & XGMAC_IF_TX_EVENT ) != 0U )
        {
            ( void ) prvNetworkInterfaceOutDone( pxInterface );
        }

        if( ( ulISREvents & XGMAC_IF_ERR_EVENT ) != 0U )
        {
        uint8_t ucErrType;
        uint8_t ucErrDMAChnlNum;

            /* Notify prvEMACHandlerTask of Error event with Error Type and Error Data */
            ucErrType = ( uint8_t ) ( ulISREvents >> 24 );
            ucErrDMAChnlNum = ( uint8_t ) ( ( ulISREvents >> 16 ) & 0xFFFFU );
            prvHandleErrorEvents( ucErrType, ucErrDMAChnlNum, pxInterface );
        }

        if( xTaskCheckForTimeOut( &xPhyTime, &xPhyRemTime ) != pdFALSE )
        {
            xStatus = xReadPhyStatus( pxInterface );

            /* PHY link status has been changed need to update XGMAC Configurations */
            if( xPHYLinkStatus != xStatus )
            {
                /* PHY link is up since last check, reconfigure parameters */
                if( xStatus == pdTRUE )
                {
                    if( xgmac_update_xgmac_speed_mode( pXGMACHandle, &xPhyDev ) != 0 )
                    {
                        FreeRTOS_printf( ( "SOCFPGA_XGMAC: Updating Configurations failed....\n" ) );
                    }
                }
                xStatus = xReadPhyStatus( pxInterface );

                if( xPHYLinkStatus != xStatus )
                {
                    xPHYLinkStatus = xStatus;
                    FreeRTOS_printf( ( "prvEMACHandlerTask: PHY LS now %d\n",
                                        xPHYLinkStatus != 0uL ) );
                }
            }

            vTaskSetTimeOutState(&xPhyTime);

            if( xPHYLinkStatus != pdFALSE )
            {
                xPhyRemTime = pdMS_TO_TICKS(PHY_LS_HIGH_CHECK_TIME_MS);
            }
            else
            {
                xPhyRemTime = pdMS_TO_TICKS(PHY_LS_LOW_CHECK_TIME_MS);
            }
        }
        else
        {
            /*do nothing*/
        }
    }
}
/*-----------------------------------------------------------*/
