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

#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>

class CmdBuffersGpu : public testing::Test
{
public:
    void SetUp() override { allocator = ldcMemoryAllocatorMalloc(); }

    void TearDown() override { ldeCmdBufferGpuFree(&cmdBuffer, &cmdBufferBuilder); }
    const void incrementResiduals(int16_t* residuals, const uint32_t layerCount)
    {
        const int16_t newVal = residuals[0] + 1;
        for (uint32_t layer = 0; layer < layerCount; layer++) {
            residuals[layer] = newVal;
        }
    }
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation bufferAllocation;
    LdcMemoryAllocation bufferCommandsAllocation;
    LdcMemoryAllocation bufferResidualsAllocation;
    LdcMemoryAllocation builderAllocation;
    LdcMemoryAllocation builderAddAllocation;
    LdcMemoryAllocation builderSetAllocation;
    LdcMemoryAllocation builderClearAndSetAllocation;
    LdeCmdBufferGpu cmdBuffer = {};
    LdeCmdBufferGpuBuilder cmdBufferBuilder = {};
};

TEST_F(CmdBuffersGpu, initializeCmdbuffers)
{
    EXPECT_EQ(ldeCmdBufferGpuInitialize(allocator, &cmdBuffer, &cmdBufferBuilder), true);
    EXPECT_EQ(ldeCmdBufferGpuReset(&cmdBuffer, &cmdBufferBuilder, 4), true);
    EXPECT_EQ(cmdBuffer.layerCount, 4);
}

TEST_F(CmdBuffersGpu, addCommandsAndBuild)
{
    EXPECT_EQ(ldeCmdBufferGpuInitialize(allocator, &cmdBuffer, &cmdBufferBuilder), true);
    static const uint32_t kLayerCount = 16;
    int16_t residuals[kLayerCount] = {0};
    ldeCmdBufferGpuReset(&cmdBuffer, &cmdBufferBuilder, kLayerCount);

    EXPECT_EQ(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 5), true);
    EXPECT_EQ(cmdBuffer.commandCount, 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBuffer.commandCount - 1].blockIndex, 0);
    EXPECT_EQ(cmdBufferBuilder.currentAddCmd, cmdBuffer.commandCount - 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitCount, 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitStart, 5);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitmask[0], 0x400000000000000);
    EXPECT_EQ(cmdBufferBuilder.residualAddCount, kLayerCount * 1);

    incrementResiduals(residuals, kLayerCount); // 1
    EXPECT_EQ(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 63), true);
    EXPECT_EQ(cmdBuffer.commandCount, 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBuffer.commandCount - 1].blockIndex, 0);
    EXPECT_EQ(cmdBufferBuilder.currentAddCmd, cmdBuffer.commandCount - 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitCount, 2);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitStart, 5);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitmask[0], 0x400000000000001);
    EXPECT_EQ(cmdBufferBuilder.residualAddCount, kLayerCount * 2);

    incrementResiduals(residuals, kLayerCount); // 2
    EXPECT_EQ(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOSet, residuals, 2), true);
    EXPECT_EQ(cmdBuffer.commandCount, 2);
    EXPECT_EQ(cmdBuffer.commands[cmdBuffer.commandCount - 1].blockIndex, 0);
    EXPECT_EQ(cmdBufferBuilder.currentSetCmd, cmdBuffer.commandCount - 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentSetCmd].bitCount, 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentSetCmd].bitStart, 2);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentSetCmd].bitmask[0], 0x2000000000000000);
    EXPECT_EQ(cmdBufferBuilder.residualSetCount, kLayerCount * 1);

    incrementResiduals(residuals, kLayerCount); // 3
    EXPECT_EQ(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 64), true);
    EXPECT_EQ(cmdBuffer.commandCount, 3);
    EXPECT_EQ(cmdBuffer.commandCount - 1, cmdBufferBuilder.currentAddCmd);
    EXPECT_EQ(cmdBuffer.commands[cmdBuffer.commandCount - 1].blockIndex, 1);
    EXPECT_EQ(cmdBufferBuilder.currentAddCmd, cmdBuffer.commandCount - 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitCount, 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitStart, 0);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentAddCmd].bitmask[0], 0x8000000000000000);
    EXPECT_EQ(cmdBufferBuilder.residualAddCount, kLayerCount * 3);

    EXPECT_EQ(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOSetZero, residuals, 2038), true);
    EXPECT_EQ(cmdBuffer.commandCount, 4);
    EXPECT_EQ(cmdBuffer.commands[cmdBuffer.commandCount - 1].blockIndex, 31);
    EXPECT_EQ(cmdBufferBuilder.currentSetZeroCmd, cmdBuffer.commandCount - 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentSetZeroCmd].bitCount, 1);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentSetZeroCmd].bitStart, 54);
    EXPECT_EQ(cmdBuffer.commands[cmdBufferBuilder.currentSetZeroCmd].bitmask[0], 0x200);

    EXPECT_EQ(ldeCmdBufferGpuBuild(&cmdBuffer, &cmdBufferBuilder), true);
    EXPECT_EQ(cmdBuffer.residualCount, 64);
    EXPECT_EQ(cmdBufferBuilder.residualCapacity, 64);
    EXPECT_EQ(cmdBuffer.commands[0].dataOffset, 0);
    EXPECT_EQ(cmdBuffer.residuals[0 * kLayerCount], 0); // Add
    EXPECT_EQ(cmdBuffer.residuals[1 * kLayerCount], 1); // Add
    EXPECT_EQ(cmdBuffer.commands[1].dataOffset, 3 * kLayerCount);
    EXPECT_EQ(cmdBuffer.residuals[3 * kLayerCount], 2); // Set (at the end of residuals)
    EXPECT_EQ(cmdBuffer.commands[2].dataOffset, 2 * kLayerCount);
    EXPECT_EQ(cmdBuffer.residuals[2 * kLayerCount], 3); // Add
    EXPECT_EQ(cmdBuffer.commands[3].dataOffset, 0);     // SetZero - no data, no offset
}
