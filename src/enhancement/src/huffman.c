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

#include "huffman.h"

#include "chunk_parser.h"

#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/stdlib.h>

/*- General utility functions -------------------------------------------------------------------*/

/* Count the leading zeroes of a code, where the code is `numBits` wide. Unlike plain clz, a code
 * of 0 has numBits leading zeroes (rather than being undefined behaviour). */
static inline uint8_t clzCode(uint32_t code, uint8_t numBits)
{
#if VN_COMPILER(MSVC)
    return (uint8_t)(_lzcnt_u32(code) + numBits - 32);
#elif VN_COMPILER(GCC) && (__GNUC__ >= 14)
    /* Annoyingly, it's undefined behaviour if you provide 0 to __builtin_clz. To match the Windows
     * behaviour (where 0 is just another leading zero), we need clzg, with sizeof(code) as
     * the default arg. */
    return (uint8_t)(__builtin_clzg(code, (int)(sizeof(code) * 8)) + numBits - 32);
#else
    /* clzg is only available in GCC version 14 or later. Outside that, we just do it manually. */
    if (code == 0) {
        return numBits;
    }
    return (uint8_t)(__builtin_clz(code) + numBits - 32);
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

    for (int16_t idx = (int16_t)(maxIdx - 1); idx > -1; idx--) {
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
static uint16_t generateCodesAndLut(HuffmanListEntry entriesIn[VN_MAX_NUM_SYMBOLS],
                                    HuffmanTable* tableOut, uint16_t maxIdx, uint8_t maxCodeLength)
{
    memset(tableOut->code, 0, sizeof(tableOut->code));
    uint8_t currLength = maxCodeLength;
    uint8_t currCode = 0;
    uint16_t minOversizedCodeIdx = maxIdx;

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
            minOversizedCodeIdx = (uint16_t)idx;
        } else {
            uint16_t tableIdx = (uint16_t)(currCode << (VN_SMALL_TABLE_MAX_SIZE - entry->bits));
            const uint16_t tableIdxEnd =
                tableIdx + (uint16_t)(1 << (VN_SMALL_TABLE_MAX_SIZE - entry->bits));
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
    for (uint16_t idx = 0; idx < listInOut->size; idx++) {
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

static inline void huffmanCodeLengthCountsAdd(uint16_t codeLengthCounts[VN_MAX_CODE_LENGTH + 1],
                                              uint8_t codeLength, uint8_t maxCodeLength)
{
    if (codeLength == 0) {
        return;
    }

    codeLengthCounts[minU8(codeLength, maxCodeLength)]++;
}

static bool validateCodeLengths(const uint16_t codeLengthCounts[VN_MAX_CODE_LENGTH + 1], uint8_t maxCodeLength)
{
    uint16_t currCode = 0;
    uint8_t currLength = maxCodeLength;

    for (uint8_t bits = maxCodeLength; bits > 0; --bits) {
        if (bits < currLength) {
            currCode >>= (currLength - bits);
            currLength = bits;
        }

        const uint16_t count = codeLengthCounts[bits];
        if (bits <= VN_SMALL_TABLE_MAX_SIZE && count > 0) {
            const uint16_t maxCode = (uint16_t)(1u << bits);
            if (currCode >= maxCode || count > (uint16_t)(maxCode - currCode)) {
                VNLogError("Huffman code lengths are too large for the LUT");
                return false;
            }
        }

        currCode += count;
    }

    return true;
}

/* The final decoded value of a coefficient which has an LSB symbol but no MSB symbol. Complete
 * triple-table entries store this precomputed, so the decode fast path is a plain load. */
static inline uint8_t lsbOnlyValue(uint8_t lsbSymbol)
{
    return (uint8_t)(int8_t)((int16_t)((lsbSymbol & 0x7E) - 0x40) >> 1);
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
    uint16_t codeLengthCounts[VN_MAX_CODE_LENGTH + 1] = {0};
    if (bits) {
        for (int32_t i = 0; i < VN_MAX_NUM_SYMBOLS; ++i) {
            /* Symbol present flag */
            VNCheckI(huffmanStreamReadBits(stream, 1, &bits));
            if (bits) {
                uint32_t codeLength = 0;
                if (lengthBits > 0) {
                    VNCheckI(huffmanStreamReadBits(stream, lengthBits, &codeLength));
                }
                const uint8_t entryBits = (uint8_t)(codeLength + state->minCodeLength);
                if (entryBits > state->maxCodeLength) {
                    VNLogError("huffman: code length [%u] exceeds signalled max length [%u]",
                               entryBits, state->maxCodeLength);
                    return -1;
                }
                if (entryBits == 0) {
                    continue;
                }
                huffmanCodeLengthCountsAdd(codeLengthCounts, entryBits, state->maxCodeLength);
                entriesOut[orderIdx].symbol = (uint8_t)i;
                entriesOut[orderIdx].bits = entryBits;
                orderIdx++;
            }
        }
    } else { /* Read symbol-count */
        uint32_t symbolCount = 0;
        VNCheckI(huffmanStreamReadBits(stream, 5, &symbolCount));

        for (uint32_t i = 0; i < symbolCount; ++i) {
            uint32_t symbol = 0;
            uint32_t codeLength = 0;
            VNCheckI(huffmanStreamReadBits(stream, 8, &symbol));
            if (lengthBits > 0) {
                VNCheckI(huffmanStreamReadBits(stream, lengthBits, &codeLength));
            }
            const uint8_t entryBits = (uint8_t)(codeLength + state->minCodeLength);
            if (entryBits > state->maxCodeLength) {
                VNLogError("huffman: code length [%u] exceeds signalled max length [%u]", entryBits,
                           state->maxCodeLength);
                return -1;
            }
            if (entryBits == 0) {
                continue;
            }
            huffmanCodeLengthCountsAdd(codeLengthCounts, entryBits, state->maxCodeLength);

            entriesOut[orderIdx].symbol = (uint8_t)symbol;
            entriesOut[orderIdx].bits = entryBits;
            orderIdx++;
        }
    }

    if (!validateCodeLengths(codeLengthCounts, state->maxCodeLength)) {
        return -1;
    }

    qsort(entriesOut, orderIdx, sizeof(HuffmanListEntry), listEntrySizeOrder);

    return orderIdx;
}

/* Declare this so that we can have a tag-team pair of recursive functions. */
static uint16_t huffmanIterateRls(HuffmanTripleTable* huffmanTableOut, const HuffmanTable* rlTable,
                                  const HuffmanList* rlList, uint16_t parentStartIdx,
                                  uint16_t parentEndIdx, uint8_t lsbSymbol, uint16_t rlSymbol,
                                  uint8_t codeSizeInStream, uint8_t recursionLevel);

/* Slightly hideous function but, sonarQube prefers functions with too many params over excessive
 * code duplication. */
static uint16_t huffmanIterateRlsLoopBody(HuffmanTripleTable* huffmanTableOut,
                                          const HuffmanTable* rlTable, const HuffmanList* rlList,
                                          uint16_t parentStartIdx, uint16_t lowestValidIdxYet,
                                          uint8_t lsbSymbol, uint16_t rlSymbol,
                                          uint8_t codeSizeInStream, uint16_t newRlCode,
                                          uint8_t newRlSymbol, uint8_t newRlBits, uint8_t recursionLevel)
{
    const uint8_t codeSizeInTable =
        codeSizeInStream - (uint8_t)(parentStartIdx >> VN_BIG_TABLE_MAX_CODE_SIZE);
    const uint8_t bitsLeft = VN_BIG_TABLE_MAX_CODE_SIZE - codeSizeInTable;
    const uint8_t bitsLeftByRl1 = bitsLeft - newRlBits;
    const uint16_t startIdxRl1 = parentStartIdx | (uint16_t)(newRlCode << bitsLeftByRl1);
    const uint16_t endIdxRl1 = startIdxRl1 + (uint16_t)(1 << bitsLeftByRl1);
    codeSizeInStream += newRlBits;

    /* recursive case: */
    if (nextSymbolIsRL(newRlSymbol)) {
        const uint16_t out =
            huffmanIterateRls(huffmanTableOut, rlTable, rlList, startIdxRl1, endIdxRl1, lsbSymbol,
                              (uint16_t)((rlSymbol << 7) | (newRlSymbol & 0x7f)), codeSizeInStream,
                              recursionLevel + 1);
        return minU16(lowestValidIdxYet, out);
    }

    /* non-recursive case: the triple is fully resolved, so store the precomputed value. */
    for (uint16_t idx = startIdxRl1; idx < endIdxRl1; idx++) {
        huffmanTableOut->code[idx].lsbOrValue = lsbOnlyValue(lsbSymbol);
        huffmanTableOut->code[idx].rl = (uint16_t)((rlSymbol << 7) | (newRlSymbol & 0x7f));
        huffmanTableOut->code[idx].contents = (uint8_t)((codeSizeInStream << 3) | 0x04);
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
    const uint8_t codeSizeInTable =
        codeSizeInStream - (uint8_t)(parentStartIdx >> VN_BIG_TABLE_MAX_CODE_SIZE);
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
                huffmanTableOut, rlTable, rlList, parentStartIdx, lowestValidlySetIdx, lsbSymbol,
                rlSymbol, codeSizeInStream, rlCode, nextRlEntry->symbol, rlBits, recursionLevel);
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
                    huffmanTableOut, rlTable, rlList, parentStartIdx, lowestValidlySetIdx,
                    lsbSymbol, rlSymbol, codeSizeInStream, nextRlEntry->code, nextRlEntry->symbol,
                    rlBits, recursionLevel);
            }
        }
    }

    /* Fill in the gap between the lowest entry we set, and the lowest entry that our parent sets.
     * All entries in this gap are incomplete due to RL overflow: we're only here because the
     * current symbol definitely IS followed by an RL, but these are the indices where none fit. */
    for (uint16_t idx = parentStartIdx; idx < lowestValidlySetIdx; idx++) {
        huffmanTableOut->code[idx].lsbOrValue = lsbSymbol;
        huffmanTableOut->code[idx].rl = rlSymbol;
        huffmanTableOut->code[idx].contents = (uint8_t)((codeSizeInStream << 3) | 0x01);
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
    uint16_t lsbIdx = 0;
    for (; lsbIdx < fullLsbListIn->size; lsbIdx++) {
        const HuffmanListEntry* lsbEntry = &fullLsbListIn->list[lsbIdx];
        uint8_t leadingZeroes = clzCode(lsbEntry->code, lsbEntry->bits);
        leadingZeroes = minU8(leadingZeroes, VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES);
        int8_t bitsLeftByLsb = VN_BIG_TABLE_MAX_CODE_SIZE - (lsbEntry->bits - leadingZeroes);
        if (bitsLeftByLsb < 0) {
            break;
        }

        uint16_t startIdx = (uint16_t)(lsbEntry->code << bitsLeftByLsb);
        startIdx |= (leadingZeroes << VN_BIG_TABLE_MAX_CODE_SIZE);
        const uint16_t endIdx = startIdx + (uint16_t)(1 << bitsLeftByLsb);
        if (nextSymbolIsMSB(lsbEntry->symbol)) {
            for (uint16_t outIdx = startIdx; outIdx < endIdx; outIdx++) {
                huffmanTableOut->code[outIdx].lsbOrValue = lsbEntry->symbol;
                huffmanTableOut->code[outIdx].contents = (uint8_t)((lsbEntry->bits << 3) | 0x02);
            }
            continue;
        }

        if (!nextSymbolIsRL(lsbEntry->symbol)) {
            for (uint16_t outIdx = startIdx; outIdx < endIdx; outIdx++) {
                huffmanTableOut->code[outIdx].lsbOrValue = lsbOnlyValue(lsbEntry->symbol);
                huffmanTableOut->code[outIdx].contents = (uint8_t)((lsbEntry->bits << 3) | 0x04);
            }
            continue;
        }

        huffmanIterateRls(huffmanTableOut, rlTable, rlList, startIdx, endIdx, lsbEntry->symbol, 0,
                          lsbEntry->bits, 0);
    }

    /* These are all the entries where the LSB is too long to fit in a LUT entry. Some may be
     * shorter than expected, if the max num of leading zeroes is low enough. */
    if (fullLsbListIn->size > lsbIdx) {
        const uint16_t additionalEntries = fullLsbListIn->size - lsbIdx;
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
        VNLogError("Failed to initialize huffman state - possible stream corruption");
        return false;
    }
    lsbList.size = res;
    generateCodes(lsbList.list, lsbList.size, state->manualStates[HuffLSB].maxCodeLength);

    /* MSB */
    res = huffmanManualInitializeCommon(&state->manualStates[HuffMSB], stream, bitstreamVersion,
                                        state->manualStates[HuffMSB].list.list);
    if (res < 0) {
        VNLogError("Failed to initialize huffman state - possible stream corruption");
        return false;
    }
    state->manualStates[HuffMSB].list.size = res;
    generateCodesAndIndices(
        state->manualStates[HuffMSB].list.list, state->manualStates[HuffMSB].list.idxOfEachBitSize,
        state->manualStates[HuffMSB].list.size, state->manualStates[HuffMSB].maxCodeLength);

    /* RL */
    if (!huffmanManualInitializeWithLut(&state->manualStates[HuffRL], &state->rlTable, stream,
                                        bitstreamVersion)) {
        VNLogError("Failed to initialize huffman state - possible stream corruption");
        return false;
    }

    /* Triple Table */
    memset(&state->tripleTable, 0, sizeof(state->tripleTable));
    huffmanTripleTableAssign(&state->tripleTable, state, &lsbList, &state->rlTable,
                             &state->manualStates[HuffRL].list);

    /* Cap the decode-time leading-zero count at min(maxLsbCodeLength, max leading zeroes): a run
     * of stream-zeros longer than the longest LSB code must still land on the longest (all-zeros)
     * code's table bucket. */
    const uint8_t maxLsbCodeLength = lsbList.size > 0 ? lsbList.list[lsbList.size - 1].bits : 0;
    const uint8_t leadingZeroCap = minU8(maxLsbCodeLength, VN_BIG_TABLE_MAX_NUM_LEADING_ZEROES);
    state->lsbGuardBit = 0x80000000u >> leadingZeroCap;

    return true;
}

/*- HuffmanStream -------------------------------------------------------------------------------*/

bool huffmanStreamInitialize(HuffmanStream* stream, const uint8_t* data, size_t size)
{
    if (!bytestreamInitialize(&stream->byteStream, data, size)) {
        return false;
    }

    stream->word = 0;
    stream->bitCount = 0;

    return true;
}

/*- HuffmanManualDecodeState --------------------------------------------------------------------*/

bool huffmanManualInitializeWithLut(HuffmanManualDecodeState* state, HuffmanTable* table,
                                    HuffmanStream* stream, uint8_t bitstreamVersion)
{
    assert(state && table && stream);

    HuffmanListEntry codes[VN_MAX_NUM_SYMBOLS];
    const int16_t size = huffmanManualInitializeCommon(state, stream, bitstreamVersion, codes);
    if (size < 0) {
        // The table is internally inconsistent (e.g., signaled code lengths but zero symbols).
        VNLogError("Failed to initialize huffman state");
        return false;
    }
    if (size == 0) {
        return true;
    }

    const uint16_t minIdxOfOversizedCodes =
        generateCodesAndLut(codes, table, (uint16_t)size, state->maxCodeLength);

    state->list.size = size - minIdxOfOversizedCodes;
    if (state->list.size > 0) {
        memcpy(state->list.list, &codes[minIdxOfOversizedCodes],
               sizeof(HuffmanListEntry) * (state->list.size));

        determineIdxOfEachBitSize(&state->list);
    }

    return true;
}

bool huffmanManualDecode(const HuffmanManualDecodeState* state, HuffmanStream* stream, uint8_t* symbolOut)
{
    const HuffmanList* list = &state->list;

    if (list->size == 0) {
        return false;
    }

    /* One refill guarantees at least min(56, remaining) bits: enough for any code (max 31 bits),
     * so the whole search can peek straight from the buffered word. Bits past the end of the
     * stream read as zeros, which end up rejected as an unknown code. */
    huffmanStreamRefill(stream);

    /* list->list is sorted by code length (increasing), then by code itself (decreasing) */
    uint16_t idx = 0;
    while (idx < list->size) {
        const HuffmanListEntry* entry = &list->list[idx];
        const uint8_t bits = entry->bits;
        const uint32_t code = huffmanStreamPeekBits(stream, bits);

        /* Binary search the equal-length group [idx, idxOfEachBitSize[bits]). Codes are sorted
         * descending, so a larger code sits at a lower index. Half-open bounds that strictly shrink
         * each step guarantee termination even when the code matches no entry. */
        uint16_t lowerLimit = idx;
        uint16_t upperLimit = list->idxOfEachBitSize[bits];
        while (lowerLimit < upperLimit) {
            const uint16_t testIdx = lowerLimit + (upperLimit - lowerLimit) / 2;
            entry = &list->list[testIdx];
            if (code > entry->code) {
                upperLimit = testIdx;
            } else if (code < entry->code) {
                lowerLimit = testIdx + 1;
            } else {
                /* Found it! Consume the bits we just matched. */
                huffmanStreamConsume(stream, bits);
                *symbolOut = entry->symbol;
                return true;
            }
        }
        idx = list->idxOfEachBitSize[bits];
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

/*- HuffmanTripleDecodeState --------------------------------------------------------------------*/

int32_t huffmanTripleDecodeSlow(const HuffmanTripleDecodeState* state, HuffmanStream* stream,
                                int16_t* valueOut, HuffmanTriple triplet)
{
    /* Seek run lengths if:
     * (1) the lsb overflowed, and either
     *      (a) is followed by an RL, or
     *      (b) is followed by and MSB, and THAT is followed by an RL
     * or
     * (2) the msb overflowed, and it's followed by an RL,
     * or
     * (3) the rl itself overflowed (which is always true if the others aren't, since this
     *     function is only reachable when SOME part overflowed).
     */
    bool seekRunLengths = true;
    uint8_t lsb = triplet.lsbOrValue;

    /* LSB */
    if (huffmanTripleLsbOverflowed(triplet.contents)) {
        if (!huffmanManualDecodeMaybeSingleSymbol(&state->manualStates[HuffLSB], stream, &lsb)) {
            return -1;
        }
        seekRunLengths = nextSymbolIsRL(lsb);
    }

    /* MSB */
    int16_t value = 0;
    if (nextSymbolIsMSB(lsb)) {
        uint8_t msb = 0;
        if (!huffmanManualDecodeMaybeSingleSymbol(&state->manualStates[HuffMSB], stream, &msb)) {
            return -1;
        }
        seekRunLengths = nextSymbolIsRL(msb);

        const int32_t exp = ((msb & 0x7f) << 8) | (lsb & 0xfe);
        value = (int16_t)(exp - 0x4000);
    } else {
        value = (int16_t)((lsb & 0x7e) - 0x40);
    }
    *valueOut = (int16_t)(value >> 1);

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
        if (zeros >= (1 << 24)) {
            return -1; // Don't overflow a 24-bit integer to prevent crashes on subsequent << 7
        }
        zeros = (zeros << 7) | (rlDetectionSymbol & 0x7f);
        seekRunLengths = nextSymbolIsRL(rlDetectionSymbol);
    }

    return zeros;
}

/*------------------------------------------------------------------------------*/
