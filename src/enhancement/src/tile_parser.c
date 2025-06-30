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

#include "tile_parser.h"

#include "entropy.h"

#include <LCEVC/common/check.h>

bool tiledRLEDecoderInitialize(TiledRLEDecoder* decoder, ByteStream* reader)
{
    decoder->reader = reader;

    /* Decode initial symbol and first run. */
    VNCheckB(bytestreamReadU8(reader, &decoder->currentSymbol));

    if (decoder->currentSymbol != 0x00 && decoder->currentSymbol != 0x01) {
        return false;
    }

    VNCheckB(bytestreamReadMultiByte(reader, &decoder->runLength));

    return true;
}

bool tiledRLEDecoderRead(TiledRLEDecoder* decoder, uint8_t* destination)
{
    if (decoder->runLength == 0) {
        /* Decode next run length and flip the symbol. */
        VNCheckB(bytestreamReadMultiByte(decoder->reader, &decoder->runLength));
        decoder->currentSymbol = !decoder->currentSymbol;

        if (decoder->runLength == 0) {
            return true;
        }
    }

    *destination = decoder->currentSymbol;
    decoder->runLength--;

    return true;
}

bool tiledSizeDecoderInitialize(LdcMemoryAllocator* allocator, TiledSizeDecoder* decoder,
                                uint32_t numSizes, ByteStream* stream,
                                LdeTileCompressionSizePerTile type, uint8_t bitstreamVersion)
{
    const EntropyDecoderType decoderType = (type == TCSPTTPrefix) ? EDTSizeUnsigned : EDTSizeSigned;

    /* Do not attempt to read sizes if none are signaled. */
    if (numSizes == 0) {
        return true;
    }

    /* Allocate buffer to store the decoded sizes. */
    if (decoder->numSizes < numSizes) {
        int16_t* newSizes = VNReallocateArray(allocator, &decoder->allocation, int16_t, numSizes);

        if (!newSizes) {
            /* Clean up.*/
            VNFree(allocator, &decoder->allocation);
            decoder->sizes = NULL;

            VNLogError("Unable to allocate tile sizes buffer");
            return false;
        }

        decoder->sizes = newSizes;
    }

    decoder->allocator = allocator;
    decoder->currentIndex = 0;
    decoder->numSizes = numSizes;

    /* Now parse the sizes */
    LdeChunk chunk = {0};
    chunk.entropyEnabled = 1;
    chunk.rleOnly = 0;
    chunk.data = bytestreamCurrent(stream);
    chunk.size = bytestreamRemaining(stream);

    EntropyDecoder layerDecoder = {0};
    VNCheckB(entropyInitialize(&layerDecoder, &chunk, decoderType, bitstreamVersion));

    VNLogVerbose("Tiled size decoder initialize");

    for (uint32_t i = 0; i < numSizes; ++i) {
        VNCheckB(entropyDecodeSize(&layerDecoder, &decoder->sizes[i]));
        VNLogVerbose("Size: %d", decoder->sizes[i]);
    }

    const uint32_t consumedBytes = entropyGetConsumedBytes(&layerDecoder);
    VNLogVerbose("Consumed bytes: %u", consumedBytes);

    VNCheckB(bytestreamSeek(stream, consumedBytes));

    if (type == TCSPTTPrefixOnDiff) {
        for (uint32_t i = 1; i < numSizes; ++i) {
            decoder->sizes[i] += decoder->sizes[i - 1];
        }
    }

    return true;
}

void tiledSizeDecoderRelease(TiledSizeDecoder* decoder)
{
    if (decoder && decoder->sizes) {
        VNFree(decoder->allocator, &decoder->allocation);
    }
}

int16_t tiledSizeDecoderRead(TiledSizeDecoder* decoder)
{
    if (decoder->currentIndex < decoder->numSizes) {
        return decoder->sizes[decoder->currentIndex++];
    }

    return -1;
}
