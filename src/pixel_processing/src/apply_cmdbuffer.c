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

#include "apply_cmdbuffer_common.h"

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/pipeline/frame.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pixel_processing/apply_cmdbuffer.h>
#include <stdbool.h>
#include <stddef.h>

/*------------------------------------------------------------------------------*/

typedef struct ApplyCmdBufferSlicedJobContext
{
    CmdBufferApplicator function;
    const LdpEnhancementTile* enhancementTile;
    const LdpPicturePlaneDesc plane;
    LdpFixedPoint fixedPoint;
    const TileDesc tileDesc;
    bool highlight;
} ApplyCmdBufferSlicedJobContext;

static bool applyCmdBufferSlicedJob(void* argument, uint32_t offset, uint32_t count)
{
    VNTraceScopedBegin();

    const ApplyCmdBufferSlicedJobContext* context = (const ApplyCmdBufferSlicedJobContext*)argument;
    bool r = true;
    for (uint32_t i = 0; i < count; ++i) {
        r &= context->function(context->enhancementTile, offset + i, &context->plane,
                               context->fixedPoint, context->highlight);
    }

    VNTraceScopedEnd();
    return r;
}

bool ldppApplyCmdBuffer(LdcTaskPool* taskPool, LdcTask* parent, LdpEnhancementTile* enhancementTile,
                        LdpFixedPoint fixedPoint, const LdpPicturePlaneDesc* plane,
                        bool rasterOrder, bool forceScalar, bool highlight)
{
    if (!plane->firstSample) {
        VNLogError("Apply cmdbuffer surface has no data pointer");
        return false;
    }

    LdeCmdBufferCpu* cmdBuffer = &enhancementTile->buffer;
    if (ldeCmdBufferCpuIsEmpty(cmdBuffer)) {
        return true;
    }

    const LdcAcceleration* acceleration = ldcAccelerationGet();

    CmdBufferApplicator applicatorFunction = NULL;
    if (rasterOrder) {
        if (!forceScalar && acceleration->NEON) {
            applicatorFunction = (CmdBufferApplicator)cmdBufferApplicatorSurfaceNEON;
        } else if (!forceScalar && acceleration->SSE) {
            applicatorFunction = (CmdBufferApplicator)cmdBufferApplicatorSurfaceSSE;
        }

        if (!applicatorFunction) {
            applicatorFunction = (CmdBufferApplicator)cmdBufferApplicatorSurfaceScalar;
        }
    } else {
        if (!forceScalar && acceleration->NEON) {
            applicatorFunction = (CmdBufferApplicator)cmdBufferApplicatorBlockNEON;
        } else if (!forceScalar && acceleration->SSE) {
            applicatorFunction = (CmdBufferApplicator)cmdBufferApplicatorBlockSSE;
        }
        if (!applicatorFunction) {
            applicatorFunction = (CmdBufferApplicator)cmdBufferApplicatorBlockScalar;
        }
    }

    if (cmdBuffer->numEntryPoints == 0 || !cmdBuffer->entryPoints) {
        LdeCmdBufferCpuEntryPoint entryPoint = {0};
        entryPoint.count = cmdBuffer->count;
        cmdBuffer->entryPoints = &entryPoint;

        if (!applicatorFunction(enhancementTile, 0, plane, fixedPoint, highlight)) {
            cmdBuffer->entryPoints = NULL;
            return false;
        }
        cmdBuffer->entryPoints = NULL;
    } else {
        ApplyCmdBufferSlicedJobContext slicedJobContext = {
            .function = applicatorFunction,
            .enhancementTile = enhancementTile,
            .plane = *plane,
            .fixedPoint = fixedPoint,
            .highlight = highlight,
        };

        return ldcTaskPoolAddSlicedDeferred(taskPool, parent, &applyCmdBufferSlicedJob, NULL,
                                            &slicedJobContext, sizeof(slicedJobContext),
                                            cmdBuffer->numEntryPoints);
    }
    return true;
}

/*------------------------------------------------------------------------------*/
