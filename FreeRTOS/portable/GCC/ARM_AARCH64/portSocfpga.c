/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Port layer for FreeRTOS Kernel V10.5.1 for ARM_AARCH64
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

#include <socfpga_interrupt.h>
#include "socfpga_sip_handler.h"

#define SOCFPGA_CNTV_CTL_ENABLE         ( ( 1U ) << 0 )
#define TMR_DELAY_SECS                  ( 1U )
#define PSCI_CPU_ON_FID                 ( 0xC4000003U )
/*-----------------------------------------------------------*/

static uint64_t ullCounterFreq = 0;
static uint64_t ullCounterCurrVal = 0;
static uint64_t ullCounterReloadVal = 0;

/*-----------------------------------------------------------*/
static void vPortSocfpgaSetVirtualTimerControl( uint32_t ulTimerControl )
{
    asm volatile ( "MSR CNTV_CTL_EL0, %0" : : "r" ( ulTimerControl ) );
}
/*-----------------------------------------------------------*/

static void vPortSocfpgaSetVirtualTimerValue( uint32_t ulTimerValue )
{
    asm volatile ( "MSR CNTV_TVAL_EL0, %0" : : "r" ( ulTimerValue ) );
}
/*-----------------------------------------------------------*/

static void vPortSocfpgaSetVirtualTimerCompareValue( void )
{
    /* The value of the counter will be equal to or greater than
     * ullCounterCurrval when this function is called.
     */
    ullCounterCurrVal += ullCounterReloadVal;
    asm volatile ( "MSR CNTV_CVAL_EL0, %0" : : "r" ( ullCounterCurrVal ) );
}
/*-----------------------------------------------------------*/

static uint32_t vPortSocfpgaTGetFrequency( void )
{
uint32_t uFreq = 0;

    asm volatile ( "MRS %0, CNTFRQ_EL0" : "=r" ( uFreq ) );
    return uFreq;
}
/*-----------------------------------------------------------*/

void vPortSocfpgaTimerIRQHandler( void *data )
{
    ( void ) data;

    /* Configure when the next interrupt will be triggered. */
    vPortSocfpgaSetVirtualTimerCompareValue();

    /* Call the Tick handler. */
    FreeRTOS_Tick_Handler();
}
/*-----------------------------------------------------------*/

void vPortSocfpgaTimerInit( void )
{
    /*Register the callback*/
    configASSERT( interrupt_register_isr( EL1VIRT_TMR_INTR, vPortSocfpgaTimerIRQHandler, NULL ) == 0 );

    /* Configure interrupt. */
    configASSERT( interrupt_enable( EL1VIRT_TMR_INTR, interrupt_min_interrupt_priority ) == 0 );

    /* Calculate the timer delay */
    ullCounterFreq = vPortSocfpgaTGetFrequency();
    ullCounterReloadVal = ( TMR_DELAY_SECS * ullCounterFreq ) / configTICK_RATE_HZ;

    /* Get the current value of the timer */
    __asm__ volatile ( "MRS %0, CNTVCT_EL0" : "=r" ( ullCounterCurrVal ) );

    /*Set the intial timer delay and enable timer interrupts */
    vPortSocfpgaSetVirtualTimerCompareValue();
    vPortSocfpgaSetVirtualTimerControl ( SOCFPGA_CNTV_CTL_ENABLE );
}
/*-----------------------------------------------------------*/
