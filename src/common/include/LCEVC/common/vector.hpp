/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova International Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_LCEVC_COMMON_VECTOR_HPP
#define VN_LCEVC_COMMON_VECTOR_HPP

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/vector.h>
#include <LCEVC/common/memory.h>

namespace lcevc_dec::common {

// Type templated C++ wrapper for LdcVector
//
template<typename T>
class Vector {
public:
    explicit Vector(uint32_t reserved, LdcMemoryAllocator* allocator) {
        ldcVectorInitialize(&m_vector, sizeof(T), reserved, allocator);
    }
    explicit Vector(uint32_t reserved) {
        ldcVectorInitialize(&m_vector, sizeof(T), reserved, ldcMemoryAllocatorMalloc());
    }
    ~Vector() {
        ldcVectorDestroy(&m_vector);
    }

    uint32_t reserved() const { return ldcVectorReserved(&m_vector); }
    uint32_t size() const { return ldcVectorSize(&m_vector); }
    bool isEmpty() const { return ldcVectorIsEmpty(&m_vector); }

    void remove(T* element) { ldcVectorRemove(&m_vector, static_cast<void *>(element)); }
    void removeIndex(uint32_t idx) { ldcVectorRemoveIdx(&m_vector, idx); }

    void removeReorder(T* element) { ldcVectorRemoveReorder(&m_vector, static_cast<void *>(element)); }
    void removeReorderIndex(uint32_t idx) { ldcVectorRemoveReorderIdx(&m_vector, idx); }

    T* at(uint32_t idx) { return static_cast<T*>(ldcVectorAt(&m_vector, idx)); }
    const T* at(uint32_t idx) const { return static_cast<const T*>(ldcVectorAt(&m_vector, idx)); }

    T& operator[](uint32_t idx) { assert(idx < size()); return *(static_cast<T*>(ldcVectorAt(&m_vector, idx))); }
    const T& operator[](uint32_t idx) const { assert(idx < size()); return *(static_cast<const T*>(ldcVectorAt(&m_vector, idx))); }

    T* find(LdcVectorCompareFn compareFn, const void* other) { return static_cast<T*>(ldcVectorFind(&m_vector, compareFn, other)); }
    const T* find(LdcVectorCompareFn compareFn, const void* other) const { return static_cast<const T*>(ldcVectorFind(&m_vector, compareFn, other)); }

    T* findUnordered(LdcVectorCompareFn compareFn, const void* other) { return static_cast<T*>(ldcVectorFindUnordered(&m_vector, compareFn, other)); }
    const T* findUnordered(LdcVectorCompareFn compareFn, const void* other) const { return static_cast<const T*>(ldcVectorFindUnordered(&m_vector, compareFn, other)); }

    int findIndex(LdcVectorCompareFn compareFn, const void* other) const { return ldcVectorFindIdx(&m_vector, compareFn, other); }

    void append(const T& element) { ldcVectorAppend(&m_vector, &element); }
    T* insert(LdcVectorCompareFn compareFn, const T& element) { return static_cast<T*>(ldcVectorInsert(&m_vector, compareFn, static_cast<const void *>(&element))); }

    VNNoCopyNoMove(Vector);
private:
    LdcVector m_vector{};
};

} // namespace lcevc_dec::common

#endif // VN_LCEVC_COMMON_VECTOR_H
