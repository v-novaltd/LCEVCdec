/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_CMDBUFFER_CPU_H
#define VN_LCEVC_ENHANCEMENT_CMDBUFFER_CPU_H

#include <LCEVC/common/memory.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief The 2-bit CPU command buffer commands */
typedef enum LdeCmdBufferCpuCmd
{
    CBCCAdd = 0,  /**< Add a residual to the temporal buffer. Binary `00 000000` */
    CBCCSet = 64, /**< Set (write) a residual to the temporal buffer. Binary `01 000000 `*/
    CBCCSetZero = 128, /**< Set (write) all zeros of TU size to the temporal buffer. Binary `10 000000` */
    CBCCClear = 192, /**< Set a 32x32px block to zeros - only at the first TU of a block. Binary `11 000000` */
} LdeCmdBufferCpuCmd;

/*! \brief Constants used for the command buffer format. */
enum LdeCmdBufferCpuConstants
{
    CBCKTUSizeDD = 2,       /**< width/height of a DD TU in pixels */
    CBCKTUSizeDDS = 4,      /**< width/height of a DDS TU in pixels */
    CBCKDDSLayers = 16,     /**< layerCount for a DDS buffer */
    CBCKDDLayers = 4,       /**< layerCount for a DD buffer */
    CBCKDDSLayerSize = 32,  /**< layer size (bytes) for a DDS buffer */
    CBCKDDLayerSize = 8,    /**< layer size (bytes) for a DD buffer */
    CBCKBigJumpSignal = 62, /**< Max 6-bit value where skip can be combined with the command */
    CBCKExtraBigJumpSignal = 63, /**< 6 binary 1s to signal to read the next 3 bytes for the jump value */
};

/*! \brief A struct indicating how to apply a slice of a command buffer.
 *
 *  We often want to apply command buffers across several threads. To do this, we split the
 *  commands roughly evenly across several threads, and mark each with an entry point.
 */
typedef struct LdeCmdBufferCpuEntryPoint
{
    uint32_t count; /**< The number of commands in this entry point. */
    uint32_t initialJump; /**< How far to jump to get to the point, in the image, where you apply these commands. */
    int32_t commandOffset; /**< The offset in the commands-end of the command buffer. */
    int32_t dataOffset;    /**< The offset in the data-end of the command buffer. */
} LdeCmdBufferCpuEntryPoint;

/*! \brief Dynamically growing memory-manager for a command buffer instance.
 *
 *  The storage can be resized after initialization. This can be performed after
 *  changing both the capacity and entry size.
 *
 *  \note This does not contract itself overtime.
 */
typedef struct LdeCmdBufferCpuStorage
{
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation allocation;
    uint8_t* start; /**< Pointer to the start of the storage. */
    uint8_t* currentCommand; /**< Pointer to the current command write position from the start of the storage. */
    uint8_t* currentResidual; /**< Pointer to the current residual write position from the end of the storage. */
    uint8_t* end;               /**< Pointer to the end of the storage. */
    uint32_t allocatedCapacity; /**< Number of elements allocated. */
} LdeCmdBufferCpuStorage;

/*! \brief All the information required to apply a cmdbuffer including entrypoints */
typedef struct LdeCmdBufferCpu
{
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation entryPointsAllocation;
    LdeCmdBufferCpuEntryPoint* entryPoints; /**< List of entry points to this cmdBuffer. */
    LdeCmdBufferCpuStorage data; /**< Memory storage for commands and jumps from the start, residuals from the end. */
    uint32_t count;          /**< Number of commands in buffer. */
    uint16_t numEntryPoints; /**< Number of entry points. */
    uint8_t transformSize; /**< Number of residuals in each data element of `data`, 16 for DDS, 4 for DD. */
} LdeCmdBufferCpu;

/*------------------------------------------------------------------------------*/

/*! \brief Get the size of the "residual" end of the command buffer (the portion which extends
 * backwards from buffer->end). */
static inline size_t ldeCmdBufferCpuGetResidualSize(const LdeCmdBufferCpu* buffer)
{
    return buffer->data.end - buffer->data.currentResidual;
}

/*! \brief Get the size of the "commands" end of the command buffer (the portion which
 *         extends forwards from buffer->start).
 */
static inline size_t ldeCmdBufferCpuGetCommandsSize(const LdeCmdBufferCpu* buffer)
{
    return buffer->data.currentCommand - buffer->data.start;
}

/*! \brief Get the total size of the command buffer (data plus commands) */
static inline size_t ldeCmdBufferCpuGetSize(const LdeCmdBufferCpu* buffer)
{
    return ldeCmdBufferCpuGetCommandsSize(buffer) + ldeCmdBufferCpuGetResidualSize(buffer);
}

/*! \brief Returns true if the command buffer contains no entries. */
static inline bool ldeCmdBufferCpuIsEmpty(const LdeCmdBufferCpu* cmdBuffer)
{
    return cmdBuffer->count == 0;
}

/*! \brief Initializes a command buffer, ready to be reset before appending.
 *
 * \param allocator              Memory allocator for all buffer allocations.
 * \param cmdBuffer              The command buffer to initialize.
 * \param numEntryPoints         The number of entry points to be created when calling
 *                               cmdBufferCpuSplit. Cannot be greater than the maximum of CBCKMaxEntryPoints.
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferCpuInitialize(LdcMemoryAllocator* allocator, LdeCmdBufferCpu* cmdBuffer,
                               uint16_t numEntryPoints);

/*! \brief Releases all the memory associated with the command buffer.
 *
 * \param cmdBuffer   The command buffer to release.
 */
void ldeCmdBufferCpuFree(LdeCmdBufferCpu* cmdBuffer);

/*! \brief Resets a command buffer back to an initial state based upon a layer count.
 *
 * This function is intended to be called at the start of processing, even if the
 * layer count hasn't changed - if the layer count changes then the command
 * buffer storage is reshaped accordingly.
 *
 * \param cmdBuffer     The command buffer to reset
 * \param transformSize    The number of layers this command buffer is intended to
 *                      store, either 4 for DD or 16 for DDS transform types
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferCpuReset(LdeCmdBufferCpu* cmdBuffer, uint8_t transformSize);

/*! \brief Appends a new entry in the command buffer for a given location with values.
 *
 * The number of values to be added to the command buffer is based upon the layer count
 * that the command buffer has been reset to - as such `cmdBufferCpuReset` must be called
 * before calling this function.
 *
 * The user must ensure the location that values is pointed at is valid.
 *
 * \param cmdBuffer   The command buffer to add the entry to.
 * \param command     Command to add.
 * \param values      A non-null pointer to memory containing layerCount values to add.
 * \param jump        Number of TUs to jump since last command.
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferCpuAppend(LdeCmdBufferCpu* cmdBuffer, LdeCmdBufferCpuCmd command,
                           const int16_t* values, uint32_t jump);

/*! \brief Determine offsets for this command buffer's entry points.
 *
 * The number of entry points is set on initialization, but their locations can't be known until
 * the command buffer has been populated. This is automatically called by ldeDecode if using CPU
 * command buffers and a value for entry points has been set.
 *
 * \param cmdBuffer   The command buffer to split
 */
void ldeCmdBufferCpuSplit(const LdeCmdBufferCpu* cmdBuffer);

/*------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_CMDBUFFER_CPU_H
