/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#include "decode/apply_cmdbuffer.h"

#include "common/cmdbuffer.h"
#include "common/log.h"
#include "common/threading.h"
#include "common/tile.h"
#include "common/types.h"
#include "context.h"
#include "decode/apply_cmdbuffer_common.h"
#include "lcevc_config.h"
#include "surface/surface.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct ApplyCmdBufferSlicedJobContext
{
    CmdBufferApplicator_t function;
    const TileState_t* tile;
    const Surface_t* surface;
    const Highlight_t* highlight;
} ApplyCmdBufferSlicedJobContext_t;

int32_t applyCmdBufferSlicedJob(const void* executeContext, JobIndex_t index, SliceOffset_t offset)
{
    VN_UNUSED(offset);
    const ApplyCmdBufferSlicedJobContext_t* context =
        (const ApplyCmdBufferSlicedJobContext_t*)executeContext;
    if (index.current >= context->tile->cmdBuffer->numEntryPoints ||
        context->tile->cmdBuffer->entryPoints[index.current].count <= 0) {
        return 0;
    }
    if (!context->function(context->tile, index.current, context->surface, context->highlight)) {
        return -1;
    }

    return 0;
}

int32_t applyCmdBuffer(Logger_t log, ThreadManager_t threadManager, const TileState_t* tile,
                       const Surface_t* surface, bool surfaceRasterOrder,
                       CPUAccelerationFeatures_t accel, const Highlight_t* highlight)
{
    if (!surface->data) {
        VN_ERROR(log, "apply cmdbuffer surface has no data pointer\n");
        return -1;
    }

    if (surface->interleaving != ILNone) {
        VN_ERROR(log, "apply cmdbuffer does not support interleaved destination surfaces\n");
        return -1;
    }

    CmdBufferApplicator_t applicatorFunction = NULL;
    if (surfaceRasterOrder) {
        applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorSurfaceScalar;
        if (accelerationFeatureEnabled(accel, CAFNEON)) {
            applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorSurfaceNEON;
        } else if (accelerationFeatureEnabled(accel, CAFSSE)) {
            applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorSurfaceSSE;
        }
    } else {
        applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorBlockScalar;
        if (accelerationFeatureEnabled(accel, CAFNEON)) {
            applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorBlockNEON;
        } else if (accelerationFeatureEnabled(accel, CAFSSE)) {
            applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorBlockSSE;
        }
    }

    const CmdBuffer_t* cmdBuffer = tile->cmdBuffer;
    if (cmdBuffer->numEntryPoints == 1 || !cmdBuffer->entryPoints) {
        if (cmdBufferIsEmpty(cmdBuffer)) {
            return 0;
        }

        if (!applicatorFunction(tile, 0, surface, highlight)) {
            return -1;
        }
    } else {
        const ApplyCmdBufferSlicedJobContext_t slicedJobContext = {
            .function = (CmdBufferApplicator_t)applicatorFunction,
            .tile = tile,
            .surface = surface,
            .highlight = highlight,
        };

        if (!threadingExecuteSlicedJobs(&threadManager, &applyCmdBufferSlicedJob, &slicedJobContext,
                                        cmdBuffer->numEntryPoints)) {
            return -1;
        }
    }
    return 0;
}

/*------------------------------------------------------------------------------*/
