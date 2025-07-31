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

#include <assert.h>
#include <LCEVC/common/bitutils.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <stdbool.h>
#include <stdio.h>

/*! \brief Constants used for the command buffer format and storage behavior. */
enum CmdBufferGpuConstants
{
    CBGKStoreGrowFactor = 2, /**< The factory multiply current capacity by when growing buffers. */
    CBGKInitialResidualBuilderCapacity =
        2048, /**< Initial capacity of each residual buffer within CmdBufferGpuBuilder in 2*bytes */
    CBGKInitialCommandBuilderCapacity =
        256, /**< Initial capacity of CmdBufferGpu commands in CmdBufferGpuCmds */
    CBGKMaxBlockIndex = (1 << 18) - 1, /**< Max 18-bit number to initialize new commands block index values to */
    CBGKDDSLayers = 16,                /**< layerCount for a DDS buffer */
    CBGKDDLayers = 4,                  /**< layerCount for a DD buffer */
    CBGKDDSBlockSize = 64, /**< Number of TUs in a DDS block */
    CBGKDDBlockSize = 256, /**< Number of TUs in a DD block */
};

/*------------------------------------------------------------------------------*/

static bool cmdBufferResidualsResize(LdcMemoryAllocator* allocator, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                                     LdeCmdBufferGpuOperation operation, uint32_t newCapacity)
{
    assert(cmdBufferBuilder);

    int16_t** residualBuffer = NULL;
    uint32_t* capacity = 0;
    LdcMemoryAllocation* allocation = 0;
    switch (operation) {
        case CBGOAdd: {
            residualBuffer = &cmdBufferBuilder->residualsAdd;
            capacity = &cmdBufferBuilder->residualAddCapacity;
            allocation = &cmdBufferBuilder->allocationAdd;
            break;
        }
        case CBGOSet: {
            residualBuffer = &cmdBufferBuilder->residualsSet;
            capacity = &cmdBufferBuilder->residualSetCapacity;
            allocation = &cmdBufferBuilder->allocationSet;
            break;
        }
        case CBGOClearAndSet: {
            residualBuffer = &cmdBufferBuilder->residualsClearAndSet;
            capacity = &cmdBufferBuilder->residualClearAndSetCapacity;
            allocation = &cmdBufferBuilder->allocationClearAndSet;
            break;
        }
        default: return false;
    }

    if (*residualBuffer) {
        *residualBuffer = VNReallocateArray(allocator, allocation, int16_t, newCapacity);
    } else {
        *residualBuffer = VNAllocateArray(allocator, allocation, int16_t, newCapacity);
    }
    if (!*residualBuffer) {
        return false;
    }

    *capacity = newCapacity;

    return true;
}

static bool cmdBufferCommandsResize(LdeCmdBufferGpu* cmdBuffer,
                                    LdeCmdBufferGpuBuilder* cmdBufferBuilder, uint32_t newCapacity)
{
    if (newCapacity < cmdBufferBuilder->commandCapacity) {
        return false;
    }

    uint32_t prevLastCmd = cmdBufferBuilder->commandCapacity - 1;
    uint32_t lastCmd = newCapacity - 1;
    LdeCmdBufferGpuCmd* commandBuffer = NULL;
    if (cmdBuffer->commands) {
        commandBuffer = VNReallocateArray(cmdBuffer->allocator, &cmdBuffer->allocationCommands,
                                          LdeCmdBufferGpuCmd, newCapacity);
        if (!commandBuffer) {
            return false;
        }
        memset(&commandBuffer[prevLastCmd], 0, (newCapacity - prevLastCmd) * sizeof(LdeCmdBufferGpuCmd));
        commandBuffer[lastCmd].blockIndex = CBGKMaxBlockIndex;
        if (cmdBufferBuilder->currentAddCmd == prevLastCmd) {
            cmdBufferBuilder->currentAddCmd = lastCmd;
        }
        if (cmdBufferBuilder->currentSetCmd == prevLastCmd) {
            cmdBufferBuilder->currentSetCmd = lastCmd;
        }
        if (cmdBufferBuilder->currentSetZeroCmd == prevLastCmd) {
            cmdBufferBuilder->currentSetZeroCmd = lastCmd;
        }
        if (cmdBufferBuilder->currentClearAndSetCmd == prevLastCmd) {
            cmdBufferBuilder->currentClearAndSetCmd = lastCmd;
        }
    } else {
        commandBuffer = VNAllocateZeroArray(cmdBuffer->allocator, &cmdBuffer->allocationCommands,
                                            LdeCmdBufferGpuCmd, newCapacity);
        if (!commandBuffer) {
            return false;
        }
        commandBuffer[lastCmd].blockIndex = CBGKMaxBlockIndex;
        cmdBufferBuilder->currentAddCmd = lastCmd;
        cmdBufferBuilder->currentSetCmd = lastCmd;
        cmdBufferBuilder->currentSetZeroCmd = lastCmd;
        cmdBufferBuilder->currentClearAndSetCmd = lastCmd;
    }

    cmdBuffer->commands = commandBuffer;
    cmdBufferBuilder->commandCapacity = newCapacity;

    return true;
}

void cmdBufferAppendResiduals(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                              LdeCmdBufferGpuCmd* cmd, const int16_t* residuals,
                              int16_t* residualBuffer, uint32_t* residualCount,
                              const uint32_t* residualCapacity, uint32_t tuIndex, bool tuRasterOrder)
{
    const bool dds = cmdBuffer->layerCount == CBGKDDSLayers;
    const uint16_t blockSize = dds ? CBGKDDSBlockSize : CBGKDDBlockSize;
    uint16_t tuBlockPosition = tuIndex % blockSize;
    if (!tuRasterOrder && dds) {
        cmd->bitmask[0] |= (1ULL << (blockSize - 1 - tuBlockPosition));
        if (cmd->bitCount == 0) {
            cmd->bitStart = clz64(cmd->bitmask[0]);
        }
    } else {
        if (tuRasterOrder) {
            tuBlockPosition = tuIndex % CBGKDDBlockSize;
        }
        const uint16_t maskIndex = tuBlockPosition >> 6;
        cmd->bitmask[maskIndex] |= (1ULL << (blockSize - 1 - (tuBlockPosition % 64)));
        if (cmd->bitCount == 0) {
            cmd->bitStart = clz64(cmd->bitmask[maskIndex]);
        }
    }
    cmd->bitCount++;

    if (residualBuffer) {
        if (*residualCount >= *residualCapacity - cmdBuffer->layerCount) {
            cmdBufferResidualsResize(cmdBuffer->allocator, cmdBufferBuilder, cmd->operation,
                                     *residualCapacity * CBGKStoreGrowFactor);
            switch (cmd->operation) {
                case CBGOAdd: residualBuffer = cmdBufferBuilder->residualsAdd; break;
                case CBGOSet: residualBuffer = cmdBufferBuilder->residualsSet; break;
                case CBGOClearAndSet:
                    residualBuffer = cmdBufferBuilder->residualsClearAndSet;
                    break;
                default: return;
            }
        }

        if (cmdBuffer->layerCount == CBGKDDSLayers) {
            residualBuffer[*residualCount + 0] = residuals[0];
            residualBuffer[*residualCount + 1] = residuals[1];
            residualBuffer[*residualCount + 2] = residuals[4];
            residualBuffer[*residualCount + 3] = residuals[5];
            residualBuffer[*residualCount + 4] = residuals[2];
            residualBuffer[*residualCount + 5] = residuals[3];
            residualBuffer[*residualCount + 6] = residuals[6];
            residualBuffer[*residualCount + 7] = residuals[7];
            residualBuffer[*residualCount + 8] = residuals[8];
            residualBuffer[*residualCount + 9] = residuals[9];
            residualBuffer[*residualCount + 10] = residuals[12];
            residualBuffer[*residualCount + 11] = residuals[13];
            residualBuffer[*residualCount + 12] = residuals[10];
            residualBuffer[*residualCount + 13] = residuals[11];
            residualBuffer[*residualCount + 14] = residuals[14];
            residualBuffer[*residualCount + 15] = residuals[15];
        } else {
            memcpy((void*)(residualBuffer + *residualCount), residuals,
                   cmdBuffer->layerCount * sizeof(int16_t));
        }

        *residualCount += cmdBuffer->layerCount;
    }
}

/*------------------------------------------------------------------------------*/

bool ldeCmdBufferGpuInitialize(LdcMemoryAllocator* allocator, LdeCmdBufferGpu* cmdBuffer,
                               LdeCmdBufferGpuBuilder* cmdBufferBuilder)
{
    assert(cmdBuffer && cmdBufferBuilder);

    cmdBuffer->allocator = allocator;
    cmdBuffer->commandCount = 0;

    cmdBufferBuilder->residualAddCapacity = CBGKInitialResidualBuilderCapacity;
    cmdBufferBuilder->residualSetCapacity = CBGKInitialResidualBuilderCapacity;
    cmdBufferBuilder->residualClearAndSetCapacity = CBGKInitialResidualBuilderCapacity;
    cmdBufferBuilder->commandCapacity = CBGKInitialCommandBuilderCapacity;

    cmdBufferBuilder->buildingClearAndSet = false;

    return true;
}

bool ldeCmdBufferGpuAppend(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                           LdeCmdBufferGpuOperation operation, const int16_t* residuals,
                           uint32_t tuIndex, bool tuRasterOrder)
{
    const uint32_t blockIndex =
        tuIndex >> (!tuRasterOrder && cmdBuffer->layerCount == CBGKDDSLayers ? 6 : 8);
    LdeCmdBufferGpuCmd* cmd = NULL;
    int16_t* residualBuffer = 0;
    uint32_t* residualCount = 0;
    const uint32_t* residualCapacity = 0;

    if (operation != CBGOClearAndSet && cmdBufferBuilder->buildingClearAndSet) {
        if (blockIndex == cmdBuffer->commands[cmdBufferBuilder->currentClearAndSetCmd].blockIndex) {
            if (operation != CBGOSetZero) {
                cmd = &cmdBuffer->commands[cmdBufferBuilder->currentClearAndSetCmd];
                residualBuffer = cmdBufferBuilder->residualsClearAndSet;
                residualCount = &cmdBufferBuilder->residualClearAndSetCount;
                residualCapacity = &cmdBufferBuilder->residualClearAndSetCapacity;

                cmdBufferAppendResiduals(cmdBuffer, cmdBufferBuilder, cmd, residuals, residualBuffer,
                                         residualCount, residualCapacity, tuIndex, false);

                return true;
            }
        } else {
            cmdBufferBuilder->buildingClearAndSet = false;
        }
    }

    switch (operation) {
        case CBGOAdd: {
            cmd = &cmdBuffer->commands[cmdBufferBuilder->currentAddCmd];
            residualBuffer = cmdBufferBuilder->residualsAdd;
            residualCount = &cmdBufferBuilder->residualAddCount;
            residualCapacity = &cmdBufferBuilder->residualAddCapacity;
            break;
        }
        case CBGOSet: {
            cmd = &cmdBuffer->commands[cmdBufferBuilder->currentSetCmd];
            residualBuffer = cmdBufferBuilder->residualsSet;
            residualCount = &cmdBufferBuilder->residualSetCount;
            residualCapacity = &cmdBufferBuilder->residualSetCapacity;
            break;
        }
        case CBGOSetZero: {
            cmd = &cmdBuffer->commands[cmdBufferBuilder->currentSetZeroCmd];
            break;
        }
        case CBGOClearAndSet: {
            cmdBufferBuilder->buildingClearAndSet = true;
            cmd = &cmdBuffer->commands[cmdBufferBuilder->currentClearAndSetCmd];
            residualBuffer = cmdBufferBuilder->residualsClearAndSet;
            residualCount = &cmdBufferBuilder->residualClearAndSetCount;
            residualCapacity = &cmdBufferBuilder->residualClearAndSetCapacity;
            break;
        }
        default: return false;
    }

    if (cmd->blockIndex == blockIndex) {
        // modify existing cmd
        if (operation != CBGOClearAndSet) {
            cmdBufferAppendResiduals(cmdBuffer, cmdBufferBuilder, cmd, residuals, residualBuffer,
                                     residualCount, residualCapacity, tuIndex, tuRasterOrder);
        }
    } else {
        if (cmdBuffer->commandCount == cmdBufferBuilder->commandCapacity - 2) {
            if (!cmdBufferCommandsResize(cmdBuffer, cmdBufferBuilder,
                                         cmdBufferBuilder->commandCapacity * CBGKStoreGrowFactor)) {
                return false;
            }
        }
        cmd = &cmdBuffer->commands[cmdBuffer->commandCount];
        switch (operation) {
            case CBGOAdd: cmdBufferBuilder->currentAddCmd = cmdBuffer->commandCount; break;
            case CBGOSet: cmdBufferBuilder->currentSetCmd = cmdBuffer->commandCount; break;
            case CBGOSetZero: cmdBufferBuilder->currentSetZeroCmd = cmdBuffer->commandCount; break;
            case CBGOClearAndSet:
                cmdBufferBuilder->currentClearAndSetCmd = cmdBuffer->commandCount;
                break;
        }
        if (operation != CBGOSetZero) {
            cmd->dataOffset = *residualCount;
        }
        // new cmd
        cmd->operation = operation;
        cmd->blockIndex = blockIndex;
        cmd->bitCount = 0;

        if (operation != CBGOClearAndSet) {
            cmdBufferAppendResiduals(cmdBuffer, cmdBufferBuilder, cmd, residuals, residualBuffer,
                                     residualCount, residualCapacity, tuIndex, tuRasterOrder);
        }

        cmdBuffer->commandCount++;
    }

    return true;
}

bool ldeCmdBufferGpuReset(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                          uint8_t layerCount)
{
    assert(cmdBuffer && cmdBufferBuilder);

    cmdBufferBuilder->residualAddCount = 0;
    cmdBufferBuilder->residualSetCount = 0;
    cmdBufferBuilder->residualClearAndSetCount = 0;
    cmdBuffer->commandCount = 0;
    cmdBuffer->residualCount = 0;

    /* Already with the right number of layers. */
    if (layerCount != cmdBuffer->layerCount) {
        if (!cmdBufferResidualsResize(cmdBuffer->allocator, cmdBufferBuilder, CBGOAdd,
                                      cmdBufferBuilder->residualAddCapacity)) {
            return false;
        }
        if (!cmdBufferResidualsResize(cmdBuffer->allocator, cmdBufferBuilder, CBGOSet,
                                      cmdBufferBuilder->residualSetCapacity)) {
            return false;
        }
        if (!cmdBufferResidualsResize(cmdBuffer->allocator, cmdBufferBuilder, CBGOClearAndSet,
                                      cmdBufferBuilder->residualClearAndSetCapacity)) {
            return false;
        }
        if (!cmdBufferCommandsResize(cmdBuffer, cmdBufferBuilder, cmdBufferBuilder->commandCapacity)) {
            return false;
        }
        cmdBuffer->layerCount = layerCount;
    } else {
        memset(cmdBuffer->commands, 0, cmdBufferBuilder->commandCapacity * sizeof(LdeCmdBufferGpuCmd));
    }

    return true;
}

bool ldeCmdBufferGpuBuild(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder,
                          bool tuRasterOrder)
{
    const uint32_t setResidualsStart = cmdBufferBuilder->residualAddCount;
    const uint32_t clearResidualsStart =
        cmdBufferBuilder->residualAddCount + cmdBufferBuilder->residualSetCount;
    const uint32_t newResidualCount = cmdBufferBuilder->residualAddCount +
                                      cmdBufferBuilder->residualSetCount +
                                      cmdBufferBuilder->residualClearAndSetCount;
    cmdBuffer->residualCount = newResidualCount;

    if (!cmdBuffer->residuals) {
        cmdBuffer->residuals = VNAllocateArray(cmdBuffer->allocator, &cmdBuffer->allocationResiduals,
                                               int16_t, newResidualCount);
        cmdBufferBuilder->residualCapacity = newResidualCount;
    } else if (newResidualCount > cmdBufferBuilder->residualCapacity) {
        cmdBuffer->residuals = VNReallocateArray(cmdBuffer->allocator, &cmdBuffer->allocationResiduals,
                                                 int16_t, newResidualCount);
        cmdBufferBuilder->residualCapacity = newResidualCount;
    }
    if (!cmdBuffer->residuals) {
        return false;
    }

    memcpy(cmdBuffer->residuals, cmdBufferBuilder->residualsAdd,
           cmdBufferBuilder->residualAddCount * sizeof(int16_t));
    if (!tuRasterOrder) {
        memcpy(&cmdBuffer->residuals[setResidualsStart], cmdBufferBuilder->residualsSet,
               cmdBufferBuilder->residualSetCount * sizeof(int16_t));
        memcpy(&cmdBuffer->residuals[clearResidualsStart], cmdBufferBuilder->residualsClearAndSet,
               cmdBufferBuilder->residualClearAndSetCount * sizeof(int16_t));

        for (uint32_t cmdIndex = 0; cmdIndex < cmdBuffer->commandCount; cmdIndex++) {
            LdeCmdBufferGpuCmd* cmd = &cmdBuffer->commands[cmdIndex];
            switch (cmd->operation) {
                case CBGOAdd: continue;
                case CBGOSet: cmd->dataOffset += setResidualsStart; break;
                case CBGOSetZero: continue;
                case CBGOClearAndSet: cmd->dataOffset += clearResidualsStart; break;
            }
        }
    }

    return true;
}

void ldeCmdBufferGpuFree(LdeCmdBufferGpu* cmdBuffer, LdeCmdBufferGpuBuilder* cmdBufferBuilder)
{
    if (cmdBufferBuilder) {
        VNFree(cmdBuffer->allocator, &cmdBufferBuilder->allocationAdd);
        VNFree(cmdBuffer->allocator, &cmdBufferBuilder->allocationSet);
        VNFree(cmdBuffer->allocator, &cmdBufferBuilder->allocationClearAndSet);
    }
    if (cmdBuffer) {
        VNFree(cmdBuffer->allocator, &cmdBuffer->allocationCommands);
        VNFree(cmdBuffer->allocator, &cmdBuffer->allocationResiduals);
    }
}
