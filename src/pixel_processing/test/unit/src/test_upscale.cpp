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

#include "test_plane.h"

#include <find_assets_dir.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pixel_processing/upscale.h>
extern "C"
{
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
    bool forceScalar;
    uint32_t threads;
} UpscaleTestParams;

std::string testNames(const testing::TestParamInfo<UpscaleTestParams>& value)
{
    const UpscaleTestParams params = value.param;

    std::string simd = params.forceScalar ? "_simdOff_" : "_simdOn_";
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
        return {{{16384, 0}, {0, 16384}}, 2, false};
    } else if (type == USLinear) {
        return {{{12288, 4096}, {4096, 12288}}, 2, false};
    } else if (type == USCubic) {
        return {{{-1382, 14285, 3942, -461}, {-461, 3942, 14285, -1382}}, 4, false};
    }
    assert(false); // Add additional kernels here, they usually come from the globalConfig
    return {{{0}, {0}}, 0, false};
}

const std::vector<bool> kForceScalar = {true, false};
const std::vector<uint32_t> kThreads = {1, 4};

const auto kUpscaleTestParams = rv::cartesian_product(kTestHashes, kForceScalar, kThreads) |
                                rv::transform([](auto value) {
                                    UpscaleTestParams test = std::get<0>(value);
                                    test.forceScalar = std::get<1>(value);
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
        ldcTaskPoolInitialize(&m_taskPool, m_allocator, m_allocator, params.threads, params.threads);

        const auto dstWidth = kWidth * 2;
        const auto dstHeight = params.scalingMode == Scale1D ? kHeight : kHeight * 2;

        m_src.initialize(kWidth, kHeight, 256, LdpFPS8);
        m_dst.initialize(dstWidth, dstHeight, 512, LdpFPS8);
        ldpInternalPictureLayoutInitialize(&m_srcLayout, LdpColorFormatGRAY_8, kWidth, kHeight, 0);
        ldpInternalPictureLayoutInitialize(&m_dstLayout, LdpColorFormatGRAY_8, dstWidth, dstHeight, 0);

        readBinaryFile(m_src, m_srcFilePath);

        m_kernel = getUpscaleKernel(params.upscaleType);
        m_args.applyPA = params.predictedAverage;
        m_args.frameDither = NULL;
        m_args.dstLayout = &m_dstLayout;
        m_args.srcLayout = &m_srcLayout;
        m_args.srcPlane = m_src.planeDesc;
        m_args.dstPlane = m_dst.planeDesc;
        m_args.forceScalar = params.forceScalar;
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

    ldppUpscale(m_allocator, &m_taskPool, NULL, &m_kernel, &m_args);

    EXPECT_EQ(params.hash, hashActiveRegion(m_dst));
}

INSTANTIATE_TEST_SUITE_P(UpscaleTests, UpscaleTest, testing::ValuesIn(kUpscaleTestParams), testNames);
