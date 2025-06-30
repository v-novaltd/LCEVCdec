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

#ifndef VN_LCEVC_COMMON_DETAIL_VECTOR_H
#define VN_LCEVC_COMMON_DETAIL_VECTOR_H

#include <assert.h>
#include <LCEVC/common/memory.h>
#include <stdint.h>
#include <string.h>

// The guts of the vector implementation
//
typedef struct LdcVector
{
    uint8_t* data;        // The vector contents
    uint32_t size;        // Number of elements in vector
    uint32_t elementSize; //
    uint32_t reserved;    //

    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation dataAllocation;
} LdcVector;

static inline uint32_t ldcVectorReserved(const LdcVector* vector) { return vector->reserved; }
static inline uint32_t ldcVectorSize(const LdcVector* vector) { return vector->size; }
static inline bool ldcVectorIsEmpty(const LdcVector* vector) { return vector->size == 0; }

// Going to add an element - grow reservation if needed
static inline void _ldcVectorGrow(LdcVector* vector)
{
    // Does table need expanding?
    if (vector->size >= vector->reserved) {
        // Double size of table
        vector->reserved *= 2;
        vector->data = VNReallocateArray(vector->allocator, &vector->dataAllocation, uint8_t,
                                         vector->reserved * vector->elementSize);
    }
}

static inline uint32_t ldcVectorAppend(LdcVector* vector, const void* element)
{
    assert(vector);
    _ldcVectorGrow(vector);

    memcpy(vector->data + (vector->size * vector->elementSize), element, vector->elementSize);

    return vector->size++;
}

static inline void* ldcVectorAt(const LdcVector* vector, uint32_t index)
{
    if (index >= vector->size) {
        return NULL;
    }

    return (void*)(vector->data + (index * vector->elementSize));
}

static inline void* ldcVectorAtEnd(const LdcVector* vector, uint32_t offset)
{
    if (offset >= vector->size) {
        return NULL;
    }

    return (void*)(vector->data + (((vector->size - 1) - offset) * vector->elementSize));
}

static inline void* ldcVectorFind(const LdcVector* vector, LdcVectorCompareFn compareFn, const void* other)
{
    // Perform a binary search
    uint32_t low = 0;
    uint32_t high = vector->size;

    while (low < high) {
        const uint32_t mid = low + (high - low) / 2;

        const int cmp = compareFn(other, (void*)(vector->data + (mid * vector->elementSize)));
        if (cmp == 0) {
            return vector->data + (mid * vector->elementSize);
        }
        if (cmp <= 0) {
            high = mid; // Move search towards the low half
        } else {
            low = mid + 1; // Move search towards the high half
        }
    }

    return NULL;
}

static inline int ldcVectorFindIdx(const LdcVector* vector, LdcVectorCompareFn compareFn, const void* other)
{
    const uint8_t* const ptr = (const uint8_t*)ldcVectorFind(vector, compareFn, other);
    if (!ptr) {
        return -1;
    }

    return (int)(ptr - vector->data) / vector->elementSize;
}

static inline void* ldcVectorFindUnordered(const LdcVector* vector, LdcVectorCompareFn compareFn,
                                           const void* other)
{
    // Perform a linear search to find the insertion index
    for (uint32_t idx = 0; idx < vector->size; ++idx) {
        void* const ptr = (void*)(vector->data + (idx * vector->elementSize));
        const int cmp = compareFn(ptr, other);
        if (cmp == 0)
            return ptr;
    }

    return NULL;
}

static inline void* ldcVectorInsert(LdcVector* vector, LdcVectorCompareFn compareFn, const void* element)
{
    _ldcVectorGrow(vector);

    // Perform a binary search to find the insertion index
    uint32_t low = 0;
    uint32_t high = vector->size;
    while (low < high) {
        const uint32_t mid = low + (high - low) / 2;
        if (compareFn(element, (void*)(vector->data + (mid * vector->elementSize))) < 0) {
            high = mid; // Move search towards the left half
        } else {
            low = mid + 1; // Move search towards the right half
        }
    }

    // `low` is now the index where the new element should be inserted
    uint8_t* const ptr = vector->data + (low * vector->elementSize);

    // Move rest of the vector up to make space for the new element
    if (low < vector->size) {
        memmove(ptr + vector->elementSize, ptr, (vector->size - low) * vector->elementSize);
    }

    vector->size++;
    memcpy(ptr, element, vector->elementSize);

    return ptr;
}

static inline void ldcVectorRemove(LdcVector* vector, void* element)
{
    uint8_t* ptr = (uint8_t*)element;
    const uint8_t* end = vector->data + (vector->size * vector->elementSize);

    assert(vector);
    assert(vector->size > 0);
    assert(vector->data <= ptr);
    assert(ptr < end);
    assert(((ptr - vector->data) % vector->elementSize == 0));

    // Move rest of vector down one slot
    const size_t s = (end - ptr) - vector->elementSize;
    if (s > 0) {
        memmove(ptr, ptr + vector->elementSize, s);
    }
    vector->size--;
}

static inline void ldcVectorRemoveIdx(LdcVector* vector, uint32_t index)
{
    uint8_t* ptr = vector->data + (index * vector->elementSize);
    const uint8_t* end = vector->data + (vector->size * vector->elementSize);

    assert(vector);
    assert(vector->size > 0);
    assert(vector->data <= ptr);
    assert(ptr < end);
    assert(((ptr - vector->data) % vector->elementSize == 0));

    // Move rest of vector down one slot
    const size_t s = (end - ptr) - vector->elementSize;
    if (s > 0) {
        memmove(ptr, ptr + vector->elementSize, s);
    }
    vector->size--;
}

static inline void ldcVectorRemoveReorder(LdcVector* vector, void* element)
{
    uint8_t* ptr = (uint8_t*)element;
    const uint8_t* end = vector->data + (vector->size * vector->elementSize);

    assert(vector);
    assert(vector->size > 0);
    assert(vector->data <= ptr);
    assert(ptr < end);
    assert(((ptr - vector->data) % vector->elementSize == 0));

    // Move last element of vector to this slot
    const uint8_t* last = end - vector->elementSize;
    if (ptr < last) {
        memcpy(ptr, last, vector->elementSize);
    }
    vector->size--;
}

static inline void ldcVectorRemoveReorderIdx(LdcVector* vector, uint32_t index)
{
    uint8_t* ptr = vector->data + (index * vector->elementSize);
    const uint8_t* end = vector->data + (vector->size * vector->elementSize);

    assert(vector);
    assert(vector->size > 0);
    assert(vector->data <= ptr);
    assert(ptr < end);
    assert(((ptr - vector->data) % vector->elementSize == 0));

    // Move last element of vector to this slot
    const uint8_t* last = end - vector->elementSize;
    if (ptr < last) {
        memcpy(ptr, last, vector->elementSize);
    }
    vector->size--;
}

static inline int ldcVectorCompareAllocationPtr(const void* element, const void* ptr)
{
    const LdcMemoryAllocation* alloc = (const LdcMemoryAllocation*)element;

    if (alloc->ptr < ptr) {
        return -1;
    }

    if (alloc->ptr > ptr) {
        return 1;
    }

    return 0;
}
#endif // VN_LCEVC_COMMON_DETAIL_VECTOR_H
