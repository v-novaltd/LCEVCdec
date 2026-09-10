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

#ifndef VN_LCEVC_PIPELINE_CPU_FRAME_CPU_H
#define VN_LCEVC_PIPELINE_CPU_FRAME_CPU_H

#include <LCEVC/pipeline/frame_base.h>

namespace lcevc_dec::pipeline_cpu {

class PipelineCPU;
class PictureCPU;
struct PipelineConfigCPU;

// Extended frame structure for this pipeline
//
class FrameCPU : public pipeline::FrameBase
{
public:
    FrameCPU(LdcMemoryAllocator* enhancementAllocator, LdcMemoryAllocator* bufferAllocator,
             uint64_t timestamp);
    ~FrameCPU() = default;

    // Add tasks to pipeline to decode frame
    void generateTasks(pipeline::PipelineBase* pipeline, uint64_t previousTimestamp) override;

    // Get plane descriptions for each of the buffers
    void getBasePlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const override;
    void getOutputPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const override;

    void getIntermediatePlaneDesc(uint32_t plane, LdeLOQIndex loq,
                                  LdpPicturePlaneDesc& planeDesc) const override;
    void getTemporalBufferPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const override;

    bool initializeIntermediateBuffers() override;
    void releaseIntermediateBuffers() override;
    bool needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const override;
    const LdpPictureLayout* getIntermediateLayout(LdeLOQIndex loq) const override;

    bool needsGPUCommandBuffers() const override { return false; };

    LdcReturnCode decodeResult() const override { return LdcReturnCodeSuccess; }

    VNNoCopyNoMove(FrameCPU);

private:
    LdcMemoryAllocator* m_bufferAllocator{};

    // Internal buffers for residual application
    LdcMemoryAllocation m_intermediateBufferAllocation[RCMaxPlanes][LOQMaxCount] = {};
    LdpPictureLayout m_intermediateLayout[LOQMaxCount] = {};

    // Pointers to buffer to use for each LOQ - may share buffers between LoQs depending on scaling modes
    uint8_t* m_intermediateBufferPtr[RCMaxPlanes][LOQMaxCount] = {};

    // True if intermediate buffers are setup
    bool m_intermediateInitialized{false};
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_FRAME_CPU_H
