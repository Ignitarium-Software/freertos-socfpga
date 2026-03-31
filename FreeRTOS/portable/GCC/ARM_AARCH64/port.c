/*
 * FreeRTOS Kernel V10.5.1
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright (c) 2025-2026 Altera Corporation.
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
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "socfpga_gic_reg.h"
#include "socfpga_interrupt.h"
#ifndef configINTERRUPT_CONTROLLER_BASE_ADDRESS
    #error configINTERRUPT_CONTROLLER_BASE_ADDRESS must be defined.  See https: /*www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html */
#endif

#ifndef configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET
    #error configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET must be defined.  See https: /*www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html */
#endif

#ifndef configUNIQUE_INTERRUPT_PRIORITIES
    #error configUNIQUE_INTERRUPT_PRIORITIES must be defined.  See https: /*www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html */
#endif

#ifndef configMAX_API_CALL_INTERRUPT_PRIORITY
    #error configMAX_API_CALL_INTERRUPT_PRIORITY must be defined.  See https: /*www.FreeRTOS.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html */
#endif

#if configMAX_API_CALL_INTERRUPT_PRIORITY == 0
    #error configMAX_API_CALL_INTERRUPT_PRIORITY must not be set to 0
#endif

#if configMAX_API_CALL_INTERRUPT_PRIORITY > configUNIQUE_INTERRUPT_PRIORITIES
    #error configMAX_API_CALL_INTERRUPT_PRIORITY must be less than or equal to configUNIQUE_INTERRUPT_PRIORITIES as the lower the numeric priority value the higher the logical interrupt priority
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1
    /* Check the configuration. */
    #if ( configMAX_PRIORITIES > 32 )
        #error configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice.
    #endif
#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/* In case security extensions are implemented. */
#if configMAX_API_CALL_INTERRUPT_PRIORITY <= ( configUNIQUE_INTERRUPT_PRIORITIES / 2 )
    #error configMAX_API_CALL_INTERRUPT_PRIORITY must be greater than ( configUNIQUE_INTERRUPT_PRIORITIES / 2 )
#endif

/* Some vendor specific files default configCLEAR_TICK_INTERRUPT() in
   portmacro.h. */
#ifndef configCLEAR_TICK_INTERRUPT
    #define configCLEAR_TICK_INTERRUPT()
#endif

/* A critical section is exited when the critical section nesting count reaches
   this value. */
#define portNO_CRITICAL_NESTING          ( ( size_t ) 0 )

/* In all GICs 255 can be written to the priority mask register to unmask all
   (but the lowest) interrupt priority. */

/* Tasks are not created with a floating point context, but can be given a
   floating point context after they have been created.  A variable is stored as
   part of the tasks context that holds portNO_FLOATING_POINT_CONTEXT if the task
   does not have an FPU context, or any other value if the task does have an FPU
   context. */
#define portNO_FLOATING_POINT_CONTEXT    ( ( StackType_t ) 0 )

/* Constants required to setup the initial task context. */
#define portSP_ELx                       ( ( StackType_t ) 0x01 )
#define portSP_EL0                       ( ( StackType_t ) 0x00 )

#define portEL1                          ( ( StackType_t ) 0x04 )
#define portINITIAL_PSTATE               ( portEL1 | portSP_EL0 )

/* Used by portASSERT_IF_INTERRUPT_PRIORITY_INVALID() when ensuring the binary
   point is zero. */
#define portBINARY_POINT_BITS            ( ( uint8_t ) 0x03 )

/* Masks all bits in the APSR other than the mode bits. */
#define portAPSR_MODE_BITS_MASK          ( 0x0C )

/* The I bit in the DAIF bits. */
#define portDAIF_I                       ( 0x80 )

/* The space required to hold the FPU registers. There are 32 128 bit registers.
   So total of 512bytes are present. Hence 64 ( 64*8 ) double words. */
#define portFPU_REGISTER_WORDS    ( 64 )

/* Macro to unmask all interrupt priorities. */
#define portCLEAR_INTERRUPTS() vPortClearInterruptMask( pdFALSE )

/* Hardware specifics used when sanity checking the configuration. */
/* #define portINTERRUPT_PRIORITY_REGISTER_OFFSET       0x400UL */
#define portINTERRUPT_PRIORITY_REGISTER_OFFSET    0x4UL
#define portMAX_8_BIT_VALUE                       ( ( uint8_t ) 0xff )
#define portBIT_0_SET                             ( ( uint8_t ) 0x01 )

/* Mask for generating SGI */
/* Only the core values is passed along with the mask as target list */
#define ICC_SGI_TARGET_MASK                       ( 0xFF00FF00FF0000ULL )
/*-----------------------------------------------------------*/
/*
 * Starts the first task executing.  This function is necessarily written in
 * assembly code so is implemented in portASM.s.
 */
extern void vPortRestoreTaskContext( void );

/*-----------------------------------------------------------*/

/* A variable is used to keep track of the critical section nesting.  This
   variable has to be stored as part of the task context and must be initialised to
   a non zero value to ensure interrupts don't inadvertently become unmasked before
   the scheduler starts.  As it is stored as part of the task context it will
   automatically be set to 0 when the first task is started. */
volatile uint64_t ullCriticalNesting[ configNUMBER_OF_CORES ] = {
    [0 ... ( configNUMBER_OF_CORES - 1 )] = 0
};

/* Saved as part of the task context.  If ullPortTaskHasFPUContext is non-zero
   then floating point context must be saved and restored for the task. */
uint64_t ullPortTaskHasFPUContext[ configNUMBER_OF_CORES ] = {
    [0 ... ( configNUMBER_OF_CORES - 1 )] = 0
};

/* Set to 1 to pend a context switch from an ISR. */
uint64_t ullPortYieldRequired[ configNUMBER_OF_CORES ] = {
    [0 ... ( configNUMBER_OF_CORES - 1 )] = pdFALSE
};

/* Counts the interrupt nesting depth.  A context switch is only performed if
   if the nesting depth is 0. */
uint64_t ullPortInterruptNesting[ configNUMBER_OF_CORES ] = {
    [0 ... ( configNUMBER_OF_CORES - 1 )] = 0
};

SpinLock_t xSpinLocks[PORT_SPIN_LOCK_COUNT] =
{
    [0 ... ( PORT_SPIN_LOCK_COUNT - 1 )] = { .ulOwnerId = 0xFFFFFFFF }
};

static BaseType_t xSchedulerStarted[ configNUMBER_OF_CORES ] = { pdFALSE };
/*
 * Variable to store the boot core ID. Since no core can have ID 4, it shows
 * the boot core has not been selected yet.
 */
/* Used in the ASM code. */
__attribute__( ( used ) ) const uint64_t ullICCEOIR = portICCEOIR_END_OF_INTERRUPT_REGISTER_ADDRESS;
__attribute__( ( used ) ) const uint64_t ullICCIAR = portICCIAR_INTERRUPT_ACKNOWLEDGE_REGISTER_ADDRESS;
__attribute__( ( used ) ) const uint64_t ullICCPMR = portICCPMR_PRIORITY_MASK_REGISTER_ADDRESS;
__attribute__( ( used ) ) const uint64_t ullMaxAPIPriorityMask = ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT );

/*-----------------------------------------------------------*/

uint32_t ulRawReadICC_RPR_EL1( void )
{
uint32_t rpr = 0;

    __asm__ __volatile__ ( "mrs %0, ICC_RPR_EL1\n\t" : "=r" ( rpr ) :  : "memory" );
    return rpr;
}
/*-----------------------------------------------------------*/

uint32_t ulRawReadICC_BPR1_EL1( void )
{
uint32_t bpr = 0;

    __asm__ __volatile__ ( "mrs %0, ICC_BPR1_EL1\n\t" : "=r" ( bpr ) :  : "memory" );
    return bpr;
}
/*-----------------------------------------------------------*/

void ulRawWriteICC_PMR_EL1( uint32_t pmr )
{
    __asm__ __volatile__ ( "msr ICC_PMR_EL1, %0\n\t" : : "r" ( pmr ) : "memory" );
}
/*-----------------------------------------------------------*/

uint32_t ulRawReadICC_PMR_EL1( void )
{
uint32_t pmr = 0;

    __asm__ __volatile__ ( "mrs %0, ICC_PMR_EL1\n\t" : "=r" ( pmr ) :  : "memory" );
    return pmr;
}
/*-----------------------------------------------------------*/

uint32_t ulRawReadSPsel( void )
{
uint32_t ulCurMode = 0;

    __asm__ __volatile__ ( "mrs %0, SPSel\n\t" : "=r" ( ulCurMode ) : : "memory" );
    ulCurMode &= 0x01;

    return ulCurMode;
}
/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    configASSERT( pxTopOfStack != NULL );
    configASSERT( pxCode != NULL );

    /* Setup the initial stack of the task.  The stack is set exactly as
       expected by the portRESTORE_CONTEXT() macro. */

    /* First all the general purpose registers. */
    pxTopOfStack--;
    *pxTopOfStack = 0x0101010101010101ULL;        /* R1 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pvParameters; /* R0 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0303030303030303ULL;        /* R3 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0202020202020202ULL;        /* R2 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0505050505050505ULL;        /* R5 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0404040404040404ULL;        /* R4 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0707070707070707ULL;        /* R7 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0606060606060606ULL;        /* R6 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0909090909090909ULL;        /* R9 */
    pxTopOfStack--;
    *pxTopOfStack = 0x0808080808080808ULL;        /* R8 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1111111111111111ULL;        /* R11 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1010101010101010ULL;        /* R10 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1313131313131313ULL;        /* R13 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1212121212121212ULL;        /* R12 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1515151515151515ULL;        /* R15 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1414141414141414ULL;        /* R14 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1717171717171717ULL;        /* R17 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1616161616161616ULL;        /* R16 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1919191919191919ULL;        /* R19 */
    pxTopOfStack--;
    *pxTopOfStack = 0x1818181818181818ULL;        /* R18 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2121212121212121ULL;        /* R21 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2020202020202020ULL;        /* R20 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2323232323232323ULL;        /* R23 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2222222222222222ULL;        /* R22 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2525252525252525ULL;        /* R25 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2424242424242424ULL;        /* R24 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2727272727272727ULL;        /* R27 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2626262626262626ULL;        /* R26 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2929292929292929ULL;        /* R29 */
    pxTopOfStack--;
    *pxTopOfStack = 0x2828282828282828ULL;        /* R28 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x00;         /* XZR - has no effect, used so there are an even number of registers. */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x00;         /* R30 - procedure call link register. */
    pxTopOfStack--;

    *pxTopOfStack = portINITIAL_PSTATE;
    pxTopOfStack--;

    *pxTopOfStack = ( StackType_t ) pxCode; /* Exception return address. */

    #if( configUSE_TASK_FPU_SUPPORT == 1 )
    {
        pxTopOfStack -= portFPU_REGISTER_WORDS;
        memset( pxTopOfStack, 0x00, portFPU_REGISTER_WORDS * sizeof( StackType_t ) );

        /* The task will start with a critical nesting count of 0 as interrupts are
           enabled. */
        pxTopOfStack--;
        *pxTopOfStack = portNO_CRITICAL_NESTING;

        pxTopOfStack--;
        *pxTopOfStack = pdTRUE;
        ullPortTaskHasFPUContext[ ulGetCoreId() ] = pdTRUE;
    }
    #else
        /* The task will start with a critical nesting count of 0 as interrupts are
           enabled. */
        pxTopOfStack--;
        *pxTopOfStack = portNO_CRITICAL_NESTING;

        /* The task will start without a floating point context.  A task that uses
          the floating point hardware must call vPortTaskUsesFPU() before executing
          any floating point instructions. */
        pxTopOfStack--;
        *pxTopOfStack = portNO_FLOATING_POINT_CONTEXT;
    #endif

    return pxTopOfStack;
}
/*-----------------------------------------------------------*/

void vCoreYield( void *pvParam);

BaseType_t xPortStartSchedulerOnCore( void )
{
    /* Interrupts are turned off in the CPU itself to ensure a tick does
       not execute while the scheduler is being started.  Interrupts are
       automatically turned back on in the CPU when the first task starts
       executing. */
    portDISABLE_INTERRUPTS();

    configASSERT( interrupt_register_isr(YIELD_CORE_INTR, vCoreYield,
            NULL ) == 0 );

    configASSERT( interrupt_enable( YIELD_CORE_INTR,
            ( configMAX_API_CALL_INTERRUPT_PRIORITY + 1 )) == 0 );

    if( ulGetCoreId() == 0 )
    {
        /* Start the timer that generates the tick ISR. */
        #ifdef configSETUP_TICK_INTERRUPT
            configSETUP_TICK_INTERRUPT();
        #else
            vPortSocfpgaTimerInit();
        #endif
    }

    xSchedulerStarted[ ulGetCoreId() ] = pdTRUE;
    /* Start the first task executing. */
    vPortRestoreTaskContext();

    /* Should never reach here */
    return 0;
}
/*-----------------------------------------------------------*/

void __attribute__((used)) vCoreEntry(void)
{
    /* Initialize the redistributor and register core to core interrupt */
    /* GIC is initialized before the scheduler starts */
    interrupt_enable_core_rdis();

    /* Start the scheduler on the core */
    xPortStartSchedulerOnCore();
}

static void vCoreInit( uint32_t ulCoreId, void *pvEntryPoint )
{
    configASSERT( pvEntryPoint != NULL );

    __asm__ volatile (
        "LDR    X0,=0xC4000003\n"
        "MOV    X1, %0\n"
        "MOV    X2, %1\n"
        "MOV    X3, XZR\n"
        "SMC    #0\n"
        :
        : "r"(ulCoreId), "r"(pvEntryPoint)
        : "x0","x1","x2","x3","memory"
    );
}

extern void _secondary_boot(void);
BaseType_t xPortStartScheduler( void )
{
uint32_t ulAPSR = 0, ulCoreId = 0;

    #if ( configASSERT_DEFINED == 1 )
    {
        /*Max value possible for priority*/
        const uint8_t ucMaxPriorityValue = 15;
        configASSERT( ucMaxPriorityValue >= portLOWEST_INTERRUPT_PRIORITY );
    }
    #endif /* configASSERT_DEFINED */

    /* Disable interrupts to prevent interrupts before scheduler starts */
    portDISABLE_INTERRUPTS();

    xSpinLocks[0].ulOwnerId = 0xFFFFFFFF;
    xSpinLocks[1].ulOwnerId = 0xFFFFFFFF;
    __asm volatile ( "MRS %0, CurrentEL" : "=r" ( ulAPSR ) );
    ulAPSR &= portAPSR_MODE_BITS_MASK;

    configASSERT( ulAPSR == portEL1 );

    /* Only continue if the binary point value is set to its lowest possible
       setting.  See the comments in vPortValidateInterruptPriority() below for
       more information. */
    configASSERT( ( ulRawReadICC_BPR1_EL1() & portBINARY_POINT_BITS ) <= portMAX_BINARY_POINT_VALUE );

    /* Initialize the secondary cores */
    for( int i = 1; i < ( configNUMBER_OF_CORES ); i++ )
    {
        /* Core ids are in multiples of 256, we start initialising cores
           that are after the boot core. If the boot core is 2 and the number
           of cores is 2, cores will start the scheduler in 2->3 order.
         */
        vCoreInit( (( i + configBOOT_CORE ) % configMAX_NUM_CORES) * 256, _secondary_boot );
    }
    xPortStartSchedulerOnCore();

    /* Should never reach here */
    return 0;
}
/*-----------------------------------------------------------*/

inline uint32_t ulGetCoreId( void )
{
    #if configNUMBER_OF_CORES == 1
        return 0;
    #else
        uint64_t ullCoreAffinity = 0U;

        __asm__ volatile ( "MRS %0, MPIDR_EL1" : "=r" ( ullCoreAffinity ) );
        ullCoreAffinity = ( ( ullCoreAffinity >> 8 ) & 0xFF );
        return ( ( ullCoreAffinity + configBOOT_CORE ) % configMAX_NUM_CORES );
    #endif
}
void vPortEndScheduler( void )
{
    /* Not implemented in ports where there is nothing to return to.
       Artificially force an assert. */
    configASSERT( ullCriticalNesting[ ulGetCoreId() ] == 1000ULL );
}
/*-----------------------------------------------------------*/

#if configNUMBER_OF_CORES == 1
/* Index 0 used to indicate primary core */
void vPortEnterCritical( void )
{
    /* Mask interrupts up to the max syscall interrupt priority. */
    portDISABLE_INTERRUPTS();

    /* Now interrupts are disabled ullCriticalNesting can be accessed
       directly.  Increment ullCriticalNesting to keep a count of how many times
       portENTER_CRITICAL() has been called. */
    ullCriticalNesting[ 0 ]++;

    /* This is not the interrupt safe version of the enter critical function so
       assert() if it is being called from an interrupt context.  Only API
       functions that end in "FromISR" can be used in an interrupt.  Only assert if
       the critical nesting count is 1 to protect against recursive calls if the
       assert function also uses a critical section. */
    if( ullCriticalNesting[ 0 ] == 1ULL )
    {
        configASSERT( ullPortInterruptNesting[0] == 0 );
    }
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
    if( ullCriticalNesting[0] > portNO_CRITICAL_NESTING )
    {
        /* Decrement the nesting count as the critical section is being
           exited. */
        ullCriticalNesting[0]--;

        /* If the nesting level has reached zero then all interrupt
           priorities must be re-enabled. */
        if( ullCriticalNesting[0] == portNO_CRITICAL_NESTING )
        {
            /* Critical nesting has reached zero so all interrupt priorities
               should be unmasked. */
            portENABLE_INTERRUPTS();
        }
    }
}
/*-----------------------------------------------------------*/
#endif

void FreeRTOS_Tick_Handler( void )
{
    /* Must be the lowest possible priority. */
    #if !defined( QEMU )
    {
        configASSERT( ulRawReadICC_RPR_EL1() >= ( uint32_t ) ( portLOWEST_USABLE_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) );
    }
    #endif

    /* Interrupts should not be enabled before this point. */
    #if ( configASSERT_DEFINED == 1 )
    {
    uint32_t ulMaskBits;

    __asm volatile ( "mrs %0, daif" : "=r" ( ulMaskBits )::"memory" );
        configASSERT( ( ulMaskBits & portDAIF_I ) != 0 );
    }
    #endif /* configASSERT_DEFINED */

    /* Set interrupt mask before altering scheduler structures.   The tick
       handler runs at the lowest priority, so interrupts cannot already be masked,
       so there is no need to save and restore the current mask value.  It is
       necessary to turn off interrupts in the CPU itself while the ICCPMR is being
       updated. */
    portDISABLE_INTERRUPTS();

    configCLEAR_TICK_INTERRUPT();

    /* Increment the RTOS tick. */
    #if configNUMBER_OF_CORES > 1
        /* We are still in the ISR for the Tick interrupt */
        /* The single core function already performs check to see
         * if its in ISR */
        BaseType_t xInterruptStatus;
        xInterruptStatus = portENTER_CRITICAL_FROM_ISR();
    #endif
    if( xTaskIncrementTick() != pdFALSE )
    {
        ullPortYieldRequired[ ulGetCoreId() ] = pdTRUE;
    }
    #if configNUMBER_OF_CORES > 1
        portEXIT_CRITICAL_FROM_ISR( xInterruptStatus );
    #endif
    /* Ensure all interrupt priorities are active again. */
    portENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/

void vPortTaskUsesFPU( void )
{
    /* A task is registering the fact that it needs an FPU context.  Set the
       FPU flag (which is saved as part of the task context). */
    ullPortTaskHasFPUContext[ ulGetCoreId() ] = pdTRUE;

    /* Consider initialising the FPSR here - but probably not necessary in
       AArch64. */
}
/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
        /* The following assertion will fail if a service routine (ISR) for
           an interrupt that has been assigned a priority above
           configMAX_SYSCALL_INTERRUPT_PRIORITY calls an ISR safe FreeRTOS API
           function.  ISR safe FreeRTOS API functions must *only* be called
           from interrupts that have been assigned a priority at or below
           configMAX_SYSCALL_INTERRUPT_PRIORITY.

           Numerically low interrupt priority numbers represent logically high
           interrupt priorities, therefore the priority of the interrupt must
           be set to a value equal to or numerically *higher* than
           configMAX_SYSCALL_INTERRUPT_PRIORITY.

           FreeRTOS maintains separate thread and ISR API functions to ensure
           interrupt entry is as fast and simple as possible. */
        configASSERT( ulRawReadICC_RPR_EL1() >= ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) );

        /* Priority grouping:  The interrupt controller (GIC) allows the bits
           that define each interrupt's priority to be split between bits that
           define the interrupt's pre-emption priority bits and bits that define
           the interrupt's sub-priority.  For simplicity all bits must be defined
           to be pre-emption priority bits.  The following assertion will fail if
           this is not the case (if some bits represent a sub-priority).

           The priority grouping is configured by the GIC's binary point register
           (ICCBPR).  Writting 0 to ICCBPR will ensure it is set to its lowest
           possible value (which may be above 0). */
        configASSERT( ( ulRawReadICC_BPR1_EL1() & portBINARY_POINT_BITS ) <= portMAX_BINARY_POINT_VALUE );
    }
/*-----------------------------------------------------------*/


#endif /* configASSERT_DEFINED */
/*-----------------------------------------------------------*/
/* vApplicationIRQHandler() is just a normal C function. */
void vApplicationIRQHandler( uint32_t ulICCIAR )
{
    /* Nesting count should always be 0, else an interrupt pre-empted
       a critical section, which should never happen. */
    configASSERT( ullCriticalNesting[ulGetCoreId()] == 0 );
    interrupt_irq_handler( ulICCIAR );
}
/*-----------------------------------------------------------*/
/* Callback function for core to core interrupt */
/*-----------------------------------------------------------*/

BaseType_t xPortIsInsideInterrupt( void )
{
BaseType_t xReturn = pdFALSE;

    /*Check the stackpointer in use to determine the mode
     * 0 - EL1t
     * 1 - EL1h
     * */
    if( xSchedulerStarted[ ulGetCoreId() ] == pdTRUE )
    {
        if( ulRawReadSPsel() != 0U )
        {
            xReturn = pdTRUE;
        }
        else
        {
            xReturn = pdFALSE;
        }
    }
    return xReturn;
}
/*-----------------------------------------------------------*/

inline void vGetLock( uint32_t ulLockType )
{
    SpinLock_t *pxLock = &xSpinLocks[ulLockType];
    uint32_t ulCoreId = ulGetCoreId();

    if (pxLock->ulOwnerId == ulCoreId)
    {
        pxLock->ulRecurCount++;
        return;
    }

    /* 1. Set w2 with new lock value
       2. Wait for lock to be 0
       3. Write new value for lock
       4. Check if write successful
       5. Repeat from 2 if any step failed */

    __asm__ __volatile__(
    "  sevl                \n"
    "  mov   w2, #1        \n"
    "1:wfe                 \n"
    "  ldaxr w1, [%0]      \n"
    "  cbnz  w1, 1b        \n"
    "  stxr  w3, w2, [%0]  \n"
    "  cbnz  w3, 1b        \n"
    "  dmb   sy            \n"
        :
        : "r" (&pxLock->ulLock)
        : "w1", "w2", "w3", "memory"
    );

    pxLock->ulOwnerId = ulCoreId;
    pxLock->ulRecurCount = 1;
}

inline int vReleaseLock( uint32_t ulLockType )
{
    int ret = 1;
    SpinLock_t *pxLock = &xSpinLocks[ulLockType];
    uint32_t ulCoreId = ulGetCoreId();

    if (pxLock->ulOwnerId != ulCoreId)
    {
        return ret;
    }
    if (--pxLock->ulRecurCount > 0)
    {
        return pxLock->ulLock;
    }

    /*
     * The lock is owned and not a recursive unlock, reset the
     * owner id before releasing the lock
     */
    pxLock->ulOwnerId = 0xFFFFFFFF;

    ret = 0;
    __asm__ __volatile__(
        "dmb sy\n"
        "stlr wzr, [%0]\n"
        :
        : "r" (&pxLock->ulLock)
        : "memory");
    return ret;
}

inline void vYieldCore( uint32_t ulCoreId )
{
    uint64_t ulSgiTarget = 0;
    /*
     * Actual core affinity is in multiples of 256, so core id 1 will
     * actually have 256 as its internal id. FreeRTOS expects core ids
     * to start from zero, the following logic changes the core ID from
     * FreeRTOS mapping to the internally used core ID.
     * eg: Boot core may have internal ID as 2, but FreeRTOS expects it
     * as 0.
     */
    ulCoreId = ( ( ulCoreId + configBOOT_CORE ) % configMAX_NUM_CORES );
    ulSgiTarget = ((ulCoreId * 256) << 8) | ( 1 << 0 ) ;

    /*Send SGI to target core, signalling it to yield */
    __asm__ volatile ("MSR ICC_SGI1R_EL1, %0\n\t" :: "r"(ulSgiTarget) : "memory");
    __asm__ volatile ("ISB SY\n\t");
}

void vCoreYield( void *pvParam)
{
    ( void ) pvParam;
    ullPortYieldRequired[ ulGetCoreId() ] = pdTRUE;
}
