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

#ifndef VN_LCEVC_ENHANCEMENT_CMDBUFFER_GPU_H
#define VN_LCEVC_ENHANCEMENT_CMDBUFFER_GPU_H

#include <LCEVC/common/memory.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \file
 * The GPU command buffer format is as follows:
 *
 * A block of 64 bit commands and bitmasks (64 bitmask for DDS, 256 bitmask for DD)
 *  (NB: 2 64 bit words for DDS, 5 64 bit words for DD)
 * A block of TU values one per set bit in 'Set' or 'Add' bitmasks
 *
 * THe 64 bit commands are:
 *
 *     2 bits: operation
 *    18 bits: blockIndex
 *    27 bits: dataOffset
 *     8 bits: bitStart
 *     9 bits: bitCount
 *
 * The offset fields allow for up to 15360×8640 image size with a residual on every pixel.
 */

typedef enum LdeCmdBufferGpuOperation
{
    CBGOAdd = 0,     /**< Add residual data to TUs specified by the bitmask */
    CBGOSet = 1,     /**< Set residual data to TUs specified by the bitmask */
    CBGOSetZero = 2, /**< Set TUs to zero specified by the bitmask */
    CBGOClearAndSet = 3, /**< Set the entire block to zero then set residual data to TUs specified by the bitmask */
} LdeCmdBufferGpuOperation;

/*! \brief The 64-bit bit-field for a single command acting on a single block-operation */
typedef struct LdeCmdBufferGpuCmd
{
    uint64_t operation : 2;   /**< Operation to perform on the block */
    uint64_t blockIndex : 18; /**< Block index within temporal buffer to operate on */
    uint64_t dataOffset : 27; /**< The byte offset to the first residual for add, set and clearAndSet commands - zero when operation is setZero */
    uint64_t bitStart : 8;    /**< Index of the first `1` value in the bitmask */
    uint64_t bitCount : 9;    /**< Count of enabled TUs in the bitmask */
    uint64_t bitmask[4]; /**< Bitmask of TUs within the block to apply the operation to, only the first element is used for DDS, DD requires all 256 bits */
} LdeCmdBufferGpuCmd;

/*! \brief All the information required to apply a cmdbuffer */
typedef struct LdeCmdBufferGpu
{
    LdcMemoryAllocator* allocator;           /**< Allocator for buffer and builder */
    LdcMemoryAllocation allocationCommands;  /**< Buffer allocation */
    LdcMemoryAllocation allocationResiduals; /**< Buffer allocation */

    LdeCmdBufferGpuCmd* commands; /**< Command buffer */
    uint32_t commandCount;        /**< Number of commands */
    uint32_t residualCount; /**< Number of residuals, multiply by layer count to get the size of `residuals` */
    int16_t* residuals; /**< Residual data buffer */
    uint8_t layerCount; /**< Number of residuals 16bit values in each residual, 16 for DDS, 4 for DD. */
} LdeCmdBufferGpu;

/*! \brief This 'builder' struct is required by `ldeDecodeEnhancement` during generation.
 *
 *  This is primarily because residuals of differing operations will be decoded from the stream in
 *  an uncontrolled order however the cmdbufferGpu format requires that residuals for a given block
 *  be adjacent in memory to simplify subsequent copies in the GPU. The intermediate buffers and
 *  their associated counts and capacities are store in this separate struct.
 */
typedef struct LdeCmdBufferGpuBuilder
{
    LdcMemoryAllocation allocationAdd;         /**< Builder allocation for add residual buffer */
    LdcMemoryAllocation allocationSet;         /**< Builder allocation for set residual buffer */
    LdcMemoryAllocation allocationClearAndSet; /**< Builder allocation for clearAndSet residual buffer */

    uint32_t currentAddCmd;         /**< Pointer to last add command within buffer */
    uint32_t currentSetCmd;         /**< Pointer to last set command within buffer */
    uint32_t currentSetZeroCmd;     /**< Pointer to last setZero command within buffer */
    uint32_t currentClearAndSetCmd; /**< Pointer to last clearAndSet command within buffer */

    uint32_t residualAddCount;         /**< Number of add residuals */
    uint32_t residualSetCount;         /**< Number of set residuals */
    uint32_t residualClearAndSetCount; /**< Number of clearAndSet residuals */

    uint32_t commandCapacity; /**< Capacity of the command buffer in the main struct - not required after building */
    uint32_t residualCapacity; /**< Capacity of the combined residual buffer in the main struct */

    uint32_t residualAddCapacity;         /**< Capacity of add residual data buffer */
    uint32_t residualSetCapacity;         /**< Capacity of set residual data buffer */
    uint32_t residualClearAndSetCapacity; /**< Capacity of clearAndSet residual data buffer */

    int16_t* residualsAdd;         /**< Add residual data buffer */
    int16_t* residualsSet;         /**< Set residual data buffer */
    int16_t* residualsClearAndSet; /**< ClearAndSet residual data buffer */

    bool buildingClearAndSet; /**< We are in the process of building a ClearAndSet block */
} LdeCmdBufferGpuBuilder;

/*! \brief Initializes a command buffer, ready to be reset before appending.
 *
 * \param allocator                    Memory allocator for all buffer and builder allocations
 * \param cmdBuffer                    The command buffer to initialize.
 * \param cmdBufferBuilder             The command buffer builder to initialization.
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferGpuInitialize(LdcMemoryAllocator* allocator, LdeCmdBufferGpu* cmdBuffer,
                               LdeCmdBufferGpuBuilder* cmdBufferBuilder);

/*! \brief Resets a command buffer back to an initial state based upon a layer count.
 *
 * This function is intended to be called at the start of processing, even if the
 * layer count hasn't changed.
 *
 * \param cmdBuffer         The command buffer to reset
 * \param cmdBufferBuilder  The command buffer builder to reset
 * \param layerCount        The number of layers this command buffer is intended to store, either 4
 * for DD or 16 for DDS transform types
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferGpuReset(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                          uint8_t layerCount);

/*! \brief Appends a new TU to the command buffer and adds any associated residuals to the builder buffer
 *
 * The number of values to be added to the command buffer is based upon the layer count
 * that the command buffer has been reset to - as such `cmdBufferGpuReset` must be called
 * before calling this function.
 *
 * The user must ensure the location that values is pointed at is valid.
 *
 * \param cmdBuffer         The command buffer to add the command to.
 * \param cmdBufferBuilder  The command buffer builder to add the residual data to.
 * \param operation         Operation type of the residuals
 * \param residuals         A non-null pointer to memory containing layerCount values to add.
 * \param tuIndex           Absolute block aligned TU index of this TU
 * \param tuRasterOrder     True if TUs are in raster order
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferGpuAppend(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                           LdeCmdBufferGpuOperation operation, const int16_t* residuals,
                           uint32_t tuIndex, bool tuRasterOrder);

/*! \brief Builds the various individual residual buffers within the builder into a continuous block
 * of residual memory in the main cmdBuffer. This is automatically called by ldeDecode if using GPU
 * command buffers. The builder struct is no longer required after this step. This function will
 * resize the `cmdBuffer.residuals` buffer but only grow it - this buffer never contracts.
 *
 * \param cmdBuffer         The command buffer to release.
 * \param cmdBufferBuilder  The command buffer builder to release.
 * \param tuRasterOrder     True if TUs are in raster order, allows some steps to be skipped.
 *
 * \return True on success, otherwise false.
 */
bool ldeCmdBufferGpuBuild(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                          bool tuRasterOrder);

/*! \brief Releases all the memory associated with the command buffer and builder.
 *
 * \param cmdBuffer         The command buffer to release.
 * \param cmdBufferBuilder  The command buffer builder to release.
 */
void ldeCmdBufferGpuFree(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_CMDBUFFER_GPU_H
