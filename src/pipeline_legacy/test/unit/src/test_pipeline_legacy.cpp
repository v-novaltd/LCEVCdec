/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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
#include <LCEVC/common/cpp_tools.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_legacy/create_pipeline.h>
//
#include <gtest/gtest.h>
//
#include <memory>

using namespace lcevc_dec::pipeline;

TEST(PipelineLegacy, Create)
{
    ldcDiagnosticsInitialize(NULL);

    auto pipelineBuilder =
        CREATE_PIPELINE_LEGACY_BUILDER_NAME(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
    ASSERT_TRUE(pipelineBuilder);

    auto pipeline = pipelineBuilder->finish(EventSink::nullSink());
    ASSERT_TRUE(pipeline);
}

class PipelineLegacyFixture : public testing::Test
{
public:
    PipelineLegacyFixture() noexcept {}

    std::unique_ptr<Pipeline> mPipeline;

    void SetUp() override
    {
        auto pipelineBuilder =
            CREATE_PIPELINE_LEGACY_BUILDER_NAME(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
        ASSERT_TRUE(pipelineBuilder);

        mPipeline = pipelineBuilder->finish(EventSink::nullSink());
        ASSERT_TRUE(mPipeline);
    }

    void TearDown() override { mPipeline.reset(); }
};

TEST_F(PipelineLegacyFixture, AllocatePicturesManaged)
{
    const LdpPictureDesc pictureDesc{1920, 1080, LdpColorFormatI420_8};

    auto picture = mPipeline->allocPictureManaged(pictureDesc);
    ASSERT_TRUE(picture);
}
