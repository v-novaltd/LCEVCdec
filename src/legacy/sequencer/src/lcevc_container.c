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

#include "predict_timestamp.h"

#include <inttypes.h>
#include <LCEVC/common/log.h>
#include <LCEVC/sequencer/lcevc_container.h>
#include <stdlib.h>
#include <string.h>

// - StampedBuffer and StampedBufferList ----------------------------------------------------------

typedef struct StampedBuffer
{
    bool wasCopied;
    const uint8_t* buffer;
    size_t bufferSize;
    uint64_t timestamp;
    uint64_t inputTime;
    bool idr;

    // This struct is also a linked-list node, but these two pointers are inaccessible by clients.
    struct StampedBuffer* previous;
    struct StampedBuffer* next;
} StampedBuffer;

typedef struct StampedBufferList
{
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation* allocation;
    StampedBuffer* head;
    size_t size;
    size_t capacity;
} StampedBufferList;

// Internal types for unencapsulation
typedef enum LdeNALType
{
    Error = 0,
    NonIDR = 28,
    IDR = 29,
} LdeNALType;

// External function

const uint8_t* stampedBufferGetBuffer(const StampedBuffer* stBuf)
{
    if (stBuf == NULL) {
        return NULL;
    }
    return stBuf->buffer;
}

size_t stampedBufferGetBufSize(const StampedBuffer* stBuf)
{
    if (stBuf == NULL) {
        return 0;
    }
    return stBuf->bufferSize;
}

uint64_t stampedBufferGetTimestamp(const StampedBuffer* stBuf)
{
    if (stBuf == NULL) {
        return kInvalidTimestamp;
    }
    return stBuf->timestamp;
}

uint64_t stampedBufferGetInsertTime(const StampedBuffer* stBuf)
{
    if (stBuf == NULL) {
        return 0;
    }
    return stBuf->inputTime;
}

bool stampedBufferGetIDR(const StampedBuffer* stBuf)
{
    if (stBuf == NULL) {
        return false;
    }
    return stBuf->idr;
}

void stampedBufferRelease(StampedBuffer** stBuf)
{
    if (stBuf == NULL) {
        return;
    }
    if (*stBuf != NULL && (*stBuf)->wasCopied) {
        free((void*)(*stBuf)->buffer);
    }
    free(*stBuf);
    *stBuf = NULL;
}

bool lcevcContainerUnencapsulate(const uint8_t* encapsulatedData, size_t encapsulatedSize,
                                 uint8_t* unencapsulatedBuffer, size_t* unencapsulatedSize, bool* isIDR)
{
    uint8_t* head = unencapsulatedBuffer;
    uint8_t zeroes = 0;
    uint8_t byte = 0;
    size_t pos = 5; // Remove the first 5 bytes and last byte in while condition
    int32_t nalStartOffset = 3;

    /* NAL Unit Header checks - MPEG-5 Part 2 LCEVC standard - 7.3.2 (Table-6) & 7.4.2.2 */
    if (encapsulatedData[encapsulatedSize - 1] != 0x80) {
        VNLogError("Malformed NAL unit: missing RBSP stop-bit");
        return false;
    }

    if (encapsulatedData[0] != 0 || encapsulatedData[1] != 0 || encapsulatedData[2] != 1) {
        if (encapsulatedData[0] != 0 || encapsulatedData[1] != 0 || encapsulatedData[2] != 0 ||
            encapsulatedData[3] != 1) {
            VNLogError("Malformed prefix: start code [0, 0, 1] or [0, 0, 0, 1] not found\n");
            return false;
        }
        nalStartOffset = 4;
        pos++;
    }

    /*  forbidden_zero_bit   u(1)
        forbidden_one_bit    u(1)
        nal_unit_type        u(5)
        reserved_flag        u(9) */

    /* forbidden bits and reserved flag */
    if ((encapsulatedData[nalStartOffset] & 0xC1) != 0x41 || encapsulatedData[nalStartOffset + 1] != 0xFF) {
        VNLogError("Malformed NAL unit header: forbidden bits or reserved flags not as expected");
        return false;
    }

    LdeNALType type = (LdeNALType)((encapsulatedData[nalStartOffset] & 0x3E) >> 1);
    if (type != NonIDR && type != IDR) {
        VNLogError("Unrecognized LCEVC NAL type, it should be IDR or NonIDR");
        return false;
    }
    *isIDR = (type == IDR) ? true : false;

    while (pos < encapsulatedSize - 1) {
        byte = *(encapsulatedData + pos);
        pos++;

        if (zeroes == 2 && byte == 3) {
            zeroes = 0;
            continue; /* skip it */
        }

        if (byte == 0) {
            ++zeroes;
        } else {
            zeroes = 0;
        }

        *(head++) = byte;
    }
    *unencapsulatedSize = (size_t)(head - unencapsulatedBuffer);

    return true;
}

// Internal functions
static StampedBuffer* stampedBufferNodeAlloc(LdcMemoryAllocator* allocator,
                                             LdcMemoryAllocation* allocation, const uint8_t* data,
                                             size_t bufferSize, uint64_t timestamp, uint64_t inputTime,
                                             bool copy, bool unencapsulate, bool idr)
{
    StampedBuffer* newNode = calloc(1, sizeof(StampedBuffer));

    const uint8_t* newEntryData = (copy ? NULL : data);
    size_t unencapsulatedSize = 0;
    if (copy && bufferSize > 0) {
        uint8_t* tempData = VNAllocateArray(allocator, allocation, uint8_t, bufferSize);
        if (unencapsulate) {
            if (!lcevcContainerUnencapsulate(data, bufferSize, tempData, &unencapsulatedSize, &idr)) {
                VNLogError("Failed to lcevcContainerUnencapsulate LCEVC data from NAL unit");
                VNFree(allocator, allocation);
                free(newNode);
                return NULL;
            }
        } else {
            memcpy(tempData, data, bufferSize);
        }
        newEntryData = tempData;
    }
    newNode->wasCopied = copy;
    newNode->buffer = newEntryData;
    newNode->bufferSize = copy && unencapsulate ? unencapsulatedSize : bufferSize;
    newNode->idr = idr;
    newNode->inputTime = inputTime;
    newNode->timestamp = timestamp;
    newNode->next = NULL;
    newNode->previous = NULL;
    return newNode;
}

static bool stampedBufferListInsert(StampedBufferList* list, StampedBuffer* entry)
{
    if ((list == NULL) || (entry == NULL) || (list->size >= list->capacity)) {
        return false;
    }

    if (list->head == NULL) {
        list->head = entry;
        list->size++;
        return true;
    }

    const uint64_t timestamp = entry->timestamp;

    StampedBuffer* curPtr = list->head;
    size_t index = 0;
    while (index < list->capacity && curPtr != NULL) {
        const uint64_t curTH = curPtr->timestamp;

        // Reject duplicate timestamps first
        if (timestamp == curTH) {
            VN_SEQ_WARNING("Attempting to insert buffer with duplicate timestamp %" PRIu64 "\n", timestamp);
            return false;
        }

        if (timestamp < curTH) {
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
        index++;
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
                               uint64_t timestamp, uint64_t inputTime, bool copy,
                               bool unencapsulate, bool isIdr)
{
    // List must be non-null. Data can only be null IF bufferSize is 0.
    if (list == NULL || (data == NULL && bufferSize != 0)) {
        return false;
    }

    StampedBuffer* newEntry = stampedBufferNodeAlloc(list->allocator, list->allocation, data, bufferSize,
                                                     timestamp, inputTime, copy, unencapsulate, isIdr);
    if (!stampedBufferListInsert(list, newEntry)) {
        stampedBufferRelease(&newEntry);
        return false;
    }

    return true;
}

static const StampedBuffer* stampedBufferQuery(const StampedBufferList* list, uint64_t timestamp,
                                               bool* isAtHeadOut)
{
    if (list == NULL) {
        return NULL;
    }
    const StampedBuffer* cur = list->head;
    *isAtHeadOut = false;

    while (cur) {
        if (cur->timestamp == timestamp) {
            *isAtHeadOut = (cur == list->head);
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

static StampedBuffer* stampedBufferExtract(StampedBufferList* list, uint64_t timestamp, bool* isAtHeadOut)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }
    *isAtHeadOut = false;

    StampedBuffer* cur = list->head;
    while (cur) {
        if (cur->timestamp == timestamp) {
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

static StampedBuffer* stampedBufferPopFront(StampedBufferList* list)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }
    bool dummyHeadOut = false;
    return stampedBufferExtract(list, list->head->timestamp, &dummyHeadOut);
}

static void stampedBufferListRelease(StampedBufferList* list)
{
    StampedBuffer* cur = (list ? list->head : NULL);
    if (cur == NULL) {
        return;
    }

    StampedBuffer* next = NULL;
    while (cur) {
        next = cur->next;
        stampedBufferRelease(&cur);
        cur = next;
    }

    list->size = 0;
    list->head = NULL;
}

// - LCEVCContainer -------------------------------------------------------------------------------

/// This struct uses the TimestampPredictor struct to keep track of valid timestamps, and uses
/// list to hold them (list is a sorted doubly-linked list of `StampedBuffer`s)
/// NOTE: this is not threadsafe. Instead, the calling code should be threadsafe.
typedef struct LCEVCContainer
{
    TimestampPredictor_t* predictor;
    StampedBufferList list;
    bool processedFirst;
} LCEVCContainer;

LCEVCContainer* lcevcContainerCreate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                     size_t capacity)
{
    LCEVCContainer* newContainer = calloc(1, sizeof(LCEVCContainer));
    newContainer->predictor = timestampPredictorCreate();
    newContainer->list.allocator = allocator;
    newContainer->list.allocation = allocation;
    newContainer->list.head = NULL;
    newContainer->list.size = 0;
    newContainer->list.capacity = capacity;

    newContainer->processedFirst = false;
    return newContainer;
}

void lcevcContainerDestroy(LCEVCContainer* container)
{
    timestampPredictorDestroy(container->predictor);
    stampedBufferListRelease(&(container->list));
    free(container);
}

size_t lcevcContainerSize(const LCEVCContainer* container) { return container->list.size; }

size_t lcevcContainerCapacity(const LCEVCContainer* container) { return container->list.capacity; }

void lcevcContainerSetMaxNumReorderFrames(LCEVCContainer* container, uint32_t maxNumReorderFrames)
{
    timestampPredictorSetMaxNumReorderFrames(container->predictor, maxNumReorderFrames);
    // As the predictor will be reset now we should give it a hint if we have any time handles
    // As the list is ordered we can use the head of the list to do the hinting
    if (container->list.head != NULL) {
        timestampPredictorHint(container->predictor, container->list.head->timestamp);
    }
}

bool lcevcContainerInsert(LCEVCContainer* container, const uint8_t* data, uint32_t size,
                          uint64_t timestamp, bool unencapsulate, uint64_t inputTime)
{
    // This will allocate the buffer itself, and fail if duplicate.
    bool ret = stampedBufferAlloc(&container->list, data, size, timestamp, inputTime, true,
                                  unencapsulate, false);
    // Hint with the list head as that will be the smallest PTS
    if (container->list.head != NULL) {
        timestampPredictorHint(container->predictor, container->list.head->timestamp);
    }
    timestampPredictorFeed(container->predictor, timestamp);
    return ret;
}

bool lcevcContainerInsertNoCopy(LCEVCContainer* container, const uint8_t* data, uint32_t size,
                                uint64_t timestamp, bool isIdr, uint64_t inputTime)
{
    // This will allocate the buffer itself, and fail if duplicate.
    bool ret = stampedBufferAlloc(&container->list, data, size, timestamp, inputTime, false, false, isIdr);
    // Hint with the list head as that will be the smallest PTS
    if (container->list.head != NULL) {
        timestampPredictorHint(container->predictor, container->list.head->timestamp);
    }
    timestampPredictorFeed(container->predictor, timestamp);
    return ret;
}

bool lcevcContainerExists(const LCEVCContainer* container, uint64_t timestamp, bool* isNext)
{
    return (stampedBufferQuery(&container->list, timestamp, isNext) != NULL);
}

bool lcevcContainerFlush(LCEVCContainer* container, uint64_t timestamp)
{
    bool dummyHeadOut = false;
    StampedBuffer* toRemove = stampedBufferExtract(&container->list, timestamp, &dummyHeadOut);
    if (!toRemove) {
        return false;
    }
    stampedBufferRelease(&toRemove);
    return true;
}

void lcevcContainerClear(LCEVCContainer* container)
{
    while (lcevcContainerSize(container) != 0) {
        StampedBuffer* toRelease = stampedBufferPopFront(&(container->list));
        stampedBufferRelease(&toRelease);
    }
    timestampPredictorDestroy(container->predictor);
    container->predictor = timestampPredictorCreate();
    container->processedFirst = false;
}

StampedBuffer* lcevcContainerExtract(LCEVCContainer* container, uint64_t timestamp, bool* isNext)
{
    uint32_t count = 0;
    size_t dummyQSize = 0;
    uint64_t dummyTH = 0;
    StampedBuffer* currentHead = lcevcContainerExtractNextInOrder(container, true, &dummyTH, &dummyQSize);
    while (currentHead && currentHead->timestamp < timestamp) {
        stampedBufferRelease(&currentHead);
        currentHead = lcevcContainerExtractNextInOrder(container, true, &dummyTH, &dummyQSize);
        count++;
    }

    if (count > 1) {
        char timestampString[128] = "unknown timestamp";
        timestampPredictorPrintTimestamp(container->predictor, timestampString, 128, timestamp);

        if (container->list.size == 0) {
            VN_SEQ_WARNING("Deleted the entire container in search of %s\n", timestampString);
        } else {
            VN_SEQ_DEBUG("found %s. deleting %" PRIu32 " items out of %zu,\n", timestampString,
                         count, container->list.size);
        }
    }

    // If we overshot without finding the requested timestamp, put the overshoot-entry back in the
    // list and set the return to NULL.
    if (currentHead != NULL && currentHead->timestamp != timestamp) {
        stampedBufferListInsert(&(container->list), currentHead);
        currentHead = NULL;
    }

    *isNext = (count == 0);
    return currentHead;
}

StampedBuffer* lcevcContainerExtractNextInOrder(LCEVCContainer* container, bool force,
                                                uint64_t* timestampOut, size_t* queueSize)
{
    *timestampOut = kInvalidTimestamp;

    *queueSize = lcevcContainerSize(container);
    if (*queueSize == 0) {
        return NULL;
    }

    const uint64_t headTimestamp = container->list.head->timestamp;
    // Moving to here allows the top of the list to always hint even if it's not next
    timestampPredictorHint(container->predictor, headTimestamp);
    if (!force && !timestampPredictorIsNext(container->predictor, headTimestamp)) {
        return NULL;
    }

    bool isAtHead = false;
    StampedBuffer* result = stampedBufferExtract(&container->list, headTimestamp, &isAtHead);
    if (!container->processedFirst) {
        char thString[128] = "unknown timestamp";
        VN_SEQ_DEBUG("processing first lcevc block: %s. Force %d, queue size %zu.\n",
                     timestampPredictorPrintTimestamp(container->predictor, thString, 128, headTimestamp),
                     force, container->list.size);
        container->processedFirst = true;
    }

    if (result == NULL) {
        VN_SEQ_ERROR("Couldn't find front but list isn't empty.\n");
    }

    if (!isAtHead) {
        VN_SEQ_ERROR("Head not at head\n");
    }

    *timestampOut = headTimestamp;
    return result;
}

void lcevcContainerSetPrinter(LCEVCContainer* container, tsPrinter printer)
{
    timestampPredictorSetPrinter(container->predictor, printer);
}
