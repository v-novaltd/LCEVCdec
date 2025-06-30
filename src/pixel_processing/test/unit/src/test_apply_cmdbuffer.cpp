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

#include "fp_types.h"
#include "test_plane.h"

#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pixel_processing/apply_cmdbuffer.h>
#include <LCEVC/utility/md5.h>
#include <range/v3/view.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <stdint.h>

#include <sstream>

namespace rg = ranges;
namespace rv = ranges::views;
using namespace lcevc_dec::utility;

constexpr uint32_t kWidth = 180;
constexpr uint32_t kHeight = 100;

typedef struct applyCmdBufferTestParams
{
    uint8_t transformSize;
    LdpFixedPoint fixedPoint;
    uint16_t entryPoints;
    bool surfaceRasterOrder;
    bool forceScalar;
    bool highlight;
    std::string hash;
} applyCmdBufferTestParams;

class ApplyCmdBuffer : public testing::TestWithParam<applyCmdBufferTestParams>
{
protected:
    void fillCmdBuffer(uint16_t entryPoints, LdpFixedPoint fixedPoint, bool surfaceRasterOrder)
    {
        int16_t residuals[16] = {128,  256,  384,  512,  640,  768,  896,  1024,
                                 1152, 1280, 1408, 1536, 1664, 1792, 1920, 2024};
        if (!surfaceRasterOrder) {
            const LdeCmdBufferCpuCmd setCmd = fixedPointIsSigned(fixedPoint) ? CBCCSet : CBCCAdd;
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, setCmd, residuals, 2);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 1);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCClear, residuals, 61);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, setCmd, residuals, 0);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 1);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer,
                                  fixedPointIsSigned(fixedPoint) ? CBCCSetZero : CBCCAdd, residuals, 295);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, setCmd, residuals, 193);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 19);
        } else {
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 0);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 19);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 170);
            ldeCmdBufferCpuAppend(&enhancementTile.buffer, CBCCAdd, residuals, 134);
        }
        if (entryPoints > 0) {
            ldeCmdBufferCpuSplit(&enhancementTile.buffer);
        }
    }

    void SetUp() override
    {
        const applyCmdBufferTestParams params = GetParam();
        uint32_t threadTaskCount = params.entryPoints ? params.entryPoints : 1;

        allocator = ldcMemoryAllocatorMalloc();
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        ldcTaskPoolInitialize(&taskPool, ldcMemoryAllocatorMalloc(), ldcMemoryAllocatorMalloc(),
                              threadTaskCount, threadTaskCount);
        testPlane.initialize(kWidth, kHeight, kWidth, params.fixedPoint);
        ldeCmdBufferCpuInitialize(allocator, &enhancementTile.buffer, params.entryPoints);
        ldeCmdBufferCpuReset(&enhancementTile.buffer, params.transformSize);
        // Set all plane values to grey for later tests
        memset(testPlane.planeDesc.firstSample, 100, testPlane.size());

        enhancementTile.tileWidth = kWidth;
        enhancementTile.tileHeight = kHeight;
        enhancementTile.planeWidth = kWidth;
        enhancementTile.planeHeight = kHeight;
    }
    void TearDown() override
    {
        ldeCmdBufferCpuFree(&enhancementTile.buffer);
        ldcTaskPoolDestroy(&taskPool);
    }

    LdcMemoryAllocator* allocator = {};
    LdcTaskPool taskPool = {};
    LdpEnhancementTile enhancementTile = {0};
    TestPlane testPlane = {};
};
class ApplyCmdBufferHash : public ApplyCmdBuffer
{
protected:
    std::string hashPlane()
    {
        {
            hash.reset();
            hash.update(testPlane.planeDesc.firstSample, static_cast<uint32_t>(testPlane.size()));
            return hash.hexDigest();
        }
    }
    MD5 hash;
};

TEST_P(ApplyCmdBuffer, AllCombinations)
{
    const applyCmdBufferTestParams params = GetParam();
    fillCmdBuffer(params.entryPoints, params.fixedPoint, params.surfaceRasterOrder);
    EXPECT_TRUE(ldppApplyCmdBuffer(&taskPool, NULL, &enhancementTile, params.fixedPoint, &testPlane.planeDesc,
                                   params.surfaceRasterOrder, params.forceScalar, params.highlight));
}

std::string testNames(const testing::TestParamInfo<applyCmdBufferTestParams>& value)
{
    const applyCmdBufferTestParams params = value.param;

    std::string transform = params.transformSize == 16 ? "DDS_" : "DD_";
    std::string raster = params.surfaceRasterOrder ? "raster_" : "block_";
    std::string simd = params.forceScalar ? "simdOff_" : "simdOn_";
    std::string highlight = params.highlight ? "highlightOn" : "highlightOff";
    std::stringstream ss;
    ss << transform << fixedPointToString(params.fixedPoint) << "_" << params.entryPoints
       << "entrypoints_" << raster << simd << highlight;
    return ss.str();
}

const std::vector<uint8_t> kTransformSizes = {4, 16};
const std::vector<uint16_t> kEntryPoints = {0};
const std::vector<bool> kBools = {true, false};
const std::vector<LdpFixedPoint> kFixedPointAll = {LdpFPU8, LdpFPU10, LdpFPU12, LdpFPU14,
                                                   LdpFPS8, LdpFPS10, LdpFPS12, LdpFPS14};

const auto kCmdBufferParams =
    rv::cartesian_product(kTransformSizes, kFixedPointAll, kEntryPoints, kBools, kBools, kBools) |
    rv::transform([](auto value) {
        return applyCmdBufferTestParams{std::get<0>(value), std::get<1>(value), std::get<2>(value),
                                        std::get<3>(value), std::get<4>(value), std::get<5>(value)};
    }) |
    rg::to_vector;

INSTANTIATE_TEST_SUITE_P(AllCombinations, ApplyCmdBuffer, testing::ValuesIn(kCmdBufferParams), testNames);

TEST_P(ApplyCmdBufferHash, HashPlane)
{
    const applyCmdBufferTestParams params = GetParam();
    fillCmdBuffer(params.entryPoints, params.fixedPoint, params.surfaceRasterOrder);
    EXPECT_TRUE(ldppApplyCmdBuffer(&taskPool, NULL, &enhancementTile, params.fixedPoint, &testPlane.planeDesc,
                                   params.surfaceRasterOrder, params.forceScalar, params.highlight));
    EXPECT_EQ(hashPlane(), params.hash);
}

INSTANTIATE_TEST_SUITE_P(
    HashPlane, ApplyCmdBufferHash,
    testing::Values(
        // transformSize, fixedPoint, entrypoints, surfaceRasterOrder, forceScalar, highlight, hash
        applyCmdBufferTestParams{4, LdpFPU8, 0, false, true, false, "f2468e478689739ea95e7daf9b1c5d4e"},
        applyCmdBufferTestParams{4, LdpFPU8, 0, false, false, false, "f2468e478689739ea95e7daf9b1c5d4e"},
        applyCmdBufferTestParams{4, LdpFPU8, 2, false, false, false, "f2468e478689739ea95e7daf9b1c5d4e"},
        applyCmdBufferTestParams{4, LdpFPS8, 0, false, false, false, "9495d255bfab0bdbb06ac305bfff1e21"},
        applyCmdBufferTestParams{16, LdpFPS8, 0, false, false, false, "8a28bc2f91d597679712d6d717cb2800"},
        applyCmdBufferTestParams{16, LdpFPS10, 0, false, false, false, "0ff4a90a59968a54e35ee34d8ab7da57"},
        applyCmdBufferTestParams{16, LdpFPS8, 0, true, false, false, "1601070b19d3f68761b18b5693d6c89d"},
        applyCmdBufferTestParams{16, LdpFPS10, 0, true, false, false, "43f8e9f02215913b66f1ab2ff51c022e"},
        applyCmdBufferTestParams{16, LdpFPU12, 0, true, false, false, "9ad5b2cd7aa4115fea6f9d51e38c670c"},
        applyCmdBufferTestParams{16, LdpFPU12, 3, true, false, false, "9ad5b2cd7aa4115fea6f9d51e38c670c"},
        applyCmdBufferTestParams{16, LdpFPS8, 0, false, false, true, "6fc6eee07ccad0a2f1d271360d9da5aa"},
        applyCmdBufferTestParams{4, LdpFPU10, 0, false, false, true, "d8e7eb2cee934527d5cf0c49bc86b441"}),
    testNames);