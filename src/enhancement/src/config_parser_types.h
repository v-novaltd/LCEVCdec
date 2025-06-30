/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_CONFIG_PARSER_TYPES_H
#define VN_LCEVC_ENHANCEMENT_CONFIG_PARSER_TYPES_H

#include <LCEVC/enhancement/bitstream_types.h>
#include <stdbool.h>
#include <stdint.h>
/*------------------------------------------------------------------------------*/

static const uint32_t kDefaultDeblockCoefficient = 16;        /* 8.9.2 */
static const uint32_t kDefaultTemporalStepWidthModifier = 48; /* 7.4.3.3 */
static const uint32_t kDefaultChromaStepWidthMultiplier = 64; /* 7.4.3.3 */

typedef enum SignalledBlockSize
{
    BS_0,
    BS_1,
    BS_2,
    BS_3,
    BS_4,
    BS_5,
    BS_Reserved1,
    BS_Custom
} SignalledBlockSize;

typedef enum TemporalSignal
{
    TSInter = 0, /* i.e. add */
    TSIntra = 1, /* i.e. set */
    TSCount = 2
} TemporalSignal;

static inline bool blockSizeFromEnum(SignalledBlockSize type, uint32_t* res)
{
    if (type < BS_0 || type > BS_5) {
        return false;
    }

    *res = (uint32_t)type;
    return true;
}

typedef enum BlockType
{
    BT_SequenceConfig,
    BT_GlobalConfig,
    BT_PictureConfig,
    BT_EncodedData,
    BT_EncodedDataTiled,
    BT_AdditionalInfo,
    BT_Filler,
    BT_Count
} BlockType;

typedef enum AdditionalInfoType
{
    AIT_SEI = 0,
    AIT_VUI = 1,
    AIT_SFilter = 23,
    AIT_HDR = 25,
} AdditionalInfoType;

typedef enum SEIPayloadType
{
    SPT_MasteringDisplayColourVolume = 1,
    SPT_ContentLightLevelInfo = 2,
    SPT_UserDataRegistered = 4,
} SEIPayloadType;

typedef struct Resolution
{
    uint16_t width;
    uint16_t height;
} Resolution;

/*! \brief The LCEVC standard defines common resolutions to avoid defining the resolution in every
 *         bitstream. See section 7.4.3.3 (Table 20) of the standard.
 */
static const Resolution kResolutions[] = {
    {0, 0},       {360, 200},   {400, 240},   {480, 320},   {640, 360},   {640, 480},
    {768, 480},   {800, 600},   {852, 480},   {854, 480},   {856, 480},   {960, 540},
    {960, 640},   {1024, 576},  {1024, 600},  {1024, 768},  {1152, 864},  {1280, 720},
    {1280, 800},  {1280, 1024}, {1360, 768},  {1366, 768},  {1400, 1050}, {1440, 900},
    {1600, 1200}, {1680, 1050}, {1920, 1080}, {1920, 1200}, {2048, 1080}, {2048, 1152},
    {2048, 1536}, {2160, 1440}, {2560, 1440}, {2560, 1600}, {2560, 2048}, {3200, 1800},
    {3200, 2048}, {3200, 2400}, {3440, 1440}, {3840, 1600}, {3840, 2160}, {3840, 2400},
    {4096, 2160}, {4096, 3072}, {5120, 2880}, {5120, 3200}, {5120, 4096}, {6400, 4096},
    {6400, 4800}, {7680, 4320}, {7680, 4800},
};

static const uint32_t kResolutionCount = (sizeof(kResolutions) / sizeof(Resolution));
static const uint32_t kResolutionCustom = 63;

/*! \brief Standard defined kernels. See section 8.7 of the standard */
static const LdeKernel kKernels[] = {
    /* Nearest */
    {{{16384, 0}, {0, 16384}}, 2, false},

    /* Bilinear */
    {{{12288, 4096}, {4096, 12288}}, 2, false},

    /* Bicubic (a = -0.6) */
    {{{-1382, 14285, 3942, -461}, {-461, 3942, 14285, -1382}}, 4, false},

    /* ModifiedCubic */
    {{{-2360, 15855, 4165, -1276}, {-1276, 4165, 15855, -2360}}, 4, false},

    /* AdaptiveCubic */
    {{{0}, {0}}, 0, false},

    /* US_Reserved1 */
    {{{0}, {0}}, 0, false},

    /* US_Reserved2 */
    {{{0}, {0}}, 0, false},

    /* US_Unspecified */
    {{{0}, {0}}, 0, false},

    /* Lanczos */
    {{{493, -2183, 14627, 4440, -1114, 121}, {121, -1114, 4440, 14627, -2183, 493}}, 6, false},

    /* Bicubic with prediction */
    {{{231, -2662, 16384, 2662, -231, 0}, {0, -231, 2662, 16384, -2662, 231}}, 6, true},

    /* MISHUS filter */
    {{{-2048, 16384, 2048, 0}, {0, 2048, 16384, -2048}}, 4, true},
};

static const uint32_t kVUIAspectRatioIDCExtendedSAR = 255;
static const uint64_t kMaximumConformanceWindowValue = (1 << 16) - 1;

enum
{
    ITUC_Length = 4
};
/*! \brief The V-Nova T.35 ITU code used to designate SEI packets with coded information from
 *         V-Nova, currently only used to store the bitstream version - which is not part of the
 *         LCEVC standard (as standards cannot have versions, only annex's and amendments). The
 *         first two bytes are the UK country code and the second two are specific to the V-Nova
 *         'manufacturer' - see https://www.cix.co.uk/~bpechey/H221/h221code.htm.
 */
static const uint8_t kVNovaITU[ITUC_Length] = {0xb4, 0x00, 0x50, 0x00};

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_CONFIG_PARSER_TYPES_H
