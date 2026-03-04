/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief change the cache setting of page
 *
 * This will change the caching attribute of the selected page
 * The page containing the VA is the target (granule size is 2M)
 *
 * @param [in] addr is the VA containied in target page
 *
 * @param [in] mode is the cache mode
 *              0: turnoff caching
 *              1: turnon caching
 *
 */
extern void config_page_caching( void * addr,
                                 int mode );
extern size_t get_smallest_ever_remaining_heap_size( void );
extern size_t get_remaining_heap_size( void );

static uintptr_t _coherent_bytes_left = 0;
static uintptr_t _coherent_pointer = 0;

size_t xPortGetFreeHeapSize( void )
{
    return get_remaining_heap_size();
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( void )
{
    return get_smallest_ever_remaining_heap_size();
}
/*-----------------------------------------------------------*/

#define GRANULE_SIZE    ( 0x200000U )

void * pvPortMallocCoherent( size_t xWantedSize )
{
void * pvReturn;
int lnumPages = xWantedSize / GRANULE_SIZE;

    vTaskSuspendAll();
    {
        if( ( _coherent_bytes_left == 0 ) || ( xWantedSize > _coherent_bytes_left ) )
        {
            pvReturn = aligned_alloc( GRANULE_SIZE, GRANULE_SIZE
                                      + ( lnumPages * GRANULE_SIZE ) );

            if( pvReturn != NULL )
            {
                config_page_caching( pvReturn, 0 );

                if( ( xWantedSize % GRANULE_SIZE ) != 0 )
                {
                    _coherent_bytes_left = GRANULE_SIZE - ( xWantedSize % GRANULE_SIZE );
                    _coherent_pointer = ( uintptr_t ) pvReturn + xWantedSize;
                }
                else
                {
                    _coherent_bytes_left = 0;
                    _coherent_pointer = 0;
                }
            }
        }
        else
        {
            pvReturn = ( void * ) _coherent_pointer;
            _coherent_pointer += xWantedSize;
            _coherent_bytes_left += xWantedSize;
        }

        traceMALLOC( pvReturn, xWantedSize );
    }
    ( void ) xTaskResumeAll();

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;
}
/*-----------------------------------------------------------*/

void * pvPortAlignedAlloc( size_t xAlignemnt,
                           size_t xWantedSize )
{
void * pvReturn;

    vTaskSuspendAll();
    {
        pvReturn = aligned_alloc( xAlignemnt, xWantedSize );
        traceMALLOC( pvReturn, xWantedSize );
    }
    ( void ) xTaskResumeAll();

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;
}
/*-----------------------------------------------------------*/
