/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_CORE_SEQUENCING_LCEVC_CONTAINER_H_
#define VN_CORE_SEQUENCING_LCEVC_CONTAINER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// - Structs ----------------------------------------------------------------------------------

typedef struct LCEVCContainer LCEVCContainer_t;

typedef const char* (*thPrinter)(char* dest, size_t destSize, uint64_t timehandle);

/*! StampedBuffer_t struct
 *
 * This struct represents a buffer of lcevc data with associated timing data, specifically its
 * timehandle and input time. The buffer, buffer size, and timing data are accessible. After a
 * StampedBuffer_t has been given to the user, and the user has finished with it, they must
 * call stampedBufferRelease on that StampedBuffer_t.
 */
typedef struct StampedBuffer StampedBuffer_t;

// - Functions: StampedBuffer_t ---------------------------------------------------------------

const uint8_t* stampedBufferGetBuffer(const StampedBuffer_t* stBuf);
size_t stampedBufferGetBufSize(const StampedBuffer_t* stBuf);
uint64_t stampedBufferGetTimehandle(const StampedBuffer_t* stBuf);
uint64_t stampedBufferGetInputTime(const StampedBuffer_t* stBuf);

/// Use this function when you're done with a StampedBuffer_t that you've extracted.
void stampedBufferRelease(StampedBuffer_t** buffer);

// - Functions: LCEVCContainer_t --------------------------------------------------------------

/*! Create a new LCEVC Container
 *
 * @param[in]  capacity             The desired capacity. Capacity of 0 means limitless capacity
 *                                  (all inserts succeed), capacity of UINT32_MAX means no
 *                                  capacity (all inserts fail), otherwise capacity will simply set
 *                                  the maximum size.
 */
LCEVCContainer_t* lcevcContainerCreate(uint32_t capacity);
void lcevcContainerDestroy(LCEVCContainer_t* container);

uint32_t lcevcContainerSize(const LCEVCContainer_t* container);
uint32_t lcevcContainerCapacity(const LCEVCContainer_t* container);

/*! Set the maxNumReorderFrames
 * @param[in]  maxNumReorderFrames  The maximum number of frames that can be fed in before you get
 *                                  a contiguous block (for instance, if this is 4, then that means
 *                                  that frames 0, 1, 2, and 3 can be fed in in any order, but they
 *                                  will ALL be fed in before frame 4). This is a property of the
 *                                  base codec. If you set this to 0 (or never set it), it will
 *                                  default to 16.
 */
void lcevcContainerSetMaxNumReorderFrames(LCEVCContainer_t* container, uint32_t maxNumReorderFrames);

/*! Insert data, thereby creating a StampedBuffer_t
 *
 * This copies the data, so `data` is still the responsibility of the client.
 *
 * @param[in]     container         The container that you want to put the data into.
 * @param[out]    data              The data (i.e. raw enhancement data extracted from a stream).
 * @param[out]    size              Size of the data.
 * @param[out]    timehandle        A unique handle. Data will be sorted and extracted using this.
 * @param[out]    inputTime         Current time, to allow you to set a timeout.
 * @return                          True if insertion succeeded (can fail due to null pointer
 *                                  inputs, a full list, duplicate timehandles, or corrupt list).
 */
bool lcevcContainerInsert(LCEVCContainer_t* container, const uint8_t* data, uint32_t size,
                          uint64_t timehandle, uint64_t inputTime);

/*! The same as lcevcContainerInsert, except that, instead of copying `data`, the pointer is simply
 * relinquished by the client, and given to the newly-created StampedBuffer_t. It is then released
 * when the owning StampedBuffer_t is released. Useful if memory bandwidth is very tight.
 */
bool lcevcContainerInsertNoCopy(LCEVCContainer_t* container, const uint8_t* data, uint32_t size,
                                uint64_t timehandle, uint64_t inputTime);

bool lcevcContainerExists(const LCEVCContainer_t* container, uint64_t timehandle, bool* isAtHeadOut);
void lcevcContainerFlush(LCEVCContainer_t* container, uint64_t timehandle);
void lcevcContainerClear(LCEVCContainer_t* container);

/*! Extract the specified StampedBuffer_t
 *
 * Returns the requested Buffer (if it's in the container) and deletes all buffers with a lower
 * timehandle (even if the requested buffer is absent). Note that this means it will return a
 * null pointer and delete the entire list if Buffer was after the latest timehandle available.
 * The returned pointer becomes the user's responsibility: they must call stampedBufferRelease
 * on it when they're done. However, any buffers that are removed by this function will be
 * deleted internally, likewise using stampedBufferRelease.
 *
 * @param[in]     timehandle        The timehandle of the StampedBuffer_t that we want.
 * @param[out]    isAtHeadOut       True if the returned StampedBuffer_t was at the head (i.e. next).
 * @return                          The requested buffer, or NULL if not found.
 */
StampedBuffer_t* lcevcContainerExtract(LCEVCContainer_t* container, uint64_t timehandle, bool* isAtHeadOut);

/*! Extract the next StampedBuffer_t
 *
 * Container's predictor allows us to check whether we actually HAVE the next one yet. If we
 * don't (and force is false), we'll return nullptr. As with `extract`, the returned buffer is
 * the user's responsibility to destroy via stampedBufferRelease.
 *
 * @param[in]     force             Forces the next timehandle to be extracted regardless of if the predictor thinks it's ready.
 * @param[out]    timehandleOut     The returned buffer's timehandle (if returned).
 * @param[out]    queueSizeOut      The number of StampedBuffer_ts stored currently, before this extraction.
 * @return                          The next StampedBuffer_t, in timehandle order.
 */
StampedBuffer_t* lcevcContainerExtractNextInOrder(LCEVCContainer_t* container, bool force,
                                                  uint64_t* timehandleOut, uint32_t* queueSizeOut);

/*! Set the timehandle printer (the function used when logging timehandles).*/
void lcevcContainerSetPrinter(LCEVCContainer_t* container, thPrinter printer);

#ifdef __cplusplus
}
#endif

#endif // VN_CORE_SEQUENCING_LCEVC_CONTAINER_H_
