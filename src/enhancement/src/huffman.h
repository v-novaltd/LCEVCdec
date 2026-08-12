/* Copyright (c) V-Nova International Limited 2022-2026. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_HUFFMAN_H
#define VN_LCEVC_ENHANCEMENT_HUFFMAN_H

#include "bitstream.h"

#include <LCEVC/common/bitutils.h>

// VN_BIG_TABLE_LEADING_ZEROES_BITS, VN_BIG_TABLE_MAX_CODE_SIZE, and VN_SMALL_TABLE_MAX_SIZE are
// defined via cmake, defaults in CMakeLists.txt.

#define VN_BIG_TABLE_MAX_SIZE (VN_BIG_TABLE_LEADING_ZEROES_BITS + VN_BIG_TABLE_MAX_CODE_SIZE)

#define VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES ((1 << VN_BIG_TABLE_LEADING_ZEROES_BITS) - 1)
#define VN_BIG_HUFFMAN_CODE_MASK ((1 << VN_BIG_TABLE_MAX_CODE_SIZE) - 1)

/* VN_MAX_NUM_SYMBOLS is a feature of the stream: for sparse symbols, 5 bits store the symbol
 * count (so, at most 32 symbols). For dense symbols, there's a presence bitmap of size 256. */
#define VN_MAX_NUM_SYMBOLS 256

/* This is a property of the stream, and therefore guaranteed. However, in practice, none of the
 * streams in our test coverage exceed 16 bits as the maximum code length. */
#define VN_MAX_CODE_LENGTH 31

/*------------------------------------------------------------------------------*/

/*! \brief Huffman types. These are all used to encode the coefficients (e.g. the 16 elements of a
 *         DDS-transformed square) which are later get "reverse hadamard transformed" into plain
 *         old residuals. For example, a 48x32 image would have 3x2=6 DDS squares, so you'd have
 *         16 huffman streams, each providing 6 coefficients. */
enum
{
    HuffLSB, /* 6 least-significant bits of the coefficients. */
    HuffMSB, /* The remaining most-significant bits. Often zero. */
    HuffRL,  /* The run-length of zeros following the last decoded symbol. */
    HuffCount
};

static inline bool nextSymbolIsMSB(uint8_t symbol) { return (symbol & 0x01); }
static inline bool nextSymbolIsRL(uint8_t symbol) { return (symbol & 0x80); }

/*- HuffmanStream -------------------------------------------------------------------------------*/

/*! \brief Bit-reader over a big-endian (MSB-first) byte stream.
 *
 * `word` is an MSB-aligned bit buffer: the top `bitCount` bits are bits which have been loaded
 * from the stream and not yet consumed. Bits below the top `bitCount` bits are either zero, or
 * hold a preview of the NEXT bits of the stream (this happens when a bulk refill loads more bits
 * than it can account for in `bitCount`; the unaccounted bytes are not counted in
 * byteStream.offset either, so a later refill simply ORs the same values over them again).
 *
 * The intended usage in a decode hot-loop is: refill once (guaranteeing at least
 * min(56, bits-left-in-stream) available bits), peek at up to 32 bits, then consume however many
 * bits were actually used. Reads past the end of the stream yield zero bits. */
typedef struct HuffmanStream
{
    ByteStream byteStream;
    uint64_t word;    /**< MSB-aligned bit buffer. */
    int32_t bitCount; /**< Number of valid (loaded, unconsumed) bits at the top of word. */
} HuffmanStream;

/*! \brief Initialize a HuffmanStream
 *
 *  \param stream  Huffman stream to initialize
 *  \param data    pointer to start of data, so that Huffman stream points there.
 *  \param size    size of data, in bytes.
 *
 *  \return True on success, otherwise false. */
bool huffmanStreamInitialize(HuffmanStream* stream, const uint8_t* data, size_t size);

/*! \brief Get number of remaining bits on the HuffmanStream */
static inline size_t huffmanStreamGetRemainingBits(const HuffmanStream* stream)
{
    return (size_t)stream->bitCount + bytestreamRemaining(&stream->byteStream) * 8;
}

/*! \brief Top up the bit buffer so that at least min(56, remaining-in-stream) bits are available.
 *
 * In the common case (8 or more bytes left in the stream) this is a single unaligned 8-byte load.
 * Near the end of the stream it falls back to byte-at-a-time loading; once the stream is
 * exhausted, the unfilled space reads as zero bits. */
static inline void huffmanStreamRefill(HuffmanStream* stream)
{
    const uint8_t* ptr = stream->byteStream.data + stream->byteStream.offset;
    if (bytestreamRemaining(&stream->byteStream) >= 8) {
        /* This shift-assembled big-endian load compiles to a single load (+byte-swap) on
         * little-endian targets. */
        const uint64_t chunk = ((uint64_t)ptr[0] << 56) | ((uint64_t)ptr[1] << 48) |
                               ((uint64_t)ptr[2] << 40) | ((uint64_t)ptr[3] << 32) |
                               ((uint64_t)ptr[4] << 24) | ((uint64_t)ptr[5] << 16) |
                               ((uint64_t)ptr[6] << 8) | (uint64_t)ptr[7];
        stream->word |= chunk >> stream->bitCount;
        /* Account for whole bytes only: bitCount goes to 56..63, preserving bitCount % 8. Any
         * bits of `chunk` beyond the new bitCount stay in `word` as a (consistent) preview. */
        stream->byteStream.offset += (size_t)((63 - stream->bitCount) >> 3);
        stream->bitCount |= 56;
        return;
    }

    /* Consuming zero-padding past the end of a (corrupt) stream can leave bitCount negative:
     * clamp so the shift below stays in range. */
    if (stream->bitCount < 0) {
        stream->bitCount = 0;
    }
    while (stream->bitCount <= 56 && stream->byteStream.offset < stream->byteStream.size) {
        stream->word |= (uint64_t)stream->byteStream.data[stream->byteStream.offset]
                        << (56 - stream->bitCount);
        stream->byteStream.offset++;
        stream->bitCount += 8;
    }
}

/*! \brief Peek at the next 32 bits of the stream (valid after huffmanStreamRefill; bits beyond
 *         the end of the stream read as zero). Does not consume anything. */
static inline uint32_t huffmanStreamPeek32(const HuffmanStream* stream)
{
    return (uint32_t)(stream->word >> 32);
}

/*! \brief Peek at the next `numBits` (1..32) bits of the stream, right-aligned. */
static inline uint32_t huffmanStreamPeekBits(const HuffmanStream* stream, uint8_t numBits)
{
    assert(numBits >= 1 && numBits <= 32);
    return (uint32_t)(stream->word >> (64 - numBits));
}

/*! \brief Consume `numBits` (0..32) bits, previously seen via a peek. */
static inline void huffmanStreamConsume(HuffmanStream* stream, uint8_t numBits)
{
    stream->word <<= numBits;
    stream->bitCount -= numBits;
}

/*! \brief Read and immediately consume up to 32 bits, with bounds-checking. Convenient in less
 *         performance-sensitive areas (e.g. initialisation); in a decode loop, prefer a single
 *         refill followed by peeks and consumes.
 *
 *  \param stream   Huffman stream to read bits from.
 *  \param numBits  The number of bits to read (1..32).
 *  \param out      The resulting data.
 *
 *  \return 0 if read was within bounds, -1 otherwise. */
static inline int32_t huffmanStreamReadBits(HuffmanStream* stream, uint8_t numBits, uint32_t* out)
{
    assert(stream && out);
    huffmanStreamRefill(stream);
    if (stream->bitCount < numBits) {
        return -1;
    }
    *out = huffmanStreamPeekBits(stream, numBits);
    huffmanStreamConsume(stream, numBits);
    return 0;
}

/*! \brief Number of bits consumed from the stream so far. */
static inline uint64_t huffmanStreamGetConsumedBits(const HuffmanStream* stream)
{
    return stream->byteStream.offset * 8 - (uint64_t)stream->bitCount;
}

/*------------------------------------------------------------------------------*/

/* Huffman list is used for codes when they don't fit in a look-up table. Note that, even though
 * the maximum code size is 31, we only need a uint8_t for code. That's because it's not possible
 * to get a code with a value higher than 255 when the max num symbols is 256: codes are counted up
 * incrementally, so you can't get a higher value than maxNumSymbols. */
typedef struct HuffmanListEntry
{
    uint8_t code;
    uint8_t symbol;
    uint8_t bits;
} HuffmanListEntry;

/* sorted by code length (increasing), then by code itself (decreasing) */
typedef struct HuffmanList
{
    HuffmanListEntry list[VN_MAX_NUM_SYMBOLS];
    uint16_t idxOfEachBitSize[VN_MAX_CODE_LENGTH + 1]; /* the index at which each bit-size ends */
    uint16_t size;
} HuffmanList;

/*------------------------------------------------------------------------------*/

typedef struct HuffmanEntry
{
    uint8_t symbol;
    uint8_t bits;
} HuffmanEntry;

typedef struct HuffmanTable
{
    HuffmanEntry code[1 << VN_SMALL_TABLE_MAX_SIZE];
} HuffmanTable;

/*------------------------------------------------------------------------------*/

/* Huffman triple-table: lookup table contains a list of triples.
 * lsbOrValue depends on the entry's completeness:
 *  - For complete entries (contents & 0x04), it holds the final decoded coefficient value
 *    (an int8_t: since a complete entry has no MSB, the value fits in [-32, 31]).
 *  - For incomplete entries, it holds the raw LSB symbol (so the fallback decode can inspect
 *    the "MSB follows"/"RL follows" bits), or is meaningless if the LSB itself overflowed.
 * rl is the run-length, derived from strictly 0 to 2 codes. Note: we could have made this handle
 *  any number of subsequent run-lengths (up to 4 are possible). However, such sequences (3 or 4
 *  run-lengths, with a total code length less than 12) are vanishingly rare. Meanwhile,
 *  accommodating them imposes costs.
 * contents works like this:
 * { uint5_t bitsTotal, bool complete, bool msbOverflowed, bool rlOverflowed}
 *  - bitsTotal is a number from 1 to 31 (or 0 for invalid) indicating the combined number of bits
 *    taken up (in the stream) by the codes for all parts of the HuffmanTriple which are present
 *  - complete (0x04) means the entry fully resolves this decode: lsbOrValue is the final value
 *    and rl is the final run-length. This is the hot-path test.
 *  - the overflows tell us which symbol (if any) was unable to fit on this triple. Note that we
 *    never store the msb in this table, so even a 1bit msb code is "overflow". Note also that we
 *    don't need an "lsbOverflowed" bit: this is indicated by having 0 bitsTotal.
 */
typedef struct HuffmanTriple
{
    uint8_t contents;
    uint8_t lsbOrValue;
    uint16_t rl;
} HuffmanTriple;

typedef struct HuffmanTripleTable
{
    HuffmanTriple code[1 << VN_BIG_TABLE_MAX_SIZE];
} HuffmanTripleTable;

/* Utility functions for HuffmanTriple's contents memvar */
static inline uint8_t huffmanTripleGetBits(uint8_t contents) { return contents >> 3; }
static inline bool huffmanTripleIsComplete(uint8_t contents) { return contents & 0x04; }
static inline bool huffmanTripleLsbOverflowed(uint8_t contents)
{
    return (huffmanTripleGetBits(contents) == 0);
}

/*! \brief Huffman "manual" decoder state. This is used when look-up tables are insufficient. */
typedef struct HuffmanManualDecodeState
{
    HuffmanList list;
    uint8_t singleSymbol;  /**< When we only have one symbol in the alphabet, this is it */
    uint8_t minCodeLength; /**< The smallest code word has this length */
    uint8_t maxCodeLength; /**< The longest code word has this length */
} HuffmanManualDecodeState;

/*! \brief huffman decoder state */
typedef struct HuffmanTripleDecodeState
{
    HuffmanTripleTable tripleTable;                   /**< TripleTable, for short triplets */
    HuffmanTable rlTable;                             /**< Fallback lookup-table for Run-lengths */
    HuffmanManualDecodeState manualStates[HuffCount]; /**< Individual decoders, as a double-fallback */
    uint32_t lsbGuardBit; /**< ORed over the peeked bits before clz, so that the leading-zero
                               count is capped at min(LSB maxCodeLength,
                               VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES) with no extra min ops. */
} HuffmanTripleDecodeState;

/*! \brief Initialize a triple huffman decoder
 *
 *  \param state   Triple-decoder to initialize
 *  \param stream  Huffman stream to read initialization from
 *
 *  \return True on success, otherwise false. */
bool huffmanTripleInitialize(HuffmanTripleDecodeState* state, HuffmanStream* stream,
                             uint8_t bitstreamVersion);

/*! \brief Fallback for huffmanTripleDecode, handling entries which the triple-table could not
 *         fully resolve (long LSB codes, any MSB, and long or numerous run-length codes).
 *
 *  \return run-length, or -1 for error */
int32_t huffmanTripleDecodeSlow(const HuffmanTripleDecodeState* state, HuffmanStream* stream,
                                int16_t* valueOut, HuffmanTriple triplet);

/*! \brief Decode the next several huffman symbols
 *
 *  \param state    Triple-decoder to decode with
 *  \param stream   Huffman stream to read codes from
 *  \param valueOut Output coefficient (lsb, possibly with msb)
 *
 *  \return run-length, or -1 for error */
static inline int32_t huffmanTripleDecode(const HuffmanTripleDecodeState* state,
                                          HuffmanStream* stream, int16_t* valueOut)
{
    huffmanStreamRefill(stream);
    const uint32_t peek = huffmanStreamPeek32(stream);

    /* The lutIdx is (number of leading zeros, capped) followed by the VN_BIG_TABLE_MAX_CODE_SIZE
     * bits after those leading zeros. The guard bit caps the clz at
     * min(maxLsbCodeLength, VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES), and also keeps clz's input
     * nonzero (clz32(0) is undefined behaviour on some compilers). */
    const uint32_t lsbLeadingZeros = clz32(peek | state->lsbGuardBit);
    const uint32_t lutIdx =
        (lsbLeadingZeros << VN_BIG_TABLE_MAX_CODE_SIZE) |
        ((peek >> (32 - VN_BIG_TABLE_MAX_CODE_SIZE - lsbLeadingZeros)) & VN_BIG_HUFFMAN_CODE_MASK);

    const HuffmanTriple triplet = state->tripleTable.code[lutIdx];
    huffmanStreamConsume(stream, huffmanTripleGetBits(triplet.contents));

    if (huffmanTripleIsComplete(triplet.contents)) {
        *valueOut = (int8_t)triplet.lsbOrValue;
        return triplet.rl;
    }

    return huffmanTripleDecodeSlow(state, stream, valueOut, triplet);
}

/*------------------------------------------------------------------------------*/

/*! \brief Huffman single-decoder. Used for temporal, and for size-parsing decoders. Unlike the
 *         triple-decoder, this decoder only decodes one symbol at a time (using a look-up table
 *         and a sorted list as a fallback). */
typedef struct HuffmanSingleDecoder
{
    HuffmanManualDecodeState manualState;
    HuffmanTable table;
} HuffmanSingleDecoder;

/*! \brief Initialize both parts of a huffman single-decoder.
 *
 *  \param state            The manual-decoder part of the single-decoder to initialize
 *  \param table            The lookup-table part of the single-decoder to initialize
 *  \param stream           HuffmanStream to read initialization from
 *  \param bitstreamVersion The bitstream version number to decode with
 */
bool huffmanManualInitializeWithLut(HuffmanManualDecodeState* state, HuffmanTable* table,
                                    HuffmanStream* stream, uint8_t bitstreamVersion);

/*! \brief Decode the next huffman symbol using a look-up table
 *
 *  \param table     Lookup-table to look in for the code seen in the stream.
 *  \param stream    HuffmanStream to decode from
 *  \param symbolOut Decoded symbol
 *
 *  \return True on success, otherwise false. */
static inline bool huffmanLutDecode(const HuffmanTable* table, HuffmanStream* stream, uint8_t* symbolOut)
{
    huffmanStreamRefill(stream);
    const uint32_t lutIdx = huffmanStreamPeekBits(stream, VN_SMALL_TABLE_MAX_SIZE);
    const HuffmanEntry entry = table->code[lutIdx];
    if (entry.bits != 0) {
        huffmanStreamConsume(stream, entry.bits);
        *symbolOut = entry.symbol;
        return true;
    }

    return false;
}

/*! \brief Decode the next huffman symbol using a manual-decoder (i.e. a sorted list)
 *
 *  \param state     Manual-decoder to use.
 *  \param stream    HuffmanStream to decode from
 *  \param symbolOut Decoded symbol
 *
 *  \return True on success, otherwise false. */
bool huffmanManualDecode(const HuffmanManualDecodeState* state, HuffmanStream* stream, uint8_t* symbolOut);

/*! \brief Get the single-symbol associated with the manual-decoder, if it's a single-symbol decoder.
 *
 *  \param state     Manual-decoder to use.
 *  \param symbolOut Decoded symbol
 *
 *  \return True if this decoder is a single-symbol decoder, otherwise false */
bool huffmanGetSingleSymbol(const HuffmanManualDecodeState* state, uint8_t* symbolOut);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_HUFFMAN_H
