/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_LCEVC_COMMON_RING_BUFFER_H
#define VN_LCEVC_COMMON_RING_BUFFER_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/threads.h>
//
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! @file
 * @brief A thread safe ring buffer.
 */
typedef struct LdcRingBuffer LdcRingBuffer;

/*! Initialize a ring buffer.
 *
 * Allocate internal buffers of given size, and sets to empty.
 *
 * @param[out] ringBuffer        The initialized ring buffer.
 * @param[in]  capacity          Maximum number of elements + 1 - must be a power of 2.
 * @param[in]  elementSize       Size in bytes of each element.
 * @param[in]  allocator         The memory allocator to be used.
 */
void ldcRingBufferInitialize(LdcRingBuffer* ringBuffer, uint32_t capacity, uint32_t elementSize,
                             LdcMemoryAllocator* allocator);

/*! Destroy a previously initialized ring buffer.
 *
 * Free all associated memory. Any pending records will be lost.
 *
 * @param[in] ringBuffer        An initialized ring buffer to be destroyed.
 */
void ldcRingBufferDestroy(LdcRingBuffer* ringBuffer);

/*! Push element into ring buffer
 *
 * Will block until there is space in the buffer.
 *
 * @param[in] ringBuffer        An initialized ring buffer.
 * @param[in] element           Pointer to `elementSize` bytes for element.
 */
static inline void ldcRingBufferPush(LdcRingBuffer* ringBuffer, const void* element);

/*! Try to push data into ring buffer
 *
 * If ring buffer is full, return false, otherwise add an element to the buffer.
 * Will not block.
 *
 * @param[in] ringBuffer        An initialized ring buffer.
 * @param[in] element           Pointer to `elementSize` bytes for element.
 * @return                      True if the element could be pushed.
 */
static inline bool ldcRingBufferTryPush(LdcRingBuffer* ringBuffer, const void* element);

/*! Pop element out of ring buffer.
 *
 * Will block if there are no elements in the buffer.
 *
 * @param[in]  ringBuffer        An initialized ring buffer.
 * @param[out] element           Pointer to `elementSize` bytes for popped element.
 */
static inline void ldcRingBufferPop(LdcRingBuffer* ringBuffer, void* element);

/*! Try to pop data from ring buffer
 *
 * If ring buffer is empty, return false, otherwise remove an element from the buffer.
 * Will not block.
 *
 * @param[in]  ringBuffer       An initialized ring buffer.
 * @param[out] element          Pointer to `elementSize` bytes for element.
 * @return                      True if the element could be popped.
 */
static inline bool ldcRingBufferTryPop(LdcRingBuffer* ringBuffer, void* element);

/*! Get the maximum number of element the buffer can hold.
 *
 * @param[in] ringBuffer        An initialized ring buffer.
 * @return                      The maximum number of elements.
 */
static inline uint32_t ldcRingBufferCapacity(const LdcRingBuffer* ringBuffer);

/*! Get the number of elements in buffer.
 *
 * @param[in] ringBuffer        An initialized ring buffer.
 * @return                      The number of element in the buffer.
 */
static inline uint32_t ldcRingBufferSize(LdcRingBuffer* ringBuffer);

/*! Test if the buffer is empty.`
 *
 * @param[in] ringBuffer        An initialized ring buffer.
 * @return                      True if the buffer is empty.
 */
static inline bool ldcRingBufferIsEmpty(LdcRingBuffer* ringBuffer);

/*! Test if the buffer is full.
 *
 * @param[in] ringBuffer        An initialized ring buffer.
 * @return                      True if the buffer is full.
 */
static inline bool ldcRingBufferIsFull(LdcRingBuffer* ringBuffer);

// Implementation
//
#include "detail/ring_buffer.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_RING_BUFFER_H
