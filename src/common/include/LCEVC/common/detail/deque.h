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

#ifndef VN_LCEVC_COMMON_DETAIL_DEQUE_H
#define VN_LCEVC_COMMON_DETAIL_DEQUE_H

#include <assert.h>
#include <LCEVC/common/memory.h>

// The guts of the deque implementation.
//
// This is a ring buffer that is grown on demand.
//
struct LdcDeque
{
    uint8_t* data;     // The element ring
    uint32_t reserved; // Number of records allocated for deque - should be a power of 2 (actual capacity is 1 less to keep in != out)
    uint32_t mask;     // Mask to bring offset into range

    uint32_t elementSize; // Size in bytes of each element in deque

    uint32_t front; // Front slot index
    uint32_t back;  // Back slot index

    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation dataAllocation;
};

// Out of line code to double the deque capacity
void ldcDequeGrow(LdcDeque* deque);

static inline bool ldcDequeIsFull(LdcDeque* deque)
{
    return ((deque->back + 1) & deque->mask) == deque->front;
}

static inline uint32_t ldcDequeReserved(const LdcDeque* deque)
{
    // Actual capacity is 1 less than storage (in==out can't be distinguished)
    return deque->reserved - 1;
}

static inline uint32_t ldcDequeSize(const LdcDeque* deque)
{
    return (deque->reserved + deque->back - deque->front) & deque->mask;
}

static inline bool ldcDequeIsEmpty(const LdcDeque* deque)
{
    //
    return deque->front == deque->back;
}

static inline void ldcDequeBackPush(LdcDeque* deque, const void* element)
{
    // Check capacity
    //
    if (ldcDequeIsFull(deque)) {
        ldcDequeGrow(deque);
        assert(!ldcDequeIsFull(deque));
    }

    memcpy(deque->data + deque->back * deque->elementSize, element, deque->elementSize);

    // Move back up
    deque->back = (deque->back + 1) & deque->mask;
}

static inline bool ldcDequeBackPop(LdcDeque* deque, void* element)
{
    if (ldcDequeIsEmpty(deque)) {
        return false;
    }

    // Move back down
    deque->back = (deque->back + deque->reserved - 1) & deque->mask;
    memcpy(element, deque->data + deque->back * deque->elementSize, deque->elementSize);
    return true;
}

static inline void ldcDequeFrontPush(LdcDeque* deque, const void* element)
{
    // Check capacity
    if (ldcDequeIsFull(deque)) {
        ldcDequeGrow(deque);
        assert(!ldcDequeIsFull(deque));
    }

    // Move front down
    deque->front = (deque->front + deque->reserved - 1) & deque->mask;

    memcpy(deque->data + deque->front * deque->elementSize, element, deque->elementSize);
}

static inline bool ldcDequeFrontPop(LdcDeque* deque, void* element)
{
    if (ldcDequeIsEmpty(deque)) {
        return false;
    }

    memcpy(element, deque->data + deque->front * deque->elementSize, deque->elementSize);

    // Move front up
    deque->front = (deque->front + 1) & deque->mask;
    return true;
}

#endif // VN_LCEVC_COMMON_DETAIL_DEQUE_H
