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

#include <LCEVC/pipeline/tasks_base.h>
//
#include <LCEVC/common/log.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/pipeline/frame_base.h>
#include <LCEVC/pipeline/pipeline_base.h>
#include <LCEVC/pixel_processing/add.h>
#include <LCEVC/pixel_processing/apply_cmdbuffer.h>
#include <LCEVC/pixel_processing/convert.h>
#include <LCEVC/pixel_processing/sharpen.h>
#include <LCEVC/pixel_processing/upscale.h>

namespace lcevc_dec::pipeline {

// All tasks fns. are static - only exported function is generateTasks()
//
//// ConvertToInternal
//
// Copy incoming picture plane to internal fixed point surface format
//
// NB: There is likely a good templated C++ class that wraps these tasks up neatly,
// Worth figuring out once this has stabilised.
//
struct TaskConvertToInternalData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    uint32_t planeIndex;
    uint32_t baseDepth;
    uint32_t enhancementDepth;
};

static void* taskConvertToInternal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskConvertToInternalData));

    const TaskConvertToInternalData& data{VNTaskData(task, TaskConvertToInternalData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.planeIndex, "baseDepth",
                      data.baseDepth, "enhancementDepth", data.enhancementDepth);

    if (pipeline->isSkipped(frame)) {
        return nullptr;
    }

    bool isNV12 = frame->basePicture->layout.layoutInfo->format == LdpColorFormatNV12_8;
    uint32_t srcPlaneIndex = (isNV12 && data.planeIndex == 2) ? 1 : data.planeIndex;
    LdpPicturePlaneDesc srcPlane;
    frame->getBasePlaneDesc(srcPlaneIndex, srcPlane);

    // Intermediate buffers are set up so that unused ones point to higher LoQs - so requesting
    // LOQ2 will pick up the correct 'input' buffer
    LdpPicturePlaneDesc dstPlane;
    frame->getIntermediatePlaneDesc(data.planeIndex, LOQ2, dstPlane);

    VNLogDebug("taskConvertToInternal ts:%" PRIx64 " plane:%d enhanced:%d", data.frame->timestamp,
               data.planeIndex);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, LOQ2, data.planeIndex);
    if (!ldppPlaneConvert(pipeline->taskPool(), task, data.planeIndex, &frame->basePicture->layout,
                          frame->getIntermediateLayout(LOQ2), &srcPlane, &dstPlane,
                          VNDiagInfoPtr(diagInfo))) {
        VNLogError("ldppPlaneBlit In failed");
    }
    return nullptr;
}

LdcTaskDependency addTaskConvertToInternal(PipelineBase* pipeline, FrameBase* frame,
                                           uint32_t planeIndex, uint32_t baseDepth,
                                           uint32_t enhancementDepth, LdcTaskDependency inputDep)
{
    const TaskConvertToInternalData data{pipeline, frame, planeIndex, baseDepth, enhancementDepth};
    const LdcTaskDependency inputs[] = {inputDep};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskConvertToInternal, &data, sizeof(data),
                          "ConvertToInternal");
}

//// ConvertFromInternal
//
// Convert a picture plane from internal fixed point to output picture pixel format.
//
struct TaskConvertFromInternalData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    uint32_t planeIndex;
};

static void* taskConvertFromInternal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskConvertFromInternalData));

    const TaskConvertFromInternalData& data{VNTaskData(task, TaskConvertFromInternalData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.planeIndex);

    if (pipeline->isSkipped(frame)) {
        return nullptr;
    }

    LdpPicturePlaneDesc srcPlane;
    frame->getIntermediatePlaneDesc(data.planeIndex, LOQ0, srcPlane);

    bool isNV12 = frame->outputPicture->layout.layoutInfo->format == LdpColorFormatNV12_8;
    uint32_t dstPlaneIndex = (isNV12 && data.planeIndex == 2) ? 1 : data.planeIndex;
    LdpPicturePlaneDesc dstPlane;
    frame->getOutputPlaneDesc(dstPlaneIndex, dstPlane);

    VNLogDebug("taskConvertFromInternal ts:%" PRIx64 " plane:%d", data.frame->timestamp, data.planeIndex);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, LOQ0, data.planeIndex);
    if (!ldppPlaneConvert(pipeline->taskPool(), task, data.planeIndex,
                          frame->getIntermediateLayout(LOQ0), &frame->outputPicture->layout,
                          &srcPlane, &dstPlane, VNDiagInfoPtr(diagInfo))) {
        VNLogError("ldppPlaneBlit out failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskConvertFromInternal(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                             LdcTaskDependency dst, LdcTaskDependency src)
{
    const TaskConvertFromInternalData data{pipeline, frame, planeIndex};
    const LdcTaskDependency inputs[] = {dst, src};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskConvertFromInternal, &data, sizeof(data),
                          "ConvertFromInternal");
}

//// Sharpen
//
// Apply sharpening to an output picture plane.
//
struct TaskSharpenData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    uint32_t planeIndex;
};

static void* taskSharpen(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskSharpenData));

    const TaskSharpenData& data{VNTaskData(task, TaskSharpenData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.planeIndex);

    const float strength = frame->sharpeningStrength(data.planeIndex);
    const LdppDitherFrame* ditherFrame = frame->dither();

    if (pipeline->isSkipped(frame) || (strength == 0.0f && ditherFrame == nullptr)) {
        return nullptr;
    }

    LdpPicturePlaneDesc planeDesc{};
    frame->getOutputPlaneDesc(data.planeIndex, planeDesc);

    VNLogDebug("taskSharpen ts:%" PRIx64 " plane:%d strength:%f", frame->timestamp, data.planeIndex,
               strength);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, LOQMaxCount, data.planeIndex);
    if (!ldppSharpen(&frame->outputPicture->layout, &planeDesc, data.planeIndex, strength,
                     ditherFrame, VNDiagInfoPtr(diagInfo))) {
        VNLogError("Sharpening failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskSharpen(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                 LdcTaskDependency inputDep)
{
    const TaskSharpenData data{pipeline, frame, planeIndex};
    const LdcTaskDependency inputs[] = {inputDep};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskSharpen, &data, sizeof(data), "Sharpen");
}

//// Upscale
//
// Upscale (1D or 2D) between 16-bit intermediate buffers.
//
// Inputs and outputs may be fixed point or 'external' format if no residuals are being applied.
//
struct TaskUpscaleData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    LdeLOQIndex fromLoq;
    LdeScalingMode scalingMode;
    uint32_t plane;
};

static void* taskUpscale(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskUpscaleData));

    const TaskUpscaleData& data{VNTaskData(task, TaskUpscaleData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.plane, "loq",
                      static_cast<uint32_t>(data.fromLoq));

    if (pipeline->isSkipped(frame)) {
        return nullptr;
    }

    LdppUpscaleArgs upscaleArgs{};

    const LdeLOQIndex fromLoq = data.fromLoq;
    const LdeLOQIndex toLoq = static_cast<LdeLOQIndex>(fromLoq - 1);
    upscaleArgs.srcLayout = frame->getIntermediateLayout(fromLoq);
    frame->getIntermediatePlaneDesc(data.plane, fromLoq, upscaleArgs.srcPlane);
    upscaleArgs.dstLayout = frame->getIntermediateLayout(toLoq);
    frame->getIntermediatePlaneDesc(data.plane, toLoq, upscaleArgs.dstPlane);

    upscaleArgs.planeIndex = data.plane;
    upscaleArgs.kernel = &frame->globalConfig->kernel;
    upscaleArgs.applyPA = frame->globalConfig->predictedAverageEnabled;
    upscaleArgs.mode = data.scalingMode;
    // If stream requests sharpening and dithering, perform dithering in taskSharpen so as not
    // to sharpen the dither making it far too strong.
    const bool deferDitherToSharpen =
        (frame->dither() != NULL) && (frame->sharpeningStrength(data.plane) > 0.0f);
    upscaleArgs.frameDither = deferDitherToSharpen ? NULL : frame->dither();

    VNLogDebug("taskUpscale timestamp:%" PRIx64 " loq:%d plane:%d", frame->timestamp,
               (uint32_t)data.fromLoq, data.plane);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, data.fromLoq, data.plane);
    if (!ldppUpscale(pipeline->taskPool(), task, &upscaleArgs, VNDiagInfoPtr(diagInfo))) {
        VNLogError("Upscale failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskUpscale(PipelineBase* pipeline, FrameBase* frame, LdeLOQIndex fromLoq,
                                 uint32_t plane, LdcTaskDependency basePicture)
{
    assert(fromLoq > LOQ0);
    const LdeScalingMode scalingMode = frame->globalConfig->scalingModes[fromLoq - 1];
    assert(scalingMode != Scale0D);

    const TaskUpscaleData data{pipeline, frame, fromLoq, scalingMode, plane};
    const LdcTaskDependency inputs[] = {basePicture};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskUpscale, &data, sizeof(data), "Upscale");
}

//// Upscale Direct
//
// Upscale (1D or 2D) between directly from source picture to output in native format. Only for
// un-enhanced planes without residuals when there's a single upscale.
static void* taskUpscaleDirect(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskUpscaleData));

    const TaskUpscaleData& data{VNTaskData(task, TaskUpscaleData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.plane, "loq",
                      static_cast<uint32_t>(data.fromLoq));

    // Exit early if the frame is skipped or for NV12 plane 2 (NV12 chroma completed as one op)
    if (pipeline->isSkipped(frame) || data.plane >= ldpPictureLayoutPlanes(&frame->basePicture->layout)) {
        return nullptr;
    }

    LdppUpscaleArgs upscaleArgs{};

    upscaleArgs.srcLayout = &frame->basePicture->layout;
    frame->getBasePlaneDesc(data.plane, upscaleArgs.srcPlane);
    upscaleArgs.dstLayout = &frame->outputPicture->layout;
    frame->getOutputPlaneDesc(data.plane, upscaleArgs.dstPlane);

    upscaleArgs.planeIndex = data.plane;
    upscaleArgs.kernel = &frame->globalConfig->kernel;
    upscaleArgs.applyPA = frame->globalConfig->predictedAverageEnabled;
    upscaleArgs.frameDither = NULL; // No dithering on un-enhanced planes
    upscaleArgs.mode = data.scalingMode;

    const uint32_t srcWidth = ldpPictureLayoutPlaneWidth(upscaleArgs.srcLayout, data.plane) *
                              upscaleArgs.srcLayout->layoutInfo->interleave[data.plane];
    const uint32_t srcHeight = ldpPictureLayoutPlaneHeight(upscaleArgs.srcLayout, data.plane);
    const uint32_t dstWidth = ldpPictureLayoutPlaneWidth(upscaleArgs.dstLayout, data.plane) *
                              upscaleArgs.dstLayout->layoutInfo->interleave[data.plane];
    const uint32_t dstHeight = ldpPictureLayoutPlaneHeight(upscaleArgs.dstLayout, data.plane);

    VNLogDebug("taskUpscaleDirect timestamp:%" PRIx64 " loq:%d plane:%d", frame->timestamp,
               (uint32_t)data.fromLoq, data.plane);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, data.fromLoq, data.plane);

    const uint32_t expectedDstWidth = srcWidth << 1;
    const uint32_t expectedDstHeight = (data.scalingMode == Scale2D) ? (srcHeight << 1) : srcHeight;
    if (dstWidth != expectedDstWidth || dstHeight != expectedDstHeight) {
        VNLogError("UpscaleDirect invalid dimensions timestamp:%" PRIx64 " plane:%d mode:%d "
                   "src:%ux%u dst:%ux%u expected:%ux%u",
                   frame->timestamp, data.plane, static_cast<int>(data.scalingMode), srcWidth,
                   srcHeight, dstWidth, dstHeight, expectedDstWidth, expectedDstHeight);
        return nullptr;
    }

    if (!ldppUpscale(pipeline->taskPool(), task, &upscaleArgs, VNDiagInfoPtr(diagInfo))) {
        VNLogError("UpscaleDirect failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskUpscaleDirect(PipelineBase* pipeline, FrameBase* frame, LdeLOQIndex fromLoq,
                                       uint32_t plane, LdcTaskDependency basePicture,
                                       LdcTaskDependency outputPicture)
{
    assert(fromLoq > LOQ0);
    const LdeScalingMode scalingMode = frame->globalConfig->scalingModes[fromLoq - 1];
    assert(scalingMode != Scale0D);

    const TaskUpscaleData data{pipeline, frame, fromLoq, scalingMode, plane};
    const LdcTaskDependency inputs[] = {basePicture, outputPicture};

    return frame->taskAdd(inputs, VNArraySize(inputs), taskUpscaleDirect, &data, sizeof(data),
                          "UpscaleDirect");
}

//// GenerateCmdBufferCPU
//
// Convert un-encapsulated chunks into a single command buffer.
//
struct TaskGenerateCmdBufferCPUData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    LdpEnhancementTile* enhancementTile;
};

static void* taskGenerateCmdBufferCPU(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskGenerateCmdBufferCPUData));

    const TaskGenerateCmdBufferCPUData& data{VNTaskData(task, TaskGenerateCmdBufferCPUData)};
    const PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "tile", data.enhancementTile->tile, "loq",
                      (uint32_t)data.enhancementTile->loq, "plane", data.enhancementTile->plane,
                      "lcevc_size", frame->config.unencapsulatedAllocation.size);

    VNLogDebug("taskGenerateCmdBufferCPU ts:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    if (pipeline->isFlushed(frame)) {
        return nullptr;
    }

    if (!ldeDecodeEnhancement(frame->globalConfig, &frame->config, data.enhancementTile->loq,
                              data.enhancementTile->plane, data.enhancementTile->tile,
                              &data.enhancementTile->buffer, nullptr, nullptr)) {
        VNLogError("ldeDecodeEnhancement failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskGenerateCmdBufferCPU(PipelineBase* pipeline, FrameBase* frame,
                                              LdpEnhancementTile* enhancementTile)
{
    const TaskGenerateCmdBufferCPUData data{pipeline, frame, enhancementTile};
    return frame->taskAdd(nullptr, 0, taskGenerateCmdBufferCPU, &data, sizeof(data), "GenerateCmdBufferCPU");
}

//// GenerateCmdBufferGPU
//
// Convert un-encapsulated chunks into a single command buffer.
//
struct TaskGenerateCmdBufferGPUData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    LdpEnhancementTile* enhancementTile;
};

static void* taskGenerateCmdBufferGPU(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskGenerateCmdBufferGPUData));

    const TaskGenerateCmdBufferGPUData& data{VNTaskData(task, TaskGenerateCmdBufferGPUData)};
    const PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "tile", data.enhancementTile->tile, "loq",
                      (uint32_t)data.enhancementTile->loq, "plane", data.enhancementTile->plane,
                      "lcevc_size", frame->config.unencapsulatedAllocation.size);

    VNLogDebug("taskGenerateCmdBufferGPU ts:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    if (pipeline->isFlushed(frame)) {
        return nullptr;
    }

    if (!ldeDecodeEnhancement(frame->globalConfig, &frame->config, data.enhancementTile->loq,
                              data.enhancementTile->plane, data.enhancementTile->tile, nullptr,
                              &data.enhancementTile->bufferGpu, &data.enhancementTile->bufferGpuBuilder)) {
        VNLogError("ldeDecodeEnhancement failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskGenerateCmdBufferGPU(PipelineBase* pipeline, FrameBase* frame,
                                              LdpEnhancementTile* enhancementTile)
{
    const TaskGenerateCmdBufferGPUData data{pipeline, frame, enhancementTile};
    return frame->taskAdd(nullptr, 0, taskGenerateCmdBufferGPU, &data, sizeof(data), "GenerateCmdBufferGPU");
}

//// ApplyCmdBufferDirect
//
// Apply a generated Base command buffer to directly to output plane. (No Temporal)
//
// NB: The output plane will be in 'internal' fixed point format
//
struct TaskApplyCmdBufferDirectData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    LdpEnhancementTile* enhancementTile;
};

static void* taskApplyCmdBufferDirect(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskApplyCmdBufferDirectData));

    const TaskApplyCmdBufferDirectData& data{VNTaskData(task, TaskApplyCmdBufferDirectData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "tile", data.enhancementTile->tile, "loq",
                      (uint32_t)data.enhancementTile->loq, "plane", data.enhancementTile->plane);

    if (pipeline->isSkipped(frame)) {
        return nullptr;
    }

    VNLogDebug("taskApplyCmdBufferDirect ts:%" PRIx64 " loq:%d plane:%d", data.frame->timestamp,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    LdpPicturePlaneDesc ppDesc{};

    frame->getIntermediatePlaneDesc(data.enhancementTile->plane, data.enhancementTile->loq, ppDesc);

    const bool tuRasterOrder =
        !frame->globalConfig->temporalEnabled && frame->globalConfig->tileDimensions == TDTNone;

    VNDiagInfo(diagInfo, task->name, frame->timestamp, data.enhancementTile->loq,
               data.enhancementTile->plane);

    if (!ldppApplyCmdBuffer(pipeline->taskPool(), NULL, data.enhancementTile, LdpFPS14, &ppDesc, tuRasterOrder,
                            pipeline->configuration().highlightResiduals, VNDiagInfoPtr(diagInfo))) {
        VNLogError("taskApplyCmdBufferDirect failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskApplyCmdBufferDirect(PipelineBase* pipeline, FrameBase* frame,
                                              LdpEnhancementTile* enhancementTile,
                                              LdcTaskDependency imageBuffer, LdcTaskDependency cmdBuffer)
{
    const TaskApplyCmdBufferDirectData data{pipeline, frame, enhancementTile};

    const LdcTaskDependency inputs[] = {imageBuffer, cmdBuffer};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskApplyCmdBufferDirect, &data,
                          sizeof(data), "ApplyCmdBufferDirect");
}

//// ApplyCmdBufferTemporal
//
// Apply a generated Base command buffer to a temporal buffer.
//
struct TaskApplyCmdBufferTemporalData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    LdpEnhancementTile* enhancementTile;
};

static void* taskApplyCmdBufferTemporal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskApplyCmdBufferTemporalData));

    const TaskApplyCmdBufferTemporalData& data{VNTaskData(task, TaskApplyCmdBufferTemporalData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "tile", data.enhancementTile->tile, "loq",
                      (uint32_t)data.enhancementTile->loq, "plane", data.enhancementTile->plane,
                      "cmdbuffer_count", data.enhancementTile->buffer.count);

    VNLogDebug("taskApplyCmdBufferTemporal ts:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    if (pipeline->isFlushed(frame)) {
        return nullptr;
    }

    LdpPicturePlaneDesc ppDesc{};
    frame->getTemporalBufferPlaneDesc(data.enhancementTile->plane, ppDesc);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, data.enhancementTile->loq,
               data.enhancementTile->plane);

    if (!ldppApplyCmdBuffer(pipeline->taskPool(), NULL, data.enhancementTile, LdpFPS14, &ppDesc, false,
                            pipeline->configuration().highlightResiduals, VNDiagInfoPtr(diagInfo))) {
        VNLogError("ldppApplyCmdBufferTemporal failed");
    }
    return nullptr;
}

LdcTaskDependency addTaskApplyCmdBufferTemporal(PipelineBase* pipeline, FrameBase* frame,
                                                LdpEnhancementTile* enhancementTile,
                                                LdcTaskDependency temporalBuffer, LdcTaskDependency cmdBuffer)
{
    const TaskApplyCmdBufferTemporalData data{pipeline, frame, enhancementTile};
    const LdcTaskDependency inputs[] = {temporalBuffer, cmdBuffer};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskApplyCmdBufferTemporal, &data,
                          sizeof(data), "ApplyCmdBufferTemporal");
}

//// ApplyAddTemporal
//
// Add a temporal buffer to a picture plane.
//
struct TaskApplyAddTemporalData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    uint32_t planeIndex;
};

static void* taskApplyAddTemporal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskApplyAddTemporalData));

    const TaskApplyAddTemporalData& data{VNTaskData(task, TaskApplyAddTemporalData)};
    PipelineBase* const pipeline{data.pipeline};
    FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.planeIndex);

    if (pipeline->isSkipped(frame) || frame->isPassthrough()) {
        // Just move temporal buffer along pipeline
        pipeline->transferTemporalBuffer(frame, data.planeIndex);
        return nullptr;
    }

    VNLogDebug("taskApplyAddTemporal ts:%" PRIx64 " plane:%d", data.frame->timestamp, data.planeIndex);

    LdpPicturePlaneDesc srcPlane{};
    frame->getIntermediatePlaneDesc(data.planeIndex, LOQ0, srcPlane);

    LdpPicturePlaneDesc tbDesc{};
    frame->getTemporalBufferPlaneDesc(data.planeIndex, tbDesc);

    LdpPicturePlaneDesc dstPlane{};
    frame->getOutputPlaneDesc(data.planeIndex, dstPlane);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, LOQ0, data.planeIndex);

    if (!ldppPlaneAdd(pipeline->taskPool(), task, data.planeIndex,
                      frame->getIntermediateLayout(LOQ0), &frame->outputPicture->layout, &tbDesc,
                      &srcPlane, &dstPlane, VNDiagInfoPtr(diagInfo))) {
        VNLogError("ldppPlaneAdd out failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskApplyAddTemporal(PipelineBase* pipeline, FrameBase* frame,
                                          uint32_t planeIndex, LdcTaskDependency temporalDep,
                                          LdcTaskDependency sourceDep, LdcTaskDependency outputDep)
{
    const TaskApplyAddTemporalData data{pipeline, frame, planeIndex};
    const LdcTaskDependency inputs[] = {temporalDep, sourceDep, outputDep};
    return frame->taskAdd(inputs, VNArraySize(inputs), taskApplyAddTemporal, &data, sizeof(data),
                          "ApplyAddTemporal");
}

//// Passthrough
//
// Copy incoming picture plane to output picture
//
struct TaskPassthroughData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    uint32_t planeIndex;
};

static void* taskPassthrough(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskPassthroughData));

    const TaskPassthroughData& data{VNTaskData(task, TaskPassthroughData)};
    PipelineBase* const pipeline{data.pipeline};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.planeIndex);

    if (pipeline->isSkipped(frame)) {
        return nullptr;
    }

    // Check if this plane is valid
    if (data.planeIndex >= ldpPictureLayoutPlanes(&frame->basePicture->layout)) {
        return nullptr;
    }

    LdpPicturePlaneDesc srcPlane;
    frame->getBasePlaneDesc(data.planeIndex, srcPlane);

    LdpPicturePlaneDesc dstPlane;
    frame->getOutputPlaneDesc(data.planeIndex, dstPlane);

    VNLogDebug("taskPassthrough ts:%" PRIx64 " plane:%d", data.frame->timestamp, data.planeIndex);

    VNDiagInfo(diagInfo, task->name, frame->timestamp, LOQ0, data.planeIndex);
    if (!ldppPlaneConvert(pipeline->taskPool(), task, data.planeIndex, &frame->basePicture->layout,
                          &frame->outputPicture->layout, &srcPlane, &dstPlane, VNDiagInfoPtr(diagInfo))) {
        VNLogError("ldppPlaneBlit In failed");
    }

    return nullptr;
}

LdcTaskDependency addTaskPassthrough(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                     LdcTaskDependency dest, LdcTaskDependency src)
{
    const TaskPassthroughData data{pipeline, frame, planeIndex};
    const LdcTaskDependency inputs[] = {dest, src};

    return frame->taskAdd(inputs, VNArraySize(inputs), taskPassthrough, &data, sizeof(data), "Passthrough");
}

//// WaitForMany
//
// Wait for several input dependencies to be met.
//
// NB: If this appears to be a bottleneck, it could be integrated better into the task pool.
//
struct TaskWaitForManyData
{
    PipelineBase* pipeline;
    FrameBase* frame;
};

static void* taskWaitForMany(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskWaitForManyData));
#if VN_SDK_FEATURE(TRACING)
    const TaskWaitForManyData& data{VNTaskData(task, TaskWaitForManyData)};
    const FrameBase* const frame{data.frame};
#endif
    VNTraceScopedArgs("timestamp", frame->timestamp);

    VNLogDebug("taskWaitForMany ts:%" PRIx64 "", VNTaskData(task, TaskWaitForManyData).frame->timestamp);
    return nullptr;
}

LdcTaskDependency addTaskWaitForMany(PipelineBase* pipeline, FrameBase* frame,
                                     const LdcTaskDependency* inputs, uint32_t inputsCount)
{
    const TaskWaitForManyData data{pipeline, frame};

    return frame->taskAdd(inputs, inputsCount, taskWaitForMany, &data, sizeof(data), "WaitForMany");
}

//// BaseDone
//
// Wait for base picture planes to be used, then send base picture back to client
//
struct TaskBaseDoneData
{
    PipelineBase* pipeline;
    FrameBase* frame;
};

static void* taskBaseDone(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskBaseDoneData));
    const TaskBaseDoneData& data{VNTaskData(task, TaskBaseDoneData)};
    const FrameBase* const frame{data.frame};

    VNTraceScopedArgs("timestamp", frame->timestamp);

    VNLogDebug("taskBaseDone ts:%" PRIx64, data.frame->timestamp);

    if (frame->basePicture == nullptr) {
        return nullptr;
    }

    // Generate event and return picture
    data.pipeline->baseDone(data.frame->basePicture);

    // Frame no longer has access to base picture
    data.frame->basePicture = nullptr;
    return nullptr;
}

void addTaskBaseDone(PipelineBase* pipeline, FrameBase* frame, const LdcTaskDependency* inputs,
                     uint32_t inputsCount)
{
    const TaskBaseDoneData data{pipeline, frame};

    frame->taskAddSink(inputs, inputsCount, taskBaseDone, &data, sizeof(data), "BaseDone");
}

//// OutputSend
//
// Wait for a bunch of input dependencies to be met, then:
//
// - Send output picture to output queue
// - Release frame
//
struct TaskOutputDoneData
{
    PipelineBase* pipeline;
    FrameBase* frame;
};

static void* taskOutputDone(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskOutputDoneData));
    const TaskOutputDoneData& data{VNTaskData(task, TaskOutputDoneData)};
    PipelineBase* const pipeline{data.pipeline};
    FrameBase* const frame{data.frame};
    VNTraceScopedArgs("timestamp", frame->timestamp);

    VNLogDebug("taskOutputDone ts:%" PRIx64, frame->timestamp);

    // Build the decode info for the frame
    frame->decodeInformation.timestamp = frame->timestamp;
    frame->decodeInformation.hasBase = true;
    frame->decodeInformation.hasEnhancement =
        frame->config.loqEnabled[LOQ1] || frame->config.loqEnabled[LOQ0];
    frame->decodeInformation.skipped = pipeline->isSkipped(frame);
    frame->decodeInformation.enhanced = !pipeline->isPassthrough(frame);
    frame->decodeInformation.baseWidth = frame->baseWidth;
    frame->decodeInformation.baseHeight = frame->baseHeight;
    frame->decodeInformation.baseBitdepth = frame->baseBitdepth;
    frame->decodeInformation.userData = frame->userData;

    // Mark frame as done
    pipeline->outputDone(frame);

    return nullptr;
}

void addTaskOutputDone(PipelineBase* pipeline, FrameBase* frame, const LdcTaskDependency* inputs,
                       uint32_t inputsCount)
{
    const TaskOutputDoneData data{pipeline, frame};

    frame->taskAddSink(inputs, inputsCount, taskOutputDone, &data, sizeof(data), "OutputDone");
}

//// TemporalTransfer
//
// Wait for a bunch of input dependencies to be met, then transfer temporal buffer to next frame
//
struct TaskTemporalTransferData
{
    PipelineBase* pipeline;
    FrameBase* frame;
    uint32_t planeIndex;
};

static void* taskTemporalTransfer(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskTemporalTransferData));
    const TaskTemporalTransferData& data{VNTaskData(task, TaskTemporalTransferData)};
    PipelineBase* const pipeline{data.pipeline};
    FrameBase* const frame{data.frame};
    VNTraceScopedArgs("timestamp", frame->timestamp, "plane", data.planeIndex);

    VNLogDebug("taskTemporalTransfer ts:%" PRIx64, frame->timestamp);

    pipeline->transferTemporalBuffer(frame, data.planeIndex);

    return nullptr;
}

void addTaskTemporalTransfer(PipelineBase* pipeline, FrameBase* frame, const LdcTaskDependency dep,
                             uint32_t planeIndex)
{
    const TaskTemporalTransferData data{pipeline, frame, planeIndex};
    const LdcTaskDependency inputs[] = {dep};

    frame->taskAddSink(inputs, VNArraySize(inputs), taskTemporalTransfer, &data, sizeof(data),
                       "TemporalTransfer");
}

} // namespace lcevc_dec::pipeline
