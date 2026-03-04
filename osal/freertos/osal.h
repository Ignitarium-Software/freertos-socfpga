/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2025-2026 Altera Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_OSAL_FREERTOS_H_
#define TUSB_OSAL_FREERTOS_H_

// FreeRTOS Headers
#include "compiler.h"
#include <stdbool.h>
#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include "stream_buffer.h"
#include <semphr.h>
#include <queue.h>
#include <task.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Task defines
#ifndef OSAL_TASK_STACK_SIZE
#define OSAL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#endif

// Timeout
#define OSAL_TIMEOUT_NOTIMEOUT     (0)          // Return immediately
#define OSAL_TIMEOUT_NORMAL        (10)         // Default timeout
#define OSAL_TIMEOUT_WAIT_FOREVER  (UINT64_MAX) // Wait forever
#define OSAL_TIMEOUT_CONTROL_XFER  OSAL_TIMEOUT_WAIT_FOREVER

#define OSAL_KERNEL_RUNNING        0
#define OSAL_KERNEL_NOT_RUNNING    1
#define OSAL_KERNEL_NOT_STARTED    2
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

#if configSUPPORT_STATIC_ALLOCATION
  typedef StaticSemaphore_t osal_semaphore_def_t;
  typedef StaticSemaphore_t osal_mutex_def_t;
#else
// not used therefore defined to smallest possible type to save space
typedef uint8_t osal_semaphore_def_t;
typedef uint8_t osal_mutex_def_t;
#endif

typedef SemaphoreHandle_t osal_semaphore_t;
typedef SemaphoreHandle_t osal_mutex_t;
typedef QueueHandle_t osal_queue_t;
typedef StreamBufferHandle_t osal_pipe_t;
typedef void (* osal_pipe_cb_t)( osal_pipe_t pipe, long is_isr,
                                long * const need_ctx_sw );

typedef struct
{
    uint16_t depth;
    uint16_t item_sz;
    void *buf;

#if defined(configQUEUE_REGISTRY_SIZE) && (configQUEUE_REGISTRY_SIZE>0)
    char const *name;
#endif

#if configSUPPORT_STATIC_ALLOCATION
  StaticQueue_t sq;
#endif
} osal_queue_def_t;

#if defined(configQUEUE_REGISTRY_SIZE) && (configQUEUE_REGISTRY_SIZE>0)
#define _OSAL_Q_NAME(_name) .name = #_name
#else
  #define _OSAL_Q_NAME(_name)
#endif

// _int_set is not used with an RTOS
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf, _OSAL_Q_NAME(_name) }

typedef void (*osal_task_routine_t)( void* );

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline bool osal_task_create(
        osal_task_routine_t routine, const char *const name,
        void *const argument, int priority )
{
    return xTaskCreate(routine, name, OSAL_TASK_STACK_SIZE, argument, priority,
            NULL);
}

TU_ATTR_ALWAYS_INLINE static inline uint64_t _osal_ms2tick( uint64_t msec )
{

    if ( msec == OSAL_TIMEOUT_WAIT_FOREVER )
        return portMAX_DELAY ;
    if ( msec == 0 )
        return 0;

    TickType_t ticks = pdMS_TO_TICKS(msec);

    // configTICK_RATE_HZ is less than 1000 and 1 tick > 1 ms
    // we still need to delay at least 1 tick
    if ( ticks == 0 )
        ticks = 1;

    return ticks;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay( uint64_t msec )
{
    vTaskDelay(pdMS_TO_TICKS(msec));
}

TU_ATTR_ALWAYS_INLINE static inline void osal_task_delete( void )
{
	vTaskDelete(NULL);
}
//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(
        osal_semaphore_def_t *semdef )
{
#if configSUPPORT_STATIC_ALLOCATION
  return xSemaphoreCreateBinaryStatic(semdef);
#else
    (void) semdef;
    return xSemaphoreCreateBinary();
#endif
}

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_counting_create(
	osal_semaphore_def_t *semdef, UBaseType_t uxMaxCount,
	UBaseType_t uxInitialCount)
{
#if configSUPPORT_STATIC_ALLOCATION
  	return xSemaphoreCreateCountingStatic(uxMaxCount, uxInitialCount, semdef);
#else
    (void) semdef;
    return xSemaphoreCreateCounting(uxMaxCount, uxInitialCount);
#endif
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_delete(
        osal_semaphore_t semd_hdl )
{
    vSemaphoreDelete(semd_hdl);
    return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(
        osal_semaphore_t sem_hdl )
{
    if ( !xPortIsInsideInterrupt() )
    {
        return xSemaphoreGive(sem_hdl) != 0;
    }
    else
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        BaseType_t res = xSemaphoreGiveFromISR(sem_hdl,
                &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        return res != 0;
    }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(
        osal_semaphore_t sem_hdl, uint64_t msec )
{
    if ( !xPortIsInsideInterrupt() )
    {
        return xSemaphoreTake(sem_hdl, _osal_ms2tick(msec));
    }
    else
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        BaseType_t res = xSemaphoreTakeFromISR(sem_hdl,
                &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return res != 0;
    }
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(
        osal_semaphore_t const sem_hdl )
{
    xQueueReset(sem_hdl);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(
        osal_mutex_def_t *mdef )
{
#if configSUPPORT_STATIC_ALLOCATION
  return xSemaphoreCreateMutexStatic(mdef);
#else
    (void) mdef;
    return xSemaphoreCreateMutex();
#endif
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_delete(
        osal_mutex_t mutex_hdl )
{
    vSemaphoreDelete(mutex_hdl);
    return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock(
        osal_mutex_t mutex_hdl, uint64_t msec )
{
    return osal_semaphore_wait(mutex_hdl, msec);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(
        osal_mutex_t mutex_hdl )
{
    return osal_semaphore_post(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(
        osal_queue_def_t *qdef )
{
    osal_queue_t q;

#if configSUPPORT_STATIC_ALLOCATION
  q = xQueueCreateStatic(qdef->depth, qdef->item_sz, (uint8_t*) qdef->buf, &qdef->sq);
#else
    q = xQueueCreate(qdef->depth, qdef->item_sz);
#endif

#if defined(configQUEUE_REGISTRY_SIZE) && (configQUEUE_REGISTRY_SIZE>0)
    vQueueAddToRegistry(q, qdef->name);
#endif

    return q;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_delete( osal_queue_t qhdl )
{
    vQueueDelete(qhdl);
    return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive( osal_queue_t qhdl,
        void *data, uint64_t msec )
{
    return xQueueReceive(qhdl, data, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send( osal_queue_t qhdl,
        void const *data )
{
    if ( !xPortIsInsideInterrupt() )
    {
        return xQueueSendToBack(qhdl, data, OSAL_TIMEOUT_WAIT_FOREVER) != 0;
    }
    else
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        BaseType_t res = xQueueSendToBackFromISR(qhdl, data,
                &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        return res != 0;
    }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty( osal_queue_t qhdl )
{
    return uxQueueMessagesWaiting(qhdl) == 0;
}

TU_ATTR_ALWAYS_INLINE static inline osal_pipe_t osal_pipe_create(uint32_t stream_size,
                            osal_pipe_cb_t read_cb, osal_pipe_cb_t write_cb)
{
    return xStreamBufferCreateWithCallback(stream_size, 1, write_cb, read_cb);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t osal_pipe_send(osal_pipe_t phndl, uint8_t * data, uint32_t size)
{
    int bytes_written = 0;
    if ( !xPortIsInsideInterrupt() )
    {
        bytes_written = xStreamBufferSend(phndl, data, size, 0);
    }
    else
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        bytes_written = xStreamBufferSendFromISR(phndl, data, size,
                &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    return bytes_written;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t osal_pipe_receive(osal_pipe_t phndl, uint8_t * buffer, uint32_t size)
{
    int bytes_read = 0;
    if ( !xPortIsInsideInterrupt() )
    {
        bytes_read = xStreamBufferReceive(phndl, buffer, size, 0);
    }
    else
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        bytes_read = xStreamBufferReceiveFromISR(phndl, buffer, size,
                &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    return bytes_read;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t osal_pipe_bytes_available(osal_pipe_t phndl)
{
    return xStreamBufferBytesAvailable(phndl);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t osal_pipe_space_available(osal_pipe_t phndl)
{
    return xStreamBufferSpacesAvailable(phndl);
}

/* When calling within a task,
 * wrap the call inside a critical section
 * */
TU_ATTR_ALWAYS_INLINE static inline uint32_t osal_get_kernel_state()
{
    uint32_t state;
    BaseType_t xShedState;

#if configNUMBER_OF_CORES > 1
    xShedState = xTaskGetSchedulerStateFromISR();
#else
    xShedState = xTaskGetSchedulerState();
#endif

    switch(xShedState)
    {
        case taskSCHEDULER_RUNNING:
            state = OSAL_KERNEL_RUNNING;
            break;
        case taskSCHEDULER_NOT_STARTED:
            state = OSAL_KERNEL_NOT_STARTED;
            break;
        case taskSCHEDULER_SUSPENDED:
        default:
            state = OSAL_KERNEL_NOT_RUNNING;
            break;

    }
    return state;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_enter_critical()
{
    taskENTER_CRITICAL();
}

TU_ATTR_ALWAYS_INLINE static inline void osal_exit_critical()
{
    taskEXIT_CRITICAL();
}
//--------------------------------------------------------------------+
// DELAY API
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline void osal_delay_ms( uint64_t msec )
{
    vTaskDelay(pdMS_TO_TICKS(msec));
}

TU_ATTR_ALWAYS_INLINE static inline void osal_assert( bool expression )
{
    configASSERT(expression);
}

#ifdef __cplusplus
}
#endif

#endif
