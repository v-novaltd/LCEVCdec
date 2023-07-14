/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_DESERIALIZER_H_
#define VN_DEC_CORE_DESERIALIZER_H_

#include "common/types.h"
#include "decode/dequant.h"
#include "surface/upscale.h"

/*------------------------------------------------------------------------------*/

typedef struct Context Context_t;

/*------------------------------------------------------------------------------*/

typedef enum ParseType
{
    Parse_Full,
    Parse_GlobalConfig,
} ParseType_t;

typedef struct Chunk
{
    uint8_t rleOnly;
    size_t size;
    const uint8_t* data;
    uint8_t entropyEnabled;
} Chunk_t;

typedef struct VNConfig
{
    bool valid;
    uint8_t bitstreamVersion;
} VNConfig_t;

typedef struct Deblock
{
    bool enabled;    /**< Whether deblocking is enabled, noting that
                      *   if it is false the values for `corner` and
                      *   `side` are undefined */
    uint32_t corner; /**< The corner coefficient to use. */
    uint32_t side;   /**< The side coefficient to use */
} Deblock_t;

typedef struct DeserialisedData
{
    Context_t* ctx;
    VNConfig_t vnova_config;

    NALType_t type;
    uint8_t* unencapsulatedData;
    size_t unencapsulatedSize;
    size_t unencapsulatedCapacity;

    Chroma_t chroma;
    BitDepth_t baseDepth;
    BitDepth_t enhaDepth;
    bool loq1UseEnhaDepth;

    PictureType_t picType;
    FieldType_t fieldType;

    uint16_t width;
    uint16_t height;
    UpscaleType_t upscale;
    ScalingMode_t scalingModes[LOQEnhancedCount];
    bool enhancementEnabled;

    uint32_t stepWidths[LOQEnhancedCount];
    uint8_t numPlanes;
    uint8_t numLayers;
    uint32_t numChunks;
    TransformType_t transform;

    uint16_t globalHeight;
    uint8_t usePredictedAverage;
    uint8_t temporalUseReducedSignalling;
    uint8_t temporalEnabled;
    uint8_t temporalRefresh;
    uint8_t temporalChunkEnabled;
    uint8_t temporalStepWidthModifier;
    DitherType_t ditherType;
    uint8_t ditherStrength;
    bool globalConfigSet;
    bool pictureConfigSet;
    Chunk_t* chunks;
    QuantMatrix_t quantMatrix;
    Deblock_t deblock;
    bool useDequantOffset;
    DequantOffsetMode_t dequantOffsetMode;
    int32_t dequantOffset;
    Kernel_t adaptiveUpscaleKernel;
    uint8_t chromaStepWidthMultiplier;
    SharpenType_t sharpenType;
    float sharpenStrength;
    bool entropyEnabled[LOQEnhancedCount];
    perseus_pipeline_mode pipelineMode;
    UserDataConfig_t userData;
    lcevc_conformance_window conformanceWindow;

    TileDimensions_t tileDimensions;
    uint16_t tileWidth[RCMaxPlanes];
    uint16_t tileHeight[RCMaxPlanes];
    TileCompressionSizePerTile_t tileSizeCompression;
    bool tileEnabledPerTileCompressionFlag;
    int32_t tilesAcross[RCMaxPlanes][LOQEnhancedCount];
    int32_t tilesDown[RCMaxPlanes][LOQEnhancedCount];
    int32_t tileCount[RCMaxPlanes][LOQEnhancedCount];
    int32_t tileChunkResidualIndex[RCMaxPlanes][LOQEnhancedCount];
    int32_t tileChunkTemporalIndex[RCMaxPlanes];

    bool currentGlobalConfigSet; /* Set to false at the start of deserialising, is
                                  * set to true if global config exists */

    bool currentVnovaConfigSet; /* Set to false at the start of deserialising, is
                                 * set to true if the V-Nova config exists */
} DeserialisedData_t;

/*------------------------------------------------------------------------------*/

/*! \brief Initialise deserialised data into a default state.
 *
 *  \param data  deserialised data instance to initialise. */
void deserialiseInitialise(Context_t* ctx, DeserialisedData_t* data);

/*! \brief Release allocations on deserialised data. This should be called when
 *		   closing the decoder instance.
 *
 *  \param data  deserialised data to release. */
void deserialiseRelease(DeserialisedData_t* data);

/*! \brief Dump the deserialised data as json.
 *
 *  \param ctx   Decoder context
 *  \param data  deserialised data to dump. */
void deserialiseDump(Context_t* ctx, DeserialisedData_t* data);

/*! \brief Obtain the array of non-temporal (layer) chunk data for a given Plane,
 *         LOQ and tile from the deserialised data.
 *
 * This function will output NULL chunk data if enhancement is disabled. The number
 * of chunks can be determined from num_layers.
 *
 *  \param data        Deserialised data to obtain the layer chunks from.
 *  \param loq         The LOQ index to obtain the layer chunks for.
 *  \param planeIndex  The plane index to obtain the layer chunks for.
 *  \param tileIndex   The tile index to obtain the layer chunks for.
 *  \param chunks      Double pointer to populate with the array of
 *                     chunks to read from. This will be set to NULL if
 *                     layer enhancement is currently disabled.
 *
 *  \return Returns 0 on success, otherwise -1. */
int32_t deserialiseGetTileLayerChunks(DeserialisedData_t* data, int32_t planeIndex, LOQIndex_t loq,
                                      int32_t tileIndex, Chunk_t** chunks);

/*! \brief Obtain a pointer to the temporal chunk data for a given plane from the
 *         deserialised data. This function will output NULL chunk data if temporal
 *         is disabled.
 *
 * \param data        Deserialised data to obtain a temporal chunk from.
 * \param planeIndex  The LOQ index to obtain the temporal chunk for.
 * \param tileIndex   The tile index to obtain the layer chunks for.
 * \param chunk       Double pointer to populate the temporal chunk to read from.
 *                    This will be set to NULL if temporal enhancement is disabled.
 *
 * \return Returns 0 on success, otherwise -1. */
int32_t deserialiseGetTileTemporalChunk(DeserialisedData_t* data, int32_t planeIndex,
                                        int32_t tileIndex, Chunk_t** chunk);

/*! \brief Calculates the correct width and height of a surface for a given loq and
 *         plane
 *
 * \note This function takes scaling mode of each LOQ into account.
 *
 * \param data        Deserialised data containing parsed settings.
 * \param loq         The LOQ of the surface.
 * \param planeIndex  The plane of the surface.
 * \param width       The location to store the calculated width.
 * \param height      The location to store the calculated height.
 */
void deserialiseCalculateSurfaceProperties(const DeserialisedData_t* data, LOQIndex_t loq,
                                           uint32_t planeIndex, uint32_t* width, uint32_t* height);

/*! \brief Deserialise encoded data from a loaded piece of data.
 *  \param ctx             Decoder context
 *  \param serialised      Serialised data to deserialise.
 *  \param serialisedSize  Byte size of the serialised data.
 *  \param output          Deserialised data instance to deserialise into.
 *  \param parseMode       Specify which blocks to be deserialised. */
int32_t deserialise(Context_t* ctx, const uint8_t* serialised, uint32_t serialisedSize,
                    DeserialisedData_t* output, ParseType_t parseMode);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_DESERIALIZER_H_ */