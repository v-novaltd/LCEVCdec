/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_DEC_CORE_CMD_BUFFER_H_
#define VN_DEC_CORE_CMD_BUFFER_H_

#include "common/memory.h"
#include "common/platform.h"

/*! \file
 * \brief This file provides an implementation of what is referred to as a
 *        `Command Buffer`, this is used to store a list of locations and optionally
 *        associated residual data for some decoded enhancement data.
 *
 * The command buffer concept can be thought of as an intermediate representation
 * of some data that lends itself well to certain parallel operations.
 *
 * The command buffer is backed by a dynamic storage mechanism that grows a contiguous
 * block of memory to supply the required data for storing of commands.
 *
 * Within this implementation there are 2 types of "command" that can be recorded,
 * but please note that a single instance of a command buffer will store only a
 * single type of command, mixing different commands within the buffer is not
 * supported.
 *
 * #Residuals
 *
 * A residuals command stores the `x` & `y` destination coordinates of a transform
 * and either 4 or 16 values for the 2x2 or 4x4 transform respectively.
 *
 * The semantics of the type of residual (i.e. intra or inter signaled) are not
 * stored within the command buffer.
 *
 * The coordinate data and residual data are stored on independent buffers to help with
 * cache alignment for direct loading of residual data.
 *
 * The coordinate data is stored as consecutive 2-value coordinate pairs formed of
 * int16_t values.
 *
 * The residual data is stored as consecutive 4 or 16 value residuals formed of int16_t
 * values.
 *
 * Both buffers will be maintained in lock-step such that the number of entries in each
 * will be the same at any moment.
 *
 * #Coordinates
 *
 * A coordinate command stores an `x` & `y` destination coordinate, this is used
 * for storing temporal buffer tile clears for clearing a region of the temporal
 * buffer back to zero.
 *
 * For tile clearing the tile size is specified as 32x32.
 *
 * The storage of the coordinates are identical to that of the `Residuals` command-type.
 *
 *
 * \note In both cases the coordinates refer to the top-left element of the object
 *       being stored (i.e. for a tile it is the top-left of the tile).
 *
 * #Recording Residuals
 *
 *    // A buffer must always be initialised, it should only be initialised once.
 *    CmdBuffer_t* buffer;
 *    cmdBufferInitialise(&buffer, CBTResiduals);
 *
 *    // This records a single transforms residuals into the command buffer.
 *    int16_t values[layerCount] = { ... };
 *    cmdBufferAppend(buffer, x, y, values);
 *
 *    // When finished with a command buffer clean up.
 *    cmdBufferFree(buffer);
 *
 * #Recording Coordinates
 *
 *    // A buffer must always be initialised, it should only be initialised once.
 *    CmdBuffer_t* buffer;
 *    cmdBufferInitialise(&buffer, CBTCoordinates);
 *
 *    // This records a single coordinate into the command buffer
 *    cmdBufferAppendCoord(&buffer, x, y);
 *
 *    // When finished with a command buffer clean up.
 *    cmdBufferFree(buffer);
 *
 * #Resetting an existing buffer
 *
 *    // A buffer can be reset whenever, it resets the state of the buffer back
 *    // to as though it was initialised, however it does not modify the data backing
 *    // the buffer..
 *    cmdBufferReset(&buffer, layerCount);
 *
 * #Retrieving raw command data
 *
 *    // Called when wanting to do something with the recorded data.
 *    CmdBufferData_t data = cmdBufferGetData(buffer, &outputData, outputCount);
 */

/*------------------------------------------------------------------------------*/

typedef enum CmdBufferCmd
{
    CBCAdd = 0,       /**< Add a residual to the temporal buffer. Binary 00 000000 */
    CBCSet = 64,      /**< Set (write) a residual to the temporal buffer. Binary 01 000000 */
    CBCSetZero = 128, /**< Set (write) all zeros of TU size to the temporal buffer. Binary 10 000000 */
    CBCClear = 192, /**< Set a 32x32px block to zeros - only at the first TU of a block. Binary 11 000000 */
} CmdBufferCmd_t;

/*! \brief Arbitrary constants used for the command buffer storage behavior. */
enum CmdBufferConstants
{
    CBKStoreGrowFactor = 2, /**< The factory multiply current capacity by when growing the buffer. */
    CBKDefaultInitialCapacity = 32768, /**< The default initial capacity of a cmdbuffer. */
    CBKTUSizeDD = 2,                   /**< width/height of a DD TU in pixels */
    CBKTUSizeDDS = 4,                  /**< width/height of a DDS TU in pixels */
    CBKDDSLayers = 16,                 /**< layerCount for a DDS buffer */
    CBKDDLayers = 4,                   /**< layerCount for a DD buffer */
    CBKDDSLayerSize = 32,              /**< layer size (bytes) for a DDS buffer */
    CBKDDLayerSize = 8,                /**< layer size (bytes) for a DD buffer */
    CBKBigJump = 62,            /**< Max 6-bit value where skip can be combined with the command */
    CBKExtraBigJumpSignal = 63, /**< 6 binary 1s to signal to read the next 3 bytes for the jump value */
    CBKExtraBigJump = UINT16_MAX /**< Max 16-bit value before overflowing to a 24-bit jump value */
};

/*! \brief A struct indicating how to apply a slice of a command buffer.
 *
 *  We often want to apply command buffers across several threads. To do this, we split the
 *  commands roughly evenly across several threads, and mark each with an entry point.
 */
typedef struct CmdBufferEntryPoint
{
    uint32_t count; /**< The number of commands in this entry point. */
    uint32_t initialJump; /**< How far to jump to get to the point, in the image, where you apply these commands. */
    int32_t commandOffset; /**< The offset in the commands-end of the command buffer. */
    int32_t dataOffset;    /**< The offset in the data-end of the command buffer. */
} CmdBufferEntryPoint_t;

/*! \brief Dynamically growing memory-manager for a command buffer instance.
 *
 *  The storage can be resized after initialization. This can be performed after
 *  changing both the capacity and entry size.
 *
 *  \note This does not contract itself overtime.
 */
typedef struct CmdBufferStorage
{
    Memory_t memory;         /**< Decoder instance this belongs to. */
    uint8_t* start;          /**< Pointer to the start of the storage. */
    uint8_t* currentCommand; /**< Pointer to the current command write position in the storage. */
    uint8_t* currentData;    /**< Pointer to the current data write position in the storage. */
    uint8_t* end;            /**< Pointer to the end of the storage. */
    uint32_t allocatedCapacity; /**< Number of elements allocated. */
} CmdBufferStorage_t;

/*! \brief A series of commands (add, set, clear, etc), and whatever information is needed to apply
 *         those commands to a surface.
 */
typedef struct CmdBuffer
{
    Memory_t memory; /**< Memory-manager from this command buffer's decoder instance. */
    CmdBufferStorage_t data; /**< Memory storage for residuals from the start, commands and jumps from the end. */
    CmdBufferEntryPoint_t* entryPoints; /**< List of entry points to this cmdBuffer. */
    uint32_t count;                     /**< Number of commands in buffer. */
    uint16_t numEntryPoints;            /**< Number of entry points. */
    uint8_t layerCount; /**< Number of residuals in each data element of `data`, 16 for DDS, 4 for DD. */
} CmdBuffer_t;

typedef struct Memory* Memory_t;

/*------------------------------------------------------------------------------*/

/*! \brief Get the size of the "data" end of the command buffer (the portion which extends backwards
 *         from buffer->end). */
static inline size_t cmdBufferGetDataSize(const CmdBuffer_t* buffer)
{
    return buffer->data.end - buffer->data.currentData;
}

/*! \brief Get the size of twhic ffmpeghe "commands" end of the command buffer (the portion which
 * extends forwards from buffer->start). */
static inline size_t cmdBufferGetCommandsSize(const CmdBuffer_t* buffer)
{
    return buffer->data.currentCommand - buffer->data.start;
}

/*! \brief Get the total size of the command buffer (data plus commands) */
static inline size_t cmdBufferGetSize(const CmdBuffer_t* buffer)
{
    return cmdBufferGetCommandsSize(buffer) + cmdBufferGetDataSize(buffer);
}

/*! \brief Initializes a command buffer, ready for appending.
 *
 * \param cmdBuffer       The command buffer location to initialise.
 * \param numEntryPoints  The number of entry points.
 *
 * \return true on success, otherwise false.
 */
bool cmdBufferInitialise(Memory_t memory, CmdBuffer_t** cmdBuffer, uint16_t numEntryPoints);

/*! \brief Releases all the memory associated with the command buffer.
 *
 * \param cmdBuffer   The command buffer to release.
 */
void cmdBufferFree(CmdBuffer_t* cmdBuffer);

/*! \brief Resets a command buffer back to an initial state based upon a layer count.
 *
 * This function is intended to be called at the start of processing, even if the
 * layer count hasn't changed - if the layer count changes then the command
 * buffer storage is reshaped accordingly.
 *
 * \param cmdBuffer     The command buffer to reset
 * \param layerCount    The number of layers this command buffer is intended to
 *                      store, this can only be non-zero if the command buffer was
 *                      initialised with `CBT_Residuals`.
 *
 * \return True on success, otherwise false.
 */
bool cmdBufferReset(CmdBuffer_t* cmdBuffer, uint8_t layerCount);

/*! \brief Returns true if the command buffer contains no entries. */
static inline bool cmdBufferIsEmpty(const CmdBuffer_t* cmdBuffer) { return cmdBuffer->count == 0; }

/*! \brief Appends a new entry in the command buffer for a given location with values.
 *
 * The number of values to be added to the command buffer is based upon the layer count
 * that the command buffer has been reset to - as such `cmdbuffer_reset()` must be called
 * before calling this function.
 *
 * The user must ensure the location that values is pointed at is valid.
 *
 * \note The buffer must have been initialised with `CBT_Residuals` otherwise this
 *       call will fail.
 *
 * \param cmdBuffer   The command buffer to add the entry to.
 * \param x           The horizontal coord for the command.
 * \param y           The vertical coord for the command.
 * \param values      A non-null pointer to memory containing layer_count values to add.
 *
 * \return true on success, otherwise false.
 */
bool cmdBufferAppend(CmdBuffer_t* cmdBuffer, CmdBufferCmd_t command, const int16_t* values, uint32_t jump);

/*! \brief Determine offsets for this command buffer's entry points.
 *
 * The number of entry points is set on initialization, but their locations can't be known until
 * the command buffer has actually been populated.
 */
void cmdBufferSplit(const CmdBuffer_t* srcBuffer);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_CMD_BUFFER_H_ */