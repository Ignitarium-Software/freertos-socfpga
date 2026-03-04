/*
 * SPDX-FileCopyrightText: Copyright (C) 2025-2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Low level driver hooks for standard library
 */

#include <sys/stat.h>
#include <unistd.h>
#include <socfpga_console.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <sys/lock.h>
#include <FreeRTOSConfig.h>
#include <task.h>

#define STDIO_LOCK_COUNT ( PORT_SPIN_LOCK_COUNT - PORT_STDLIB_SPIN_LOCK_STDIN )
#define STDIO_LOCK_ID(x)    ( x - PORT_STDLIB_SPIN_LOCK_MALLOC )

struct __lock
{
    int spinlock_id;
};
typedef struct __lock * _LOCK_T;

static size_t remaining_heap;
static size_t smallest_ever_remaining_heap;
extern struct __lock __lock___malloc_recursive_mutex;
struct __lock stdio_lock_data[ STDIO_LOCK_COUNT ];

static void get_spin_lock(int id)
{
    portDISABLE_INTERRUPTS();
    portGET_STDLIB_LOCK(id);
}

static int give_spin_lock(int id)
{
    int ret = portRELEASE_STDLIB_LOCK(id);
    if( ret <= 0 )
    {
        portENABLE_INTERRUPTS();
    }
    return ret;
}

/* Hooks needed for locking internal buffer usage
 * important in the case of SMP
 * */

void __wrap___retarget_lock_init(_LOCK_T *lock)
{
    ( void ) lock;
}

void __wrap___retarget_lock_init_recursive(_LOCK_T *lock)
{
    static int lock_init_idx = 0;
    if(lock_init_idx < 3)
    {
        stdio_lock_data[lock_init_idx].spinlock_id  = lock_init_idx + 1;
        *lock = &stdio_lock_data[lock_init_idx];
        lock_init_idx++;
    }
}

void __wrap___retarget_lock_close(_LOCK_T lock)
{
    ( void ) lock;
}

void __wrap___retarget_lock_close_recursive(_LOCK_T lock)
{
    ( void ) lock;
}

void __wrap___retarget_lock_acquire(_LOCK_T lock)
{
    get_spin_lock(lock->spinlock_id);
}

void __wrap___retarget_lock_acquire_recursive(_LOCK_T lock)
{
    get_spin_lock(lock->spinlock_id);
}

int __wrap___retarget_lock_try_acquire(_LOCK_T lock)
{
    get_spin_lock(lock->spinlock_id);
    return 1;
}

int __wrap___retarget_lock_try_acquire_recursive(_LOCK_T lock)
{
    get_spin_lock(lock->spinlock_id);
    return 1;
}

void __wrap___retarget_lock_release(_LOCK_T lock)
{
    (void)give_spin_lock(lock->spinlock_id);
}

void __wrap___retarget_lock_release_recursive(_LOCK_T lock)
{
    (void)give_spin_lock(lock->spinlock_id);
}
/*-----------------------------------------------------------*/

int _close( int file )
{
    ( void ) file;
    return -1;
}
/*-----------------------------------------------------------*/

int _fstat( int file,
            struct stat * st )
{
    ( void ) file;
    if( st == NULL )
    {
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}
/*-----------------------------------------------------------*/

int _isatty( int file )
{
    ( void ) file;
    return 1;
}
/*-----------------------------------------------------------*/

int _lseek( int file,
            int ptr,
            int dir )
{
    ( void ) file;
    ( void ) ptr;
    ( void ) dir;
    return 0;
}
/*-----------------------------------------------------------*/

int _read( int file,
           char * ptr,
           int len )
{
    ( void ) file;
    ( void ) len;
    int lock_recur_cnt = 0;

    do
    {
        lock_recur_cnt++;
    } while(give_spin_lock(STDIO_LOCK_ID(PORT_STDLIB_SPIN_LOCK_STDIN)));

    console_read( ( unsigned char * const ) ptr, 1 );

    for(;lock_recur_cnt > 0; lock_recur_cnt--)
    {
        get_spin_lock(STDIO_LOCK_ID(PORT_STDLIB_SPIN_LOCK_STDIN));
    }

    return 1;
}
/*-----------------------------------------------------------*/

int _write( int file,
            const char * ptr,
            int len )
{
    ( void ) file;
    int lock_recur_cnt = 0;
    console_fill_buffer( ( unsigned char * const ) ptr, len );

    do
    {
        lock_recur_cnt++;
    } while(give_spin_lock(STDIO_LOCK_ID(PORT_STDLIB_SPIN_LOCK_STDOUT)));

    console_signal_fill();

    for(;lock_recur_cnt > 0; lock_recur_cnt--)
    {
        get_spin_lock(STDIO_LOCK_ID(PORT_STDLIB_SPIN_LOCK_STDOUT));
    }

    return len;
}
/*-----------------------------------------------------------*/

int _kill( int pid,
           int sig )
{
    ( void ) pid;
    ( void ) sig;
    return 0;
}
/*-----------------------------------------------------------*/

int _getpid( void )
{
    return 1;
}
/*-----------------------------------------------------------*/

int atexit( void ( *func )( void ) )
{
    ( void ) func;
    return 0;
}
/*-----------------------------------------------------------*/

void exit( int rc )
{
    ( void ) rc;

    while( 1 )
    {
    }
}
/*-----------------------------------------------------------*/

void * _sbrk( ptrdiff_t incr )
{
extern char end;       /* Set by linker.  */
extern char _heap_end; /* Set by linker */
static char * heap_end;
char * prev_heap_end;

    if( heap_end == NULL )
    {
        /*Init the heap */
        heap_end = &end;
        remaining_heap = &_heap_end - &end;
        smallest_ever_remaining_heap = remaining_heap;
    }

    if( ( heap_end + incr ) >= &_heap_end )
    {
        prev_heap_end = NULL;
    }
    else
    {
        prev_heap_end = heap_end;
        heap_end += incr;
        remaining_heap -= incr;

        if( smallest_ever_remaining_heap > remaining_heap )
        {
            smallest_ever_remaining_heap = remaining_heap;
        }
    }

    return ( void * ) prev_heap_end;
}
/*-----------------------------------------------------------*/

size_t get_remaining_heap_size( void )
{
    return remaining_heap;
}
/*-----------------------------------------------------------*/

size_t get_smallest_ever_remaining_heap_size( void )
{
    return smallest_ever_remaining_heap;
}
/*-----------------------------------------------------------*/
