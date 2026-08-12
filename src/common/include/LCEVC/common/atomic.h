/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_ATOMIC_H
#define VN_LCEVC_COMMON_ATOMIC_H

#include <LCEVC/common/stdlib.h>

/*! @file
 * @brief Atomic utilities.
 *
 * A custom implementation can be provided by defining VN_SDK_ATOMIC_CUSTOM, and providing
 * `atomic_custom.h` in the include path. The custom header should provide the static inline
 * implementations declared below.
 */

// Opaque atomic types
typedef struct LdcAtomicUint LdcAtomicUint;
typedef struct LdcAtomicSizeT LdcAtomicSizeT;

// Fetch sub
static inline uint32_t atomicFetchSubU32(LdcAtomicUint* atomic, uint32_t val);
static inline size_t atomicFetchSubSizeT(LdcAtomicSizeT* atomic, size_t val);

// Fetch add
static inline uint32_t atomicFetchAddU32(LdcAtomicUint* atomic, uint32_t val);
static inline size_t atomicFetchAddSizeT(LdcAtomicSizeT* atomic, size_t val);

// Store
static inline void atomicStoreU32(LdcAtomicUint* atomic, uint32_t val);
static inline void atomicStoreSizeT(LdcAtomicSizeT* atomic, size_t val);

// Load
static inline uint32_t atomicLoadU32(LdcAtomicUint* atomic);
static inline size_t atomicLoadSizeT(LdcAtomicSizeT* atomic);

#if VN_SDK_FEATURE(ATOMIC_CUSTOM)
#include "atomic_custom.h"
#else
#include "detail/atomic_default.h"
#endif

#endif // VN_LCEVC_COMMON_ATOMIC_H
