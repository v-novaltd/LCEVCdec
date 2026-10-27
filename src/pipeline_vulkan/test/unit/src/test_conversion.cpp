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
#include "picture_vulkan.h"
#include "pipeline_vulkan.h"
#include "test_utility.h"
//
#include <LCEVC/build_config.h>
#include <LCEVC/common/acceleration.h>
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
#include <algorithm>
#include <cstring>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

#if VN_OS(ANDROID)
#include <android/hardware_buffer.h>

#include <cmath>
#endif

// CPU scalar convert function - declared in pixel_processing but not in a public header.
extern "C"
{
typedef struct LdpPicturePlaneDesc LdpPicturePlaneDesc;

typedef struct LdppConvertArgs
{
    const LdpPicturePlaneDesc* src;
    const LdpPicturePlaneDesc* dst;
    uint32_t minWidth;
    uint32_t offset;
    uint32_t count;
} LdppConvertArgs;

typedef void (*PlaneConvertFunction)(const LdppConvertArgs* args);

PlaneConvertFunction planeConvertGetFunctionScalar(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                                   uint32_t planeIndex, bool isNV12);
}

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

// -----------------------------------------------------------------------------

namespace {

// Resolutions to test. For 420 chroma, both dimensions must be even.
// Chosen to exercise: workgroup-alignment boundaries, small sizes, non-power-of-2,
// and typical production resolutions.
struct Resolution
{
    uint32_t width;
    uint32_t height;

    std::string toString() const { return std::to_string(width) + "x" + std::to_string(height); }
};

constexpr Resolution kResolutions[] = {
    // Removed some resolutions for CI test runtime

    // {960, 540},   // Standard qHD
    // {1920, 1080}, // Full HD
    // {128, 128},   // Small power-of-2
    {66, 34},   // Small non-aligned, even
    {254, 130}, // Non-power-of-2, not workgroup-aligned
    // {2, 2},       // Minimum valid 420 size
};

// Map a bit depth to the corresponding unsigned and signed fixed-point types.
LdpFixedPoint unsignedFPFromBitDepth(uint8_t bitDepth)
{
    switch (bitDepth) {
        case 8: return LdpFPU8;
        case 10: return LdpFPU10;
        case 12: return LdpFPU12;
        case 14: return LdpFPU14;
        default: return LdpFPU8;
    }
}

LdpFixedPoint signedFPFromBitDepth(uint8_t bitDepth)
{
    switch (bitDepth) {
        case 8: return LdpFPS8;
        case 10: return LdpFPS10;
        case 12: return LdpFPS12;
        case 14: return LdpFPS14;
        default: return LdpFPS8;
    }
}

bool isNV12Format(LdpColorFormat format)
{
    return format == LdpColorFormatNV12_8 || format == LdpColorFormatNV21_8;
}

// Fill a buffer with random values appropriate for the given fixed-point type.
void fillBufferWithNoise(uint8_t* data, uint32_t sizeBytes, LdpFixedPoint fp, std::mt19937& rng)
{
    if (fp == LdpFPU8) {
        std::uniform_int_distribution<int> dist(0, 255);
        for (uint32_t i = 0; i < sizeBytes; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng));
        }
    } else if (fp == LdpFPU10) {
        std::uniform_int_distribution<int> dist(0, 1023);
        auto* data16 = reinterpret_cast<uint16_t*>(data);
        for (uint32_t i = 0; i < sizeBytes / 2; ++i) {
            data16[i] = static_cast<uint16_t>(dist(rng));
        }
    } else if (fp == LdpFPU12) {
        std::uniform_int_distribution<int> dist(0, 4095);
        auto* data16 = reinterpret_cast<uint16_t*>(data);
        for (uint32_t i = 0; i < sizeBytes / 2; ++i) {
            data16[i] = static_cast<uint16_t>(dist(rng));
        }
    } else if (fp == LdpFPU14) {
        std::uniform_int_distribution<int> dist(0, 16383);
        auto* data16 = reinterpret_cast<uint16_t*>(data);
        for (uint32_t i = 0; i < sizeBytes / 2; ++i) {
            data16[i] = static_cast<uint16_t>(dist(rng));
        }
    } else {
        // Signed types (S8.7, S10.5, S12.3, S14.1) - all stored as int16_t in range [-16384, 16383]
        std::uniform_int_distribution<int> dist(-16384, 16383);
        auto* data16 = reinterpret_cast<int16_t*>(data);
        for (uint32_t i = 0; i < sizeBytes / 2; ++i) {
            data16[i] = static_cast<int16_t>(dist(rng));
        }
    }
}

} // namespace

// -----------------------------------------------------------------------------

struct ConversionTestParams
{
    LdpColorFormat externalFormat; // The non-internal format (e.g. I420_8, NV12_8)
    bool toInternal;               // Direction of conversion
    uint8_t bitDepth;              // Bit depth of the external picture
    LdeChroma chroma;
    Resolution resolution;

    // For test naming
    std::string name() const
    {
        std::stringstream ss;
        ss << (toInternal ? "ToInternal" : "FromInternal") << "_" << static_cast<int>(bitDepth) << "bit";
        if (externalFormat == LdpColorFormatNV12_8) {
            ss << "_NV12";
        } else if (externalFormat == LdpColorFormatNV21_8) {
            ss << "_NV21";
        } else if (externalFormat == LdpColorFormatP010_10) {
            ss << "_P010";
        }
        switch (chroma) {
            case LdeChroma::CT420: ss << "_420"; break;
            case LdeChroma::CT422: ss << "_422"; break;
            case LdeChroma::CT444: ss << "_444"; break;
            case LdeChroma::CTMonochrome: ss << "_Mono"; break;
            default: break;
        }
        ss << "_" << resolution.toString();
        return ss.str();
    }
};

// Helper for printing a meaningful name for the test parameter
std::string ConversionTestName(const testing::TestParamInfo<ConversionTestParams>& info)
{
    return info.param.name();
}

// -----------------------------------------------------------------------------

class PipelineVulkanConversionTest : public testing::TestWithParam<ConversionTestParams>
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

        m_vulkanPipeline = static_cast<PipelineVulkan*>(m_pipeline.get());
        if (!m_vulkanPipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }

        // Ensure CPU scalar path is available
        ldcAccelerationInitialize(false);
    }

    void TearDown() override {}

protected:
    // Determine the internal color format that matches the chroma type.
    static LdpColorFormat internalFormatForChroma(LdeChroma chroma)
    {
        switch (chroma) {
            case LdeChroma::CTMonochrome: return LdpColorFormatGRAY_16_LE;
            case LdeChroma::CT420: return LdpColorFormatI420_16_LE;
            case LdeChroma::CT422: return LdpColorFormatI422_16_LE;
            case LdeChroma::CT444: return LdpColorFormatI444_16_LE;
            default: return LdpColorFormatI420_16_LE;
        }
    }

    // Run the GPU conversion and return the dst buffer contents.
    std::vector<uint8_t> runGPUConversion(const uint8_t* srcData, uint32_t srcDataSize,
                                          LdpColorFormat srcFormat, LdpColorFormat dstFormat,
                                          uint32_t width, uint32_t height, bool toInternal,
                                          uint8_t bitDepth, LdeChroma chroma, float sharpenStrength,
                                          int selectedPhysicalPlane) const
    {
        auto* src =
            vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, srcFormat, width, height);
        auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
        srcBuffer->copyIn(0, srcData, srcDataSize);

        auto* dst =
            vulkan_test_util::allocateTestPictureAndBuffer(m_vulkanPipeline, dstFormat, width, height);
        auto* dstBuffer = static_cast<BufferVulkan*>(dst->buffer);
        if (selectedPhysicalPlane >= 0) {
            const std::vector<uint8_t> untouched(dstBuffer->size(), 0xa5);
            dstBuffer->copyIn(0, untouched.data(), static_cast<uint32_t>(untouched.size()));
        }

        auto& compute = m_vulkanPipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
        EXPECT_NE(frameContext, nullptr);
        compute.beginCompute(frameContext);

        if (toInternal) {
            VulkanConvertInArgs args{};
            args.src = src;
            args.dst = dst;
            args.bitDepth = bitDepth;
            args.chroma = chroma;
            args.context = frameContext;
            EXPECT_TRUE(m_vulkanPipeline->backend().prepareConvertIn(&args));
            for (uint8_t plane = 0; plane < ldpPictureLayoutPlanes(&src->layout); ++plane) {
                if (selectedPhysicalPlane < 0 || plane == selectedPhysicalPlane) {
                    EXPECT_TRUE(m_vulkanPipeline->backend().convertIn(&args, plane));
                }
            }
            frameContext->insertComputeBarrier("barrier_after_convert_in");
        } else {
            VulkanConvertOutArgs args{};
            args.src = src;
            args.dst = dst;
            args.bitDepth = bitDepth;
            args.chroma = chroma;
            args.sharpenStrength = sharpenStrength;
            args.context = frameContext;
            EXPECT_TRUE(m_vulkanPipeline->backend().convertOut(&args));
        }

        compute.endCompute(frameContext);
        m_vulkanPipeline->addCompletionContext(frameContext);
        m_vulkanPipeline->waitCompletionContextIdle();

        // Read back result
        uint32_t dstSize = dstBuffer->size();
        std::vector<uint8_t> result(dstSize);
        dstBuffer->copyOut(result.data(), 0, dstSize);

        m_vulkanPipeline->freePicture(src);
        m_vulkanPipeline->freePicture(dst);

        return result;
    }

    // Run the CPU scalar conversion per-plane, writing into the provided dst buffer.
    // srcLayout and dstLayout must match the GPU picture layouts.
    void runCPUConversion(const uint8_t* srcData, uint8_t* dstData, const LdpPictureLayout& srcLayout,
                          const LdpPictureLayout& dstLayout, bool toInternal, uint8_t bitDepth) const
    {
        const LdpFixedPoint unsignedFP = unsignedFPFromBitDepth(bitDepth);
        const LdpFixedPoint signedFP = signedFPFromBitDepth(bitDepth);

        const LdpFixedPoint srcFP = toInternal ? unsignedFP : signedFP;
        const LdpFixedPoint dstFP = toInternal ? signedFP : unsignedFP;
        const LdpColorFormat srcFormat = ldpPictureLayoutFormat(&srcLayout);
        const LdpColorFormat dstFormat = ldpPictureLayoutFormat(&dstLayout);
        if (!toInternal && dstFormat == LdpColorFormatP010_10) {
            const auto toP010 = [](int16_t value) {
                const int output = std::clamp((static_cast<int>(value) + 0x4000 + 16) >> 5, 0, 1023);
                return static_cast<uint16_t>(output << 6);
            };
            for (uint32_t y = 0; y < srcLayout.height; ++y) {
                const auto* srcRow = reinterpret_cast<const int16_t*>(
                    srcData + srcLayout.planeOffsets[0] + y * srcLayout.rowStrides[0]);
                auto* dstRow = reinterpret_cast<uint16_t*>(dstData + dstLayout.planeOffsets[0] +
                                                           y * dstLayout.rowStrides[0]);
                for (uint32_t x = 0; x < srcLayout.width; ++x) {
                    dstRow[x] = toP010(srcRow[x]);
                }
            }
            const uint32_t chromaWidth = srcLayout.width >> 1;
            const uint32_t chromaHeight = srcLayout.height >> 1;
            for (uint32_t y = 0; y < chromaHeight; ++y) {
                const auto* srcURow = reinterpret_cast<const int16_t*>(
                    srcData + srcLayout.planeOffsets[1] + y * srcLayout.rowStrides[1]);
                const auto* srcVRow = reinterpret_cast<const int16_t*>(
                    srcData + srcLayout.planeOffsets[2] + y * srcLayout.rowStrides[2]);
                auto* dstUVRow = reinterpret_cast<uint16_t*>(dstData + dstLayout.planeOffsets[1] +
                                                             y * dstLayout.rowStrides[1]);
                for (uint32_t x = 0; x < chromaWidth; ++x) {
                    dstUVRow[2 * x] = toP010(srcURow[x]);
                    dstUVRow[2 * x + 1] = toP010(srcVRow[x]);
                }
            }
            return;
        }

        const bool srcIsNV12 = isNV12Format(srcFormat);
        const bool dstIsNV12 = isNV12Format(dstFormat);
        const bool nv12 = srcIsNV12 || dstIsNV12;

        // For NV12 conversions, we always have 3 color components (Y, U, V)
        // but the NV12 side has only 2 memory planes (Y + interleaved UV).
        // We iterate over the 3 color components.
        if (!nv12) {
            // Non-NV12: simple per-plane conversion
            const uint8_t numPlanes = ldpPictureLayoutPlanes(&srcLayout);
            for (uint8_t plane = 0; plane < numPlanes; ++plane) {
                const uint32_t planeWidth = ldpPictureLayoutPlaneWidth(&srcLayout, plane);
                const uint32_t planeHeight = ldpPictureLayoutPlaneHeight(&srcLayout, plane);

                PlaneConvertFunction func = planeConvertGetFunctionScalar(srcFP, dstFP, plane, false);
                if (!func) {
                    continue;
                }

                LdpPicturePlaneDesc srcPlaneDesc{};
                srcPlaneDesc.firstSample = const_cast<uint8_t*>(srcData) + srcLayout.planeOffsets[plane];
                srcPlaneDesc.rowByteStride = srcLayout.rowStrides[plane];

                LdpPicturePlaneDesc dstPlaneDesc{};
                dstPlaneDesc.firstSample = dstData + dstLayout.planeOffsets[plane];
                dstPlaneDesc.rowByteStride = dstLayout.rowStrides[plane];

                LdppConvertArgs convertArgs{};
                convertArgs.src = &srcPlaneDesc;
                convertArgs.dst = &dstPlaneDesc;
                convertArgs.minWidth = planeWidth;
                convertArgs.offset = 0;
                convertArgs.count = planeHeight;
                func(&convertArgs);
            }
        } else {
            // NV12 conversion: handle Y plane normally, then U and V components
            // which are interleaved on the NV12 side.
            //
            // The planar (I420) side has 3 planes: Y, U, V
            // The NV12 side has 2 planes: Y, UV (interleaved)
            const LdpPictureLayout& planarLayout = toInternal ? dstLayout : srcLayout;
            const LdpPictureLayout& nv12Layout = toInternal ? srcLayout : dstLayout;

            // Y plane (component 0) - identical to non-NV12
            {
                const uint32_t planeWidth = planarLayout.width;
                const uint32_t planeHeight = planarLayout.height;

                PlaneConvertFunction func = planeConvertGetFunctionScalar(srcFP, dstFP, 0, false);
                if (func) {
                    LdpPicturePlaneDesc srcPlaneDesc{};
                    srcPlaneDesc.firstSample = const_cast<uint8_t*>(srcData) + srcLayout.planeOffsets[0];
                    srcPlaneDesc.rowByteStride = srcLayout.rowStrides[0];

                    LdpPicturePlaneDesc dstPlaneDesc{};
                    dstPlaneDesc.firstSample = dstData + dstLayout.planeOffsets[0];
                    dstPlaneDesc.rowByteStride = dstLayout.rowStrides[0];

                    LdppConvertArgs convertArgs{};
                    convertArgs.src = &srcPlaneDesc;
                    convertArgs.dst = &dstPlaneDesc;
                    convertArgs.minWidth = planeWidth;
                    convertArgs.offset = 0;
                    convertArgs.count = planeHeight;
                    func(&convertArgs);
                }
            }

            // U and V components (planes 1 and 2 in I420, interleaved in NV12 plane 1)
            const uint32_t chromaWidth = planarLayout.width / 2;
            const uint32_t chromaHeight = planarLayout.height / 2;

            for (uint8_t comp = 0; comp < 2; ++comp) {
                // comp 0 = U, comp 1 = V
                const uint8_t planarPlane = comp + 1; // plane 1=U, plane 2=V in I420
                const uint8_t componentOffset =
                    (ldpPictureLayoutFormat(&nv12Layout) == LdpColorFormatNV21_8) ? 1 - comp : comp;

                // The NV12 scalar function (copyU8ToS16Nv12 / copyS16ToU8Nv12) handles
                // the x2 stride internally (srcPixel += 2 or dstPixel += 2).
                // We just need to point firstSample at the right component.
                PlaneConvertFunction func = planeConvertGetFunctionScalar(srcFP, dstFP, planarPlane, true);
                if (!func) {
                    continue;
                }

                LdpPicturePlaneDesc srcPlaneDesc{};
                LdpPicturePlaneDesc dstPlaneDesc{};

                if (toInternal) {
                    // NV12 → I420: src is NV12 (interleaved), dst is planar
                    srcPlaneDesc.firstSample =
                        const_cast<uint8_t*>(srcData) + nv12Layout.planeOffsets[1] + componentOffset;
                    srcPlaneDesc.rowByteStride = nv12Layout.rowStrides[1];

                    dstPlaneDesc.firstSample = dstData + planarLayout.planeOffsets[planarPlane];
                    dstPlaneDesc.rowByteStride = planarLayout.rowStrides[planarPlane];
                } else {
                    // I420 → NV12: src is planar, dst is NV12 (interleaved)
                    srcPlaneDesc.firstSample =
                        const_cast<uint8_t*>(srcData) + planarLayout.planeOffsets[planarPlane];
                    srcPlaneDesc.rowByteStride = planarLayout.rowStrides[planarPlane];

                    dstPlaneDesc.firstSample = dstData + nv12Layout.planeOffsets[1] + componentOffset;
                    dstPlaneDesc.rowByteStride = nv12Layout.rowStrides[1];
                }

                LdppConvertArgs convertArgs{};
                convertArgs.src = &srcPlaneDesc;
                convertArgs.dst = &dstPlaneDesc;
                convertArgs.minWidth = chromaWidth;
                convertArgs.offset = 0;
                convertArgs.count = chromaHeight;
                func(&convertArgs);
            }
        }
    }

    std::unique_ptr<Pipeline> m_pipeline;
    PipelineVulkan* m_vulkanPipeline{};
};

// -----------------------------------------------------------------------------

TEST_P(PipelineVulkanConversionTest, CompareGPUvsCPUScalar)
{
    const auto& params = GetParam();

    const LdpColorFormat internalFormat = internalFormatForChroma(params.chroma);
    const LdpColorFormat srcFormat = params.toInternal ? params.externalFormat : internalFormat;
    const LdpColorFormat dstFormat = params.toInternal ? internalFormat : params.externalFormat;
    const uint32_t width = params.resolution.width;
    const uint32_t height = params.resolution.height;

    // Create layouts matching GPU picture layouts to know strides and offsets.
    // The GPU uses allocateTestPictureAndBuffer which calls ldpDefaultPictureDesc + setDescAndBind.
    // We replicate the same layout here for CPU-side processing.
    LdpPictureLayout srcLayout{};
    LdpPictureLayout dstLayout{};
    ldpPictureLayoutInitialize(&srcLayout, srcFormat, width, height, kBufferRowAlignment);
    ldpPictureLayoutInitialize(&dstLayout, dstFormat, width, height, kBufferRowAlignment);

    const uint32_t srcSize = ldpPictureLayoutSize(&srcLayout);
    const uint32_t dstSize = ldpPictureLayoutSize(&dstLayout);

    // Generate random source data
    const LdpFixedPoint srcFP = params.toInternal ? unsignedFPFromBitDepth(params.bitDepth)
                                                  : signedFPFromBitDepth(params.bitDepth);
    std::mt19937 rng(123456);
    std::vector<uint8_t> srcData(srcSize);
    fillBufferWithNoise(srcData.data(), srcSize, srcFP, rng);

    // Run GPU conversion
    std::vector<uint8_t> gpuDst =
        runGPUConversion(srcData.data(), srcSize, srcFormat, dstFormat, width, height,
                         params.toInternal, params.bitDepth, params.chroma, 0.0f, -1);

    // Run CPU scalar conversion
    std::vector<uint8_t> cpuDst(dstSize, 0);
    runCPUConversion(srcData.data(), cpuDst.data(), srcLayout, dstLayout, params.toInternal, params.bitDepth);

    // Compare per-plane (only active region, ignoring stride padding)
    const uint8_t numPlanes = ldpPictureLayoutPlanes(&dstLayout);
    for (uint8_t plane = 0; plane < numPlanes; ++plane) {
        const uint32_t planeWidth = ldpPictureLayoutPlaneWidth(&dstLayout, plane);
        const uint32_t planeHeight = ldpPictureLayoutPlaneHeight(&dstLayout, plane);
        const uint32_t sampleSize = ldpPictureLayoutSampleSize(&dstLayout);
        const uint32_t interleave = ldpPictureLayoutPlaneInterleave(&dstLayout, plane);
        const uint32_t rowBytes = planeWidth * sampleSize * interleave;
        const uint32_t stride = dstLayout.rowStrides[plane];
        const uint32_t offset = dstLayout.planeOffsets[plane];

        for (uint32_t y = 0; y < planeHeight; ++y) {
            const uint8_t* gpuRow = gpuDst.data() + offset + y * stride;
            const uint8_t* cpuRow = cpuDst.data() + offset + y * stride;
            if (memcmp(gpuRow, cpuRow, rowBytes) != 0) {
                // Find first mismatch for diagnostics
                for (uint32_t x = 0; x < rowBytes; ++x) {
                    if (gpuRow[x] != cpuRow[x]) {
                        FAIL() << fmt::format("Mismatch at plane {}, row {}, byte {} ({}x{}): "
                                              "GPU=0x{:02x} CPU=0x{:02x}",
                                              plane, y, x, width, height, gpuRow[x], cpuRow[x]);
                    }
                }
            }
        }
    }
}

class PipelineVulkanPlaneConversionTest : public PipelineVulkanConversionTest
{};

TEST_P(PipelineVulkanPlaneConversionTest, ConvertsOnlySelectedPhysicalPlane)
{
    const auto& params = GetParam();
    const auto dstFormat = internalFormatForChroma(params.chroma);
    LdpPictureLayout srcLayout{};
    LdpPictureLayout dstLayout{};
    ldpPictureLayoutInitialize(&srcLayout, params.externalFormat, params.resolution.width,
                               params.resolution.height, kBufferRowAlignment);
    ldpPictureLayoutInitialize(&dstLayout, dstFormat, params.resolution.width,
                               params.resolution.height, kBufferRowAlignment);

    std::mt19937 rng(123456);
    std::vector<uint8_t> srcData(ldpPictureLayoutSize(&srcLayout));
    fillBufferWithNoise(srcData.data(), static_cast<uint32_t>(srcData.size()),
                        unsignedFPFromBitDepth(params.bitDepth), rng);
    std::vector<uint8_t> cpuDst(ldpPictureLayoutSize(&dstLayout));
    runCPUConversion(srcData.data(), cpuDst.data(), srcLayout, dstLayout, true, params.bitDepth);

    const bool semiplanar = params.externalFormat == LdpColorFormatNV12_8 ||
                            params.externalFormat == LdpColorFormatNV21_8;
    // Start each dispatch with a fresh sentinel-filled destination. In particular, dispatching
    // only NV12/NV21 physical plane 1 must write U and V while leaving Y untouched.
    for (uint8_t selected = 0; selected < ldpPictureLayoutPlanes(&srcLayout); ++selected) {
        SCOPED_TRACE(testing::Message() << "selected physical plane " << unsigned(selected));
        const auto gpuDst = runGPUConversion(srcData.data(), static_cast<uint32_t>(srcData.size()),
                                             params.externalFormat, dstFormat,
                                             params.resolution.width, params.resolution.height,
                                             true, params.bitDepth, params.chroma, 0.0f, selected);
        for (uint8_t plane = 0; plane < ldpPictureLayoutPlanes(&dstLayout); ++plane) {
            SCOPED_TRACE(testing::Message() << "destination plane " << unsigned(plane));
            const bool written = semiplanar ? (selected == 0 ? plane == 0 : plane != 0) : plane == selected;
            const uint32_t offset = dstLayout.planeOffsets[plane];
            if (!written) {
                const auto begin = gpuDst.begin() + offset;
                EXPECT_TRUE(std::all_of(begin, begin + ldpPictureLayoutPlaneSize(&dstLayout, plane),
                                        [](uint8_t value) { return value == 0xa5; }));
                continue;
            }
            const uint32_t rowBytes = ldpPictureLayoutRowSize(&dstLayout, plane);
            for (uint32_t y = 0; y < ldpPictureLayoutPlaneHeight(&dstLayout, plane); ++y) {
                const uint32_t rowOffset = offset + y * dstLayout.rowStrides[plane];
                ASSERT_EQ(std::memcmp(gpuDst.data() + rowOffset, cpuDst.data() + rowOffset, rowBytes), 0)
                    << "row " << y;
            }
        }
    }
}

class PipelineVulkanConversionSharpenTest : public PipelineVulkanConversionTest
{};

TEST_P(PipelineVulkanConversionSharpenTest, SpecializedPipelineMatchesReference)
{
    constexpr uint32_t kWidth = 8;
    constexpr uint32_t kHeight = 6;
    constexpr float kSharpenStrength = 0.5f;
    constexpr int64_t kSharpenStrengthU16 = 32767;
    const auto& params = GetParam();
    const LdpColorFormat internalFormat = internalFormatForChroma(LdeChroma::CT420);

    LdpPictureLayout srcLayout{};
    LdpPictureLayout dstLayout{};
    ldpPictureLayoutInitialize(&srcLayout, internalFormat, kWidth, kHeight, kBufferRowAlignment);
    ldpPictureLayoutInitialize(&dstLayout, params.externalFormat, kWidth, kHeight, kBufferRowAlignment);

    std::vector<uint8_t> srcData(ldpPictureLayoutSize(&srcLayout), 0);
    std::vector<uint8_t> yValues(kWidth * kHeight);
    std::vector<uint8_t> uValues((kWidth >> 1) * (kHeight >> 1));
    std::vector<uint8_t> vValues((kWidth >> 1) * (kHeight >> 1));
    const auto writeInternal = [&srcData, &srcLayout](uint8_t plane, uint32_t x, uint32_t y, uint8_t value) {
        auto* row =
            reinterpret_cast<int16_t*>(srcData.data() + ldpPictureLayoutPlaneOffset(&srcLayout, plane) +
                                       y * ldpPictureLayoutRowStride(&srcLayout, plane));
        row[x] = static_cast<int16_t>((static_cast<int>(value) << 7) - 0x4000);
    };

    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const uint8_t value = static_cast<uint8_t>(32 + ((x * 37 + y * 19 + x * y * 7) % 192));
            yValues[y * kWidth + x] = value;
            writeInternal(0, x, y, value);
        }
    }
    for (uint32_t y = 0; y < (kHeight >> 1); ++y) {
        for (uint32_t x = 0; x < (kWidth >> 1); ++x) {
            const uint32_t index = y * (kWidth >> 1) + x;
            uValues[index] = static_cast<uint8_t>(72 + x * 17 + y * 9);
            vValues[index] = static_cast<uint8_t>(184 - x * 13 - y * 11);
            writeInternal(1, x, y, uValues[index]);
            writeInternal(2, x, y, vValues[index]);
        }
    }

    const std::vector<uint8_t> gpuDst = runGPUConversion(
        srcData.data(), static_cast<uint32_t>(srcData.size()), internalFormat, params.externalFormat,
        kWidth, kHeight, false, params.bitDepth, LdeChroma::CT420, kSharpenStrength, -1);

    const int outputScale = 1 << (params.bitDepth - 8);
    const int outputMax = (1 << params.bitDepth) - 1;
    const auto convertedY = [&](uint32_t x, uint32_t y) {
        return static_cast<int>(yValues[y * kWidth + x]) * outputScale;
    };
    const auto expectedY = [&](uint32_t x, uint32_t y) {
        const int center = convertedY(x, y);
        if (x == 0 || x + 1 == kWidth || y == 0 || y + 1 == kHeight) {
            return center;
        }
        const int weight = 4 * center - convertedY(x - 1, y) - convertedY(x + 1, y) -
                           convertedY(x, y - 1) - convertedY(x, y + 1);
        const int coefficient = static_cast<int>((kSharpenStrengthU16 * weight + 32768) >> 16);
        return std::clamp(center + coefficient, 0, outputMax);
    };

    const bool output16Bit = params.bitDepth > 8;
    const bool p010 = params.externalFormat == LdpColorFormatP010_10;
    const uint32_t yStride = ldpPictureLayoutRowStride(&dstLayout, 0);
    const uint32_t yOffset = ldpPictureLayoutPlaneOffset(&dstLayout, 0);
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            if (output16Bit) {
                const auto* row = reinterpret_cast<const uint16_t*>(gpuDst.data() + yOffset + y * yStride);
                const uint16_t expected = static_cast<uint16_t>(expectedY(x, y) << (p010 ? 6 : 0));
                EXPECT_EQ(row[x], expected) << "luma mismatch at (" << x << ", " << y << ")";
            } else {
                const auto* row = gpuDst.data() + yOffset + y * yStride;
                EXPECT_EQ(row[x], expectedY(x, y)) << "luma mismatch at (" << x << ", " << y << ")";
            }
        }
    }

    if (params.externalFormat == LdpColorFormatI420_8 || params.externalFormat == LdpColorFormatI420_10_LE) {
        for (uint8_t plane = 1; plane < 3; ++plane) {
            const auto& values = (plane == 1) ? uValues : vValues;
            const uint32_t stride = ldpPictureLayoutRowStride(&dstLayout, plane);
            const uint32_t offset = ldpPictureLayoutPlaneOffset(&dstLayout, plane);
            for (uint32_t y = 0; y < (kHeight >> 1); ++y) {
                for (uint32_t x = 0; x < (kWidth >> 1); ++x) {
                    const int expected = values[y * (kWidth >> 1) + x] * outputScale;
                    if (output16Bit) {
                        const auto* row =
                            reinterpret_cast<const uint16_t*>(gpuDst.data() + offset + y * stride);
                        EXPECT_EQ(row[x], expected);
                    } else {
                        EXPECT_EQ(gpuDst[offset + y * stride + x], expected);
                    }
                }
            }
        }
    } else {
        const uint32_t stride = ldpPictureLayoutRowStride(&dstLayout, 1);
        const uint32_t offset = ldpPictureLayoutPlaneOffset(&dstLayout, 1);
        for (uint32_t y = 0; y < (kHeight >> 1); ++y) {
            for (uint32_t x = 0; x < (kWidth >> 1); ++x) {
                const uint32_t index = y * (kWidth >> 1) + x;
                const int expectedU = uValues[index] * outputScale;
                const int expectedV = vValues[index] * outputScale;
                if (p010) {
                    const auto* row =
                        reinterpret_cast<const uint16_t*>(gpuDst.data() + offset + y * stride);
                    EXPECT_EQ(row[2 * x], expectedU << 6);
                    EXPECT_EQ(row[2 * x + 1], expectedV << 6);
                } else {
                    const auto* row = gpuDst.data() + offset + y * stride;
                    EXPECT_EQ(row[2 * x], expectedU);
                    EXPECT_EQ(row[2 * x + 1], expectedV);
                }
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    BufferFormats, PipelineVulkanConversionSharpenTest,
    testing::Values(ConversionTestParams{LdpColorFormatI420_8, false, 8, LdeChroma::CT420, {8, 6}},
                    ConversionTestParams{LdpColorFormatI420_10_LE, false, 10, LdeChroma::CT420, {8, 6}},
                    ConversionTestParams{LdpColorFormatNV12_8, false, 8, LdeChroma::CT420, {8, 6}},
                    ConversionTestParams{LdpColorFormatP010_10, false, 10, LdeChroma::CT420, {8, 6}}),
    ConversionTestName);

#if VN_OS(ANDROID)
namespace {

int internalFrom8(uint8_t val) { return (static_cast<int>(val) << 7) - 0x4000; }

void writeInternalSample(std::vector<uint8_t>& data, const LdpPictureLayout& layout, uint8_t plane,
                         uint32_t x, uint32_t y, uint8_t val)
{
    auto* row = reinterpret_cast<int16_t*>(data.data() + ldpPictureLayoutPlaneOffset(&layout, plane) +
                                           y * ldpPictureLayoutRowStride(&layout, plane));
    row[x] = static_cast<int16_t>(internalFrom8(val));
}

uint8_t expectedRgba8(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

std::array<uint8_t, 4> expectedYuvToRgba(uint8_t y, uint8_t u, uint8_t v)
{
    const float yf = static_cast<float>(y) / 255.0f;
    const float uf = (static_cast<float>(u) / 255.0f) - 0.5f;
    const float vf = (static_cast<float>(v) / 255.0f) - 0.5f;

    return {expectedRgba8(yf + 1.403f * vf), expectedRgba8(yf - 0.344f * uf - 0.714f * vf),
            expectedRgba8(yf + 1.770f * uf), 255};
}

} // namespace

class PipelineVulkanConversionAhbTest : public testing::TestWithParam<LdeChroma>
{};

TEST_P(PipelineVulkanConversionAhbTest, ConvertOutRgba8Image)
{
    constexpr uint32_t kWidth = 8;
    constexpr uint32_t kHeight = 8;
    const LdeChroma chroma = GetParam();

    LdpColorFormat srcFormat{};
    uint32_t chromaWidthShift{};
    uint32_t chromaHeightShift{};
    switch (chroma) {
        case LdeChroma::CT420:
            srcFormat = LdpColorFormatI420_16_LE;
            chromaWidthShift = 1;
            chromaHeightShift = 1;
            break;
        case LdeChroma::CT422:
            srcFormat = LdpColorFormatI422_16_LE;
            chromaWidthShift = 1;
            break;
        case LdeChroma::CT444: srcFormat = LdpColorFormatI444_16_LE; break;
        default: FAIL() << "Unsupported chroma mode";
    }
    const uint32_t chromaWidth = kWidth >> chromaWidthShift;
    const uint32_t chromaHeight = kHeight >> chromaHeightShift;

    auto* pipelineBuilder = new PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
    ASSERT_TRUE(pipelineBuilder);

    EventSink* eventSink = nullptr;
    std::unique_ptr<Pipeline> pipeline = pipelineBuilder->finish(eventSink);
    delete pipelineBuilder;

    auto* vulkanPipeline = static_cast<PipelineVulkan*>(pipeline.get());
    if (!vulkanPipeline) {
        GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
    }

    auto* src = vulkan_test_util::allocateTestPictureAndBuffer(vulkanPipeline, srcFormat, kWidth, kHeight);
    auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
    const LdpPictureLayout& srcLayout = src->layout;
    std::vector<uint8_t> srcData(ldpPictureLayoutSize(&srcLayout), 0);

    std::vector<uint8_t> yValues(kWidth * kHeight);
    std::vector<uint8_t> uValues(chromaWidth * chromaHeight);
    std::vector<uint8_t> vValues(chromaWidth * chromaHeight);

    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const uint8_t value = static_cast<uint8_t>(32 + x * 17 + y * 9);
            yValues[y * kWidth + x] = value;
            writeInternalSample(srcData, srcLayout, 0, x, y, value);
        }
    }
    for (uint32_t y = 0; y < chromaHeight; ++y) {
        for (uint32_t x = 0; x < chromaWidth; ++x) {
            const uint8_t u = static_cast<uint8_t>(80 + x * 20 + y * 7);
            const uint8_t v = static_cast<uint8_t>(160 - x * 11 + y * 13);
            uValues[y * chromaWidth + x] = u;
            vValues[y * chromaWidth + x] = v;
            writeInternalSample(srcData, srcLayout, 1, x, y, u);
            writeInternalSample(srcData, srcLayout, 2, x, y, v);
        }
    }
    srcBuffer->copyIn(0, srcData.data(), static_cast<uint32_t>(srcData.size()));

    AHardwareBuffer_Desc ahbDesc{};
    ahbDesc.width = kWidth;
    ahbDesc.height = kHeight;
    ahbDesc.layers = 1;
    ahbDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    ahbDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                    AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;

    if (!AHardwareBuffer_isSupported(&ahbDesc)) {
        vulkanPipeline->freePicture(src);
        GTEST_SKIP() << "RGBA8 AHardwareBuffer is not supported on this device";
    }

    AHardwareBuffer* ahb = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_allocate(&ahbDesc, &ahb));
    ASSERT_NE(nullptr, ahb);

    LdpPictureDesc dstDesc{};
    ASSERT_EQ(LdcReturnCodeSuccess, ldpDefaultPictureDesc(&dstDesc, LdpColorFormatRGBA_8, kWidth, kHeight));
    LdpPicture* dstLdp = nullptr;
    ASSERT_EQ(LdcReturnCodeSuccess, vulkanPipeline->allocPicture(dstDesc, dstLdp));
    auto* dst = static_cast<PictureVulkan*>(dstLdp);
    ASSERT_NE(nullptr, dst);
    ASSERT_TRUE(dst->setAhbBuffer(*vulkanPipeline, ahb));

    auto& compute = vulkanPipeline->backend().compute();
    VulkanFrameContext* frameContext = compute.acquireFrameContext(nullptr);
    ASSERT_NE(frameContext, nullptr);
    ASSERT_TRUE(compute.beginCompute(frameContext));

    VulkanConvertOutArgs args{};
    args.src = src;
    args.dst = dst;
    args.bitDepth = 8;
    args.chroma = chroma;
    args.context = frameContext;
    EXPECT_TRUE(vulkanPipeline->backend().convertOut(&args));

    EXPECT_TRUE(compute.endCompute(frameContext));
    vulkanPipeline->addCompletionContext(frameContext);
    vulkanPipeline->waitCompletionContextIdle();

    AHardwareBuffer_Desc lockedDesc{};
    AHardwareBuffer_describe(ahb, &lockedDesc);

    void* lockedData = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_lock(ahb, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &lockedData));
    ASSERT_NE(nullptr, lockedData);

    const auto* pixels = static_cast<const uint8_t*>(lockedData);
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const uint32_t chromaX = x >> chromaWidthShift;
            const uint32_t chromaY = y >> chromaHeightShift;
            const auto expected =
                expectedYuvToRgba(yValues[y * kWidth + x], uValues[chromaY * chromaWidth + chromaX],
                                  vValues[chromaY * chromaWidth + chromaX]);
            const uint8_t* actual = pixels + (y * lockedDesc.stride + x) * 4;
            for (uint32_t c = 0; c < 4; ++c) {
                EXPECT_LE(std::abs(static_cast<int>(actual[c]) - static_cast<int>(expected[c])), 1)
                    << "Mismatch at (" << x << ", " << y << ") channel " << c
                    << ": actual=" << static_cast<int>(actual[c])
                    << " expected=" << static_cast<int>(expected[c]);
            }
        }
    }

    AHardwareBuffer_unlock(ahb, nullptr);
    vulkanPipeline->freePicture(src);
    vulkanPipeline->freePicture(dst);
    AHardwareBuffer_release(ahb);
}

INSTANTIATE_TEST_SUITE_P(ChromaFormats, PipelineVulkanConversionAhbTest,
                         testing::Values(LdeChroma::CT420, LdeChroma::CT422, LdeChroma::CT444),
                         [](const testing::TestParamInfo<LdeChroma>& info) {
                             switch (info.param) {
                                 case LdeChroma::CT420: return "420";
                                 case LdeChroma::CT422: return "422";
                                 case LdeChroma::CT444: return "444";
                                 default: return "Unknown";
                             }
                         });

#endif // VN_OS(ANDROID)

// -----------------------------------------------------------------------------

// Build parameterized test values: all format/direction combos × all resolutions.
namespace {

std::vector<ConversionTestParams> generateI420Params()
{
    struct FormatEntry
    {
        LdpColorFormat format;
        uint8_t bitDepth;
    };
    const FormatEntry formats[] = {
        {LdpColorFormatI420_8, 8},
        {LdpColorFormatI420_10_LE, 10},
        {LdpColorFormatI420_12_LE, 12},
        {LdpColorFormatI420_14_LE, 14},
    };

    std::vector<ConversionTestParams> params;
    for (const auto& res : kResolutions) {
        for (const auto& fmt : formats) {
            params.push_back({fmt.format, true, fmt.bitDepth, LdeChroma::CT420, res});
            params.push_back({fmt.format, false, fmt.bitDepth, LdeChroma::CT420, res});
        }
    }
    return params;
}

std::vector<ConversionTestParams> generateSemiplanarParams()
{
    std::vector<ConversionTestParams> params;
    for (const auto& res : kResolutions) {
        params.push_back({LdpColorFormatNV12_8, true, 8, LdeChroma::CT420, res});
        params.push_back({LdpColorFormatNV12_8, false, 8, LdeChroma::CT420, res});
        params.push_back({LdpColorFormatNV21_8, true, 8, LdeChroma::CT420, res});
        params.push_back({LdpColorFormatNV21_8, false, 8, LdeChroma::CT420, res});
        params.push_back({LdpColorFormatP010_10, false, 10, LdeChroma::CT420, res});
    }
    return params;
}

std::vector<ConversionTestParams> generatePlanarComplianceParams()
{
    struct FormatEntry
    {
        LdpColorFormat format;
        uint8_t bitDepth;
        LdeChroma chroma;
    };
    const FormatEntry formats[] = {
        {LdpColorFormatI422_8, 8, LdeChroma::CT422},
        {LdpColorFormatI422_10_LE, 10, LdeChroma::CT422},
        {LdpColorFormatI422_12_LE, 12, LdeChroma::CT422},
        {LdpColorFormatI422_14_LE, 14, LdeChroma::CT422},
        {LdpColorFormatI444_8, 8, LdeChroma::CT444},
        {LdpColorFormatI444_10_LE, 10, LdeChroma::CT444},
        {LdpColorFormatI444_12_LE, 12, LdeChroma::CT444},
        {LdpColorFormatI444_14_LE, 14, LdeChroma::CT444},
        {LdpColorFormatGRAY_8, 8, LdeChroma::CTMonochrome},
        {LdpColorFormatGRAY_10_LE, 10, LdeChroma::CTMonochrome},
        {LdpColorFormatGRAY_12_LE, 12, LdeChroma::CTMonochrome},
        {LdpColorFormatGRAY_14_LE, 14, LdeChroma::CTMonochrome},
    };

    std::vector<ConversionTestParams> params;
    for (const auto& res : kResolutions) {
        for (const auto& fmt : formats) {
            params.push_back({fmt.format, false, fmt.bitDepth, fmt.chroma, res});
        }
    }
    return params;
}

} // namespace

// I420 conversions at various bit depths and resolutions
INSTANTIATE_TEST_SUITE_P(I420Conversions, PipelineVulkanConversionTest,
                         testing::ValuesIn(generateI420Params()), ConversionTestName);

// NV12, NV21, and P010 conversions at various resolutions
INSTANTIATE_TEST_SUITE_P(SemiplanarConversions, PipelineVulkanConversionTest,
                         testing::ValuesIn(generateSemiplanarParams()), ConversionTestName);

// Planar 4:2:2, 4:4:4, and monochrome buffer output paths.
INSTANTIATE_TEST_SUITE_P(PlanarComplianceConversions, PipelineVulkanConversionTest,
                         testing::ValuesIn(generatePlanarComplianceParams()), ConversionTestName);

// Exercise independent input dispatches for planar bit depths and both UV orderings.
INSTANTIATE_TEST_SUITE_P(InputPlanes, PipelineVulkanPlaneConversionTest, testing::ValuesIn([] {
                             auto params = generateI420Params();
                             const auto semiplanar = generateSemiplanarParams();
                             params.insert(params.end(), semiplanar.begin(), semiplanar.end());
                             params.erase(std::remove_if(params.begin(), params.end(),
                                                         [](const auto& p) { return !p.toInternal; }),
                                          params.end());
                             return params;
                         }()),
                         ConversionTestName);
