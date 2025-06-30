/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_DEQUE_H
#define VN_LCEVC_COMMON_DEQUE_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
//
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! @file
 * @brief A double ended queue that reallocates to grow.
 *
 * Not thread safe.
 */
typedef struct LdcDeque LdcDeque;

/*! Initialize a deque.
 *
 * Allocate internal buffers of given size, and sets to empty.
 *
 * @param[out] deque             The initialized deque.
 * @param[in]  capacity          Maximum number of elements + 1 - must be a power of 2.
 * @param[in]  elementSize       Size in bytes of each element.
 * @param[in]  allocator         The memory allocator to be used.
 */
void ldcDequeInitialize(LdcDeque* deque, uint32_t capacity, uint32_t elementSize,
                        LdcMemoryAllocator* allocator);

/*! Destroy a previously initialized deque.
 *
 * Free all associated memory.
 *
 * @param[in]  deque             An initialized deque to be destroyed.
 */
void ldcDequeDestroy(LdcDeque* deque);

/*! Push element onto back of deque
 *
 * @param[in]  deque             An initialized deque.
 * @param[in]  element           Pointer to `elementSize` bytes for pushed element.
 */
static inline void ldcDequeBackPush(LdcDeque* deque, const void* element);

/*! Pop element from back of deque
 *
 * @param[in]  deque             An initialized deque.
 * @param[out] element           Pointer to `elementSize` bytes for popped element.
 */
static inline bool ldcDequeBackPop(LdcDeque* deque, void* element);

/*! Push element onto front of deque
 *
 * @param[in]  deque            An initialized deque.
 * @param[in]  element          Pointer to `elementSize` bytes for pushed element.
 */
static inline void ldcDequeFrontPush(LdcDeque* deque, const void* element);

/*! Pop element from front of deque
 *
 * @param[in]  deque            An initialized deque.
 * @param[out] element          Pointer to `elementSize` bytes for popped element.
 */
static inline bool ldcDequeFrontPop(LdcDeque* deque, void* element);

/*! Get the number of element the buffer can hold before reallocating
 *
 * @param[in]  deque            An initialized deque.
 * @return                      The number of reserved elements.
 */
static inline uint32_t ldcDequeReserved(const LdcDeque* deque);

/*! Get the number of elements in the deque
 *
 * @param[in]  deque             An initialized deque.
 * @return                       The number of elements
 */
static inline uint32_t ldcDequeSize(const LdcDeque* deque);

/*! Test if the buffer is empty.`
 *
 * @param[in] deque              An initialized deque.
 * @return                       True if the buffer is empty.
 */
static inline bool ldcDequeIsEmpty(const LdcDeque* deque);

// Implementation
//
#include "detail/deque.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_DEQUE_H
