/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "decode/entropy.h"

#include "context.h"

#include <assert.h>
#include <stdio.h>

/*------------------------------------------------------------------------------*/

/*! \brief run length states */
enum
{
    RLELSB,
    RLEMSB,
    RLEZero,
    RLECount
};

/*! \brief binary run length states */
enum
{
    RLEBinaryZero,
    RLEBinaryOne,
    RLEBinaryCount
};

enum
{
    RLESizeCount = 2
};

/*! \brief run length transition table */
static const uint8_t kNextContext[RLECount][4] = {{RLELSB, RLEMSB, RLEZero, RLEMSB},
                                                  {RLELSB, RLELSB, RLEZero, RLEZero},
                                                  {RLELSB, RLELSB, RLEZero, RLEZero}};

/*! \brief binary run length transition table */
static const uint8_t kNextBinaryContext[RLEBinaryCount][2] = {{RLEBinaryOne, RLEBinaryZero},
                                                              {RLEBinaryZero, RLEBinaryOne}};

/*! \brief number of huffman decoders required for a given type. */
static const int32_t kNumStates[EDTCount] = {
    RLECount,       /* LDT_Default */
    RLEBinaryCount, /* LDT_Temporal */
    RLESizeCount,   /* LDT_SizeUnsigned */
    RLESizeCount    /* LDT_SizeSigned */
};

/*------------------------------------------------------------------------------*/

static inline int32_t getNumStatesForType(EntropyDecoderType_t type, int32_t* count)
{
    if ((uint32_t)type < EDTCount) {
        *count = kNumStates[type];
        return 0;
    }

    return -1;
}

/*------------------------------------------------------------------------------*/

/*! \brief initialise the chunk for the entropy decoder. This loads up the bitstream
 *         reader for rle decoding when the syntax signals the case otherwise it
 *         loads up the huffman decoder.
 *  \param ctx 			decoding context
 *  \param state 		layer decoder state to initialise.
 *  \param chunk 		The chunk to decode from.
 *  \param chunk_idx 	chunk index to load.
 */
static int32_t chunkInitialise(Context_t* ctx, EntropyDecoder_t* state, const Chunk_t* chunk)
{
    int32_t res = 0;

    state->chunk = chunk;
    state->entropyEnabled = chunk->entropyEnabled == 1 ? true : false;

    /* Initialise huffman decoder */
    if (chunk->entropyEnabled == 1) {
        if (!chunk->rleOnly) {
            int32_t stateCount = 0;
            VN_CHECK(getNumStatesForType(state->type, &stateCount));

            if (chunk->size > 0) {
                /* Load up bitstream with huffman tables contained at the beginning of the chunk. */
                VN_CHECK(bitstreamInitialise(&state->stream, chunk->data, chunk->size));

                for (int32_t i = 0; i < stateCount; ++i) {
                    VN_CHECK(huffmanInitialise(ctx, &state->huffman[i], &state->stream));
                }

                /* Track number of consumed bits for the tables. */
                state->hstream.stream = state->stream;
                state->hstream.byte = -1;
                state->hstream.bitsRead = bitstreamGetConsumedBits(&state->stream);
            }
        } else {
            state->rleOnly = true;
            state->rleData = chunk->data;
        }
    }

    return 0;
}

/*! \brief get the next run length symbol
 *  \param ctx  decoding context
 *  \param state  state of the decoder to initialize
 *
 *  \return RLE symbol (< 0 on failure) */
static int32_t getNextRLE(EntropyDecoder_t* state)
{
    uint8_t symbol = 0;
    int32_t res = 0;
    int32_t bits = 0;

    /* General case for all syntax versions, they are either reading rle or
     * decoding from the huffman table. */
    if (state->rleOnly) {
        /* RLE decode. */
        symbol = state->rleData[state->rawOffset++];
    } else {
        if (state->type == EDTTemporal) {
            if (state->rawOffset == 0) {
                /* First byte is sent raw to determine initial state */
                VN_CHECK(bitstreamReadBits(&state->hstream.stream, 8, &bits));
                symbol = (uint8_t)bits;
            } else {
                /* Huffman decode next run. */
                VN_CHECK(huffmanDecode(state->ctx, &state->huffman[state->currHuff],
                                       &state->hstream, &symbol));
            }

            state->rawOffset++;
        } else {
            /* Huffman decode */
            uint8_t next = 0;
            VN_CHECK(huffmanDecode(state->ctx, &state->huffman[state->currHuff], &state->hstream, &symbol));

            /* Update the decoder context */
            next = (symbol & 0x01) | (symbol & 0x80) >> 6;
            state->currHuff = kNextContext[state->currHuff][next];
        }
    }

    /* Special case the temporal layer for binary state transition during symbol read */
    if (state->type == EDTTemporal) {
        if (state->rawOffset == 1) {
            /* The first symbol read is always a raw value indicating the initial state */
            state->currHuff = symbol & 0x01;
        } else {
            /* Subsequent symbols will flip the state when the symbol signals no further data */
            state->currHuff = kNextBinaryContext[state->currHuff][(symbol & 0x80) >> 7];
        }
    }

    return symbol;
}

/*------------------------------------------------------------------------------*/

int32_t entropyInitialise(Context_t* ctx, EntropyDecoder_t* state, const Chunk_t* chunk,
                          EntropyDecoderType_t type)
{
    int32_t res = 0;

    if (state == NULL || chunk == NULL) {
        VN_ERROR(ctx->log, "state or chunk NULL\n");
        return -1;
    }

    /* Shared state. */
    state->ctx = ctx;
    state->currHuff = 0;
    state->rawOffset = 0;
    state->rleOnly = false;
    state->rleData = NULL;
    state->entropyEnabled = true;
    state->type = type;

    /* Syntax specific setup. */
    VN_CHECK(chunkInitialise(ctx, state, chunk));

    return 0;
}

void entropyRelease(EntropyDecoder_t* state)
{
    for (int32_t i = 0; i < 3; i++) {
        huffmanRelease(state->ctx, &state->huffman[i]);
    }
}

int32_t entropyDecode(EntropyDecoder_t* state, int16_t* pel)
{
    int16_t value = 0;
    int32_t zeros = 0;
    int32_t symbol = 0;

    assert(state->type == EDTDefault);

    if (!state->entropyEnabled) {
        *pel = 0;
        return EntropyNoData;
    }

    if ((symbol = getNextRLE(state)) < 0) {
        return symbol;
    }

    /* We always start in the low byte state */

    /* If the lsb is set, we also need to decode the high byte */
    if (symbol & 0x01) {
        value = (int16_t)(symbol & 0xFE);

        if ((symbol = getNextRLE(state)) < 0) {
            return symbol;
        }

        int32_t exp = (symbol & 0x7f) << 8 | value;
        value = (int16_t)(exp - 0x4000);
        value >>= 1;
    } else {
        value = (int16_t)((symbol & 0x7E) - 0x40);
        value >>= 1;
    }

    *pel = value;

    /* if the msb is set, we know the next symbol is a run, so decode it */
    if (symbol & 0x80) {
        do {
            if ((symbol = getNextRLE(state)) < 0) {
                return symbol;
            }

            zeros = (zeros << 7) | (symbol & 0x7f);
        } while (symbol & 0x80);
    }

    return zeros;
}

int32_t entropyDecodeTemporal(EntropyDecoder_t* state, uint8_t* pel)
{
    uint8_t value = state->currHuff;
    int32_t count = 0;
    int32_t symbol = 0;

    assert(state->type == EDTTemporal);

    if (!state->entropyEnabled) {
        *pel = 0;
        return EntropyNoData;
    }

    /* First symbol is always sent as raw, so we know which state we are starting with */
    if (state->rawOffset == 0) {
        if ((symbol = getNextRLE(state)) < 0) {
            return symbol;
        }

        value = symbol & 0x01;
    }

    /* Read in next count. */
    do {
        if ((symbol = getNextRLE(state)) < 0) {
            return symbol;
        }

        count = count << 7 | (symbol & 0x7f);
    } while (symbol & 0x80);

    *pel = value;

    return count;
}

int32_t entropyDecodeSize(EntropyDecoder_t* state, int16_t* size)
{
    int32_t res = 0;

    assert(state->type == EDTSizeUnsigned || state->type == EDTSizeSigned);
    assert(state->rleOnly == 0);

    uint8_t lsb = 0;
    VN_CHECK(huffmanDecode(state->ctx, &state->huffman[RLELSB], &state->hstream, &lsb));

    if (lsb & 0x01) {
        uint8_t msb = 0;
        VN_CHECK(huffmanDecode(state->ctx, &state->huffman[RLEMSB], &state->hstream, &msb));

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

    return res;
}

uint32_t entropyGetConsumedBytes(const EntropyDecoder_t* state)
{
    return (uint32_t)((state->hstream.bitsRead + 7) >> 3);
}

/*------------------------------------------------------------------------------*/