/******************************************************************************
* Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
******************************************************************************/
/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Modifications for SoC FPGA
 */

.global _boot
.global _secondary_boot
.global _core_setup
.global _prestart
.global _cpu_init_hook
.global _freertos_vector_table

.global __el3_stack
.global __el2_stack
.global __el1_stack
.global __el0_stack

.extern vCoreEntry
.set vector_base, _freertos_vector_table
//.section .boot,"ax"

/*
 * Stack total: 1Mb
 * 256kb for each EL
 * Ad per the current LD script
 * 0xFFEFFFFF   0xFFF3FFFF  0xFFF7FFFF  0xFFFBFFFF
   |    XXX     |    EL2    |    EL1    |    EL0    |
 **/
.set EL2_stack,     __el2_stack
.set EL1_stack,     __el1_stack
.set EL0_stack,     __el0_stack
.set EL01_stack,    __el01_stack
.set EL11_stack,    __el11_stack
.set EL21_stack,    __el21_stack

.macro GET_CORE_ID reg
    MRS     \reg, MPIDR_EL1
    UBFX    \reg, \reg, #8, #8
.endm

/* Boot core execution starts here */
_boot:
	// Initialize the stack pointer of EL2.
	ldr	x2,=EL2_stack
	mov	sp, x2

	ldr	 x2,=EL1_stack
	msr	 sp_el1,x2

	ldr x0, =EL0_stack // Load the stack pointer address for EL0 into x0
	msr sp_el0, x0       // Set SP_EL0

    bl _core_setup

    /*
     * _mmu_configure(uint64_t setup)
     * setup=1: setup PT, setup=0: do not setup PT
     */
    mov x0, #1
    bl _mmu_configure
    bl _mainCRTStartup
    b error

_secondary_boot:
	ldr	x2,=EL21_stack
	mov	sp,x2

	ldr	 x2,=EL11_stack
	msr	 sp_el1,x2

	ldr x0, =EL01_stack
	msr sp_el0, x0

    bl _core_setup

    /*
     * _mmu_configure(uint64_t setup)
     * setup=1: setup PT, setup=0: do not setup PT
     */
    mov x0, #0
    bl _mmu_configure
    /* vCoreEntry should not return */
    bl vCoreEntry
    b error

/* Contains setup instructions required by all cores */
_core_setup:
    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x8, #0
    mov x9, #0
    mov x10, #0
    mov x11, #0
    mov x12, #0
    mov x13, #0
    mov x14, #0
    mov x15, #0
    mov x16, #0
    mov x17, #0
    mov x18, #0
    mov x19, #0
    mov x20, #0
    mov x21, #0
    mov x22, #0
    mov x23, #0
    mov x24, #0
    mov x25, #0
    mov x26, #0
    mov x27, #0
    mov x28, #0
    mov x29, #0
    /* X30 contains the return address */

    MOV x1, #1             // Set NS bit, to access Non-secure registers

    ISB
    MSR ICC_SRE_EL2, x0
    ISB
    MSR ICC_SRE_EL1, x1    // SRE = 1, Enable the Ssystem register interface for the current security level.

    mrs x0, currentEL      // Read currentEL and jump based on currentl exception level.
    cmp x0, #0x8
    beq SwitchToEL1

    cmp x0, #0x4
    beq setupEL1

    b error         // go to error if current exception level is neither EL3 nor EL1

SwitchToEL1:
	// Initialize VBAR_EL3.
	ldr x1, =vector_base
	msr VBAR_EL2, x1
	ldr x1, =vector_base
	msr VBAR_EL1, x1

	msr hcr_el2, xzr     // Clear the hcr_el2
	ldr x1, =0x30C50838
	msr sctlr_el2, x1
	msr sctlr_el1, x1


	msr sctlr_el1, xzr
	mrs x0, hcr_el2
	orr x0, x0,  #(1<<31) // RW=1  EL1 Execution state is AArch64.
	orr x0, x0,  #(1<<34) // E2H=1 EL2 host enable.
	msr hcr_el2, x0

	mov x0, #0b00101      // Use the EL1 stack from EL1.
	msr spsr_el2, x0      // M[4:0]=00101 EL1h must match HCR_EL2.RW.

	mov x0, #0x33ff
	msr cptr_el2, x0

	adr x0, setupEL1      // Set the address to jump to after eret executes.
	msr elr_el2, x0

	eret

setupEL1:
	/*Set vector table base address*/
	ldr	x1, =vector_base
	msr	VBAR_EL1,x1

	/* Trap floating point access only in case of standalone BSP */
    mrs x0, CPACR_EL1
    orr x0, x0, #(0x3 << 20)
    msr CPACR_EL1, x0
	isb

	/*
	 * Clear FPUStatus variable to make sure that it contains current
	 * status of FPU i.e. disabled. In case of a warm restart execution
	 * when bss sections are not cleared, it may contain previously updated
	 * value which does not hold true now.
	 */

	/* Disable MMU first */
	mrs	x1, SCTLR_EL1
	bic x1, x1, #0x1
	orr x1, x1, #0x2
	msr     SCTLR_EL1, x1
	mov x0, #0b00100      // Use the EL1 stack from EL1.
	msr spsr_el1, x0      // M[4:0]=00101 EL1h must match HCR_EL2.RW.
	isb
    dsb sy
    ret

error: 	b	error

.end
