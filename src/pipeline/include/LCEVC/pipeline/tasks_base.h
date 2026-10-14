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

#ifndef VN_LCEVC_PIPELINE_TASKS_BASE_H
#define VN_LCEVC_PIPELINE_TASKS_BASE_H

#include <LCEVC/common/task_pool.h>
#include <LCEVC/pipeline/frame_base.h>
#include <LCEVC/pipeline/pipeline_base.h>

namespace lcevc_dec::pipeline {

LdcTaskDependency addTaskConvertToInternal(PipelineBase* pipeline, FrameBase* frame,
                                           uint32_t planeIndex, uint32_t baseDepth,
                                           uint32_t enhancementDepth, LdcTaskDependency inputDep);

LdcTaskDependency addTaskConvertFromInternal(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                             LdcTaskDependency dst, LdcTaskDependency src);

LdcTaskDependency addTaskSharpen(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                 LdcTaskDependency inputDep);

LdcTaskDependency addTaskUpscale(PipelineBase* pipeline, FrameBase* frame, LdeLOQIndex fromLoq,
                                 uint32_t plane, LdcTaskDependency basePicture);

LdcTaskDependency addTaskUpscaleDirect(PipelineBase* pipeline, FrameBase* frame, LdeLOQIndex fromLoq,
                                       uint32_t plane, LdcTaskDependency basePicture,
                                       LdcTaskDependency outputPicture);

LdcTaskDependency addTaskGenerateCmdBufferCPU(PipelineBase* pipeline, FrameBase* frame,
                                              LdpEnhancementTile* enhancementTile);

LdcTaskDependency addTaskGenerateCmdBufferGPU(PipelineBase* pipeline, FrameBase* frame,
                                              LdpEnhancementTile* enhancementTile);

LdcTaskDependency addTaskApplyCmdBufferDirect(PipelineBase* pipeline, FrameBase* frame,
                                              LdpEnhancementTile* enhancementTile,
                                              LdcTaskDependency imageBuffer, LdcTaskDependency cmdBuffer);

LdcTaskDependency addTaskApplyCmdBufferTemporal(PipelineBase* pipeline, FrameBase* frame,
                                                LdpEnhancementTile* enhancementTile,
                                                LdcTaskDependency temporalBuffer,
                                                LdcTaskDependency cmdBuffer);

LdcTaskDependency addTaskAddTemporal(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                     LdcTaskDependency temporalDep, LdcTaskDependency sourceDep,
                                     LdcTaskDependency outputDep);

LdcTaskDependency addTaskPassthrough(PipelineBase* pipeline, FrameBase* frame, uint32_t planeIndex,
                                     LdcTaskDependency dest, LdcTaskDependency src);

LdcTaskDependency addTaskWaitForMany(PipelineBase* pipeline, FrameBase* frame,
                                     const LdcTaskDependency* inputs, uint32_t inputsCount);

void addTaskBaseDone(PipelineBase* pipeline, FrameBase* frame, const LdcTaskDependency* inputs,
                     uint32_t inputsCount);

void addTaskOutputDone(PipelineBase* pipeline, FrameBase* frame, const LdcTaskDependency* inputs,
                       uint32_t inputsCount);

void addTaskTemporalTransfer(PipelineBase* pipeline, FrameBase* frame, const LdcTaskDependency dep,
                             uint32_t planeIndex);

} // namespace lcevc_dec::pipeline

#endif // VN_LCEVC_PIPELINE_TASKS_BASE_H
