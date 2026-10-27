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

#include "tasks_vulkan.h"

#include "from_base.h"
#include "picture_vulkan.h"

#include <LCEVC/common/constants.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/tasks_base.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
#include <LCEVC/pixel_processing/apply_cmdbuffer.h>
#include <LCEVC/pixel_processing/upscale.h>

namespace lcevc_dec::pipeline_vulkan {

namespace {

    PictureVulkan* acquireIntermediatePicture(PipelineVulkan* pipeline)
    {
        return pipeline->allocatePicture();
    }

    void discardIntermediatePicture(PipelineVulkan* pipeline, PictureVulkan* picture)
    {
        pipeline->releasePicture(picture);
    }

    // Bind and upload the base picture, compacting external planes when needed.
    bool vPrepareBaseInput(VulkanFrameContext* context)
    {
        auto* srcPicture = fromPipeline(context->getFrame()->basePicture);
        if (!srcPicture) {
            return false;
        }

        LdpPictureBufferDesc exDesc{};
        const bool hasBufferDesc = srcPicture->getBufferDesc(exDesc);
        LdpPicturePlaneDesc srcPlaneDescs[kLdpPictureMaxNumPlanes]{};
        if (srcPicture->getPlaneDescArr(srcPlaneDescs)) {
            if (srcPicture->buffer == nullptr) {
                if (!srcPicture->bindBuffer(pipeline::BufferUsageIn)) {
                    VNLogError("bindBuffer failed for ConvertToInternal");
                    return false;
                }
            }

            auto* managedBuffer = fromPipeline(srcPicture->buffer);
            if (!managedBuffer) {
                VNLogError("managedBuffer is null for ConvertToInternal");
                return false;
            }

            uint32_t compactSize = 0;
            bool needsCompaction = !hasBufferDesc || exDesc.data == nullptr;
            const uint8_t planeCount = ldpPictureLayoutPlanes(&srcPicture->layout);

            for (uint8_t plane = 0; plane < planeCount; ++plane) {
                const uint32_t rowBytes = ldpPictureLayoutRowSize(&srcPicture->layout, plane);
                const uint32_t planeHeight = ldpPictureLayoutPlaneHeight(&srcPicture->layout, plane);

                if (srcPlaneDescs[plane].rowByteStride != rowBytes) {
                    needsCompaction = true;
                }
                if (hasBufferDesc && exDesc.data &&
                    srcPlaneDescs[plane].firstSample != (exDesc.data + compactSize)) {
                    needsCompaction = true;
                }

                compactSize += rowBytes * planeHeight;
            }

            VNTraceBeginArgs("vulkan:input.compactAndCopy", "bytes", compactSize, "needsCompaction",
                             needsCompaction);
            {
                LdcTraceScoped copyTrace;
                if (!needsCompaction && hasBufferDesc && exDesc.byteSize == compactSize) {
                    managedBuffer->copyIn(0, exDesc.data, exDesc.byteSize);
                } else {
                    uint32_t compactOffset = 0;
                    for (uint8_t plane = 0; plane < planeCount; ++plane) {
                        const uint32_t rowBytes = ldpPictureLayoutRowSize(&srcPicture->layout, plane);
                        const uint32_t planeHeight =
                            ldpPictureLayoutPlaneHeight(&srcPicture->layout, plane);

                        for (uint32_t y = 0; y < planeHeight; ++y) {
                            managedBuffer->copyIn(compactOffset + y * rowBytes,
                                                  srcPlaneDescs[plane].firstSample +
                                                      y * srcPlaneDescs[plane].rowByteStride,
                                                  rowBytes);
                        }

                        srcPicture->layout.rowStrides[plane] = rowBytes;
                        srcPicture->layout.planeOffsets[plane] = compactOffset;
                        compactOffset += rowBytes * planeHeight;
                    }
                    srcPicture->layout.size = compactOffset;
                }
            }
        }

        return true;
    }

    // Convert the prepared base picture to internal fixed point.
    bool vConvertToInternal(PipelineVulkan* pipeline, VulkanFrameContext* context,
                            const std::array<bool, kLdpPictureMaxNumPlanes>& selectedPhysicalPlanes)
    {
        VNTraceScopedNamedArgs("vulkan:vConvertToInternal", "timestamp", context->getFrame()->timestamp);
        auto* srcPicture = fromPipeline(context->getFrame()->basePicture);
        if (!srcPicture) {
            return false;
        }
        bool conversionNeeded = false;
        for (uint8_t plane = 0; plane < ldpPictureLayoutPlanes(&srcPicture->layout); ++plane) {
            conversionNeeded |= selectedPhysicalPlanes[plane];
        }
        if (!conversionNeeded) {
            return true;
        }

        LdpPictureDesc srcDesc;
        srcPicture->getDesc(srcDesc);
        srcDesc.colorFormat = pipeline->chromaToColorFormat(pipeline->getChroma());
        PictureVulkan* dstPicture = acquireIntermediatePicture(pipeline);
        if (dstPicture == nullptr) {
            VNLogError("allocatePicture failed for ConvertToInternal");
            return false;
        }

        if (!dstPicture->setDescAndBind(srcDesc, pipeline::BufferUsageInternal)) {
            VNLogError("setDescAndBind failed for ConvertToInternal");
            discardIntermediatePicture(pipeline, dstPicture);
            return false;
        }

        BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);
        if (dstBuffer == nullptr || dstBuffer->getVkBuffer() == VK_NULL_HANDLE || dstBuffer->size() == 0) {
            VNLogError("invalid internal buffer for ConvertToInternal");
            discardIntermediatePicture(pipeline, dstPicture);
            return false;
        }
        context->getFrame()->addIntermediatePicture(dstPicture);

        VulkanConvertInArgs args{};
        args.src = srcPicture;
        args.dst = dstPicture;
        args.bitDepth = context->getFrame()->baseBitdepth;
        args.chroma = pipeline->getChroma();
        args.context = context;

        if (!pipeline->backend().prepareConvertIn(&args)) {
            VNLogError("Failed to prepare ConvertIn descriptors");
            return false;
        }
        for (uint8_t plane = 0; plane < ldpPictureLayoutPlanes(&srcPicture->layout); ++plane) {
            if (!selectedPhysicalPlanes[plane]) {
                continue;
            }
            if (!VNTraceFuncNamed("vulkan:input.recordConversion",
                                  pipeline->backend().convertIn(&args, plane))) {
                VNLogError("Conversion to internal failed for physical plane %u", plane);
                return false;
            }
        }
        context->insertComputeBarrier("barrier_after_convert_in");

        context->getFrame()->setIntermediatePicture(LOQ2, dstPicture);

        return true;
    }

    // Allocate the i16 destination once before recording plane upscales.
    PictureVulkan* vPrepareUpscaleDestination(PipelineVulkan* pipeline, VulkanFrameContext* context,
                                              PictureVulkan* srcPicture, LdeScalingMode mode,
                                              uint8_t destination)
    {
        if ((mode != Scale1D && mode != Scale2D) || !srcPicture) {
            return nullptr;
        }
        LdpPictureDesc desc{};
        srcPicture->getDesc(desc);
        desc.colorFormat = pipeline->chromaToColorFormat(pipeline->getChroma());
        desc.width *= 2;
        if (mode == Scale2D) {
            desc.height *= 2;
        }

        PictureVulkan* dstPicture = acquireIntermediatePicture(pipeline);
        if (!dstPicture) {
            return nullptr;
        }
        if (!dstPicture->setDescAndBind(desc, pipeline::BufferUsageInternal)) {
            discardIntermediatePicture(pipeline, dstPicture);
            return nullptr;
        }
        auto* dstBuffer = fromPipeline(dstPicture->buffer);
        if (!dstBuffer || dstBuffer->getVkBuffer() == VK_NULL_HANDLE || dstBuffer->size() == 0) {
            discardIntermediatePicture(pipeline, dstPicture);
            return nullptr;
        }
        context->getFrame()->addIntermediatePicture(dstPicture);
        context->getFrame()->setIntermediatePicture(destination, dstPicture);
        return dstPicture;
    }

    // Convert the internal picture to the requested output format.
    bool vConvertFromInternal(PipelineVulkan* pipeline, VulkanFrameContext* context,
                              uint32_t bitDepth, uint8_t intermediatePtr)
    {
        VNTraceScopedNamedArgs("vulkan:vConvertFromInternal", "timestamp", context->getFrame()->timestamp);
        auto* srcPicture = context->getFrame()->getIntermediatePicture(intermediatePtr);
        if (!srcPicture) {
            VNLogError("srcPicture is null for ConvertFromInternal");
            return false;
        }

        LdpPictureDesc dstDesc;
        srcPicture->getDesc(dstDesc);

        auto* dstPicture = fromPipeline(context->getFrame()->outputPicture);
        if (!dstPicture) {
            VNLogError("dstPicture is null for ConvertFromInternal");
            return false;
        }

        dstDesc.colorFormat = context->getFrame()->outputPicture->layout.layoutInfo->format;

        PictureVulkan* outPicture = fromPipeline(context->getFrame()->outputPicture);
        if (!outPicture) {
            VNLogError("outPicture is null for ConvertFromInternal");
            return false;
        }
        if (!outPicture->setDescAndBind(dstDesc, pipeline::BufferUsageOut)) {
            VNLogError("setDescAndBind failed for ConvertFromInternal");
            return false;
        }

        // sharpeningStrength() and the crop check both touch globalConfig,
        // which is null in the unscaled-passthrough path.
        const auto* gc = context->getFrame()->globalConfig;
        VulkanConvertOutArgs args{};
        args.src = srcPicture;
        args.dst = dstPicture;
        args.bitDepth = bitDepth;
        args.chroma = pipeline->getChroma();
        args.sharpenStrength = gc ? context->getFrame()->sharpeningStrength(0) : 0.0f;
        args.temporal = pipeline->m_temporalPicture;
        args.numEnhancedPlanes = context->getFrame()->numEnhancedPlanes();
        args.addTemporal = gc && gc->temporalEnabled && !context->getFrame()->isPassthrough() &&
                           pipeline->m_temporalPicture && pipeline->m_temporalPicture->buffer != nullptr &&
                           !pipeline->m_temporalPicture->temporalBufferEmpty;
        args.context = context;

        if (!pipeline->backend().convertOut(&args)) {
            VNLogError("Conversion from internal failed");
            return false;
        }

        // Crop only applies when there's an LCEVC global configuration; the
        // unscaled-passthrough path runs without one.
        if (gc && gc->cropEnabled) {
            args.dst->margins.left = gc->crop.left;
            args.dst->margins.right = gc->crop.right;
            args.dst->margins.top = gc->crop.top;
            args.dst->margins.bottom = gc->crop.bottom;
        }

        return true;
    }

    // Upscale an internal picture by one LOQ.
    bool vUpscale(PipelineVulkan* pipeline, VulkanFrameContext* context, LdeLOQIndex loq, uint8_t intermediatePtr)
    {
        VNTraceScopedNamedArgs("vulkan:vUpscale", "timestamp", context->getFrame()->timestamp,
                               "loq", static_cast<uint32_t>(loq));
        auto* srcPicture = context->getFrame()->getIntermediatePicture(intermediatePtr);
        if (!srcPicture) {
            VNLogError("getIntermediatePicture failed for Upscale");
            return false;
        }

        const uint8_t destination = intermediatePtr - 1;
        const LdeScalingMode mode = context->getFrame()->globalConfig->scalingModes[loq - 1];
        PictureVulkan* dstPicture =
            vPrepareUpscaleDestination(pipeline, context, srcPicture, mode, destination);
        if (dstPicture == nullptr) {
            VNLogError("Failed to prepare Upscale destination");
            return false;
        }

        VulkanUpscaleArgs upscaleArgs{};
        upscaleArgs.src = srcPicture;
        upscaleArgs.dst = dstPicture;
        upscaleArgs.base = srcPicture;
        upscaleArgs.pa = context->getFrame()->globalConfig->predictedAverageEnabled;
        upscaleArgs.mode = mode;
        upscaleArgs.loq = static_cast<LdeLOQIndex>(loq - 1);
        upscaleArgs.chroma = pipeline->getChroma();
        upscaleArgs.context = context;

        assert(upscaleArgs.mode != Scale0D);
        VNLogDebug("taskUpsample timestamp:%" PRIx64 " loq:%d", context->getFrame()->timestamp,
                   (uint32_t)loq);

        if (!pipeline->backend().prepareUpscale(&upscaleArgs)) {
            VNLogError("Failed to prepare Upscale descriptors");
            return false;
        }
        for (uint8_t plane = 0; plane < ldpPictureLayoutPlanes(&srcPicture->layout); ++plane) {
            if (!VNTraceFuncNamed("vulkan:upscale.recordCommands",
                                  pipeline->backend().upscale(&context->getFrame()->globalConfig->kernel,
                                                              &upscaleArgs, plane))) {
                VNLogError("Upsample failed for plane %u", plane);
                return false;
            }
        }
        context->insertComputeBarrier(mode == Scale2D ? "barrier_after_upscale_2d"
                                                      : "barrier_after_upscale_1d");

        return true;
    }

    // Bind the resources shared by apply dispatches at this LOQ.
    bool vApplyCommon(PipelineVulkan* pipeline, VulkanFrameContext* context,
                      LdpEnhancementTile* enhancementTile, uint8_t intermediatePtr, bool applyDirect)
    {
        VNTraceScopedNamedArgs("vulkan:vApplyCommon", "timestamp", context->getFrame()->timestamp,
                               "loq", static_cast<uint32_t>(enhancementTile->loq));
        auto* picture = context->getFrame()->getIntermediatePicture(intermediatePtr);
        if (!picture) {
            VNLogError("Invalid picture for intermediatePtr %u", intermediatePtr);
            return false;
        }

        VulkanApplyCommonArgs args{};
        pipeline->prepareApplyCommonArgs(args, picture, enhancementTile, context, applyDirect);
        if (!applyDirect) {
            args.temporalPicture = pipeline->m_temporalPicture;
        }

        if (!pipeline->backend().applyCommon(&args)) {
            VNLogError("Vulkan applyCommon failed");
            return false;
        }

        return true;
    }

    // Apply a tile directly to the internal picture.
    bool vApplyCmdBufferDirect(PipelineVulkan* pipeline, VulkanFrameContext* context,
                               LdpEnhancementTile* enhancementTile, uint8_t intermediatePtr)
    {
        auto* picture = context->getFrame()->getIntermediatePicture(intermediatePtr);
        if (!picture) {
            VNLogError("Invalid picture for intermediatePtr %u", intermediatePtr);
            return false;
        }

        VNLogDebug("taskApplyCmdBufferDirect timestamp:%" PRIx64 " loq:%d plane:%d",
                   context->getFrame()->timestamp, (uint32_t)enhancementTile->loq, enhancementTile->plane);

        VulkanApplyTileArgs args{};
        pipeline->prepareApplyTileArgs(args, picture, enhancementTile, context, true);

        if (!pipeline->backend().applyTile(&args)) {
            VNLogError("Vulkan apply direct failed");
            return false;
        }

        return true;
    }

    // Apply a tile to the temporal buffer.
    bool vApplyCmdBufferTemporal(PipelineVulkan* pipeline, VulkanFrameContext* context,
                                 LdpEnhancementTile* enhancementTile, uint8_t intermediatePtr)
    {
        VNLogDebug("taskApplyCmdBufferTemporal timestamp:%" PRIx64 " tile:%d loq:%d plane:%d",
                   context->getFrame()->timestamp, enhancementTile->tile,
                   (uint32_t)enhancementTile->loq, enhancementTile->plane);

        auto* picture = context->getFrame()->getIntermediatePicture(intermediatePtr);
        if (!picture) {
            VNLogError("Invalid picture for intermediatePtr %u", intermediatePtr);
            return false;
        }

        VulkanApplyTileArgs args{};
        pipeline->prepareApplyTileArgs(args, picture, enhancementTile, context, false);
        args.temporalPicture = pipeline->m_temporalPicture;

        if (!pipeline->backend().applyTile(&args)) {
            VNLogError("Vulkan apply temporal failed");
            return false;
        }

        return true;
    }

    struct TaskVulkanDecodeData
    {
        PipelineVulkan* pipeline;
        FrameVulkan* frame;
        uint64_t previousTimestamp;
    };

    // Every exit must hand off the submission turn, including pre-submission failures.
    // GPU completion remains responsible for publishing successful frame outcomes.
    struct ScopedSubmissionTurn
    {
        PipelineVulkan* pipeline;
        FrameVulkan* frame;
        ~ScopedSubmissionTurn() { pipeline->completeSubmissionTurn(frame); }
    };

    void abortVulkanTask(PipelineVulkan* pipeline, FrameVulkan* frame, VulkanFrameContext* frameContext)
    {
        const bool outcomeSet = frame->setComputeOutcome(VulkanComputeOutcome::Failed);

        frameContext->abort();
        frame->freeIntermediatePictures(pipeline);
        pipeline->backend().compute().releaseFrameContext(frameContext);

        // It is only safe to signalComputeTaskDone once, so don't do it if outcomeSet didn't work -
        // that means another caller managed to set the outcome before this one did. Applies to all
        // uses of signalComputeTaskDone.
        if (outcomeSet) {
            frame->signalComputeTaskDone();
        }
    }

    void* taskVulkanDecode(LdcTask* task, const LdcTaskPart* /*part*/)
    {
        const TaskVulkanDecodeData& data{VNTaskData(task, TaskVulkanDecodeData)};
        PipelineVulkan* const pipeline{data.pipeline};
        FrameVulkan* frame{data.frame};
        const ScopedSubmissionTurn submissionTurn{pipeline, frame};
        VNTraceScopedNamedArgs("vulkan:taskVulkanDecode", "timestamp", frame->timestamp);

        // Skip flushed or skipped frames before admitting them to Vulkan. Once admitted, the task
        // must finish so helper success always means its output was produced.
        if (pipeline->isFlushed(frame) || pipeline->isSkipped(frame)) {
            if (frame->setComputeOutcome(VulkanComputeOutcome::Cancelled)) {
                frame->signalComputeTaskDone();
            }
            return nullptr;
        }

        auto& compute = pipeline->backend().compute();
        VulkanFrameContext* frameContext;
        frameContext =
            VNTraceFuncNamed("vulkan:decode.acquireFrameContext", compute.acquireFrameContext(frame));
        if (!frameContext) {
            VNLogError("Failed to acquire Vulkan frame context");
            if (frame->setComputeOutcome(VulkanComputeOutcome::Failed)) {
                frame->signalComputeTaskDone();
            }
            return nullptr;
        }

        // Convenience values for readability
        const LdeFrameConfig& frameConfig{frame->config};
        const LdeGlobalConfig& globalConfig{*frame->globalConfig};
        const uint8_t numImagePlanes{frame->numImagePlanes()};

        auto intermediatePtr = static_cast<uint8_t>(LOQ2);

        pipeline->setChroma(frame->globalConfig->chroma);

        uint32_t enhancementTileIdx = 0;

        // Tile counts
        uint32_t tileCountTotal{};
        for (int loq = 0; loq < LOQEnhancedCount; ++loq) {
            for (int plane = 0; plane < RCMaxPlanes; ++plane) {
                tileCountTotal += globalConfig.numTiles[plane][loq];
            }
        }

        // GpuCommandBuffer sizes
        const uint32_t gpuCommandBufferSize[LOQEnhancedCount] = {
            frame->getTotalGpuCommandBufferSize(LOQ0),
            frame->getTotalGpuCommandBufferSize(LOQ1),
        };

        // begin vulkan recording state
        VNTraceBeginArgs("vulkan:decode.beginCompute", "tiles", tileCountTotal);
        const bool computeBegun = compute.beginCompute(frameContext);
        VNTraceEnd();
        if (!computeBegun) {
            VNLogError("Failed to begin Vulkan compute");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        // Make sure Apply input buffers have enough space for all tiles
        VNTraceBeginArgs("vulkan:decode.prepareCommandBuffers", "loq0Bytes",
                         gpuCommandBufferSize[LOQ0], "loq1Bytes", gpuCommandBufferSize[LOQ1]);
        const bool commandBuffersPrepared =
            frameContext->prepareCommandBuffer(LOQ1, gpuCommandBufferSize[LOQ1]) &&
            frameContext->prepareCommandBuffer(LOQ0, gpuCommandBufferSize[LOQ0]);
        VNTraceEnd();
        if (!commandBuffersPrepared) {
            VNLogError("Failed to prepare Vulkan apply command buffers");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        if (!vPrepareBaseInput(frameContext)) {
            VNLogError("vPrepareBaseInput failed");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }
        std::array<bool, kLdpPictureMaxNumPlanes> selectedPhysicalPlanes{};
        selectedPhysicalPlanes.fill(true);
        if (!vConvertToInternal(pipeline, frameContext, selectedPhysicalPlanes)) {
            VNLogError("vConvertToInternal failed");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        // First upsample
        if (globalConfig.scalingModes[LOQ1] != Scale0D) {
            if (!vUpscale(pipeline, frameContext, LOQ2, intermediatePtr)) {
                VNLogError("vUpscale failed");
                abortVulkanTask(pipeline, frame, frameContext);
                return nullptr;
            }
            intermediatePtr--;
        }

        // Enhancement LOQ1 decoding
        if (gpuCommandBufferSize[LOQ1] > 0) {
            // Bind descriptor sets once for the LOQ before processing all planes
            LdpEnhancementTile* firstTile = frame->getEnhancementTile(enhancementTileIdx);
            if (!vApplyCommon(pipeline, frameContext, firstTile, intermediatePtr, true)) {
                VNLogError("vApplyCommon failed for LOQ1");
                abortVulkanTask(pipeline, frame, frameContext);
                return nullptr;
            }
            for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
                const bool isEnhanced1 = frame->isPlaneEnhanced(LOQ1, plane);
                if (isEnhanced1 && frameConfig.loqEnabled[LOQ1]) {
                    const uint32_t numTiles = globalConfig.numTiles[plane][LOQ1];
                    for (unsigned tile = 0; tile < numTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->loq == LOQ1 && et->tile == tile);
                        if (!vApplyCmdBufferDirect(pipeline, frameContext, et, intermediatePtr)) {
                            VNLogError("vApplyCmdBufferDirect failed for LOQ1 tile %u", tile);
                            abortVulkanTask(pipeline, frame, frameContext);
                            return nullptr;
                        }
                        ldeCmdBufferGpuFree(&et->bufferGpu, &et->bufferGpuBuilder);
                    }
                }
            }
            // Memory barrier after all LOQ1 apply dispatches
            frameContext->insertComputeBarrier("barrier_after_apply_loq1");
        }

        // Upsample from combined intermediate picture to preliminary output picture
        if (globalConfig.scalingModes[LOQ0] != Scale0D) {
            if (!vUpscale(pipeline, frameContext, LOQ1, intermediatePtr)) {
                VNLogError("vUpscale failed");
                abortVulkanTask(pipeline, frame, frameContext);
                return nullptr;
            }
            intermediatePtr--;
        }

        const bool temporalEnabled = globalConfig.temporalEnabled && !frame->isPassthrough();

        // Bind descriptor sets once for LOQ0 before processing all planes
        if (gpuCommandBufferSize[LOQ0] > 0) {
            const bool applyDirect = !temporalEnabled;
            LdpEnhancementTile* firstTile = frame->getEnhancementTile(enhancementTileIdx);
            if (!vApplyCommon(pipeline, frameContext, firstTile, intermediatePtr, applyDirect)) {
                VNLogError("vApplyCommon failed for LOQ0");
                abortVulkanTask(pipeline, frame, frameContext);
                return nullptr;
            }
        }

        if (temporalEnabled) {
            // Wait for this context's previous temporal work before accessing the shared buffer.
            frameContext->waitEvent();

            for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
                if (plane == 0) {
                    if (frame->config.temporalRefresh && pipeline->m_temporalPicture->buffer) {
                        auto* temporalBuffer = fromPipeline(pipeline->m_temporalPicture->buffer);
                        if (temporalBuffer->usage() == pipeline::BufferUsageInternal) {
                            temporalBuffer->clearCmd(frameContext->getCommandBuffer());
                        } else {
                            temporalBuffer->clear();
                        }
                        pipeline->m_temporalPicture->temporalBufferEmpty = true;
                    }
                }

                const bool isEnhanced0 = frame->isPlaneEnhanced(LOQ0, plane);
                if (isEnhanced0 && frameConfig.loqEnabled[LOQ0]) {
                    const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                    for (unsigned tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->loq == LOQ0 && et->tile == tile);
                        if (et->bufferGpu.commandCount != 0) {
                            pipeline->m_temporalPicture->temporalBufferEmpty = false;
                        }
                        if (!vApplyCmdBufferTemporal(pipeline, frameContext, et, intermediatePtr)) {
                            VNLogError("vApplyCmdBufferTemporal failed for LOQ0 tile %u", tile);
                            abortVulkanTask(pipeline, frame, frameContext);
                            return nullptr;
                        }
                        ldeCmdBufferGpuFree(&et->bufferGpu, &et->bufferGpuBuilder);
                    }
                }
            }

            if (gpuCommandBufferSize[LOQ0] > 0) {
                frameContext->insertComputeBarrier("barrier_after_apply_loq0_temporal");
            }

        } else {
            for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
                const bool isEnhanced0 = frame->isPlaneEnhanced(LOQ0, plane);
                if (isEnhanced0 && frameConfig.loqEnabled[LOQ0]) {
                    const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                    for (unsigned tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->loq == LOQ0 && et->tile == tile);
                        if (!vApplyCmdBufferDirect(pipeline, frameContext, et, intermediatePtr)) {
                            VNLogError("vApplyCmdBufferDirect failed for LOQ0 tile %u", tile);
                            abortVulkanTask(pipeline, frame, frameContext);
                            return nullptr;
                        }
                        ldeCmdBufferGpuFree(&et->bufferGpu, &et->bufferGpuBuilder);
                    }
                }
            }

            // Memory barrier after all LOQ0 direct apply dispatches
            if (gpuCommandBufferSize[LOQ0] > 0) {
                frameContext->insertComputeBarrier("barrier_after_apply_loq0_direct");
            }
        }

        assert(enhancementTileIdx == frame->enhancementTileCount);

        if (!vConvertFromInternal(pipeline, frameContext, frame->getEnhancementBitDepth(), intermediatePtr)) {
            VNLogError("vConvertFromInternal failed");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        // Fused convert-out reads the temporal buffer, so signal only after that read is complete.
        if (temporalEnabled) {
            frameContext->setEvent();
        }

        // end Vulkan recording state
        const bool computeEnded =
            VNTraceFuncNamed("vulkan:decode.submit", compute.endCompute(frameContext));
        if (!computeEnded) {
            VNLogError("Failed to end Vulkan compute");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        pipeline->addCompletionContext(frameContext);

        return nullptr;
    }

    void addTaskVulkanDecode(PipelineVulkan* pipeline, FrameVulkan* frame,
                             const LdcTaskDependency* inputs, uint32_t inputsCount)
    {
        const TaskVulkanDecodeData data{pipeline, frame};

        frame->taskAddSink(inputs, inputsCount, taskVulkanDecode, &data, sizeof(data), "VulkanDecode");
    }

    // Unscaled passthrough converts in and out on the GPU without upscale or apply.
    // Scaled passthrough uses the enhancement task graph.
    void* taskVulkanPassthrough(LdcTask* task, const LdcTaskPart* /*part*/)
    {
        VNTraceScopedNamed("vulkan:taskVulkanPassthrough");
        assert(task->dataSize == sizeof(TaskVulkanDecodeData));
        const TaskVulkanDecodeData& data{VNTaskData(task, TaskVulkanDecodeData)};
        PipelineVulkan* const pipeline{data.pipeline};
        FrameVulkan* const frame{data.frame};
        const ScopedSubmissionTurn submissionTurn{pipeline, frame};

        if (pipeline->isFlushed(frame) || pipeline->isSkipped(frame)) {
            if (frame->setComputeOutcome(VulkanComputeOutcome::Cancelled)) {
                frame->signalComputeTaskDone();
            }
            return nullptr;
        }
        if (frame->basePicture == nullptr || frame->outputPicture == nullptr) {
            VNLogError("Missing picture for Vulkan passthrough");
            if (frame->setComputeOutcome(VulkanComputeOutcome::Failed)) {
                frame->signalComputeTaskDone();
            }
            return nullptr;
        }

        auto& compute = pipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(frame);
        if (!frameContext) {
            VNLogError("Failed to acquire Vulkan frame context (passthrough)");
            if (frame->setComputeOutcome(VulkanComputeOutcome::Failed)) {
                frame->signalComputeTaskDone();
            }
            return nullptr;
        }

        // Derive chroma from the base picture, since globalConfig may be null.
        pipeline->setChroma(ldpPictureLayoutChroma(&frame->basePicture->layout));

        // No enhancement tiles in passthrough.
        if (!compute.beginCompute(frameContext)) {
            VNLogError("Failed to begin Vulkan compute (passthrough)");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        if (!vPrepareBaseInput(frameContext)) {
            VNLogError("vPrepareBaseInput failed");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }
        std::array<bool, kLdpPictureMaxNumPlanes> selectedPhysicalPlanes{};
        selectedPhysicalPlanes.fill(true);
        if (!vConvertToInternal(pipeline, frameContext, selectedPhysicalPlanes)) {
            VNLogError("vConvertToInternal failed");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }
        if (!vConvertFromInternal(pipeline, frameContext, frame->baseBitdepth, static_cast<uint8_t>(LOQ2))) {
            VNLogError("vConvertFromInternal failed");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }

        if (!compute.endCompute(frameContext)) {
            VNLogError("Failed to end Vulkan compute (passthrough)");
            abortVulkanTask(pipeline, frame, frameContext);
            return nullptr;
        }
        pipeline->addCompletionContext(frameContext);

        return nullptr;
    }

    void addTaskVulkanPassthrough(PipelineVulkan* pipeline, FrameVulkan* frame,
                                  const LdcTaskDependency* inputs, uint32_t inputsCount)
    {
        const TaskVulkanDecodeData data{pipeline, frame};
        frame->taskAddSink(inputs, inputsCount, taskVulkanPassthrough, &data, sizeof(data),
                           "VulkanPassthrough");
    }

    void generateTasksEnhancement(PipelineVulkan* pipeline, FrameVulkan* frame, uint64_t previousTimestamp)
    {
        const uint32_t numTiles = frame->enhancementTileCount;

        VNTraceScopedNamedArgs("vulkan:generateTasksEnhancement", "timestamp", frame->timestamp,
                               "previousTimestamp", previousTimestamp);

        // The compute-finish thread marks this dependency when Vulkan work completes.
        LdcTaskDependency computeDone{frame->taskDependencyAdd()};
        frame->setComputeTaskDone(computeDone);

        // Prepare commands in parallel, but record and submit in frame task-generation order.
        const uint32_t numDeps = 3 + numTiles;
        LdcTaskDependency* deps =
            static_cast<LdcTaskDependency*>(alloca(numDeps * sizeof(LdcTaskDependency)));

        deps[0] = frame->depBasePicture();
        deps[1] = frame->depOutputPicture();
        deps[2] = pipeline->registerSubmissionTurn(frame);

        // Create a separate generateCmdBuffer task for each enhancement tile.
        // These have no input dependencies and can run concurrently.
        for (uint32_t i = 0; i < numTiles; ++i) {
            LdpEnhancementTile* et = frame->getEnhancementTile(i);
            deps[3 + i] = addTaskGenerateCmdBufferGPU(pipeline, frame, et);
        }

        // The decode task depends on all generateCmdBuffer tasks completing
        addTaskVulkanDecode(pipeline, frame, deps, numDeps);

        // Send output and base when compute finishes
        pipeline::addTaskOutputDone(pipeline, frame, &computeDone, 1);
        pipeline::addTaskBaseDone(pipeline, frame, &computeDone, 1);
    }

    // Create a GPU task for unscaled passthrough; CPU tasks cannot access Vulkan pictures.
    void generateTasksPassthrough(PipelineVulkan* pipeline, FrameVulkan* frame)
    {
        VNTraceScopedNamedArgs("vulkan:generateTasksPassthrough", "timestamp", frame->timestamp);

        // Passthrough participates in the same submission order as enhanced frames.
        // Compute completion will signal computeTaskDone, which OutputDone /
        // BaseDone depend on (mirroring the enhancement path).
        LdcTaskDependency computeDone{frame->taskDependencyAdd()};
        frame->setComputeTaskDone(computeDone);

        const LdcTaskDependency inputs[] = {frame->depBasePicture(), frame->depOutputPicture(),
                                            pipeline->registerSubmissionTurn(frame)};
        addTaskVulkanPassthrough(pipeline, frame, inputs, static_cast<uint32_t>(VNArraySize(inputs)));

        pipeline::addTaskOutputDone(pipeline, frame, &computeDone, 1);
        pipeline::addTaskBaseDone(pipeline, frame, &computeDone, 1);
    }

} // anonymous namespace

void generateTasks(PipelineVulkan* pipeline, FrameVulkan* frame, uint64_t previousTimestamp)
{
    // Scaled passthrough uses the enhancement graph to run upscale without apply.
    if (frame->isPassthrough() &&
        (pipeline->configuration().passthroughMode != pipeline::PassthroughMode::Scale ||
         !frame->hasGoodConfig())) {
        generateTasksPassthrough(pipeline, frame);
    } else if (pipeline->isFlushed(frame)) {
        generateTasksPassthrough(pipeline, frame);
    } else {
        generateTasksEnhancement(pipeline, frame, previousTimestamp);
    }

    if (pipeline->configuration().showTasks) {
#if VN_SDK_LOG(DEBUG)
        ldcTaskPoolDump(pipeline->taskPool(), frame->taskGroup());
#endif
        ldcTaskGroupUnblock(frame->taskGroup());
    }
}

} // namespace lcevc_dec::pipeline_vulkan
