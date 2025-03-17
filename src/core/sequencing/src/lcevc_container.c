/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#include "lcevc_container.h"

#include "predict_timehandle.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

// - StampedBuffer and StampedBufferList ----------------------------------------------------------

typedef struct StampedBuffer
{
    bool wasCopied;
    const uint8_t* buffer;
    size_t bufferSize;
    uint64_t timehandle;
    uint64_t inputTime;

    // This struct is also a linked-list node, but these two pointers are inaccessible by clients.
    struct StampedBuffer* previous;
    struct StampedBuffer* next;
} StampedBuffer_t;

typedef struct StampedBufferList
{
    StampedBuffer_t* head;
    size_t size;
    size_t capacity;
} StampedBufferList;

// External function

const uint8_t* stampedBufferGetBuffer(const StampedBuffer_t* stBuf)
{
    if (stBuf == NULL) {
        return NULL;
    }
    return stBuf->buffer;
}

size_t stampedBufferGetBufSize(const StampedBuffer_t* stBuf)
{
    if (stBuf == NULL) {
        return 0;
    }
    return stBuf->bufferSize;
}

uint64_t stampedBufferGetTimehandle(const StampedBuffer_t* stBuf)
{
    if (stBuf == NULL) {
        return kInvalidTimehandle;
    }
    return stBuf->timehandle;
}

uint64_t stampedBufferGetInputTime(const StampedBuffer_t* stBuf)
{
    if (stBuf == NULL) {
        return 0;
    }
    return stBuf->inputTime;
}

void stampedBufferRelease(StampedBuffer_t** buffer)
{
    if (buffer == NULL) {
        return;
    }
    if (*buffer != NULL && (*buffer)->wasCopied) {
        free((void*)(*buffer)->buffer);
    }
    free(*buffer);
    *buffer = NULL;
}

// Internal functions

static StampedBuffer_t* stampedBufferNodeAlloc(const uint8_t* data, size_t bufferSize,
                                               uint64_t timehandle, uint64_t inputTime, bool copy)
{
    StampedBuffer_t* newNode = calloc(1, sizeof(StampedBuffer_t));

    const uint8_t* newEntryData = (copy ? NULL : data);
    if (copy && bufferSize != 0 && data != NULL) {
        uint8_t* tempData = malloc(bufferSize);
        memcpy(tempData, data, bufferSize);
        newEntryData = tempData;
    }
    newNode->wasCopied = copy;
    newNode->buffer = newEntryData;
    newNode->bufferSize = bufferSize;
    newNode->inputTime = inputTime;
    newNode->timehandle = timehandle;
    newNode->next = NULL;
    newNode->previous = NULL;
    return newNode;
}

static bool stampedBufferListInsert(StampedBufferList* list, StampedBuffer_t* entry)
{
    if ((list == NULL) || (entry == NULL) || (list->size >= list->capacity)) {
        return false;
    }

    if (list->head == NULL) {
        list->head = entry;
        list->size++;
        return true;
    }

    const uint64_t timehandle = entry->timehandle;

    StampedBuffer_t* curPtr = list->head;
    size_t i = 0;
    while (i < list->capacity && curPtr != NULL) {
        const uint64_t curTH = curPtr->timehandle;

        // Reject duplicate timehandles first
        if (timehandle == curTH) {
            VN_SEQ_WARNING("Attempting to insert buffer with duplicate timehandle %" PRIu64 "\n", timehandle);
            return false;
        }

        if (timehandle < curTH) {
            entry->next = curPtr;
            entry->previous = curPtr->previous;
            if (entry->previous) {
                entry->previous->next = entry;
            } else {
                list->head = entry;
            }
            curPtr->previous = entry;

            list->size++;
            return true;
        }

        if (curPtr->next == NULL) {
            entry->previous = curPtr;
            curPtr->next = entry;

            list->size++;
            return true;
        }

        curPtr = curPtr->next;
        i++;
    }

    // This shouldn't be possible, because we checked the list size at the top of this function.
    // Somehow the size of the list has gotten out of sync, so repair it.
    VN_SEQ_ERROR(
        "Size out of sync! Claims to be %zu, but we're already full and capacity is %zu.\n",
        list->size, list->capacity);
    list->size = list->capacity;
    return false;
}

static bool stampedBufferAlloc(StampedBufferList* list, const uint8_t* data, size_t bufferSize,
                               uint64_t timehandle, uint64_t inputTime, bool copy)
{
    // List must be non-null. Data can only be null IF bufferSize is 0.
    if (list == NULL || (data == NULL && bufferSize != 0)) {
        return false;
    }

    StampedBuffer_t* newEntry = stampedBufferNodeAlloc(data, bufferSize, timehandle, inputTime, copy);
    if (!stampedBufferListInsert(list, newEntry)) {
        stampedBufferRelease(&newEntry);
        return false;
    }

    return true;
}

static const StampedBuffer_t* stampedBufferQuery(const StampedBufferList* list, uint64_t timehandle,
                                                 bool* isAtHeadOut)
{
    if (list == NULL) {
        return NULL;
    }
    const StampedBuffer_t* cur = list->head;
    *isAtHeadOut = false;

    while (cur) {
        if (cur->timehandle == timehandle) {
            *isAtHeadOut = (cur == list->head);
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

static StampedBuffer_t* stampedBufferExtract(StampedBufferList* list, uint64_t timehandle, bool* isAtHeadOut)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }
    *isAtHeadOut = false;

    StampedBuffer_t* cur = list->head;
    while (cur) {
        if (cur->timehandle == timehandle) {
            // Found the right entry
            *isAtHeadOut = (cur == list->head);

            if (cur->next) {
                cur->next->previous = cur->previous;
            }

            if (cur->previous) {
                if (cur == list->head) {
                    VN_SEQ_ERROR("Entry has a previous buffer, but is the head of our list. Head "
                                 "will be reset.\n");
                    list->head = cur->next;
                } else {
                    cur->previous->next = cur->next;
                }
            } else {
                list->head = cur->next;
            }

            list->size--;
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

static StampedBuffer_t* stampedBufferPopFront(StampedBufferList* list)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }
    bool dummyHeadOut = false;
    return stampedBufferExtract(list, list->head->timehandle, &dummyHeadOut);
}

static void stampedBufferListRelease(StampedBufferList* list)
{
    StampedBuffer_t* cur = (list ? list->head : NULL);
    if (cur == NULL) {
        return;
    }

    StampedBuffer_t* next = NULL;
    while (cur) {
        next = cur->next;
        stampedBufferRelease(&cur);
        cur = next;
    }

    list->size = 0;
    list->head = NULL;
}

// - LCEVCContainer -------------------------------------------------------------------------------

/// This struct uses the TimehandlePredictor struct to keep track of valid timehandles, and uses
/// list to hold them (list is a sorted doubly-linked list of `StampedBuffer_t`s)
/// NOTE: this is not threadsafe. Instead, the calling code should be threadsafe.
typedef struct LCEVCContainer
{
    TimehandlePredictor_t* predictor;
    StampedBufferList list;
    bool processedFirst;
} LCEVCContainer_t;

LCEVCContainer_t* lcevcContainerCreate(size_t capacity)
{
    if (capacity == 0) {
        capacity = SIZE_MAX;
    } else if (capacity == SIZE_MAX) {
        capacity = 0;
    }

    LCEVCContainer_t* newContainer = calloc(1, sizeof(LCEVCContainer_t));
    newContainer->predictor = timehandlePredictorCreate();
    newContainer->list.head = NULL;
    newContainer->list.size = 0;
    newContainer->list.capacity = capacity;

    newContainer->processedFirst = false;
    return newContainer;
}

void lcevcContainerDestroy(LCEVCContainer_t* container)
{
    timehandlePredictorDestroy(container->predictor);
    stampedBufferListRelease(&(container->list));
    free(container);
}

size_t lcevcContainerSize(const LCEVCContainer_t* container) { return container->list.size; }

size_t lcevcContainerCapacity(const LCEVCContainer_t* container)
{
    return container->list.capacity;
}

void lcevcContainerSetMaxNumReorderFrames(LCEVCContainer_t* container, uint32_t maxNumReorderFrames)
{
    timehandlePredictorSetMaxNumReorderFrames(container->predictor, maxNumReorderFrames);
    // As the predictor will be reset now we should give it a hint if we have any time handles
    // As the list is ordered we can use the head of the list to do the hinting
    if (container->list.head != NULL) {
        timehandlePredictorHint(container->predictor, container->list.head->timehandle);
    }
}

bool lcevcContainerInsert(LCEVCContainer_t* container, const uint8_t* data, uint32_t size,
                          uint64_t timehandle, uint64_t inputTime)
{
    // This will allocate the buffer itself, and fail if duplicate.
    bool ret = stampedBufferAlloc(&container->list, data, size, timehandle, inputTime, true);
    // Hint with the list head as that will be the smallest PTS
    if (container->list.head != NULL) {
        timehandlePredictorHint(container->predictor, container->list.head->timehandle);
    }
    timehandlePredictorFeed(container->predictor, timehandle);
    return ret;
}

bool lcevcContainerInsertNoCopy(LCEVCContainer_t* container, const uint8_t* data, uint32_t size,
                                uint64_t timehandle, uint64_t inputTime)
{
    // This will allocate the buffer itself, and fail if duplicate.
    bool ret = stampedBufferAlloc(&container->list, data, size, timehandle, inputTime, false);
    // Hint with the list head as that will be the smallest PTS
    if (container->list.head != NULL) {
        timehandlePredictorHint(container->predictor, container->list.head->timehandle);
    }
    timehandlePredictorFeed(container->predictor, timehandle);
    return ret;
}

bool lcevcContainerExists(const LCEVCContainer_t* container, uint64_t timehandle, bool* isAtHeadOut)
{
    return (stampedBufferQuery(&container->list, timehandle, isAtHeadOut) != NULL);
}

void lcevcContainerFlush(LCEVCContainer_t* container, uint64_t timehandle)
{
    bool dummyHeadOut = false;
    StampedBuffer_t* toRemove = stampedBufferExtract(&container->list, timehandle, &dummyHeadOut);
    stampedBufferRelease(&toRemove);
}

void lcevcContainerClear(LCEVCContainer_t* container)
{
    while (lcevcContainerSize(container) != 0) {
        StampedBuffer_t* toRelease = stampedBufferPopFront(&(container->list));
        stampedBufferRelease(&toRelease);
    }
    timehandlePredictorDestroy(container->predictor);
    container->predictor = timehandlePredictorCreate();
    container->processedFirst = false;
}

StampedBuffer_t* lcevcContainerExtract(LCEVCContainer_t* container, uint64_t timehandle, bool* isAtHeadOut)
{
    uint32_t count = 0;
    size_t dummyQSize = 0;
    uint64_t dummyTH = 0;
    StampedBuffer_t* currentHead =
        lcevcContainerExtractNextInOrder(container, true, &dummyTH, &dummyQSize);
    while (currentHead && currentHead->timehandle < timehandle) {
        stampedBufferRelease(&currentHead);
        currentHead = lcevcContainerExtractNextInOrder(container, true, &dummyTH, &dummyQSize);
        count++;
    }

    if (count > 1) {
        char timehandleString[128] = "unknown timehandle";
        timehandlePredictorPrintTimehandle(container->predictor, timehandleString, 128, timehandle);

        if (container->list.size == 0) {
            VN_SEQ_WARNING("Deleted the entire container in search of %s\n", timehandleString);
        } else {
            VN_SEQ_DEBUG("found %s. deleting %" PRIu32 " items out of %zu,\n", timehandleString,
                         count, container->list.size);
        }
    }

    // If we overshot without finding the requested timehandle, put the overshoot-entry back in the
    // list and set the return to NULL.
    if (currentHead != NULL && currentHead->timehandle != timehandle) {
        stampedBufferListInsert(&(container->list), currentHead);
        currentHead = NULL;
    }

    *isAtHeadOut = (count == 0);
    return currentHead;
}

StampedBuffer_t* lcevcContainerExtractNextInOrder(LCEVCContainer_t* container, bool force,
                                                  uint64_t* timehandleOut, size_t* queueSizeOut)
{
    *timehandleOut = kInvalidTimehandle;

    *queueSizeOut = lcevcContainerSize(container);
    if (*queueSizeOut == 0) {
        return NULL;
    }

    const uint64_t headTimehandle = container->list.head->timehandle;
    // Moving to here allows the top of the list to always hint even if it's not next
    timehandlePredictorHint(container->predictor, headTimehandle);
    if (!force && !timehandlePredictorIsNext(container->predictor, headTimehandle)) {
        return NULL;
    }

    bool isAtHead = false;
    StampedBuffer_t* result = stampedBufferExtract(&container->list, headTimehandle, &isAtHead);
    if (!container->processedFirst) {
        char thString[128] = "unknown timehandle";
        VN_SEQ_DEBUG("processing first lcevc block: %s. Force %d, queue size %zu.\n",
                     timehandlePredictorPrintTimehandle(container->predictor, thString, 128, headTimehandle),
                     force, container->list.size);
        container->processedFirst = true;
    }

    if (result == NULL) {
        VN_SEQ_ERROR("Couldn't find front but list isn't empty.\n");
    }

    if (!isAtHead) {
        VN_SEQ_ERROR("Head not at head\n");
    }

    *timehandleOut = headTimehandle;
    return result;
}

void lcevcContainerSetPrinter(LCEVCContainer_t* container, thPrinter printer)
{
    timehandlePredictorSetPrinter(container->predictor, printer);
}
