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

#ifndef VN_LCEVC_PIPELINE_PIPELINE_H
#define VN_LCEVC_PIPELINE_PIPELINE_H

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/configure.hpp>
#include <LCEVC/common/return_code.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/types.h>
//
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lcevc_dec::pipeline {

// PipelineBuilder
//
// Interface between API and decoder pipeline creation.
//
//
// Pipeline implementations will expose a factory function along the lines of:
//
// std::unique_ptr<PipelineBuilder> createPipelineBuilderXXXX();
//
// Depending on the pipeline, it may need connections to system objects or resources - these
// would be pipeline specific parameters to the factory function.
//
// Configuration settings are passed to the builder, then `finish()` is called to create the actual
// pipeline.
//
// This two stage process is to allow the specialization of the Pipeline implementation depending
// on configuration, and to keep the online interface clear of configuration.
//
class Pipeline;

class PipelineBuilder : public common::Configurable
{
protected:
    PipelineBuilder() = default;

public:
    virtual ~PipelineBuilder() = 0;

    virtual std::unique_ptr<Pipeline> finish(EventSink* eventSink) const = 0;

    VNNoCopyNoMove(PipelineBuilder);

private:
};

// Pipeline
//
// Interface between API and decoder pipelines.
//
class Pipeline
{
protected:
    Pipeline() = default;

public:
    virtual ~Pipeline() = 0;

    // Send/receive
    virtual LdcReturnCode sendBasePicture(uint64_t timestamp, LdpPicture* basePicture,
                                          uint32_t timeoutUs, void* userData) = 0;
    virtual LdcReturnCode sendEnhancementData(uint64_t timestamp, const uint8_t* data,
                                              uint32_t byteSize) = 0;
    virtual LdcReturnCode sendOutputPicture(LdpPicture* outputPicture) = 0;

    virtual LdpPicture* receiveOutputPicture(LdpDecodeInformation& decodeInfoOut) = 0;
    virtual LdpPicture* receiveFinishedBasePicture() = 0;

    // "Trick-play"
    virtual LdcReturnCode flush(uint64_t timestamp) = 0;
    virtual LdcReturnCode peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut) = 0;
    virtual LdcReturnCode skip(uint64_t timestamp) = 0;
    virtual LdcReturnCode synchronize(bool dropPending) = 0;

    // Picture-handling
    virtual LdpPicture* allocPictureManaged(const LdpPictureDesc& desc) = 0;
    virtual LdpPicture* allocPictureExternal(const LdpPictureDesc& desc,
                                             const LdpPicturePlaneDesc* planeDescArr,
                                             const LdpPictureBufferDesc* buffer) = 0;

    virtual void freePicture(LdpPicture* picture) = 0;

    VNNoCopyNoMove(Pipeline);

private:
};

} // namespace lcevc_dec::pipeline

#endif // VN_LCEVC_PIPELINE_PIPELINE_H
