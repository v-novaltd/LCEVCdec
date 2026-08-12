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

#include "buffer_vulkan.h"
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

// All tasks fns. are static - only exported function is generateTasks()
//
namespace {

    //// ConvertToInternal
    //
    // Copy incoming picture plane to internal fixed point surface format
    //
    void* vConvertToInternal(PipelineVulkan* pipeline, VulkanFrameContext* context,
                             uint32_t baseDepth, uint32_t enhancementDepth)
    {
        if (pipeline->isSkipped(context->getFrame())) {
            return nullptr;
        }

        auto* srcPicture = fromPipeline(context->getFrame()->basePicture);
        if (!srcPicture) {
            return nullptr;
        }
        // external base check
        LdpPictureBufferDesc exDesc{};
        const bool hasBufferDesc = srcPicture->getBufferDesc(exDesc);
        LdpPicturePlaneDesc srcPlaneDescs[kLdpPictureMaxNumPlanes]{};
        if (srcPicture->getPlaneDescArr(srcPlaneDescs)) {
            if (srcPicture->buffer == nullptr) {
                srcPicture->bindBuffer(pipeline::BufferUsageIn);
            }

            auto* managedBuffer = fromPipeline(srcPicture->buffer);
            assert(managedBuffer);

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

            if (!needsCompaction && hasBufferDesc && exDesc.byteSize == compactSize) {
                managedBuffer->copyIn(0, exDesc.data, exDesc.byteSize);
            } else {
                uint32_t compactOffset = 0;
                for (uint8_t plane = 0; plane < planeCount; ++plane) {
                    const uint32_t rowBytes = ldpPictureLayoutRowSize(&srcPicture->layout, plane);
                    const uint32_t planeHeight = ldpPictureLayoutPlaneHeight(&srcPicture->layout, plane);

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

        LdpPictureDesc srcDesc;
        srcPicture->getDesc(srcDesc);
        srcDesc.colorFormat = pipeline->chromaToColorFormat(pipeline->getChroma());
        LdpPicture* dstLdp = nullptr;
        if (pipeline->allocPicture(srcDesc, dstLdp) != LdcReturnCodeSuccess || !dstLdp) {
            VNLogError("allocPicture failed for ConvertToInternal");
            return nullptr;
        }
        auto* dstPicture = fromPipeline(dstLdp);
        dstPicture->bindBuffer(pipeline::BufferUsageInternal);
        context->getFrame()->addIntermediatePicture(dstPicture);

        VulkanConvertInArgs args{};
        args.src = srcPicture;
        args.dst = dstPicture;
        args.bitDepth = context->getFrame()->baseBitdepth;
        args.chroma = pipeline->getChroma();
        args.context = context;

        if (!pipeline->backend().convertIn(&args)) {
            VNLogError("Conversion to internal failed");
        }

        context->getFrame()->setIntermediatePicture(LOQ2, dstPicture);

        return nullptr;
    }

    //// ConvertFromInternal
    //
    // Concert a picture plane from internal fixed point to output picture pixel format.
    //
    void* vConvertFromInternal(PipelineVulkan* pipeline, VulkanFrameContext* context,
                               uint32_t bitDepth, uint8_t intermediatePtr)
    {
        if (pipeline->isSkipped(context->getFrame())) {
            return nullptr;
        }

        auto* srcPicture = context->getFrame()->getIntermediatePicture(intermediatePtr);

        LdpPictureDesc dstDesc;
        srcPicture->getDesc(dstDesc);
        dstDesc.colorFormat = context->getFrame()->outputPicture->layout.layoutInfo->format;
        PictureVulkan* op = fromPipeline(context->getFrame()->outputPicture);
        op->setDescAndBind(dstDesc, pipeline::BufferUsageOut);

        // sharpeningStrength() and the crop check both touch globalConfig,
        // which is null in the unscaled-passthrough path.
        const auto* gc = context->getFrame()->globalConfig;
        VulkanConvertOutArgs args{};
        args.src = srcPicture;
        args.dst = fromPipeline(context->getFrame()->outputPicture);
        args.bitDepth = bitDepth;
        args.chroma = pipeline->getChroma();
        args.sharpenStrength = gc ? context->getFrame()->sharpeningStrength(0) : 0.0f;
        args.temporal = pipeline->m_temporalPicture;
        args.numEnhancedPlanes = context->getFrame()->numEnhancedPlanes();
        args.addTemporal = gc && gc->temporalEnabled && !context->getFrame()->isPassthrough() &&
                           pipeline->m_temporalPicture && pipeline->m_temporalPicture->buffer != nullptr;
        args.context = context;

        if (!pipeline->backend().convertOut(&args)) {
            VNLogError("Conversion from internal failed");
        }

        // Crop only applies when there's an LCEVC global configuration; the
        // unscaled-passthrough path runs without one.
        if (gc && gc->cropEnabled) {
            args.dst->margins.left = gc->crop.left;
            args.dst->margins.right = gc->crop.right;
            args.dst->margins.top = gc->crop.top;
            args.dst->margins.bottom = gc->crop.bottom;
        }

        return nullptr;
    }

    //// Upsample
    //
    // Upscale (1D or 2D) for one plane of picture.
    //
    // Inputs and outputs may be fixed point or 'external' format if no residuals are being applied.
    //

    void* vUpscale(PipelineVulkan* pipeline, VulkanFrameContext* context, LdeLOQIndex loq,
                   uint8_t intermediatePtr)
    {
        if (pipeline->isSkipped(context->getFrame())) {
            return nullptr;
        }

        VulkanUpscaleArgs upscaleArgs{};
        upscaleArgs.src = context->getFrame()->getIntermediatePicture(intermediatePtr);
        LdpPictureDesc desc{};
        ldpDefaultPictureDesc(&desc, LdpColorFormatI420_8, 2, 2);

        LdpPicture* ipLdp = nullptr;
        if (pipeline->allocPicture(desc, ipLdp) != LdcReturnCodeSuccess || !ipLdp) {
            VNLogError("allocPicture failed for Upscale");
            return nullptr;
        }
        PictureVulkan* ip = fromPipeline(ipLdp);
        ip->bindBuffer(pipeline::BufferUsageInternal);
        context->getFrame()->setIntermediatePicture(intermediatePtr - 1, ip);

        upscaleArgs.dst = context->getFrame()->getIntermediatePicture(intermediatePtr - 1);
        context->getFrame()->addIntermediatePicture(
            context->getFrame()->getIntermediatePicture(intermediatePtr - 1));

        upscaleArgs.applyPA =
            static_cast<uint8_t>(context->getFrame()->globalConfig->predictedAverageEnabled);
        upscaleArgs.dither = nullptr; // TODO pipeline->m_dither;
        upscaleArgs.mode = context->getFrame()->globalConfig->scalingModes[loq - 1];
        upscaleArgs.vertical = false;
        upscaleArgs.loq1 = (loq == 2) ? true : false;
        upscaleArgs.intermediateUpscalePicture[0] = pipeline->m_intermediateUpscalePicture[LOQ0];
        upscaleArgs.intermediateUpscalePicture[1] = pipeline->m_intermediateUpscalePicture[LOQ1];
        upscaleArgs.chroma = pipeline->getChroma();
        upscaleArgs.pipeline = pipeline;
        upscaleArgs.context = context;

        assert(upscaleArgs.mode != Scale0D);
        VNLogDebug("taskUpsample timestamp:%" PRIx64 " loq:%d", context->getFrame()->timestamp,
                   (uint32_t)loq);

        if (!pipeline->backend().upscaleFrame(&context->getFrame()->globalConfig->kernel, &upscaleArgs)) {
            VNLogError("Upsample failed");
        }

        return nullptr;
    }

    //// ApplyCmdBufferDirect
    //
    // Apply a generated GPU command buffer to directly to output plane. (No Temporal)
    //
    // NB: The output plane will be in 'internal' fixed' point format
    //
    void vApplyCommon(PipelineVulkan* pipeline, VulkanFrameContext* context,
                      LdpEnhancementTile* enhancementTile, uint8_t intermediatePtr, bool applyDirect)
    {
        auto* picture = context->getFrame()->getIntermediatePicture(intermediatePtr);

        VulkanApplyCommonArgs args{};
        pipeline->prepareApplyCommonArgs(args, picture, enhancementTile, context, applyDirect);
        if (!applyDirect) {
            args.temporalPicture = pipeline->m_temporalPicture;
        }

        if (!pipeline->backend().applyCommon(&args)) {
            VNLogError("Vulkan applyCommon failed");
        }
    }

    void* vApplyCmdBufferDirect(PipelineVulkan* pipeline, VulkanFrameContext* context,
                                LdpEnhancementTile* enhancementTile, uint8_t intermediatePtr)
    {
        if (pipeline->isSkipped(context->getFrame())) {
            return nullptr;
        }

        VNLogDebug("taskApplyCmdBufferDirect timestamp:%" PRIx64 " loq:%d plane:%d",
                   context->getFrame()->timestamp, (uint32_t)enhancementTile->loq, enhancementTile->plane);

        auto* picture = context->getFrame()->getIntermediatePicture(intermediatePtr);

        VulkanApplyTileArgs args{};
        pipeline->prepareApplyTileArgs(args, picture, enhancementTile, context, true);

        if (!pipeline->backend().applyTile(&args)) {
            VNLogError("Vulkan apply direct failed");
        }

        return nullptr;
    }

    //// ApplyCmdBufferTemporal
    //
    // Apply a generated GPU command buffer to a temporal buffer.
    //

    void* vApplyCmdBufferTemporal(PipelineVulkan* pipeline, VulkanFrameContext* context,
                                  LdpEnhancementTile* enhancementTile, uint8_t intermediatePtr)
    {
        VNLogDebug("taskApplyCmdBufferTemporal timestamp:%" PRIx64 " tile:%d loq:%d plane:%d",
                   context->getFrame()->timestamp, enhancementTile->tile,
                   (uint32_t)enhancementTile->loq, enhancementTile->plane);

        auto* picture = context->getFrame()->getIntermediatePicture(intermediatePtr);

        VulkanApplyTileArgs args{};
        pipeline->prepareApplyTileArgs(args, picture, enhancementTile, context, false);
        args.temporalPicture = pipeline->m_temporalPicture;

        if (!pipeline->backend().applyTile(&args)) {
            VNLogError("Vulkan apply temporal failed");
        }

        return nullptr;
    }

    //// VulkanDecode
    //
    // Decode the frame
    //
    struct TaskVulkanDecodeData
    {
        PipelineVulkan* pipeline;
        FrameVulkan* frame;
        uint64_t previousTimestamp;
    };

    void* taskVulkanDecode(LdcTask* task, const LdcTaskPart* /*part*/)
    {
        VNTraceScoped();

        const TaskVulkanDecodeData& data{VNTaskData(task, TaskVulkanDecodeData)};
        PipelineVulkan* const pipeline{data.pipeline};
        FrameVulkan* frame{data.frame};

        // Skip flushed or skipped frames — their base/output pictures and
        // globalConfig may be null.  Signal computeTaskDone so downstream
        // tasks (OutputDone, BaseDone) can still complete during flush.
        if (pipeline->isFlushed(frame) || pipeline->isSkipped(frame)) {
            frame->signalComputeTaskDone();
            VNTraceScopedEnd();
            return nullptr;
        }

        auto& compute = pipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(frame);
        if (!frameContext) {
            VNLogError("Failed to acquire Vulkan frame context");
            VNTraceScopedEnd();
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
        pipeline->backend().compute().beginCompute(frameContext, tileCountTotal);

        // Make sure Apply input buffers have enough space for all tiles
        frameContext->prepareCommandBuffer(LOQ1, gpuCommandBufferSize[LOQ1]);
        frameContext->prepareCommandBuffer(LOQ0, gpuCommandBufferSize[LOQ0]);

        //// Input conversion
        vConvertToInternal(pipeline, frameContext, frame->getEnhancementBitDepth(), globalConfig.baseDepth);

        //// LoQ 1

        // First upsample
        if (globalConfig.scalingModes[LOQ1] != Scale0D) {
            vUpscale(pipeline, frameContext, LOQ2, intermediatePtr);
            intermediatePtr--;
        }

        // Enhancement LOQ1 decoding
        if (gpuCommandBufferSize[LOQ1] > 0) {
            // Bind descriptor sets once for the LOQ before processing all planes
            LdpEnhancementTile* firstTile = frame->getEnhancementTile(enhancementTileIdx);
            vApplyCommon(pipeline, frameContext, firstTile, intermediatePtr, true);
            for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
                const bool isEnhanced1 = frame->isPlaneEnhanced(LOQ1, plane);
                if (isEnhanced1 && frameConfig.loqEnabled[LOQ1]) {
                    const uint32_t numTiles = globalConfig.numTiles[plane][LOQ1];
                    for (unsigned tile = 0; tile < numTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->loq == LOQ1 && et->tile == tile);
                        vApplyCmdBufferDirect(pipeline, frameContext, et, intermediatePtr);
                        ldeCmdBufferGpuFree(&et->bufferGpu, &et->bufferGpuBuilder);
                    }
                }
            }
            // Memory barrier after all LOQ1 apply dispatches
            frameContext->insertComputeBarrier();
        }

        // Upsample from combined intermediate picture to preliminary output picture
        if (globalConfig.scalingModes[LOQ0] != Scale0D) {
            vUpscale(pipeline, frameContext, LOQ1, intermediatePtr);
            intermediatePtr--;
        }

        //// LoQ 0

        // Bind descriptor sets once for LOQ0 before processing all planes
        if (gpuCommandBufferSize[LOQ0] > 0) {
            const bool applyDirect = !(globalConfig.temporalEnabled && !frame->isPassthrough());
            LdpEnhancementTile* firstTile = frame->getEnhancementTile(enhancementTileIdx);
            vApplyCommon(pipeline, frameContext, firstTile, intermediatePtr, applyDirect);
        }

        if (globalConfig.temporalEnabled && !frame->isPassthrough()) {
            // Temporal path: wait for previous frame's temporal writes before first tile
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
                    }
                }

                const bool isEnhanced0 = frame->isPlaneEnhanced(LOQ0, plane);
                if (isEnhanced0 && frameConfig.loqEnabled[LOQ0]) {
                    const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                    for (unsigned tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->loq == LOQ0 && et->tile == tile);
                        vApplyCmdBufferTemporal(pipeline, frameContext, et, intermediatePtr);
                        ldeCmdBufferGpuFree(&et->bufferGpu, &et->bufferGpuBuilder);
                    }
                }
            }

            if (gpuCommandBufferSize[LOQ0] > 0) {
                frameContext->insertComputeBarrier();
            }

            // Signal that this frame's temporal writes are complete
            frameContext->setEvent();

        } else {
            for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
                const bool isEnhanced0 = frame->isPlaneEnhanced(LOQ0, plane);
                if (isEnhanced0 && frameConfig.loqEnabled[LOQ0]) {
                    const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                    for (unsigned tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->loq == LOQ0 && et->tile == tile);
                        vApplyCmdBufferDirect(pipeline, frameContext, et, intermediatePtr);
                        ldeCmdBufferGpuFree(&et->bufferGpu, &et->bufferGpuBuilder);
                    }
                }
            }

            // Memory barrier after all LOQ0 direct apply dispatches
            if (gpuCommandBufferSize[LOQ0] > 0) {
                frameContext->insertComputeBarrier();
            }
        }

        assert(enhancementTileIdx == frame->enhancementTileCount);

        vConvertFromInternal(pipeline, frameContext, frame->getEnhancementBitDepth(), intermediatePtr);

        // end Vulkan recording state
        pipeline->backend().compute().endCompute(frameContext);

        pipeline->addCompletionContext(frameContext);

        return nullptr;
    }

    void addTaskVulkanDecode(PipelineVulkan* pipeline, FrameVulkan* frame,
                             const LdcTaskDependency* inputs, uint32_t inputsCount)
    {
        const TaskVulkanDecodeData data{pipeline, frame};

        frame->taskAddSink(inputs, inputsCount, taskVulkanDecode, &data, sizeof(data), "VulkanDecode");
    }

    //// VulkanPassthrough
    //
    // Vulkan-native unscaled passthrough: convertIn -> convertOut on the GPU,
    // skipping upscale/apply.  Used when there's no LCEVC global config (e.g.
    // VVC-only stream) or pass-through is forced unscaled.  The scaled
    // pass-through case is handled by the enhancement task graph already
    // (skipped tiles -> upscale-only).
    //
    void* taskVulkanPassthrough(LdcTask* task, const LdcTaskPart* /*part*/)
    {
        assert(task->dataSize == sizeof(TaskVulkanDecodeData));
        const TaskVulkanDecodeData& data{VNTaskData(task, TaskVulkanDecodeData)};
        PipelineVulkan* const pipeline{data.pipeline};
        FrameVulkan* const frame{data.frame};

        if (pipeline->isFlushed(frame) || pipeline->isSkipped(frame) ||
            frame->basePicture == nullptr || frame->outputPicture == nullptr) {
            frame->signalComputeTaskDone();
            return nullptr;
        }

        auto& compute = pipeline->backend().compute();
        VulkanFrameContext* frameContext = compute.acquireFrameContext(frame);
        if (!frameContext) {
            VNLogError("Failed to acquire Vulkan frame context (passthrough)");
            frame->signalComputeTaskDone();
            return nullptr;
        }

        // Derive chroma from the base picture, since globalConfig may be null.
        pipeline->setChroma(ldpPictureLayoutChroma(&frame->basePicture->layout));

        // No enhancement tiles in passthrough.
        compute.beginCompute(frameContext, /*tileCountTotal=*/0);

        vConvertToInternal(pipeline, frameContext, frame->baseBitdepth, frame->baseBitdepth);
        vConvertFromInternal(pipeline, frameContext, frame->baseBitdepth, static_cast<uint8_t>(LOQ2));

        compute.endCompute(frameContext);
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

        VNTraceScopedArgs("timestamp", frame->timestamp, "previousTimestamp", previousTimestamp);

        // The event from the vulkan compute work finishing will mark this dependpecy as met via the
        // pipeline's computeFinish thread
        LdcTaskDependency computeDone{frame->taskDependencyAdd()};
        frame->setComputeTaskDone(computeDone);

        // Dependencies: base picture + output picture + one generateCmdBuffer task per tile
        const uint32_t numDeps = 2 + numTiles;
        LdcTaskDependency* deps =
            static_cast<LdcTaskDependency*>(alloca(numDeps * sizeof(LdcTaskDependency)));

        deps[0] = frame->depBasePicture();
        deps[1] = frame->depOutputPicture();

        // Create a separate generateCmdBuffer task for each enhancement tile.
        // These have no input dependencies and can run concurrently.
        for (uint32_t i = 0; i < numTiles; ++i) {
            LdpEnhancementTile* et = frame->getEnhancementTile(i);
            deps[2 + i] = addTaskGenerateCmdBufferGPU(pipeline, frame, et);
        }

        // The decode task depends on all generateCmdBuffer tasks completing
        addTaskVulkanDecode(pipeline, frame, deps, numDeps);

        // Send output and base when compute finishes
        pipeline::addTaskOutputDone(pipeline, frame, &computeDone, 1);
        pipeline::addTaskBaseDone(pipeline, frame, &computeDone, 1);
    }

    // Fill out a task group for a simple unscaled pass-through configuration.
    //
    // Runs a single Vulkan-native task that does convertIn -> convertOut on
    // the GPU.  The previous implementation used the generic per-plane CPU
    // passthrough task, which can't read Vulkan-resident base pictures or
    // write to AHB-imported output images.
    //
    void generateTasksPassthrough(PipelineVulkan* pipeline, FrameVulkan* frame)
    {
        VNTraceScopedArgs("timestamp", frame->timestamp);

        // Wait for both base and output pictures, then run the GPU passthrough.
        // Compute completion will signal computeTaskDone, which OutputDone /
        // BaseDone depend on (mirroring the enhancement path).
        LdcTaskDependency computeDone{frame->taskDependencyAdd()};
        frame->setComputeTaskDone(computeDone);

        const LdcTaskDependency inputs[] = {frame->depBasePicture(), frame->depOutputPicture()};
        addTaskVulkanPassthrough(pipeline, frame, inputs, static_cast<uint32_t>(VNArraySize(inputs)));

        pipeline::addTaskOutputDone(pipeline, frame, &computeDone, 1);
        pipeline::addTaskBaseDone(pipeline, frame, &computeDone, 1);
    }

} // anonymous namespace

// Generate task graph for a frame
//
void generateTasks(PipelineVulkan* pipeline, FrameVulkan* frame, uint64_t previousTimestamp)
{
    // Choose pass-through or enhancement task graph generation.
    //
    // If the pass through is 'Scaled', then use the enhancement graph, which
    // will just end up doing scaling as there is no enhancement data.
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
