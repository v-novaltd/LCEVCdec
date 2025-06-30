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

#include "huffman.h"

#include "chunk_parser.h"

#include <assert.h>
#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*- General utility functions -------------------------------------------------------------------*/

static inline uint8_t clz(uint32_t streamData, uint8_t numBits)
{
#if VN_COMPILER(MSVC)
    return (uint8_t)(_lzcnt_u32(streamData) + numBits - 32);
#elif VN_COMPILER(GCC) && (__GNUC__ >= 14)
    /* Annoyingly, it's undefined behaviour if you provide 0 to __builtin_clz. To match the Windows
     * behaviour (where 0 is just another leading zero), we need clzg, with sizeof(streamData) as
     * the default arg. */
    return (uint8_t)(__builtin_clzg(streamData, (int)(sizeof(streamData) * 8)) + numBits - 32);
#else
    /* clzg is only available in GCC version 14 or later. Outside that, we just do it manually. */
    if (streamData == 0) {
        return numBits;
    }
    return (uint8_t)(__builtin_clz(streamData) + numBits - 32);
#endif
}

static int8_t bitWidth(uint8_t x, uint8_t bitstreamVersion)
{
    /* Lengths are ceil(log2(length + 1)), as per 9.2.1 of the standard. This table is indexed by
     * bitstreamVersion (since each of the first 3 versions introduced a new table). */
    static const int8_t kTable[BitstreamVersionAlignWithSpec + 1][32] = {
        {
            1, 1, 2, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5,
            5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        },
        {
            1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        },
        {
            0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        },
    };

    /* Old code lengths indexed the "table" with max_length - min_length + 1
     * New code lengths index the table with max_length - min_length. */
    if (bitstreamVersion < BitstreamVersionNewCodeLengths) {
        x++;
    }

    if (x > 31) {
        /* This should be impossible as lengths are 5-bits maximum. */
        return -1;
    }

    const uint8_t table = minU8(bitstreamVersion, BitstreamVersionAlignWithSpec);

    return kTable[table][x];
}

static void generateCodesAndIndices(HuffmanListEntry entriesInOut[VN_MAX_NUM_SYMBOLS],
                                    uint16_t idxOfEachBitSize[VN_MAX_CODE_LENGTH], int16_t maxIdx,
                                    uint8_t maxCodeLength)
{
    uint8_t currLength = maxCodeLength;
    uint8_t currCode = 0;

    idxOfEachBitSize[currLength] = maxIdx;

    for (int16_t idx = maxIdx - 1; idx > -1; idx--) {
        if (entriesInOut[idx].bits < currLength) {
            currCode >>= (currLength - entriesInOut[idx].bits);
            currLength = entriesInOut[idx].bits;
            idxOfEachBitSize[currLength] = idx + 1;
        }
        entriesInOut[idx].code = currCode++;
    }
}

/* Generate codes, without setting the idxOfEachBitSize array. This is because the final LSB list
 * will be quite different from the one created here. */
static void generateCodes(HuffmanListEntry entriesInOut[VN_MAX_NUM_SYMBOLS], int16_t maxIdx,
                          uint8_t maxCodeLength)
{
    uint8_t currLength = maxCodeLength;
    uint8_t currCode = 0;

    for (int16_t idx = maxIdx - 1; idx > -1; idx--) {
        if (entriesInOut[idx].bits < currLength) {
            currCode >>= (currLength - entriesInOut[idx].bits);
            currLength = entriesInOut[idx].bits;
        }
        entriesInOut[idx].code = currCode++;
    }
}

/* This function generates codes, a manual-search list, and a look-up table. Currently, we use it
 * for the run-length huffman stream, but in principle it could be used for the temporal layer too
 * (although attempts at this have sometimes worsened performance: it seems that the existing
 * tableAssign may actually be faster, but would be awkward to change its interface). */
static uint8_t generateCodesAndLut(HuffmanListEntry entriesIn[VN_MAX_NUM_SYMBOLS],
                                   HuffmanTable* tableOut, uint8_t maxIdx, uint8_t maxCodeLength)
{
    memset(tableOut->code, 0, sizeof(tableOut->code));
    uint8_t currLength = maxCodeLength;
    uint8_t currCode = 0;
    uint8_t minOversizedCodeIdx = maxIdx;

    /* This list is sorted from large to small, so start by assigning list entries for codes which
     * are too long for the look-up table */
    int16_t idx = (int16_t)(maxIdx)-1;
    for (; idx > -1; idx--) {
        HuffmanListEntry* entry = &entriesIn[idx];

        if (entry->bits < currLength) {
            currCode >>= (currLength - entry->bits);
            currLength = entry->bits;
        }

        if (entry->bits > VN_SMALL_TABLE_MAX_SIZE) {
            entry->code = currCode;
            minOversizedCodeIdx = (uint8_t)idx;
        } else {
            uint16_t tableIdx = currCode << (VN_SMALL_TABLE_MAX_SIZE - entry->bits);
            const uint16_t tableIdxEnd = tableIdx + (1 << (VN_SMALL_TABLE_MAX_SIZE - entry->bits));
            for (; tableIdx < tableIdxEnd; tableIdx++) {
                tableOut->code[tableIdx].symbol = entry->symbol;
                tableOut->code[tableIdx].bits = entry->bits;
            }
        }

        currCode++;
    }
    return minOversizedCodeIdx;
}

static void determineIdxOfEachBitSize(HuffmanList* listInOut)
{
    uint8_t bitSize = listInOut->list[0].bits;
    for (uint8_t idx = 0; idx < listInOut->size; idx++) {
        if (listInOut->list[idx].bits > bitSize) {
            listInOut->idxOfEachBitSize[bitSize] = idx;
            bitSize = listInOut->list[idx].bits;
        }
    }
    listInOut->idxOfEachBitSize[bitSize] = listInOut->size;
}

/* Ascending size, then descending symbol */
static int listEntrySizeOrder(const void* left, const void* right)
{
    const HuffmanListEntry* leftEntry = (const HuffmanListEntry*)left;
    const HuffmanListEntry* rightEntry = (const HuffmanListEntry*)right;
    if (leftEntry->bits != rightEntry->bits) {
        return (int)leftEntry->bits - rightEntry->bits;
    }

    return (int)rightEntry->symbol - leftEntry->symbol;
}

/* Utility functions for HuffmanTriple's contents memvar */
static inline uint8_t getBits(uint8_t contents) { return contents >> 3; }
static inline bool lsbOverflowed(uint8_t contents) { return (getBits(contents) == 0); }
static inline bool isIncomplete(uint8_t contents)
{
    return lsbOverflowed(contents) || (contents & 0B00000011);
}

/*- Initialisation functions --------------------------------------------------------------------*/

/* \brief  Initialize a HuffmanManualDecodeState
 *
 * \return The number of codes in entriesOut, or -1 for error
 *         (note that this should never return 1: when there is a single-symbol layer, the symbol
 *         will go in state->singleSymbol, not entriesOut). */
static int16_t huffmanManualInitializeCommon(HuffmanManualDecodeState* state, HuffmanStream* stream,
                                             uint8_t bitstreamVersion,
                                             HuffmanListEntry entriesOut[VN_MAX_NUM_SYMBOLS])
{
    memset(state->list.list, 0, sizeof(state->list.list));

    uint32_t bits = 0;
    VNCheckI(huffmanStreamReadBits(stream, 5, &bits));
    state->minCodeLength = (uint8_t)bits;

    VNCheckI(huffmanStreamReadBits(stream, 5, &bits));
    state->maxCodeLength = (uint8_t)bits;

    if (state->maxCodeLength < state->minCodeLength) {
        VNLogError("Huffman code lengths are invalid, max length [%u] is less than min length [%u]",
                   state->maxCodeLength, state->minCodeLength);
        return -1;
    }

    if (state->minCodeLength == VN_MAX_CODE_LENGTH && state->maxCodeLength == VN_MAX_CODE_LENGTH) {
        /* "Special" case - empty table*/
        return 0;
    }

    if (state->minCodeLength == 0 && state->maxCodeLength == 0) {
        /* another "Special" case */
        /* only one code */
        VNCheckI(huffmanStreamReadBits(stream, 8, &bits));
        state->singleSymbol = (uint8_t)bits;
        return 0;
    }

    const int8_t lengthBits = bitWidth(state->maxCodeLength - state->minCodeLength, bitstreamVersion);
    if (lengthBits < 0) {
        VNLogError("huffman: code lengths are invalid, resulted in incorrect bit-width max"
                   " length [%u], min length [%u]\n",
                   state->maxCodeLength, state->minCodeLength);
        return -1;
    }

    /* Determines whether to use a "presence bitmap" (efficient if very many symbols are used) */
    VNCheckI(huffmanStreamReadBits(stream, 1, &bits));

    int16_t orderIdx = 0;
    if (bits) {
        for (int32_t i = 0; i < VN_MAX_NUM_SYMBOLS; ++i) {
            /* Symbol present flag */
            VNCheckI(huffmanStreamReadBits(stream, 1, &bits));
            if (bits) {
                uint32_t codeLength = 0;
                VNCheckI(huffmanStreamReadBits(stream, lengthBits, &codeLength));
                entriesOut[orderIdx].symbol = (uint8_t)i;
                entriesOut[orderIdx].bits = (uint8_t)(codeLength + state->minCodeLength);
                orderIdx++;
            }
        }
    } else { /* Read symbol-count */
        uint32_t symbolCount = 0;
        VNCheckI(huffmanStreamReadBits(stream, 5, &symbolCount));

        if (symbolCount <= 0) {
            return -1;
        }

        for (uint32_t i = 0; i < symbolCount; ++i) {
            uint32_t symbol = 0;
            uint32_t codeLength = 0;
            VNCheckI(huffmanStreamReadBits(stream, 8, &symbol));
            VNCheckI(huffmanStreamReadBits(stream, lengthBits, &codeLength));

            entriesOut[orderIdx].symbol = (uint8_t)symbol;
            entriesOut[orderIdx].bits = (uint8_t)(codeLength + state->minCodeLength);
            orderIdx++;
        }
    }

    qsort(entriesOut, orderIdx, sizeof(HuffmanListEntry), listEntrySizeOrder);

    return orderIdx;
}

/* Declare this so that we can have a tag-team pair of recursive functions. */
static uint16_t huffmanIterateRls(HuffmanTripleTable* huffmanTableOut, const HuffmanTable* rlTable,
                                  const HuffmanList* rlList, uint16_t parentStartIdx,
                                  uint16_t parentEndIdx, uint8_t lsbSymbol, uint16_t rlSymbol,
                                  uint8_t validBits, uint8_t recursionLevel);

/* Slightly hideous function but, sonarQube prefers functions with too many params over excessive
 * code duplication. */
static uint16_t huffmanIterateRlsLoopBody(HuffmanTripleTable* huffmanTableOut,
                                          const HuffmanTable* rlTable, const HuffmanList* rlList,
                                          uint16_t parentStartIdx, uint16_t lowestValidIdxYet,
                                          uint8_t lsbSymbol, uint16_t rlSymbol,
                                          uint8_t codeSizeInStream, uint16_t newRlCode,
                                          uint8_t newRlSymbol, uint8_t newRlBits, uint8_t recursionLevel)
{
    const uint8_t codeSizeInTable = codeSizeInStream - (parentStartIdx >> VN_BIG_TABLE_MAX_CODE_SIZE);
    const uint8_t bitsLeft = VN_BIG_TABLE_MAX_CODE_SIZE - codeSizeInTable;
    const uint8_t bitsLeftByRl1 = bitsLeft - newRlBits;
    const uint16_t startIdxRl1 = parentStartIdx | (newRlCode << bitsLeftByRl1);
    const uint16_t endIdxRl1 = startIdxRl1 + (1 << bitsLeftByRl1);
    codeSizeInStream += newRlBits;

    /* recursive case: */
    if (nextSymbolIsRL(newRlSymbol)) {
        const uint16_t out = huffmanIterateRls(huffmanTableOut, rlTable, rlList, startIdxRl1, endIdxRl1,
                                               lsbSymbol, (rlSymbol << 7) | (newRlSymbol & 0x7f),
                                               codeSizeInStream, recursionLevel + 1);
        return minU16(lowestValidIdxYet, out);
    }

    /* non-recursive case: */
    for (uint16_t idx = startIdxRl1; idx < endIdxRl1; idx++) {
        huffmanTableOut->code[idx].lsb = lsbSymbol;
        huffmanTableOut->code[idx].rl = (rlSymbol << 7) | (newRlSymbol & 0x7f);
        huffmanTableOut->code[idx].contents = codeSizeInStream << 3;
    }
    return minU16(lowestValidIdxYet, startIdxRl1);
}

/* Recursive function to assign codes for the run-lengths in the triple-table (recursive because a
 * run-length can be followed by any number of subsequent run-lengths). */
static uint16_t huffmanIterateRls(HuffmanTripleTable* huffmanTableOut, const HuffmanTable* rlTable,
                                  const HuffmanList* rlList, uint16_t parentStartIdx,
                                  uint16_t parentEndIdx, uint8_t lsbSymbol, uint16_t rlSymbol,
                                  uint8_t codeSizeInStream, uint8_t recursionLevel)
{
    uint16_t lowestValidlySetIdx = parentEndIdx;
    // Code's size in table is its size in the stream, minus the number of leading zeroes.
    const uint8_t codeSizeInTable = codeSizeInStream - (parentStartIdx >> VN_BIG_TABLE_MAX_CODE_SIZE);
    const uint8_t bitsLeft = VN_BIG_TABLE_MAX_CODE_SIZE - codeSizeInTable;

    if (recursionLevel < 2) {
        /* First, look through the RL LUT for symbols. Iterate from end to start, so that we can
         * early- break when we hit a too-large index (rather than having to repeat-continue). */
        uint8_t rlBits = 0;
        for (int16_t rlIdx = (1 << VN_SMALL_TABLE_MAX_SIZE) - 1; rlIdx >= 0;
             rlIdx -= (1 << (VN_SMALL_TABLE_MAX_SIZE - rlBits))) {
            const HuffmanEntry* nextRlEntry = &rlTable->code[rlIdx];
            rlBits = nextRlEntry->bits;
            if (rlBits == 0 || rlBits > bitsLeft) {
                /* 0bit entries are the placeholder entries to signify that the LSB can't fit.
                 * Entries where rlBits > bitsLeft are those where this latest RL can't fit. */
                break;
            }
            uint16_t rlCode = rlIdx >> (VN_SMALL_TABLE_MAX_SIZE - rlBits);

            lowestValidlySetIdx = huffmanIterateRlsLoopBody(
                huffmanTableOut, rlTable, rlList, parentStartIdx, parentEndIdx, lsbSymbol, rlSymbol,
                codeSizeInStream, rlCode, nextRlEntry->symbol, rlBits, recursionLevel);
        }

        /* Now, IF there's space for big RLs, we can include them (this might seem niche, but
         * remember that short codes are the most common, so a 1bit LSB is the most important) */
        if (bitsLeft > VN_SMALL_TABLE_MAX_SIZE) {
            for (uint16_t rlIdx = 0; rlIdx < rlList->size; rlIdx++) {
                const HuffmanListEntry* nextRlEntry = &rlList->list[rlIdx];
                rlBits = nextRlEntry->bits;
                if (rlBits > bitsLeft) {
                    /* beyond table size limit */
                    break;
                }
                lowestValidlySetIdx = huffmanIterateRlsLoopBody(
                    huffmanTableOut, rlTable, rlList, parentStartIdx, parentEndIdx, lsbSymbol, rlSymbol,
                    codeSizeInStream, nextRlEntry->code, nextRlEntry->symbol, rlBits, recursionLevel);
            }
        }
    }

    /* Fill in the gap between the lowest entry we set, and the lowest entry that our parent sets.
     * All entries in this gap are incomplete due to RL overflow: we're only here because the
     * current symbol definitely IS followed by an RL, but these are the indices where none fit. */
    for (uint16_t idx = parentStartIdx; idx < lowestValidlySetIdx; idx++) {
        huffmanTableOut->code[idx].lsb = lsbSymbol;
        huffmanTableOut->code[idx].rl = rlSymbol;
        huffmanTableOut->code[idx].contents = (codeSizeInStream << 3) | 0x01;
    }
    /* Experimentally, the LUT always seems to be compact at high indices: there are never gaps at
     * the top. I think this is a genuine property of Huffman codes, but it might need some more
     * investigation.*/

    return minU16(parentStartIdx, lowestValidlySetIdx);
}

static void huffmanTripleTableAssign(HuffmanTripleTable* huffmanTableOut,
                                     HuffmanTripleDecodeState* huffmanState, const HuffmanList* fullLsbListIn,
                                     const HuffmanTable* rlTable, const HuffmanList* rlList)
{
    HuffmanList* overflowLsbListOut = &huffmanState->manualStates[HuffLSB].list;
    uint8_t lsbIdx = 0;
    for (; lsbIdx < fullLsbListIn->size; lsbIdx++) {
        const HuffmanListEntry* lsbEntry = &fullLsbListIn->list[lsbIdx];
        uint8_t leadingZeroes = clz(lsbEntry->code, lsbEntry->bits);
        leadingZeroes = minU8(leadingZeroes, VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES);
        int8_t bitsLeftByLsb = VN_BIG_TABLE_MAX_CODE_SIZE - (lsbEntry->bits - leadingZeroes);
        if (bitsLeftByLsb < 0) {
            break;
        }

        uint16_t startIdx = (lsbEntry->code << bitsLeftByLsb);
        startIdx |= (leadingZeroes << VN_BIG_TABLE_MAX_CODE_SIZE);
        const uint16_t endIdx = startIdx + (1 << bitsLeftByLsb);
        if (nextSymbolIsMSB(lsbEntry->symbol)) {
            for (uint16_t outIdx = startIdx; outIdx < endIdx; outIdx++) {
                huffmanTableOut->code[outIdx].lsb = lsbEntry->symbol;
                huffmanTableOut->code[outIdx].contents = (lsbEntry->bits << 3) | 0x02;
            }
            continue;
        }

        if (!nextSymbolIsRL(lsbEntry->symbol)) {
            for (uint16_t outIdx = startIdx; outIdx < endIdx; outIdx++) {
                huffmanTableOut->code[outIdx].lsb = lsbEntry->symbol;
                huffmanTableOut->code[outIdx].contents = (lsbEntry->bits << 3);
            }
            continue;
        }

        huffmanIterateRls(huffmanTableOut, rlTable, rlList, startIdx, endIdx, lsbEntry->symbol, 0,
                          lsbEntry->bits, 0);
    }

    /* These are all the entries where the LSB is too long to fit in a LUT entry. Some may be
     * shorter than expected, if the max num of leading zeroes is low enough. */
    if (fullLsbListIn->size > lsbIdx) {
        const uint8_t additionalEntries = (fullLsbListIn->size - lsbIdx);
        const uint16_t curSize = overflowLsbListOut->size;
        memcpy(overflowLsbListOut->list + curSize, (fullLsbListIn->list + lsbIdx),
               additionalEntries * sizeof(HuffmanListEntry));
        overflowLsbListOut->size += additionalEntries;
    }

    /* Determine the "idx of each bit size" list here, because (1) This list is shorter than the
     * one in the generateCodes step, so it's quicker overall, and (2) This list may be an
     * unpredictable subset of the full list, due to leading zeroes. */
    if (overflowLsbListOut->size > 0) {
        determineIdxOfEachBitSize(overflowLsbListOut);
    }
}

bool huffmanTripleInitialize(HuffmanTripleDecodeState* state, HuffmanStream* stream, uint8_t bitstreamVersion)
{
    assert(state && stream);

    int16_t res = 0;

    /* LSB */
    HuffmanList lsbList = {{{0}}, {0}, 0};
    res = huffmanManualInitializeCommon(&state->manualStates[HuffLSB], stream, bitstreamVersion,
                                        lsbList.list);
    if (res < 0) {
        VNLogError("Failed to initialize huffman state");
        return false;
    }
    lsbList.size = res;
    generateCodes(lsbList.list, lsbList.size, state->manualStates[HuffLSB].maxCodeLength);

    /* MSB */
    res = huffmanManualInitializeCommon(&state->manualStates[HuffMSB], stream, bitstreamVersion,
                                        state->manualStates[HuffMSB].list.list);
    if (res < 0) {
        VNLogError("Failed to initialize huffman state");
        return false;
    }
    state->manualStates[HuffMSB].list.size = res;
    generateCodesAndIndices(
        state->manualStates[HuffMSB].list.list, state->manualStates[HuffMSB].list.idxOfEachBitSize,
        state->manualStates[HuffMSB].list.size, state->manualStates[HuffMSB].maxCodeLength);

    /* RL */
    huffmanManualInitializeWithLut(&state->manualStates[HuffRL], &state->rlTable, stream, bitstreamVersion);

    /* Triple Table */
    memset(&state->tripleTable, 0, sizeof(state->tripleTable));
    huffmanTripleTableAssign(&state->tripleTable, state, &lsbList, &state->rlTable,
                             &state->manualStates[HuffRL].list);

    return true;
}

/*- HuffmanStream -------------------------------------------------------------------------------*/

bool huffmanStreamInitialize(HuffmanStream* stream, const uint8_t* data, size_t size)
{
    if (!bytestreamInitialize(&stream->byteStream, data, size)) {
        return false;
    }

    stream->wordEndBit = 32;
    stream->wordStartBit = 32;
    stream->bitsRead = 0;
    stream->word = 0;

    return true;
}

/*- HuffmanManualDecodeState --------------------------------------------------------------------*/

void huffmanManualInitializeWithLut(HuffmanManualDecodeState* state, HuffmanTable* table,
                                    HuffmanStream* stream, uint8_t bitstreamVersion)
{
    assert(state && table && stream);

    HuffmanListEntry codes[VN_MAX_NUM_SYMBOLS];
    const int16_t size = huffmanManualInitializeCommon(state, stream, bitstreamVersion, codes);
    if (size <= 0) {
        // Early exit if there's no work to do
        return;
    }

    const uint8_t minIdxOfOversizedCodes =
        generateCodesAndLut(codes, table, (uint8_t)size, state->maxCodeLength);

    state->list.size = size - minIdxOfOversizedCodes;
    if (state->list.size > 0) {
        memcpy(state->list.list, &codes[minIdxOfOversizedCodes],
               sizeof(HuffmanListEntry) * (state->list.size));

        determineIdxOfEachBitSize(&state->list);
    }
}

bool huffmanManualDecode(const HuffmanManualDecodeState* state, HuffmanStream* stream, uint8_t* symbolOut)
{
    const HuffmanList* list = &state->list;
    uint8_t bitsUnderConsideration = list->list[0].bits;
    uint32_t code = huffmanStreamAdvanceToNthBit(stream, bitsUnderConsideration);

    /* list->list is sorted by code length (increasing), then by code itself (decreasing) */
    for (uint16_t idx = 0; idx < list->size; idx = list->idxOfEachBitSize[bitsUnderConsideration]) {
        const HuffmanListEntry* entry = &list->list[idx];
        while (bitsUnderConsideration < entry->bits) {
            bitsUnderConsideration++;
            code = huffmanStreamAdvanceToNthBit(stream, bitsUnderConsideration);
        }

        /* binary search
         * Lower and Upper limit are inclusive bounds (whereas idxOfEach... is an exclusive bound) */
        uint16_t lowerLimit = idx;
        uint16_t upperLimit = list->idxOfEachBitSize[bitsUnderConsideration] - 1;
        for (uint16_t testIdx = lowerLimit + (upperLimit - lowerLimit + 1) / 2;;) {
            /* Go down */
            entry = &list->list[testIdx];
            if (code > entry->code) {
                if (testIdx == lowerLimit) {
                    break;
                }
                upperLimit = testIdx;
                testIdx = testIdx - (testIdx - lowerLimit + 1) / 2;
                continue;
            }

            /* Go up */
            if (code < entry->code) {
                if (testIdx == upperLimit) {
                    break;
                }
                lowerLimit = testIdx;
                testIdx = testIdx + (upperLimit - testIdx + 1) / 2;
                continue;
            }

            /* Found it! Now advance wordStartBit, so we're no longer looking at those bits. */
            stream->wordStartBit += entry->bits;
            assert(stream->wordStartBit <= 32);
            *symbolOut = entry->symbol;
            return true;
        }
    }

    /* Unknown huffman code */
    return false;
}

static bool huffmanManualDecodeMaybeSingleSymbol(const HuffmanManualDecodeState* state,
                                                 HuffmanStream* stream, uint8_t* symbolOut)
{
    /* This function allows us to do the LUT check FIRST, for huffman types which are usually in
     * the LUT, and rarely (but sometimes) single-symbol. */
    if (state->maxCodeLength + state->minCodeLength == 0) {
        *symbolOut = state->singleSymbol;
        return true;
    }

    return huffmanManualDecode(state, stream, symbolOut);
}

bool huffmanGetSingleSymbol(const HuffmanManualDecodeState* state, uint8_t* symbolOut)
{
    if (state->maxCodeLength + state->minCodeLength == 0) {
        *symbolOut = state->singleSymbol;
        return true;
    }
    return false;
}

/*- HuffmanTable --------------------------------------------------------------------------------*/

bool huffmanLutDecode(const HuffmanTable* rlTable, HuffmanStream* stream, uint8_t* symbolOut)
{
    uint16_t lutIdx = (uint16_t)huffmanStreamAdvanceToNthBit(stream, VN_SMALL_TABLE_MAX_SIZE);
    uint8_t bits = rlTable->code[lutIdx].bits;
    stream->wordStartBit += bits;
    if (bits != 0) {
        assert(stream->wordStartBit <= 32);
        *symbolOut = rlTable->code[lutIdx].symbol;
        return true;
    }

    return false;
}

/*- HuffmanTripleDecodeState --------------------------------------------------------------------*/

int32_t huffmanTripleDecode(const HuffmanTripleDecodeState* state, HuffmanStream* stream, int16_t* valueOut)
{
    assert(state && stream && (stream->wordStartBit <= stream->wordEndBit) &&
           (stream->wordStartBit + VN_BIG_TABLE_CODE_SIZE_TO_READ >= stream->wordEndBit));

    /* Top up our HuffmanStream_t until we have VN_BIG_TABLE_CODE_SIZE_TO_READ bits of data,
     * and then grab those bits of data. Later, we'll find out how much of it, if any, is useful.*/
    huffmanStreamAdvanceByNBits(stream, VN_BIG_TABLE_CODE_SIZE_TO_READ -
                                            (stream->wordEndBit - stream->wordStartBit));
    const uint32_t code = extractBits(stream->word, stream->wordStartBit, stream->wordEndBit);

    /* We now have a number, of size VN_BIG_TABLE_CODE_SIZE_TO_READ. Count the number of leading
     * zeroes in this number. This count will form the first few bits of our lutIdx. We have to
     * take the min because 0 is a valid code (always the longest one), and because there's a limit
     * to the number of bits we can fit at the front of lutIdx.*/
    uint8_t lsbLeadingZeros = clz((int32_t)code, VN_BIG_TABLE_CODE_SIZE_TO_READ);
    lsbLeadingZeros = minU8(lsbLeadingZeros, state->manualStates[HuffLSB].maxCodeLength);
    lsbLeadingZeros = minU8(lsbLeadingZeros, VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES);

    /* Now assemble the lutIdx by replacing the leading zeroes in `code` with the actual count of
     * leading zeros (lsbLzs).*/
    const uint8_t plausiblyUsefulBits = (VN_BIG_TABLE_MAX_CODE_SIZE + lsbLeadingZeros);
    uint16_t lutIdx = (uint16_t)(code >> (VN_BIG_TABLE_CODE_SIZE_TO_READ - plausiblyUsefulBits));
    assert(lutIdx <= VN_BIG_HUFFMAN_CODE_MASK);
    lutIdx |= (lsbLeadingZeros << VN_BIG_TABLE_MAX_CODE_SIZE);

    /* Seek symbols in huffman table.*/
    const HuffmanTripleTable* table = &state->tripleTable;
    HuffmanTriple triplet = table->code[lutIdx];
    uint8_t bits = getBits(triplet.contents);
    stream->wordStartBit += bits;
    assert(stream->wordStartBit <= 32);

    /* Quickly dismiss the fast case: */
    if (!isIncomplete(triplet.contents)) {
        *valueOut = ((int16_t)(triplet.lsb & 0x7e) - 0x40) >> 1;
        return triplet.rl;
    }

    /* Seek run lengths if:
     * (1) the lsb overflowed, and either
     *      (a) is followed by an RL, or
     *      (b) is followed by and MSB, and THAT is followed by an RL
     * or
     * (2) the msb overflowed, and it's followed by an RL,
     * or
     * (3) the rl itself overflowed (which is always true if the other true aren't, since this
     *     block of code is only reachable when SOME part overflowed).
     */
    bool seekRunLengths = true;

    /* LSB */
    if (lsbOverflowed(triplet.contents)) {
        if (!huffmanManualDecodeMaybeSingleSymbol(&state->manualStates[HuffLSB], stream, (uint8_t*)valueOut)) {
            return -1;
        }
        seekRunLengths = nextSymbolIsRL((uint8_t)*valueOut);
    } else {
        *valueOut = triplet.lsb;
    }

    /* MSB */
    if (nextSymbolIsMSB((uint8_t)*valueOut)) {
        uint8_t msb = 0;
        if (!huffmanManualDecodeMaybeSingleSymbol(&state->manualStates[HuffMSB], stream, &msb)) {
            return -1;
        }
        seekRunLengths = nextSymbolIsRL(msb);

        *valueOut = (int16_t)(*valueOut & 0xfe);

        int32_t exp = (msb & 0x7f) << 8 | *valueOut;
        *valueOut = (int16_t)(exp - 0x4000);
    } else {
        *valueOut = (int16_t)((*valueOut & 0x7e) - 0x40);
    }
    *valueOut >>= 1;

    /* RL */
    int32_t zeros = triplet.rl;
    const HuffmanManualDecodeState* rlState = &state->manualStates[HuffRL];
    uint8_t rlDetectionSymbol = 0;
    while (seekRunLengths) {
        if (!huffmanLutDecode(&state->rlTable, stream, &rlDetectionSymbol)) {
            if (!huffmanManualDecodeMaybeSingleSymbol(rlState, stream, &rlDetectionSymbol)) {
                return -1;
            }
        }
        zeros = (zeros << 7) | (rlDetectionSymbol & 0x7f);
        seekRunLengths = nextSymbolIsRL(rlDetectionSymbol);
    }

    return zeros;
}

/*------------------------------------------------------------------------------*/
