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

#include "tasks_cpu.h"
//
#include "frame_cpu.h"
#include "pipeline_cpu.h"

#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/tasks_base.h>

namespace lcevc_dec::pipeline_cpu {

namespace {

    // Fill out a task group given a frame configuration
    //
    void generateTasksEnhancement(PipelineCPU* pipeline, FrameCPU* frame, uint64_t previousTimestamp)
    {
        VNTraceScopedArgs("timestamp", frame->timestamp, "previousTimestamp", previousTimestamp);

        // Convenience values for readability
        const LdeFrameConfig& frameConfig{frame->config};
        const LdeGlobalConfig& globalConfig{*frame->globalConfig};
        const uint8_t numImagePlanes{
            std::min(frame->numImagePlanes(), static_cast<uint8_t>(kLdpPictureMaxNumPlanes))};
        const uint8_t numEnhancedPlanes =
            std::min(globalConfig.numPlanes, static_cast<uint8_t>(RCMaxPlanes));
        const LdeScalingMode scalingModes[LOQEnhancedCount] = {globalConfig.scalingModes[LOQ0],
                                                               globalConfig.scalingModes[LOQ1]};
        const bool sharpeningEnabled = frame->sharpeningEnabled();

        uint32_t enhancementTileIdx = 0;

        bool directUpscale[kLdpPictureMaxNumPlanes] = {};
        for (uint8_t plane = 0; plane < numEnhancedPlanes; ++plane) {
            directUpscale[plane] = pipeline->directUpscaleEligible(*frame, plane);
        }

        //// LoQ 1
        //
        LdcTaskDependency basePlanes[kLdpPictureMaxNumPlanes] = {};

        // Input plane dependencies - will be filled in via sendBase()
        for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
            basePlanes[plane] = frame->depBasePicture();
        }

        // Upscale and residuals
        for (uint8_t plane = 0; plane < numEnhancedPlanes; ++plane) {
            const bool planeEnhanced = frame->isPlaneEnhanced(LOQ1, plane);

            if (directUpscale[plane]) {
                if (planeEnhanced) {
                    enhancementTileIdx += globalConfig.numTiles[plane][LOQ1];
                }
                continue;
            }

            //// Input conversion
            //
            LdcTaskDependency basePlane{};

            // Convert between base and enhancement bit depth
            basePlane = addTaskConvertToInternal(pipeline, frame, plane, globalConfig.baseDepth,
                                                 globalConfig.enhancedDepth, basePlanes[plane]);

            //// Base + Residuals
            //
            // First upscale
            LdcTaskDependency baseUpscaled{kTaskDependencyInvalid};
            if (scalingModes[LOQ1] != Scale0D) {
                baseUpscaled = addTaskUpscale(pipeline, frame, LOQ2, plane, basePlane);
            } else {
                baseUpscaled = basePlane;
            }

            // Enhancement LOQ1 decoding
            if (planeEnhanced) {
                const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ1];
                if (numPlaneTiles > 1) {
                    LdcTaskDependency* tiles = static_cast<LdcTaskDependency*>(
                        alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                    // Generate and apply each tile's command buffer
                    for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->plane == plane && et->loq == LOQ1 && et->tile == tile);

                        LdcTaskDependency commands = addTaskGenerateCmdBufferCPU(pipeline, frame, et);
                        tiles[tile] =
                            addTaskApplyCmdBufferDirect(pipeline, frame, et, baseUpscaled, commands);
                    }
                    // Wait for all tiles to finish
                    basePlanes[plane] = addTaskWaitForMany(pipeline, frame, tiles, numPlaneTiles);
                } else {
                    LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                    assert(et->plane == plane && et->loq == LOQ1 && et->tile == 0);

                    LdcTaskDependency commands = addTaskGenerateCmdBufferCPU(pipeline, frame, et);
                    basePlanes[plane] =
                        addTaskApplyCmdBufferDirect(pipeline, frame, et, baseUpscaled, commands);
                }
            } else {
                basePlanes[plane] = baseUpscaled;
            }
        }

        // Upscale from combined intermediate picture to preliminary output picture
        LdcTaskDependency upscaledPlanes[kLdpPictureMaxNumPlanes] = {};
        LdcTaskDependency outputPlanes[kLdpPictureMaxNumPlanes] = {};

        for (uint8_t plane = 0; plane < numEnhancedPlanes; ++plane) {
            if (directUpscale[plane]) {
                const LdeLOQIndex fromLoq = scalingModes[LOQ0] != Scale0D ? LOQ1 : LOQ2;
                outputPlanes[plane] = addTaskUpscaleDirect(
                    pipeline, frame, fromLoq, plane, basePlanes[plane], frame->depOutputPicture());
                if (plane == 0 && sharpeningEnabled) {
                    outputPlanes[plane] = addTaskSharpen(pipeline, frame, plane, outputPlanes[plane]);
                }
                continue;
            }
            if (scalingModes[LOQ0] != Scale0D) {
                upscaledPlanes[plane] = addTaskUpscale(pipeline, frame, LOQ1, plane, basePlanes[plane]);
            } else {
                upscaledPlanes[plane] = basePlanes[plane];
            }
        }

        // Upscaling and passthrough for planes with no residuals
        for (uint8_t plane = numEnhancedPlanes; plane < numImagePlanes; ++plane) {
            if (scalingModes[LOQ0] == Scale0D && scalingModes[LOQ1] == Scale0D) {
                // 0D 0D - no upscaling, passthrough chroma planes to output
                outputPlanes[plane] = addTaskPassthrough(pipeline, frame, plane,
                                                         frame->depOutputPicture(), basePlanes[plane]);
            } else if ((scalingModes[LOQ0] != Scale0D && scalingModes[LOQ1] != Scale0D) ||
                       globalConfig.baseDepth != globalConfig.enhancedDepth) {
                // Double upscale or bitdepth conversion - convert to internal, run both upscales in
                // S16 and convert back to output
                LdcTaskDependency baseConverted =
                    addTaskConvertToInternal(pipeline, frame, plane, globalConfig.baseDepth,
                                             globalConfig.enhancedDepth, basePlanes[plane]);
                LdcTaskDependency upscaledLOQ1 = baseConverted;
                LdcTaskDependency upscaledLOQ0 = upscaledLOQ1;
                if (scalingModes[LOQ1] != Scale0D) {
                    upscaledLOQ1 = addTaskUpscale(pipeline, frame, LOQ2, plane, baseConverted);
                }
                if (scalingModes[LOQ0] != Scale0D) {
                    upscaledLOQ0 = addTaskUpscale(pipeline, frame, LOQ1, plane, upscaledLOQ1);
                }
                outputPlanes[plane] = addTaskConvertFromInternal(
                    pipeline, frame, plane, frame->depOutputPicture(), upscaledLOQ0);
            } else {
                // Only one LOQ requires upscaling (most common case). Limit base planes for NV12
                const LdeLOQIndex fromLoq = (scalingModes[LOQ0] != Scale0D) ? LOQ1 : LOQ2;
                outputPlanes[plane] = addTaskUpscaleDirect(
                    pipeline, frame, fromLoq, plane, basePlanes[plane], frame->depOutputPicture());
            }
        }

        //// LoQ 0
        //
        for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
            const bool planeEnhanced = frame->isPlaneEnhanced(LOQ0, plane);

            if (directUpscale[plane]) {
                if (planeEnhanced) {
                    enhancementTileIdx += globalConfig.numTiles[plane][LOQ0];
                }
                if (globalConfig.temporalEnabled) {
                    const LdcTaskDependency temporal =
                        frame->needsTemporalBuffer(previousTimestamp, plane);
                    pipeline->findTemporalBuffer(frame, plane);
                    // Preserve temporal clear/transfer even though no residual commands are needed.
                    const LdcTaskDependency deps[] = {temporal, outputPlanes[plane]};
                    const LdcTaskDependency ready = addTaskWaitForMany(pipeline, frame, deps, 2);
                    addTaskTemporalTransfer(pipeline, frame, ready, plane);
                    outputPlanes[plane] = ready;
                }
                continue;
            }

            LdcTaskDependency recon{upscaledPlanes[plane]};

            if (globalConfig.temporalEnabled && !frame->isPassthrough()) {
                LdcTaskDependency temporal{kTaskDependencyInvalid};

                if (plane < numEnhancedPlanes) {
                    // Still need a temporal buffer, even if the particular frame is not enhanced
                    // winds up getting passed through and applied
                    temporal = frame->needsTemporalBuffer(previousTimestamp, plane);
                    pipeline->findTemporalBuffer(frame, plane);
                }

                if (planeEnhanced) {
                    // Enhancement residuals
                    const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                    if (numPlaneTiles > 1) {
                        LdcTaskDependency* tiles = static_cast<LdcTaskDependency*>(
                            alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                        // Generate and apply each tile's command buffer
                        for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                            LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                            assert(et->plane == plane && et->loq == LOQ0 && et->tile == tile);
                            LdcTaskDependency commands{addTaskGenerateCmdBufferCPU(pipeline, frame, et)};

                            tiles[tile] =
                                addTaskApplyCmdBufferTemporal(pipeline, frame, et, temporal, commands);
                        }
                        // Wait for all tiles to finish
                        temporal = addTaskWaitForMany(pipeline, frame, tiles, numPlaneTiles);
                    } else {
                        LdpEnhancementTile* et = frame->getEnhancementTile(enhancementTileIdx++);
                        assert(et && et->plane == plane && et->loq == LOQ0 && et->tile == 0);

                        LdcTaskDependency commands{addTaskGenerateCmdBufferCPU(pipeline, frame, et)};

                        temporal = addTaskApplyCmdBufferTemporal(pipeline, frame, et, temporal, commands);
                    }
                }

                // Resolve temporal state at execution time; empty buffers only require output conversion.
                if (plane < numEnhancedPlanes) {
                    LdcTaskDependency converted = addTaskAddTemporal(pipeline, frame, plane, temporal,
                                                                     recon, frame->depOutputPicture());
                    addTaskTemporalTransfer(pipeline, frame, converted, plane);
                    if ((plane == 0 && sharpeningEnabled) ||
                        (scalingModes[LOQ0] == Scale0D && scalingModes[LOQ1] == Scale0D && frame->dither())) {
                        outputPlanes[plane] = addTaskSharpen(pipeline, frame, plane, converted);
                    } else {
                        outputPlanes[plane] = converted;
                    }
                } else {
                    outputPlanes[plane] = recon;
                }
            } else {
                if (planeEnhanced && frameConfig.loqEnabled[LOQ0]) {
                    // Enhancement residuals
                    const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                    if (numPlaneTiles > 1) {
                        LdcTaskDependency* tiles = static_cast<LdcTaskDependency*>(
                            alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                        // Generate and apply each tile's command buffer
                        for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                            LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                            assert(et->plane == plane && et->loq == LOQ0 && et->tile == tile);
                            LdcTaskDependency commands{addTaskGenerateCmdBufferCPU(pipeline, frame, et)};
                            tiles[tile] =
                                addTaskApplyCmdBufferDirect(pipeline, frame, et, recon, commands);
                        }
                        // Wait for all tiles to finish
                        recon = addTaskWaitForMany(pipeline, frame, tiles, numPlaneTiles);
                    } else {
                        LdpEnhancementTile* et = frame->getEnhancementTile(enhancementTileIdx++);
                        assert(et->plane == plane && et->loq == LOQ0 && et->tile == 0);

                        LdcTaskDependency commands{addTaskGenerateCmdBufferCPU(pipeline, frame, et)};

                        recon = addTaskApplyCmdBufferDirect(pipeline, frame, et, recon, commands);
                    }
                }
                if (plane < numEnhancedPlanes) {
                    LdcTaskDependency converted = addTaskConvertFromInternal(
                        pipeline, frame, plane, frame->depOutputPicture(), recon);
                    if ((plane == 0 && sharpeningEnabled) ||
                        (scalingModes[LOQ0] == Scale0D && scalingModes[LOQ1] == Scale0D &&
                         frameConfig.ditherStrength > 0)) {
                        outputPlanes[plane] = addTaskSharpen(pipeline, frame, plane, converted);
                    } else {
                        outputPlanes[plane] = converted;
                    }
                }
            }
        }

        assert(enhancementTileIdx == frame->enhancementTileCount);
        // Send output when all planes are ready
        addTaskOutputDone(pipeline, frame, outputPlanes, numImagePlanes);

        // Send base when all tasks that use it have completed
        LdcTaskDependency deps[kLdpPictureMaxNumPlanes] = {};
        uint32_t depsCount = 0;
        ldcTaskGroupFindOutputSetFromInput(frame->taskGroup(), frame->depBasePicture(), deps,
                                           kLdpPictureMaxNumPlanes, &depsCount);
        addTaskBaseDone(pipeline, frame, deps, depsCount);
    }

    // Fill out a task group for a simple unscaled passthrough configuration
    //
    void generateTasksPassthrough(PipelineCPU* pipeline, FrameCPU* frame)
    {
        VNTraceScopedArgs("timestamp", frame->timestamp);

        uint8_t numImagePlanes{kLdpPictureMaxNumPlanes};
        if (frame->basePicture) {
            VNLogDebugF("No base for passthrough: ts:%" PRIx64, frame->timestamp);
            numImagePlanes = ldpPictureLayoutPlanes(&frame->basePicture->layout);
        }

        LdcTaskDependency outputPlanes[kLdpPictureMaxNumPlanes] = {};

        for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
            outputPlanes[plane] = addTaskPassthrough(pipeline, frame, plane, frame->depOutputPicture(),
                                                     frame->depBasePicture());
        }

        // Send output and base when all planes are ready
        addTaskOutputDone(pipeline, frame, outputPlanes, numImagePlanes);
        addTaskBaseDone(pipeline, frame, outputPlanes, numImagePlanes);
    }

} // anonymous namespace

// Generate task graph for a frame
//
void localGenerateTasks(PipelineCPU* pipeline, FrameCPU* frame, uint64_t previousTimestamp)
{
    // Fill out tasks for this frame
    if (pipeline->configuration().showTasks) {
        // Don't consume tasks whilst group is generated
        ldcTaskGroupBlock(frame->taskGroup());
    }

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

} // namespace lcevc_dec::pipeline_cpu
