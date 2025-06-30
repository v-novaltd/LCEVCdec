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

#ifndef VN_LCEVC_ENHANCEMENT_ENTROPY_H
#define VN_LCEVC_ENHANCEMENT_ENTROPY_H

#include "config_parser_types.h"
#include "huffman.h"

#include <LCEVC/enhancement/config_types.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

/*! \brief temporal Huffman states. */
enum
{
    HuffTemporalZero,
    HuffTemporalOne,
    HuffTemporalCount
};

enum EntropyConstants
{
    EntropyNoData = -2
};

typedef enum EntropyDecoderType
{
    EDTDefault = 0,
    EDTTemporal,
    EDTSizeUnsigned,
    EDTSizeSigned,
    EDTCount
} EntropyDecoderType;

typedef struct EntropyDecoder
{
    uint8_t currHuff;
    uint32_t rawOffset;
    HuffmanSingleDecoder_t huffman[HuffTemporalCount]; // Note that HuffTemporalCount == HuffSizeCount
    HuffmanTripleDecodeState comboHuffman;
    HuffmanStream hstream;
    bool rleOnly;
    const uint8_t* rleData;
    bool entropyEnabled;
    EntropyDecoderType type;
} EntropyDecoder;

/*------------------------------------------------------------------------------*/

/*! \brief Initialize a entropy decoder into a default state from for decompressing.
 *
 *  \param state              state of the decoder to initialize
 *  \param chunk              chunk to use for this layer.
 *  \param type               specifies the type of layer decoder to prepare for this chunk.
 *  \param bitstreamVersion   Stream version (streams that lack this are treated as "current"
 *
 *  \return True on success, otherwise false.
 */
bool entropyInitialize(EntropyDecoder* state, const LdeChunk* chunk, EntropyDecoderType type,
                       uint8_t bitstreamVersion);

/*! \brief Decode the next coefficient from a stream. Coefficients are the things that get
 *         transformed ("Inverse hadamard transformed") to produce residuals.
 *
 *  \param state    layer decoder state to decode from
 *  \param coeffOut s8.7 fixed point coefficient to decode into
 *
 *  \return the number of zeros following this coefficient (<0 on fail)
 */
int32_t entropyDecode(EntropyDecoder* state, int16_t* coeffOut);

/*! \brief Decode the next temporal signal from a temporal stream.
 *
 *  \param state  layer decoder state to decode from
 *  \param out    location to decode temporal signal into
 *
 *  \return the number of zeros following this pel (<0 on fail)
 */
int32_t entropyDecodeTemporal(EntropyDecoder* state, TemporalSignal* out);

/*! \brief Decode the next size signal from a compressed size stream.
 *
 *  \param state  layer decoder state to decode from
 *  \param size   int16_t location to decode size into
 *
 *  \return True on success, otherwise false.
 */
bool entropyDecodeSize(EntropyDecoder* state, int16_t* size);

/*! \brief Retrieve the number of bytes consumed by the entropy decoder.
 *
 *  \param state  layer decoder state.
 *
 *  \return the number of bytes read by the decoder. */
uint32_t entropyGetConsumedBytes(const EntropyDecoder* state);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_ENTROPY_H
