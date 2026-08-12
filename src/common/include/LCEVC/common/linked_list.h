/* Copyright (c) V-Nova International Limited 2023-2026. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_LINKED_LIST_H
#define VN_LCEVC_COMMON_LINKED_LIST_H

#include <LCEVC/common/stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

// A very simple doubly linked list implementation.
//
// Uses the convention that the node pointers are always non-null, and the `LdcLinkedList` structure
// acts as two overlapping sentinel nodes. (c.f. Amiga Exec)
//
typedef struct LdcLinkedNode
{
    // Never null - end of list will point at the `tail` member of DllList
    struct LdcLinkedNode* next;
    // Never null - end of list will point at the `head` member of DllList
    struct LdcLinkedNode* prev;
} LdcLinkedNode;

// List anchor
//
typedef struct LdcLinkedList
{
    // Pointer to head node. This and `tail` act as the head sentinel node
    struct LdcLinkedNode* head;
    // Always NULL, this and `tailPrev` act as the tail sentinel node
    struct LdcLinkedNode* tail;
    // Pointer to the node before the tail sentinel
    struct LdcLinkedNode* tailPrev;
} LdcLinkedList;

/*! Initialize a list as empty.
 *
 * The head and tail sentinel nodes are linked to each other.
 */
static inline void ldcLinkedListInitialize(LdcLinkedList* list)
{
    assert(list);
    list->head = (LdcLinkedNode*)&list->tail;
    list->tail = NULL;
    list->tailPrev = (LdcLinkedNode*)&list->head;
}

/*! Initialize a node as unlinked
 *
 * The head and tail sentinel nodes are linked to each other.
 */
static inline void ldcLinkedNodeInitialize(LdcLinkedNode* node)
{
    assert(node);
    node->next = NULL;
    node->prev = NULL;
}

/*! Insert `nodeNew` into the list immediately after `nodeAt`.
 */
static inline void ldcLinkedInsertAfter(LdcLinkedNode* nodeAt, LdcLinkedNode* nodeNew)
{
    assert(nodeAt);
    assert(nodeNew);
    LdcLinkedNode* const next = nodeAt->next;
    assert(next);
    nodeNew->prev = nodeAt;
    nodeNew->next = next;
    nodeAt->next = nodeNew;
    next->prev = nodeNew;
}

/*! Insert `nodeNew` into the list immediately before `nodeAt`.
 */
static inline void ldcLinkedInsertBefore(LdcLinkedNode* nodeAt, LdcLinkedNode* nodeNew)
{
    assert(nodeAt);
    assert(nodeNew);
    LdcLinkedNode* const prev = nodeAt->prev;
    assert(prev);
    nodeNew->next = nodeAt;
    nodeNew->prev = prev;
    prev->next = nodeNew;
    nodeAt->prev = nodeNew;
}

/*! Remove a node from the list it belongs to.
 */
static inline void ldcLinkedRemove(LdcLinkedNode* node)
{
    LdcLinkedNode* const next = node->next;
    LdcLinkedNode* const prev = node->prev;
    assert(next);
    assert(prev);

    prev->next = next;
    next->prev = prev;

    node->next = NULL;
    node->prev = NULL;
}

/*! Add `node` to the tail of the list.
 */
static inline void ldcLinkedPushBack(LdcLinkedList* list, LdcLinkedNode* node)
{
    LdcLinkedNode* const tailSentinel = (LdcLinkedNode*)&list->tail;
    ldcLinkedInsertAfter(tailSentinel->prev, node);
}

/*! Add `node` to the head of the list.
 */
static inline void ldcLinkedPushFront(LdcLinkedList* list, LdcLinkedNode* node)
{
    LdcLinkedNode* const headSentinel = (LdcLinkedNode*)&list->head;
    ldcLinkedInsertAfter(headSentinel, node);
}

/*! Get node at front of the list.
 *
 * @return: node at front of list, or NULL if list is empty.
 */
static inline LdcLinkedNode* ldcLinkedFront(const LdcLinkedList* list)
{
    LdcLinkedNode* const head = list->head;
    LdcLinkedNode* const tailSentinel = (LdcLinkedNode*)&list->tail;
    return (head == tailSentinel) ? NULL : head;
}

/*! Get node at back of the list.
 *
 * @return: node at back of list, or NULL if list is empty.
 */
static inline LdcLinkedNode* ldcLinkedBack(const LdcLinkedList* list)
{
    LdcLinkedNode* const tailPrev = list->tailPrev;
    LdcLinkedNode* const headSentinel = (LdcLinkedNode*)&list->head;
    return (tailPrev == headSentinel) ? NULL : tailPrev;
}

/*! Remove node from the front of the list
 *
 */
static inline void ldcLinkedPopFront(LdcLinkedList* list)
{
    LdcLinkedNode* const front = ldcLinkedFront(list);
    if (front != NULL) {
        ldcLinkedRemove(front);
    }
}

/*! Remove node from the back of the list
 *
 */
static inline void ldcLinkedPopBack(LdcLinkedList* list)
{
    LdcLinkedNode* const back = ldcLinkedBack(list);
    if (back != NULL) {
        ldcLinkedRemove(back);
    }
}

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_LINKED_LIST_H
