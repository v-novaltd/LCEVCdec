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

#include <LCEVC/pipeline/types.h>

bool operator==(const LdpPictureDesc& lhs, const LdpPictureDesc& rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.colorFormat == rhs.colorFormat &&
           lhs.colorRange == rhs.colorRange && lhs.colorPrimaries == rhs.colorPrimaries &&
           lhs.matrixCoefficients == rhs.matrixCoefficients &&
           lhs.transferCharacteristics == rhs.transferCharacteristics &&
           lhs.hdrStaticInfo == rhs.hdrStaticInfo &&
           lhs.sampleAspectRatioNum == rhs.sampleAspectRatioNum &&
           lhs.sampleAspectRatioDen == rhs.sampleAspectRatioDen && lhs.cropTop == rhs.cropTop &&
           lhs.cropBottom == rhs.cropBottom && lhs.cropLeft == rhs.cropLeft &&
           lhs.cropRight == rhs.cropRight;
}

bool operator==(const LdpPicturePlaneDesc& lhs, const LdpPicturePlaneDesc& rhs)
{
    return lhs.firstSample == rhs.firstSample && lhs.rowByteStride == rhs.rowByteStride;
}

bool operator==(const LdpPictureBufferDesc& lhs, const LdpPictureBufferDesc& rhs)
{
    return lhs.data == rhs.data && lhs.byteSize == rhs.byteSize &&
           lhs.accelBuffer == rhs.accelBuffer && lhs.access == rhs.access;
}

bool operator==(const LdpHDRStaticInfo& lhs, const LdpHDRStaticInfo& rhs)
{
    return lhs.displayPrimariesX0 == rhs.displayPrimariesX0 &&
           lhs.displayPrimariesY0 == rhs.displayPrimariesY0 &&
           lhs.displayPrimariesX1 == rhs.displayPrimariesX1 &&
           lhs.displayPrimariesY1 == rhs.displayPrimariesY1 &&
           lhs.displayPrimariesX2 == rhs.displayPrimariesX2 &&
           lhs.displayPrimariesY2 == rhs.displayPrimariesY2 && lhs.whitePointX == rhs.whitePointX &&
           lhs.whitePointY == rhs.whitePointY &&
           lhs.maxDisplayMasteringLuminance == rhs.maxDisplayMasteringLuminance &&
           lhs.minDisplayMasteringLuminance == rhs.minDisplayMasteringLuminance &&
           lhs.maxContentLightLevel == rhs.maxContentLightLevel &&
           lhs.maxFrameAverageLightLevel == rhs.maxFrameAverageLightLevel;
}

LdcReturnCode ldpDefaultPictureDesc(LdpPictureDesc* pictureDesc, LdpColorFormat format,
                                    uint32_t width, uint32_t height)
{
    if (pictureDesc == nullptr) {
        return LdcReturnCodeInvalidParam;
    }

    pictureDesc->width = width;
    pictureDesc->height = height;
    pictureDesc->colorFormat = format;
    pictureDesc->colorRange = LdpColorRangeUnknown;
    pictureDesc->colorPrimaries = LdpColorPrimariesUnspecified;
    pictureDesc->matrixCoefficients = LdpMatrixCoefficientsUnspecified;
    pictureDesc->transferCharacteristics = LdpTransferCharacteristicsUnspecified;
    pictureDesc->hdrStaticInfo = {};
    pictureDesc->sampleAspectRatioNum = 1;
    pictureDesc->sampleAspectRatioDen = 1;
    pictureDesc->cropTop = 0;
    pictureDesc->cropBottom = 0;
    pictureDesc->cropLeft = 0;
    pictureDesc->cropRight = 0;

    return LdcReturnCodeSuccess;
}
