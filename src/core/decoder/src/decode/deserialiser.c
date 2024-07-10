/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "decode/deserialiser.h"

#include "common/bitstream.h"
#include "common/bytestream.h"
#include "common/memory.h"
#include "common/types.h"
#include "context.h"
#include "decode/dequant.h"
#include "decode/entropy.h"
#include "surface/sharpen.h"

#include <assert.h>
#include <stdio.h>

#define VN_ENABLE_DEBUG_SYNTAX() 0
#if VN_ENABLE_DEBUG_SYNTAX()
#define VN_DEBUG_SYNTAX(...) VN_DEBUG(ctx->log, __VA_ARGS__)
#else
#define VN_DEBUG_SYNTAX(...)
#endif

/*------------------------------------------------------------------------------
 Syntax functionality
 ------------------------------------------------------------------------------*/

typedef enum SignalledBlockSize
{
    BS_0,
    BS_1,
    BS_2,
    BS_3,
    BS_4,
    BS_5,
    BS_Reserved1,
    BS_Custom
} SignalledBlockSize_t;

static inline int32_t blockSizeFromEnum(SignalledBlockSize_t type, uint32_t* res)
{
    if ((type < BS_0) || (type > BS_5)) {
        return -1;
    }

    *res = (uint32_t)type;
    return 0;
}

typedef enum BlockType
{
    BT_SequenceConfig,
    BT_GlobalConfig,
    BT_PictureConfig,
    BT_EncodedData,
    BT_EncodedDataTiled,
    BT_AdditionalInfo,
    BT_Filler,
    BT_Count
} BlockType_t;

typedef enum AdditionalInfoType
{
    AIT_SEI = 0,
    AIT_VUI = 1,
    AIT_SFilter = 23,
    AIT_HDR = 25,
} AdditionalInfoType_t;

typedef enum SEIPayloadType
{
    SPT_MasteringDisplayColourVolume = 1,
    SPT_ContentLightLevelInfo = 2,
    SPT_UserDataRegistered = 4,
} SEIPayloadType_t;

typedef struct Resolution
{
    uint16_t width;
    uint16_t height;
} Resolution_t;

static const Resolution_t kResolutions[] = {
    {0, 0},       {360, 200},   {400, 240},   {480, 320},   {640, 360},   {640, 480},
    {768, 480},   {800, 600},   {852, 480},   {854, 480},   {856, 480},   {960, 540},
    {960, 640},   {1024, 576},  {1024, 600},  {1024, 768},  {1152, 864},  {1280, 720},
    {1280, 800},  {1280, 1024}, {1360, 768},  {1366, 768},  {1400, 1050}, {1440, 900},
    {1600, 1200}, {1680, 1050}, {1920, 1080}, {1920, 1200}, {2048, 1080}, {2048, 1152},
    {2048, 1536}, {2160, 1440}, {2560, 1440}, {2560, 1600}, {2560, 2048}, {3200, 1800},
    {3200, 2048}, {3200, 2400}, {3440, 1440}, {3840, 1600}, {3840, 2160}, {3840, 3072},
    {4096, 2160}, {4096, 3072}, {5120, 2880}, {5120, 3200}, {5120, 4096}, {6400, 4096},
    {6400, 4800}, {7680, 4320}, {7680, 4800},
};

static const uint32_t kResolutionCount = (sizeof(kResolutions) / sizeof(Resolution_t));
static const uint32_t kResolutionCustom = 63;

static const uint32_t kVUIAspectRatioIDCExtendedSAR = 255;
static const uint64_t kMaximumConformanceWindowValue = (1 << 16) - 1;

enum
{
    ITUC_Length = 4
};
static const uint8_t kVNovaITU[ITUC_Length] = {0xb4, 0x00, 0x50, 0x00};

/*------------------------------------------------------------------------------*/

#if VN_ENABLE_DEBUG_SYNTAX()

static inline const char* blockTypeToString(BlockType_t type)
{
    switch (type) {
        case BT_SequenceConfig: return "sequence_config";
        case BT_GlobalConfig: return "global_config";
        case BT_PictureConfig: return "picture_config";
        case BT_EncodedData: return "encoded_data";
        case BT_EncodedDataTiled: return "encoded_data_tiled";
        case BT_AdditionalInfo: return "additional_info";
        case BT_Filler: return "filler";
        case BT_Count: break;
    }

    return "Unknown";
}

static inline const char* additionalInfoTypeToString(AdditionalInfoType_t type)
{
    switch (type) {
        case AIT_SEI: return "sei";
        case AIT_VUI: return "vui";
        case AIT_SFilter: return "s_filter";
        case AIT_HDR: return "hdr";
    }

    return "Unknown";
}

static inline const char* seiPayloadTypeToString(SEIPayloadType_t type)
{
    switch (type) {
        case SPT_MasteringDisplayColourVolume: return "mastering_display_colour_volume";
        case SPT_ContentLightLevelInfo: return "content_light_level_info";
        case SPT_UserDataRegistered: return "user_data_registered";
    }

    return "Unknown";
}

#endif

/*------------------------------------------------------------------------------*/

/* NAL Unit Header - 7.3.2 (Table-6) & 7.4.2.2 */
static int32_t parseNALHeader(Logger_t log, DeserialisedData_t* data, ByteStream_t* stream)
{
    int32_t res = 0;
    uint8_t header[5] = {0};

    VN_CHECK(bytestreamReadN8(stream, header, 5));

    /* start-code (@todo: Proposal to remove this). */
    if (header[0] != 0x0 || header[1] != 0x0 || header[2] != 0x1) {
        VN_ERROR(log, "Malformed header, prefix bytes are not [0x0, 0x0, 0x1]\n");
        return -1;
    }

    /*  forbidden_zero_bit   u(1)
        forbidden_one_bit    u(1)
        nal_unit_type        u(5)
        reserved_flag        u(9) */

    /* forbidden bits and reserved flag */
    if ((header[3] & 0xC1) != 0x41 || header[4] != 0xFF) {
        VN_ERROR(log, "Malformed header: forbidden bits or reserved flags not as expected\n");
        return -1;
    }

    data->type = (NALType_t)((header[3] & 0x3E) >> 1);
    if (data->type != NTNonIDR && data->type != NTIDR) {
        VN_ERROR(log, "Unrecognized LCEVC nal type, it should be IDR or NonIDR\n");
        return -1;
    }

    return 0;
}

static int32_t unencapsulate(Memory_t memory, Logger_t log, DeserialisedData_t* data, ByteStream_t* stream)
{
    int32_t res = 0;

    /* Check for RBSP stop-bit - since LCEVC syntax is byte-aligned the bit will
     * be on the top-bit of the last byte (0x80) */
    stream->size -= 1;

    if (stream->data[stream->size] != 0x80) {
        VN_ERROR(log, "Malformed NAL unit: missing RBSP stop-bit\n");
    }

    if (parseNALHeader(log, data, stream) < 0) {
        return -1;
    }

    /* Cache the unencapsulation buffer. */
    if (stream->size > data->unencapsulatedCapacity) {
        uint8_t* newPtr = VN_REALLOC_T_ARR(memory, data->unencapsulatedData, uint8_t, stream->size);

        if (newPtr == NULL) {
            VN_ERROR(log, "unencapsulation buffer realloc returned NULL");
            VN_FREE(memory, data->unencapsulatedData);
            return -1;
        }

        data->unencapsulatedData = newPtr;
        data->unencapsulatedCapacity = stream->size;
    }

    uint8_t* head = data->unencapsulatedData;
    uint8_t zeroes = 0;
    uint8_t byte = 0;

    // @todo(bob): bytestreamReadU8 is doing a lot of unnecessary checks
    //             here, fix this so it's not using the byte stream at all.
    //

    while (bytestreamRemaining(stream) > 0) {
        res = bytestreamReadU8(stream, &byte);

        if (res < 0) {
            break;
        }

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

    if (res < 0) {
        data->unencapsulatedSize = 0;
    } else {
        data->unencapsulatedSize = (uint32_t)(head - data->unencapsulatedData);
    }

    return res;
}

/*------------------------------------------------------------------------------*/

/* @brief State for the RLE decoding of the compressed syntax for the chunk enabled
 *        flag.
 *
 * @note This is an identical scheme to the layers decoders temporal signalling
 *       decoder - however to use that here would require building up an actual
 *       layer decoder with huffman state & bitstream reader.
 */
typedef struct TiledRLEDecoder
{
    ByteStream_t* reader;
    uint8_t currentSymbol;
    uint64_t runLength;
} TiledRLEDecoder_t;

static int32_t tiledRLEDecoderInitialise(TiledRLEDecoder_t* decoder, ByteStream_t* reader)
{
    int32_t res = 0;
    decoder->reader = reader;

    /* Decode initial symbol and first run. */
    VN_CHECK(bytestreamReadU8(reader, &decoder->currentSymbol));

    if ((decoder->currentSymbol != 0x00) && (decoder->currentSymbol != 0x01)) {
        return -1;
    }

    VN_CHECK(bytestreamReadMultiByte(reader, &decoder->runLength));

    return res;
}

static int32_t tiledRLEDecoderRead(TiledRLEDecoder_t* decoder, uint8_t* destination)
{
    int32_t res = 0;

    if (decoder->runLength == 0) {
        /* Decode next run length and flip the symbol. */
        VN_CHECK(bytestreamReadMultiByte(decoder->reader, &decoder->runLength));
        decoder->currentSymbol = !decoder->currentSymbol;

        if (decoder->runLength == 0) {
            return -1;
        }
    }

    *destination = decoder->currentSymbol;
    decoder->runLength--;

    return res;
}

/*------------------------------------------------------------------------------*/

typedef struct TiledSizeDecoder
{
    Memory_t memory;
    int16_t* sizes;
    uint32_t currentIndex;
    uint32_t numSizes;
} TiledSizeDecoder_t;

static int32_t tiledSizeDecoderInitialise(Memory_t memory, Logger_t log, TiledSizeDecoder_t* decoder,
                                          uint32_t numSizes, ByteStream_t* stream,
                                          TileCompressionSizePerTile_t type, bool useOldCodeLengths)
{
    int32_t res = 0;

    const EntropyDecoderType_t decoderType = (type == TCSPTTPrefix) ? EDTSizeUnsigned : EDTSizeSigned;

    /* Do not attempted to read sizes if none are signaled. */
    if (numSizes == 0) {
        return res;
    }

    /* Allocate buffer to store the decoded sizes. */
    if (decoder->numSizes < numSizes) {
        int16_t* newSizes = VN_REALLOC_T_ARR(memory, decoder->sizes, int16_t, numSizes);

        if (!newSizes) {
            /* Clean up.*/
            VN_FREE(memory, decoder->sizes);
            decoder->sizes = NULL;

            VN_ERROR(log, "unable to allocate tile sizes buffer\n");
            return -1;
        }

        decoder->sizes = newSizes;
    }

    decoder->memory = memory;
    decoder->currentIndex = 0;
    decoder->numSizes = numSizes;

    /* Now parse the sizes */
    Chunk_t chunk = {0};
    chunk.entropyEnabled = 1;
    chunk.rleOnly = 0;
    chunk.data = bytestreamCurrent(stream);
    chunk.size = bytestreamRemaining(stream);

    EntropyDecoder_t layerDecoder = {0};
    VN_CHECK(entropyInitialise(log, &layerDecoder, &chunk, decoderType, useOldCodeLengths));

    VN_DEBUG_SYNTAX("Tiled size decoder\n");

    for (uint32_t i = 0; i < numSizes; ++i) {
        VN_CHECK(entropyDecodeSize(&layerDecoder, &decoder->sizes[i]));
        VN_DEBUG_SYNTAX("Size: %d\n", decoder->sizes[i]);
    }

    const uint32_t consumedBytes = entropyGetConsumedBytes(&layerDecoder);
    VN_DEBUG_SYNTAX("Consumed bytes: %u\n", consumedBytes);

    VN_CHECK(bytestreamSeek(stream, consumedBytes));

    if (type == TCSPTTPrefixOnDiff) {
        for (uint32_t i = 1; i < numSizes; ++i) {
            decoder->sizes[i] += decoder->sizes[i - 1];
        }
    }

    return res;
}

static void tiledSizeDecoderRelease(TiledSizeDecoder_t* decoder)
{
    if (decoder && decoder->sizes) {
        VN_FREE(decoder->memory, decoder->sizes);
    }
}

static int16_t tiledSizeDecoderRead(TiledSizeDecoder_t* decoder)
{
    if (decoder->currentIndex < decoder->numSizes) {
        return decoder->sizes[decoder->currentIndex++];
    }

    return -1;
}

/*------------------------------------------------------------------------------*/

static int32_t quantMatrixParseLOQ(ByteStream_t* stream, LOQIndex_t loq, DeserialisedData_t* output)
{
    uint8_t* values = quantMatrixGetValues(&output->quantMatrix, loq);

    for (int32_t i = 0; i < output->numLayers; i++) {
        if (bytestreamReadU8(stream, values) != 0) {
            return -1;
        }
        values++;
    }

    return 0;
}

static void quantMatrixDebugLog(const DeserialisedData_t* deserialised, LOQIndex_t loq)
{
#if !VN_ENABLE_DEBUG_SYNTAX()
    VN_UNUSED(deserialised);
    VN_UNUSED(loq);
#else
    const uint8_t* values = quantMatrixGetValues(&deserialised->quantMatrix, loq);

    if (deserialisedOut->transform == TransformDD) {
        VN_DEBUG_SYNTAX("  Quant-matrix LOQ-%u: %u %u %u %u\n", loq, values[0], values[1],
                        values[2], values[3]);
    } else if (deserialisedOut->transform == TransformDDS) {
        VN_DEBUG_SYNTAX("  Quant-matrix LOQ-%u: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
                        loq, values[0], values[1], values[2], values[3], values[4], values[5],
                        values[6], values[7], values[8], values[9], values[10], values[11],
                        values[12], values[13], values[14], values[15]);
    } else {
        VN_DEBUG_SYNTAX("  Unknown layer count for quant-matrix\n");
    }
#endif
}

static int32_t parseConformanceValue(ByteStream_t* stream, uint16_t* dst, const char* debugLabel)
{
    uint64_t value;
    int32_t res = 0;

    VN_UNUSED(debugLabel);

    VN_CHECK(bytestreamReadMultiByte(stream, &value));

    /* Validate values can be assigned to window args */
    if (value > kMaximumConformanceWindowValue) {
        return -1;
    }

    *dst = (uint16_t)value;
    VN_DEBUG_SYNTAX("Conformance window %s: %u\n", debugLabel, *dst);
    return res;
}

/*------------------------------------------------------------------------------*/

/*! \brief Updates the deserialised data with the correct tile dimensions for each
 *         plane.
 *
 * \note This is performed to ensure that there ends up being the same number of
 *       tiles per-plane independent of the chroma subsampling being performed. This
 *       may change in the future such that the tile size remains the same across
 *       all planes.
 *
 * \param data  Deserialised data to update the tile dimensions for.
 */
static int32_t calculateTileDimensions(DeserialisedData_t* data)
{
    int32_t hshift = 0;
    int32_t vshift = 0;

    switch (data->chroma) {
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
        case CTCount: return -1;
    }

    data->tileWidth[1] = data->tileWidth[2] = (uint16_t)(data->tileWidth[0] + hshift) >> hshift;
    data->tileHeight[1] = data->tileHeight[2] = (uint16_t)(data->tileHeight[0] + vshift) >> vshift;

    return 0;
}

/*! \brief Determines the number of whole and partial tiles across and down for
 *         each Plane and LOQ.
 *
 * \param log        Decoder context's logger
 * \param data       Deserialised data to obtain dimensions from
 *
 * \return 0 on success, otherwise -1
 */
static int32_t calculateTileCounts(Logger_t log, DeserialisedData_t* data)
{
    const int32_t tuSize = (data->transform == TransformDDS) ? 4 : 2;

    /* Determine appropriate number of tiles for each plane and loq. */
    for (int32_t plane = 0; plane < data->numPlanes; ++plane) {
        int32_t* tilesAcross = &data->tilesAcross[plane][0];
        int32_t* tilesDown = &data->tilesDown[plane][0];

        if ((data->tileWidth[plane] % tuSize) || (data->tileHeight[plane] % tuSize)) {
            VN_ERROR(log, "invalid stream: Tile dimensions must be divisible by transform size");
            return -1;
        }

        for (int32_t loq = 0; loq < LOQEnhancedCount; ++loq) {
            uint32_t loqWidth;
            uint32_t loqHeight;

            deserialiseCalculateSurfaceProperties(data, (LOQIndex_t)loq, plane, &loqWidth, &loqHeight);

            tilesAcross[loq] = divideCeilS32(loqWidth, data->tileWidth[plane]);
            tilesDown[loq] = divideCeilS32(loqHeight, data->tileHeight[plane]);

            data->tileCount[plane][loq] = tilesAcross[loq] * tilesDown[loq];

            VN_DEBUG_SYNTAX("  Tile count plane %u LOQ-%u: %dx%d (%d)\n", plane, loq,
                            tilesAcross[loq], tilesDown[loq], data->tileCount[plane][loq]);

            /* As it is currently intended that all planes at a given LOQ have the
             * same number of tiles, ensure that is the case. */
            if ((plane > 1) && (data->tileCount[plane][loq] != data->tileCount[0][loq])) {
                VN_ERROR(log, "Invalid tile counts calculated. Each plane should have the "
                              "same number of tiles\n");
                return -1;
            }
        }
    }

    return 0;
}

static inline void calculateTileChunkIndices(DeserialisedData_t* data)
{
    int32_t offset = 0;

    memset(data->tileChunkResidualIndex, 0, sizeof(int32_t) * RCMaxPlanes * LOQEnhancedCount);
    memset(data->tileChunkTemporalIndex, 0, sizeof(int32_t) * RCMaxPlanes);

    for (int32_t plane = 0; plane < data->numPlanes; ++plane) {
        /* num_layers chunks per plane-loq-tile */
        if (data->enhancementEnabled) {
            for (int32_t loq = 0; loq < LOQEnhancedCount; ++loq) {
                const int32_t tileCount = data->tileCount[plane][loq];
                const int32_t chunkCount = tileCount * data->numLayers;

                data->tileChunkResidualIndex[plane][loq] = offset;
                offset += chunkCount;
            }
        }

        /* one chunk per plane-loq-tile */
        if (data->temporalChunkEnabled) {
            const int32_t chunkCount = data->tilesAcross[plane][LOQ0] * data->tilesDown[plane][LOQ0];

            data->tileChunkTemporalIndex[plane] = offset;
            offset += chunkCount;
        }
    }
}

static int32_t calculateTileConfiguration(Logger_t log, DeserialisedData_t* data)
{
    int32_t res = 0;

    /* Ensure all tile dimensions are now valid across all planes. */
    VN_CHECK(calculateTileDimensions(data));

    /* Determine number of tiles across all planes and LOQs. */
    VN_CHECK(calculateTileCounts(log, data));

    /* Pre-calculate chunk offsets for quicker chunk lookup. */
    calculateTileChunkIndices(data);

    return res;
}

static int32_t getLayerChunkIndex(const DeserialisedData_t* data, int32_t planeIndex,
                                  LOQIndex_t loq, int32_t tile, int32_t layer)
{
    /* Requires the indices to be cached. */
    return data->tileChunkResidualIndex[planeIndex][loq] + (tile * data->numLayers) + layer;
}

/*------------------------------------------------------------------------------*/

static bool isDepthConfigSupported(Logger_t log, DeserialisedData_t* data)
{
    /* Currently only support promoting base-depth to enhancement depth. */
    if (data->enhaDepth < data->baseDepth) {
        VN_ERROR(log, "stream: Unsupported functionality. depth configuration is unsupported - [base_depth=%s, enha_depth=%s, loq1_use_enha_depth=%s]\n",
                 bitdepthToString(data->baseDepth), bitdepthToString(data->enhaDepth),
                 data->loq1UseEnhaDepth ? "true" : "false");
        return false;
    }

    return true;
}

static bool validateResolution(Logger_t log, DeserialisedData_t* data)
{
    const ScalingMode_t scaling = data->scalingModes[LOQ0];
    const Chroma_t chroma = data->chroma;

    /* This is a safety net, mono-chrome should always signal as 1 plane.*/
    const bool withChroma = (data->numPlanes > 1) && (chroma != CTMonochrome);
    const uint16_t transformAlignment = (data->transform == TransformDD) ? 2 : 4;

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
    if ((data->width & (horiAlignment - 1)) || (data->height & (vertAlignment - 1))) {
        VN_ERROR(log, "Resolution not supported in LCEVC layer. Resolution must be a factor "
                      "of whole transforms\n");
        return false;
    }

    return true;
}

/*------------------------------------------------------------------------------*/

static void vnovaConfigReset(VNConfig_t* cfg) { memset(cfg, 0, sizeof(VNConfig_t)); }

/*------------------------------------------------------------------------------*/

/* 7.3.4 (Table-8) & 7.4.3.2 */
static int32_t parseBlockSequenceConfig(ByteStream_t* stream, DeserialisedData_t* output)
{
    int32_t res = 0;

    uint8_t data;
    VN_CHECK(bytestreamReadU8(stream, &data));

    /* Profile: 4 bits */
    uint8_t profile = (data >> 4) & 0x0F;
    VN_UNUSED(profile);
    VN_DEBUG_SYNTAX("  Profile: %u\n", profile);

    /* Level: 4 bits */
    uint8_t level = (data >> 0) & 0x0F;
    VN_UNUSED(level);
    VN_DEBUG_SYNTAX("  Level: %u\n", level);

    VN_CHECK(bytestreamReadU8(stream, &data));

    /* Sub-level: 2 bits */
    uint8_t sublevel = (data >> 6) & 0x03;
    VN_UNUSED(sublevel);
    VN_DEBUG_SYNTAX("  Sub-level: %u\n", sublevel);

    /* Conformance window flag: 1 bit */
    lcevc_conformance_window* conformanceWindow = &output->conformanceWindow;

    conformanceWindow->enabled = (data >> 5) & 0x01;
    VN_DEBUG_SYNTAX("  Conformance window enabled: %u\n", conformanceWindow->enabled);

    if (conformanceWindow->enabled) {
        /* conf_win_left_offset: mb
           conf_win_right_offset: mb
           conf_win_top_offset: mb
           conf_win_bottom_offset: mb */
        VN_CHECK(parseConformanceValue(stream, &conformanceWindow->planes[0].left, "left"));
        VN_CHECK(parseConformanceValue(stream, &conformanceWindow->planes[0].right, "right"));
        VN_CHECK(parseConformanceValue(stream, &conformanceWindow->planes[0].top, "top"));
        VN_CHECK(parseConformanceValue(stream, &conformanceWindow->planes[0].bottom, "bottom"));

        VN_DEBUG_SYNTAX("  Conformance window: %u %u %u %u\n", &conformanceWindow->planes[0].left,
                        &conformanceWindow->planes[0].right, &conformanceWindow->planes[0].top,
                        &conformanceWindow->planes[0].bottom);
    }

    return 0;
}

static void setUserDataConfig(DeserialisedData_t* output, UserDataMode_t mode)
{
    UserDataConfig_t* userData = &output->userData;
    memorySet(userData, 0, sizeof(UserDataConfig_t));

    VN_DEBUG_SYNTAX("  User data mode: %s\n", userDataModeToString(mode));

    if (mode != UDMNone) {
        userData->enabled = true;
        userData->layerIndex = (output->transform == TransformDDS) ? UDCLayerIndexDDS : UDCLayerIndexDD;
        userData->shift = (mode == UDMWith2Bits) ? UDCShift2 : UDCShift6;
    }

    VN_DEBUG_SYNTAX("  User data mode: %s\n", userDataModeToString(mode));
    VN_DEBUG_SYNTAX("  User data layer: %d\n", userData->layerIndex);
    VN_DEBUG_SYNTAX("  User data shift: %d\n", userData->shift);
}

/* 7.3.5 (Table-9) & 7.4.3.3 */
static int32_t parseBlockGlobalConfig(Logger_t log, ByteStream_t* stream, DeserialisedData_t* output)
{
    int32_t res = 0;

    if (!output->currentVnovaConfigSet) {
        /* V-Nova config should always arrive before global config. If it has not
         * been sent this frame and a global config is received, then V-Nova config
         * is disabled. */
        vnovaConfigReset(&output->vnova_config);
    }

    uint8_t data = 0;
    VN_CHECK(bytestreamReadU8(stream, &data));

    /* plane_mode_flag: 1 bit */
    const uint8_t planeModeFlag = (data >> 7) & 0x01;
    VN_DEBUG_SYNTAX("  Plane mode flag: %u\n", planeModeFlag);

    /* resolution_type: 6 bits */
    const uint8_t resType = (data >> 1) & 0x3F;
    VN_DEBUG_SYNTAX("  Resolution type: %u\n", resType);

    if (resType > 0 && resType < kResolutionCount) {
        output->width = kResolutions[resType].width;
        output->height = kResolutions[resType].height;
        VN_DEBUG_SYNTAX("  Resolution width: %u\n", output->width);
        VN_DEBUG_SYNTAX("  Resolution height: %u\n", output->height);
    } else if (resType != kResolutionCustom) {
        VN_ERROR(log, "Packet gave an unsupported resolution type %d\n", resType);
        return -1;
    }

    /* transform_type: 1 bit */
    output->transform = (TransformType_t)((data >> 0) & 0x01);
    VN_DEBUG_SYNTAX("  Transform type: %s\n", transformTypeToString(output->transform));

    switch (output->transform) {
        case TransformDD: output->numLayers = RCLayerCountDD; break;
        case TransformDDS: output->numLayers = RCLayerCountDDS; break;
        default: {
            VN_ERROR(log, "Supplied transform is unrecognised: %s\n",
                     transformTypeToString(output->transform));
            return -1;
        }
    }

    VN_CHECK(bytestreamReadU8(stream, &data));

    /* chroma_sampling_type: 2 bits */
    output->chroma = (Chroma_t)((data >> 6) & 0x03);
    VN_DEBUG_SYNTAX("  Chroma sampling type: %s\n", chromaToString(output->chroma));

    /* base_depth_type: 2 bits */
    output->baseDepth = (BitDepth_t)((data >> 4) & 0x03);
    VN_DEBUG_SYNTAX("  Base depth: %s\n", bitdepthToString(output->baseDepth));

    /* enhancement_depth_type: 2 bit */
    output->enhaDepth = (BitDepth_t)((data >> 2) & 0x03);
    VN_DEBUG_SYNTAX("  Enhancement depth: %s\n", bitdepthToString(output->enhaDepth));

    /* temporal use step width modifier: 1 bit */
    const uint8_t useTemporalStepWidthModifier = (data >> 1) & 0x01;
    VN_DEBUG_SYNTAX("  Use temporal step-width modifier: %u\n", useTemporalStepWidthModifier);

    /* use predicted avg: 1 bit */
    output->usePredictedAverage = (data >> 0) & 0x01;
    VN_DEBUG_SYNTAX("  Use predicted average: %u\n", output->usePredictedAverage);

    VN_CHECK(bytestreamReadU8(stream, &data));

    /* temporal reduced signaling: 1 bit */
    output->temporalUseReducedSignalling = (data >> 7) & 0x01;
    VN_DEBUG_SYNTAX("  Temporal use reduced signalling: %u\n", output->temporalUseReducedSignalling);

    /* temporal enabled: 1 bit */
    output->temporalEnabled = (data >> 6) & 0x01;
    VN_DEBUG_SYNTAX("  Temporal enabled: %u\n", output->temporalEnabled);

    /* upsample type: 3 bits */
    const UpscaleType_t upsample = (UpscaleType_t)((data >> 3) & 0x07);
    VN_DEBUG_SYNTAX("  Upsample type: %s\n", upscaleTypeToString(upsample));

    if (upsample != USNearest && upsample != USLinear && upsample != USCubic &&
        upsample != USModifiedCubic && upsample != USAdaptiveCubic) {
        VN_ERROR(log, "unrecognized upscale type\n");
        return -1;
    }

    output->upscale = upsample;

    /* level-1 filtering enabled: 1 bit */
    output->deblock.enabled = (data >> 2) & 0x01;
    VN_DEBUG_SYNTAX("  Use deblocking: %u\n", output->deblock.enabled);

    /* scaling mode level-1: 2 bits */
    output->scalingModes[LOQ1] = (ScalingMode_t)((data >> 0) & 0x03);
    VN_DEBUG_SYNTAX("  Scaling mode LOQ-1: %s\n", scalingModeToString(output->scalingModes[LOQ1]));

    VN_CHECK(bytestreamReadU8(stream, &data));

    /* scaling mode level-2: 2 bits*/
    output->scalingModes[LOQ0] = (ScalingMode_t)((data >> 6) & 0x03);
    VN_DEBUG_SYNTAX("  Scaling mode LOQ-0: %s\n", scalingModeToString(output->scalingModes[LOQ0]));

    /* tile dimensions type: 2 bits */
    output->tileDimensions = (TileDimensions_t)((data >> 4) & 0x03);
    VN_DEBUG_SYNTAX("  Tile dimensions: %s\n", tileDimensionsToString(output->tileDimensions));

    /* user data mode: 2 bits */
    setUserDataConfig(output, (UserDataMode_t)((data >> 2) & 0x03));

    /* level-1 depth flag: 1 bit
       reserved: 1 bit */
    output->loq1UseEnhaDepth = ((data >> 1) & 0x01) ? true : false;
    VN_DEBUG_SYNTAX("  LOQ-1 use enhancement depth: %u\n", output->loq1UseEnhaDepth);

    /* chroma stepwidth flag: 1 bit*/
    const uint8_t chromaStepWidthFlag = (data >> 0) & 0x01;
    VN_DEBUG_SYNTAX("  Chroma step-width flag: %u\n", chromaStepWidthFlag);

    if (!isDepthConfigSupported(log, output)) {
        return -1;
    }

    /* plane_type: 4 bits
       reserved: 4 bits */
    if (planeModeFlag) {
        VN_CHECK(bytestreamReadU8(stream, &data));
        PlanesType_t planeType = (PlanesType_t)((data >> 4) & 0x0f);
        VN_DEBUG_SYNTAX("  Plane type: %s\n", planesTypeToString(planeType));

        switch (planeType) {
            case PlanesY: output->numPlanes = 1; break;
            case PlanesYUV: output->numPlanes = 3; break;
            default: {
                VN_ERROR(log, "Unrecognised plane type: %u\n", planeType);
                output->numPlanes = 0;
                return -1;
            }
        }
    } else {
        output->numPlanes = 1;
    }
    VN_DEBUG_SYNTAX("  Plane count: %u\n", output->numPlanes);

    if (useTemporalStepWidthModifier != 0) {
        VN_CHECK(bytestreamReadU8(stream, &output->temporalStepWidthModifier));
    } else {
        output->temporalStepWidthModifier = 48;
    }
    VN_DEBUG_SYNTAX("  Temporal step-width modifier: %u\n", output->temporalStepWidthModifier);

    if (upsample == USAdaptiveCubic) {
        const int8_t kernelSize = 4;

        output->adaptiveUpscaleKernel.length = kernelSize;
        output->adaptiveUpscaleKernel.isPreBakedPA = false;

        for (int16_t i = 0; i < kernelSize; i++) {
            /* First and last coeffs are negative */
            const int16_t multiplier = ((i % 3) == 0) ? -1 : 1;
            uint16_t coeff;

            VN_CHECK(bytestreamReadU16(stream, &coeff));

            output->adaptiveUpscaleKernel.coeffs[0][i] =
                output->adaptiveUpscaleKernel.coeffs[1][kernelSize - 1 - i] =
                    (int16_t)(multiplier * coeff);
        }

        VN_DEBUG_SYNTAX(
            "  Adaptive upsampler kernel: %d %d %d %d\n",
            output->adaptiveUpscaleKernel.coeffs[0][0], output->adaptiveUpscaleKernel.coeffs[0][1],
            output->adaptiveUpscaleKernel.coeffs[0][2], output->adaptiveUpscaleKernel.coeffs[0][3]);
    }

    if (output->deblock.enabled) {
        VN_CHECK(bytestreamReadU8(stream, &data));

        /* De-block corner: 4 bits */
        output->deblock.corner = 16 - ((data >> 4) & 0x0F);

        /* De-block side: 4 bits */
        output->deblock.side = 16 - ((data >> 0) & 0x0F);

        VN_DEBUG_SYNTAX("  Deblocking coeffs - 0: %u, 1: %u\n", output->deblock.corner,
                        output->deblock.side);
    }

    if (output->tileDimensions != TDTNone) {
        if (output->tileDimensions == TDTCustom) {
            /* custom_tile_width: 16 bits */
            VN_CHECK(bytestreamReadU16(stream, &output->tileWidth[0]));

            /* custom_tile_height: 16 bits */
            VN_CHECK(bytestreamReadU16(stream, &output->tileHeight[0]));
        } else {
            VN_CHECK(tileDimensionsFromType(output->tileDimensions, &output->tileWidth[0],
                                            &output->tileHeight[0]));
        }

        VN_CHECK(bytestreamReadU8(stream, &data));

        /* reserved: 5 bits
           compression_type_entropy_enabled_per_tile_flag: 1 bit */
        output->tileEnabledPerTileCompressionFlag = (data >> 2) & 0x01;

        /* compression_type_size_per_tile: 2 bits */
        output->tileSizeCompression = (TileCompressionSizePerTile_t)((data >> 0) & 0x03);

        VN_DEBUG_SYNTAX("  Custom tile size: %ux%u\n", output->tileWidth[0], output->tileHeight[0]);
        VN_DEBUG_SYNTAX("  Per tile enabled compression: %d\n", output->tileEnabledPerTileCompressionFlag);
        VN_DEBUG_SYNTAX("  Tile size compression: %u\n", output->tileSizeCompression);
    }

    /* custom resolution */
    if (resType == kResolutionCustom) {
        VN_CHECK(bytestreamReadU16(stream, &output->width));
        VN_CHECK(bytestreamReadU16(stream, &output->height));

        VN_DEBUG_SYNTAX("  Custom resolution width: %u\n", output->width);
        VN_DEBUG_SYNTAX("  Custom resolution height: %u\n", output->height);
    }

    output->globalHeight = output->height;

    /* chroma step-width multiplier */
    if (chromaStepWidthFlag) {
        VN_CHECK(bytestreamReadU8(stream, &output->chromaStepWidthMultiplier));
    } else {
        output->chromaStepWidthMultiplier = QDefaultChromaSWMultiplier;
    }

    VN_DEBUG_SYNTAX("  Chroma step-width multiplier: %u\n", output->chromaStepWidthMultiplier);

    /* checks on viability of settings */
    if (!validateResolution(log, output)) {
        return -1;
    }

    /* initialise default quant matrix on first global config */
    if (!output->globalConfigSet) {
        quantMatrixSetDefault(&output->quantMatrix, output->scalingModes[LOQ0], output->transform);
        VN_DEBUG_SYNTAX("  Defaulting quant-matrix\n");
    }

    /* prepare tile information */
    if (output->tileDimensions == TDTNone) {
        /* When tiling is disabled, there is a single tile that is the size of
         * the surface for each plane. */
        output->tileWidth[0] = output->width;
        output->tileHeight[0] = output->height;
    }

    /* validate/update conformance window */
    if (output->conformanceWindow.enabled) {
        lcevc_conformance_window* window = &output->conformanceWindow;

        const int32_t shiftw = chromaShiftWidth(output->chroma);
        const int32_t shifth = chromaShiftHeight(output->chroma);

        /* Mirror from luma entry to chroma entries */
        memcpy(&window->planes[1], &window->planes[0], sizeof(lcevc_conformance_window_plane));
        memcpy(&window->planes[2], &window->planes[0], sizeof(lcevc_conformance_window_plane));

        /* The conformance window is signaled as the window to crop for the chroma
         * planes - as a convenience the DPI outputs the crop windows for each plane
         * in absolute pixels for that given plane based upon the chroma_t setting.
         * Therefore scale the luma entry appropriately. */
        window->planes[0].left <<= shiftw;
        window->planes[0].right <<= shiftw;
        window->planes[0].top <<= shifth;
        window->planes[0].bottom <<= shifth;

        for (int32_t i = 0; i < 3; ++i) {
            VN_UNUSED(i);
            VN_DEBUG_SYNTAX(
                "  Conformance window plane: %d - left: %u, right: %u, top: %u, bottom: %u\n", i,
                window->planes[i].left, window->planes[i].right, window->planes[i].top,
                window->planes[i].bottom);
        }

        if ((window->planes[0].left + window->planes[0].right) >= output->width) {
            VN_ERROR(log, "stream: Conformance window values combined are greater than decode width [left: %u, right: %u, width: %u]\n",
                     window->planes[0].left, window->planes[0].right, output->width);
            return -1;
        }

        if ((window->planes[0].top + window->planes[0].bottom) >= output->height) {
            VN_ERROR(log, "stream: Window values combined are greater than decode width [top: %u, bottom: %u, height: %u]\n",
                     window->planes[0].top, window->planes[0].bottom, output->height);
            return -1;
        }
    }

    output->globalConfigSet = true;
    output->currentGlobalConfigSet = true;

    return 0;
}

/* 7.3.6 (Table-10) & 7.4.3.4 */
static int32_t parseBlockPictureConfig(ByteStream_t* stream, DeserialisedData_t* output)
{
    int32_t res;
    uint8_t data;

    /* Enhancement enabled check. (Signaled as disabled, so invert for better logic). */
    VN_CHECK(bytestreamReadU8(stream, &data));
    output->enhancementEnabled = (data & 0x80) ? 0 : 1;

    if (output->enhancementEnabled) {
        QuantMatrixMode_t qmMode;
        uint8_t dequantOffsetEnabled;
        uint8_t stepWidthLOQ1Enabled;
        uint8_t ditherControl;

        VN_DEBUG_SYNTAX("  Enhancement enabled\n");

        /* perseus disabled: 1 bit (already interpreted)
         * quant-matrix mode: 3 bits */
        qmMode = (QuantMatrixMode_t)((data >> 4) & 0x07);
        VN_DEBUG_SYNTAX("  Quant-matrix mode: %s\n", quantMatrixModeToString(qmMode));

        /* dequant offset enabled: 1 bit */
        dequantOffsetEnabled = (data >> 3) & 0x01;
        VN_DEBUG_SYNTAX("  Dequant offset enabled: %u\n", dequantOffsetEnabled);

        /* picture type: 1 bit */
        output->picType = (PictureType_t)((data >> 2) & 0x01);
        VN_DEBUG_SYNTAX("  Picture type: %s\n", pictureTypeToString(output->picType));

        /* temporal refresh: 1 bit */
        output->temporalRefresh = (data >> 1) & 0x01;
        VN_DEBUG_SYNTAX("  Temporal refresh: %u\n", output->temporalRefresh);

        /* step width LOQ 1 enabled: 1 bit */
        stepWidthLOQ1Enabled = (data >> 0) & 0x01;
        VN_DEBUG_SYNTAX("  Step-width LOQ-1 enabled: %u\n", stepWidthLOQ1Enabled);

        uint16_t data16;
        VN_CHECK(bytestreamReadU16(stream, &data16));

        /* step width LOQ-0: 15 bits */
        output->stepWidths[LOQ0] = (data16 >> 1) & 0x7FFF;
        VN_DEBUG_SYNTAX("  Step-width LOQ-0: %u\n", output->stepWidths[LOQ0]);

        /* dither control: 1 bit */
        ditherControl = (data16 >> 0) & 0x01;
        VN_DEBUG_SYNTAX("  Dither control: %u\n", ditherControl);

        if (output->picType == PTField) {
            VN_CHECK(bytestreamReadU8(stream, &data));

            /* field type: 1 bit
             * reserved: 7 bits*/
            output->fieldType = (FieldType_t)((data >> 7) & 0x01);
            VN_DEBUG_SYNTAX("  Field type: %s\n", fieldTypeToString(output->fieldType));
        }

        if (stepWidthLOQ1Enabled) {
            VN_CHECK(bytestreamReadU16(stream, &data16));

            /* step width LOQ-1: 15 bits
             * reserved: 1 bit */
            output->stepWidths[LOQ1] = (data16 >> 1) & 0x7FFF;
        } else {
            output->stepWidths[LOQ1] = QMaxStepWidth;
        }

        VN_DEBUG_SYNTAX("  Step-width LOQ-1: %u\n", output->stepWidths[LOQ1]);

        if (qmMode != QMMUsePrevious) {
            /* Default both quant-matrices initially if the frame is IDR */
            if (output->type == NTIDR) {
                quantMatrixSetDefault(&output->quantMatrix, output->scalingModes[LOQ0], output->transform);
            }

            /* Load up LOQ-0 quant-matrix if it's signalled */
            if (qmMode == QMMCustomBoth || qmMode == QMMCustomLOQ0 || qmMode == QMMCustomBothUnique) {
                /* Load up LOQ-0 quant-matrix */
                VN_CHECK(quantMatrixParseLOQ(stream, LOQ0, output));
            }

            /* Load up LOQ-1 quant-matrix if its signalled */
            if (qmMode == QMMCustomLOQ1 || qmMode == QMMCustomBothUnique) {
                VN_CHECK(quantMatrixParseLOQ(stream, LOQ1, output));
            } else if (qmMode == QMMCustomBoth) {
                /* Copy LOQ-0 QM to LOQ-1 QM if both use the same custom signal. */
                const uint8_t* loq0QM = quantMatrixGetValues(&output->quantMatrix, LOQ0);
                uint8_t* loq1QM = quantMatrixGetValues(&output->quantMatrix, LOQ1);
                const uint32_t copysize = output->numLayers * sizeof(uint8_t);

                memcpy(loq1QM, loq0QM, copysize);
            }
        }

        quantMatrixDebugLog(output, LOQ0);
        quantMatrixDebugLog(output, LOQ1);

        if (dequantOffsetEnabled != 0) {
            VN_CHECK(bytestreamReadU8(stream, &data));

            output->dequantOffsetMode = (DequantOffsetMode_t)((data >> 7) & 0x01);
            output->dequantOffset = (int32_t)(data & 0x7F);

            VN_DEBUG_SYNTAX("  Dequant offset mode: %s\n",
                            dequantOffsetModeToString(output->dequantOffsetMode));
            VN_DEBUG_SYNTAX("  Dequant offset: %d\n", output->dequantOffset);
        } else {
            output->dequantOffset = -1;
        }

        if (ditherControl) {
            VN_CHECK(bytestreamReadU8(stream, &data));

            /* dither type: 2 bits*/
            output->ditherType = (DitherType_t)((data >> 6) & 0x03);

            /* reserved: 1 bit
             * dither strength: 5 bits */
            output->ditherStrength = (data >> 0) & 0x1F;
        } else if (output->currentGlobalConfigSet) {
            /* On an IDR frame when dither is not signaled it should be disabled,
             * otherwise the previous value should be used. (7.4.3.4) */
            output->ditherType = DTNone;
            output->ditherStrength = 0;
        }

        VN_DEBUG_SYNTAX("  Dithering type: %s\n", ditherTypeToString(output->ditherType));
        VN_DEBUG_SYNTAX("  Dither strength: %u\n", output->ditherStrength);

        /* Separate chunk is only signalled when we're not refreshing (and
         * embedded is disabled) */
        output->temporalChunkEnabled = output->temporalEnabled && !output->temporalRefresh;
        VN_DEBUG_SYNTAX("  Temporal chunk enabled: %u\n", output->temporalChunkEnabled);
    } else {
        /* perseus disabled: 1 bit (already interpreted)
         * reserved: 4 bits */
        VN_DEBUG_SYNTAX("  Enhancement disabled\n");

        /* picture_type_bit: 1 bit */
        output->picType = (PictureType_t)((data >> 2) & 0x01);
        VN_DEBUG_SYNTAX("  Picture type: %s\n", pictureTypeToString(output->picType));

        /* temporal_refresh_bit: 1 bit */
        output->temporalRefresh = ((data >> 1) & 0x01);
        VN_DEBUG_SYNTAX("  Temporal refresh: %u\n", output->temporalRefresh);

        /* temporal_signalling_bit: 1 bit */
        output->temporalChunkEnabled = ((data >> 0) & 0x01);
        VN_DEBUG_SYNTAX("  Temporal chunk enabled: %u\n", output->temporalChunkEnabled);

        if (output->currentGlobalConfigSet) {
            /* Same situation as with LCEVC enabled, excepting that
             * dither control is implicitly not signalled here. */
            VN_DEBUG_SYNTAX("Resetting dither state on IDR with LCEVC disabled\n");
            output->ditherType = DTNone;
            output->ditherStrength = 0;
        }
    }

    output->height = (output->globalHeight >> output->picType);
    output->pictureConfigSet = true;

    return 0;
}

/*! \brief Helper that checks the chunk array allocation is sufficiently sized and
 *         if not it will resize accordingly.
 *
 * \param memory        Decoder context's memory manager.
 * \param log           Decoder context's logger.
 * \param data          Deserialised data to allocate the chunk array on.
 * \param chunk_count   The number of chunks desired for the array.
 *
 * \return 0 on success, otherwise -1
 */
static int32_t chunkCheckAlloc(Memory_t memory, Logger_t log, DeserialisedData_t* data)
{
    assert(data);

    uint32_t chunkCount = 0;

    /* Determine number of desired chunks. */
    if (data->enhancementEnabled) {
        for (int32_t plane = 0; plane < data->numPlanes; ++plane) {
            chunkCount += (data->tileCount[plane][LOQ0] + data->tileCount[plane][LOQ1]) * data->numLayers;
        }
    }

    if (data->temporalChunkEnabled) {
        for (int32_t plane = 0; plane < data->numPlanes; ++plane) {
            chunkCount += data->tileCount[plane][LOQ0];
        }
    }

    /* Reallocate chunk memory if needed. */
    if (chunkCount != data->numChunks || !data->chunks) {
        if (data->chunks) {
            VN_FREE(memory, data->chunks);
        }

        data->chunks = VN_CALLOC_T_ARR(memory, Chunk_t, chunkCount);
        data->numChunks = chunkCount;
    }

    if (!data->chunks) {
        VN_ERROR(log, "Memory allocation for chunk data failed\n");
        return -1;
    }

    VN_DEBUG_SYNTAX("  Chunk count: %d\n", data->numChunks);

    return 0;
}

static int32_t parseChunk(Logger_t log, ByteStream_t* stream, Chunk_t* chunk,
                          bool* loqEntropyEnabled, TiledSizeDecoder_t* sizeDecoder)
{
    int32_t res = 0;

    chunk->size = 0;

    if (chunk->entropyEnabled) {
        if (sizeDecoder) {
            int16_t chunkSize = tiledSizeDecoderRead(sizeDecoder);

            if (chunkSize < 0) {
                VN_ERROR(log, "stream: Failed to decode compressed chunk size\n");
                return -1;
            }

            chunk->size = (uint32_t)chunkSize;
        } else {
            uint64_t mb;
            VN_CHECK(bytestreamReadMultiByte(stream, &mb));

            if (mb > INT_MAX) {
                VN_ERROR(log, "stream: Chunk data size is larger than INT_MAX\n");
                return -1;
            }

            chunk->size = (uint32_t)mb;
        }

        /* Signal that the LOQ has some data. */
        if (loqEntropyEnabled) {
            *loqEntropyEnabled = true;
        }

        chunk->data = bytestreamCurrent(stream);
        VN_CHECK(bytestreamSeek(stream, chunk->size));

        VN_DEBUG_SYNTAX("%s=%u\n", chunk->rleOnly ? "RLE" : "Huffman", chunk->size);
    } else {
        VN_DEBUG_SYNTAX("disabled\n");
    }

    return 0;
}

static int32_t parseChunkFlags(BitStream_t* stream, Chunk_t* chunks, int32_t numChunks)
{
    int32_t res = 0;

    for (int32_t i = 0; i < numChunks; ++i) {
        VN_CHECK(bitstreamReadBit(stream, &chunks[i].entropyEnabled));
        VN_CHECK(bitstreamReadBit(stream, &chunks[i].rleOnly));
    }

    return res;
}

static int32_t parseCoeffChunks(Logger_t log, ByteStream_t* stream, DeserialisedData_t* output,
                                int32_t plane, LOQIndex_t loq)
{
    int32_t res = 0;

    Chunk_t* chunks;
    VN_CHECK(deserialiseGetTileLayerChunks(output, plane, loq, 0, &chunks));

    for (int32_t layer = 0; layer < output->numLayers; ++layer) {
        VN_DEBUG_SYNTAX("    [%d, %d, %2d]: ", plane, loq, layer);
        VN_CHECK(parseChunk(log, stream, &chunks[layer], &output->entropyEnabled[loq], NULL));
    }

    VN_DEBUG_SYNTAX("    %s enabled: %d\n", loqIndexToString((LOQIndex_t)loq),
                    output->entropyEnabled[loq]);

    return res;
}

static int32_t parseEncodedData(Memory_t memory, Logger_t log, ByteStream_t* stream,
                                DeserialisedData_t* output, perseus_pipeline_mode pipelineMode)
{
    int32_t res = 0;

    if (!output->globalConfigSet) {
        VN_ERROR(log, "stream: Have not yet received a global config block\n");
        return -1;
    }

    if (!output->pictureConfigSet) {
        VN_ERROR(log, "stream: Have not yet received a picture config block\n");
        return -1;
    }

    VN_CHECK(calculateTileConfiguration(log, output));

    VN_CHECK(chunkCheckAlloc(memory, log, output));

    output->entropyEnabled[LOQ0] = false;
    output->entropyEnabled[LOQ1] = false;

    /* --- Read the enabled & RLE-only flags --- */

    BitStream_t chunkHeadersStream = {{0}};
    VN_CHECK(bitstreamInitialise(&chunkHeadersStream, bytestreamCurrent(stream),
                                 bytestreamRemaining(stream)));

    for (int32_t plane = 0; plane < output->numPlanes; ++plane) {
        if (output->enhancementEnabled) {
            for (int32_t loq = LOQ1; loq >= LOQ0; --loq) {
                Chunk_t* chunks;
                VN_CHECK(deserialiseGetTileLayerChunks(output, plane, (LOQIndex_t)loq, 0, &chunks));
                VN_CHECK(parseChunkFlags(&chunkHeadersStream, chunks, output->numLayers));
            }
        }

        if (output->temporalChunkEnabled) {
            Chunk_t* temporalChunk;
            VN_CHECK(deserialiseGetTileTemporalChunk(output, plane, 0, &temporalChunk));
            VN_CHECK(parseChunkFlags(&chunkHeadersStream, temporalChunk, 1));
        }
    }

    // @todo(bob): This should be removed, not sure why we need to pass on the pipeline mode to the parsed data.
    output->pipelineMode = pipelineMode;

    /* Move bytestream forward with byte alignment */
    bytestreamSeek(stream, bitstreamGetConsumedBytes(&chunkHeadersStream));

    /* --- Read chunk data --- */

    VN_DEBUG_SYNTAX("  [Plane, LoQ, Layer]\n");
    for (int32_t plane = 0; plane < output->numPlanes; ++plane) {
        if (output->enhancementEnabled) {
            for (int32_t loq = LOQ1; loq >= LOQ0; --loq) {
                VN_CHECK(parseCoeffChunks(log, stream, output, plane, (LOQIndex_t)loq));
            }
        }

        if (output->temporalChunkEnabled) {
            VN_DEBUG_SYNTAX("    [temporal: %d]: ", plane);
            Chunk_t* temporalChunk = NULL;
            VN_CHECK(deserialiseGetTileTemporalChunk(output, plane, 0, &temporalChunk));
            if (temporalChunk == NULL) {
                return -1;
            }
            VN_CHECK(parseChunk(log, stream, temporalChunk, &output->entropyEnabled[LOQ0], NULL));
        }
    }

    return 0;
}

static int32_t parseEncodedDataTiled(Memory_t memory, Logger_t log, ByteStream_t* stream,
                                     DeserialisedData_t* output, bool useOldCodeLengths)
{
    int32_t res = 0;

    if (!output->globalConfigSet) {
        VN_ERROR(log, "stream: Have not yet received a global config block\n");
        return -1;
    }

    if (!output->pictureConfigSet) {
        VN_ERROR(log, "stream: Have not yet received a picture config block\n");
        return -1;
    }

    if (output->tileWidth[0] == 0 || output->tileHeight[0] == 0) {
        VN_ERROR(log, "stream: Both tile dimensions must not be 0\n");
        return -1;
    }

    VN_CHECK(calculateTileConfiguration(log, output));

    VN_CHECK(chunkCheckAlloc(memory, log, output));

    output->entropyEnabled[LOQ0] = false;
    output->entropyEnabled[LOQ1] = false;

    if (output->enhancementEnabled || output->temporalChunkEnabled) {
        BitStream_t rleOnlyBS = {{0}};
        uint8_t layerRLEOnly = 0;

        BitStream_t entropyEnabledBS = {{0}};
        TiledRLEDecoder_t entropyEnabledRLE = {0};
        TiledSizeDecoder_t sizeDecoder = {0};
        TiledSizeDecoder_t* sizeDecoderPtr =
            (output->tileSizeCompression != TCSPTTNone) ? &sizeDecoder : NULL;

        VN_CHECK(bitstreamInitialise(&rleOnlyBS, bytestreamCurrent(stream), bytestreamRemaining(stream)));

        /* --- Read the RLE-only flags --- */

        VN_DEBUG_SYNTAX("  RLE only flags\n");
        VN_DEBUG_SYNTAX("  [Plane, LoQ, Layer]\n");

        for (int32_t plane = 0; plane < output->numPlanes; ++plane) {
            /* Whole surface RLE only flag per-layer */
            if (output->enhancementEnabled) {
                for (int32_t loq = LOQ1; loq >= LOQ0; --loq) {
                    const int32_t currentTileCount = output->tileCount[plane][loq];

                    for (int32_t layer = 0; layer < output->numLayers; ++layer) {
                        /* Read a bit for RLE signal. */
                        VN_CHECK(bitstreamReadBit(&rleOnlyBS, &layerRLEOnly));

                        VN_DEBUG_SYNTAX("  [%u, %d, %2u]: %u\n", plane, loq, layer, layerRLEOnly);

                        /* Broadcast RLE only to all tiles for a layer. */
                        for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                            const int32_t chunkIndex =
                                getLayerChunkIndex(output, plane, (LOQIndex_t)loq, tile, layer);
                            Chunk_t* chunk = &output->chunks[chunkIndex];
                            chunk->rleOnly = layerRLEOnly;
                        }
                    }
                }
            }

            /* Temporal layer RLE only flag*/
            if (output->temporalChunkEnabled) {
                /* Read a bit for RLE signal. */
                uint8_t temporalRLEOnly = 0;
                const int32_t currentTileCount = output->tileCount[plane][LOQ0];
                Chunk_t* temporalTileChunks = &output->chunks[output->tileChunkTemporalIndex[plane]];

                VN_CHECK(bitstreamReadBit(&rleOnlyBS, &temporalRLEOnly));
                VN_DEBUG_SYNTAX("  [temporal: %u]: %u\n", plane, temporalRLEOnly);

                /* Broadcast RLE only to all tiles for the temporal layer. */
                for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                    temporalTileChunks[tile].rleOnly = temporalRLEOnly;
                }
            }
        }

        /* Move bytestream forward with byte alignment */
        bytestreamSeek(stream, bitstreamGetConsumedBytes(&rleOnlyBS));

        /* --- Read the entropy enabled flags --- */

        if (output->tileEnabledPerTileCompressionFlag) {
            VN_CHECK(tiledRLEDecoderInitialise(&entropyEnabledRLE, stream));
        } else {
            VN_CHECK(bitstreamInitialise(&entropyEnabledBS, bytestreamCurrent(stream),
                                         bytestreamRemaining(stream)));
        }

        for (int32_t plane = 0; plane < output->numPlanes; ++plane) {
            if (output->enhancementEnabled) {
                for (int32_t loq = LOQ1; loq >= LOQ0; --loq) {
                    const int32_t currentTileCount = output->tileCount[plane][loq];

                    for (int32_t layer = 0; layer < output->numLayers; ++layer) {
                        for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                            const int32_t chunkIndex =
                                getLayerChunkIndex(output, plane, (LOQIndex_t)loq, tile, layer);
                            Chunk_t* chunk = &output->chunks[chunkIndex];

                            if (output->tileEnabledPerTileCompressionFlag) {
                                VN_CHECK(tiledRLEDecoderRead(&entropyEnabledRLE, &chunk->entropyEnabled));
                            } else {
                                VN_CHECK(bitstreamReadBit(&entropyEnabledBS, &chunk->entropyEnabled));
                            }
                        }
                    }
                }
            }

            if (output->temporalChunkEnabled) {
                const int32_t currentTileCount = output->tileCount[plane][LOQ0];
                Chunk_t* temporalTileChunks = &output->chunks[output->tileChunkTemporalIndex[plane]];

                for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                    if (output->tileEnabledPerTileCompressionFlag) {
                        VN_CHECK(tiledRLEDecoderRead(&entropyEnabledRLE,
                                                     &temporalTileChunks[tile].entropyEnabled));
                    } else {
                        VN_CHECK(bitstreamReadBit(&entropyEnabledBS,
                                                  &temporalTileChunks[tile].entropyEnabled));
                    }
                }
            }
        }

        if (output->tileEnabledPerTileCompressionFlag == 0) {
            /* Move bytestream forward with byte alignment */
            bytestreamSeek(stream, bitstreamGetConsumedBytes(&entropyEnabledBS));
        }

        /* --- Read chunk data --- */

        VN_DEBUG_SYNTAX("  Entropy Signal\n");
        VN_DEBUG_SYNTAX("  [Plane, LOQ, Layer, Tile] \n");
        for (int32_t plane = 0; plane < output->numPlanes; ++plane) {
            if (output->enhancementEnabled) {
                for (int32_t loq = LOQ1; loq >= LOQ0; --loq) {
                    const int32_t currentTileCount = output->tileCount[plane][loq];

                    for (int32_t layer = 0; layer < output->numLayers; ++layer) {
                        if (output->tileSizeCompression != TCSPTTNone) {
                            /* Determine number of chunks enabled to know how many
                             * sizes we want to decode. */
                            uint32_t numChunksEnabled = 0;

                            for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                                const int32_t chunkIndex =
                                    getLayerChunkIndex(output, plane, (LOQIndex_t)loq, tile, layer);
                                Chunk_t* chunk = &output->chunks[chunkIndex];
                                numChunksEnabled += chunk->entropyEnabled;
                            }

                            VN_CHECK(tiledSizeDecoderInitialise(memory, log, &sizeDecoder, numChunksEnabled,
                                                                stream, output->tileSizeCompression,
                                                                useOldCodeLengths));
                        }

                        for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                            const int32_t chunkIndex =
                                getLayerChunkIndex(output, plane, (LOQIndex_t)loq, tile, layer);
                            Chunk_t* chunk = &output->chunks[chunkIndex];

                            VN_DEBUG_SYNTAX("    [%d, %d, %2d, %3d] chunk %-4d: ", plane, loq,
                                            layer, tile, chunkIndex);
                            VN_CHECK(parseChunk(log, stream, chunk, &output->entropyEnabled[loq],
                                                sizeDecoderPtr));
                        }
                    }

                    VN_DEBUG_SYNTAX("    %s enabled: %s\n", loqIndexToString((LOQIndex_t)loq),
                                    output->entropyEnabled[loq] ? "true" : "false");
                }
            }

            if (output->temporalChunkEnabled) {
                const int32_t currentTileCount = output->tileCount[plane][LOQ0];
                Chunk_t* temporalTileChunks = &output->chunks[output->tileChunkTemporalIndex[plane]];

                if (output->tileSizeCompression != TCSPTTNone) {
                    uint32_t numChunksEnabled = 0;

                    for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                        numChunksEnabled += temporalTileChunks[tile].entropyEnabled;
                    }

                    VN_CHECK(tiledSizeDecoderInitialise(memory, log, &sizeDecoder, numChunksEnabled, stream,
                                                        output->tileSizeCompression, useOldCodeLengths));
                }

                for (int32_t tile = 0; tile < currentTileCount; ++tile) {
                    VN_DEBUG_SYNTAX("    temporal: [%d, %3u]: ", plane, tile);
                    VN_CHECK(parseChunk(log, stream, &temporalTileChunks[tile],
                                        &output->entropyEnabled[LOQ0], sizeDecoderPtr));
                }
            }
        }

        tiledSizeDecoderRelease(sizeDecoderPtr);
    }

    return 0;
}

static int32_t parseBlockFiller(ByteStream_t* stream, uint32_t blockSize)
{
    /* Skip block. */
    return bytestreamSeek(stream, blockSize);
}

static int32_t parseSEIPayload(ByteStream_t* stream, lcevc_hdr_info* hdrInfoOut,
                               DeserialisedData_t* deserialisedOut, uint32_t blockSize)
{
    int32_t res = 0;

    uint8_t data;
    VN_CHECK(bytestreamReadU8(stream, &data));
    SEIPayloadType_t payloadType = (SEIPayloadType_t)data;
    VN_DEBUG_SYNTAX("    sei_payload_type: %s\n", seiPayloadTypeToString(payloadType));

    if (payloadType == SPT_MasteringDisplayColourVolume) {
        /* D.2.2 */

        /* Write values to hdrInfoOut */
        lcevc_mastering_display_colour_volume* colorInfo = &hdrInfoOut->mastering_display;

        for (int32_t i = 0; i < VN_MDCV_NUM_PRIMARIES; ++i) {
            VN_CHECK(bytestreamReadU16(stream, &colorInfo->display_primaries_x[i]));
            VN_CHECK(bytestreamReadU16(stream, &colorInfo->display_primaries_y[i]));

            VN_DEBUG_SYNTAX("      display primaries: index %u - x=%u, y=%u\n", i,
                            colorInfo->display_primaries_x[i], colorInfo->display_primaries_y[i]);
        }

        VN_CHECK(bytestreamReadU16(stream, &colorInfo->white_point_x));
        VN_CHECK(bytestreamReadU16(stream, &colorInfo->white_point_y));
        VN_CHECK(bytestreamReadU32(stream, &colorInfo->max_display_mastering_luminance));
        VN_CHECK(bytestreamReadU32(stream, &colorInfo->min_display_mastering_luminance));

        VN_DEBUG_SYNTAX("      white point x: %u\n", colorInfo->white_point_x);
        VN_DEBUG_SYNTAX("      white point y: %u\n", colorInfo->white_point_y);
        VN_DEBUG_SYNTAX("      max display mastering luminance: %u\n",
                        colorInfo->max_display_mastering_luminance);
        VN_DEBUG_SYNTAX("      min display mastering luminance: %u\n",
                        colorInfo->min_display_mastering_luminance);

        hdrInfoOut->flags |= LCEVC_HDRF_MASTERING_DISPLAY_COLOUR_VOLUME_PRESENT;
    } else if (payloadType == SPT_ContentLightLevelInfo) {
        /* D.2.3 */

        /* Write values to hdrInfoOut */
        lcevc_content_light_level* lightLevel = &hdrInfoOut->content_light_level;

        VN_CHECK(bytestreamReadU16(stream, &lightLevel->max_content_light_level));
        VN_CHECK(bytestreamReadU16(stream, &lightLevel->max_pic_average_light_level));

        VN_DEBUG_SYNTAX("      max content light level: %u\n", lightLevel->max_content_light_level);
        VN_DEBUG_SYNTAX("      max pic average light level: %u\n", lightLevel->max_pic_average_light_level);

        hdrInfoOut->flags |= LCEVC_HDRF_CONTENT_LIGHT_LEVEL_INFO_PRESENT;
    } else if (payloadType == SPT_UserDataRegistered) {
        /* D.2.4 */

        /* Write values to deserialisedOut and its vnova_config (IF present in stream) */
        uint8_t ituHeader[ITUC_Length] = {0};

        VN_CHECK(bytestreamReadU8(stream, &ituHeader[0]));

        /* Check for UK country code first. */
        if (ituHeader[0] != kVNovaITU[0]) {
            return bytestreamSeek(stream, blockSize - 1);
        }

        VN_CHECK(bytestreamReadU8(stream, &ituHeader[1]));
        VN_CHECK(bytestreamReadU8(stream, &ituHeader[2]));
        VN_CHECK(bytestreamReadU8(stream, &ituHeader[3]));

        if (memcmp(ituHeader, kVNovaITU, ITUC_Length) != 0) {
            return bytestreamSeek(stream, blockSize - ITUC_Length);
        }

        VNConfig_t* cfg = &deserialisedOut->vnova_config;
        VN_DEBUG_SYNTAX("      V-Nova SEI Payload Found\n");

        VN_CHECK(bytestreamReadU8(stream, &cfg->bitstreamVersion));
        VN_DEBUG_SYNTAX("      Bitstream version: %u\n", cfg->bitstreamVersion);

        cfg->valid = true;
        deserialisedOut->currentVnovaConfigSet = true;

        VN_DEBUG_SYNTAX("V-Nova Bitstream Version: %u\n", cfg->bitstreamVersion);

    } else {
        VN_DEBUG_SYNTAX("      unsupported SEI payload type, skipping %d bytes\n", blockSize - 1);
        return bytestreamSeek(stream, blockSize - 1);
    }

    return res;
}

/* E.2 */
static int32_t parseVUIParameters(ByteStream_t* stream, lcevc_vui_info* vuiInfoOut, uint32_t vuiSize)
{
    int32_t res = 0;
    uint8_t bit = 0;
    int32_t bits = 0;

    BitStream_t bitstream = {{0}};
    VN_CHECK(bitstreamInitialise(&bitstream, bytestreamCurrent(stream), vuiSize));

    /* aspect_ratio_info_present_flag: 1 bit */
    VN_CHECK(bitstreamReadBit(&bitstream, &bit));
    VN_DEBUG_SYNTAX("    aspect_ratio_info_present: %d\n", bit);

    if (bit) {
        vuiInfoOut->flags |= PSS_VUIF_ASPECT_RATIO_INFO_PRESENT;

        /* aspect_ratio_idc: 8 bits */
        VN_CHECK(bitstreamReadBits(&bitstream, 8, &bits));
        vuiInfoOut->aspect_ratio_idc = (uint8_t)bits;
        VN_DEBUG_SYNTAX("      aspect_ratio_idc: %u\n", vuiInfoOut->aspect_ratio_idc);

        if (vuiInfoOut->aspect_ratio_idc == kVUIAspectRatioIDCExtendedSAR) {
            /* sar_width: 16 bits */
            VN_CHECK(bitstreamReadBits(&bitstream, 16, &bits));
            vuiInfoOut->sar_width = (uint16_t)bits;

            /* sar_height: 16 bits */
            VN_CHECK(bitstreamReadBits(&bitstream, 16, &bits));
            vuiInfoOut->sar_height = (uint16_t)bits;

            VN_DEBUG_SYNTAX("      sar_width: %u\n", vuiInfoOut->sar_width);
            VN_DEBUG_SYNTAX("      sar_height: %u\n", vuiInfoOut->sar_height);
        }
    }

    /* overscan_info_present_flag: 1 bit */
    VN_CHECK(bitstreamReadBit(&bitstream, &bit));
    VN_DEBUG_SYNTAX("    overscan_info_present: %d\n", bit);

    if (bit) {
        vuiInfoOut->flags |= PSS_VUIF_OVERSCAN_INFO_PRESENT;

        /* overscan_appropraite_flag: 1 bit*/
        VN_CHECK(bitstreamReadBit(&bitstream, &bit));
        if (bit) {
            vuiInfoOut->flags |= PSS_VUIF_OVERSCAN_APPROPRIATE;
        }

        VN_DEBUG_SYNTAX("      overscan_appropriate: %d\n", bit);
    }

    /* video_signal_type_present_flag: 1 bit */
    VN_CHECK(bitstreamReadBit(&bitstream, &bit));
    VN_DEBUG_SYNTAX("    video_signal_type: %d\n", bit);

    if (bit) {
        vuiInfoOut->flags |= PSS_VUIF_VIDEO_SIGNAL_TYPE_PRESENT;

        /* video_format: 3 bits */
        VN_CHECK(bitstreamReadBits(&bitstream, 3, &bits));
        vuiInfoOut->video_format = (lcevc_vui_video_format)bits;
        VN_DEBUG_SYNTAX("      video_format: %u\n", vuiInfoOut->video_format);

        /* video_full_range_flag: 1 bit */
        VN_CHECK(bitstreamReadBit(&bitstream, &bit));
        if (bit) {
            vuiInfoOut->flags |= PSS_VUIF_VIDEO_SIGNAL_FULL_RANGE_FLAG;
        }
        VN_DEBUG_SYNTAX("      video_full_range: %d\n", bit);

        /* colour_description_present_flag: 1 bit */
        VN_CHECK(bitstreamReadBit(&bitstream, &bit));
        VN_DEBUG_SYNTAX("      colour_description_present: %d\n", bit);

        if (bit) {
            vuiInfoOut->flags |= PSS_VUIF_VIDEO_SIGNAL_COLOUR_DESC_PRESENT;

            /* colour_primaries: 8 bits */
            VN_CHECK(bitstreamReadBits(&bitstream, 8, &bits));
            vuiInfoOut->colour_primaries = (uint8_t)bits;

            /* transfer_characteristics: 8 bits */
            VN_CHECK(bitstreamReadBits(&bitstream, 8, &bits));
            vuiInfoOut->transfer_characteristics = (uint8_t)bits;

            /* matrix_coefficients: 8 bits*/
            VN_CHECK(bitstreamReadBits(&bitstream, 8, &bits));
            vuiInfoOut->matrix_coefficients = (uint8_t)bits;

            VN_DEBUG_SYNTAX("        colour_primaries: %u\n", vuiInfoOut->colour_primaries);
            VN_DEBUG_SYNTAX("        transfer_characteristics: %u\n", vuiInfoOut->transfer_characteristics);
            VN_DEBUG_SYNTAX("        matrix_coefficients: %u\n", vuiInfoOut->matrix_coefficients);
        }
    }

    /* chroma_loc_info_present_flag: 1 bit*/
    VN_CHECK(bitstreamReadBit(&bitstream, &bit));
    VN_DEBUG_SYNTAX("    chroma_loc_info_present: %d\n", bit);

    if (bit) {
        vuiInfoOut->flags |= PSS_VUIF_CHROMA_LOC_INFO_PRESENT;

        /* chroma_sample_loc_type_top_field: ue(v) */
        VN_CHECK(bitstreamReadExpGolomb(&bitstream, &vuiInfoOut->chroma_sample_loc_type_top_field));

        /* chroma_sample_loc_type_bottom_field: ue(v) */
        VN_CHECK(bitstreamReadExpGolomb(&bitstream, &vuiInfoOut->chroma_sample_loc_type_bottom_field));

        VN_DEBUG_SYNTAX("      chroma_sample_loc_type_top_field: %u\n",
                        vuiInfoOut->chroma_sample_loc_type_top_field);
        VN_DEBUG_SYNTAX("      chroma_sample_loc_type_bottom_field: %u\n",
                        vuiInfoOut->chroma_sample_loc_type_bottom_field);
    }

    /* Finally seek the byte-stream forward */
    return bytestreamSeek(stream, vuiSize);
}

static int32_t parseSFilterPayload(ByteStream_t* stream, DeserialisedData_t* output)
{
    int32_t res = 0;
    uint8_t sfilterByte;
    VN_CHECK(bytestreamReadU8(stream, &sfilterByte));

    output->sharpenType = (SharpenType_t)((sfilterByte & 0xE0) >> 5);
    const uint8_t signalledSharpenStrength = (sfilterByte & 0x1F);
    output->sharpenStrength = (signalledSharpenStrength + 1) * 0.01f;
    VN_DEBUG_SYNTAX("    sharpen_type: %s\n", sharpenTypeToString(output->sharpenType));
    VN_DEBUG_SYNTAX("    sharpen_strength: %d [%f]\n", signalledSharpenStrength, output->sharpenStrength);
    return res;
}

static int32_t parseHDRPayload(Logger_t log, ByteStream_t* stream, lcevc_hdr_info* hdrInfoOut,
                               lcevc_deinterlacing_info* deinterlacingInfoOut)
{
    int32_t res = 0;
    uint8_t byte = 0;

    VN_CHECK(bytestreamReadU8(stream, &byte));

    /* tone_mapper_location: 1 bit */
    uint8_t toneMapperLocation = (byte >> 7) & 0b1;
    VN_DEBUG_SYNTAX("    tone_mapper_location: %u\n", toneMapperLocation);
    /* tone_mapper_type: 5 bit */
    uint8_t toneMapperType = (byte >> 2) & 0b11111;
    VN_DEBUG_SYNTAX("    tone_mapper_type: %u\n", toneMapperType);
    /* tone_mapper_data_present_flag: 1 bit */
    uint8_t toneMapperDataPresentFlag = (byte >> 1) & 0b1;
    VN_DEBUG_SYNTAX("    tone_mapper_data_present_flag: %u\n", toneMapperDataPresentFlag);
    /* deinterlacer_enabled_flag: 1 bit */
    uint8_t deinterlacerEnabledFlag = byte & 0b1;
    VN_DEBUG_SYNTAX("    deinterlacer_enabled_flag: %u\n", deinterlacerEnabledFlag);

    if (toneMapperDataPresentFlag) {
        /*  tone_mapper.size: multibyte */
        uint64_t toneMapperSize = 0;
        VN_CHECK(bytestreamReadMultiByte(stream, &toneMapperSize));
        VN_DEBUG_SYNTAX("        tone_mapper_size: %" PRIu64 "\n", toneMapperSize);
        /*  tone_mapper.payload: tone_mapper.size */
        // Skip tonemapper data as not supported yet
        VN_CHECK(bytestreamSeek(stream, (size_t)toneMapperSize));
    }
    if (toneMapperType == 31) {
        /* tone_mapper_type_extended: 8 bit */
        VN_CHECK(bytestreamReadU8(stream, &toneMapperType));
        VN_DEBUG_SYNTAX("        tone_mapper_type_extended: %u\n", toneMapperType);
    }
    int8_t deinterlacerType = -1;
    uint8_t topFieldFirstFlag = 0;
    if (deinterlacerEnabledFlag) {
        VN_CHECK(bytestreamReadU8(stream, &byte));

        /* deinterlacer_type: 4 bit */
        deinterlacerType = (byte >> 4) & 0b1111;
        VN_DEBUG_SYNTAX("        deinterlacer_type: %u\n", deinterlacerType);
        /* top_field_first_flag: 1 bit */
        topFieldFirstFlag = (byte >> 3) & 0b1;
        VN_DEBUG_SYNTAX("        top_field_first_flag: %u\n", topFieldFirstFlag);
        /* reserved_zeros_3bit: 3 bit */
        if (byte & 0b111) {
            VN_ERROR(log, "hdr_payload_global_config: reserved_zeros_3bit is non zero\n");
            return -1;
        }
    }

    // Set ctx
    hdrInfoOut->flags |= LCEVC_HDRF_HDR_PAYLOAD_GLOBAL_CONFIG_PRESENT;
    hdrInfoOut->tonemapper_config[toneMapperLocation].type = toneMapperType;
    if (toneMapperDataPresentFlag) {
        hdrInfoOut->flags |= LCEVC_HDRF_TONE_MAPPER_DATA_PRESENT;
    }
    if (deinterlacerEnabledFlag) {
        hdrInfoOut->flags |= LCEVC_HDRF_DEINTERLACER_ENABLED;
        deinterlacingInfoOut->deinterlacer_type = deinterlacerType;
        deinterlacingInfoOut->top_field_first_flag = topFieldFirstFlag;
    }
    return res;
}

/* 7.3.10 (Table-14) */
static int32_t parseBlockAdditionalInfo(Logger_t log, ByteStream_t* stream,
                                        lcevc_hdr_info* hdrInfoOut, lcevc_vui_info* vuiInfoOut,
                                        lcevc_deinterlacing_info* deinterlacingInfoOut,
                                        DeserialisedData_t* deserialisedOut, uint32_t blockSize)
{
    int32_t res = 0;

    if (blockSize == 0) {
        VN_ERROR(log,
                 "stream: Additional info block size is 0, this is not possible in the standard\n");
        return -1;
    }

    uint8_t byte = 0;
    VN_CHECK(bytestreamReadU8(stream, &byte));
    const AdditionalInfoType_t infoType = (AdditionalInfoType_t)byte;
    VN_DEBUG_SYNTAX("  additional_info_type: %s\n", additionalInfoTypeToString(infoType));

    switch (infoType) {
        case AIT_SEI:
            VN_CHECK(parseSEIPayload(stream, hdrInfoOut, deserialisedOut, blockSize - 1));
            break;
        case AIT_VUI: VN_CHECK(parseVUIParameters(stream, vuiInfoOut, blockSize - 1)); break;
        case AIT_SFilter: VN_CHECK(parseSFilterPayload(stream, deserialisedOut)); break;
        case AIT_HDR:
            VN_CHECK(parseHDRPayload(log, stream, hdrInfoOut, deinterlacingInfoOut));
            break;
        default:
            VN_DEBUG_SYNTAX("    unsupported additional info type, skipping %d bytes\n", blockSize - 1);
            return bytestreamSeek(stream, blockSize - 1);
    }

    return 0;
}

/* Return 1 when using parse_mode == Parse_GlobalConfig and global config has been hit*/
static int32_t parseBlock(Memory_t memory, Logger_t log, ByteStream_t* stream, lcevc_hdr_info* hdrOut,
                          lcevc_vui_info* vuiOut, lcevc_deinterlacing_info* deinterlacingOut,
                          DeserialisedData_t* deserialisedOut, ParseType_t parseMode,
                          perseus_pipeline_mode pipelineMode, bool useOldCodeLengths)
{
    // @todo(bob): Remove parse mode, think its probably not exactly what we're after.
    // @todo(bob): swap to using size_t for things that are sized in bytes.

    int32_t res = 0;

    /* Load block header. */
    uint8_t data;
    VN_CHECK(bytestreamReadU8(stream, &data));
    const BlockType_t blockType = (BlockType_t)(data & 0x1F);
    const SignalledBlockSize_t blockSizeSignal = (SignalledBlockSize_t)((data & 0xE0) >> 5);

    /* Determine block byte size. */
    uint32_t blockSize = 0;

    if (blockSizeSignal == BS_Custom) {
        uint64_t customBlockSize;
        VN_CHECK(bytestreamReadMultiByte(stream, &customBlockSize));

        if (customBlockSize > 0xFFFFFFFF) {
            VN_ERROR(log, "stream: Invalid custom block size, expect < 32-bits used, value is: %" PRIu64 "\n",
                     customBlockSize);
            return -1;
        }

        blockSize = (uint32_t)customBlockSize;
    } else {
        VN_CHECK(blockSizeFromEnum(blockSizeSignal, &blockSize));
    }

    /* Process each block. */
    const size_t initialOffset = stream->offset;

    VN_DEBUG_SYNTAX("Block: %s - size: %u\n", blockTypeToString(blockType), blockSize);

    if (parseMode == Parse_Full) {
        switch (blockType) {
            case BT_SequenceConfig:
                VN_CHECK(parseBlockSequenceConfig(stream, deserialisedOut));
                break;
            case BT_GlobalConfig:
                VN_CHECK(parseBlockGlobalConfig(log, stream, deserialisedOut));
                break;
            case BT_PictureConfig:
                VN_CHECK(parseBlockPictureConfig(stream, deserialisedOut));
                break;
            case BT_EncodedData:
                VN_CHECK(parseEncodedData(memory, log, stream, deserialisedOut, pipelineMode));
                break;
            case BT_EncodedDataTiled:
                VN_CHECK(parseEncodedDataTiled(memory, log, stream, deserialisedOut, useOldCodeLengths));
                break;
            case BT_AdditionalInfo:
                VN_CHECK(parseBlockAdditionalInfo(log, stream, hdrOut, vuiOut, deinterlacingOut,
                                                  deserialisedOut, blockSize));
                break;
            case BT_Filler: VN_CHECK(parseBlockFiller(stream, blockSize)); break;
            default: {
                VN_WARNING(log, "Unrecognised block type received, skipping: %u\n", blockType);
                bytestreamSeek(stream, blockSize);
                break;
            }
        }
    } else if (parseMode == Parse_GlobalConfig) {
        if (blockType == BT_GlobalConfig) {
            VN_CHECK(parseBlockGlobalConfig(log, stream, deserialisedOut));
            res = 1;
        } else {
            bytestreamSeek(stream, blockSize);
        }
    } else {
        VN_ERROR(log, "Unrecognised parse_mode specified in parse_block() %u\n", parseMode);
        return -1;
    }

    VN_DEBUG_SYNTAX("\n");

    /* Handle block misread. */
    if ((stream->offset - initialOffset) - blockSize) {
        VN_ERROR(log, "stream: Block parser error. Initial offset: %u, Current offset: %u, Expected offset: %u\n",
                 initialOffset, stream->offset, initialOffset + blockSize);
        return -1;
    }

    return res;
}

/*------------------------------------------------------------------------------*/

void deserialiseInitialise(Memory_t memory, DeserialisedData_t* data)
{
    memset(data, 0, sizeof(DeserialisedData_t));

    data->memory = memory;
    data->chroma = CT420;
    data->baseDepth = Depth8;
    data->enhaDepth = Depth8;
    data->picType = PTFrame;
    data->upscale = USLinear;
    data->scalingModes[LOQ0] = Scale2D;
    data->scalingModes[LOQ1] = Scale0D;
    data->chromaStepWidthMultiplier = QDefaultChromaSWMultiplier;

    vnovaConfigReset(&data->vnova_config);
}

void deserialiseRelease(DeserialisedData_t* data)
{
    if (data->unencapsulatedData != NULL) {
        VN_FREE(data->memory, data->unencapsulatedData);
        data->unencapsulatedData = NULL;
        data->unencapsulatedSize = 0;
    }

    if (data->chunks) {
        VN_FREE(data->memory, data->chunks);
        data->chunks = NULL;
    }
}

void deserialiseDump(Logger_t log, const char* debugConfigPath, DeserialisedData_t* data)
{
    if (data == NULL) {
        VN_ERROR(log, "Unable to dump, data is invalid");
        return;
    }

    FILE* file = fopen(debugConfigPath, "w");

    if (file == NULL) {
        VN_ERROR(log, "Unable to open \"%s\"", debugConfigPath);
        return;
    }

    fwrite("{\n", 1, 2, file);
    fprintf(file, "    \"chroma\": \"%s\",\n", chromaToString(data->chroma));
    fprintf(file, "    \"base_depth\": \"%s\",\n", bitdepthToString(data->baseDepth));
    fprintf(file, "    \"enhancement_depth\": \"%s\",\n", bitdepthToString(data->enhaDepth));
    fprintf(file, "    \"width\": %u,\n", data->width);
    fprintf(file, "    \"height\": %u,\n", data->height);
    fprintf(file, "    \"upsample\": \"%s\",\n", upscaleTypeToString(data->upscale));
    fprintf(file, "    \"scaling_mode_level0\": \"%s\",\n", scalingModeToString(data->scalingModes[LOQ0]));
    fprintf(file, "    \"scaling_mode_level1\": \"%s\",\n", scalingModeToString(data->scalingModes[LOQ1]));
    fprintf(file, "    \"use_predicted_average\": %s,\n", data->usePredictedAverage ? "true" : "false");
    fprintf(file, "    \"temporal_enabled\": %s,\n", data->temporalEnabled ? "true" : "false");

    if (data->temporalEnabled) {
        fprintf(file, "    \"temporal_use_reduced_signalling\": %s,\n",
                data->temporalUseReducedSignalling ? "true" : "false");
    }

    fprintf(file, "    \"dither_type\": \"%s\",\n", ditherTypeToString(data->ditherType));
    fprintf(file, "    \"use_deblocking\": %s,\n", data->deblock.enabled ? "true" : "false");

    if (data->deblock.enabled) {
        fprintf(file, "    \"deblocking_coefficient_corner\": %u,\n", data->deblock.corner);
        fprintf(file, "    \"deblocking_coefficient_side\": %u,\n", data->deblock.corner);
    }

    fprintf(file, "    \"use_dequant_offset\": %s,\n", data->useDequantOffset ? "true" : "false");

    if (data->useDequantOffset) {
        fprintf(file, "    \"dequant_offset\": %d,\n", data->dequantOffset);
    }

    fprintf(file, "    \"sharpen_type\": \"%s\",\n", sharpenTypeToString(data->sharpenType));
    fprintf(file, "    \"sharpen_strength\": %.2f,\n", data->sharpenStrength);

    /* Have this last so there's no trailing comma */
    fprintf(file, "    \"num_layers\": %u\n", data->numLayers);
    fwrite("}\n", 1, 2, file);
    fclose(file);
}

int32_t deserialiseGetTileLayerChunks(const DeserialisedData_t* data, int32_t planeIndex,
                                      LOQIndex_t loq, int32_t tileIndex, Chunk_t** chunks)
{
    if (!data || !chunks) {
        return -1;
    }

    if (planeIndex < 0 || planeIndex > data->numPlanes) {
        return -1;
    }

    if (loq != LOQ0 && loq != LOQ1) {
        return -1;
    }

    if (data->enhancementEnabled && data->chunks) {
        const int32_t chunkIndex = getLayerChunkIndex(data, planeIndex, loq, tileIndex, 0);

        if (tileIndex < 0 || tileIndex >= data->tileCount[planeIndex][loq]) {
            return -1;
        }

        assert(chunkIndex < (int32_t)data->numChunks);

        *chunks = &data->chunks[chunkIndex];
    } else {
        *chunks = NULL;
    }

    return 0;
}

int32_t deserialiseGetTileTemporalChunk(const DeserialisedData_t* data, int32_t planeIndex,
                                        int32_t tileIndex, Chunk_t** chunk)
{
    if (!data || !chunk) {
        return -1;
    }

    if (planeIndex < 0 || planeIndex > data->numPlanes) {
        return -1;
    }

    if (data->temporalEnabled && data->temporalChunkEnabled && data->chunks) {
        const int32_t chunkIndex = data->tileChunkTemporalIndex[planeIndex] + tileIndex;

        if (tileIndex < 0 || tileIndex >= data->tileCount[planeIndex][LOQ0]) {
            return -1;
        }

        assert(chunkIndex < (int32_t)data->numChunks);

        *chunk = &data->chunks[chunkIndex];
    } else {
        *chunk = NULL;
    }

    return 0;
}

void deserialiseCalculateSurfaceProperties(const DeserialisedData_t* data, LOQIndex_t loq,
                                           uint32_t planeIndex, uint32_t* width, uint32_t* height)
{
    uint32_t calcWidth = data->width;
    uint32_t calcHeight = data->height;

    /* Scale to the correct LOQ. */
    for (int32_t i = 0; i < loq; ++i) {
        const ScalingMode_t loqScalingMode = data->scalingModes[i];

        if (loqScalingMode != Scale0D) {
            calcWidth = (calcWidth + 1) >> 1;

            if (loqScalingMode == Scale2D) {
                calcHeight = (calcHeight + 1) >> 1;
            }
        }
    }

    /* Scale to correct plane */
    if (planeIndex > 0) {
        const Chroma_t chroma = data->chroma;

        if (chroma == CT420 || chroma == CT422) {
            calcWidth = (calcWidth + 1) >> 1;

            if (chroma == CT420) {
                calcHeight = (calcHeight + 1) >> 1;
            }
        }
    }

    *width = calcWidth;
    *height = calcHeight;
}

int32_t deserialise(Memory_t memory, Logger_t log, const uint8_t* serialised, uint32_t serialisedSize,
                    DeserialisedData_t* deserialisedOut, Context_t* ctxOut, ParseType_t parseMode)
{
    ByteStream_t stream;
    int32_t res = 0;

    VN_DEBUG_SYNTAX("------>>> Begin: %" PRId64 "\n\n", ctx->deserialiseCount);

    // @todo(bob): Don't really need a byte-stream for unencapsulation.
    if (bytestreamInitialise(&stream, serialised, serialisedSize) < 0) {
        return -1;
    }

    deserialisedOut->currentGlobalConfigSet = false;
    deserialisedOut->currentVnovaConfigSet = false;
    deserialisedOut->pictureConfigSet = false;

    VN_CHECK(unencapsulate(memory, log, deserialisedOut, &stream));

    if (bytestreamInitialise(&stream, deserialisedOut->unencapsulatedData,
                             deserialisedOut->unencapsulatedSize) < 0) {
        return -1;
    }

    while (bytestreamRemaining(&stream) > 0) {
        VN_CHECK(parseBlock(memory, log, &stream, &(ctxOut->hdrInfo), &(ctxOut->vuiInfo),
                            &(ctxOut->deinterlacingInfo), deserialisedOut, parseMode,
                            ctxOut->pipelineMode, ctxOut->useOldCodeLengths));

        /* global config hit when using parse_mode == Parse_GlobalConfig skip other blocks. */
        if (res == 1) {
            if (parseMode != Parse_GlobalConfig) {
                VN_ERROR(log,
                         "parse_block returned 1 when parse_mode is not Parse_GlobalConfig. \n");
                return -1;
            }

            res = 0;
            break;
        }
    }

    VN_DEBUG_SYNTAX("------>>> End: %" PRId64 "\n\n", ctx->deserialiseCount);

    ctxOut->deserialiseCount += 1;

    return res;
}

/*------------------------------------------------------------------------------*/
