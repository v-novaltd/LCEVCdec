/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

// Cross-validation for merging the temporal-buffer add into output conversion.
//
//   reference: add(temporal -> prediction) ; convertOut(prediction)
//   merged:    convertOut(prediction, temporal, addTemporal=true)

#include "buffer_vulkan.h"
#include "picture_vulkan.h"
#include "pipeline_vulkan.h"
#include "test_utility.h"
//
#include <LCEVC/pipeline/buffer_alignment.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
//
#include <fmt/core.h>
#include <gtest/gtest.h>
//
#include <cstring>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

namespace {

struct Resolution
{
    uint32_t width;
    uint32_t height;
    std::string toString() const { return std::to_string(width) + "x" + std::to_string(height); }
};

constexpr Resolution kResolutions[] = {
    {66, 34},   // small, non workgroup-aligned, even
    {254, 130}, // non-power-of-2, not workgroup-aligned
    {128, 128}, // power-of-2
};

void fillInternalNoise(int16_t* data, uint32_t count, std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(-16384, 16383);
    for (uint32_t i = 0; i < count; ++i) {
        data[i] = static_cast<int16_t>(dist(rng));
    }
}

LdpColorFormat internalFormatForChroma(LdeChroma chroma)
{
    switch (chroma) {
        case LdeChroma::CT420: return LdpColorFormatI420_16_LE;
        case LdeChroma::CT422: return LdpColorFormatI422_16_LE;
        case LdeChroma::CT444: return LdpColorFormatI444_16_LE;
        default: return LdpColorFormatI420_16_LE;
    }
}

} // namespace

// -----------------------------------------------------------------------------

struct ConversionWithAddParams
{
    LdpColorFormat outputFormat;
    uint8_t bitDepth;
    LdeChroma chroma;
    uint8_t numEnhancedPlanes;
    Resolution resolution;

    std::string name() const
    {
        std::stringstream ss;
        ss << static_cast<int>(bitDepth) << "bit";
        if (outputFormat == LdpColorFormatNV12_8) {
            ss << "_NV12";
        }
        ss << "_" << static_cast<int>(numEnhancedPlanes) << "planes_" << resolution.toString();
        return ss.str();
    }
};

std::string ConversionWithAddTestName(const testing::TestParamInfo<ConversionWithAddParams>& info)
{
    return info.param.name();
}

// -----------------------------------------------------------------------------

class PipelineVulkanConversionWithAddTest : public testing::TestWithParam<ConversionWithAddParams>
{
public:
    void SetUp() override
    {
        auto* pipelineBuilder = new PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
        ASSERT_TRUE(pipelineBuilder);

        EventSink* eventSink = nullptr;
        m_pipeline = pipelineBuilder->finish(eventSink);
        delete pipelineBuilder;

        m_vulkanPipeline = static_cast<PipelineVulkan*>(m_pipeline.get());
        if (!m_vulkanPipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }
    }

protected:
    // Run convertOut for one picture; optionally merging in the temporal add.
    std::vector<uint8_t> runConvertOut(PictureVulkan* src, PictureVulkan* temporal,
                                       bool addTemporal, uint8_t numEnhancedPlanes,
                                       LdpColorFormat outputFormat, uint32_t width, uint32_t height,
                                       uint8_t bitDepth, LdeChroma chroma) const
    {
        auto* dst = vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, outputFormat,
                                                                   width, height);
        auto* dstBuffer = static_cast<BufferVulkan*>(dst->buffer);

        auto& compute = m_vulkanPipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
        EXPECT_NE(frameContext, nullptr);
        compute.beginCompute(frameContext);

        VulkanConvertOutArgs args{};
        args.src = src;
        args.dst = dst;
        args.bitDepth = bitDepth;
        args.chroma = chroma;
        args.temporal = temporal;
        args.addTemporal = addTemporal;
        args.numEnhancedPlanes = numEnhancedPlanes;
        args.context = frameContext;
        EXPECT_TRUE(m_vulkanPipeline->backend().convertOut(&args));

        compute.endCompute(frameContext);
        m_vulkanPipeline->addCompletionContext(frameContext);
        m_vulkanPipeline->waitCompletionContextIdle();

        std::vector<uint8_t> result(dstBuffer->size());
        dstBuffer->copyOut(result.data(), 0, dstBuffer->size());

        m_vulkanPipeline->freePicture(dst);
        return result;
    }

    std::unique_ptr<Pipeline> m_pipeline;
    PipelineVulkan* m_vulkanPipeline{};
};

// -----------------------------------------------------------------------------

TEST_P(PipelineVulkanConversionWithAddTest, MergedMatchesAddThenConvert)
{
    const auto& params = GetParam();
    const uint32_t width = params.resolution.width;
    const uint32_t height = params.resolution.height;
    const LdpColorFormat internalFormat = internalFormatForChroma(params.chroma);

    LdpPictureLayout internalLayout{};
    ldpPictureLayoutInitialize(&internalLayout, internalFormat, width, height, kBufferRowAlignment);
    const uint32_t internalSize = ldpPictureLayoutSize(&internalLayout);
    const uint32_t sampleCount = internalSize / sizeof(int16_t);

    // Random prediction and temporal data in the S14 internal range.
    std::mt19937 rng(0xC0FFEE);
    std::vector<int16_t> predictionData(sampleCount);
    std::vector<int16_t> temporalData(sampleCount);
    fillInternalNoise(predictionData.data(), sampleCount, rng);
    fillInternalNoise(temporalData.data(), sampleCount, rng);

    // Shared temporal picture for both paths.
    auto* temporal =
        vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, internalFormat, width, height);
    static_cast<BufferVulkan*>(temporal->buffer)->copyIn(0, temporalData.data(), internalSize);

    // --- Reference path: add(temporal -> combined) then convertOut(combined) ---
    auto* combined =
        vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, internalFormat, width, height);
    static_cast<BufferVulkan*>(combined->buffer)->copyIn(0, predictionData.data(), internalSize);
    {
        auto& compute = m_vulkanPipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
        ASSERT_NE(frameContext, nullptr);
        compute.beginCompute(frameContext);

        VulkanAddArgs addArgs{};
        addArgs.src = temporal;
        addArgs.dst = combined;
        addArgs.numEnhancedPlanes = params.numEnhancedPlanes;
        addArgs.chroma = params.chroma;
        addArgs.context = frameContext;
        EXPECT_TRUE(m_vulkanPipeline->backend().add(&addArgs));

        compute.endCompute(frameContext);
        m_vulkanPipeline->addCompletionContext(frameContext);
        m_vulkanPipeline->waitCompletionContextIdle();
    }
    const std::vector<uint8_t> referenceOut =
        runConvertOut(combined, /*temporal=*/nullptr, /*addTemporal=*/false, params.numEnhancedPlanes,
                      params.outputFormat, width, height, params.bitDepth, params.chroma);

    // --- Merged path: convertOut(prediction, temporal, addTemporal=true) ---
    auto* prediction =
        vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, internalFormat, width, height);
    static_cast<BufferVulkan*>(prediction->buffer)->copyIn(0, predictionData.data(), internalSize);
    const std::vector<uint8_t> mergedOut =
        runConvertOut(prediction, temporal, /*addTemporal=*/true, params.numEnhancedPlanes,
                      params.outputFormat, width, height, params.bitDepth, params.chroma);

    // --- Compare per-plane active regions bit-exactly ---
    LdpPictureLayout outputLayout{};
    ldpPictureLayoutInitialize(&outputLayout, params.outputFormat, width, height, kBufferRowAlignment);
    ASSERT_EQ(referenceOut.size(), mergedOut.size());

    const uint8_t numPlanes = ldpPictureLayoutPlanes(&outputLayout);
    for (uint8_t plane = 0; plane < numPlanes; ++plane) {
        const uint32_t planeWidth = ldpPictureLayoutPlaneWidth(&outputLayout, plane);
        const uint32_t planeHeight = ldpPictureLayoutPlaneHeight(&outputLayout, plane);
        const uint32_t sampleSize = ldpPictureLayoutSampleSize(&outputLayout);
        const uint32_t interleave = ldpPictureLayoutPlaneInterleave(&outputLayout, plane);
        const uint32_t rowBytes = planeWidth * sampleSize * interleave;
        const uint32_t stride = outputLayout.rowStrides[plane];
        const uint32_t offset = outputLayout.planeOffsets[plane];

        for (uint32_t y = 0; y < planeHeight; ++y) {
            const uint8_t* refRow = referenceOut.data() + offset + y * stride;
            const uint8_t* mergeRow = mergedOut.data() + offset + y * stride;
            if (memcmp(refRow, mergeRow, rowBytes) != 0) {
                for (uint32_t x = 0; x < rowBytes; ++x) {
                    if (refRow[x] != mergeRow[x]) {
                        FAIL() << fmt::format("Mismatch at plane {}, row {}, byte {} ({}x{}, {}): "
                                              "add+convert=0x{:02x} merged=0x{:02x}",
                                              plane, y, x, width, height, params.name(), refRow[x],
                                              mergeRow[x]);
                    }
                }
            }
        }
    }

    m_vulkanPipeline->freePicture(temporal);
    m_vulkanPipeline->freePicture(combined);
    m_vulkanPipeline->freePicture(prediction);
}

// -----------------------------------------------------------------------------

namespace {

std::vector<ConversionWithAddParams> generateParams()
{
    struct FormatEntry
    {
        LdpColorFormat format;
        uint8_t bitDepth;
    };
    const FormatEntry formats[] = {
        {LdpColorFormatI420_8, 8},      {LdpColorFormatI420_10_LE, 10},
        {LdpColorFormatI420_12_LE, 12}, {LdpColorFormatI420_14_LE, 14},
        {LdpColorFormatNV12_8, 8},
    };

    std::vector<ConversionWithAddParams> params;
    for (const auto& res : kResolutions) {
        for (const auto& fmt : formats) {
            // Both luma-only and fully-enhanced (Y+U+V) temporal adds.
            params.push_back({fmt.format, fmt.bitDepth, LdeChroma::CT420, 1, res});
            params.push_back({fmt.format, fmt.bitDepth, LdeChroma::CT420, 3, res});
        }
    }
    return params;
}

} // namespace

INSTANTIATE_TEST_SUITE_P(ConversionWithAdd, PipelineVulkanConversionWithAddTest,
                         testing::ValuesIn(generateParams()), ConversionWithAddTestName);
