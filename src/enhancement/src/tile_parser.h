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

#ifndef VN_LCEVC_ENHANCEMENT_TILE_PARSER_H
#define VN_LCEVC_ENHANCEMENT_TILE_PARSER_H

#include "bytestream.h"

#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/bitstream_types.h>

/*! @brief State for the RLE decoding of the compressed syntax for the chunk enabled
 *        flag.
 *
 * @note This is an identical scheme to the layers decoders temporal signalling
 *       decoder - however to use that here would require building up an actual
 *       layer decoder with huffman state & bitstream reader.
 */
typedef struct TiledRLEDecoder
{
    ByteStream* reader;
    uint8_t currentSymbol;
    uint64_t runLength;
} TiledRLEDecoder;

typedef struct TiledSizeDecoder
{
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation allocation;
    int16_t* sizes;
    uint32_t currentIndex;
    uint32_t numSizes;
} TiledSizeDecoder;

bool tiledRLEDecoderInitialize(TiledRLEDecoder* decoder, ByteStream* reader);

bool tiledRLEDecoderRead(TiledRLEDecoder* decoder, uint8_t* destination);

bool tiledSizeDecoderInitialize(LdcMemoryAllocator* allocator, TiledSizeDecoder* decoder,
                                uint32_t numSizes, ByteStream* stream,
                                LdeTileCompressionSizePerTile type, uint8_t bitstreamVersion);

void tiledSizeDecoderRelease(TiledSizeDecoder* decoder);

int16_t tiledSizeDecoderRead(TiledSizeDecoder* decoder);

#endif // VN_LCEVC_ENHANCEMENT_TILE_PARSER_H
