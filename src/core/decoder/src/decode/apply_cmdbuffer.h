/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_APPLY_CMDBUFFER_H_
#define VN_DEC_CORE_APPLY_CMDBUFFER_H_

#include "common/platform.h"
#include "common/types.h"

/*! \file
 *
 *   This module implements several routines for processing commands buffers.
 *
 *   There are 3 types of command buffer actions to be performed.
 *
 *   #Apply inter residuals
 *
 *       This performs a saturating addition of residuals onto a destination
 *       surface, there are several different destinations that can be used
 *       here:
 *
 *       1. Unsigned surfaces (U8, U10, U12, U14), this is for the in-place scenario
 *          when temporal is turned off.
 *       2. Signed surfaces (S8.7, S10.5, S12.3, S14.1), this is for updating the
 *          temporal buffer.
 *       3. Others??? - e.g. lower bit-depth temporal buffer?
 *
 *   #Apply intra residuals
 *
 *       This performs a write to the destination, the only use case for this is
 *       for performing writes into the temporal buffer. Similarly we may have
 *       a lower bit-depth temporal buffer approximation.
 *
 *   #Tile clear
 *
 *       This resets a region of the temporal buffer back to zero. The region size
 *       is always 32x32 pixels.
 *
 *   #Destination surface layout
 *
 *       **This is for future work, currently we only implement point 1 below.**
 *
 *       The destination surface could have different representations depending on
 *       optimization strategies used.
 *
 *       There are currently 2 approaches here:
 *
 *       1. The "default" mode is the surfaces are stored in raster scanline ordering
 *          and so can be treated exactly like an image.
 *
 *       2. For more efficient access of the temporal buffer we may choose to store
 *          the temporal buffer in transform unit linear ordering such that the 4
 *          or 16 values for a transform are stored in linear address order rather than
 *          across 2 or 4 addressable locations.
 *
 *          Additionally the temporal buffer may be stored in block order too to help
 *          with efficient access during tile clears.
 */

/*------------------------------------------------------------------------------*/

typedef struct Surface Surface_t;
typedef struct CmdBuffer CmdBuffer_t;
typedef struct Highlight Highlight_t;

/*------------------------------------------------------------------------------*/

typedef enum ApplyCmdBufferMode
{
    ACBM_Inter,
    ACBM_Intra,
    ACBM_Tiles,
    ACBM_Highlight,
} ApplyCmdBufferMode_t;

/*------------------------------------------------------------------------------*/

bool applyCmdBuffer(Context_t* ctx, const CmdBuffer_t* buffer, const Surface_t* surface,
                    ApplyCmdBufferMode_t mode, CPUAccelerationFeatures_t accel,
                    const Highlight_t* highlight);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_APPLY_CMDBUFFER_H_ */