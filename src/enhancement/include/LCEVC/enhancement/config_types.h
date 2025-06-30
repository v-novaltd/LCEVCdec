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

#ifndef VN_LCEVC_ENHANCEMENT_CONFIG_TYPES_H
#define VN_LCEVC_ENHANCEMENT_CONFIG_TYPES_H

#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/hdr_types.h>

/*! \brief A single layer of encoded data, either huffman or run-length encoded */
typedef struct LdeChunk
{
    uint8_t rleOnly;
    size_t size;
    const uint8_t* data;
    bool entropyEnabled;
} LdeChunk;

/*! \brief Parameters that are global to a stream. For standard streams these might be constant for
 * the entire stream. This config will certainly not change within a GOP. Closely follows
 * section 7.3.5 (Table 9) of the LCEVC MPEG-5 Part 2 standard.
 */
typedef struct LdeGlobalConfig
{
    bool initialized; /**< Tracks if the config has been initialized */
    bool bitstreamVersionSet; /**< Tracks if the version has been set, if not set default to the newest version */
    uint8_t bitstreamVersion; /**< After revisions of the LCEVC standard, tracking the version of a bitstream allows older streams to be played correctly */

    LdeChroma chroma; /**< Chroma subsampling of the LCEVC stream for picture size operations */
    LdeBitDepth baseDepth;     /**< Bit depth of the base picture */
    LdeBitDepth enhancedDepth; /**< Bit depth of the output (enhanced) picture */
    bool loq1UseEnhancedDepth; /**< Use `enhancedDepth` for residuals on the base layer */

    uint16_t width; /**< Pixel width of the frame */
    uint16_t height; /**< Pixel height of the frame (progressive - doesn't account of interlaced fields) */
    LdeUpscaleType upscale; /**< Algorithm to use for upscaling */

    LdeScalingMode scalingModes[LOQEnhancedCount]; /**< Dimensional scaling modes to use for each LOQ */

    uint8_t numPlanes; /**< Number of enhanced planes - pictures may have chroma planes without residuals */
    bool temporalEnabled;         /**< Enable temporal decoding and use of the temporal plane */
    bool predictedAverageEnabled; /**< Enable the PA feature during upscaling */
    bool temporalReducedSignallingEnabled; /**< Enable 'reduced signalling' for temporal block clears */
    LdeTransformType transform;            /**< Specify inverse Hadamard type - DD or DDS */
    uint8_t numLayers;                     /**< Number of Huffman layers (chunks) */

    uint8_t temporalStepWidthModifier; /**< Temporal modifier constant for use in dequantization */
    uint8_t chromaStepWidthMultiplier; /**< Chroma modifier constant for use in dequantization */
    LdeKernel kernel;                  /**< Upscaling kernel */
    LdeDeblock deblock;                /**< Deblocking filter constants */
    bool cropEnabled; /**< Enable cropping at the edge of the picture for very non-standard resolutions */
    LdeCrop crop; /**< Crop amounts from each edge of the picture if cropping is enabled */
    LdeUserDataConfig userData; /**< Some user data can be tied to each global config - not much use for this yet */

    LdeHDRInfo hdrInfo; /**< HDR parameters defined in annex D and E of the standard */
    LdeVUIInfo vuiInfo; /**< VUI parameters storage */
    LdeDeinterlacingInfo deinterlacingInfo; /**< Deinterlacing parameters for interlaced HDR streams */

    LdeTileDimensions tileDimensions; /**< Dimensions of tiled pictures */
    uint16_t tileWidth[RCMaxPlanes];  /**< Tile width of each plane */
    uint16_t tileHeight[RCMaxPlanes]; /**< Tile height of each plane */
    LdeTileCompressionSizePerTile tileSizeCompression; /**< Tracks if the custom tile sizes themselves are compressed - not needed past config parsing */
    bool perTileCompressionEnabled; /**< True if each tile is encoded separately */
    uint32_t numTiles[RCMaxPlanes][LOQEnhancedCount]; /**< Helper to track the total number of tiles on each LOQ and plane */
} LdeGlobalConfig;

typedef struct LdeFrameConfig
{
    bool frameConfigSet;  /**< Tracks if the config has been initialized */
    bool globalConfigSet; /**< Set if Global Config was also updated for this frame */

    LdcMemoryAllocator* allocator; /**< Allocator for chunks and underlying unencapsulated stream */
    LdcMemoryAllocation chunkAllocation;          /**< Memory allocation for chunks */
    LdcMemoryAllocation unencapsulatedAllocation; /**< Memory allocation raw LCEVC data */
    uint32_t numChunks;                           /**< Number of huffman chunks (layers) */
    LdeChunk* chunks;                             /**< Pointer to huffman chunks (layers) */

    LdeNALType nalType; /**< Flag for IDR frames */
    LdePictureType pictureType; /**< Flag for interlaced or progressive LCEVC data - doesn't necessarily match the base */
    LdeFieldType fieldType; /**< Flag for the top or bottom interlaced field, only for interlaced picType */
    bool entropyEnabled; /**< Primarily an internal flag for signaling other parsed parameters, use `loqEnabled` for high level enhancement on/off checks */
    bool temporalRefresh; /**< True if the temporal plane should be reset at the start of this frame */
    bool temporalSignallingPresent;    /**< Internal flag for locating the temporal chunk */
    bool loqEnabled[LOQEnhancedCount]; /**< Holds whether residuals are enabled on LOQ1 and LOQ0 */

    uint32_t tileChunkResidualIndex[RCMaxPlanes][LOQEnhancedCount]; /**< Helper for finding the correct residual chunk for a given LOQ, plane, tile */
    uint32_t tileChunkTemporalIndex[RCMaxPlanes]; /**< Helper for finding the correct temporal chunk for a given plane and tile */

    LdeQuantMatrix quantMatrix; /**< The quantization matrix required for dequant functions */
    int32_t stepWidths[LOQEnhancedCount]; /**< Encoded step widths for each LOQ required for dequant functions */
    LdeDequantOffsetMode dequantOffsetMode; /**< Mode toggle required for dequant functions */
    int32_t dequantOffset;                  /**< Offset constant required for dequant functions */
    bool deblockEnabled;                    /**< Flag to enable deblocking */

    bool ditherEnabled;         /**< Flag to enable dithering */
    LdeDitherType ditherType;   /**< Dithering mode, if enabled */
    uint8_t ditherStrength;     /**< Dithering strength of the frame, if enabled */
    LdeSharpenType sharpenType; /**< Sharpening type */
    float sharpenStrength;      /**< Sharpening strength of the frame, if enabled */
} LdeFrameConfig;

#endif // VN_LCEVC_ENHANCEMENT_CONFIG_TYPES_H
