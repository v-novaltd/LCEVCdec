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

#ifndef VN_LCEVC_COMMON_DIAGNOSTICS_BUFFER_H
#define VN_LCEVC_COMMON_DIAGNOSTICS_BUFFER_H

/*! @file
 *  @brief Specialized ring buffer for diagnostics.
 *
 * Made up of a ring buffer of DiagRecords, and an associated  buffer of variable
 * sized blobs of bytes. The offsets into the variable size buffer are managed
 * such that the blobs remain contiguous.
 */
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct LdcDiagnosticsBuffer LdcDiagnosticsBuffer;

/*! Initialize a diagnostics buffer - allocate buffers of given sizes.
 *
 * @param[out] buffer            The initialized diagnostic buffer.
 * @param[in]  capacity          Maximum number of records in buffer +1  - must be a power of 2.
 * @param[in]  varDataCapacity   Total size of variable data buffer - must be a power of 2.
 * @param[in]  allocator         The memory allocator to be used.
 */
void ldcDiagnosticsBufferInitialize(LdcDiagnosticsBuffer* buffer, uint32_t capacity,
                                    uint32_t varDataCapacity, LdcMemoryAllocator* allocator);

/*! Destroy a previously initialized diagnostics diagnostic buffer.
 *
 * Free all associated memory. Any pending records will be lost.
 *
 * @param[in]  buffer            An initialized diagnostics buffer to be destroyed.
 */
void ldcDiagnosticsBufferDestroy(LdcDiagnosticsBuffer* buffer);

/*! Push record into diagnostic buffer
 *
 * Will block until there is space in the buffer.
 *
 * @param[in]  buffer            An initialized diagnostic buffer.
 * @param[in]  diagRecord        Record to push to.
 * @param[in]  varData           Data to push.
 * @param[in]  varSize           Data size.
 */
static inline void ldcDiagnosticsBufferPush(LdcDiagnosticsBuffer* buffer, const LdcDiagRecord* diagRecord,
                                            const uint8_t* varData, size_t varSize);

/*! Pop element out of diagnostic buffer.
 *
 * Will block if there are no elements in the buffer.
 *
 * @param[in]  buffer            An initialized diagnostic buffer.
 * @param[in]  diagRecord        Record to retrieve from.
 * @param[out] varData           Output buffer to write to.
 * @param[in]  varMaxSize        Max size of `varData`.
 * @param[in]  varSize           Actual output size.
 * @return                       True if buffer was emptied by this pop
 */
static inline bool ldcDiagnosticsBufferPop(LdcDiagnosticsBuffer* buffer, LdcDiagRecord* diagRecord,
                                           uint8_t* varData, size_t varMaxSize, size_t* varSize);

/*! Get the maximum number of element the buffer can hold.
 *
 * @param[in]  buffer            An initialized diagnostic buffer.
 * @return                       The maximum number of elements.
 */
static inline uint32_t ldcDiagnosticsBufferCapacity(const LdcDiagnosticsBuffer* buffer);

/*! Get the number of elements in buffer.
 *
 * @param[in]  buffer            An initialized diagnostic buffer.
 * @return                       The number of element in the buffer.
 */
static inline uint32_t ldcDiagnosticsBufferSize(LdcDiagnosticsBuffer* buffer);

/*! Test if the buffer is empty.
 *
 * @param[in]  buffer            An initialized diagnostic buffer.
 * @return                       True if the buffer is empty.
 */
static inline bool ldcDiagnosticsBufferIsEmpty(LdcDiagnosticsBuffer* buffer);

/*! Test if the buffer is full.
 *
 * @param[in]  buffer            An initialized diagnostic buffer.
 * @return                       True if the buffer is full.
 */
static inline bool ldcDiagnosticsBufferIsFull(LdcDiagnosticsBuffer* buffer);

/* Split push operation - to minimize copies in logging
 *
 * diagnosticsBufferPushBegin() does first half of push and returns pointer to record to be filled.
 * diagnosticsBufferPushPop()  completes the push.
 */
static inline LdcDiagRecord* ldcDiagnosticsBufferPushBegin(LdcDiagnosticsBuffer* buffer, size_t varSize);
static inline void ldcDiagnosticsBufferPushEnd(LdcDiagnosticsBuffer* buffer);

// Implementation
//
#include "detail/diagnostics_buffer.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_DIAGNOSTICS_BUFFER_H
