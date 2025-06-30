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

#ifndef VN_LCEVC_COMMON_VECTOR_H
#define VN_LCEVC_COMMON_VECTOR_H

#include <LCEVC/common/memory.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! @file
 * @brief A vector
 */
typedef struct LdcVector LdcVector;

/*! Comparison function for insert & find
 */
typedef int (*LdcVectorCompareFn)(const void* element, const void* other);

/*! Initialize a vector
 *
 * Allocate initial vector of given size, and sets to empty.
 *
 * @param[out] vector           The initialized vector.
 * @param[in] elementSize       Size in bytes of each element.
 * @param[in] reserved          Initial number of reserved elements.
 * @param[in] allocator         The memory allocator to be used.
 */
void ldcVectorInitialize(LdcVector* vector, uint32_t elementSize, uint32_t reserved,
                         LdcMemoryAllocator* allocator);

/*! Destroy a previously initialized vector.
 *
 * Free all associated memory.
 *
 * @param[in] vector            An initialized vector to be destroyed.
 */
void ldcVectorDestroy(LdcVector* vector);

/*! Get the number of element slots that have been reserved
 *
 * @param[in] vector            An initialized vector.
 * @return                      The maximum number of elements.
 */
static inline uint32_t ldcVectorReserved(const LdcVector* vector);

/*! Get the number of elements in the vector.
 *
 * @param[in] vector            An initialized vector.
 * @return                      The number of element in the vector.
 */
static inline uint32_t ldcVectorSize(const LdcVector* vector);

/*! Test if the vector is empty.`
 *
 * @param[in] vector            An initialized vector.
 * @return                      True if the vector is empty.
 */
static inline bool ldcVectorIsEmpty(const LdcVector* vector);

/*! Append element onto vector
 *
 * @param[in] vector            An initialized vector.
 * @param[in] element           Pointer to `elementSize` bytes for element.
 * @return                      Index of new element in vector
 */
static inline uint32_t ldcVectorAppend(LdcVector* vector, const void* element);

/*! Remove element from vector by pointer, preserving order
 *
 * The addresses of remaining elements may change.
 *
 * @param[in] vector            An initialized vector.
 * @param[in] element           Pointer to element within vector to remove
 */
static inline void ldcVectorRemove(LdcVector* vector, void* element);

/*! Remove element from vector by index, preserving order
 *
 * The addresses of remaining elements may change.
 *
 * @param[in] vector            An initialized vector.
 * @param[in] index             Index of element within vector to remove
 */
static inline void ldcVectorRemoveIdx(LdcVector* vector, uint32_t index);

/*! Remove element from vector, possibly reordering contents
 *
 * The addresses of remaining elements will not change
 *
 * @param[in] vector            An initialized vector.
 * @param[in] element           Element  within vector to remove
 */
static inline void ldcVectorRemoveReorder(LdcVector* vector, void* element);

/*! Remove element from vector, possibly reordering contents
 *
 * The addresses of remaining elements will not change
 *
 * @param[in] vector            An initialized vector.
 * @param[in] index             Index of element within vector to remove
 */
static inline void ldcVectorRemoveReorderIdx(LdcVector* vector, uint32_t index);

/*! Get pointer to indexed element of vector
 *
 * @param[in] vector            An initialized vector.
 * @param[in] index             Index of element from start vector
 * @return                      Pointer to indexed element, or NULL if index .ge. size.
 */
static inline void* ldcVectorAt(const LdcVector* vector, uint32_t index);

/*! Get pointer to an element of a vector by offset, counting from the end of the vector
 *
 * @param[in] vector            An initialized vector.
 * @param[in] offset            Offset of element from end of within vector
 * @return                      Pointer to indexed element, or NULL if index .ge. size.
 */
static inline void* ldcVectorAtEnd(const LdcVector* vector, uint32_t offset);

/*! Get pointer to element in a sorted vector that matches a comparison function
 *
 * This used a binary search.
 *
 * @param[in] vector            An initialized vector.
 * @param[in] compareFn         Pointer function that provides a comparison between elements <0, 0 or >0.
 * @param[in] other             The second argument to the comparison function.
 * @return                      Pointer to indexed element, or NULL if index .ge. size.
 */
static inline void* ldcVectorFind(const LdcVector* vector, LdcVectorCompareFn compareFn, const void* other);

/*! Get index to element in a sorted vector that matches a comparison function
 *
 * This used a binary search.
 *
 * @param[in] vector            An initialized vector.
 * @param[in] compareFn         Pointer function that provides a comparison between elements <0, 0 or >0.
 * @param[in] other             The second argument to the comparison function.
 * @return                      index of element, or -1 if not found
 */
static inline int ldcVectorFindIdx(const LdcVector* vector, LdcVectorCompareFn compareFn, const void* other);

/*! Get pointer to first element in an unsorted vector that matches a comparison function
 *
 * This used a linear search.
 *
 * @param[in] vector            An initialized vector.
 * @param[in] compareFn         Pointer function that provides a comparison between elements <0, 0 or >0.
 * @param[in] other             The second argument to the comparison function.
 * @return                      Pointer to indexed element, or NULL if index .ge. size.
 */
static inline void* ldcVectorFindUnordered(const LdcVector* vector, LdcVectorCompareFn compareFn,
                                           const void* other);

/*! Insert element into vector
 *
 * @param[in] vector            An initialized vector.
 * @param[in] compareFn         Pointer function that provides a comparison between elements <0, 0 or >0
 * @param[in] element           Pointer to `elementSize` bytes of element ot be inserted.
 * @return                      Pointer to indexed element, or NULL if index .ge. size.
 */
static inline void* ldcVectorInsert(LdcVector* vector, LdcVectorCompareFn compareFn, const void* element);

/*! Fund function that compares a given pointer with an element that is an LdcMemoryAllocation
 *
 * @param[in] element           Pointer to LdcMemoryAllocation
 * @param[in] ptr               Pointer that is compared to the `ptr` in `element`
 *
 * @return                      Result of comparison: -1, 0, 1, for <, == or >
 */
static inline int ldcVectorCompareAllocationPtr(const void* element, const void* ptr);

// Implementation
//
#include "detail/vector.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_VECTOR_H
