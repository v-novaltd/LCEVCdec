/* Copyright (c) V-Nova International Limited 2023. All rights reserved.
 *
 * This is deliberately written as a C module to allow use by various C-only integrations - it
 * is logic that we really don't want being reimplemented with subtle bugs.
 */
#include "LCEVC/utility/extract.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Number of different NAL types to search for in a block of ES data
 */
#define kNumNalTypes 3

/* nalu types for SEI
 */
#define H264_NAL_UNIT_TYPE_SEI 6
#define H265_NAL_UNIT_TYPE_PREFIX_SEI 39
#define H266_NAL_UNIT_TYPE_PREFIX_SEI 23

/* LCEVC nalu types as read by an H264 parser on bits 0,1,2,3,4
 */
#define H264_NAL_UNIT_TYPE_LCEVC_NON_IDR 25
#define H264_NAL_UNIT_TYPE_LCEVC_IDR 27

/* LCEVC nalu types as read by an H265 parser on bits 1,2,3,4,5,6
 */
#define H265_NAL_UNIT_TYPE_LCEVC_NON_IDR 60
#define H265_NAL_UNIT_TYPE_LCEVC_IDR 61

/* LCEVC nalu types as read by an H266 parser on bits 3,4,5,6,7 of the 2nd byte
 */
#define H266_NAL_UNIT_TYPE_LCEVC_NON_IDR 31
#define H266_NAL_UNIT_TYPE_LCEVC_IDR 31

/* Possible NAL types for each sort of MPEG elementary stream
 */
static const uint8_t kNalTypes[][kNumNalTypes] = {
    {0}, /* LCEVC_CodecType_Unknown */
    {H264_NAL_UNIT_TYPE_SEI, H264_NAL_UNIT_TYPE_LCEVC_NON_IDR,
     H264_NAL_UNIT_TYPE_LCEVC_IDR}, /* LCEVC_CodecType_H264 */
    {H265_NAL_UNIT_TYPE_PREFIX_SEI, H265_NAL_UNIT_TYPE_LCEVC_NON_IDR, H265_NAL_UNIT_TYPE_LCEVC_IDR}, /* LCEVC_CodecType_H265 */
    {H266_NAL_UNIT_TYPE_PREFIX_SEI, H266_NAL_UNIT_TYPE_LCEVC_NON_IDR, H266_NAL_UNIT_TYPE_LCEVC_IDR}, /* LCEVC_CodecType_H266 */
};

/* The interesting SEI NAL type for each sort of MPEG elementary stream
 */
static const uint8_t kNalTypesSEI[] = {
    0,                             /* LCEVC_CodecType_Unknown */
    H264_NAL_UNIT_TYPE_SEI,        /* LCEVC_CodecType_H264 */
    H265_NAL_UNIT_TYPE_PREFIX_SEI, /* LCEVC_CodecType_H265 */
    H266_NAL_UNIT_TYPE_PREFIX_SEI, /* LCEVC_CodecType_H266 */
};

/* Payload type for SEI registered user data
 */
const uint8_t kSeiPayloadTypeUserDataRegisteredItuTt35 = 0x04;

/* Registered SEI user data ID for LCEVC
 */
const uint8_t kITU[4] = {0xb4, 0x00, 0x50, 0x00};

/* State of LCEVC extraction
 */
typedef struct ExtractState
{
    /* Overall ES type
     */
    LCEVC_CodecType codecType;

    /* AnnexeB vs. length prefix
     */
    LCEVC_NALFormat nalFormat;

    /* NAL Unit types that are of interest
     */
    const uint8_t* nalTypes;

    /* NAL type used for LCEVC registered user data
     */
    uint8_t nalTypeSEI;

    /* Data buffer being searched
     */
    uint8_t* data;
    uint32_t size;

    /* Current offset within data buffer
     */
    uint32_t offset;
} ExtractState;

/* Describes a found NAL unit in data buffer
 */
typedef struct NalUnitSpan
{
    /* Overall span of NAL unit
     */
    uint8_t* data;
    uint32_t size;

    /* Where payload starts in NAL unit
     */
    const uint8_t* payload;

    /* The type of NAL unit
     */
    uint8_t type;
} NalUnitSpan;

/* Copy from src to dst, removing 'start code emulation prevention' sequences
 * Any leading zeros should be signalled in 'zeros'
 */
static uint32_t unencapsulate(uint32_t zeros, uint8_t* dst, const uint8_t* src, uint32_t size)
{
    const uint8_t* end = src + size;
    uint8_t* dstPtr = dst;

    /* 0b00000000 0b00000000 0b00000011 0b000000xx -> 0b00000000 0b00000000 0b000000xx
     */
    while (src < end) {
        if (*src == 0) {
            if(zeros < 2) {
                zeros++;
            }
        } else {
            if (zeros == 2 && *src == 3) {
                src++;
            }
            zeros = 0;
        }

        *dstPtr++ = *src++;
    }

    return (uint32_t)(dstPtr - dst);
}

static uint32_t getNalUnitType(ExtractState* state, const uint8_t* nalUnitHeader)
{
    switch (state->codecType) {
        case LCEVC_CodecType_H264: return nalUnitHeader < (state->data + state->size)?(nalUnitHeader[0] & 0x1F):0;
        case LCEVC_CodecType_H265: return nalUnitHeader < (state->data + state->size)?((nalUnitHeader[0] >> 1) & 0x3F):0;
        case LCEVC_CodecType_H266: return nalUnitHeader < (state->data + state->size - 1)?(nalUnitHeader[1] >> 3):0;
        default: return 0;
    }
}

static uint8_t getNalUnitHeaderSize(ExtractState* state, uint8_t nalType)
{
    return (state->codecType == LCEVC_CodecType_H266 || state->codecType == LCEVC_CodecType_H265 ||
            (state->codecType == LCEVC_CodecType_H264 &&
             (nalType == H264_NAL_UNIT_TYPE_LCEVC_NON_IDR || nalType == H264_NAL_UNIT_TYPE_LCEVC_IDR)))
               ? 2
               : 1;
}

/* Look for the next interesting NAL unit in data, using Annexe B start code delimiters
 */
static bool findNextNalUnitAnnexeB(ExtractState* state, NalUnitSpan* nalSpan)
{
    uint32_t zeros = 0;
    nalSpan->type = 0;
    nalSpan->data = NULL;
    nalSpan->payload = NULL;
    nalSpan->size = 0;

    for (; state->offset < state->size; state->offset++) {
        if (state->data[state->offset] == 0) {
            if (zeros < 3) {
                zeros++;
            }
        } else if (zeros >= 2 && state->data[state->offset] == 1) {
            if (nalSpan->data) {
                state->offset -= zeros;
                break;
            }
            nalSpan->data = state->data + state->offset - zeros;
            nalSpan->payload = state->data + state->offset + 1;
            zeros = 0;
        } else {
            zeros = 0;
        }
    }

    if (!nalSpan->data) {
        return false;
    }
    const uint8_t nalType = getNalUnitType(state, nalSpan->payload);
    if (memchr(state->nalTypes, nalType, kNumNalTypes)) {
        nalSpan->type = nalType;
        nalSpan->size = (uint32_t)((state->data + state->offset) - nalSpan->data);
        nalSpan->payload += getNalUnitHeaderSize(state, nalType);
    }

    return true;
}

/* Look for the next interesting NAL unit in data, using length prefix delimiters
 */
static bool findNextNalUnitLengthPrefix(ExtractState* state, NalUnitSpan* nalSpan)
{
    const uint8_t kStartPrefixLen = 4;

    /* Initialise un-found state.
     */
    nalSpan->data = NULL;

    while (state->offset < state->size && (nalSpan->data == NULL)) {
        nalSpan->size = kStartPrefixLen + (((uint32_t)state->data[state->offset] << 24) |
                                           ((uint32_t)state->data[state->offset + 1] << 16) |
                                           ((uint32_t)state->data[state->offset + 2] << 8) |
                                           state->data[state->offset + 3]);

        uint8_t nalType = getNalUnitType(state, &state->data[state->offset + kStartPrefixLen]);
        if (memchr(state->nalTypes, nalType, kNumNalTypes)) {
            nalSpan->type = nalType;
            nalSpan->data = state->data + state->offset;
            nalSpan->payload = nalSpan->data + kStartPrefixLen + getNalUnitHeaderSize(state, nalType);
        }
        state->offset += nalSpan->size;
    }

    return (state->offset < state->size);
}

/* Look for the next NAL unit in data
 */
static bool findNextNalUnit(ExtractState* state, NalUnitSpan* nalSpan)
{
    switch (state->nalFormat) {
        case LCEVC_NALFormat_AnnexeB: return findNextNalUnitAnnexeB(state, nalSpan);
        case LCEVC_NALFormat_LengthPrefix: return findNextNalUnitLengthPrefix(state, nalSpan);
        default: assert(0); return false;
    }
}

/* Edit out previously found NAL unit from AU data
 */
static void removeNalUnit(ExtractState* state, NalUnitSpan* nalSpan)
{
    assert(state->data <= nalSpan->data);
    assert((state->data + state->size) >= (nalSpan->data + nalSpan->size));
    assert((nalSpan->data + nalSpan->size) <= (state->data + state->offset));

    /* Move following data down
     */
    memmove(nalSpan->data, nalSpan->data + nalSpan->size, nalSpan->size);

    /* Adjust offset and size
     */
    state->offset -= nalSpan->size;
    state->size -= nalSpan->size;
}

/* Common work for both exported 'extract' functions
 */
uint32_t extractEnhancementFromNAL(uint8_t* data, uint32_t dataSize, LCEVC_CodecType codecType,
                                  LCEVC_NALFormat nalFormat, uint8_t* outputData, uint32_t outputCapacity,
                                  uint32_t* outputSize, bool remove, uint32_t* newDataSize)
{
    uint32_t outputOffset = 0;

    assert(codecType >= 0 && codecType < sizeof(kNalTypes) / sizeof(kNalTypes[0]));

    ExtractState state = {
        codecType, nalFormat, kNalTypes[codecType], kNalTypesSEI[codecType], data, dataSize,
    };

    NalUnitSpan nalSpan = {NULL, 0};

    while (findNextNalUnit(&state, &nalSpan)) {
        if (nalSpan.type == 0) {
            continue;
        }

        /* Don't do start code emulation prevention on the start of the
         * NAL units we care about - we know that the 0,0,[1-3] pattern will not
         * appear. The start code emulation prevention unencapsulate() call is only
         * invoked to copy the data into the output buffer.
         */
        if (nalSpan.type == state.nalTypeSEI) {
            uint32_t payloadOffset = 0;
            uint32_t payloadSize = 0;

            if (nalSpan.payload[0] != kSeiPayloadTypeUserDataRegisteredItuTt35) {
                continue;
            }
            /* Calculate SEI payload size
             */
            payloadOffset = 1;
            uint32_t seiSize = 0;
            while (nalSpan.payload[payloadOffset] == 0xFF) {
                seiSize += 0xFF;
                payloadOffset++;
            }
            seiSize += nalSpan.payload[payloadOffset++];

            /* Check SEI UUID type and move payload into vector
             */
            if (memcmp(kITU, nalSpan.payload + payloadOffset, sizeof(kITU)) != 0) {
                continue;
            }

            /* Found LCEVC data encapsulated in SEI User Data Registered
             */
            payloadOffset += sizeof(kITU);
            seiSize -= sizeof(kITU);
            payloadSize = (uint32_t)(nalSpan.size - (nalSpan.payload - nalSpan.data) - payloadOffset);

            /* Append payload to output
            */
            if (outputOffset + payloadSize > outputCapacity || seiSize > payloadSize) {
                return 0;
            }

            unencapsulate(1, outputData + outputOffset,
                                       nalSpan.payload + payloadOffset, payloadSize);

            outputOffset += seiSize;

        } else {
            /* Found LCEVC data encapsulated in LCEVC NAL unit
             */
            const uint32_t payloadSize = (uint32_t)(nalSpan.size - (nalSpan.data - nalSpan.payload));

            if (outputOffset + payloadSize > outputCapacity) {
                return 0;
            }

            outputOffset += unencapsulate(0, outputData + outputOffset,
                                          nalSpan.payload, payloadSize);
        }

        if (remove) {
            /* Remove NALU from source
             */
            removeNalUnit(&state, &nalSpan);
        }
    }

    assert(outputSize != NULL);
    *outputSize = outputOffset;

    if (newDataSize) {
        *newDataSize = state.size;
    }

    return outputOffset;
}

/* Extract LCEVC enhancement data from a buffer containing NAL Units
 */
void LCEVC_extractEnhancementFromNAL(const uint8_t* nalData, uint32_t nalSize, LCEVC_NALFormat nalFormat,
                                    LCEVC_CodecType codecType, uint8_t* enhancementData,
                                    uint32_t enhancementCapacity, uint32_t* enhancementSize)
{
    assert(nalData);
    assert(enhancementData);
    assert(enhancementSize);

    extractEnhancementFromNAL((uint8_t *)(nalData), nalSize, codecType, nalFormat, enhancementData, enhancementCapacity, enhancementSize, false, NULL);
}


/* Extract LCEVC enhancement data from a buffer containing NAL Units, and splice extracted data from input buffer.
 */
void LCEVC_extractAndRemoveEnhancementFromNAL(uint8_t* nalData, uint32_t nalSize,
                                             LCEVC_NALFormat nalFormat, LCEVC_CodecType codecType,
                                             uint32_t* nalOutSize, uint8_t* enhancementData,
                                             uint32_t enhancementCapacity, uint32_t* enhancementSize)
{
    assert(nalData);
    assert(enhancementData);
    assert(enhancementSize);
    assert(nalOutSize);

    extractEnhancementFromNAL(nalData, nalSize, codecType, nalFormat, enhancementData,
                             enhancementCapacity, enhancementSize, true, nalOutSize);
}
