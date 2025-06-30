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

#ifndef VN_LCEVC_ENHANCEMENT_HDR_TYPES_H
#define VN_LCEVC_ENHANCEMENT_HDR_TYPES_H

/*!
 * VN_MDCV_NUM_PRIMARIES.
 *
 * The number of primaries in the mastering_display_color_volume SEI message.
 */
#define VN_MDCV_NUM_PRIMARIES 3

/*!
 * \brief LCEVC HDR flags. Used to indicate the validity of the various fields in the HDR info structure.
 */
typedef enum LdeHDRFlags
{
    HDRMasteringDisplayColourVolumePresent = 0x00000001,
    HDRContentLightLevelInfoPresent = 0x00000002,
    HDRPayloadGlobalConfigPresent = 0x00000004,
    HDRToneMapperDataPresent = 0x00000008,
    HDRDeinterlacerEnabled = 0x00000010
} LdeHDRFlags;

/*!
 * \brief LCEVC VUI flags. Used to indicate the validity of the various fields in the VUI info structure
 */
typedef enum LdeVUIFlags
{
    VUIAspectRatioInfoPresent = 0x00000001,
    VUIOverscanInfoPresent = 0x00000010,
    VUIOverscanAppropriate = 0x00000020,
    VUIVideoSignalTypePresent = 0x00000100,
    VUIVideoSignalFullRangeFlag = 0x00000200,
    VUIVideoSignalColorDescPresent = 0x00000400,
    VUIChromaLocInfoPresent = 0x00001000,
} LdeVUIFlags;

typedef struct LdeDeinterlacingInfo
{
    uint8_t deinterlacerType;  /**< Valid if `HDRDeinterlacerEnabled` flag is set */
    uint8_t topFieldFirstFlag; /**< Valid if `HDRDeinterlacerEnabled` flag is set */
} LdeDeinterlacingInfo;

/*!
 * \brief LCEVC VUI video format.
 */
typedef enum LdeVUIVideoFormat
{
    VUIVFComponent,
    VUIVFPAL,
    VUIVFNTSC,
    VUIVFSECAM,
    VUIVFMAC,
    VUIVFUnspecified,
    VUIVFReserved0,
    VUIVFReserved1,
} LdeVUIVideoFormat;

/*!
 * \brief LCEVC VUI info. This contains the VUI info signaled in the LCEVC bitstream. More
 * information on what these parameters mean can be found in the LCEVC standard documentation (E.3).
 */
typedef struct LdeVUIInfo
{
    uint32_t flags; /**< Combination of `VUIVideoFormat` that can be inspected for data-validity or sub-flag presence. */

    /* Aspect ratio info. Valid if `VUIAspectRatioInfoPresent` flag is set. */
    uint8_t aspectRatioIdc;
    uint16_t sarWidth;
    uint16_t sarHeight;

    /* Video signal type - Valid if `VUIVideoSignalTypePresent` flag is set. */
    LdeVUIVideoFormat videoFormat;
    uint8_t colourPrimaries;
    uint8_t transferCharacteristics;
    uint8_t matrixCoefficients;

    /* Chroma loc info - Valid if `VUIChromaLocInfoPresent` flag is set. */
    uint32_t chromaSampleLocTypeTopField;
    uint32_t chromaSampleLocTypeBottomField;
} LdeVUIInfo;

/*!
 * \brief LCEVC mastering display colour volume. Seek out the LCEVC standard documentation (D.2) for
 * explanation on these fields and how to use them.
 */
typedef struct LdeMasteringDisplayColorVolume
{
    uint16_t displayPrimariesX[VN_MDCV_NUM_PRIMARIES];
    uint16_t displayPrimariesY[VN_MDCV_NUM_PRIMARIES];
    uint16_t whitePointX;
    uint16_t whitePointY;
    uint32_t maxDisplayMasteringLuminance;
    uint32_t minDisplayMasteringLuminance;
} LdeMasteringDisplayColorVolume;

/*!
 * \brief LCEVC content light level. Seek out the LCEVC standard documentation (D.3) for explanation
 *        on these fields and how to use them.
 */
typedef struct LdeContentLightLevel
{
    uint16_t maxContentLightLevel;
    uint16_t maxPicAverageLightLevel;
} ContentLightLevel;

typedef struct LdeTonemapperConfig
{
    uint8_t type;
    uint32_t dataSize; /**< Valid if `HDRToneMapperDataPresent` flag is set */
    uint8_t* data;     /**< Valid if `HDRToneMapperDataPresent` flag is set */
} LdeTonemapperConfig;

/*!
 * \brief LCEVC HDR info. This contains additional info regarding HDR configuration that may be
 *        signaled in the LCEVC bitstream. Seek out the LCEVC standard documentation Annex D & E for
 *        details.
 */
typedef struct LdeHDRInfo
{
    uint32_t flags; /**< Combination of `lcevc_hdr_flags` that can be inspected for data-validity. */
    LdeMasteringDisplayColorVolume masteringDisplay; /**< Valid if `HDRMasteringDisplayColourVolumePresent` flag is set */
    ContentLightLevel contentLightLevel; /**< Valid if `HDRContentLightLevelInfoPresent` flag is set */
    LdeTonemapperConfig tonemapperConfig[2]; /**< Valid if `HDRPayloadGlobalConfigPresent` flag is set */
} LdeHDRInfo;

#endif // VN_LCEVC_ENHANCEMENT_HDR_TYPES_H
