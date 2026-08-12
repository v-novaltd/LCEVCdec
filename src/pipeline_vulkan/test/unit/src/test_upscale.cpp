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
#include <LCEVC/common/task_pool.h>
#include <LCEVC/pipeline/buffer_alignment.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
#include <LCEVC/pixel_processing/upscale.h>
#include <picture_vulkan.h>
#include <pipeline_vulkan.h>
//
#include <fmt/core.h>
#include <gtest/gtest.h>
//
#include <algorithm>
#include <cstring>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

// -----------------------------------------------------------------------------

namespace {

struct Resolution
{
    uint32_t width;
    uint32_t height;

    std::string toString() const { return std::to_string(width) + "x" + std::to_string(height); }
};

// Source resolutions (dst is 2x in the upscaled dimension(s)).
// Both dimensions must be even for 420 chroma.
constexpr Resolution kResolutions[] = {
    // Removed some resolutions for CI test runtime

    // {960, 540},   // Standard qHD
    // {480, 270}, // Quarter HD
    // {128, 128},   // Small power-of-2
    {66, 34},   // Small non-aligned, even
    {254, 130}, // Non-power-of-2, not workgroup-aligned
};

struct KernelEntry
{
    const char* name;
    LdeKernel kernel;
};

const KernelEntry kKernels[] = {
    {"Nearest", {{0, 16384, 0, 0}, 2, false}},
    {"Linear", {{0, 12288, 4096, 0}, 2, false}},
    {"Cubic", {{1382, 14285, 3942, 461}, 4, false}},
};

void fillInternalNoise(int16_t* data, uint32_t count, std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(-16384, 16383);
    for (uint32_t i = 0; i < count; ++i) {
        data[i] = static_cast<int16_t>(dist(rng));
    }
}

} // namespace

// -----------------------------------------------------------------------------

struct UpscaleTestParams
{
    Resolution srcResolution;
    LdeScalingMode scalingMode;
    const char* kernelName;
    LdeKernel kernel;
    bool predictedAverage;

    std::string name() const
    {
        std::stringstream ss;
        ss << srcResolution.toString();
        ss << "_" << (scalingMode == Scale1D ? "1D" : "2D");
        ss << "_" << kernelName;
        ss << (predictedAverage ? "_PA" : "_noPA");
        return ss.str();
    }
};

std::string UpscaleTestName(const testing::TestParamInfo<UpscaleTestParams>& info)
{
    return info.param.name();
}

// -----------------------------------------------------------------------------

class PipelineVulkanUpscaleTest : public testing::TestWithParam<UpscaleTestParams>
{
public:
    void SetUp() override
    {
        auto* pipelineBuilder =
            new lcevc_dec::pipeline_vulkan::PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
        ASSERT_TRUE(pipelineBuilder);

        EventSink* eventSink = nullptr;
        m_pipeline = pipelineBuilder->finish(eventSink);
        delete pipelineBuilder;

        // Use scalar CPU path as reference.
        ldcAccelerationInitialize(false);

        m_allocator = ldcMemoryAllocatorMalloc();
        ldcTaskPoolInitialize(&m_taskPool, m_allocator, m_allocator, 1, 1);

        m_vulkanPipeline = static_cast<PipelineVulkan*>(m_pipeline.get());
        if (!m_vulkanPipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }

        m_intermediateUpscalePicture0 = m_vulkanPipeline->allocatePicture();
        m_intermediateUpscalePicture1 = m_vulkanPipeline->allocatePicture();
    }

    void TearDown() override
    {
        ldcTaskPoolDestroy(&m_taskPool);
        if (m_vulkanPipeline) {
            m_vulkanPipeline->freePicture(m_intermediateUpscalePicture0);
            m_vulkanPipeline->freePicture(m_intermediateUpscalePicture1);
        }
    }

protected:
    // Run GPU upscale and return the dst buffer contents.
    std::vector<uint8_t> runGPUUpscale(const uint8_t* srcData, uint32_t srcDataSize,
                                       uint32_t srcWidth, uint32_t srcHeight, uint32_t dstWidth,
                                       uint32_t dstHeight, const LdeKernel& kernel,
                                       LdeScalingMode mode, bool predictedAverage) const
    {
        const LdpColorFormat internalFormat = LdpColorFormatI420_16_LE;

        auto* src = vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, internalFormat,
                                                                   srcWidth, srcHeight);
        auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
        srcBuffer->copyIn(0, srcData, srcDataSize);

        auto* dst = vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, internalFormat,
                                                                   dstWidth, dstHeight);

        VulkanUpscaleArgs upscaleArgs{};
        upscaleArgs.src = src;
        upscaleArgs.dst = dst;
        upscaleArgs.base = src; // base = src for PA
        upscaleArgs.applyPA = predictedAverage;
        upscaleArgs.dither = nullptr;
        upscaleArgs.mode = mode;
        upscaleArgs.vertical = false;
        upscaleArgs.loq1 = false;
        upscaleArgs.intermediateUpscalePicture[0] = m_intermediateUpscalePicture0;
        upscaleArgs.intermediateUpscalePicture[1] = m_intermediateUpscalePicture1;
        upscaleArgs.chroma = LdeChroma::CT420;
        upscaleArgs.pipeline = m_vulkanPipeline;

        LdeKernel kernelCopy = kernel;

        auto& compute = m_vulkanPipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
        EXPECT_NE(frameContext, nullptr);
        upscaleArgs.context = frameContext;
        compute.beginCompute(frameContext);
        EXPECT_TRUE(m_vulkanPipeline->backend().upscaleFrame(&kernelCopy, &upscaleArgs));
        compute.endCompute(frameContext);
        m_vulkanPipeline->addCompletionContext(frameContext);
        m_vulkanPipeline->waitCompletionContextIdle();

        auto* dstBuffer = static_cast<BufferVulkan*>(dst->buffer);
        uint32_t dstSize = dstBuffer->size();
        std::vector<uint8_t> result(dstSize);
        dstBuffer->copyOut(result.data(), 0, dstSize);

        m_vulkanPipeline->freePicture(src);
        m_vulkanPipeline->freePicture(dst);

        return result;
    }

    // Run CPU scalar upscale per-plane and assemble the result into a single buffer.
    void runCPUUpscale(const uint8_t* srcData, uint8_t* dstData, const LdpPictureLayout& srcPictureLayout,
                       const LdpPictureLayout& dstPictureLayout, const LdeKernel& kernel,
                       LdeScalingMode mode, bool predictedAverage)
    {
        const uint8_t numPlanes = ldpPictureLayoutPlanes(&srcPictureLayout);

        for (uint8_t plane = 0; plane < numPlanes; ++plane) {
            const uint32_t srcPlaneWidth = ldpPictureLayoutPlaneWidth(&srcPictureLayout, plane);
            const uint32_t srcPlaneHeight = ldpPictureLayoutPlaneHeight(&srcPictureLayout, plane);
            const uint32_t dstPlaneWidth = ldpPictureLayoutPlaneWidth(&dstPictureLayout, plane);
            const uint32_t dstPlaneHeight = ldpPictureLayoutPlaneHeight(&dstPictureLayout, plane);

            // Create single-plane (greyscale) internal layouts for the CPU upscale function.
            // The CPU upscale works per-plane, so we create GRAY_16_LE layouts that match
            // the plane dimensions and strides from the I420 picture layout.
            LdpPictureLayout srcPlaneLayout{};
            ldpInternalPictureLayoutInitialize(&srcPlaneLayout, LdpColorFormatGRAY_16_LE,
                                               srcPlaneWidth, srcPlaneHeight, 0);

            LdpPictureLayout dstPlaneLayout{};
            ldpInternalPictureLayoutInitialize(&dstPlaneLayout, LdpColorFormatGRAY_16_LE,
                                               dstPlaneWidth, dstPlaneHeight, 0);

            // Allocate temporary plane buffers (matching the GRAY_16_LE layout strides).
            const uint32_t srcPlaneSize = ldpPictureLayoutSize(&srcPlaneLayout);
            const uint32_t dstPlaneSize = ldpPictureLayoutSize(&dstPlaneLayout);
            std::vector<uint8_t> srcPlaneData(srcPlaneSize, 0);
            std::vector<uint8_t> dstPlaneData(dstPlaneSize, 0);

            // Copy source plane data from the I420 picture buffer into the temp plane buffer,
            // respecting different strides.
            const uint32_t i420SrcStride = srcPictureLayout.rowStrides[plane];
            const uint32_t graySrcStride = srcPlaneLayout.rowStrides[0];
            const uint32_t copyWidth = srcPlaneWidth * sizeof(int16_t);
            for (uint32_t y = 0; y < srcPlaneHeight; ++y) {
                const uint8_t* srcRow = srcData + srcPictureLayout.planeOffsets[plane] + y * i420SrcStride;
                uint8_t* dstRow = srcPlaneData.data() + y * graySrcStride;
                std::memcpy(dstRow, srcRow, copyWidth);
            }

            LdpPicturePlaneDesc srcPlaneDesc{};
            srcPlaneDesc.firstSample = srcPlaneData.data();
            srcPlaneDesc.rowByteStride = graySrcStride;

            LdpPicturePlaneDesc dstPlaneDesc{};
            dstPlaneDesc.firstSample = dstPlaneData.data();
            dstPlaneDesc.rowByteStride = dstPlaneLayout.rowStrides[0];

            LdeKernel kernelCopy = kernel;
            LdppUpscaleArgs args{};
            args.planeIndex = 0; // Always 0: we use per-plane GRAY layouts, not multi-plane I420
            args.srcLayout = &srcPlaneLayout;
            args.dstLayout = &dstPlaneLayout;
            args.srcPlane = srcPlaneDesc;
            args.dstPlane = dstPlaneDesc;
            args.kernel = &kernelCopy;
            args.applyPA = predictedAverage;
            args.frameDither = nullptr;
            args.mode = mode;

            const LdpPipelineDiagInfo diagInfo = {"UpscaleTest", 0, 0, 0};
            ldppUpscale(&m_taskPool, nullptr, &args, &diagInfo);

            // Copy result back into the I420-layout output buffer.
            const uint32_t i420DstStride = dstPictureLayout.rowStrides[plane];
            const uint32_t grayDstStride = dstPlaneLayout.rowStrides[0];
            const uint32_t dstCopyWidth = dstPlaneWidth * sizeof(int16_t);
            for (uint32_t y = 0; y < dstPlaneHeight; ++y) {
                const uint8_t* srcRow = dstPlaneData.data() + y * grayDstStride;
                uint8_t* dstRow = dstData + dstPictureLayout.planeOffsets[plane] + y * i420DstStride;
                std::memcpy(dstRow, srcRow, dstCopyWidth);
            }
        }
    }

    std::unique_ptr<Pipeline> m_pipeline;
    PipelineVulkan* m_vulkanPipeline{};
    PictureVulkan* m_intermediateUpscalePicture0{};
    PictureVulkan* m_intermediateUpscalePicture1{};
    LdcMemoryAllocator* m_allocator{};
    LdcTaskPool m_taskPool{};
};

// -----------------------------------------------------------------------------

TEST_P(PipelineVulkanUpscaleTest, CompareGPUvsCPUScalar)
{
    const auto& params = GetParam();
    const uint32_t srcWidth = params.srcResolution.width;
    const uint32_t srcHeight = params.srcResolution.height;

    uint32_t dstWidth{};
    uint32_t dstHeight{};
    if (params.scalingMode == Scale1D) {
        // 1D upscale: GPU upscaleFrame does horizontal (width * 2) when vertical=false
        dstWidth = srcWidth * 2;
        dstHeight = srcHeight;
    } else {
        // 2D upscale: vertical then horizontal
        dstWidth = srcWidth * 2;
        dstHeight = srcHeight * 2;
    }

    const LdpColorFormat internalFormat = LdpColorFormatI420_16_LE;

    LdpPictureLayout srcLayout{};
    LdpPictureLayout dstLayout{};
    ldpPictureLayoutInitialize(&srcLayout, internalFormat, srcWidth, srcHeight, kBufferRowAlignment);
    ldpPictureLayoutInitialize(&dstLayout, internalFormat, dstWidth, dstHeight, kBufferRowAlignment);

    const uint32_t srcSize = ldpPictureLayoutSize(&srcLayout);
    const uint32_t dstSize = ldpPictureLayoutSize(&dstLayout);

    // Generate random source data in S14 internal range.
    std::mt19937 rng(123456);
    std::vector<uint8_t> srcData(srcSize);
    fillInternalNoise(reinterpret_cast<int16_t*>(srcData.data()), srcSize / sizeof(int16_t), rng);

    // Run GPU upscale.
    std::vector<uint8_t> gpuDst =
        runGPUUpscale(srcData.data(), srcSize, srcWidth, srcHeight, dstWidth, dstHeight,
                      params.kernel, params.scalingMode, params.predictedAverage);

    // Run CPU scalar upscale.
    std::vector<uint8_t> cpuDst(dstSize, 0);
    runCPUUpscale(srcData.data(), cpuDst.data(), srcLayout, dstLayout, params.kernel,
                  params.scalingMode, params.predictedAverage);

    // Compare per-plane active region.
    const uint8_t numPlanes = ldpPictureLayoutPlanes(&dstLayout);
    for (uint8_t plane = 0; plane < numPlanes; ++plane) {
        const uint32_t planeWidth = ldpPictureLayoutPlaneWidth(&dstLayout, plane);
        const uint32_t planeHeight = ldpPictureLayoutPlaneHeight(&dstLayout, plane);
        const uint32_t rowBytes = planeWidth * sizeof(int16_t);
        const uint32_t stride = dstLayout.rowStrides[plane];
        const uint32_t offset = dstLayout.planeOffsets[plane];

        for (uint32_t y = 0; y < planeHeight; ++y) {
            const uint8_t* gpuRow = gpuDst.data() + offset + y * stride;
            const uint8_t* cpuRow = cpuDst.data() + offset + y * stride;
            if (memcmp(gpuRow, cpuRow, rowBytes) != 0) {
                const auto* gpuSamples = reinterpret_cast<const int16_t*>(gpuRow);
                const auto* cpuSamples = reinterpret_cast<const int16_t*>(cpuRow);
                for (uint32_t x = 0; x < planeWidth; ++x) {
                    if (gpuSamples[x] != cpuSamples[x]) {
                        FAIL() << fmt::format("Mismatch at plane {}, row {}, sample {} ({}x{} -> "
                                              "{}x{}): GPU={} CPU={}",
                                              plane, y, x, srcWidth, srcHeight, dstWidth, dstHeight,
                                              gpuSamples[x], cpuSamples[x]);
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

namespace {

std::vector<UpscaleTestParams> generateUpscaleParams()
{
    std::vector<UpscaleTestParams> params;

    for (const auto& res : kResolutions) {
        for (const auto& kernelEntry : kKernels) {
            // 2D upscale without PA
            params.push_back({res, Scale2D, kernelEntry.name, kernelEntry.kernel, false});
            // 2D upscale with PA
            params.push_back({res, Scale2D, kernelEntry.name, kernelEntry.kernel, true});
            // 1D horizontal upscale without PA
            params.push_back({res, Scale1D, kernelEntry.name, kernelEntry.kernel, false});
            // 1D horizontal upscale with PA
            params.push_back({res, Scale1D, kernelEntry.name, kernelEntry.kernel, true});
        }
    }

    return params;
}

} // namespace

INSTANTIATE_TEST_SUITE_P(UpscaleTests, PipelineVulkanUpscaleTest,
                         testing::ValuesIn(generateUpscaleParams()), UpscaleTestName);
