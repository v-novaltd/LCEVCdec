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

#ifndef VN_LCEVC_PIPELINE_LEGACY_DECODER_BUILDER_H
#define VN_LCEVC_PIPELINE_LEGACY_DECODER_BUILDER_H

#include <LCEVC/pipeline/pipeline.h>
//
#include "decoder_config.h"
//
#include <string>
#include <vector>

// ------------------------------------------------------------------------------------------------

struct perseus_decoder_config;

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

class DecoderBuilder : public pipeline::PipelineBuilder
{
    template <typename T>
    using Vec = std::vector<T>;

public:
    bool configure(std::string_view name, bool val) override;
    bool configure(std::string_view name, int32_t val) override;
    bool configure(std::string_view name, float val) override;
    bool configure(std::string_view name, const std::string& val) override;

    bool configure(std::string_view name, const std::vector<bool>& arr) override;
    bool configure(std::string_view name, const std::vector<int32_t>& arr) override;
    bool configure(std::string_view name, const std::vector<float>& arr) override;
    bool configure(std::string_view name, const std::vector<std::string>& arr) override;

    //
    std::unique_ptr<pipeline::Pipeline> finish(pipeline::EventSink* eventSink) const override;

    DecoderBuilder();

private:
    friend std::unique_ptr<pipeline::PipelineBuilder> createPipelineBuilderLegacy();

    DecoderConfig m_config;
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_PIPELINE_LEGACY_DECODER_BUILDER_H
