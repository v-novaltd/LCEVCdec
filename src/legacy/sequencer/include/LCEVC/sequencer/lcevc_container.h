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

#ifndef VN_LCEVC_LEGACY_LCEVC_CONTAINER_H
#define VN_LCEVC_LEGACY_LCEVC_CONTAINER_H

#include <LCEVC/common/memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// - Structs ----------------------------------------------------------------------------------

/*!
 * The main container of LCEVC data. One container should be created per video stream. Use
 * lcevcContainerInsert to insert raw data and lcevcContainerExtract... functions to retrieve data
 * in display order. After usage release container memory with lcevcContainerDestroy
 */
typedef struct LCEVCContainer LCEVCContainer;

typedef const char* (*tsPrinter)(char* dest, size_t destSize, uint64_t timestamp);

/*!
 * This struct represents a buffer of LCEVC data with associated timing data, specifically its
 * timestamp and input time. The buffer, buffer size, and timing data are accessible. After a
 * StampedBuffer has been given to the user, and the user has finished with it, they must
 * call stampedBufferRelease on that StampedBuffer.
 */
typedef struct StampedBuffer StampedBuffer;

// - Functions: StampedBuffer ---------------------------------------------------------------

/*! Retrieves data pointer from a given StampedBuffer
 *
 * @param[in]  stBuf                StampedBuffer to get data from
 * @return                          Pointer to the start of the contained data
 */
const uint8_t* stampedBufferGetBuffer(const StampedBuffer* stBuf);

/*! Retrieves the size of a given StampedBuffer
 *
 * @param[in]  stBuf                StampedBuffer to get size of
 * @return                          Size of the buffer's data
 */
size_t stampedBufferGetBufSize(const StampedBuffer* stBuf);

/*! Retrieves the timestamp of a given StampedBuffer
 *
 * @param[in]  stBuf                StampedBuffer to get the timestamp of
 * @return                          Buffer's timestamp
 */
uint64_t stampedBufferGetTimestamp(const StampedBuffer* stBuf);

/*! Retrieves the time when the StampedBuffer was inserted into the container
 *
 * @param[in]  stBuf                StampedBuffer to get the insert time of
 * @return                          Buffer's insert time
 */
uint64_t stampedBufferGetInsertTime(const StampedBuffer* stBuf);

/*! Retrieves the whether an inserted NAL unit was an IDR frame - only applicable when using copy
 *  and unencapsulate insertion
 *
 * @param[in]  stBuf                StampedBuffer to get the IDR status of
 * @return                          true if IDR
 */
bool stampedBufferGetIDR(const StampedBuffer* stBuf);

/*! Releases memory of a StampedBuffer - ensure this is called once a buffer has been used
 *
 * @param[in]  stBuf                StampedBuffer to release
 */
void stampedBufferRelease(StampedBuffer** stBuf);

// - Functions: LCEVCContainer ----------------------------------------------------------------

/*! Create a new LCEVCContainer
 *
 * @param[in]  allocator            Memory allocator instance
 * @param[in]  allocation           Memory allocation to store stamped buffers within the container
 * @param[in]  capacity             The maximum StampedBuffers in the container before errors
 *                                  during insertion. Maps to API config item loq_unprocessed_cap.
 * @return                          Pointer to a new LCEVCContainer
 */
LCEVCContainer* lcevcContainerCreate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                     size_t capacity);

/*! Destroy the LCEVCContainer and release memory of all un-extracted data.
 *
 * @param[in]  container            Container to destroy.
 */
void lcevcContainerDestroy(LCEVCContainer* container);

/*! Get the number of StampedBuffers currently held by the container
 *
 * @param[in]  container            Container to check the size of
 * @return                          Number of held StampedBuffers
 */
size_t lcevcContainerSize(const LCEVCContainer* container);

/*! Gets the maximum capacity of the LCEVCContainer set during lcevcContainerCreate
 *
 * @param[in]  container            Container to check the capacity of
 * @return                          Maximum capacity of StampedBuffers
 */
size_t lcevcContainerCapacity(const LCEVCContainer* container);

/*! Set the maxNumReorderFrames
 *
 * @param[in]  container            The container to set the parameter of
 * @param[in]  maxNumReorderFrames  The maximum number of frames that can be fed in before you get
 *                                  a contiguous block (for instance, if this is 4, then that means
 *                                  that frames 0, 1, 2, and 3 can be fed in in any order, but they
 *                                  will ALL be fed in before frame 4). This is a property of the
 *                                  base codec. If you set this to 0 (or never set it), it will
 *                                  default to 16.
 */
void lcevcContainerSetMaxNumReorderFrames(LCEVCContainer* container, uint32_t maxNumReorderFrames);

/*! Insert data, thereby creating a StampedBuffer
 *
 * This copies and optionally unencapsulates the data, so `data` is still the responsibility of the
 * client.
 *
 * @param[in]     container         The container that you want to put the data into.
 * @param[in]     data              The data (i.e. raw enhancement data extracted from a stream)
 * @param[in]     size              Size of the data
 * @param[in]     timestamp         A unique handle. Data will be sorted and extracted using this
 * @param[in]     unencapsulate     Set true if data should be unencapsulated on copy
 * @param[in]     inputTime         Current time, to allow you to set a timeout
 * @return                          True if insertion succeeded (can fail due to null pointer
 *                                  inputs, a full list, duplicate timestamps, or corrupt list)
 */
bool lcevcContainerInsert(LCEVCContainer* container, const uint8_t* data, uint32_t size,
                          uint64_t timestamp, bool unencapsulate, uint64_t inputTime);

/*! The same as lcevcContainerInsert, except that, instead of copying `data`, the pointer is simply
 * relinquished by the client, and given to the newly-created StampedBuffer. The `data` must
 * already be unencapsulated from the NAL unit using the `lcevcContainerUnencapsulate` utility or
 * similar. The pointer is then released when the owning StampedBuffer is released. Useful if
 * memory bandwidth is very tight or data can be unencapsulated by another process.
 *
 * @param[in]     container         The container that you want to put the data into.
 * @param[in]     data              The data (i.e. raw enhancement data extracted from a stream).
 * @param[in]     size              Size of the data.
 * @param[in]     timestamp         A unique handle. Data will be sorted and extracted using this
 * @param[in]     isIdr             This function cannot determine if the frame is an IDR frame
 *                                  as it has already been unencapsulated, set to true for an IDR.
 * @param[in]     inputTime         Current time, to allow you to set a timeout.
 * @return                          True if insertion succeeded (can fail due to null pointer
 *                                  inputs, a full list, duplicate timestamps, or corrupt list)
 */
bool lcevcContainerInsertNoCopy(LCEVCContainer* container, const uint8_t* data, uint32_t size,
                                uint64_t timestamp, bool isIdr, uint64_t inputTime);

/*! Query if a given timestamp referenced StampedBuffer exists
 *
 * @param[in]     container         Container to query
 * @param[in]     timestamp         Timestamp to query
 * @param[out]    isNext            True if the queried timestamp is next in order
 * @return                          Number of held StampedBuffers
 */
bool lcevcContainerExists(const LCEVCContainer* container, uint64_t timestamp, bool* isNext);

/*! Remove a single timestamp referenced StampedBuffer from the container
 *
 * @param[in]     container         Container to flush
 * @param[in]     timestamp         Timestamp to flush
 * @return                          True if timestamp was found and flushed
 */
bool lcevcContainerFlush(LCEVCContainer* container, uint64_t timestamp);

/*! Remove all StampedBuffers from a container
 *
 * @param[in]     container         Container to clear
 */
void lcevcContainerClear(LCEVCContainer* container);

/*! Extract the specified StampedBuffer
 *
 * Returns the requested Buffer (if it's in the container) and deletes all buffers with a lower
 * timestamp (even if the requested buffer is absent). Note that this means it will return a
 * null pointer and delete the entire list if Buffer was after the latest timestamp available.
 * The returned pointer becomes the user's responsibility: they must call stampedBufferRelease
 * on it when they're done. However, any buffers that are removed by this function will be
 * deleted internally, likewise using stampedBufferRelease.
 *
 * @param[in]     container         The container to extract from
 * @param[in]     timestamp         The timestamp of the StampedBuffer that we want
 * @param[out]    isNext            True if the returned StampedBuffer was next in order anyway
 * @return                          The requested buffer, or NULL if not found
 */
StampedBuffer* lcevcContainerExtract(LCEVCContainer* container, uint64_t timestamp, bool* isNext);

/*! Extract the next StampedBuffer
 *
 * The container's internal predictor is aware of whether the next StampedBuffer is available. If it
 * is not available (and force is false), returns nullptr. As with `extract`, the returned buffer is
 * the user's responsibility to destroy via stampedBufferRelease.
 *
 * @param[in]     container         The container to extract from
 * @param[in]     force             Forces the next timestamp to be extracted regardless of whether
 *                                  it is actually the next one according to the internal predictor.
 *                                  This can result in out of order LCEVC packets and a corrupted
 *                                  decode.
 * @param[out]    timestampOut      The returned buffer's timestamp (if returned)
 * @param[out]    queueSize         The number of StampedBuffers stored currently, before this
 *                                  extraction
 * @return                          The next StampedBuffer, in timestamp order
 */
StampedBuffer* lcevcContainerExtractNextInOrder(LCEVCContainer* container, bool force,
                                                uint64_t* timestampOut, size_t* queueSize);

// - Helpers ---------------------------------------------------------------------------------------

/*! Set the timestamp printer (logging)
 *
 * @param[in]     container         The container to attach printer to
 * @param[in]     printer           Printer function to use for logging
 */
void lcevcContainerSetPrinter(LCEVCContainer* container, tsPrinter printer);

/*! Validate and remove NAL encapsulation from an LCEVC unit. Also returns whether the NAL unit is
 *  an IDR frame for later use.
 *
 * @param[in]     encapsulatedData      Pointer to encapsulated data
 * @param[in]     encapsulatedSize      Size in bytes of encapsulated data
 * @param[out]    unencapsulatedBuffer  Pointer to output buffer, malloc'd to encapsulatedSize
 * @param[out]    unencapsulatedSize    Size in bytes of unencapsulated data - will be slightly
 *                                      smaller than the input data
 * @param[out]    isIDR                 true if NAL unit is for an IDR frame
 */
bool lcevcContainerUnencapsulate(const uint8_t* encapsulatedData, size_t encapsulatedSize,
                                 uint8_t* unencapsulatedBuffer, size_t* unencapsulatedSize, bool* isIDR);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_LEGACY_LCEVC_CONTAINER_H
