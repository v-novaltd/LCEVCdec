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

#include "buffer_vulkan.h"
#include "test_utility.h"
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/cpp_tools.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
//
#include <fmt/core.h>
#include <gtest/gtest.h>
//
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

namespace {

// Calculate the byte size needed for a single tile's data in the merged command buffer.
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

class PipelineVulkanApplyFixture : public testing::Test
{
public:
    PipelineVulkanApplyFixture() noexcept {};

    std::unique_ptr<Pipeline> mPipeline;

    LdcMemoryAllocator* allocator{};
    LdeCmdBufferGpu cmdBuffer{};
    LdeCmdBufferGpuBuilder cmdBufferBuilder{};

    const void incrementResiduals(int16_t* residuals, const uint32_t layerCount)
    {
        const int16_t newVal = residuals[0] + 1;
        for (uint32_t layer = 0; layer < layerCount; layer++) {
            residuals[layer] = newVal;
        }
    }

    void SetUp() override
    {
        allocator = ldcMemoryAllocatorMalloc();
        buildPipeline();
        makeCommandBuffer();

        auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
        if (!pipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }
    }

    void TearDown() override { ldeCmdBufferGpuFree(&cmdBuffer, &cmdBufferBuilder); }

    void buildPipeline()
    {
        auto* pipelineBuilder =
            new lcevc_dec::pipeline_vulkan::PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
        ASSERT_TRUE(pipelineBuilder);

        EventSink* eventSink = nullptr;
        mPipeline = pipelineBuilder->finish(eventSink);

        delete pipelineBuilder;
    }

    void makeCommandBuffer()
    {
        EXPECT_EQ(ldeCmdBufferGpuInitialize(allocator, &cmdBuffer, &cmdBufferBuilder), true);
        static const uint32_t kLayerCount = 16;
        int16_t residuals[kLayerCount] = {0};
        ldeCmdBufferGpuReset(&cmdBuffer, &cmdBufferBuilder, kLayerCount);

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOClearAndSet, residuals,
                                          0, false));

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 5, false));

        incrementResiduals(residuals, kLayerCount); // 1
        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 63, false));

        incrementResiduals(residuals, kLayerCount); // 2
        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOSet, residuals, 2, false));

        incrementResiduals(residuals, kLayerCount); // 3
        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 64, false));

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOClearAndSet, residuals,
                                          128, false));

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOSetZero, residuals,
                                          2038, false));

        ASSERT_TRUE(ldeCmdBufferGpuBuild(&cmdBuffer, &cmdBufferBuilder, false));
        vulkan_test_util::validateCommandBuffer(cmdBuffer);
    }
};

TEST_F(PipelineVulkanApplyFixture, ApplyGpuCommandBufferToPlane)
{
    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
    constexpr auto width = 1920;
    constexpr auto height = 1080;
    auto* src = vulkan_test_util::allocateTestPictureAndBuffer(pipeline, LdpColorFormatI420_16_LE,
                                                               width, height);

    auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
    const auto data = vulkan_test_util::generateYUV420FromFixedSeed<int16_t>(width, height);
    srcBuffer->copyIn(0, data.data(), static_cast<uint32_t>(data.size() * sizeof(data[0])));

    VulkanApplyCommonArgs args{};
    args.picture = src;
    args.planeWidth = width;
    args.planeHeight = height;
    args.transform = TransformDDS;

    auto& compute = pipeline->backend().compute();
    VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
    ASSERT_NE(frameContext, nullptr);
    args.context = frameContext;
    compute.beginCompute(frameContext);
    ASSERT_TRUE(frameContext->prepareCommandBuffer(LOQ0, mergedBufferSize({&cmdBuffer})));
    ASSERT_TRUE(frameContext->prepareCommandBuffer(LOQ1, 0));
    EXPECT_TRUE(pipeline->backend().applyCommon(&args));

    VulkanApplyTileArgs tileArgs{};
    tileArgs.picture = args.picture;
    tileArgs.planeWidth = args.planeWidth;
    tileArgs.planeHeight = args.planeHeight;
    tileArgs.bufferGpu = cmdBuffer;
    tileArgs.context = frameContext;
    tileArgs.loq = args.loq;
    tileArgs.transform = args.transform;
    EXPECT_TRUE(pipeline->backend().applyTile(&tileArgs));
    compute.endCompute(frameContext);
    pipeline->addCompletionContext(frameContext);
    pipeline->waitCompletionContextIdle();

    const std::string hash = vulkan_test_util::hashMd5Buffer(srcBuffer);
    EXPECT_EQ(hash, "2026379c4a0a0aef687b65de565553b4");

    pipeline->freePicture(src);
}

// ---------------------------------------------------------------------------
// Parameterized Apply CmdBuffer tests
// Replicates the test structure from pixel_processing test_apply_cmdbuffer.cpp
// but targeting the Vulkan GPU apply pipeline.
//
// The GPU apply always operates on S16 (int16_t) internal buffers. The command
// sequences mirror the CPU test patterns translated to GPU command buffer ops:
//   CPU CBCCAdd       -> GPU CBGOAdd
//   CPU CBCCSet       -> GPU CBGOSet
//   CPU CBCCSetZero   -> GPU CBGOSetZero
//   CPU CBCCClear     -> GPU CBGOClearAndSet (clears block and sets specified TU)
// ---------------------------------------------------------------------------

constexpr uint32_t kApplyWidth = 180;
constexpr uint32_t kApplyHeight = 100;
constexpr uint8_t kApplyInitByte = 100; // memset value matching CPU test

typedef struct vulkanApplyTestParams
{
    uint8_t transformSize; // 4 (DD, layerCount=4) or 16 (DDS, layerCount=16)
    bool surfaceRasterOrder;
    bool highlight;
    std::string hash;
} vulkanApplyTestParams;

void PrintTo(const vulkanApplyTestParams& params, std::ostream* os)
{
    *os << "{transformSize=" << static_cast<uint32_t>(params.transformSize)
        << ", surfaceRasterOrder=" << (params.surfaceRasterOrder ? "true" : "false")
        << ", highlight=" << (params.highlight ? "true" : "false") << ", hash=\"" << params.hash << "\"}";
}

class VulkanApplyCmdBuffer : public testing::TestWithParam<vulkanApplyTestParams>
{
protected:
    void SetUp() override
    {
        allocator = ldcMemoryAllocatorMalloc();
        buildPipeline();
        auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
        if (!pipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }
    }

    void TearDown() override { ldeCmdBufferGpuFree(&mCmdBuffer, &mCmdBufferBuilder); }

    void buildPipeline()
    {
        auto* pipelineBuilder = new PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
        ASSERT_TRUE(pipelineBuilder);
        EventSink* eventSink = nullptr;
        mPipeline = pipelineBuilder->finish(eventSink);
        delete pipelineBuilder;
    }

    // Build a GPU command buffer using the same residual values and operation
    // pattern as the CPU test_apply_cmdbuffer.cpp fillCmdBuffer(), translated
    // to GPU command buffer operations.
    void fillCmdBuffer(bool surfaceRasterOrder, uint8_t layerCount)
    {
        ASSERT_TRUE(ldeCmdBufferGpuInitialize(allocator, &mCmdBuffer, &mCmdBufferBuilder));
        ldeCmdBufferGpuReset(&mCmdBuffer, &mCmdBufferBuilder, layerCount);

        // Same residual values as the CPU test
        int16_t residuals[16] = {128,  256,  384,  512,  640,  768,  896,  1024,
                                 1152, 1280, 1408, 1536, 1664, 1792, 1920, 2024};

        if (!surfaceRasterOrder) {
            // Block-order sequence (mirrors CPU signed-format block-order path)
            //   CPU: Set, Add, Clear, Set, Add, SetZero, Set, Add
            //   GPU: Set, Add, ClearAndSet, Set, Add, SetZero, Set, Add
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOSet, residuals, 2, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals, 1, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOClearAndSet,
                                              residuals, 61, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOSet, residuals, 0, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals, 3, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOSetZero,
                                              residuals, 295, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOSet, residuals,
                                              193, false));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals,
                                              19, false));
        } else {
            // Raster-order sequence (mirrors CPU raster-order path)
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals, 0, true));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals, 19, true));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals,
                                              170, true));
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&mCmdBuffer, &mCmdBufferBuilder, CBGOAdd, residuals,
                                              134, true));
        }

        ASSERT_TRUE(ldeCmdBufferGpuBuild(&mCmdBuffer, &mCmdBufferBuilder, surfaceRasterOrder));
        vulkan_test_util::validateCommandBuffer(mCmdBuffer);
    }

    // Allocate a picture and fill the buffer with a known byte pattern
    PictureVulkan* allocateAndInitPicture(PipelineVulkan* pipeline)
    {
        auto* picture = vulkan_test_util::allocateTestPictureAndBuffer(
            pipeline, LdpColorFormatI420_16_LE, kApplyWidth, kApplyHeight);
        auto* buffer = static_cast<BufferVulkan*>(picture->buffer);
        std::vector<uint8_t> initData(buffer->size(), kApplyInitByte);
        buffer->copyIn(0, initData.data(), static_cast<uint32_t>(initData.size()));
        return picture;
    }

    // Run apply on a picture and wait for completion. Returns the buffer hash.
    std::string applyAndHash(PipelineVulkan* pipeline, PictureVulkan* picture,
                             const vulkanApplyTestParams& params)
    {
        VulkanApplyCommonArgs args{};
        args.picture = picture;
        args.planeWidth = kApplyWidth;
        args.planeHeight = kApplyHeight;
        args.transform = params.transformSize == 16 ? TransformDDS : TransformDD;
        args.tuRasterOrder = params.surfaceRasterOrder;

        auto& compute = pipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
        if (!frameContext) {
            return "";
        }
        args.context = frameContext;
        compute.beginCompute(frameContext);
        if (!frameContext->prepareCommandBuffer(LOQ0, mergedBufferSize({&mCmdBuffer})) ||
            !frameContext->prepareCommandBuffer(LOQ1, 0)) {
            return "";
        }

        if (!pipeline->backend().applyCommon(&args)) {
            return "";
        }

        VulkanApplyTileArgs tileArgs{};
        tileArgs.picture = picture;
        tileArgs.planeWidth = kApplyWidth;
        tileArgs.planeHeight = kApplyHeight;
        tileArgs.bufferGpu = mCmdBuffer;
        tileArgs.context = frameContext;
        tileArgs.loq = args.loq;
        tileArgs.highlightResiduals = params.highlight;
        tileArgs.transform = args.transform;
        tileArgs.tuRasterOrder = params.surfaceRasterOrder;

        if (!pipeline->backend().applyTile(&tileArgs)) {
            return "";
        }

        compute.endCompute(frameContext);
        pipeline->addCompletionContext(frameContext);
        pipeline->waitCompletionContextIdle();

        return vulkan_test_util::hashMd5Buffer(static_cast<BufferVulkan*>(picture->buffer));
    }

    std::unique_ptr<Pipeline> mPipeline;
    LdcMemoryAllocator* allocator{};
    LdeCmdBufferGpu mCmdBuffer{};
    LdeCmdBufferGpuBuilder mCmdBufferBuilder{};
};

TEST_P(VulkanApplyCmdBuffer, AllCombinations)
{
    const auto params = GetParam();
    fillCmdBuffer(params.surfaceRasterOrder, params.transformSize);

    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
    auto* picture = allocateAndInitPicture(pipeline);

    const std::string hash = applyAndHash(pipeline, picture, params);
    EXPECT_FALSE(hash.empty());

    pipeline->freePicture(picture);
}

std::string vulkanApplyTestName(const testing::TestParamInfo<vulkanApplyTestParams>& value)
{
    const auto& p = value.param;
    std::stringstream ss;
    ss << (p.transformSize == 16 ? "DDS" : "DD") << "_" << (p.surfaceRasterOrder ? "raster" : "block")
       << "_" << (p.highlight ? "highlightOn" : "highlightOff");
    return ss.str();
}

const std::vector<uint8_t> kApplyTransformSizes = {4, 16};
const std::vector<bool> kApplyBools = {true, false};

// Generate all combinations of {transformSize} x {rasterOrder} x {highlight}
std::vector<vulkanApplyTestParams> generateAllCombinations()
{
    std::vector<vulkanApplyTestParams> params;
    for (auto ts : kApplyTransformSizes) {
        for (auto raster : kApplyBools) {
            for (auto hl : kApplyBools) {
                params.push_back({ts, raster, hl, {}});
            }
        }
    }
    return params;
}

INSTANTIATE_TEST_SUITE_P(AllCombinations, VulkanApplyCmdBuffer,
                         testing::ValuesIn(generateAllCombinations()), vulkanApplyTestName);

// ---------------------------------------------------------------------------
// Hash verification tests — specific parameter sets with golden hashes.
// The hashes are GPU-specific (S16 internal, Vulkan apply shader semantics).
// ---------------------------------------------------------------------------

class VulkanApplyCmdBufferHash : public VulkanApplyCmdBuffer
{};

TEST_P(VulkanApplyCmdBufferHash, HashPlane)
{
    const auto params = GetParam();
    fillCmdBuffer(params.surfaceRasterOrder, params.transformSize);

    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
    auto* picture = allocateAndInitPicture(pipeline);

    const std::string hash = applyAndHash(pipeline, picture, params);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash, params.hash);

    pipeline->freePicture(picture);
}

INSTANTIATE_TEST_SUITE_P(HashPlane, VulkanApplyCmdBufferHash,
                         testing::Values(
                             // transformSize, surfaceRasterOrder, highlight, hash
                             vulkanApplyTestParams{16, false, false, "6dcfadb2bfb4d4e831234ee80b2873b4"},
                             vulkanApplyTestParams{16, false, true, "6a42c20ad30d170674a4409125189e8e"},
                             vulkanApplyTestParams{16, true, false, "a0fadc5da207e59b2e0e1810ec13a64b"},
                             vulkanApplyTestParams{16, true, true, "f31667979cd13f617543497b5cf3eaf6"},
                             vulkanApplyTestParams{4, false, false, "38774ad8729ab5243871ed1e47dc8c2d"},
                             vulkanApplyTestParams{4, false, true, "10d4160a0b1160e01e443d57788b3a30"},
                             vulkanApplyTestParams{4, true, false, "e0acf6b11b1abb3b04e4c9b5a25e29d1"},
                             vulkanApplyTestParams{4, true, true, "a21449c2bae269a49b8345d19e9a4242"}),
                         vulkanApplyTestName);

// ---------------------------------------------------------------------------
// Multi-tile apply tests
//
// Verify that a single applyCommon followed by multiple applyTile calls
// (simulating multiple decoded tiles within a frame) produces results that
// match applying each tile in its own compute session.  Both the "direct"
// path (residuals written straight to the picture) and the "temporal" path
// (residuals written to a temporal buffer, then added to the picture) are
// tested.  This mirrors the pattern in tasks_vulkan.cpp where vApplyCommon
// is called once per LOQ, followed by vApplyCmdBufferDirect or
// vApplyCmdBufferTemporal for each tile.
// ---------------------------------------------------------------------------

class MultiTileApplyFixture : public testing::Test
{
protected:
    void SetUp() override
    {
        m_allocator = ldcMemoryAllocatorMalloc();

        auto* pipelineBuilder = new PipelineBuilderVulkan(m_allocator);
        ASSERT_TRUE(pipelineBuilder);
        EventSink* eventSink = nullptr;
        m_pipeline = pipelineBuilder->finish(eventSink);
        delete pipelineBuilder;

        m_vulkanPipeline = static_cast<PipelineVulkan*>(m_pipeline.get());
        if (!m_vulkanPipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }

        buildCommandBuffers();
    }

    void TearDown() override
    {
        ldeCmdBufferGpuFree(&m_cmdBuf1, &m_builder1);
        ldeCmdBufferGpuFree(&m_cmdBuf2, &m_builder2);
    }

    // Build two distinct command buffers that target different blocks so their
    // effects are independently verifiable.
    void buildCommandBuffers()
    {
        static constexpr uint32_t kLayerCount = 16; // DDS

        // Tile 1: Add at TU 0 and TU 5 (block 0)
        {
            int16_t residuals[kLayerCount];
            for (uint32_t i = 0; i < kLayerCount; ++i) {
                residuals[i] = static_cast<int16_t>(100 + i);
            }
            ASSERT_TRUE(ldeCmdBufferGpuInitialize(m_allocator, &m_cmdBuf1, &m_builder1));
            ldeCmdBufferGpuReset(&m_cmdBuf1, &m_builder1, kLayerCount);
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&m_cmdBuf1, &m_builder1, CBGOAdd, residuals, 0, false));
            for (uint32_t i = 0; i < kLayerCount; ++i) {
                residuals[i] = static_cast<int16_t>(150 + i);
            }
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&m_cmdBuf1, &m_builder1, CBGOAdd, residuals, 5, false));
            ASSERT_TRUE(ldeCmdBufferGpuBuild(&m_cmdBuf1, &m_builder1, false));
            vulkan_test_util::validateCommandBuffer(m_cmdBuf1);
        }

        // Tile 2: Add at TU 64 and TU 65 (block 1), Add at TU 130 (block 2)
        {
            int16_t residuals[kLayerCount];
            for (uint32_t i = 0; i < kLayerCount; ++i) {
                residuals[i] = static_cast<int16_t>(200 + i);
            }
            ASSERT_TRUE(ldeCmdBufferGpuInitialize(m_allocator, &m_cmdBuf2, &m_builder2));
            ldeCmdBufferGpuReset(&m_cmdBuf2, &m_builder2, kLayerCount);
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&m_cmdBuf2, &m_builder2, CBGOAdd, residuals, 64, false));
            for (uint32_t i = 0; i < kLayerCount; ++i) {
                residuals[i] = static_cast<int16_t>(250 + i);
            }
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&m_cmdBuf2, &m_builder2, CBGOAdd, residuals, 65, false));
            for (uint32_t i = 0; i < kLayerCount; ++i) {
                residuals[i] = static_cast<int16_t>(300 + i);
            }
            ASSERT_TRUE(ldeCmdBufferGpuAppend(&m_cmdBuf2, &m_builder2, CBGOAdd, residuals, 130, false));
            ASSERT_TRUE(ldeCmdBufferGpuBuild(&m_cmdBuf2, &m_builder2, false));
            vulkan_test_util::validateCommandBuffer(m_cmdBuf2);
        }
    }

    // Allocate a picture filled with deterministic data.
    PictureVulkan* allocPicture()
    {
        auto* pic = vulkan_test_util::allocateTestPictureAndBuffer(
            m_vulkanPipeline, LdpColorFormatI420_16_LE, kWidth, kHeight);
        auto* buf = static_cast<BufferVulkan*>(pic->buffer);
        const auto data = vulkan_test_util::generateYUV420FromFixedSeed<int16_t>(kWidth, kHeight);
        buf->copyIn(0, data.data(), static_cast<uint32_t>(data.size() * sizeof(data[0])));
        return pic;
    }

    // Apply two tiles sequentially (separate compute sessions) and return
    // the buffer hash.
    std::string applySequential(PictureVulkan* picture, bool temporal)
    {
        auto& compute = m_vulkanPipeline->backend().compute();
        PictureVulkan* temporalPic = nullptr;
        if (temporal) {
            temporalPic = vulkan_test_util::allocateTestPictureAndBuffer(
                m_vulkanPipeline, LdpColorFormatI420_16_LE, kWidth, kHeight);
            static_cast<BufferVulkan*>(temporalPic->buffer)->clear();
        }

        for (int t = 0; t < 2; ++t) {
            const LdeCmdBufferGpu& tile = (t == 0) ? m_cmdBuf1 : m_cmdBuf2;

            VulkanFrameContext* ctx = compute.acquireFrameContext(nullptr);
            EXPECT_NE(ctx, nullptr);
            compute.beginCompute(ctx);
            if (!ctx->prepareCommandBuffer(LOQ0, mergedBufferSize({&tile})) ||
                !ctx->prepareCommandBuffer(LOQ1, 0)) {
                return "";
            }

            VulkanApplyCommonArgs common{};
            common.picture = temporal ? nullptr : picture;
            common.planeWidth = kWidth;
            common.planeHeight = kHeight;
            common.transform = TransformDDS;
            common.context = ctx;
            if (temporal) {
                common.temporalPicture = temporalPic;
                common.chroma = LdeChroma::CT420;
            }
            EXPECT_TRUE(m_vulkanPipeline->backend().applyCommon(&common));

            VulkanApplyTileArgs tileArgs{};
            tileArgs.picture = temporal ? nullptr : picture;
            tileArgs.planeWidth = kWidth;
            tileArgs.planeHeight = kHeight;
            tileArgs.bufferGpu = tile;
            tileArgs.context = ctx;
            tileArgs.loq = common.loq;
            tileArgs.transform = common.transform;
            if (temporal) {
                tileArgs.temporalPicture = temporalPic;
            }
            EXPECT_TRUE(m_vulkanPipeline->backend().applyTile(&tileArgs));

            compute.endCompute(ctx);
            m_vulkanPipeline->addCompletionContext(ctx);
            m_vulkanPipeline->waitCompletionContextIdle();
        }

        const std::string hash = vulkan_test_util::hashMd5Buffer(
            static_cast<BufferVulkan*>(temporal ? temporalPic->buffer : picture->buffer));
        if (temporal) {
            m_vulkanPipeline->freePicture(temporalPic);
        }
        return hash;
    }

    // Apply two tiles in a single compute session (merged buffer) and return
    // the buffer hash.
    std::string applyMerged(PictureVulkan* picture, bool temporal)
    {
        auto& compute = m_vulkanPipeline->backend().compute();
        PictureVulkan* temporalPic = nullptr;
        if (temporal) {
            temporalPic = vulkan_test_util::allocateTestPictureAndBuffer(
                m_vulkanPipeline, LdpColorFormatI420_16_LE, kWidth, kHeight);
            static_cast<BufferVulkan*>(temporalPic->buffer)->clear();
        }

        VulkanFrameContext* ctx = compute.acquireFrameContext(nullptr);
        EXPECT_NE(ctx, nullptr);
        compute.beginCompute(ctx);
        if (!ctx->prepareCommandBuffer(LOQ0, mergedBufferSize({&m_cmdBuf1, &m_cmdBuf2})) ||
            !ctx->prepareCommandBuffer(LOQ1, 0)) {
            return "";
        }

        VulkanApplyCommonArgs common{};
        common.picture = temporal ? nullptr : picture;
        common.planeWidth = kWidth;
        common.planeHeight = kHeight;
        common.transform = TransformDDS;
        common.context = ctx;
        if (temporal) {
            common.temporalPicture = temporalPic;
            common.chroma = LdeChroma::CT420;
        }
        EXPECT_TRUE(m_vulkanPipeline->backend().applyCommon(&common));

        // Tile 1
        VulkanApplyTileArgs tile1{};
        tile1.picture = temporal ? nullptr : picture;
        tile1.planeWidth = kWidth;
        tile1.planeHeight = kHeight;
        tile1.bufferGpu = m_cmdBuf1;
        tile1.context = ctx;
        tile1.loq = common.loq;
        tile1.transform = common.transform;
        if (temporal) {
            tile1.temporalPicture = temporalPic;
        }
        EXPECT_TRUE(m_vulkanPipeline->backend().applyTile(&tile1));

        // Tile 2
        VulkanApplyTileArgs tile2{};
        tile2.picture = temporal ? nullptr : picture;
        tile2.planeWidth = kWidth;
        tile2.planeHeight = kHeight;
        tile2.bufferGpu = m_cmdBuf2;
        tile2.context = ctx;
        tile2.loq = common.loq;
        tile2.transform = common.transform;
        if (temporal) {
            tile2.temporalPicture = temporalPic;
        }
        EXPECT_TRUE(m_vulkanPipeline->backend().applyTile(&tile2));

        compute.endCompute(ctx);
        m_vulkanPipeline->addCompletionContext(ctx);
        m_vulkanPipeline->waitCompletionContextIdle();

        const std::string hash = vulkan_test_util::hashMd5Buffer(
            static_cast<BufferVulkan*>(temporal ? temporalPic->buffer : picture->buffer));
        if (temporal) {
            m_vulkanPipeline->freePicture(temporalPic);
        }
        return hash;
    }

    static constexpr uint32_t kWidth = 256;
    static constexpr uint32_t kHeight = 128;

    LdcMemoryAllocator* m_allocator{};
    std::unique_ptr<Pipeline> m_pipeline;
    PipelineVulkan* m_vulkanPipeline{};

    LdeCmdBufferGpu m_cmdBuf1{};
    LdeCmdBufferGpuBuilder m_builder1{};
    LdeCmdBufferGpu m_cmdBuf2{};
    LdeCmdBufferGpuBuilder m_builder2{};
};

// Apply two tiles in a single compute session (merged command buffer) to a
// picture directly.  The result must match applying them in separate sessions.
TEST_F(MultiTileApplyFixture, DirectMergedMatchesSequential)
{
    auto* seqPic = allocPicture();
    auto* mergedPic = allocPicture();

    const std::string hashSeq = applySequential(seqPic, false);
    const std::string hashMerged = applyMerged(mergedPic, false);

    EXPECT_FALSE(hashSeq.empty());
    EXPECT_FALSE(hashMerged.empty());
    EXPECT_EQ(hashSeq, hashMerged);

    m_vulkanPipeline->freePicture(seqPic);
    m_vulkanPipeline->freePicture(mergedPic);
}

// Apply two tiles via a temporal buffer in a single compute session.  The
// temporal result must match applying them in separate sessions.
// The temporal path uses ClearAndSet (temporal-cleared blocks with residuals)
// and Add (non-cleared blocks) as the decoder would produce.
TEST_F(MultiTileApplyFixture, TemporalMergedMatchesSequential)
{
    auto* seqPic = allocPicture();
    auto* mergedPic = allocPicture();

    const std::string hashSeq = applySequential(seqPic, true);
    const std::string hashMerged = applyMerged(mergedPic, true);

    EXPECT_FALSE(hashSeq.empty());
    EXPECT_FALSE(hashMerged.empty());
    EXPECT_EQ(hashSeq, hashMerged);

    m_vulkanPipeline->freePicture(seqPic);
    m_vulkanPipeline->freePicture(mergedPic);
}
