/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include "test_plane.h"

#include <find_assets_dir.h>
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pixel_processing/upscale.h>
extern "C"
{
#include "upscale_common.h"

#include <log_utilities.h> // Pulled from enhancement/src
}

#include <gtest/gtest.h>
#include <range/v3/view.hpp>
#include <range/v3/view/cartesian_product.hpp>

// -----------------------------------------------------------------------------

using namespace lcevc_dec::utility;
namespace rg = ranges;
namespace rv = ranges::views;

static constexpr uint32_t kWidth = 180;
static constexpr uint32_t kHeight = 100;

static constexpr bool kPAOn = true;
static constexpr bool kPAOff = false;

static constexpr std::string_view kFile = "ElfuenteTunnel_180x100_16bit_400p_lf.yuv";

const static std::filesystem::path kTestAssets = findAssetsDir("src/pixel_processing/test/assets");

// -----------------------------------------------------------------------------

typedef struct UpscaleTestParams
{
    LdeScalingMode scalingMode;
    LdeUpscaleType upscaleType;
    bool predictedAverage;
    std::string hash;
    bool disableSIMD;
    uint32_t threads;
} UpscaleTestParams;

void PrintTo(const UpscaleTestParams& params, std::ostream* os)
{
    *os << "{scalingMode=" << scalingModeToString(params.scalingMode)
        << ", upscaleType=" << upscaleTypeToString(params.upscaleType)
        << ", predictedAverage=" << (params.predictedAverage ? "true" : "false") << ", hash=\""
        << params.hash << "\""
        << ", disableSIMD=" << (params.disableSIMD ? "true" : "false")
        << ", threads=" << params.threads << "}";
}

std::ostream& operator<<(std::ostream& os, const UpscaleTestParams& params)
{
    PrintTo(params, &os);
    return os;
}

std::string testNames(const testing::TestParamInfo<UpscaleTestParams>& value)
{
    const UpscaleTestParams params = value.param;

    std::string simd = params.disableSIMD ? "_simdOff_" : "_simdOn_";
    std::string pa = params.predictedAverage ? "_paOn" : "_paOff";
    std::stringstream ss;
    ss << upscaleTypeToString(params.upscaleType) << "_" << scalingModeToString(params.scalingMode)
       << pa << simd << params.threads << "_threads";
    return ss.str();
}

const std::vector<UpscaleTestParams> kTestHashes = {
    /* clang-format off */
    {Scale1D, USNearest, kPAOff, "b2278423995827beaebd882ca51a47d7"},
    {Scale1D, USLinear,  kPAOff, "f068e74d70395e38a1ebff55ebd01dda"},
    {Scale1D, USCubic,   kPAOff, "79ee78bd375c35e1c662f969636f150b"},
    {Scale1D, USNearest, kPAOn,  "b2278423995827beaebd882ca51a47d7"},
    {Scale1D, USLinear,  kPAOn,  "7414e3b895d2f7323d2866b826d90d4b"},
    {Scale1D, USCubic,   kPAOn,  "f3eb2b0554c93daea294575cce5f3ac9"},
    {Scale2D, USNearest, kPAOff, "4c637520d6910cdad62e3362c13b38b4"},
    {Scale2D, USLinear,  kPAOff, "252928da58e144de14a6d7b5423fb599"},
    {Scale2D, USCubic,   kPAOff, "aa45ede7398d359efa125a97e9354702"},
    {Scale2D, USNearest, kPAOn,  "4c637520d6910cdad62e3362c13b38b4"},
    {Scale2D, USLinear,  kPAOn,  "406c1d522cf58d58061ac0d492dabe7a"},
    {Scale2D, USCubic,   kPAOn,  "3588950bc8080b5caea99c690abfae71"},
    /* clang-format on */
};

const LdeKernel getUpscaleKernel(LdeUpscaleType type)
{
    if (type == USNearest) {
        return {{0, 16384, 0, 0}, 2, false};
    } else if (type == USLinear) {
        return {{0, 12288, 4096, 0}, 2, false};
    } else if (type == USCubic) {
        return {{1382, 14285, 3942, 461}, 4, false};
    }
    assert(false); // Add additional kernels here, they usually come from the globalConfig
    return {{0}, 0, false};
}

const std::vector<bool> kForceScalar = {true, false};
const std::vector<uint32_t> kThreads = {1, 4};

const auto kUpscaleTestParams = rv::cartesian_product(kTestHashes, kForceScalar, kThreads) |
                                rv::transform([](auto value) {
                                    UpscaleTestParams test = std::get<0>(value);
                                    test.disableSIMD = std::get<1>(value);
                                    test.threads = std::get<2>(value);
                                    return test;
                                }) |
                                rg::to_vector;

// -----------------------------------------------------------------------------

class UpscaleTest : public testing::TestWithParam<UpscaleTestParams>
{
protected:
    void SetUp() override
    {
        m_allocator = ldcMemoryAllocatorMalloc();
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        const UpscaleTestParams params = GetParam();

        ldcAccelerationInitialize(!params.disableSIMD);

        ldcTaskPoolInitialize(&m_taskPool, m_allocator, m_allocator, params.threads, params.threads);

        const auto dstWidth = kWidth * 2;
        const auto dstHeight = params.scalingMode == Scale1D ? kHeight : kHeight * 2;

        m_src.initialize(kWidth, kHeight, 256, LdpFPS8);
        m_dst.initialize(dstWidth, dstHeight, 512, LdpFPS8);
        ldpInternalPictureLayoutInitialize(&m_srcLayout, LdpColorFormatGRAY_8, kWidth, kHeight, 0);
        ldpInternalPictureLayoutInitialize(&m_dstLayout, LdpColorFormatGRAY_8, dstWidth, dstHeight, 0);

        readBinaryFile(m_src, m_srcFilePath);

        m_kernel = getUpscaleKernel(params.upscaleType);
        m_args.kernel = &m_kernel;
        m_args.applyPA = params.predictedAverage;
        m_args.frameDither = NULL;
        m_args.srcLayout = &m_srcLayout;
        m_args.dstLayout = &m_dstLayout;
        m_args.srcPlane = m_src.planeDesc;
        m_args.dstPlane = m_dst.planeDesc;
        m_args.mode = params.scalingMode;
    }

    void TearDown() override { ldcTaskPoolDestroy(&m_taskPool); }

    const std::string m_srcFilePath{(kTestAssets / kFile).string()};
    LdcMemoryAllocator* m_allocator = nullptr;
    LdcTaskPool m_taskPool = {0};
    TestPlane m_src = {};
    TestPlane m_dst = {};
    LdpPicturePlaneDesc m_interDesc = {0};
    LdpPictureLayout m_dstLayout = {0};
    LdpPictureLayout m_srcLayout = {0};
    LdppUpscaleArgs m_args = {0};
    LdeKernel m_kernel = {};
};

TEST_P(UpscaleTest, HashPlane)
{
    const UpscaleTestParams params = GetParam();

#if VN_SDK_FEATURE(TRACING)
    const LdpPipelineDiagInfo diagInfo = {"UpscaleTest", 0, 0, 0};
#endif

    ldppUpscale(&m_taskPool, NULL, &m_args,
#if VN_SDK_FEATURE(TRACING)
                &diagInfo
#else
                NULL
#endif
    );

    EXPECT_EQ(params.hash, hashActiveRegion(m_dst));
}

TEST(UpscaleTest, WideKernelUsesScalarWith64BitAccumulators)
{
    constexpr uint32_t width = 8;
    constexpr uint32_t height = 5;
    constexpr uint16_t wideKernel[4] = {49151, 65535, 65535, 65535};
    std::vector<int16_t> src(width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            src[(y * width) + x] = static_cast<int16_t>(((x * 7919) + (y * 12347)) - 16384);
        }
    }

    LdppUpscaleParams params = {};
    std::copy(std::begin(wideKernel), std::end(wideKernel), params.kernel);
    params.kernelIsWide = true;

    ldcAccelerationInitialize(true);
    const int32_t kernel[4] = {-49151, 65535, 65535, -65535};
    const auto shiftAndSaturate = [](int64_t value) {
        return static_cast<int16_t>(std::clamp<int64_t>((value + 0x2000) >> 14, -0x4000, 0x3fff));
    };
    const auto convolve = [&kernel, &shiftAndSaturate](const auto& sample, int32_t pos, bool second) {
        int64_t value = 0;
        if (!second) {
            value = ((int64_t)sample(pos - 2) * kernel[3]) + ((int64_t)sample(pos - 1) * kernel[2]) +
                    ((int64_t)sample(pos) * kernel[1]) + ((int64_t)sample(pos + 1) * kernel[0]);
        } else {
            value = ((int64_t)sample(pos - 1) * kernel[0]) + ((int64_t)sample(pos) * kernel[1]) +
                    ((int64_t)sample(pos + 1) * kernel[2]) + ((int64_t)sample(pos + 2) * kernel[3]);
        }
        return shiftAndSaturate(value);
    };

    for (const bool is2D : {false, true}) {
        const UpscaleFunction scalar =
            upscaleGetFunctionScalar(LdpFPS8, LdpFPS8, ILNone, is2D, false, false);
        const UpscaleFunction selected =
            getUpscaleFunction(LdpFPS8, LdpFPS8, ILNone, is2D, false, false, wideKernel);
        ASSERT_NE(scalar, nullptr);
        ASSERT_EQ(selected, scalar);

        const uint32_t dstHeight = is2D ? height * 2 : height;
        std::vector<int16_t> dst(width * 2 * dstHeight);
        selected(reinterpret_cast<uint8_t*>(src.data()), reinterpret_cast<uint8_t*>(dst.data()),
                 width * sizeof(int16_t), (width * 2) * sizeof(int16_t), width, height, 0, height,
                 params, nullptr);

        std::vector<int16_t> vertical(width * dstHeight);
        if (is2D) {
            for (int32_t y = 0; y < static_cast<int32_t>(height); ++y) {
                for (int32_t x = 0; x < static_cast<int32_t>(width); ++x) {
                    const auto sample = [&src, x](int32_t sampleY) {
                        const int32_t y = std::clamp(sampleY, 0, static_cast<int32_t>(height) - 1);
                        return src[(y * width) + x];
                    };
                    vertical[((y * 2) * width) + x] = convolve(sample, y, false);
                    vertical[(((y * 2) + 1) * width) + x] = convolve(sample, y, true);
                }
            }
        } else {
            vertical = src;
        }

        std::vector<int16_t> expected(width * 2 * dstHeight);
        for (int32_t y = 0; y < static_cast<int32_t>(dstHeight); ++y) {
            for (int32_t x = 0; x < static_cast<int32_t>(width); ++x) {
                const auto sample = [&vertical, y](int32_t sampleX) {
                    const int32_t x = std::clamp(sampleX, 0, static_cast<int32_t>(width) - 1);
                    return vertical[(y * width) + x];
                };
                expected[(y * width * 2) + (x * 2)] = convolve(sample, x, false);
                expected[(y * width * 2) + (x * 2) + 1] = convolve(sample, x, true);
            }
        }
        EXPECT_EQ(dst, expected) << "is2D: " << is2D;
    }
}

TEST(UpscaleTest, SIMDConvolutionSaturatesBeforeNarrowing)
{
    constexpr uint32_t width = 16;
    constexpr uint32_t height = 8;
    constexpr int16_t cubicKernel[4] = {-1382, 14285, 3942, -461};

    std::vector<int16_t> src(width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t patternIndex = x & 3;
            const bool positive = patternIndex == 1 || patternIndex == 2;
            src[(y * width) + x] = positive ? std::numeric_limits<int16_t>::max()
                                            : std::numeric_limits<int16_t>::min();
        }
    }

    LdppUpscaleParams params = {};
    std::copy(std::begin(cubicKernel), std::end(cubicKernel), params.kernel);

    for (const LdeScalingMode mode : {Scale1D, Scale2D}) {
        const bool is2D = mode == Scale2D;
        const uint32_t dstWidth = width * 2;
        const uint32_t dstHeight = is2D ? height * 2 : height;
        std::vector<int16_t> scalarDst(dstWidth * dstHeight);
        std::vector<int16_t> simdDst(dstWidth * dstHeight);

        ldcAccelerationInitialize(true);
        const UpscaleFunction scalarFunction =
            upscaleGetFunctionScalar(LdpFPS8, LdpFPS8, ILNone, is2D, false, false);
        UpscaleFunction simdFunction = nullptr;
        if (ldcAccelerationGet()->hasAVX2) {
            simdFunction = upscaleGetFunctionAVX(LdpFPS8, LdpFPS8, ILNone, is2D, false, false);
        } else if (ldcAccelerationGet()->hasNeon) {
            simdFunction = upscaleGetFunctionNEON(LdpFPS8, LdpFPS8, ILNone, is2D, false, false);
        } else if (ldcAccelerationGet()->hasSSE) {
            simdFunction = upscaleGetFunctionSSE(LdpFPS8, LdpFPS8, ILNone, is2D, false, false);
        }

        ASSERT_NE(scalarFunction, nullptr);
        if (!simdFunction) {
            GTEST_SKIP() << "No SIMD upscaler available";
        }

        scalarFunction(reinterpret_cast<uint8_t*>(src.data()),
                       reinterpret_cast<uint8_t*>(scalarDst.data()), width * sizeof(int16_t),
                       dstWidth * sizeof(int16_t), width, height, 0, height, params, nullptr);
        simdFunction(reinterpret_cast<uint8_t*>(src.data()),
                     reinterpret_cast<uint8_t*>(simdDst.data()), width * sizeof(int16_t),
                     dstWidth * sizeof(int16_t), width, height, 0, height, params, nullptr);

        EXPECT_EQ(scalarDst, simdDst) << "Scaling mode: " << scalingModeToString(mode);
        EXPECT_NE(std::find(scalarDst.begin(), scalarDst.end(), 0x3fff), scalarDst.end());
        EXPECT_NE(std::find(scalarDst.begin(), scalarDst.end(), -0x4000), scalarDst.end());
    }
}

INSTANTIATE_TEST_SUITE_P(UpscaleTests, UpscaleTest, testing::ValuesIn(kUpscaleTestParams), testNames);

TEST(UpscaleTest, SIMDMatchesScalarForUnsignedFormats)
{
    constexpr uint32_t width = 32;
    constexpr uint32_t height = 8;
    constexpr uint16_t kernel[4] = {static_cast<uint16_t>(-1382), 14285, 3942,
                                    static_cast<uint16_t>(-461)};

    ldcAccelerationInitialize(true);

    for (const LdpFixedPoint fp : {LdpFPU8, LdpFPU10, LdpFPU12, LdpFPU14}) {
        const bool isU8 = fp == LdpFPU8;
        const uint32_t bytesPerSample = isU8 ? 1 : 2;
        const uint16_t maximum = fp == LdpFPU10 ? 1023 : fp == LdpFPU12 ? 4095 : 16383;

        for (const bool is2D : {false, true}) {
            for (const bool applyPA : {false, true}) {
                const UpscaleFunction scalar =
                    upscaleGetFunctionScalar(fp, fp, ILNone, is2D, applyPA, false);
                UpscaleFunction simd = nullptr;
                if (ldcAccelerationGet()->hasAVX2) {
                    simd = upscaleGetFunctionAVX(fp, fp, ILNone, is2D, applyPA, false);
                }
                if (!simd) {
                    simd = upscaleGetFunctionSSE(fp, fp, ILNone, is2D, applyPA, false);
                }
                if (!simd) {
                    simd = upscaleGetFunctionNEON(fp, fp, ILNone, is2D, applyPA, false);
                }
                ASSERT_NE(scalar, nullptr);
                if (!simd) {
                    GTEST_SKIP() << "No SIMD upscaler available";
                }

                const uint32_t dstWidth = width * 2;
                const uint32_t dstHeight = is2D ? height * 2 : height;
                std::vector<uint8_t> src(width * height * bytesPerSample);
                std::vector<uint8_t> scalarDst(dstWidth * dstHeight * bytesPerSample);
                std::vector<uint8_t> simdDst(dstWidth * dstHeight * bytesPerSample);

                if (isU8) {
                    for (uint32_t y = 0; y < height; ++y) {
                        for (uint32_t x = 0; x < width; ++x) {
                            src[(y * width) + x] = static_cast<uint8_t>(32 + ((x * 17 + y * 11) % 192));
                        }
                    }
                } else {
                    uint16_t* samples = reinterpret_cast<uint16_t*>(src.data());
                    for (uint32_t y = 0; y < height; ++y) {
                        for (uint32_t x = 0; x < width; ++x) {
                            samples[(y * width) + x] = static_cast<uint16_t>((x * 37 + y * 23) & maximum);
                        }
                    }
                }

                LdppUpscaleParams params = {};
                std::copy(std::begin(kernel), std::end(kernel), params.kernel);
                params.applyPA = applyPA;
                params.shift = fp == LdpFPU8 ? 7 : fp == LdpFPU10 ? 5 : fp == LdpFPU12 ? 3 : 1;
                params.offset = fp == LdpFPU8 ? 64 : fp == LdpFPU10 ? 16 : fp == LdpFPU12 ? 4 : 1;
                params.midpoint = fp == LdpFPU8    ? 128
                                  : fp == LdpFPU10 ? 512
                                  : fp == LdpFPU12 ? 2048
                                                   : 8192;

                scalar(src.data(), scalarDst.data(), width * bytesPerSample,
                       dstWidth * bytesPerSample, width, height, 0, height, params, nullptr);
                simd(src.data(), simdDst.data(), width * bytesPerSample, dstWidth * bytesPerSample,
                     width, height, 0, height, params, nullptr);

                EXPECT_EQ(scalarDst, simdDst)
                    << "fp=" << static_cast<int>(fp) << " is2D=" << is2D << " applyPA=" << applyPA;
            }
        }
    }
}

TEST(UpscaleTest, SIMDMatchesScalarForNV12)
{
    ldcAccelerationInitialize(true);

    constexpr uint32_t width = 32;
    constexpr uint32_t height = 8;
    constexpr uint16_t kernel[4] = {static_cast<uint16_t>(-1382), 14285, 3942,
                                    static_cast<uint16_t>(-461)};

    std::vector<uint8_t> src(width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; x += 2) {
            src[(y * width) + x] = static_cast<uint8_t>(32 + ((x * 13 + y * 7) % 192));
            src[(y * width) + x + 1] = static_cast<uint8_t>(48 + ((x * 5 + y * 19) % 160));
        }
    }

    for (const bool is2D : {false, true}) {
        for (const bool applyPA : {false, true}) {
            const UpscaleFunction scalar =
                upscaleGetFunctionScalar(LdpFPU8, LdpFPU8, ILNV12, is2D, applyPA, false);
            UpscaleFunction simd = nullptr;
            if (ldcAccelerationGet()->hasAVX2) {
                simd = upscaleGetFunctionAVX(LdpFPU8, LdpFPU8, ILNV12, is2D, applyPA, false);
            }
            if (!simd) {
                simd = upscaleGetFunctionSSE(LdpFPU8, LdpFPU8, ILNV12, is2D, applyPA, false);
            }
            if (!simd) {
                simd = upscaleGetFunctionNEON(LdpFPU8, LdpFPU8, ILNV12, is2D, applyPA, false);
            }
            ASSERT_NE(scalar, nullptr);
            if (!simd) {
                GTEST_SKIP() << "No SIMD upscaler available";
            }

            const uint32_t dstHeight = is2D ? height * 2 : height;
            std::vector<uint8_t> scalarDst(width * 2 * dstHeight);
            std::vector<uint8_t> simdDst(width * 2 * dstHeight);
            LdppUpscaleParams params = {};
            std::copy(std::begin(kernel), std::end(kernel), params.kernel);
            params.applyPA = applyPA;
            params.shift = 7;
            params.offset = 64;
            params.midpoint = 128;

            scalar(src.data(), scalarDst.data(), width, width * 2, width, height, 0, height, params, nullptr);
            simd(src.data(), simdDst.data(), width, width * 2, width, height, 0, height, params, nullptr);

            EXPECT_EQ(scalarDst, simdDst) << "is2D=" << is2D << " applyPA=" << applyPA;
        }
    }
}
