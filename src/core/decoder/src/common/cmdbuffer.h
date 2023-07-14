/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_CMD_BUFFER_H_
#define VN_DEC_CORE_CMD_BUFFER_H_

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

typedef enum CmdBufferType
{
    CBTResiduals,
    CBTCoordinates
} CmdBufferType_t;

typedef struct CmdBufferData
{
    const int16_t* coordinates;
    const int16_t* residuals;
    int32_t count;
} CmdBufferData_t;

typedef struct CmdBuffer CmdBuffer_t;
typedef struct Memory* Memory_t;

/*------------------------------------------------------------------------------*/

/*! \brief Initializes a command buffer read for appending.
 *
 * \param cmdBuffer   The command buffer location to initialise.
 *
 * \return true on success, otherwise false.
 */
bool cmdBufferInitialise(Memory_t memory, CmdBuffer_t** cmdBuffer, CmdBufferType_t type);

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
bool cmdBufferReset(CmdBuffer_t* cmdBuffer, int32_t layerCount);

/*! \brief Retrieves the layer count the command buffer has been reset to.
 *
 * \param cmdBuffer   The command buffer to retrieve the count from.
 *
 * \return The number of layers if cmdBufferReset has been called, otherwise 0.
 */
int32_t cmdBufferGetLayerCount(const CmdBuffer_t* cmdBuffer);

/*! \brief Retrieves the underlying data from the cmdbuffer.
 *
 * \note The returned values are invalidated if any other cmdBuffer API
 *       is called after this function returns.
 *
 * \param cmdBuffer The command buffer to retrieve data from.
 */
CmdBufferData_t cmdBufferGetData(const CmdBuffer_t* cmdBuffer);

/*! \brief Returns true if the command buffer contains no entries. */
bool cmdBufferIsEmpty(const CmdBuffer_t* cmdBuffer);

int32_t cmdBufferGetCount(const CmdBuffer_t* cmdBuffer);

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
bool cmdBufferAppend(CmdBuffer_t* cmdBuffer, int16_t x, int16_t y, const int16_t* values);

/*! \brief Appends a new entry in the command buffer for a given location.
 *
 * Unlike `cmdBufferAppend()` this function does not require cmdbuffer_reset() to
 * be called before being used, as the default state of the command buffer is sufficient
 * for appending just coordinate commands. However when resetting through `cmdbufferReset()`
 * it is required that it is called with a layer_count of 0.

 * \note The buffer must have been initialised with `CBT_Residuals` otherwise this
 *       call will fail.
 *
 * \param cmdBuffer   The command buffer to add the entry to.
 * \param x           The horizontal coord for the command.
 * \param y           The vertical coord for the command.
 *
 * \return true on success, otherwise false.
 */
bool cmdBufferAppendCoord(CmdBuffer_t* cmdBuffer, int16_t x, int16_t y);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_CMD_BUFFER_H_ */