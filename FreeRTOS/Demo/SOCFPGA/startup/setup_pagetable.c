/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 */

#include <stdint.h>
#include <stddef.h>

/* Using C even before initialising the c runtime
 * This means we cannot expect some pre requisits of C
 * Example the BSS wont be zero and any global data in bss will be cleared
 * after we use it here (dont expect bss data to persist after we exit this
 * file)
 * */

#define PT_DESC_VALID               ( uint64_t ) 1UL
#define PT_DESC_AF                  ( uint64_t ) ( 1UL << 10UL )
#define PT_ENTRY_IS_TABLE           ( uint64_t ) ( 1UL << 1UL )
#define PT_MAIR_IDX( x )    ( uint64_t ) ( ( uint64_t ) x << 2UL )
#define PT_DESC_PXN                 ( uint64_t ) ( 1UL << 53UL )
#define PT_DESC_UXN                 ( uint64_t ) ( 1UL << 54UL )
#define PT_DESC_SH_INNER            ( uint64_t ) ( 3UL << 8UL )
#define PT_DESC_SH_OUTER            ( uint64_t ) ( 2UL << 8UL )

#define MEM_NORMAL_NON_CACHEABLE    ( 0x44UL )
#define MEM_NORMAL_CACHEABLE        ( 0xFFUL )
#define MEM_DEVICE_nGnRnE           ( 0x0UL )
#define MEM_OFFSET_L2_IDX( x )       ( ( uint64_t ) x << 21ULL )
#define MEM_OFFSET_L1_IDX( x )       ( ( uint64_t ) x << 30ULL )

#define WRITE_SYS_REG( REG, VAR )    asm volatile ( "msr " # REG ", %0" : : "r" ( VAR ) )
#define READ_SYS_REG( REG, VAR )     asm volatile ( "mrs %0, " # REG : "=r" ( VAR ) )

#define NUM_L2_PT    512
#define NUM_L1_PT    512

/* Make sure the Memory is zerowed out here
 * Becaues the crt init happens after this
 * Using C here even befor the C runtime init just for convineance and avoid
 * the hassle of using ASSEMBLY
 * */
uint64_t l0_pagetable[ 2 ] __attribute__( ( section( ".data" ), aligned( 4096 ) ) ) = { 0 };
uint64_t l1_pagetable[ NUM_L1_PT ] __attribute__( ( section( ".data" ), aligned( 4096 ) ) ) = { 0 };
uint64_t l2_pagetable[ NUM_L2_PT ] __attribute__( ( section( ".data" ), aligned( 4096 ) ) ) = { 0 };

static inline void invalidte_tlb()
{
    /* Invalidate the TLB and flush the instruction cache*/
    asm volatile ( "TLBI VMALLE1" );
    asm volatile ( "DSB SY" );
    asm volatile ( "ISB" );
}
/*-----------------------------------------------------------*/

static inline void invalidate_cache( void * address,
                                     size_t length )
{
uint64_t ctr_el0;
uint32_t cacheline_width;
uintptr_t addr = ( uintptr_t ) address;
uintptr_t end_addr = ( uintptr_t ) address + length;

    /* Get the min cache lane width
       in CTR_EL0 bits x[16:19], lane width = 2^x * 4 */
    READ_SYS_REG( CTR_EL0, ctr_el0 );
    cacheline_width = ( ctr_el0 & 0xF0000 ) >> 16;
    cacheline_width = ( 2 << ( cacheline_width - 1 ) ) * 4;

    while( addr < end_addr )
    {
        asm volatile ( "DC IVAC, %0" : : "r" ( addr ) : "memory" );
        addr += cacheline_width;
    }
}
/*-----------------------------------------------------------*/

void prepare_l0_pages( void )
{
    /* The system has 40bit address space. So PT starts with l0
       l0 has 2 entries
       0x0000000000 to 0x7FFFFFFFFF (area of interrest)
       0x8000000000 to 0xFFFFFFFFFF (not used) */
    l0_pagetable[ 0 ] = ( ( uint64_t ) l1_pagetable | PT_MAIR_IDX( 0 ) | PT_DESC_VALID | PT_ENTRY_IS_TABLE );
    l0_pagetable[ 1 ] = ( 0x8000000000 | PT_MAIR_IDX( 0 ) );
}
/*-----------------------------------------------------------*/

void prepare_l1_pages( void )
{
    /* l1 page table has 512 entries
       Spliting the range further to 1GB segments
       0x00000000 to 0x3FFFFFFF ( Device memory )
       0x40000000 to 0x7FFFFFFF ( Device memory )
       0x80000000 to 0xBFFFFFFF ( Normal memory )
       0xC0000000 to 0xFFFFFFFF ( Normal memory )
       ...
       Device memory (default) */
    l1_pagetable[ 0 ] =
        MEM_OFFSET_L1_IDX( 0 ) | PT_DESC_AF | PT_DESC_VALID | PT_MAIR_IDX( 0 ) |
        PT_DESC_PXN | PT_DESC_UXN;
    l1_pagetable[ 1 ] =
        MEM_OFFSET_L1_IDX( 1 ) | PT_DESC_AF | PT_DESC_VALID | PT_MAIR_IDX( 0 ) |
        PT_DESC_PXN | PT_DESC_UXN;
    l1_pagetable[ 2 ] =
        ( uint64_t ) l2_pagetable | PT_DESC_AF | PT_DESC_VALID | PT_MAIR_IDX( 1 ) | PT_ENTRY_IS_TABLE;
    l1_pagetable[ 3 ] =
        MEM_OFFSET_L1_IDX( 3 ) | PT_DESC_AF | PT_DESC_VALID | PT_MAIR_IDX( 1 ) | PT_DESC_SH_OUTER;

    for( int idx = 4; idx < NUM_L1_PT - 4; idx++ )
    {
        l1_pagetable[ idx ] =
            MEM_OFFSET_L1_IDX( idx ) | PT_DESC_AF | PT_MAIR_IDX( 0 );
    }
}
/*-----------------------------------------------------------*/

void prepare_l2_pages( int offset )
{
    /*l2 page table has yet another 512 entries
      Bringing down the page size to 2MB chunks */
    for( int idx = 0; idx < NUM_L2_PT; idx++ )
    {
        l2_pagetable[ idx ] =
            ( MEM_OFFSET_L1_IDX( offset ) + MEM_OFFSET_L2_IDX( idx ) ) |
            PT_DESC_AF | PT_DESC_VALID | PT_MAIR_IDX( 1 ) | PT_DESC_SH_OUTER;
    }
}
/*-----------------------------------------------------------*/

void config_page_caching( void * addr,
                          int mode )
{
uint64_t * ptr;
uint32_t offset = ( uint32_t ) ( ( uint64_t ) ( addr - 0x80000000 ) >> 20 ) / 2;
int mair_idx;

    if( mode == 1 )
    {
        mair_idx = 1;
    }
    else
    {
        mair_idx = 2;
    }

    l2_pagetable[ offset ] =
        ( l2_pagetable[ offset ] & ~( ( uint64_t ) 0xC ) ) | PT_MAIR_IDX( mair_idx );

    /*Writeback the pagetable from cache to memory before TLB invalidate*/
    ptr = &l2_pagetable[ offset ];
    asm volatile ( "DC CVAC, %0" : : "r" ( ptr ) : "memory" );

    /*Force the TLB to be reloaded*/
    invalidte_tlb();
}
/*-----------------------------------------------------------*/

void _mmu_configure( uint64_t setup )
{
uint64_t mair;
uint64_t tcr_el1;
uint64_t sctlr_el1;
uint64_t cpacr_el1;
uint64_t ttbr0_el1 = ( uint64_t ) l0_pagetable;

    if( setup != 0 )
    {
        prepare_l0_pages();
        prepare_l1_pages();
        prepare_l2_pages( 2 ); /* Use L2 pagetable for region 0x80000000-0xBFFFFFFF */
    }
    /* Setup MAIR attributes
       Index 0: Device memory nGnRnE
       Index 1: Normal memory (cache enabled)
       Index 2: Normal memory (cache disabled) */
    mair = MEM_DEVICE_nGnRnE |
           MEM_NORMAL_CACHEABLE << 8 |
           MEM_NORMAL_NON_CACHEABLE << 16;

    WRITE_SYS_REG( MAIR_EL1, mair );

    /* Set T0SZ, defined number of bits used for pageing
       bits used : 64 - T0SZ */
    /*tcr_el1 = 0x18 | (0x1 << 8) | (0x1 << 10) | (0x3 << 12) | (0x1 << 23);*/
    tcr_el1 = 0x18 | ( 0x1 << 23 );
    /*tcr_el1 = 0x18;*/

    WRITE_SYS_REG( TCR_EL1, tcr_el1 );

    /*Set the l1 pagetable address to be used for paging*/
    WRITE_SYS_REG( TTBR0_EL1, ttbr0_el1 );
    asm volatile ( "ISB" );

    /* Invalidate the TLB and flush the instruction cache*/
    invalidte_tlb();

    /*READ_SYS_REG(SCTLR_EL1, sctlr_el1);*/
    /*sctlr_el1 &= ~(1UL < 1UL); Clear A bit (alignment check)*/
    sctlr_el1 = 0;
    sctlr_el1 |= 1UL | ( 1UL << 2 ) | ( 1UL << 12 ); /* Enable MMU */
    WRITE_SYS_REG( SCTLR_EL1, sctlr_el1 );

    READ_SYS_REG( CPACR_EL1, cpacr_el1 );
    cpacr_el1 |= ( uint64_t ) ( 3UL << 20UL ); /* Set FPEN bits */
    cpacr_el1 |= ( uint64_t ) ( 3UL << 26UL ); /* set SMEN bits */
    WRITE_SYS_REG( CPACR_EL1, cpacr_el1 );

    asm volatile ( "ISB" );
    if( setup != 0 )
    {
        invalidate_cache( l0_pagetable, sizeof( l0_pagetable ) );
        invalidate_cache( l1_pagetable, sizeof( l1_pagetable ) );
        invalidate_cache( l2_pagetable, sizeof( l2_pagetable ) );
    }
}
/*-----------------------------------------------------------*/
