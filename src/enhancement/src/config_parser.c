/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/dimensions.h>
//
#include "bitstream.h"
#include "bytestream.h"
#include "chunk.h"
#include "chunk_parser.h"
#include "config_parser_types.h"
#include "dequant.h"
#include "log_utilities.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------------------*/

static bool parseConformanceValue(ByteStream* stream, uint16_t* dst, const char* debugLabel)
{
    uint64_t value;

    VNCheckB(bytestreamReadMultiByte(stream, &value));

    /* Validate values can be assigned to window args */
    if (value > kMaximumConformanceWindowValue) {
        return false;
    }

    *dst = (uint16_t)value;
    VNLogDebug("Conformance window %s: %u", debugLabel, *dst);
    return true;
}

static bool isDepthConfigSupported(const LdeGlobalConfig* globalConfig)
{
    /* Currently only support promoting base-depth to enhancement depth. */
    if (globalConfig->enhancedDepth < globalConfig->baseDepth) {
        VNLogError("stream: Unsupported functionality. depth configuration is unsupported - "
                   "[base_depth=%s, enha_depth=%s, loq1_use_enha_depth=%s]",
                   bitdepthToString(globalConfig->baseDepth),
                   bitdepthToString(globalConfig->enhancedDepth),
                   globalConfig->loq1UseEnhancedDepth ? "true" : "false");
        return false;
    }

    return true;
}

static bool validateResolution(const LdeGlobalConfig* globalConfig)
{
    const LdeScalingMode scaling = globalConfig->scalingModes[LOQ0];
    const LdeChroma chroma = globalConfig->chroma;

    /* This is a safety net, monochrome should always signal as 1 plane.*/
    const bool withChroma = (globalConfig->numPlanes > 1) && (chroma != CTMonochrome);
    const uint16_t transformAlignment = (globalConfig->transform == TransformDD) ? 2 : 4;

    /* Expand for scaling mode */
    const uint16_t horiScaling = (scaling != Scale0D) ? 2 : 1;
    const uint16_t vertScaling = (scaling == Scale2D) ? 2 : 1;

    /* Expand alignment for chroma (if enabled). */
    const uint16_t horiChroma = (withChroma && chroma != CT444) ? 2 : 1;
    const uint16_t vertChroma = (withChroma && chroma == CT420) ? 2 : 1;

    /* Determine signal width/height alignment requirements. */
    const uint16_t horiAlignment = transformAlignment * horiScaling * horiChroma;
    const uint16_t vertAlignment = transformAlignment * vertScaling * vertChroma;

    /* This relies on alignments both being a power of 2. */
    if ((globalConfig->width & (horiAlignment - 1)) || (globalConfig->height & (vertAlignment - 1))) {
        VNLogError("Resolution not supported in LCEVC layer. Resolution must be a factor "
                   "of whole transforms");
        return false;
    }

    return true;
}

static bool tileDimensionsFromType(LdeTileDimensions type, uint16_t* width, uint16_t* height)
{
    assert(width);
    assert(height);

    switch (type) {
        case TDT512x256:
            *width = 512;
            *height = 256;
            break;
        case TDT1024x512:
            *width = 1024;
            *height = 512;
            break;
        case TDTNone:
        case TDTCustom: return false;
    }

    return true;
}

/*! \brief Updates the global config with the correct tile dimensions for each plane.
 *
 * This is performed to ensure that there ends up being the same number of tiles per-plane
 * independent of the chroma subsampling being performed.
 *
 * \param globalConfig  Global config to update the tile dimensions for.
 *
 * \return True on success, otherwise false.
 */
static bool calculateTileDimensions(LdeGlobalConfig* globalConfig)
{
    uint16_t hshift = 0;
    uint16_t vshift = 0;

    switch (globalConfig->chroma) {
        case CT420:
            hshift = 1;
            vshift = 1;
            break;
        case CT422:
            hshift = 1;
            vshift = 0;
            break;
        case CTMonochrome:
        case CT444: break;
        case CTCount: return false;
    }

    globalConfig->tileWidth[1] = globalConfig->tileWidth[2] =
        (globalConfig->tileWidth[0] + hshift) >> hshift;
    globalConfig->tileHeight[1] = globalConfig->tileHeight[2] =
        (globalConfig->tileHeight[0] + vshift) >> vshift;

    return true;
}

/*! \brief Determines the number of whole and partial tiles across and down for
 *         each Plane and LOQ.
 *
 * \param globalConfig       Global config to obtain dimensions from
 *
 * \return True on success, otherwise false.
 */
static bool calculateTileCounts(LdeGlobalConfig* globalConfig)
{
    const uint8_t tuSize = (globalConfig->transform == TransformDDS) ? 4 : 2;

    /* Determine appropriate number of tiles for each plane and loq. */
    for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
        if ((globalConfig->tileWidth[plane] % tuSize) || (globalConfig->tileHeight[plane] % tuSize)) {
            VNLogError("invalid stream: Tile dimensions must be divisible by transform size");
            return false;
        }

        for (uint8_t loq = 0; loq < LOQEnhancedCount; ++loq) {
            uint16_t loqWidth = 0;
            uint16_t loqHeight = 0;

            ldePlaneDimensionsFromConfig(globalConfig, (LdeLOQIndex)loq, plane, &loqWidth, &loqHeight);

            uint32_t tilesAcross = divideCeilU16(loqWidth, globalConfig->tileWidth[plane]);
            uint32_t tilesDown = divideCeilU16(loqHeight, globalConfig->tileHeight[plane]);

            globalConfig->numTiles[plane][loq] = tilesAcross * tilesDown;

            VNLogVerbose("  Tile count plane %u LOQ-%u: %dx%d (%d)", plane, loq, tilesAcross,
                         tilesDown, globalConfig->numTiles[plane][loq]);

            /* As it is currently intended that all planes at a given LOQ have the
             * same number of tiles, ensure that is the case. */
            if ((plane > 1) && (globalConfig->numTiles[plane][loq] != globalConfig->numTiles[0][loq])) {
                VNLogError("Invalid tile counts calculated. Each plane should have the "
                           "same number of tiles");
                return false;
            }
        }
    }

    return true;
}

static bool calculateTileConfiguration(LdeGlobalConfig* globalConfig)
{
    /* Ensure all tile dimensions are now valid across all planes. */
    VNCheckB(calculateTileDimensions(globalConfig));

    /* Determine number of tiles across all planes and LOQs. */
    VNCheckB(calculateTileCounts(globalConfig));

    return true;
}

/*------------------------------------------------------------------------------*/

/* 7.3.4 (Table-8) & 7.4.3.2
 * The profiles and levels tell us information about the expected bitrate of the stream, and impose
 * limitations on the chroma subsampling, but we don't use this information (other than printing it
 * out). Occurs with the first IDR (and possibly other IDRs) */
static bool parseBlockSequenceConfig(ByteStream* stream, LdeGlobalConfig* globalConfig)
{
    uint8_t data;
    VNCheckB(bytestreamReadU8(stream, &data));

    /* Profile: 4 bits */
    uint8_t profile = (data >> 4) & 0x0F;
    VNLogDebug("  Profile: %u", profile);

    /* Level: 4 bits */
    uint8_t level = (data >> 0) & 0x0F;
    VNLogDebug("  Level: %u", level);

    VNCheckB(bytestreamReadU8(stream, &data));

    /* Sub-level: 2 bits */
#ifdef VN_SDK_LOG_ENABLE_DEBUG
    uint8_t sublevel = (data >> 6) & 0x03;
    VNLogDebug("  Sub-level: %u", sublevel);
#endif

    /* Conformance window flag: 1 bit */
    globalConfig->cropEnabled = (data >> 5) & 0x01;
    VNLogDebug("  Conformance window enabled: %u", globalConfig->cropEnabled);

    /* Possible extended profile: 8 bits */
    if (profile == 15 || level == 15) {
        VNCheckB(bytestreamReadU8(stream, &data));
#ifdef VN_SDK_LOG_ENABLE_DEBUG
        const uint8_t extendedProfile = (data >> 5) & 0x07;
        VNLogDebug("   Extended profile: %u", extendedProfile);

        const uint8_t extendedLevel = (data >> 1) & 0x7F;
        VNLogDebug("   Extended level: %u", extendedLevel);
#endif
    }

    if (globalConfig->cropEnabled) {
        /* conf_win_left_offset: multibyte
           conf_win_right_offset: multibyte
           conf_win_top_offset: multibyte
           conf_win_bottom_offset: multibyte */
        VNCheckB(parseConformanceValue(stream, &globalConfig->crop.left, "left"));
        VNCheckB(parseConformanceValue(stream, &globalConfig->crop.right, "right"));
        VNCheckB(parseConformanceValue(stream, &globalConfig->crop.top, "top"));
        VNCheckB(parseConformanceValue(stream, &globalConfig->crop.bottom, "bottom"));

        VNLogDebug("  Conformance window: %u %u %u %u", globalConfig->crop.left,
                   globalConfig->crop.right, globalConfig->crop.top, globalConfig->crop.bottom);
    }

    return true;
}

static void setUserDataConfig(LdeGlobalConfig* globalConfig, LdeUserDataMode mode)
{
    LdeUserDataConfig* userData = &globalConfig->userData;
    memset(userData, 0, sizeof(LdeUserDataConfig));

    VNLogDebug("  User data mode: %s", userDataModeToString(mode));

    if (mode != UDMNone) {
        userData->enabled = true;
        userData->layerIndex =
            (globalConfig->transform == TransformDDS) ? UDCLayerIndexDDS : UDCLayerIndexDD;
        userData->shift = (mode == UDMWith2Bits) ? UDCShift2 : UDCShift6;
    }

    VNLogDebug("  User data mode: %s", userDataModeToString(mode));
    VNLogDebug("  User data layer: %d", userData->layerIndex);
    VNLogDebug("  User data shift: %d", userData->shift);
}

static bool postParseInitBlockGlobalConfig(LdeGlobalConfig* globalConfig)
{
    /* When tiling is disabled, there is a single tile that is the size of the surface for each
     * plane. We cannot do this when parsing the tiling data, because the bitstream
     * order is: normal resolution, then tiling data, finally custom resolution. */
    if (globalConfig->tileDimensions == TDTNone) {
        globalConfig->tileWidth[0] = globalConfig->width;
        globalConfig->tileHeight[0] = globalConfig->height;
    }

    /* validate/update conformance window */
    if (globalConfig->cropEnabled) {
        const uint16_t shiftw = (globalConfig->chroma == CT420 || globalConfig->chroma == CT422) ? 1 : 0;
        const uint16_t shifth = globalConfig->chroma == CT420 ? 1 : 0;

        globalConfig->crop.left <<= shiftw;
        globalConfig->crop.right <<= shiftw;
        globalConfig->crop.top <<= shifth;
        globalConfig->crop.bottom <<= shifth;

        VNLogDebug("  Conformance window: left: %u, right: %u, top: %u, bottom: %u",
                   globalConfig->crop.left, globalConfig->crop.right, globalConfig->crop.top,
                   globalConfig->crop.bottom);

        if ((globalConfig->crop.left + globalConfig->crop.right) >= globalConfig->width) {
            VNLogError("stream: Conformance window values combined are greater than decode width "
                       "[left: %u, right: %u, width: %u]",
                       globalConfig->crop.left, globalConfig->crop.right, globalConfig->width);
            return false;
        }

        if ((globalConfig->crop.top + globalConfig->crop.bottom) >= globalConfig->height) {
            VNLogError("stream: Window values combined are greater than decode width [top: %u, "
                       "bottom: %u, height: %u]",
                       globalConfig->crop.top, globalConfig->crop.bottom, globalConfig->height);
            return false;
        }
    }
    return true;
}

static uint8_t parseGlobalConfigGetNumPlanes(ByteStream* stream, const uint8_t planeModeFlag)
{
    /* plane_type: 4 bits
       reserved: 4 bits */
    if (!planeModeFlag) {
        return 1;
    }

    uint8_t data = 0;
    if (!bytestreamReadU8(stream, &data)) {
        return 0;
    }
    LdePlanesType planeType = (LdePlanesType)((data >> 4) & 0x0f);
    VNLogDebug("  Plane type: %s", planesTypeToString(planeType));

    switch (planeType) {
        case PlanesY: return 1; break;
        case PlanesYUV: return 3; break;
        default: {
            VNLogError("Unrecognized plane type: %u", (uint8_t)planeType);
            break;
        }
    }
    return 0;
}

/* 7.3.5 (Table-9), from row "if (tile_dimensions_type > 0) {" */
static bool parseBlockGlobalConfigTiles(ByteStream* stream, LdeGlobalConfig* globalConfig)
{
    if (globalConfig->tileDimensions == TDTNone) {
        /* This case is handled later, in postParseInitBlockGlobalConfig */
        return true;
    }

    if (globalConfig->tileDimensions == TDTCustom) {
        /* custom_tile_width: 16 bits */
        VNCheckB(bytestreamReadU16(stream, &globalConfig->tileWidth[0]));

        /* custom_tile_height: 16 bits */
        VNCheckB(bytestreamReadU16(stream, &globalConfig->tileHeight[0]));
    } else {
        VNCheckB(tileDimensionsFromType(globalConfig->tileDimensions, &globalConfig->tileWidth[0],
                                        &globalConfig->tileHeight[0]));
    }

    uint8_t data = 0;
    VNCheckB(bytestreamReadU8(stream, &data));

    /* reserved: 5 bits
       compression_type_entropy_enabled_per_tile_flag: 1 bit */
    globalConfig->perTileCompressionEnabled = (data >> 2) & 0x01;

    /* compression_type_size_per_tile: 2 bits */
    globalConfig->tileSizeCompression = (LdeTileCompressionSizePerTile)((data >> 0) & 0x03);

    VNLogDebug("  Custom tile size: %ux%u", globalConfig->tileWidth[0], globalConfig->tileHeight[0]);
    VNLogDebug("  Per tile enabled compression: %d", globalConfig->perTileCompressionEnabled);
    VNLogDebug("  Tile size compression: %u", (uint8_t)globalConfig->tileSizeCompression);

    return true;
}

/* 7.3.5 (Table-9) & 7.4.3.3
 * Occurs once per IDR frame */
static bool parseBlockGlobalConfig(ByteStream* stream, LdeGlobalConfig* globalConfig)
{
    if (!globalConfig->bitstreamVersionSet) {
        /* V-Nova config should always arrive before global config. If it has not been sent this
         * frame and a global config is received, then set the version permanently to the current
         * version */
        globalConfig->bitstreamVersionSet = true;
        globalConfig->bitstreamVersion = BitstreamVersionCurrent;
    }

    uint8_t data = 0;
    VNCheckB(bytestreamReadU8(stream, &data));

    /* plane_mode_flag: 1 bit */
    const uint8_t planeModeFlag = (data >> 7) & 0x01;
    VNLogDebug("  Plane mode flag: %u", planeModeFlag);

    /* resolution_type: 6 bits */
    const uint8_t resType = (data >> 1) & 0x3F;
    VNLogDebug("  Resolution type: %u", resType);

    if (resType > 0 && resType < kResolutionCount) {
        globalConfig->width = kResolutions[resType].width;
        globalConfig->height = kResolutions[resType].height;
        VNLogDebug("  Resolution width: %u", globalConfig->width);
        VNLogDebug("  Resolution height: %u", globalConfig->height);
    } else if (resType != kResolutionCustom) {
        VNLogError("Packet gave an unsupported resolution type %d", resType);
        return false;
    }

    /* transform_type: 1 bit */
    globalConfig->transform = (LdeTransformType)((data >> 0) & 0x01);
    VNLogDebug("  Transform type: %s", transformTypeToString(globalConfig->transform));

    switch (globalConfig->transform) {
        case TransformDD: globalConfig->numLayers = RCLayerCountDD; break;
        case TransformDDS: globalConfig->numLayers = RCLayerCountDDS; break;
        default: {
            VNLogError("Supplied transform is unrecognized: %s",
                       transformTypeToString(globalConfig->transform));
            return false;
        }
    }

    VNCheckB(bytestreamReadU8(stream, &data));

    /* chroma_sampling_type: 2 bits */
    globalConfig->chroma = (LdeChroma)((data >> 6) & 0x03);
    VNLogDebug("  Chroma sampling type: %s", chromaToString(globalConfig->chroma));

    /* base_depth_type: 2 bits */
    globalConfig->baseDepth = (LdeBitDepth)((data >> 4) & 0x03);
    VNLogDebug("  Base depth: %s", bitdepthToString(globalConfig->baseDepth));

    /* enhancement_depth_type: 2 bit */
    globalConfig->enhancedDepth = (LdeBitDepth)((data >> 2) & 0x03);
    VNLogDebug("  Enhancement depth: %s", bitdepthToString(globalConfig->enhancedDepth));

    /* temporal_step_width_modifier_signalled_flag: 1 bit */
    const uint8_t useTemporalStepWidthModifier = (data >> 1) & 0x01;
    VNLogDebug("  Use temporal step-width modifier: %u", useTemporalStepWidthModifier);

    /* predicted_residual_mode_flag: 1 bit */
    globalConfig->predictedAverageEnabled = (data >> 0) & 0x01;
    VNLogDebug("  Use predicted average: %u", globalConfig->predictedAverageEnabled);

    VNCheckB(bytestreamReadU8(stream, &data));

    /* temporal_tile_intra_signalling_enabled_flag: 1 bit */
    globalConfig->temporalReducedSignallingEnabled = (data >> 7) & 0x01;
    VNLogDebug("  Temporal use reduced signalling: %u", globalConfig->temporalReducedSignallingEnabled);

    /* temporal_enabled_flag: 1 bit */
    globalConfig->temporalEnabled = (data >> 6) & 0x01;
    VNLogDebug("  Temporal enabled: %u", globalConfig->temporalEnabled);

    /* upsample_type: 3 bits */
    const LdeUpscaleType upsample = (LdeUpscaleType)((data >> 3) & 0x07);
    VNLogDebug("  Upsample type: %s", upscaleTypeToString(upsample));

    if (upsample != USNearest && upsample != USLinear && upsample != USCubic &&
        upsample != USModifiedCubic && upsample != USAdaptiveCubic) {
        VNLogError("Unrecognized upscale type");
        return false;
    }

    globalConfig->upscale = upsample;

    /* level1_filtering_signalled_flag: 1 bit */
    const uint8_t deblockingSignalled = (data >> 2) & 0x01;
    VNLogDebug("  Deblocking signalled: %u", deblockingSignalled);

    /* scaling_mode_level1: 2 bits */
    globalConfig->scalingModes[LOQ1] = (LdeScalingMode)((data >> 0) & 0x03);
    VNLogDebug("  Scaling mode LOQ-1: %s", scalingModeToString(globalConfig->scalingModes[LOQ1]));

    VNCheckB(bytestreamReadU8(stream, &data));

    /* scaling_mode_level2: 2 bits*/
    globalConfig->scalingModes[LOQ0] = (LdeScalingMode)((data >> 6) & 0x03);
    VNLogDebug("  Scaling mode LOQ-0: %s", scalingModeToString(globalConfig->scalingModes[LOQ0]));

    /* tile_dimensions_type: 2 bits */
    globalConfig->tileDimensions = (LdeTileDimensions)((data >> 4) & 0x03);
    VNLogDebug("  Tile dimensions: %s", tileDimensionsToString(globalConfig->tileDimensions));

    /* user_data_enabled: 2 bits */
    setUserDataConfig(globalConfig, (LdeUserDataMode)((data >> 2) & 0x03));

    /* level1_depth_flag: 1 bit
       reserved: 1 bit */
    globalConfig->loq1UseEnhancedDepth = ((data >> 1) & 0x01) ? true : false;
    VNLogDebug("  LOQ-1 use enhancement depth: %u", globalConfig->loq1UseEnhancedDepth);

    /* chroma_step_width_flag: 1 bit*/
    const uint8_t chromaStepWidthFlag = (data >> 0) & 0x01;
    VNLogDebug("  Chroma step-width flag: %u", chromaStepWidthFlag);

    if (!isDepthConfigSupported(globalConfig)) {
        return false;
    }

    globalConfig->numPlanes = parseGlobalConfigGetNumPlanes(stream, planeModeFlag);
    if (globalConfig->numPlanes == 0) {
        return false;
    }
    VNLogDebug("  Plane count: %u", globalConfig->numPlanes);

    /* temporal_step_width_modifier: 8 bits. (if absent, correct default is already set in initialization). */
    if (useTemporalStepWidthModifier != 0) {
        VNCheckB(bytestreamReadU8(stream, &globalConfig->temporalStepWidthModifier));
    }
    VNLogDebug("  Temporal step-width modifier: %u", globalConfig->temporalStepWidthModifier);

    if (upsample == USAdaptiveCubic) {
        /* 8.6.7 */
        const int8_t kernelSize = 4;

        globalConfig->kernel.length = kernelSize;
        globalConfig->kernel.approximatedPA = false;

        for (int16_t i = 0; i < kernelSize; i++) {
            /* First and last coefficients are negative */
            const int16_t multiplier = ((i % 3) == 0) ? -1 : 1;
            uint16_t coeff;

            VNCheckB(bytestreamReadU16(stream, &coeff));

            globalConfig->kernel.coeffs[0][i] = globalConfig->kernel.coeffs[1][kernelSize - 1 - i] =
                (int16_t)(multiplier * coeff);
        }

        VNLogDebug("  Adaptive upsampler kernel: %d %d %d %d", globalConfig->kernel.coeffs[0][0],
                   globalConfig->kernel.coeffs[0][1], globalConfig->kernel.coeffs[0][2],
                   globalConfig->kernel.coeffs[0][3]);
    } else {
        globalConfig->kernel = kKernels[upsample];
    }

    /* Deblocking coefficients. */
    if (deblockingSignalled) {
        VNCheckB(bytestreamReadU8(stream, &data));

        /* level1_filtering_first_coefficient: 4 bits */
        globalConfig->deblock.corner = 16 - ((data >> 4) & 0x0F);

        /* level1_filtering_second_coefficient: 4 bits */
        globalConfig->deblock.side = 16 - ((data >> 0) & 0x0F);
    } else {
        globalConfig->deblock.corner = kDefaultDeblockCoefficient;
        globalConfig->deblock.side = kDefaultDeblockCoefficient;
    }
    VNLogDebug("  Deblocking coeffs - 0: %u, 1: %u", globalConfig->deblock.corner,
               globalConfig->deblock.side);

    VNCheckB(parseBlockGlobalConfigTiles(stream, globalConfig));

    /* custom resolution */
    if (resType == kResolutionCustom) {
        VNCheckB(bytestreamReadU16(stream, &globalConfig->width));
        VNCheckB(bytestreamReadU16(stream, &globalConfig->height));

        VNLogDebug("  Custom resolution width: %u", globalConfig->width);
        VNLogDebug("  Custom resolution height: %u", globalConfig->height);
    }

    /* chroma_step_width_multiplier: 8 bits. If absent, correct default is already set by initialization */
    if (chromaStepWidthFlag) {
        VNCheckB(bytestreamReadU8(stream, &globalConfig->chromaStepWidthMultiplier));
    }

    VNLogDebug("  Chroma step-width multiplier: %u", globalConfig->chromaStepWidthMultiplier);

    /* Validate settings, and use them to default initialise some data. */
    if (!validateResolution(globalConfig)) {
        return false;
    }

    VNCheckB(postParseInitBlockGlobalConfig(globalConfig));
    VNCheckB(calculateTileConfiguration(globalConfig));

    globalConfig->initialized = true;

    return true;
}

static bool parseEncodedData(ByteStream* stream, LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    if (!globalConfig->initialized) {
        VNLogError("stream: Have not yet received a global config block");
        return false;
    }

    if (!frameConfig->frameConfigSet) {
        VNLogError("stream: Have not yet received a picture config block");
        return false;
    }

    /* Pre-calculate chunk offsets for quicker chunk lookup. */
    calculateTileChunkIndices(frameConfig, globalConfig);
    VNCheckB(chunkCheckAlloc(frameConfig, globalConfig));

    /* --- Read the enabled & RLE-only flags --- */
    BitStream chunkHeadersStream = {{0}};
    VNCheckB(bitstreamInitialize(&chunkHeadersStream, bytestreamCurrent(stream),
                                 bytestreamRemaining(stream)));

    for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
        if (frameConfig->entropyEnabled) {
            for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
                LdeChunk* chunks;
                VNCheckB(getLayerChunks(globalConfig, frameConfig, plane, (LdeLOQIndex)loq, 0, &chunks));
                VNCheckB(parseChunkFlags(&chunkHeadersStream, chunks, globalConfig->numLayers));
            }
        }

        if (frameConfig->temporalSignallingPresent) {
            LdeChunk* temporalChunk;
            VNCheckB(getTemporalChunk(globalConfig, frameConfig, plane, 0, &temporalChunk));
            VNCheckB(parseChunkFlags(&chunkHeadersStream, temporalChunk, 1));
        }
    }

    /* Move bytestream forward with byte alignment */
    bytestreamSeek(stream, bitstreamGetConsumedBytes(&chunkHeadersStream));

    /* --- Read chunk data --- */
    VNLogVerbose("  [Plane, LoQ, Layer]");
    for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
        if (frameConfig->entropyEnabled) {
            for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
                VNCheckB(parseCoefficientChunks(stream, globalConfig, frameConfig, (LdeLOQIndex)loq, plane));
            }
        }

        if (frameConfig->temporalSignallingPresent) {
            VNLogVerbose("    [temporal: %d]: ", plane);
            LdeChunk* temporalChunk = NULL;
            VNCheckB(getTemporalChunk(globalConfig, frameConfig, plane, 0, &temporalChunk));
            if (temporalChunk == NULL) {
                return false;
            }
            VNCheckB(parseChunk(stream, temporalChunk, &frameConfig->loqEnabled[LOQ0], NULL));
        }
    }

    return true;
}

static bool parseEncodedDataTiled(ByteStream* stream, LdeFrameConfig* frameConfig,
                                  const LdeGlobalConfig* globalConfig)
{
    if (!globalConfig->initialized) {
        VNLogError("stream: Have not yet received a global config block");
        return false;
    }

    if (!frameConfig->frameConfigSet) {
        VNLogError("stream: Have not yet received a picture config block");
        return false;
    }

    if (globalConfig->tileWidth[0] == 0 || globalConfig->tileHeight[0] == 0) {
        VNLogError("stream: Both tile dimensions must not be 0");
        return false;
    }

    /* Pre-calculate chunk offsets for quicker chunk lookup. */
    calculateTileChunkIndices(frameConfig, globalConfig);
    VNCheckB(chunkCheckAlloc(frameConfig, globalConfig));

    if (frameConfig->entropyEnabled || frameConfig->temporalSignallingPresent) {
        BitStream rleOnlyBS = {{0}};
        uint8_t layerRLEOnly = 0;

        BitStream entropyEnabledBS = {{0}};
        TiledRLEDecoder entropyEnabledRLE = {0};
        TiledSizeDecoder sizeDecoder = {0};
        TiledSizeDecoder* sizeDecoderPtr =
            (globalConfig->tileSizeCompression != TCSPTTNone) ? &sizeDecoder : NULL;

        VNCheckB(bitstreamInitialize(&rleOnlyBS, bytestreamCurrent(stream), bytestreamRemaining(stream)));

        /* --- Read the RLE-only flags --- */

        VNLogVerbose("  RLE only flags");
        VNLogVerbose("  [Plane, LoQ, Layer]");

        for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
            /* Whole surface RLE only flag per-layer */
            if (frameConfig->entropyEnabled) {
                for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
                    const uint32_t currentTileCount = globalConfig->numTiles[plane][loq];

                    for (uint8_t layer = 0; layer < globalConfig->numLayers; ++layer) {
                        /* Read a bit for RLE signal. */
                        VNCheckB(bitstreamReadBit(&rleOnlyBS, &layerRLEOnly));

                        VNLogVerbose("  [%u, %d, %2u]: %u", plane, loq, layer, layerRLEOnly);

                        /* Broadcast RLE only to all tiles for a layer. */
                        for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                            const uint32_t chunkIndex = getLayerChunkIndex(
                                frameConfig, globalConfig, (LdeLOQIndex)loq, plane, tile, layer);
                            frameConfig->chunks[chunkIndex].rleOnly = layerRLEOnly;
                        }
                    }
                }
            }

            /* Temporal layer RLE only flag*/
            if (frameConfig->temporalSignallingPresent) {
                /* Read a bit for RLE signal. */
                uint8_t temporalRLEOnly = 0;
                const uint32_t currentTileCount = globalConfig->numTiles[plane][LOQ0];
                LdeChunk* temporalTileChunks =
                    &frameConfig->chunks[frameConfig->tileChunkTemporalIndex[plane]];

                VNCheckB(bitstreamReadBit(&rleOnlyBS, &temporalRLEOnly));
                VNLogVerbose("  [temporal: %u]: %u", plane, temporalRLEOnly);

                /* Broadcast RLE only to all tiles for the temporal layer. */
                for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                    temporalTileChunks[tile].rleOnly = temporalRLEOnly;
                }
            }
        }

        /* Move bytestream forward with byte alignment */
        bytestreamSeek(stream, bitstreamGetConsumedBytes(&rleOnlyBS));

        /* --- Read the entropy enabled flags --- */
        if (globalConfig->perTileCompressionEnabled) {
            VNCheckB(tiledRLEDecoderInitialize(&entropyEnabledRLE, stream));
        } else {
            VNCheckB(bitstreamInitialize(&entropyEnabledBS, bytestreamCurrent(stream),
                                         bytestreamRemaining(stream)));
        }

        for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
            if (frameConfig->entropyEnabled) {
                for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
                    const uint32_t currentTileCount = globalConfig->numTiles[plane][loq];

                    for (uint8_t layer = 0; layer < globalConfig->numLayers; ++layer) {
                        for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                            const uint32_t chunkIndex = getLayerChunkIndex(
                                frameConfig, globalConfig, (LdeLOQIndex)loq, plane, tile, layer);
                            LdeChunk* chunk = &frameConfig->chunks[chunkIndex];

                            if (globalConfig->perTileCompressionEnabled) {
                                VNCheckB(tiledRLEDecoderRead(&entropyEnabledRLE,
                                                             (uint8_t*)&chunk->entropyEnabled));
                            } else {
                                VNCheckB(bitstreamReadBit(&entropyEnabledBS,
                                                          (uint8_t*)&chunk->entropyEnabled));
                            }
                        }
                    }
                }
            }

            if (frameConfig->temporalSignallingPresent) {
                const uint32_t currentTileCount = globalConfig->numTiles[plane][LOQ0];
                LdeChunk* temporalTileChunks =
                    &frameConfig->chunks[frameConfig->tileChunkTemporalIndex[plane]];

                for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                    if (globalConfig->perTileCompressionEnabled) {
                        VNCheckB(tiledRLEDecoderRead(
                            &entropyEnabledRLE, (uint8_t*)&temporalTileChunks[tile].entropyEnabled));
                    } else {
                        VNCheckB(bitstreamReadBit(&entropyEnabledBS,
                                                  (uint8_t*)&temporalTileChunks[tile].entropyEnabled));
                    }
                }
            }
        }

        if (globalConfig->perTileCompressionEnabled == 0) {
            /* Move bytestream forward with byte alignment */
            bytestreamSeek(stream, bitstreamGetConsumedBytes(&entropyEnabledBS));
        }

        /* --- Read chunk data --- */

        VNLogVerbose("  Entropy Signal");
        VNLogVerbose("  [Plane, LOQ, Layer, Tile] ");
        for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
            if (frameConfig->entropyEnabled) {
                for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
                    const uint32_t currentTileCount = globalConfig->numTiles[plane][loq];

                    for (uint8_t layer = 0; layer < globalConfig->numLayers; ++layer) {
                        if (globalConfig->tileSizeCompression != TCSPTTNone) {
                            /* Determine number of chunks enabled to know how many
                             * sizes we want to decode. */
                            uint32_t numChunksEnabled = 0;

                            for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                                const uint32_t chunkIndex = getLayerChunkIndex(
                                    frameConfig, globalConfig, (LdeLOQIndex)loq, plane, tile, layer);
                                LdeChunk* chunk = &frameConfig->chunks[chunkIndex];
                                numChunksEnabled += chunk->entropyEnabled;
                            }

                            VNCheckB(tiledSizeDecoderInitialize(
                                frameConfig->allocator, &sizeDecoder, numChunksEnabled, stream,
                                globalConfig->tileSizeCompression, globalConfig->bitstreamVersion));
                        }

                        for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                            const int32_t chunkIndex = getLayerChunkIndex(
                                frameConfig, globalConfig, (LdeLOQIndex)loq, plane, tile, layer);
                            LdeChunk* chunk = &frameConfig->chunks[chunkIndex];

                            VNLogVerbose("    [%d, %d, %2d, %3d] chunk %-4d: ", plane, loq, layer,
                                         tile, chunkIndex);
                            VNCheckB(parseChunk(stream, chunk, &frameConfig->loqEnabled[loq], sizeDecoderPtr));
                        }
                    }

                    VNLogVerbose("    %s enabled: %s", loqIndexToString((LdeLOQIndex)loq),
                                 frameConfig->loqEnabled[loq] ? "true" : "false");
                }
            }

            if (frameConfig->temporalSignallingPresent) {
                const uint32_t currentTileCount = globalConfig->numTiles[plane][LOQ0];
                LdeChunk* temporalTileChunks =
                    &frameConfig->chunks[frameConfig->tileChunkTemporalIndex[plane]];

                if (globalConfig->tileSizeCompression != TCSPTTNone) {
                    uint32_t numChunksEnabled = 0;

                    for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                        numChunksEnabled += temporalTileChunks[tile].entropyEnabled;
                    }

                    VNCheckB(tiledSizeDecoderInitialize(
                        frameConfig->allocator, &sizeDecoder, numChunksEnabled, stream,
                        globalConfig->tileSizeCompression, globalConfig->bitstreamVersion));
                }
                for (uint32_t tile = 0; tile < currentTileCount; ++tile) {
                    VNLogVerbose("    temporal: [%d, %3u]: ", plane, tile);
                    VNCheckB(parseChunk(stream, &temporalTileChunks[tile],
                                        &frameConfig->loqEnabled[LOQ0], sizeDecoderPtr));
                }
            }
        }

        tiledSizeDecoderRelease(sizeDecoderPtr);
    }

    return true;
}

static bool parseBlockFiller(ByteStream* stream, size_t blockSize)
{
    /* Skip block. */
    return bytestreamSeek(stream, blockSize);
}

static bool parseSEIPayload(ByteStream* stream, LdeGlobalConfig* globalConfig, uint32_t blockSize)
{
    LdeHDRInfo* hdrInfoOut = &globalConfig->hdrInfo;
    uint8_t data;
    VNCheckB(bytestreamReadU8(stream, &data));
    SEIPayloadType payloadType = (SEIPayloadType)data;
    VNLogVerbose("    sei_payload_type: %s", seiPayloadTypeToString(payloadType));

    if (payloadType == SPT_MasteringDisplayColourVolume) {
        /* D.2.2 */

        /* Write values to hdrInfoOut */
        LdeMasteringDisplayColorVolume* colorInfo = &hdrInfoOut->masteringDisplay;

        for (uint8_t i = 0; i < VN_MDCV_NUM_PRIMARIES; ++i) {
            VNCheckB(bytestreamReadU16(stream, &colorInfo->displayPrimariesX[i]));
            VNCheckB(bytestreamReadU16(stream, &colorInfo->displayPrimariesY[i]));

            VNLogVerbose("      display primaries: index %u - x=%u, y=%u", i,
                         colorInfo->displayPrimariesX[i], colorInfo->displayPrimariesY[i]);
        }

        VNCheckB(bytestreamReadU16(stream, &colorInfo->whitePointX));
        VNCheckB(bytestreamReadU16(stream, &colorInfo->whitePointY));
        VNCheckB(bytestreamReadU32(stream, &colorInfo->maxDisplayMasteringLuminance));
        VNCheckB(bytestreamReadU32(stream, &colorInfo->minDisplayMasteringLuminance));

        VNLogVerbose("      white point x: %u", colorInfo->whitePointX);
        VNLogVerbose("      white point y: %u", colorInfo->whitePointY);
        VNLogVerbose("      max display mastering luminance: %u", colorInfo->maxDisplayMasteringLuminance);
        VNLogVerbose("      min display mastering luminance: %u", colorInfo->minDisplayMasteringLuminance);

        hdrInfoOut->flags |= HDRMasteringDisplayColourVolumePresent;
    } else if (payloadType == SPT_ContentLightLevelInfo) {
        /* D.2.3 */

        /* Write values to hdrInfoOut */
        ContentLightLevel* lightLevel = &hdrInfoOut->contentLightLevel;

        VNCheckB(bytestreamReadU16(stream, &lightLevel->maxContentLightLevel));
        VNCheckB(bytestreamReadU16(stream, &lightLevel->maxPicAverageLightLevel));

        VNLogVerbose("      max content light level: %u", lightLevel->maxContentLightLevel);
        VNLogVerbose("      max pic average light level: %u", lightLevel->maxPicAverageLightLevel);

        hdrInfoOut->flags |= HDRContentLightLevelInfoPresent;
    } else if (payloadType == SPT_UserDataRegistered) {
        /* D.2.4 */

        /* Write values to deserialisedOut and its vnovaConfig (IF present in stream) */
        uint8_t ituHeader[ITUC_Length] = {0};

        VNCheckB(bytestreamReadU8(stream, &ituHeader[0]));

        /* Check for UK country code first. */
        if (ituHeader[0] != kVNovaITU[0]) {
            return bytestreamSeek(stream, blockSize - 1);
        }

        VNCheckB(bytestreamReadU8(stream, &ituHeader[1]));
        VNCheckB(bytestreamReadU8(stream, &ituHeader[2]));
        VNCheckB(bytestreamReadU8(stream, &ituHeader[3]));

        if (memcmp(ituHeader, kVNovaITU, ITUC_Length) != 0) {
            return bytestreamSeek(stream, blockSize - ITUC_Length);
        }

        VNLogVerbose("      V-Nova SEI Payload Found");
        if (globalConfig->bitstreamVersionSet) {
            VNCheckB(bytestreamSeek(stream, 1));
            /* Stream shouldn't provide version more than once, but it's technically not bad if it
             * does, so just do a debug rather than full warning. */
            VNLogDebug(
                "      Ignoring version. Version was either set to %u by the config, or else the "
                "stream is providing bad version data (i.e. multiple times, or too late)",
                globalConfig->bitstreamVersion);
        } else {
            VNCheckB(bytestreamReadU8(stream, &globalConfig->bitstreamVersion));
            globalConfig->bitstreamVersionSet =
                (globalConfig->bitstreamVersion >= BitstreamVersionInitial &&
                 globalConfig->bitstreamVersion <= BitstreamVersionCurrent);
            if (!globalConfig->bitstreamVersionSet) {
                VNLogError("Unsupported bitstream version detected %d, supported versions are "
                           "between %d and %d",
                           globalConfig->bitstreamVersion, BitstreamVersionInitial,
                           BitstreamVersionCurrent);
                return false;
            }
            VNLogDebug("      Bitstream version: %u", globalConfig->bitstreamVersion);
        }
    } else {
        VNLogWarning("      unsupported SEI payload type, skipping %d bytes", blockSize - 1);
        return bytestreamSeek(stream, blockSize - 1);
    }

    return true;
}

/* E.2 */
static bool parseVUIParameters(ByteStream* stream, LdeVUIInfo* vuiInfo, uint32_t vuiSize)
{
    uint8_t bit = 0;
    int32_t bits = 0;

    BitStream bitstream = {{0}};
    VNCheckB(bitstreamInitialize(&bitstream, bytestreamCurrent(stream), vuiSize));

    /* aspect_ratio_info_present_flag: 1 bit */
    VNCheckB(bitstreamReadBit(&bitstream, &bit));
    VNLogVerbose("    aspect_ratio_info_present: %d", bit);

    if (bit) {
        vuiInfo->flags |= VUIAspectRatioInfoPresent;

        /* aspect_ratio_idc: 8 bits */
        VNCheckB(bitstreamReadBits(&bitstream, 8, &bits));
        vuiInfo->aspectRatioIdc = (uint8_t)bits;
        VNLogVerbose("      aspect_ratio_idc: %u", vuiInfo->aspectRatioIdc);

        if (vuiInfo->aspectRatioIdc == kVUIAspectRatioIDCExtendedSAR) {
            /* sar_width: 16 bits */
            VNCheckB(bitstreamReadBits(&bitstream, 16, &bits));
            vuiInfo->sarWidth = (uint16_t)bits;

            /* sar_height: 16 bits */
            VNCheckB(bitstreamReadBits(&bitstream, 16, &bits));
            vuiInfo->sarHeight = (uint16_t)bits;

            VNLogVerbose("      sar_width: %u", vuiInfo->sarWidth);
            VNLogVerbose("      sar_height: %u", vuiInfo->sarHeight);
        }
    }

    /* overscan_info_present_flag: 1 bit */
    VNCheckB(bitstreamReadBit(&bitstream, &bit));
    VNLogVerbose("    overscan_info_present: %d", bit);

    if (bit) {
        vuiInfo->flags |= VUIOverscanInfoPresent;

        /* overscan_appropraite_flag: 1 bit*/
        VNCheckB(bitstreamReadBit(&bitstream, &bit));
        if (bit) {
            vuiInfo->flags |= VUIOverscanAppropriate;
        }

        VNLogVerbose("      overscan_appropriate: %d", bit);
    }

    /* video_signal_type_present_flag: 1 bit */
    VNCheckB(bitstreamReadBit(&bitstream, &bit));
    VNLogVerbose("    video_signal_type: %d", bit);

    if (bit) {
        vuiInfo->flags |= VUIVideoSignalTypePresent;

        /* video_format: 3 bits */
        VNCheckB(bitstreamReadBits(&bitstream, 3, &bits));
        vuiInfo->videoFormat = (LdeVUIVideoFormat)bits;
        VNLogVerbose("      video_format: %u", (uint8_t)vuiInfo->videoFormat);

        /* video_full_range_flag: 1 bit */
        VNCheckB(bitstreamReadBit(&bitstream, &bit));
        if (bit) {
            vuiInfo->flags |= VUIVideoSignalFullRangeFlag;
        }
        VNLogVerbose("      video_full_range: %d", bit);

        /* colour_description_present_flag: 1 bit */
        VNCheckB(bitstreamReadBit(&bitstream, &bit));
        VNLogVerbose("      colour_description_present: %d", bit);

        if (bit) {
            vuiInfo->flags |= VUIVideoSignalColorDescPresent;

            /* colour_primaries: 8 bits */
            VNCheckB(bitstreamReadBits(&bitstream, 8, &bits));
            vuiInfo->colourPrimaries = (uint8_t)bits;

            /* transfer_characteristics: 8 bits */
            VNCheckB(bitstreamReadBits(&bitstream, 8, &bits));
            vuiInfo->transferCharacteristics = (uint8_t)bits;

            /* matrix_coefficients: 8 bits*/
            VNCheckB(bitstreamReadBits(&bitstream, 8, &bits));
            vuiInfo->matrixCoefficients = (uint8_t)bits;

            VNLogVerbose("        colour_primaries: %u", vuiInfo->colourPrimaries);
            VNLogVerbose("        transfer_characteristics: %u", vuiInfo->transferCharacteristics);
            VNLogVerbose("        matrix_coefficients: %u", vuiInfo->matrixCoefficients);
        }
    }

    /* chroma_loc_info_present_flag: 1 bit*/
    VNCheckB(bitstreamReadBit(&bitstream, &bit));
    VNLogVerbose("    chroma_loc_info_present: %d", bit);

    if (bit) {
        vuiInfo->flags |= VUIChromaLocInfoPresent;

        /* chroma_sample_loc_type_top_field: ue(v) */
        VNCheckB(bitstreamReadExpGolomb(&bitstream, &vuiInfo->chromaSampleLocTypeTopField));

        /* chroma_sample_loc_type_bottom_field: ue(v) */
        VNCheckB(bitstreamReadExpGolomb(&bitstream, &vuiInfo->chromaSampleLocTypeBottomField));

        VNLogVerbose("      chroma_sample_loc_type_top_field: %u", vuiInfo->chromaSampleLocTypeTopField);
        VNLogVerbose("      chroma_sample_loc_type_bottom_field: %u",
                     vuiInfo->chromaSampleLocTypeBottomField);
    }

    /* Finally seek the byte-stream forward */
    return bytestreamSeek(stream, vuiSize);
}

static bool parseSFilterPayload(ByteStream* stream, LdeFrameConfig* frameConfig)
{
    uint8_t sfilterByte;
    VNCheckB(bytestreamReadU8(stream, &sfilterByte));

    frameConfig->sharpenType = (LdeSharpenType)((sfilterByte & 0xE0) >> 5);
    const uint8_t signalledSharpenStrength = (sfilterByte & 0x1F);
    frameConfig->sharpenStrength = (signalledSharpenStrength + 1) * 0.01f;
    VNLogVerbose("    sharpen_type: %s", sharpenTypeToString(frameConfig->sharpenType));
    VNLogVerboseF("    sharpen_strength: %d [%f]", signalledSharpenStrength, frameConfig->sharpenStrength);
    return true;
}

static bool parseHDRPayload(ByteStream* stream, LdeGlobalConfig* globalConfig)
{
    LdeHDRInfo* hdrInfoOut = &globalConfig->hdrInfo;
    LdeDeinterlacingInfo* deinterlacingInfoOut = &globalConfig->deinterlacingInfo;
    uint8_t byte = 0;

    VNCheckB(bytestreamReadU8(stream, &byte));

    /* tone_mapper_location: 1 bit */
    uint8_t toneMapperLocation = (byte >> 7) & 0b1;
    VNLogVerbose("    tone_mapper_location: %u", toneMapperLocation);
    /* tone_mapper_type: 5 bit */
    uint8_t toneMapperType = (byte >> 2) & 0b11111;
    VNLogVerbose("    tone_mapper_type: %u", toneMapperType);
    /* tone_mapper_data_present_flag: 1 bit */
    uint8_t toneMapperDataPresentFlag = (byte >> 1) & 0b1;
    VNLogVerbose("    tone_mapper_data_present_flag: %u", toneMapperDataPresentFlag);
    /* deinterlacer_enabled_flag: 1 bit */
    uint8_t deinterlacerEnabledFlag = byte & 0b1;
    VNLogVerbose("    deinterlacer_enabled_flag: %u", deinterlacerEnabledFlag);

    if (toneMapperDataPresentFlag) {
        /*  tone_mapper.size: multibyte */
        uint64_t toneMapperSize = 0;
        VNCheckB(bytestreamReadMultiByte(stream, &toneMapperSize));
        VNLogVerbose("        tone_mapper_size: %" PRIu64 "", toneMapperSize);
        /*  tone_mapper.payload: tone_mapper.size */
        // Skip tonemapper data as not supported yet
        VNCheckB(bytestreamSeek(stream, (size_t)toneMapperSize));
    }
    if (toneMapperType == 31) {
        /* tone_mapper_type_extended: 8 bit */
        VNCheckB(bytestreamReadU8(stream, &toneMapperType));
        VNLogVerbose("        tone_mapper_type_extended: %u", toneMapperType);
    }
    int8_t deinterlacerType = -1;
    uint8_t topFieldFirstFlag = 0;
    if (deinterlacerEnabledFlag) {
        VNCheckB(bytestreamReadU8(stream, &byte));

        /* deinterlacer_type: 4 bit */
        deinterlacerType = (byte >> 4) & 0b1111;
        VNLogVerbose("        deinterlacer_type: %u", deinterlacerType);
        /* top_field_first_flag: 1 bit */
        topFieldFirstFlag = (byte >> 3) & 0b1;
        VNLogVerbose("        top_field_first_flag: %u", topFieldFirstFlag);
        /* reserved_zeros_3bit: 3 bit */
        if (byte & 0b111) {
            VNLogError("hdr_payload_global_config: reserved_zeros_3bit is non zero");
            return false;
        }
    }

    // Write output
    hdrInfoOut->flags |= HDRPayloadGlobalConfigPresent;
    hdrInfoOut->tonemapperConfig[toneMapperLocation].type = toneMapperType;
    if (toneMapperDataPresentFlag) {
        hdrInfoOut->flags |= HDRToneMapperDataPresent;
    }
    if (deinterlacerEnabledFlag) {
        hdrInfoOut->flags |= HDRDeinterlacerEnabled;
        deinterlacingInfoOut->deinterlacerType = deinterlacerType;
        deinterlacingInfoOut->topFieldFirstFlag = topFieldFirstFlag;
    }
    return false;
}

/* 7.3.10 (Table-14) */
static bool parseBlockAdditionalInfo(ByteStream* stream, uint32_t blockSize, LdeGlobalConfig* globalConfig,
                                     LdeFrameConfig* frameConfig, bool* globalConfigModified)
{
    if (blockSize == 0) {
        VNLogError("stream: Additional info block size is 0, this is not possible in the standard");
        return false;
    }

    uint8_t byte = 0;
    VNCheckB(bytestreamReadU8(stream, &byte));
    const AdditionalInfoType infoType = (AdditionalInfoType)byte;
    VNLogVerbose("  additional_info_type: %s", additionalInfoTypeToString(infoType));

    switch (infoType) {
        case AIT_SEI:
            *globalConfigModified = true;
            VNCheckB(parseSEIPayload(stream, globalConfig, blockSize - 1));
            break;
        case AIT_VUI:
            VNCheckB(parseVUIParameters(stream, &globalConfig->vuiInfo, blockSize - 1));
            break;
        case AIT_SFilter: VNCheckB(parseSFilterPayload(stream, frameConfig)); break;
        case AIT_HDR:
            *globalConfigModified = true;
            VNCheckB(parseHDRPayload(stream, globalConfig));
            break;
        default:
            VNLogWarning("    unsupported additional info type, skipping %d bytes", blockSize - 1);
            return bytestreamSeek(stream, blockSize - 1);
    }

    return true;
}

/*------------------------------------------------------------------------------*/

static bool parseBlock(ByteStream* stream, LdeGlobalConfig* globalConfig,
                       LdeFrameConfig* frameConfig, bool* globalConfigModified)
{
    /* Load block header. */
    uint8_t data;
    VNCheckB(bytestreamReadU8(stream, &data));
    const BlockType blockType = (BlockType)(data & 0x1F);
    const SignalledBlockSize blockSizeSignal = (SignalledBlockSize)((data & 0xE0) >> 5);

    /* Determine block byte size. */
    uint32_t blockSize = 0;

    if (blockSizeSignal == BS_Custom) {
        uint64_t customBlockSize;
        VNCheckB(bytestreamReadMultiByte(stream, &customBlockSize));

        if (customBlockSize > 0xFFFFFFFF) {
            VNLogError(
                "stream: Invalid custom block size, expect < 32-bits used, value is: %" PRIu64 "",
                customBlockSize);
            return false;
        }

        blockSize = (uint32_t)customBlockSize;
    } else {
        VNCheckB(blockSizeFromEnum(blockSizeSignal, &blockSize));
    }

    /* Process each block. */
    const size_t initialOffset = stream->offset;

    VNLogVerbose("Block: %s - size: %u", blockTypeToString(blockType), blockSize);

    switch (blockType) {
        case BT_SequenceConfig:
            *globalConfigModified = true;
            VNCheckB(parseBlockSequenceConfig(stream, globalConfig));
            break;
        case BT_GlobalConfig:
            *globalConfigModified = true;
            frameConfig->globalConfigSet = true;
            VNCheckB(parseBlockGlobalConfig(stream, globalConfig));
            break;
        case BT_PictureConfig:
            VNCheckB(parseBlockPictureConfig(stream, frameConfig, globalConfig));
            break;
        case BT_EncodedData: VNCheckB(parseEncodedData(stream, frameConfig, globalConfig)); break;
        case BT_EncodedDataTiled:
            VNCheckB(parseEncodedDataTiled(stream, frameConfig, globalConfig));
            break;
        case BT_AdditionalInfo:
            VNCheckB(parseBlockAdditionalInfo(stream, blockSize, globalConfig, frameConfig,
                                              globalConfigModified));
            break;
        case BT_Filler: VNCheckB(parseBlockFiller(stream, blockSize)); break;
        default: {
            VNLogWarning("Unrecognized block type received, skipping: %u", (uint8_t)blockType);
            bytestreamSeek(stream, blockSize);
            break;
        }
    }

    VNLogVerbose("");

    /* Handle block misread. */
    if ((stream->offset - initialOffset) - blockSize) {
        VNLogError("Config parser: unparsed blocks of the stream remain. Initial offset: %u, "
                   "Current offset: %u, Expected offset: %u",
                   (uint32_t)initialOffset, (uint32_t)stream->offset,
                   (uint32_t)(initialOffset + blockSize));
        return false;
    }

    return true;
}

bool unencapsulate(const uint8_t* encapsulatedData, size_t encapsulatedSize,
                   uint8_t* unencapsulatedBuffer, size_t* unencapsulatedSize, bool* isIDR)
{
    uint8_t* head = unencapsulatedBuffer;
    uint8_t zeroes = 0;
    uint8_t byte = 0;
    size_t pos = 5; // Remove the first 5 bytes and last byte in while condition
    int32_t nalStartOffset = 3;

    /* NAL Unit Header checks - MPEG-5 Part 2 LCEVC standard - 7.3.2 (Table-6) & 7.4.2.2 */
    if (encapsulatedData[encapsulatedSize - 1] != 0x80) {
        VNLogError("Malformed NAL unit: missing RBSP stop-bit");
        return false;
    }

    if (encapsulatedData[0] != 0 || encapsulatedData[1] != 0 || encapsulatedData[2] != 1) {
        if (encapsulatedData[0] != 0 || encapsulatedData[1] != 0 || encapsulatedData[2] != 0 ||
            encapsulatedData[3] != 1) {
            VNLogError("Malformed prefix: start code [0, 0, 1] or [0, 0, 0, 1] not found\n");
            return false;
        }
        nalStartOffset = 4;
        pos++;
    }

    /*  forbidden_zero_bit   u(1)
        forbidden_one_bit    u(1)
        nal_unit_type        u(5)
        reserved_flag        u(9) */

    /* forbidden bits and reserved flag */
    if ((encapsulatedData[nalStartOffset] & 0xC1) != 0x41 || encapsulatedData[nalStartOffset + 1] != 0xFF) {
        VNLogError("Malformed NAL unit header: forbidden bits or reserved flags not as expected");
        return false;
    }

    LdeNALType type = (LdeNALType)((encapsulatedData[nalStartOffset] & 0x3E) >> 1);
    if (type != NTNonIDR && type != NTIDR) {
        VNLogError("Unrecognized LCEVC NAL type, it should be IDR or NonIDR");
        return false;
    }
    *isIDR = (type == NTIDR) ? true : false;

    while (pos < encapsulatedSize - 1) {
        byte = *(encapsulatedData + pos);
        pos++;

        if (zeroes == 2 && byte == 3) {
            zeroes = 0;
            continue; /* skip it */
        }

        if (byte == 0) {
            ++zeroes;
        } else {
            zeroes = 0;
        }

        *(head++) = byte;
    }
    *unencapsulatedSize = (size_t)(head - unencapsulatedBuffer);

    return true;
}

/*------------------------------------------------------------------------------*/

void ldeGlobalConfigInitialize(uint8_t forceBitstreamVersion, LdeGlobalConfig* globalConfig)

{
    memset(globalConfig, 0, sizeof(LdeGlobalConfig));

    /* If the bitstream version has been specified, set it immediately because it will affect
     * the parsing process. */
    if (forceBitstreamVersion == BitstreamVersionUnspecified) {
        /* If the bitstream version is unspecified it should be picked up from the stream in the
         * first SEIPayload, not standardised - currently only supported by V-Nova SDK encoder.
         * Safe to assume the current version otherwise. */
        globalConfig->bitstreamVersionSet = false;
        globalConfig->bitstreamVersion = BitstreamVersionCurrent;
    } else {
        globalConfig->bitstreamVersionSet = true;
        globalConfig->bitstreamVersion = forceBitstreamVersion;
    }

    /* Set defaults. Many of these are determined by the standard. */
    globalConfig->chroma = CT420;
    globalConfig->baseDepth = Depth8;
    globalConfig->enhancedDepth = Depth8;
    globalConfig->upscale = USLinear;
    globalConfig->scalingModes[LOQ0] = Scale2D;
    globalConfig->scalingModes[LOQ1] = Scale0D;
    globalConfig->chromaStepWidthMultiplier = kDefaultChromaStepWidthMultiplier;
    globalConfig->temporalStepWidthModifier = kDefaultTemporalStepWidthModifier;
}

void ldeFrameConfigInitialize(LdcMemoryAllocator* allocator, LdeFrameConfig* frameConfig)
{
    frameConfig->allocator = allocator;
    frameConfig->pictureType = PTFrame;
    frameConfig->ditherType = DTNone;
    frameConfig->ditherStrength = 0;
    frameConfig->deblockEnabled = false; /* 7.4.3.4 */
    for (uint8_t loq = 0; loq < LOQEnhancedCount; loq++) {
        memset(frameConfig->quantMatrix.values[loq], 0,
               RCLayerCountDDS * sizeof(frameConfig->quantMatrix.values[0][0]));
    }
    frameConfig->quantMatrix.set = false;
}

void ldeConfigsReleaseFrame(LdeFrameConfig* frameConfig)
{
    if (frameConfig->chunks) {
        VNFree(frameConfig->allocator, &frameConfig->chunkAllocation);
        frameConfig->chunks = NULL;
    }
    if (VNIsAllocated(frameConfig->unencapsulatedAllocation)) {
        VNFree(frameConfig->allocator, &frameConfig->unencapsulatedAllocation);
    }
}

void ldeConfigReset(LdeFrameConfig* frameConfig)
{
    frameConfig->globalConfigSet = false;
    frameConfig->frameConfigSet = false;
    frameConfig->numChunks = 0;
    frameConfig->loqEnabled[LOQ0] = false;
    frameConfig->loqEnabled[LOQ1] = false;
}

bool ldeConfigsParse(const uint8_t* serialized, const size_t serializedSize, LdeGlobalConfig* globalConfig,
                     LdeFrameConfig* frameConfig, bool* globalConfigModified)
{
    if (!serialized || !serializedSize) {
        VNLogError("Serialised NULL or no size");
        return false;
    }

    // Unencapsulate - output size will always be the same or smaller
    size_t unencapsulatedSize = 0;
    bool idr = false;
    if (VNIsAllocated(frameConfig->unencapsulatedAllocation)) {
        VNFree(frameConfig->allocator, &frameConfig->unencapsulatedAllocation);
    }
    uint8_t* unencapsulated = VNAllocateZeroArray(
        frameConfig->allocator, &frameConfig->unencapsulatedAllocation, uint8_t, serializedSize);

    if (!unencapsulate(serialized, serializedSize, unencapsulated, &unencapsulatedSize, &idr)) {
        VNLogError("Unencapsulation failed during NAL unit parsing");
        VNFree(frameConfig->allocator, &frameConfig->unencapsulatedAllocation);
        return false;
    }

    VNLogVerbose("------>>> Begin deserialize");

    ByteStream stream;
    *globalConfigModified = false;
    frameConfig->frameConfigSet = false;
    frameConfig->globalConfigSet = false;
    frameConfig->nalType = idr ? NTIDR : NTNonIDR;
    frameConfig->loqEnabled[LOQ0] = false;
    frameConfig->loqEnabled[LOQ1] = false;
    frameConfig->numChunks = 0;

    if (!bytestreamInitialize(&stream, unencapsulated, unencapsulatedSize)) {
        VNFree(frameConfig->allocator, &frameConfig->unencapsulatedAllocation);
        return false;
    }

    while (bytestreamRemaining(&stream) > 0) {
        if (!parseBlock(&stream, globalConfig, frameConfig, globalConfigModified)) {
            VNFree(frameConfig->allocator, &frameConfig->unencapsulatedAllocation);
            return false;
        }
    }

    return true;
}

/*------------------------------------------------------------------------------*/
