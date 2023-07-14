/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_HUFFMAN_H_
#define VN_DEC_CORE_HUFFMAN_H_

#include "common/bitstream.h"

/*------------------------------------------------------------------------------*/

typedef struct HuffmanTable HuffmanTable_t;
typedef struct Context Context_t;

typedef struct HuffmanEntry
{
    uint8_t symbol;
    uint8_t bits;
    HuffmanTable_t* next;
} HuffmanEntry_t;

typedef struct HuffmanTable
{
    HuffmanEntry_t* code;
    uint8_t idxBits;
} HuffmanTable_t;

typedef struct HuffmanStream
{
    BitStream_t stream;
    int16_t byte;
    uint64_t bitsRead;
} HuffmanStream_t;

/*! \brief huffman decoder state */
typedef struct HuffmanDecodeState
{
    uint8_t singleSymbol;      /**< When we only have one symbol in the alphabet, this is it */
    uint32_t minCodeLength;    /**< The smallest code word has this length */
    uint32_t maxCodeLength;    /**< The longest code word has this length */
    HuffmanEntry_t codes[256]; /**< The 8 bit index table of huffman codes */
    HuffmanTable_t table;      /**< Top level huffman table */
} HuffmanDecodeState_t;

/*------------------------------------------------------------------------------*/

/*! \brief Initialize a huffman decoder
 *
 *  \param state   State of the decoder to initialize
 *  \param stream  Bitstream to read initialization from
 *
 *  \return 0 on success, otherwise -1 */
int32_t huffmanInitialise(Context_t* ctx, HuffmanDecodeState_t* state, BitStream_t* stream);

/*! \brief De-initialize a huffman decoder
 *  \param state  state of the decoder to initialize */
void huffmanRelease(Context_t* ctx, HuffmanDecodeState_t* state);

/*! \brief Decode the next huffman symbol
 *
 *  \param ctx     decoding context
 *  \param state   huffman decoder state to decode from
 *  \param stream  bitstream to decode from
 *  \param symbol  decoded symbol
 *
 *  \return 0 on success, otherwise -1 */
int32_t huffmanDecode(Context_t* ctx, HuffmanDecodeState_t* state, HuffmanStream_t* stream, uint8_t* symbol);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_HUFFMAN_H_ */