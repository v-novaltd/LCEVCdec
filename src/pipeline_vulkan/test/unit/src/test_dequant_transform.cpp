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

#include "buffer_vulkan_vma.h"
#include "pipeline_builder_vulkan.h"
#include "pipeline_vulkan.h"
#include "test_transform.h"

#include <gtest/gtest.h>
#include <LCEVC/common/acceleration.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/pipeline.h>

extern "C"
{
#include "decode_utilities.h"
#include "dequant.h"
#include "transform.h"
}

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

struct DequantTransformTestParams : TransformTestParams
{
    TemporalSignal temporal;
    bool deblockEnabled;
};

using Residuals = std::array<int16_t, RCLayerCountDDS>;

constexpr uint32_t kDeblockCorner = 11;
constexpr uint32_t kDeblockSide = 7;

int16_t deblockResidualCpu(int16_t residual, uint32_t coefficient)
{
    return static_cast<int16_t>((coefficient * static_cast<uint32_t>(residual)) >> 4);
}

struct GpuDequantParameters
{
    int16_t stepWidth[TSCount][RCLayerCountDDS];
    int16_t offset[TSCount][RCLayerCountDDS];
};
static_assert(sizeof(GpuDequantParameters) == 128); // Matching shader consumers.

const char* temporalSignalName(TemporalSignal temporal)
{
    return temporal == TSInter ? "Inter" : "Intra";
}

Dequant makeDequant(const DequantTransformTestParams& params)
{
    LdeGlobalConfig globalConfig{};
    globalConfig.transform = params.transform;
    globalConfig.numLayers = static_cast<uint8_t>(transformTypeLayerCount(params.transform));
    globalConfig.scalingModes[LOQ0] = params.scaling;
    globalConfig.temporalEnabled = true;
    globalConfig.temporalStepWidthModifier = kDefaultTemporalStepWidthModifier;

    LdeFrameConfig frameConfig{};
    frameConfig.stepWidths[LOQ0] = 1000;
    frameConfig.dequantOffsetMode = DQMConstOffset;
    frameConfig.dequantOffset = 64;
    quantMatrixSetDefault(&frameConfig.quantMatrix, params.scaling, params.transform, LOQ0);

    Dequant dequant{};
    EXPECT_TRUE(calculateDequant(&dequant, &globalConfig, &frameConfig, 0, LOQ0));
    return dequant;
}

void deblockResidualsCpu(Residuals& residuals)
{
    constexpr std::array<uint32_t, 4> kCornerIndices = {0, 5, 10, 15};
    constexpr std::array<uint32_t, 8> kSideIndices = {1, 4, 2, 7, 8, 13, 11, 14};
    for (const uint32_t index : kCornerIndices) {
        residuals[index] = deblockResidualCpu(residuals[index], kDeblockCorner);
    }
    for (const uint32_t index : kSideIndices) {
        residuals[index] = deblockResidualCpu(residuals[index], kDeblockSide);
    }
}

// This is the CPU reference operation that the Vulkan shader output will be compared against.
// Coefficients are copied because dequantization operates in place.
Residuals dequantAndInverseTransformCpu(const DequantTransformTestParams& params, const Dequant& dequant,
                                        const std::vector<int16_t>& quantizedCoefficients,
                                        bool enableAcceleration)
{
    ldcAccelerationInitialize(enableAcceleration);

    std::vector<int16_t> dequantizedCoefficients = quantizedCoefficients;
    dequantCoeffs(&dequant, params.temporal,
                  static_cast<uint8_t>(transformTypeLayerCount(params.transform)),
                  dequantizedCoefficients.data());

    const TransformFunction inverseTransform = transformGetFunction(params.transform, params.scaling);
    EXPECT_NE(inverseTransform, nullptr);

    Residuals residuals{};
    if (inverseTransform) {
        inverseTransform(dequantizedCoefficients.data(), residuals.data());
    }
    if (params.deblockEnabled) {
        deblockResidualsCpu(residuals);
    }
    return residuals;
}

std::vector<DequantTransformTestParams> makeTestParams()
{
    constexpr std::array<CoefficientValuesType, 7> kCoefficientValues = {
        CoefficientValuesType::Zeros,          CoefficientValuesType::Ones,
        CoefficientValuesType::OnesRandomSign, CoefficientValuesType::Incrementing,
        CoefficientValuesType::Overflow,       CoefficientValuesType::Underflow,
        CoefficientValuesType::Random};
    constexpr std::array<LdeTransformType, 2> kTransforms = {TransformDD, TransformDDS};
    constexpr std::array<LdeScalingMode, 2> kScalingModes = {Scale1D, Scale2D};
    constexpr std::array<TemporalSignal, 2> kTemporalSignals = {TSInter, TSIntra};

    std::vector<DequantTransformTestParams> params;
    for (const auto coefficientValues : kCoefficientValues) {
        for (const auto transform : kTransforms) {
            for (const auto scaling : kScalingModes) {
                for (const auto temporal : kTemporalSignals) {
                    params.push_back({{coefficientValues, transform, scaling}, temporal, false});
                    if (transform == TransformDDS) {
                        params.push_back({{coefficientValues, transform, scaling}, temporal, true});
                    }
                }
            }
        }
    }
    return params;
}

std::string dequantTransformTestName(const testing::TestParamInfo<DequantTransformTestParams>& value)
{
    std::stringstream name;
    name << coefficientValuesTypeToString(value.param.coeffsValues) << "_"
         << transformTypeToString(value.param.transform) << "_"
         << scalingModeToString(value.param.scaling) << "_" << temporalSignalName(value.param.temporal);
    if (value.param.deblockEnabled) {
        name << "_Deblock";
    }
    return name.str();
}

class VulkanDequantTransformTest : public testing::TestWithParam<DequantTransformTestParams>
{
protected:
    static void SetUpTestSuite()
    {
        auto pipelineBuilder = std::make_unique<PipelineBuilderVulkan>(ldcMemoryAllocatorMalloc());
        EventSink* eventSink = nullptr;
        sPipeline = pipelineBuilder->finish(eventSink);
        sVulkanPipeline = static_cast<PipelineVulkan*>(sPipeline.get());
    }

    static void TearDownTestSuite()
    {
        sVulkanPipeline = nullptr;
        sPipeline.reset();
    }

    void SetUp() override
    {
        if (sVulkanPipeline == nullptr) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }
    }

    bool runGpuShader(const DequantTransformTestParams& params, const Dequant& dequant,
                      const std::vector<int16_t>& quantizedCoefficients,
                      const Residuals& initialResiduals, Residuals& gpuResiduals)
    {
        constexpr uint32_t kTransformUnitCount = 3;
        const uint32_t layers = transformTypeLayerCount(params.transform);
        std::vector<int16_t> coefficientPayload(kTransformUnitCount * layers);
        std::vector<int16_t> residualPayload(kTransformUnitCount * layers);
        for (uint32_t transformUnit = 0; transformUnit < kTransformUnitCount; ++transformUnit) {
            std::copy_n(quantizedCoefficients.begin(), layers,
                        coefficientPayload.begin() + transformUnit * layers);
            std::copy_n(initialResiduals.begin(), layers, residualPayload.begin() + transformUnit * layers);
        }

        GpuDequantParameters gpuDequant{};
        for (uint32_t temporal = 0; temporal < TSCount; ++temporal) {
            for (uint32_t layer = 0; layer < RCLayerCountDDS; ++layer) {
                gpuDequant.stepWidth[temporal][layer] = dequant.stepWidth[temporal][layer];
                gpuDequant.offset[temporal][layer] = dequant.offset[temporal][layer];
            }
        }

        auto& backend = sVulkanPipeline->backend();
        const uint32_t payloadSize =
            static_cast<uint32_t>(coefficientPayload.size() * sizeof(coefficientPayload[0]));
        BufferVulkanVma coefficientBuffer(backend, payloadSize, BufferUsageIn, BufferVulkanBinding::Storage);
        BufferVulkanVma dequantBuffer(backend, sizeof(gpuDequant), BufferUsageIn,
                                      BufferVulkanBinding::Uniform);
        BufferVulkanVma residualBuffer(backend, payloadSize, BufferUsageOut, BufferVulkanBinding::Storage);
        coefficientBuffer.copyIn(0, coefficientPayload.data(), payloadSize);
        dequantBuffer.copyIn(0, &gpuDequant, sizeof(gpuDequant));
        residualBuffer.copyIn(0, residualPayload.data(), payloadSize);

        auto& compute = backend.compute();
        VulkanFrameContext* context = compute.acquireFrameContext(nullptr);
        if (context == nullptr) {
            return false;
        }
        if (!compute.beginCompute(context)) {
            compute.releaseFrameContext(context);
            return false;
        }

        VulkanDequantTransformArgs args{};
        args.coefficients = &coefficientBuffer;
        args.dequantParameters = &dequantBuffer;
        args.residuals = &residualBuffer;
        args.context = context;
        args.transformUnitCount = kTransformUnitCount;
        args.layerCount = layers;
        args.temporalSignal = static_cast<uint32_t>(params.temporal);
        args.scalingMode = params.scaling;
        args.deblockEnabled = params.deblockEnabled;
        args.deblockCorner = kDeblockCorner;
        args.deblockSide = kDeblockSide;

        if (!compute.dequantTransform(&args) || !compute.endCompute(context)) {
            context->abort();
            compute.releaseFrameContext(context);
            return false;
        }

        sVulkanPipeline->addCompletionContext(context);
        sVulkanPipeline->waitCompletionContextIdle();
        const uint32_t resultSize = layers * sizeof(gpuResiduals[0]);
        residualBuffer.copyOut(gpuResiduals.data(), (kTransformUnitCount - 1) * resultSize, resultSize);
        return true;
    }

private:
    inline static std::unique_ptr<Pipeline> sPipeline;
    inline static PipelineVulkan* sVulkanPipeline{};
};

TEST_P(VulkanDequantTransformTest, CompareCpuAndGpu)
{
    const auto& params = GetParam();
    const std::vector<int16_t> quantizedCoefficients =
        getCoefficientValues(params.coeffsValues, params.transform);
    const Dequant dequant = makeDequant(params);

    const Residuals scalarResiduals =
        dequantAndInverseTransformCpu(params, dequant, quantizedCoefficients, false);
    const Residuals acceleratedResiduals =
        dequantAndInverseTransformCpu(params, dequant, quantizedCoefficients, true);

    EXPECT_EQ(scalarResiduals, acceleratedResiduals);

    Residuals initialGpuResiduals{};
    initialGpuResiduals.fill(0x5A5A);
    Residuals gpuResiduals{};
    ASSERT_TRUE(runGpuShader(params, dequant, quantizedCoefficients, initialGpuResiduals, gpuResiduals));

    const uint32_t layers = transformTypeLayerCount(params.transform);
    for (uint32_t layer = 0; layer < layers; ++layer) {
        EXPECT_EQ(scalarResiduals[layer], gpuResiduals[layer]) << "layer " << layer;
    }
}

INSTANTIATE_TEST_SUITE_P(DequantTransformTests, VulkanDequantTransformTest,
                         testing::ValuesIn(makeTestParams()), dequantTransformTestName);

} // namespace
