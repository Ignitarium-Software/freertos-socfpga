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

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
* Port specific definitions.
*
* The settings in this file configure FreeRTOS correctly for the given hardware
* and compiler.
*
* These settings should not be altered.
*-----------------------------------------------------------
*/

/* Type definitions. */
#define portCHAR          char
#define portFLOAT         float
#define portDOUBLE        double
#define portLONG          long
#define portSHORT         short
#define portSTACK_TYPE    size_t
#define portBASE_TYPE     long

typedef portSTACK_TYPE   StackType_t;
typedef portBASE_TYPE    BaseType_t;
typedef uint64_t         UBaseType_t;

typedef uint64_t         TickType_t;
#define portMAX_DELAY              ( ( TickType_t ) 0xffffffffffffffff )

/* 32-bit tick type on a 32-bit architecture, so reads of the tick count do
not need to be guarded with a critical section. */
#define portTICK_TYPE_IS_ATOMIC    ( 1 )

/*-----------------------------------------------------------*/

/* Hardware specifics. */
#define portSTACK_GROWTH         ( -1 )
#define portTICK_PERIOD_MS       ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT       ( 16 )
#define portPOINTER_SIZE_TYPE    uint64_t

/*-----------------------------------------------------------*/

/* Task utilities. */

/* Called at the end of an ISR that can cause a context switch. */
#define portEND_SWITCHING_ISR( xSwitchRequired )            \
{                                                           \
extern uint64_t ullPortYieldRequired[];                     \
                                                            \
    if( ( xSwitchRequired ) != pdFALSE )                    \
    {                                                       \
        ullPortYieldRequired[ ulGetCoreId() ] = pdTRUE;     \
    }                                                       \
}

#define portYIELD_FROM_ISR( x )    portEND_SWITCHING_ISR( x )
#define portYIELD()                                            \
        __asm volatile ( "SVC 0" ::: "memory" );               \

#define portYIELD_CORE( x )        vYieldCore( x )
/*-----------------------------------------------------------
* Critical section control
*----------------------------------------------------------*/

extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );
extern UBaseType_t uxPortSetInterruptMask( void );
extern void vPortClearInterruptMask( UBaseType_t uxNewMaskValue );
extern void vPortInstallFreeRTOSVectorTable( void );
uint32_t ulRawReadICC_PMR_EL1( void );
void ulRawWriteICC_PMR_EL1( uint32_t pmr );

#define portUNMASK_VALUE                 ( 0xFFUL )

#define portCHECK_DAIF()                            \
    do {                                            \
    uint32_t daif;                                  \
    __asm volatile("MRS %0, DAIF" : "=r"(daif)::);  \
    daif;                                           \
    } while(0)

#define portICCPMR_PRIORITY_MASK \
    ((uint32_t)(configMAX_SYSCALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT))

#define portCPU_IRQ_DISABLE()                       \
__asm volatile ( "MSR DAIFSET, #2" ::: "memory" );  \
__asm volatile ( "DSB SY" );                        \
__asm volatile ( "ISB SY" );

#define portCPU_IRQ_ENABLE()                            \
    __asm volatile ( "MSR DAIFCLR, #2" ::: "memory" );  \
    __asm volatile ( "DSB SY" );                        \
    __asm volatile ( "ISB SY" );

#define portDISABLE_INTERRUPTS()                        \
    do {                                                \
        ulRawWriteICC_PMR_EL1( ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) );\
        __asm volatile ("DSB SY" ::: "memory");         \
        __asm volatile ("ISB SY" ::: "memory");         \
    } while(0)

#define portENABLE_INTERRUPTS()                         \
    do {                                                \
        __asm volatile ( "msr ICC_PMR_EL1, %0\n" : : "r" ( portUNMASK_VALUE ) : "memory" );\
        __asm volatile ("DSB SY" ::: "memory");         \
        __asm volatile ("ISB SY" ::: "memory");         \
    } while(0)

#define portSET_INTERRUPT_MASK()                        \
    ({                                                  \
        uint32_t __pmr;                                 \
        __pmr = ulRawReadICC_PMR_EL1();                 \
        if (__pmr != (configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT))          \
        {                                               \
            ulRawWriteICC_PMR_EL1( ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) );\
            __asm volatile ("DSB SY" ::: "memory");     \
            __asm volatile ("ISB SY" ::: "memory");     \
        }                                               \
        __pmr;                                          \
    })

#define portCLEAR_INTERRUPT_MASK(pmr)                   \
    do {                                                \
        ulRawWriteICC_PMR_EL1( ( uint32_t ) ( pmr));    \
        __asm volatile ("DSB SY" ::: "memory");         \
        __asm volatile ("ISB SY" ::: "memory");         \
    } while(0)

#define portSET_INTERRUPT_MASK_FROM_ISR()               \
    ({                                                  \
        uint32_t __pmr;                                 \
        __pmr = ulRawReadICC_PMR_EL1();                 \
        if (__pmr != (configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT))          \
        {                                               \
            ulRawWriteICC_PMR_EL1( ( uint32_t ) ( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT ) );\
            __asm volatile ("DSB SY" ::: "memory");     \
            __asm volatile ("ISB SY" ::: "memory");     \
        }                                               \
        __pmr;                                          \
    })

#define portCLEAR_INTERRUPT_MASK_FROM_ISR(pmr)          \
    do {                                                \
        ulRawWriteICC_PMR_EL1( ( uint32_t ) ( pmr));    \
        __asm volatile ("DSB SY" ::: "memory");         \
        __asm volatile ("ISB SY" ::: "memory");         \
    } while(0)
/* These macros do not globally disable/enable interrupts.  They do mask off
interrupts that have a priority below configMAX_API_CALL_INTERRUPT_PRIORITY. */
#if configNUMBER_OF_CORES == 1
    #define portENTER_CRITICAL()                      vPortEnterCritical();
    #define portEXIT_CRITICAL()                       vPortExitCritical();
#else
    #define portGET_CORE_ID()                         ulGetCoreId()
    extern volatile uint64_t ullCriticalNesting[ configNUMBER_OF_CORES ];
    extern uint64_t ullPortInterruptNesting[ configNUMBER_OF_CORES ];
    #define portGET_CRITICAL_NESTING_COUNT()          ( ullCriticalNesting[ portGET_CORE_ID() ] )
    #define portSET_CRITICAL_NESTING_COUNT( x )       ( ullCriticalNesting[ portGET_CORE_ID() ] = ( x ) )
    #define portINCREMENT_CRITICAL_NESTING_COUNT()    ( ullCriticalNesting[ portGET_CORE_ID() ]++ )
    #define portDECREMENT_CRITICAL_NESTING_COUNT()    ( ullCriticalNesting[ portGET_CORE_ID() ]-- )
    #define portENTER_CRITICAL()                      vTaskEnterCritical()
    #define portEXIT_CRITICAL()                       vTaskExitCritical()
    #define portASSERT_IF_IN_ISR()                    configASSERT( xPortIsInsideInterrupt() == pdFALSE )
#endif
#define portENTER_CRITICAL_FROM_ISR()             vTaskEnterCriticalFromISR()
#define portEXIT_CRITICAL_FROM_ISR( x )           vTaskExitCriticalFromISR( x )
/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
not required for this port but included in case common demo code that uses these
macros is used. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )

/* Prototype of the FreeRTOS tick handler.  This must be installed as the
handler for whichever peripheral is used to generate the RTOS tick. */
void FreeRTOS_Tick_Handler( void );

/* Any task that uses the floating point unit MUST call vPortTaskUsesFPU()
before any floating point instructions are executed. */
void vPortTaskUsesFPU( void );
#define portTASK_USES_FLOATING_POINT()    vPortTaskUsesFPU()

#define portLOWEST_INTERRUPT_PRIORITY           ( ( ( uint32_t ) configUNIQUE_INTERRUPT_PRIORITIES ) - 1UL )
#define portLOWEST_USABLE_INTERRUPT_PRIORITY    ( portLOWEST_INTERRUPT_PRIORITY - 1UL )

/* Architecture specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1

    /* Store/clear the ready priorities in a bit map. */
    #define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities )    ( ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) ) )
    #define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities )     ( ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) ) )

    /*-----------------------------------------------------------*/

    #define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities )    ( ( uxTopPriority ) = ( 31UL - ( uint32_t ) __builtin_clz( ( uxReadyPriorities ) ) ) )

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

#ifdef configASSERT
    void vPortValidateInterruptPriority( void );
    #define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()    vPortValidateInterruptPriority()
#endif /* configASSERT */

#define portNOP()                                         __asm volatile ( "NOP" )
#define portINLINE    __inline

#ifdef __cplusplus
} /* extern C */
#endif

/* The number of bits to shift for an interrupt priority is dependent on the
number of bits implemented by the interrupt controller. */
#if configUNIQUE_INTERRUPT_PRIORITIES == 16
    #define portPRIORITY_SHIFT            4
    #define portMAX_BINARY_POINT_VALUE    3
#elif configUNIQUE_INTERRUPT_PRIORITIES == 32
    #define portPRIORITY_SHIFT            3
    #define portMAX_BINARY_POINT_VALUE    2
#elif configUNIQUE_INTERRUPT_PRIORITIES == 64
    #define portPRIORITY_SHIFT            2
    #define portMAX_BINARY_POINT_VALUE    1
#elif configUNIQUE_INTERRUPT_PRIORITIES == 128
    #define portPRIORITY_SHIFT            1
    #define portMAX_BINARY_POINT_VALUE    0
#elif configUNIQUE_INTERRUPT_PRIORITIES == 256
    #define portPRIORITY_SHIFT            0
    #define portMAX_BINARY_POINT_VALUE    0
#else /* if configUNIQUE_INTERRUPT_PRIORITIES == 16 */
    #error Invalid configUNIQUE_INTERRUPT_PRIORITIES setting.  configUNIQUE_INTERRUPT_PRIORITIES must be set to the number of unique priorities implemented by the target hardware
#endif /* if configUNIQUE_INTERRUPT_PRIORITIES == 16 */

/* Interrupt controller access addresses. */
#define portICCPMR_PRIORITY_MASK_OFFSET                      ( 0x04 )
#define portICCIAR_INTERRUPT_ACKNOWLEDGE_OFFSET              ( 0x0C )
#define portICCEOIR_END_OF_INTERRUPT_OFFSET                  ( 0x10 )
#define portICCBPR_BINARY_POINT_OFFSET                       ( 0x08 )
#define portICCRPR_RUNNING_PRIORITY_OFFSET                   ( 0x14 )

#define portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS       ( configINTERRUPT_CONTROLLER_BASE_ADDRESS + configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET )
#define portICCPMR_PRIORITY_MASK_REGISTER                    ( *( ( volatile uint32_t * ) ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCPMR_PRIORITY_MASK_OFFSET ) ) )
#define portICCIAR_INTERRUPT_ACKNOWLEDGE_REGISTER_ADDRESS    ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCIAR_INTERRUPT_ACKNOWLEDGE_OFFSET )
#define portICCEOIR_END_OF_INTERRUPT_REGISTER_ADDRESS        ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCEOIR_END_OF_INTERRUPT_OFFSET )
#define portICCPMR_PRIORITY_MASK_REGISTER_ADDRESS            ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCPMR_PRIORITY_MASK_OFFSET )
#define portICCBPR_BINARY_POINT_REGISTER                     ( *( ( const volatile uint32_t * ) ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCBPR_BINARY_POINT_OFFSET ) ) )
#define portICCRPR_RUNNING_PRIORITY_REGISTER                 ( *( ( const volatile uint32_t * ) ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCRPR_RUNNING_PRIORITY_OFFSET ) ) )

#define portMEMORY_BARRIER()    __asm volatile ( "" ::: "memory" )

enum {
    PORT_ISR_SPIN_LOCK,
    PORT_TASK_SPIN_LOCK,
    PORT_STDLIB_SPIN_LOCK_MALLOC,
    PORT_STDLIB_SPIN_LOCK_STDIN,
    PORT_STDLIB_SPIN_LOCK_STDOUT,
    PORT_STDLIB_SPIN_LOCK_STDERR,
    PORT_SPIN_LOCK_COUNT
};

typedef struct
{
    uint32_t ulOwnerId;
    uint32_t ulLock;
    uint32_t ulRecurCount;
}SpinLock_t;

void vGetLock(uint32_t ulLockType);
int vReleaseLock(uint32_t ulLockType);
uint32_t ulGetCoreId( void );
void vYieldCore( uint32_t ulCoreId );
#define portGET_ISR_LOCK()        (vGetLock(PORT_ISR_SPIN_LOCK))
#define portRELEASE_ISR_LOCK()    ((void)vReleaseLock(PORT_ISR_SPIN_LOCK))
#define portGET_TASK_LOCK()       (vGetLock(PORT_TASK_SPIN_LOCK))
#define portRELEASE_TASK_LOCK()   ((void)vReleaseLock(PORT_TASK_SPIN_LOCK))

#define portGET_STDLIB_LOCK(x)       (vGetLock(PORT_STDLIB_SPIN_LOCK_MALLOC + x))
#define portRELEASE_STDLIB_LOCK(x)   (vReleaseLock(PORT_STDLIB_SPIN_LOCK_MALLOC + x))
extern void vPortSocfpgaTimerInit( void );
extern void interrupt_irq_handler( unsigned int ulInterruptID );
BaseType_t xPortIsInsideInterrupt( void );
/* SMP */
#endif /* PORTMACRO_H */
