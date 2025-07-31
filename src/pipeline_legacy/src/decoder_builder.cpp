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

#include "decoder_builder.h"
//
#include "decoder.h"

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline_legacy/create_pipeline.h>
//
#include <memory>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

DecoderBuilder::DecoderBuilder() = default;

bool DecoderBuilder::configure(std::string_view name, bool val)
{
    // Make a 'real string' so we can get a null terminated char *
    std::string str(name);
    return m_config.set(str.c_str(), val);
}

bool DecoderBuilder::configure(std::string_view name, int32_t val)
{
    std::string str(name);
    return m_config.set(str.c_str(), val);
}

bool DecoderBuilder::configure(std::string_view name, float val)
{
    std::string str(name);
    return m_config.set(str.c_str(), val);
}

bool DecoderBuilder::configure(std::string_view name, const std::string& val)
{
    std::string str(name);
    return m_config.set(str.c_str(), val);
}

bool DecoderBuilder::configure(std::string_view name, const std::vector<bool>& arr)
{
    std::string str(name);
    return m_config.set(str.c_str(), arr);
}
bool DecoderBuilder::configure(std::string_view name, const std::vector<int32_t>& arr)
{
    std::string str(name);
    return m_config.set(str.c_str(), arr);
}
bool DecoderBuilder::configure(std::string_view name, const std::vector<float>& arr)
{
    std::string str(name);
    return m_config.set(str.c_str(), arr);
}
bool DecoderBuilder::configure(std::string_view name, const std::vector<std::string>& arr)
{
    std::string str(name);
    return m_config.set(str.c_str(), arr);
}

//
std::unique_ptr<pipeline::Pipeline> DecoderBuilder::finish(pipeline::EventSink* eventSink) const
{
    auto decoder = std::make_unique<Decoder>(m_config, eventSink);

    decoder->initialize();
    return decoder;
}

} // namespace lcevc_dec::decoder

// Factory function - extern "C" so it can be grabbed from DLLs
//
VN_LCEVC_PIPELINE_API lcevc_dec::pipeline::PipelineBuilder*
CREATE_PIPELINE_LEGACY_BUILDER_NAME(void* diagnosticsState, void* accelerationState)
{
    // Connect this shared libraries diagnostics and acceleration to parent
    ldcDiagnosticsInitialize(diagnosticsState);
    ldcAccelerationSet((const LdcAcceleration*)accelerationState);

    return new lcevc_dec::decoder::DecoderBuilder();
}
