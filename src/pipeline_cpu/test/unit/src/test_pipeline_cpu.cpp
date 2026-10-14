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

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline/frame_base.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/pipeline_base.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_cpu/create_pipeline.h>
//
#include <gtest/gtest.h>
//
#include <memory>

using namespace lcevc_dec::pipeline;

TEST(PipelineCPU, Create)
{
    auto pipelineBuilder =
        CREATE_PIPELINE_CPU_BUILDER_NAME(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
    ASSERT_TRUE(pipelineBuilder);

    auto pipeline = pipelineBuilder->finish(EventSink::nullSink());
    ASSERT_TRUE(pipeline);
}

class PipelineCPUFixture : public testing::Test
{
public:
    PipelineCPUFixture() noexcept {}

    std::unique_ptr<Pipeline> mPipeline;

    void SetUp() override
    {
        auto pipelineBuilder =
            CREATE_PIPELINE_CPU_BUILDER_NAME(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
        ASSERT_TRUE(pipelineBuilder);

        mPipeline = pipelineBuilder->finish(EventSink::nullSink());
        ASSERT_TRUE(mPipeline);
    }

    void TearDown() override { mPipeline.reset(); }
};

TEST_F(PipelineCPUFixture, AllocatePicturesManaged)
{
    const LdpPictureDesc pictureDesc{1920, 1080, LdpColorFormatI420_8};

    LdpPicture* picture = nullptr;
    ASSERT_EQ(LdcReturnCodeSuccess, mPipeline->allocPicture(pictureDesc, picture));
    ASSERT_TRUE(picture);
}

TEST_F(PipelineCPUFixture, LumaDirectUpscaleWithoutTemporal)
{
    auto* pipeline = static_cast<PipelineBase*>(mPipeline.get());
    FrameBase* frame = pipeline->allocateFrame(0);
    ASSERT_NE(frame, nullptr);
    LdeGlobalConfig global{};
    global.initialized = true;
    global.numPlanes = 3;
    global.baseDepth = Depth10;
    global.enhancedDepth = Depth10;
    global.scalingModes[LOQ0] = Scale2D;
    global.scalingModes[LOQ1] = Scale0D;
    frame->globalConfig = &global;
    frame->config.frameConfigSet = true;

    EXPECT_TRUE(pipeline->directUpscaleEligible(*frame, 0));

    global.temporalEnabled = true;
    frame->config.temporalRefresh = true;
    EXPECT_TRUE(pipeline->directUpscaleEligible(*frame, 0));

    frame->config.temporalRefresh = false;
    frame->config.entropyEnabled = true;
    frame->config.loqEnabled[LOQ0] = true;
    EXPECT_FALSE(pipeline->directUpscaleEligible(*frame, 0));
    pipeline->freeFrame(frame);
}
