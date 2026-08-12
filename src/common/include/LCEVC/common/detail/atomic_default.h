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

#ifndef VN_LCEVC_COMMON_DETAIL_ATOMIC_DEFAULT_H
#define VN_LCEVC_COMMON_DETAIL_ATOMIC_DEFAULT_H

#ifdef __cplusplus
#include <atomic>
#include <cstddef>
#include <cstdint>
#else
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
struct LdcAtomicSizeT
{
    std::atomic_size_t atomic;
};
struct LdcAtomicUint
{
    std::atomic_uint atomic;
};
static_assert(sizeof(LdcAtomicSizeT) == sizeof(size_t), "atomic uint size mismatch");
static_assert(alignof(LdcAtomicUint) == alignof(unsigned int), "atomic uint alignment mismatch");
using std::memory_order_seq_cst;
#else
struct LdcAtomicSizeT
{
    atomic_size_t atomic;
};
struct LdcAtomicUint
{
    atomic_uint atomic;
};
#endif

// Fetch sub

static inline uint32_t atomicFetchSubU32(LdcAtomicUint* atomic, uint32_t val)
{
    return atomic_fetch_sub_explicit(&atomic->atomic, val, memory_order_seq_cst);
}

static inline size_t atomicFetchSubSizeT(LdcAtomicSizeT* atomic, size_t val)
{
    return atomic_fetch_sub_explicit(&atomic->atomic, val, memory_order_seq_cst);
}

// Fetch add

static inline uint32_t atomicFetchAddU32(LdcAtomicUint* atomic, uint32_t val)
{
    return atomic_fetch_add_explicit(&atomic->atomic, val, memory_order_seq_cst);
}

static inline size_t atomicFetchAddSizeT(LdcAtomicSizeT* atomic, size_t val)
{
    return atomic_fetch_add_explicit(&atomic->atomic, val, memory_order_seq_cst);
}

// Store

static inline void atomicStoreU32(LdcAtomicUint* atomic, uint32_t val)
{
    atomic_store_explicit(&atomic->atomic, val, memory_order_seq_cst);
}

static inline void atomicStoreSizeT(LdcAtomicSizeT* atomic, size_t val)
{
    atomic_store_explicit(&atomic->atomic, val, memory_order_seq_cst);
}

// Load

static inline uint32_t atomicLoadU32(LdcAtomicUint* atomic)
{
    return atomic_load_explicit(&atomic->atomic, memory_order_seq_cst);
}

static inline size_t atomicLoadSizeT(LdcAtomicSizeT* atomic)
{
    return atomic_load_explicit(&atomic->atomic, memory_order_seq_cst);
}

#endif // VN_LCEVC_COMMON_DETAIL_ATOMIC_DEFAULT_H
