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

#include "FreeRTOSConfig.h"

    .org 0
    .text

    /* Variables and functions. */
    .extern ullMaxAPIPriorityMask
    .extern pxCurrentTCB
    .extern vTaskSwitchContext
    .extern vApplicationIRQHandler
    .extern ullPortInterruptNesting
    .extern ullPortTaskHasFPUContext
    .extern ullCriticalNesting
    .extern ullPortYieldRequired
    .extern ullICCEOIR
    .extern ullICCIAR
    .extern _freertos_vector_table

    .global _freertos_vector_table
    .global FreeRTOS_IRQ_Handler
    .global FreeRTOS_SWI_Handler
    .global vPortRestoreTaskContext
    .global _vector_table
    .global _boot

    .org 0
    .section .vectors, "a"
_vector_table:
    .set    VBAR, _vector_table
    .org VBAR
        b   _boot
    .org (VBAR + 0x80)
        b .
    .org (VBAR + 0x100)
        b .
    .org (VBAR + 0x180)
        b .
    .org (VBAR + 0x200)
        b .
    .org (VBAR + 0x280)
        b  FreeRTOS_IRQ_Handler
    .org (VBAR + 0x300)
        b .
    .org (VBAR + 0x380)
        b .
    .org (VBAR + 0x400)
        b  EnableFPU
    .org (VBAR + 0x480)
        b .
    .org (VBAR + 0x500)
        b .
    .org (VBAR + 0x580)
        b .
    .org (VBAR + 0x600)
        b .
    .org (VBAR + 0x680)
        b .
    .org (VBAR + 0x700)
        b .
    .org (VBAR + 0x780)
        b .

    .set    FREERTOS_VBAR, (VBAR+0x1000)
    .org(FREERTOS_VBAR)
_freertos_vector_table:
        b   FreeRTOS_SWI_Handler
    .org (FREERTOS_VBAR + 0x80)
        b   FreeRTOS_IRQ_Handler
    .org (FREERTOS_VBAR + 0x100)
        b .
    .org (FREERTOS_VBAR + 0x180)
        b .
    .org (FREERTOS_VBAR + 0x200)
        b   FreeRTOS_SWI_Handler
    .org (FREERTOS_VBAR + 0x280)
        b   FreeRTOS_IRQ_Handler
    .org (FREERTOS_VBAR + 0x300)
        b .
    .org (FREERTOS_VBAR + 0x380)
        b .
    .org (FREERTOS_VBAR + 0x400)
        b  EnableFPU
    .org (FREERTOS_VBAR + 0x480)
        b .
    .org (FREERTOS_VBAR + 0x500)
        b .
    .org (FREERTOS_VBAR + 0x580)
        b .
    .org (FREERTOS_VBAR + 0x600)
        b .
    .org (FREERTOS_VBAR + 0x680)
        b .
    .org (FREERTOS_VBAR + 0x700)
        b .
    .org (FREERTOS_VBAR + 0x780)
        b .
.macro GET_CORE_ID reg
#if ( configNUMBER_OF_CORES == 1 )
    MOV     \reg, #0
#else
    MRS     \reg, MPIDR_EL1
    LSR     \reg, \reg, #8
    AND     \reg, \reg, #0xFF
    ADD     \reg, \reg, #configBOOT_CORE
    AND     \reg, \reg, #( configMAX_NUM_CORES - 1 )
#endif
.endm

.macro portSAVE_CONTEXT

    /* Switch to use the EL0 stack pointer. */
    MSR     SPSEL, #0

    /* Save the entire context. */
    STP     X0, X1, [SP, #-0x10]!
    STP     X2, X3, [SP, #-0x10]!
    STP     X4, X5, [SP, #-0x10]!
    STP     X6, X7, [SP, #-0x10]!
    STP     X8, X9, [SP, #-0x10]!
    STP     X10, X11, [SP, #-0x10]!
    STP     X12, X13, [SP, #-0x10]!
    STP     X14, X15, [SP, #-0x10]!
    STP     X16, X17, [SP, #-0x10]!
    STP     X18, X19, [SP, #-0x10]!
    STP     X20, X21, [SP, #-0x10]!
    STP     X22, X23, [SP, #-0x10]!
    STP     X24, X25, [SP, #-0x10]!
    STP     X26, X27, [SP, #-0x10]!
    STP     X28, X29, [SP, #-0x10]!
    STP     X30, XZR, [SP, #-0x10]!

    /* Save the SPSR. */
    MRS     X3, SPSR_EL1
    MRS     X2, ELR_EL1

    STP     X2, X3, [SP, #-0x10]!

    /* Save the critical section nesting depth. */
    LDR     X0, ullCriticalNestingConst
    GET_CORE_ID X10
    LDR     X3, [X0, X10, LSL #3]

    /* Save the FPU context indicator. */
    LDR     X0, ullPortTaskHasFPUContextConst
    LDR     X2, [X0, X10, LSL #3]

    /* Save the FPU context, if any (32 128-bit registers). */
    CMP     X2, #0
    B.EQ    1f
    STP     Q0, Q1, [SP,#-0x20]!
    STP     Q2, Q3, [SP,#-0x20]!
    STP     Q4, Q5, [SP,#-0x20]!
    STP     Q6, Q7, [SP,#-0x20]!
    STP     Q8, Q9, [SP,#-0x20]!
    STP     Q10, Q11, [SP,#-0x20]!
    STP     Q12, Q13, [SP,#-0x20]!
    STP     Q14, Q15, [SP,#-0x20]!
    STP     Q16, Q17, [SP,#-0x20]!
    STP     Q18, Q19, [SP,#-0x20]!
    STP     Q20, Q21, [SP,#-0x20]!
    STP     Q22, Q23, [SP,#-0x20]!
    STP     Q24, Q25, [SP,#-0x20]!
    STP     Q26, Q27, [SP,#-0x20]!
    STP     Q28, Q29, [SP,#-0x20]!
    STP     Q30, Q31, [SP,#-0x20]!

1:
    /* Store the critical nesting count and FPU context indicator. */
    STP     X2, X3, [SP, #-0x10]!

    LDR     X0, pxCurrentTCBConst
    LDR     X1, [X0, X10, LSL #3]
    MOV     X0, SP
    STR     X0, [X1]

    /* Switch to use the ELx stack pointer. */
    MSR     SPSEL, #1

    .endm

; /**********************************************************************/

.macro portRESTORE_CONTEXT

    /* Switch to use the EL0 stack pointer. */
    MSR     SPSEL, #0

    /* Set the SP to point to the stack of the task being restored. */
    LDR     X0, pxCurrentTCBConst
    GET_CORE_ID X10
    LDR     X1, [X0, X10, LSL #3]
    LDR     X0, [X1]
    MOV     SP, X0

    LDP     X2, X3, [SP], #0x10  /* Critical nesting and FPU context. */

    /* Set the PMR register to be correct for the current critical nesting
    depth. */
    LDR     X0, ullCriticalNestingConst /* X0 holds the address of ullCriticalNesting. */
    STR     X3, [X0, X10, LSL #3]        /* Restore the task's critical nesting count. */
    MOV     X1, #255                    /* X1 holds the unmask value. */
    CMP     X3, #0
    B.EQ    1f
    LDR     X6, ullMaxAPIPriorityMaskConst
    LDR     X1, [X6]                    /* X1 holds the mask value. */
1:
    MSR     ICC_PMR_EL1, X1

    DSB     SY                          /* _RB_Barriers probably not required here. */
    ISB     SY

    /* Restore the FPU context indicator. */
    LDR     X0, ullPortTaskHasFPUContextConst
    STR     X2, [X0, X10, LSL #3]

    /* Restore the FPU context, if any. */
    CMP     X2, #0
    B.EQ    1f
    LDP     Q30, Q31, [SP], #0x20
    LDP     Q28, Q29, [SP], #0x20
    LDP     Q26, Q27, [SP], #0x20
    LDP     Q24, Q25, [SP], #0x20
    LDP     Q22, Q23, [SP], #0x20
    LDP     Q20, Q21, [SP], #0x20
    LDP     Q18, Q19, [SP], #0x20
    LDP     Q16, Q17, [SP], #0x20
    LDP     Q14, Q15, [SP], #0x20
    LDP     Q12, Q13, [SP], #0x20
    LDP     Q10, Q11, [SP], #0x20
    LDP     Q8, Q9, [SP], #0x20
    LDP     Q6, Q7, [SP], #0x20
    LDP     Q4, Q5, [SP], #0x20
    LDP     Q2, Q3, [SP], #0x20
    LDP     Q0, Q1, [SP], #0x20
1:
    LDP     X2, X3, [SP], #0x10  /* SPSR and ELR. */

    /* Restore the SPSR. */
    MSR     SPSR_EL1, X3
    /* Restore the ELR. */
    MSR     ELR_EL1, X2

    LDP     X30, XZR, [SP], #0x10
    LDP     X28, X29, [SP], #0x10
    LDP     X26, X27, [SP], #0x10
    LDP     X24, X25, [SP], #0x10
    LDP     X22, X23, [SP], #0x10
    LDP     X20, X21, [SP], #0x10
    LDP     X18, X19, [SP], #0x10
    LDP     X16, X17, [SP], #0x10
    LDP     X14, X15, [SP], #0x10
    LDP     X12, X13, [SP], #0x10
    LDP     X10, X11, [SP], #0x10
    LDP     X8, X9, [SP], #0x10
    LDP     X6, X7, [SP], #0x10
    LDP     X4, X5, [SP], #0x10
    LDP     X2, X3, [SP], #0x10
    LDP     X0, X1, [SP], #0x10

    /* Switch to use the ELx stack pointer.  _RB_ Might not be required. */
    MSR     SPSEL, #1

    ERET

    .endm


/******************************************************************************
 * FreeRTOS_SWI_Handler handler is used to perform a context switch.
 *****************************************************************************/
.align 8
.type FreeRTOS_SWI_Handler, %function
FreeRTOS_SWI_Handler:
    /* Save the context of the current task and select a new task to run. */
    portSAVE_CONTEXT
    MRS     X0, ESR_EL1

    LSR     X1, X0, #26

    CMP     X1, #0x15   /* 0x15 = SVC instruction. */
    B.NE    FreeRTOS_Abort
    GET_CORE_ID X10
    MOV X0, X10
    BL      vTaskSwitchContext

    portRESTORE_CONTEXT

FreeRTOS_Abort:
    /* Full ESR is in X0, exception class code is in X1. */
    B       .

/******************************************************************************
 * vPortRestoreTaskContext is used to start the scheduler.
 *****************************************************************************/
.align 8
.type vPortRestoreTaskContext, %function
vPortRestoreTaskContext:
.set freertos_vector_base,  _freertos_vector_table

    /* Install the FreeRTOS interrupt handlers. */
    LDR     X1, =freertos_vector_base
    MSR     VBAR_EL1, X1
    DSB     SY
    ISB     SY

    /* Start the first task. */
    portRESTORE_CONTEXT


/******************************************************************************
 * FreeRTOS_IRQ_Handler handles IRQ entry and exit.
 *****************************************************************************/
.align 8
.type FreeRTOS_IRQ_Handler, %function
FreeRTOS_IRQ_Handler:
    /* Save volatile registers. */
    STP     X0, X1, [SP, #-0x10]!
    STP     X2, X3, [SP, #-0x10]!
    STP     X4, X5, [SP, #-0x10]!
    STP     X6, X7, [SP, #-0x10]!
    STP     X8, X9, [SP, #-0x10]!
    STP     X10, X11, [SP, #-0x10]!
    STP     X12, X13, [SP, #-0x10]!
    STP     X14, X15, [SP, #-0x10]!
    STP     X16, X17, [SP, #-0x10]!
    STP     X18, X19, [SP, #-0x10]!
    STP     X29, X30, [SP, #-0x10]!

    /* Save the SPSR and ELR. */
    MRS     X3, SPSR_EL1
    MRS     X2, ELR_EL1
    STP     X2, X3, [SP, #-0x10]!

    /* Increment the interrupt nesting counter. */
    LDR     X5, ullPortInterruptNestingConst
    GET_CORE_ID X10
    LDR     X1, [X5, X10, LSL #3]
    ADD     X6, X1, #1
    STR     X6, [X5, X10, LSL #3]

    /* Maintain the interrupt nesting information across the function call. */
    STP     X1, X5, [SP, #-0x10]!

    /* Read value from the interrupt acknowledge register, which is stored in W0
    for future parameter and interrupt clearing use. */

    MRS     X0, ICC_IAR1_EL1

    /* Check if its a special interrupt id. If it is, consider it as an invalid
     * read and exit the IRQ Handler
     */
    CMP X0, #1020
    B.GE Skip_ISR_Handler

    /* Maintain the ICCIAR value across the function call. */
    STP     X0, X1, [SP, #-0x10]!

    /* Call the C handler. */
    BL vApplicationIRQHandler

    /* Disable interrupts. */
    MSR     DAIFSET, #2
    DSB     SY
    ISB     SY

    /* Restore the ICCIAR value. */
    LDP     X0, X1, [SP], #0x10

    /* End IRQ processing by writing ICCIAR to the EOI register. */
    MSR   ICC_EOIR1_EL1, X0

Skip_ISR_Handler:
    /* Restore the critical nesting count. */
    LDP     X1, X5, [SP], #0x10
    GET_CORE_ID X10
    STR     X1, [X5, X10, LSL #3]

    /* Has interrupt nesting unwound? */
    CMP     X1, #0
    B.NE    Exit_IRQ_No_Context_Switch

    /* Is a context switch required? */
    LDR     X0, ullPortYieldRequiredConst
    LDR     X1, [X0, X10, LSL #3]
    CMP     X1, #0
    B.EQ    Exit_IRQ_No_Context_Switch

    /* Reset ullPortYieldRequired to 0. */
    MOV     X2, #0
    STR     X2, [X0, X10, LSL #3]

    /* Restore volatile registers. */
    LDP     X4, X5, [SP], #0x10  /* SPSR and ELR. */
    MSR     SPSR_EL1, X5
    MSR     ELR_EL1, X4
    DSB     SY
    ISB     SY

    LDP     X29, X30, [SP], #0x10
    LDP     X18, X19, [SP], #0x10
    LDP     X16, X17, [SP], #0x10
    LDP     X14, X15, [SP], #0x10
    LDP     X12, X13, [SP], #0x10
    LDP     X10, X11, [SP], #0x10
    LDP     X8, X9, [SP], #0x10
    LDP     X6, X7, [SP], #0x10
    LDP     X4, X5, [SP], #0x10
    LDP     X2, X3, [SP], #0x10
    LDP     X0, X1, [SP], #0x10

    /* Save the context of the current task and select a new task to run. */
    portSAVE_CONTEXT
    GET_CORE_ID X10
    MOV X0, X10
    BL vTaskSwitchContext
    portRESTORE_CONTEXT

Exit_IRQ_No_Context_Switch:
    /* Restore volatile registers. */
    LDP     X4, X5, [SP], #0x10  /* SPSR and ELR. */
    MSR     SPSR_EL1, X5
    MSR     ELR_EL1, X4
    DSB     SY
    ISB     SY

    LDP     X29, X30, [SP], #0x10
    LDP     X18, X19, [SP], #0x10
    LDP     X16, X17, [SP], #0x10
    LDP     X14, X15, [SP], #0x10
    LDP     X12, X13, [SP], #0x10
    LDP     X10, X11, [SP], #0x10
    LDP     X8, X9, [SP], #0x10
    LDP     X6, X7, [SP], #0x10
    LDP     X4, X5, [SP], #0x10
    LDP     X2, X3, [SP], #0x10
    LDP     X0, X1, [SP], #0x10

    ERET

EnableFPU:
    SUB SP, SP, #16
    STR X0, [SP]

    MRS     X0, CPACR_EL1
    ORR     X0, X0, #(0XF << 20)
    MSR     CPACR_EL1, X0
    ISB

    LDR X0, [SP]
    ADD SP, SP, #16

    ERET

.align 8
#if configNUMBER_OF_CORES > 1
pxCurrentTCBConst: .dword pxCurrentTCBs
#else
pxCurrentTCBConst: .dword pxCurrentTCB
#endif
ullCriticalNestingConst: .dword ullCriticalNesting
ullPortTaskHasFPUContextConst: .dword ullPortTaskHasFPUContext

ullMaxAPIPriorityMaskConst: .dword ullMaxAPIPriorityMask
ullPortInterruptNestingConst: .dword ullPortInterruptNesting
ullPortYieldRequiredConst: .dword ullPortYieldRequired
vApplicationIRQHandlerConst: .word vApplicationIRQHandler

.end
