/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#include "entropy.h"

#include "huffman.h"

#include <assert.h>
#include <LCEVC/common/check.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*------------------------------------------------------------------------------*/

/* Number of Huffman streams for a size-decoder */
enum
{
    HuffSizeCount = 2
};

/*! \brief temporal run length transition table */
static const uint8_t kNextTemporalContext[HuffTemporalCount][2] = {{HuffTemporalOne, HuffTemporalZero},
                                                                   {HuffTemporalZero, HuffTemporalOne}};

/*------------------------------------------------------------------------------*/

/*! \brief initialize the chunk for the entropy decoder. This loads up the HuffmanStream
 *         reader for rle decoding when the syntax signals the case otherwise it
 *         loads up the huffman decoder.
 *  \param state             Layer decoder state to initialise.
 *  \param chunk             The chunk to decode from.
 *  \param bitstreamVersion  Bitstream version (only present in some streams).
 */
static bool chunkInitialize(EntropyDecoder* state, const LdeChunk* chunk, const uint8_t bitstreamVersion)
{
    state->entropyEnabled = chunk->entropyEnabled;

    /* Initialize huffman decoder */
    if (!chunk->entropyEnabled) {
        return true;
    }

    if (chunk->rleOnly) {
        state->rleOnly = true;
        state->rleData = chunk->data;
        return true;
    }

    if (chunk->size <= 0) {
        return true;
    }

    /* Load up the stream with huffman tables contained at the beginning of the chunk. */
    VNCheckB(huffmanStreamInitialize(&state->hstream, chunk->data, chunk->size));

    if (state->type == EDTDefault) {
        /* The default type of entropy decoder (consisting of 3 huffman streams: lsb, msb, and rl)
         * uses a triple-decoder as an optimisation. */
        VNCheckB(huffmanTripleInitialize(&state->comboHuffman, &state->hstream, bitstreamVersion));
    } else {
        /* Other entropy decodes just have two huffman streams (or, HuffTemporalCount, which equals
         * HuffSizeCount), so initialize each. */
        for (uint8_t huffType = 0; huffType < HuffTemporalCount; huffType++) {
            huffmanManualInitializeWithLut(&state->huffman[huffType].manualState,
                                           &state->huffman[huffType].table, &state->hstream,
                                           bitstreamVersion);
        }
    }

    return true;
}

static inline bool huffmanSingleDecode(const HuffmanSingleDecoder_t* decoder, HuffmanStream* stream,
                                       uint8_t* symbolOut)
{
    /* This order is optimized for streams which are frequently single-symbol, like MSB and
     * temporal (especially HuffTemporalOne). */
    if (huffmanGetSingleSymbol(&decoder->manualState, symbolOut)) {
        return true;
    }
    if (huffmanLutDecode(&decoder->table, stream, symbolOut)) {
        return true;
    }
    if (huffmanManualDecode(&decoder->manualState, stream, symbolOut)) {
        return true;
    }

    return false;
}

static inline int32_t getNextSymbolRLEOnly(EntropyDecoder* state)
{
    return state->rleData[state->rawOffset++];
}

static inline void toggleTemporalState(EntropyDecoder* state, const uint8_t symbol)
{
    if (state->rawOffset == 1) {
        /* The first symbol read is always a raw value indicating the initial state */
        state->currHuff = symbol & 0x01;
    } else {
        /* Subsequent symbols will flip the state when the symbol signals no further data */
        state->currHuff = kNextTemporalContext[state->currHuff][(symbol & 0x80) >> 7];
    }
}

static int32_t getNextSymbolTemporalAndRLEOnly(EntropyDecoder* state)
{
    const uint8_t symbol = getNextSymbolRLEOnly(state);
    toggleTemporalState(state, symbol);
    return symbol;
}

static int32_t getNextSymbolTemporalAndHuffman(EntropyDecoder* state)
{
    uint8_t symbol = 0;

    if (state->rawOffset == 0) {
        /* First byte is sent raw to determine initial state */
        uint32_t bits = 0;
        VNCheckI(huffmanStreamReadBits(&state->hstream, 8, &bits));
        symbol = (uint8_t)bits;
    } else {
        /* Huffman decode next run. */
        if (!huffmanSingleDecode(&state->huffman[state->currHuff], &state->hstream, &symbol)) {
            VNLogError("Huffman single decode failed");
            return -1;
        }
    }

    state->rawOffset++;

    toggleTemporalState(state, symbol);

    return symbol;
}

/*------------------------------------------------------------------------------*/

bool entropyInitialize(EntropyDecoder* state, const LdeChunk* chunk, const EntropyDecoderType type,
                       const uint8_t bitstreamVersion)
{
    if (state == NULL || chunk == NULL) {
        VNLogError("Cannot initialize entropy decoder - state or chunk NULL");
        return false;
    }

    /* Shared state. */
    state->currHuff = 0;
    state->rawOffset = 0;
    state->rleOnly = false;
    state->rleData = NULL;
    state->entropyEnabled = true;
    state->type = type;

    /* Syntax specific setup. */
    VNCheckB(chunkInitialize(state, chunk, bitstreamVersion));

    return true;
}

#define VN_ENTROPY_DECODE_DEFINE(symbolGetterFn)                                       \
    static int32_t entropyDecode_##symbolGetterFn(EntropyDecoder* state, int16_t* out) \
    {                                                                                  \
        int16_t value = 0;                                                             \
        int32_t zeros = 0;                                                             \
        int32_t symbol = 0;                                                            \
                                                                                       \
        if ((symbol = symbolGetterFn(state)) < 0) {                                    \
            return symbol;                                                             \
        }                                                                              \
                                                                                       \
        if (nextSymbolIsMSB(symbol)) {                                                 \
            value = (int16_t)(symbol & 0xFE);                                          \
                                                                                       \
            if ((symbol = symbolGetterFn(state)) < 0) {                                \
                return symbol;                                                         \
            }                                                                          \
                                                                                       \
            int32_t exp = (symbol & 0x7f) << 8 | value;                                \
            value = (int16_t)(exp - 0x4000);                                           \
            value >>= 1;                                                               \
        } else {                                                                       \
            value = (int16_t)((symbol & 0x7E) - 0x40);                                 \
            value >>= 1;                                                               \
        }                                                                              \
                                                                                       \
        *out = value;                                                                  \
                                                                                       \
        while (nextSymbolIsRL(symbol)) {                                               \
            if ((symbol = symbolGetterFn(state)) < 0) {                                \
                return symbol;                                                         \
            }                                                                          \
                                                                                       \
            zeros = (zeros << 7) | (symbol & 0x7f);                                    \
        }                                                                              \
                                                                                       \
        return zeros;                                                                  \
    }

VN_ENTROPY_DECODE_DEFINE(getNextSymbolRLEOnly)

#define VN_ENTROPY_DECODE_TEMPORAL_DEFINE(symbolGetterFn)                                             \
    static int32_t entropyDecodeTemporal_##symbolGetterFn(EntropyDecoder* state, TemporalSignal* out) \
    {                                                                                                 \
        /* For temporal, "value" is just a 1 or a 0, telling us which temporal state we're in when    \
         * we leave this function */                                                                  \
        uint8_t value = state->currHuff;                                                              \
        int32_t count = 0;                                                                            \
        int32_t symbol = 0;                                                                           \
                                                                                                      \
        /* First symbol is always sent as raw, so we know which state we are starting with */         \
        if (state->rawOffset == 0) {                                                                  \
            if ((symbol = symbolGetterFn(state)) < 0) {                                               \
                return symbol;                                                                        \
            }                                                                                         \
                                                                                                      \
            value = symbol & 0x01;                                                                    \
        }                                                                                             \
                                                                                                      \
        /* Read in next count. */                                                                     \
        do {                                                                                          \
            if ((symbol = symbolGetterFn(state)) < 0) {                                               \
                return symbol;                                                                        \
            }                                                                                         \
                                                                                                      \
            count = count << 7 | (symbol & 0x7f);                                                     \
        } while (symbol & 0x80);                                                                      \
                                                                                                      \
        *out = (TemporalSignal)value;                                                                 \
                                                                                                      \
        return count;                                                                                 \
    }

VN_ENTROPY_DECODE_TEMPORAL_DEFINE(getNextSymbolTemporalAndRLEOnly)
VN_ENTROPY_DECODE_TEMPORAL_DEFINE(getNextSymbolTemporalAndHuffman)

int32_t entropyDecode(EntropyDecoder* state, int16_t* out)
{
    assert(state->type == EDTDefault);

    if (!state->entropyEnabled) {
        *out = 0;
        return EntropyNoData;
    }

    if (state->rleOnly) {
        return entropyDecode_getNextSymbolRLEOnly(state, out);
    }

    return huffmanTripleDecode(&state->comboHuffman, &state->hstream, out);
}

int32_t entropyDecodeTemporal(EntropyDecoder* state, TemporalSignal* out)
{
    assert(state->type == EDTTemporal);

    if (!state->entropyEnabled) {
        *out = 0;
        return EntropyNoData;
    }

    if (state->rleOnly) {
        return entropyDecodeTemporal_getNextSymbolTemporalAndRLEOnly(state, out);
    }
    return entropyDecodeTemporal_getNextSymbolTemporalAndHuffman(state, out);
}

bool entropyDecodeSize(EntropyDecoder* state, int16_t* size)
{
    assert(state->type == EDTSizeUnsigned || state->type == EDTSizeSigned);
    assert(state->rleOnly == 0);

    uint8_t lsb = 0;
    VNCheckB(huffmanSingleDecode(&state->huffman[HuffLSB], &state->hstream, &lsb));

    if (lsb & 0x01) {
        uint8_t msb = 0;
        VNCheckB(huffmanSingleDecode(&state->huffman[HuffMSB], &state->hstream, &msb));

        const uint16_t val = (uint16_t)(msb << 7) | (lsb >> 1);

        if (state->type == EDTSizeSigned) {
            *size = (int16_t)(((val & 0x4000) << 1) | val);
        } else {
            *size = (int16_t)val;
        }
    } else {
        if (state->type == EDTSizeSigned) {
            /* This double cast is here to show explicitly what needs to happen to
             * get the 7-bit signed value to the 16-bit signed output. These are
             * the steps that are happening:
             *    1. Broadcast 7th bit to 8th bit.
             *    2. Reinterpret as int8_t.
             *    3. Cast to int16_t.
             *
             * Without step 2 the value will be treated as a positive value and
             * step 3 is normally just implicitly performed by the compiler.
             */
            const uint8_t val = lsb >> 1;
            *size = (int16_t)(int8_t)(((val & 0x40) << 1) | val);
        } else {
            *size = (int16_t)(lsb >> 1);
        }
    }

    return true;
}

uint32_t entropyGetConsumedBytes(const EntropyDecoder* state)
{
    const uint8_t numUsableBits = state->hstream.wordEndBit - state->hstream.wordStartBit;
    return (uint32_t)((state->hstream.bitsRead + 7 - numUsableBits) >> 3);
}

/*------------------------------------------------------------------------------*/