/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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

// Cross-validation test: decode an LCEVC enhancement stream to both CPU and GPU
// command buffers, apply each to a separate destination plane, and verify that
// the results are identical.
//
// This confirms that the Vulkan GPU apply shader produces the same pixel output
// as the CPU apply path for real decoded enhancement data.
//
// Both block-order (temporal-on) and raster-order (temporal-off) streams are
// tested.  The GPU buffer row stride may include alignment padding
// (kBufferRowAlignment = 16 bytes); the apply shader handles this correctly.

#include "buffer_vulkan.h"
#include "test_utility.h"
//
#include <find_assets_dir.h>
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/enhancement/utilities.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
#include <LCEVC/pixel_processing/apply_cmdbuffer.h>
#include <LCEVC/utility/bin_reader.h>
//
#include <gtest/gtest.h>
//
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;
using namespace lcevc_dec::utility;

namespace {

const std::filesystem::path kTestAssets = findAssetsDir("src/enhancement/test/assets");

// Calculate the byte size needed for a single tile in the merged command buffer.
inline uint32_t tileBufferSize(const LdeCmdBufferGpu& buf)
{
    const uint32_t residualOff = static_cast<uint32_t>(sizeof(LdeCmdBufferGpuCmd)) * buf.commandCount;
    return residualOff + buf.residualCount * sizeof(uint16_t);
}

// Calculate the total merged buffer size for multiple tiles, including 4-byte alignment.
inline uint32_t mergedBufferSize(std::initializer_list<const LdeCmdBufferGpu*> buffers)
{
    uint32_t total = 0;
    for (const auto* buf : buffers) {
        const uint32_t sz = tileBufferSize(*buf);
        if (sz > 0) {
            total = (total + 3u) & ~3u;
            total += sz;
        }
    }
    return total;
}

} // namespace

// ---------------------------------------------------------------------------
// Test parameters
// ---------------------------------------------------------------------------

struct CrossValidationParams
{
    std::string binFile; // Stream asset filename (under kTestAssets)
    LdeLOQIndex loq;     // LOQ to decode and apply
    uint8_t planeIdx;    // Plane index (0 = Y)
    int frameIndex;      // 0-based frame index (how many extra getFrame() calls after SetUp)
};

void PrintTo(const CrossValidationParams& p, std::ostream* os)
{
    *os << "{bin=\"" << p.binFile << "\", loq=" << (p.loq == LOQ0 ? "LOQ0" : "LOQ1")
        << ", plane=" << static_cast<int>(p.planeIdx) << ", frame=" << p.frameIndex << "}";
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ApplyCrossValidation : public testing::TestWithParam<CrossValidationParams>
{
protected:
    void SetUp() override
    {
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        ldcAccelerationInitialize(true);

        m_allocator = ldcMemoryAllocatorMalloc();

        // --- Vulkan pipeline ---
        auto* pipelineBuilder = new PipelineBuilderVulkan(m_allocator);
        ASSERT_TRUE(pipelineBuilder);
        EventSink* eventSink = nullptr;
        m_pipeline = pipelineBuilder->finish(eventSink);
        delete pipelineBuilder;

        if (!static_cast<PipelineVulkan*>(m_pipeline.get())) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }

        // --- Stream parsing ---
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &m_globalConfig);
        ldeFrameConfigInitialize(m_allocator, &m_frameConfig);
        m_frameConfig.chunkAllocation = m_chunkAllocation;

        const auto params = GetParam();
        m_binReader = createBinReader((kTestAssets / params.binFile).string());
        ASSERT_TRUE(m_binReader) << "Failed to open BIN file: " << params.binFile;

        // Read the first frame
        ASSERT_TRUE(readNextFrame());

        // Advance to the requested frame
        for (int i = 0; i < params.frameIndex; ++i) {
            ASSERT_TRUE(readNextFrame()) << "Failed to read frame " << (i + 1);
        }
    }

    void TearDown() override
    {
        ldeCmdBufferCpuFree(&m_cmdBufferCpu);
        ldeCmdBufferGpuFree(&m_cmdBufferGpu, &m_cmdBufferGpuBuilder);
    }

    bool readNextFrame()
    {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> rawNalUnit;

        if (!m_binReader->read(decodeIndex, presentationIndex, rawNalUnit)) {
            return false;
        }

        bool globalConfigModified = false;
        return ldeConfigsParse(rawNalUnit.data(), rawNalUnit.size(), &m_globalConfig,
                               &m_frameConfig, &globalConfigModified);
    }

    // Decode the enhancement to a CPU command buffer.
    void decodeCpu(LdeLOQIndex loq, uint8_t planeIdx)
    {
        ASSERT_TRUE(ldeCmdBufferCpuInitialize(m_allocator, &m_cmdBufferCpu, 0));
        ASSERT_TRUE(ldeCmdBufferCpuReset(&m_cmdBufferCpu, m_globalConfig.numLayers));
        ASSERT_TRUE(ldeDecodeEnhancement(&m_globalConfig, &m_frameConfig, loq, planeIdx, 0,
                                         &m_cmdBufferCpu, nullptr, nullptr));
    }

    // Decode the enhancement to a GPU command buffer.
    void decodeGpu(LdeLOQIndex loq, uint8_t planeIdx)
    {
        ASSERT_TRUE(ldeCmdBufferGpuInitialize(m_allocator, &m_cmdBufferGpu, &m_cmdBufferGpuBuilder));
        ASSERT_TRUE(ldeCmdBufferGpuReset(&m_cmdBufferGpu, &m_cmdBufferGpuBuilder, m_globalConfig.numLayers));
        ASSERT_TRUE(ldeDecodeEnhancement(&m_globalConfig, &m_frameConfig, loq, planeIdx, 0, nullptr,
                                         &m_cmdBufferGpu, &m_cmdBufferGpuBuilder));
    }

    // Apply the CPU command buffer to a local int16_t plane.
    // The plane uses the supplied row byte stride so that the pixel addressing
    // matches the GPU buffer layout.
    // Returns the plane data as a dense width*height vector (stride padding stripped).
    std::vector<int16_t> applyCpu(LdeLOQIndex loq, uint8_t planeIdx, uint16_t planeWidth,
                                  uint16_t planeHeight, uint32_t rowByteStride)
    {
        const uint32_t pixelStride = rowByteStride / sizeof(int16_t);
        std::vector<int16_t> planeData(static_cast<size_t>(pixelStride) * planeHeight, 0);

        LdpPicturePlaneDesc planeDesc{};
        planeDesc.firstSample = reinterpret_cast<uint8_t*>(planeData.data());
        planeDesc.rowByteStride = rowByteStride;

        LdpEnhancementTile tile{};
        tile.buffer = m_cmdBufferCpu;
        tile.tileWidth = planeWidth;
        tile.tileHeight = planeHeight;
        tile.planeWidth = planeWidth;
        tile.planeHeight = planeHeight;
        tile.loq = loq;
        tile.plane = planeIdx;

        // Use LdpFPS10 (not LdpFPS8).  All signed FP types share the same S16
        // add/set/clear functions, but the CPU apply derives the pixel stride from
        // rowByteStride:  for bitdepth > 8 it divides by 2, for bitdepth == 8 it
        // does not.  LdpFPS8 has bitdepth 8 with int16_t storage, so the pixel
        // stride would be wrong.  LdpFPS10 (bitdepth 10) divides correctly.
        const bool rasterOrder = !m_globalConfig.temporalEnabled;
        EXPECT_TRUE(ldppApplyCmdBuffer(nullptr, nullptr, &tile, LdpFPS10, &planeDesc, rasterOrder,
                                       false, nullptr));

        // Extract valid pixel region (strip stride padding).
        std::vector<int16_t> result(static_cast<size_t>(planeWidth) * planeHeight, 0);
        for (uint32_t row = 0; row < planeHeight; ++row) {
            std::memcpy(result.data() + row * planeWidth, planeData.data() + row * pixelStride,
                        planeWidth * sizeof(int16_t));
        }
        return result;
    }

    // Apply the GPU command buffer to a Vulkan picture and read back the plane.
    // Returns the plane data as a dense width*height vector (stride padding stripped).
    std::vector<int16_t> applyGpu(LdeLOQIndex loq, uint8_t planeIdx, uint16_t planeWidth, uint16_t planeHeight)
    {
        auto* pipeline = static_cast<PipelineVulkan*>(m_pipeline.get());
        const bool dds = (m_globalConfig.numLayers == 16);
        const bool rasterOrder = !m_globalConfig.temporalEnabled;

        auto* picture = vulkan_test_util::allocateTestPictureAndBuffer(
            pipeline, LdpColorFormatI420_16_LE, planeWidth, planeHeight);
        auto* buffer = static_cast<BufferVulkan*>(picture->buffer);

        // Zero-initialise the buffer (same as the CPU plane).
        buffer->clear();

        // --- Vulkan apply ---
        VulkanApplyCommonArgs commonArgs{};
        commonArgs.picture = picture;
        commonArgs.planeWidth = planeWidth;
        commonArgs.planeHeight = planeHeight;
        commonArgs.plane = planeIdx;
        commonArgs.loq = loq;
        commonArgs.dds = dds;
        commonArgs.tuRasterOrder = rasterOrder;

        auto& compute = pipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
        EXPECT_NE(frameContext, nullptr);
        commonArgs.context = frameContext;

        compute.beginCompute(frameContext);
        if (loq == LOQ0) {
            if (!frameContext->prepareCommandBuffer(LOQ0, mergedBufferSize({&m_cmdBufferGpu})) ||
                !frameContext->prepareCommandBuffer(LOQ1, 0)) {
                return {};
            }
        } else {
            if (!frameContext->prepareCommandBuffer(LOQ0, 0) ||
                !frameContext->prepareCommandBuffer(LOQ1, mergedBufferSize({&m_cmdBufferGpu}))) {
                return {};
            }
        }

        EXPECT_TRUE(pipeline->backend().applyCommon(&commonArgs));

        VulkanApplyTileArgs tileArgs{};
        tileArgs.picture = picture;
        tileArgs.planeWidth = planeWidth;
        tileArgs.planeHeight = planeHeight;
        tileArgs.bufferGpu = m_cmdBufferGpu;
        tileArgs.plane = planeIdx;
        tileArgs.loq = loq;
        tileArgs.context = frameContext;
        tileArgs.tuRasterOrder = rasterOrder;

        EXPECT_TRUE(pipeline->backend().applyTile(&tileArgs));

        compute.endCompute(frameContext);
        pipeline->addCompletionContext(frameContext);
        pipeline->waitCompletionContextIdle();

        // --- Read back the plane ---
        const uint32_t layoutOffset = ldpPictureLayoutPlaneOffset(&picture->layout, planeIdx);
        const uint32_t layoutStride = ldpPictureLayoutRowStride(&picture->layout, planeIdx);

        LdpBufferMapping mapping{};
        EXPECT_TRUE(buffer->map(&mapping, 0, buffer->size(), LdpAccessRead));

        std::vector<int16_t> gpuPlane(static_cast<size_t>(planeWidth) * planeHeight, 0);

        for (uint32_t row = 0; row < planeHeight; ++row) {
            const uint8_t* src = mapping.ptr + layoutOffset + row * layoutStride;
            auto* dst = gpuPlane.data() + row * planeWidth;
            std::memcpy(dst, src, planeWidth * sizeof(int16_t));
        }

        buffer->unmap(&mapping);
        pipeline->freePicture(picture);

        return gpuPlane;
    }

    std::unique_ptr<Pipeline> m_pipeline;
    std::unique_ptr<BinReader> m_binReader;

    LdcMemoryAllocator* m_allocator{};
    LdcMemoryAllocation m_chunkAllocation{};
    LdeGlobalConfig m_globalConfig{};
    LdeFrameConfig m_frameConfig{};

    LdeCmdBufferCpu m_cmdBufferCpu{};
    LdeCmdBufferGpu m_cmdBufferGpu{};
    LdeCmdBufferGpuBuilder m_cmdBufferGpuBuilder{};
};

// ---------------------------------------------------------------------------
// Test body
// ---------------------------------------------------------------------------

TEST_P(ApplyCrossValidation, CpuAndGpuApplyMatch)
{
    const auto params = GetParam();

    // Get plane dimensions for this LOQ/plane
    uint16_t planeWidth = 0;
    uint16_t planeHeight = 0;
    ldePlaneDimensionsFromConfig(&m_globalConfig, params.loq, params.planeIdx, &planeWidth, &planeHeight);
    ASSERT_GT(planeWidth, 0);
    ASSERT_GT(planeHeight, 0);

    // Decode the same frame to both command buffer formats
    decodeCpu(params.loq, params.planeIdx);
    decodeGpu(params.loq, params.planeIdx);

    // Determine the GPU layout stride so the CPU plane uses the same stride.
    auto* pipeline = static_cast<PipelineVulkan*>(m_pipeline.get());
    uint32_t gpuRowByteStride = 0;
    {
        auto* tmpPic = vulkan_test_util::allocateTestPictureAndBuffer(pipeline, LdpColorFormatI420_16_LE,
                                                                      planeWidth, planeHeight);
        gpuRowByteStride = ldpPictureLayoutRowStride(&tmpPic->layout, params.planeIdx);
        pipeline->freePicture(tmpPic);
    }

    // Apply each command buffer to an identically initialised (zero) plane.
    const auto cpuResult =
        applyCpu(params.loq, params.planeIdx, planeWidth, planeHeight, gpuRowByteStride);
    const auto gpuResult = applyGpu(params.loq, params.planeIdx, planeWidth, planeHeight);

    ASSERT_EQ(cpuResult.size(), gpuResult.size());

    size_t diffCount = 0;
    size_t firstDiffIdx = 0;
    for (size_t i = 0; i < cpuResult.size(); ++i) {
        if (cpuResult[i] != gpuResult[i]) {
            if (diffCount == 0) {
                firstDiffIdx = i;
            }
            ++diffCount;
        }
    }

    EXPECT_EQ(diffCount, 0u) << "CPU and GPU apply differ at " << diffCount << " of "
                             << cpuResult.size() << " samples."
                             << " First difference at index " << firstDiffIdx
                             << " (x=" << (firstDiffIdx % planeWidth)
                             << ", y=" << (firstDiffIdx / planeWidth)
                             << "): cpu=" << cpuResult[firstDiffIdx]
                             << " gpu=" << gpuResult[firstDiffIdx] << " | stream=" << params.binFile
                             << " loq=" << (params.loq == LOQ0 ? "LOQ0" : "LOQ1")
                             << " plane=" << static_cast<int>(params.planeIdx)
                             << " frame=" << params.frameIndex << " (" << planeWidth << "x"
                             << planeHeight << ", stride=" << gpuRowByteStride << ")";
}

// ---------------------------------------------------------------------------
// Test name generator
// ---------------------------------------------------------------------------

std::string crossValidationTestName(const testing::TestParamInfo<CrossValidationParams>& info)
{
    const auto& p = info.param;
    std::stringstream ss;
    std::string bin = p.binFile;
    if (const auto pos = bin.rfind('.'); pos != std::string::npos) {
        bin = bin.substr(0, pos);
    }
    ss << bin << "_" << (p.loq == LOQ0 ? "LOQ0" : "LOQ1") << "_plane"
       << static_cast<int>(p.planeIdx) << "_frame" << p.frameIndex;
    return ss.str();
}

// ---------------------------------------------------------------------------
// Test instances -- exercise both temporal-on (block order) and temporal-off
// (raster order) streams across multiple LOQs and frames.
// ---------------------------------------------------------------------------

INSTANTIATE_TEST_SUITE_P(CrossValidation, ApplyCrossValidation,
                         testing::Values(
                             // Temporal ON (block-order apply) -- decode_temp_on.bin
                             CrossValidationParams{"decode_temp_on.bin", LOQ1, 0, 0},
                             CrossValidationParams{"decode_temp_on.bin", LOQ0, 0, 0},
                             CrossValidationParams{"decode_temp_on.bin", LOQ0, 0, 1},

                             // Temporal OFF (raster-order apply) -- decode_temp_off.bin
                             CrossValidationParams{"decode_temp_off.bin", LOQ1, 0, 0},
                             CrossValidationParams{"decode_temp_off.bin", LOQ0, 0, 0},
                             CrossValidationParams{"decode_temp_off.bin", LOQ0, 0, 1}),
                         crossValidationTestName);
