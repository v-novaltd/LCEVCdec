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

// This checks that the API enuma and structs match the pipeline ones as appropriate.
//
// Whilst they are pretty much identical at the moment - the internal types will likely
// evolve to be a superset of the API ones. THis keeps the common elements the same.
//

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
//
#include "utils.h"
//
#include <gtest/gtest.h>
#include <handle.h>
#include <pool.h>
//
#include <cstddef>
#include <future>

using namespace lcevc_dec::decoder;
using namespace lcevc_dec::pipeline;

TEST(PipelineTypes, EnumAccess)
{
    ASSERT_EQ(LdpAccessUnknown, LCEVC_Access_Unknown);
    ASSERT_EQ(LdpAccessRead, LCEVC_Access_Read);
    ASSERT_EQ(LdpAccessModify, LCEVC_Access_Modify);
    ASSERT_EQ(LdpAccessWrite, LCEVC_Access_Write);
}

TEST(PipelineTypes, EnumColorRange)
{
    ASSERT_EQ(LdpColorRangeUnknown, LCEVC_ColorRange_Unknown);
    ASSERT_EQ(LdpColorRangeFull, LCEVC_ColorRange_Full);
    ASSERT_EQ(LdpColorRangeLimited, LCEVC_ColorRange_Limited);
}

TEST(PipelineTypes, EnumColorFormat)
{
    ASSERT_EQ(LdpColorFormatUnknown, LCEVC_ColorFormat_Unknown);

    ASSERT_EQ(LdpColorFormatI420_8, LCEVC_I420_8);
    ASSERT_EQ(LdpColorFormatI420_10_LE, LCEVC_I420_10_LE);
    ASSERT_EQ(LdpColorFormatI420_12_LE, LCEVC_I420_12_LE);
    ASSERT_EQ(LdpColorFormatI420_14_LE, LCEVC_I420_14_LE);
    ASSERT_EQ(LdpColorFormatI420_16_LE, LCEVC_I420_16_LE);

    ASSERT_EQ(LdpColorFormatI422_8, LCEVC_I422_8);
    ASSERT_EQ(LdpColorFormatI422_10_LE, LCEVC_I422_10_LE);
    ASSERT_EQ(LdpColorFormatI422_12_LE, LCEVC_I422_12_LE);
    ASSERT_EQ(LdpColorFormatI422_14_LE, LCEVC_I422_14_LE);
    ASSERT_EQ(LdpColorFormatI422_16_LE, LCEVC_I422_16_LE);

    ASSERT_EQ(LdpColorFormatI444_8, LCEVC_I444_8);
    ASSERT_EQ(LdpColorFormatI444_10_LE, LCEVC_I444_10_LE);
    ASSERT_EQ(LdpColorFormatI444_12_LE, LCEVC_I444_12_LE);
    ASSERT_EQ(LdpColorFormatI444_14_LE, LCEVC_I444_14_LE);
    ASSERT_EQ(LdpColorFormatI444_16_LE, LCEVC_I444_16_LE);

    ASSERT_EQ(LdpColorFormatNV12_8, LCEVC_NV12_8);
    ASSERT_EQ(LdpColorFormatNV21_8, LCEVC_NV21_8);

    ASSERT_EQ(LdpColorFormatRGB_8, LCEVC_RGB_8);
    ASSERT_EQ(LdpColorFormatBGR_8, LCEVC_BGR_8);
    ASSERT_EQ(LdpColorFormatRGBA_8, LCEVC_RGBA_8);
    ASSERT_EQ(LdpColorFormatBGRA_8, LCEVC_BGRA_8);
    ASSERT_EQ(LdpColorFormatARGB_8, LCEVC_ARGB_8);
    ASSERT_EQ(LdpColorFormatABGR_8, LCEVC_ABGR_8);

    ASSERT_EQ(LdpColorFormatRGBA_10_2_LE, LCEVC_RGBA_10_2_LE);

    ASSERT_EQ(LdpColorFormatGRAY_8, LCEVC_GRAY_8);
    ASSERT_EQ(LdpColorFormatGRAY_10_LE, LCEVC_GRAY_10_LE);
    ASSERT_EQ(LdpColorFormatGRAY_12_LE, LCEVC_GRAY_12_LE);
    ASSERT_EQ(LdpColorFormatGRAY_14_LE, LCEVC_GRAY_14_LE);
    ASSERT_EQ(LdpColorFormatGRAY_16_LE, LCEVC_GRAY_16_LE);
}

TEST(PipelineTypes, EnumColorPrimaries)
{
    ASSERT_EQ(LdpColorPrimariesReserved0, LCEVC_ColorPrimaries_Reserved_0);
    ASSERT_EQ(LdpColorPrimariesBT709, LCEVC_ColorPrimaries_BT709);
    ASSERT_EQ(LdpColorPrimariesUnspecified, LCEVC_ColorPrimaries_Unspecified);
    ASSERT_EQ(LdpColorPrimariesReserved3, LCEVC_ColorPrimaries_Reserved_3);
    ASSERT_EQ(LdpColorPrimariesBT470M, LCEVC_ColorPrimaries_BT470_M);
    ASSERT_EQ(LdpColorPrimariesBT470BG, LCEVC_ColorPrimaries_BT470_BG);
    ASSERT_EQ(LdpColorPrimariesBT601_NTSC, LCEVC_ColorPrimaries_BT601_NTSC);
    ASSERT_EQ(LdpColorPrimariesSMPTE240, LCEVC_ColorPrimaries_SMPTE240);
    ASSERT_EQ(LdpColorPrimariesGENERIC_FILM, LCEVC_ColorPrimaries_GENERIC_FILM);
    ASSERT_EQ(LdpColorPrimariesBT2020, LCEVC_ColorPrimaries_BT2020);
    ASSERT_EQ(LdpColorPrimariesXYZ, LCEVC_ColorPrimaries_XYZ);
    ASSERT_EQ(LdpColorPrimariesSMPTE431, LCEVC_ColorPrimaries_SMPTE431);
    ASSERT_EQ(LdpColorPrimariesSMPTE432, LCEVC_ColorPrimaries_SMPTE432);
    ASSERT_EQ(LdpColorPrimariesReserved13, LCEVC_ColorPrimaries_Reserved_13);
    ASSERT_EQ(LdpColorPrimariesReserved14, LCEVC_ColorPrimaries_Reserved_14);
    ASSERT_EQ(LdpColorPrimariesReserved15, LCEVC_ColorPrimaries_Reserved_15);
    ASSERT_EQ(LdpColorPrimariesReserved16, LCEVC_ColorPrimaries_Reserved_16);
    ASSERT_EQ(LdpColorPrimariesReserved17, LCEVC_ColorPrimaries_Reserved_17);
    ASSERT_EQ(LdpColorPrimariesReserved18, LCEVC_ColorPrimaries_Reserved_18);
    ASSERT_EQ(LdpColorPrimariesReserved19, LCEVC_ColorPrimaries_Reserved_19);
    ASSERT_EQ(LdpColorPrimariesReserved20, LCEVC_ColorPrimaries_Reserved_20);
    ASSERT_EQ(LdpColorPrimariesReserved21, LCEVC_ColorPrimaries_Reserved_21);
    ASSERT_EQ(LdpColorPrimariesP22, LCEVC_ColorPrimaries_P22);
}

TEST(PipelineTypes, EnumTransferCharacteristics)
{
    ASSERT_EQ(LdpTransferCharacteristicsReserved0, LCEVC_TransferCharacteristics_Reserved_0);
    ASSERT_EQ(LdpTransferCharacteristicsBT709, LCEVC_TransferCharacteristics_BT709);
    ASSERT_EQ(LdpTransferCharacteristicsUnspecified, LCEVC_TransferCharacteristics_Unspecified);
    ASSERT_EQ(LdpTransferCharacteristicsReserved3, LCEVC_TransferCharacteristics_Reserved_3);
    ASSERT_EQ(LdpTransferCharacteristicsGAMMA22, LCEVC_TransferCharacteristics_GAMMA22);
    ASSERT_EQ(LdpTransferCharacteristicsGAMMA28, LCEVC_TransferCharacteristics_GAMMA28);
    ASSERT_EQ(LdpTransferCharacteristicsBT601, LCEVC_TransferCharacteristics_BT601);
    ASSERT_EQ(LdpTransferCharacteristicsSMPTE240, LCEVC_TransferCharacteristics_SMPTE240);
    ASSERT_EQ(LdpTransferCharacteristicsLINEAR, LCEVC_TransferCharacteristics_LINEAR);
    ASSERT_EQ(LdpTransferCharacteristicsLOG100, LCEVC_TransferCharacteristics_LOG100);
    ASSERT_EQ(LdpTransferCharacteristicsLOG100_SQRT10, LCEVC_TransferCharacteristics_LOG100_SQRT10);
    ASSERT_EQ(LdpTransferCharacteristicsIEC61966, LCEVC_TransferCharacteristics_IEC61966);
    ASSERT_EQ(LdpTransferCharacteristicsBT1361, LCEVC_TransferCharacteristics_BT1361);
    ASSERT_EQ(LdpTransferCharacteristicsSRGBSYCC, LCEVC_TransferCharacteristics_SRGB_SYCC);
    ASSERT_EQ(LdpTransferCharacteristicsBT2020_10BIT, LCEVC_TransferCharacteristics_BT2020_10BIT);
    ASSERT_EQ(LdpTransferCharacteristicsBT2020_12BIT, LCEVC_TransferCharacteristics_BT2020_12BIT);
    ASSERT_EQ(LdpTransferCharacteristicsPQ, LCEVC_TransferCharacteristics_PQ);
    ASSERT_EQ(LdpTransferCharacteristicsSMPTE428, LCEVC_TransferCharacteristics_SMPTE428);
    ASSERT_EQ(LdpTransferCharacteristicsHLG, LCEVC_TransferCharacteristics_HLG);
};

TEST(PipelineTypes, EnumMatrixCoefficients)
{
    ASSERT_EQ(LdpMatrixCoefficientsIDENTITY, LCEVC_MatrixCoefficients_IDENTITY);
    ASSERT_EQ(LdpMatrixCoefficientsBT709, LCEVC_MatrixCoefficients_BT709);
    ASSERT_EQ(LdpMatrixCoefficientsUnspecified, LCEVC_MatrixCoefficients_Unspecified);
    ASSERT_EQ(LdpMatrixCoefficientsReserved3, LCEVC_MatrixCoefficients_Reserved_3);
    ASSERT_EQ(LdpMatrixCoefficientsUSFCC, LCEVC_MatrixCoefficients_USFCC);
    ASSERT_EQ(LdpMatrixCoefficientsBT470_BG, LCEVC_MatrixCoefficients_BT470_BG);
    ASSERT_EQ(LdpMatrixCoefficientsBT601_NTSC, LCEVC_MatrixCoefficients_BT601_NTSC);
    ASSERT_EQ(LdpMatrixCoefficientsSMPTE240, LCEVC_MatrixCoefficients_SMPTE240);
    ASSERT_EQ(LdpMatrixCoefficientsYCGCO, LCEVC_MatrixCoefficients_YCGCO);
    ASSERT_EQ(LdpMatrixCoefficientsBT2020_NCL, LCEVC_MatrixCoefficients_BT2020_NCL);
    ASSERT_EQ(LdpMatrixCoefficientsBT2020_CL, LCEVC_MatrixCoefficients_BT2020_CL);
    ASSERT_EQ(LdpMatrixCoefficientsSMPTE2085, LCEVC_MatrixCoefficients_SMPTE2085);
    ASSERT_EQ(LdpMatrixCoefficientsCHROMATICITY_NCL, LCEVC_MatrixCoefficients_CHROMATICITY_NCL);
    ASSERT_EQ(LdpMatrixCoefficientsCHROMATICITY_CL, LCEVC_MatrixCoefficients_CHROMATICITY_CL);
    ASSERT_EQ(LdpMatrixCoefficientsICTCP, LCEVC_MatrixCoefficients_ICTCP);
}

TEST(PipelineTypes, StructDecodeInformation)
{
    ASSERT_EQ(offsetof(LdpDecodeInformation, timestamp), offsetof(LCEVC_DecodeInformation, timestamp));
    ASSERT_EQ(offsetof(LdpDecodeInformation, hasBase), offsetof(LCEVC_DecodeInformation, hasBase));
    ASSERT_EQ(offsetof(LdpDecodeInformation, hasEnhancement),
              offsetof(LCEVC_DecodeInformation, hasEnhancement));
    ASSERT_EQ(offsetof(LdpDecodeInformation, skipped), offsetof(LCEVC_DecodeInformation, skipped));
    ASSERT_EQ(offsetof(LdpDecodeInformation, enhanced), offsetof(LCEVC_DecodeInformation, enhanced));
    ASSERT_EQ(offsetof(LdpDecodeInformation, baseWidth), offsetof(LCEVC_DecodeInformation, baseWidth));
    ASSERT_EQ(offsetof(LdpDecodeInformation, baseHeight), offsetof(LCEVC_DecodeInformation, baseHeight));
    ASSERT_EQ(offsetof(LdpDecodeInformation, baseBitdepth),
              offsetof(LCEVC_DecodeInformation, baseBitdepth));
    ASSERT_EQ(offsetof(LdpDecodeInformation, userData), offsetof(LCEVC_DecodeInformation, baseUserData));
}

TEST(PipelineTypes, StructPictureBufferDesc)
{
    ASSERT_EQ(offsetof(LdpPictureBufferDesc, data), offsetof(LCEVC_PictureBufferDesc, data));
    ASSERT_EQ(offsetof(LdpPictureBufferDesc, byteSize), offsetof(LCEVC_PictureBufferDesc, byteSize));
    ASSERT_EQ(offsetof(LdpPictureBufferDesc, accelBuffer), offsetof(LCEVC_PictureBufferDesc, accelBuffer));
    ASSERT_EQ(offsetof(LdpPictureBufferDesc, access), offsetof(LCEVC_PictureBufferDesc, access));
}

TEST(PipelineTypes, StructPicturePlaneDesc)
{
    ASSERT_EQ(offsetof(LdpPicturePlaneDesc, firstSample), offsetof(LCEVC_PicturePlaneDesc, firstSample));
    ASSERT_EQ(offsetof(LdpPicturePlaneDesc, rowByteStride),
              offsetof(LCEVC_PicturePlaneDesc, rowByteStride));
}

TEST(PipelineTypes, StructHDRStaticInfo)
{
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, displayPrimariesX0),
              offsetof(LCEVC_HDRStaticInfo, displayPrimariesX0));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, displayPrimariesY0),
              offsetof(LCEVC_HDRStaticInfo, displayPrimariesY0));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, displayPrimariesX1),
              offsetof(LCEVC_HDRStaticInfo, displayPrimariesX1));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, displayPrimariesY1),
              offsetof(LCEVC_HDRStaticInfo, displayPrimariesY1));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, displayPrimariesX2),
              offsetof(LCEVC_HDRStaticInfo, displayPrimariesX2));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, displayPrimariesY2),
              offsetof(LCEVC_HDRStaticInfo, displayPrimariesY2));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, whitePointX), offsetof(LCEVC_HDRStaticInfo, whitePointX));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, whitePointY), offsetof(LCEVC_HDRStaticInfo, whitePointY));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, maxDisplayMasteringLuminance),
              offsetof(LCEVC_HDRStaticInfo, maxDisplayMasteringLuminance));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, minDisplayMasteringLuminance),
              offsetof(LCEVC_HDRStaticInfo, minDisplayMasteringLuminance));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, maxContentLightLevel),
              offsetof(LCEVC_HDRStaticInfo, maxContentLightLevel));
    ASSERT_EQ(offsetof(LdpHDRStaticInfo, maxFrameAverageLightLevel),
              offsetof(LCEVC_HDRStaticInfo, maxFrameAverageLightLevel));
}

TEST(PipelineTypes, StructPictureDesc)
{
    ASSERT_EQ(offsetof(LdpPictureDesc, width), offsetof(LCEVC_PictureDesc, width));
    ASSERT_EQ(offsetof(LdpPictureDesc, height), offsetof(LCEVC_PictureDesc, height));
    ASSERT_EQ(offsetof(LdpPictureDesc, colorFormat), offsetof(LCEVC_PictureDesc, colorFormat));
    ASSERT_EQ(offsetof(LdpPictureDesc, colorRange), offsetof(LCEVC_PictureDesc, colorRange));
    ASSERT_EQ(offsetof(LdpPictureDesc, colorPrimaries), offsetof(LCEVC_PictureDesc, colorPrimaries));
    ASSERT_EQ(offsetof(LdpPictureDesc, matrixCoefficients),
              offsetof(LCEVC_PictureDesc, matrixCoefficients));
    ASSERT_EQ(offsetof(LdpPictureDesc, transferCharacteristics),
              offsetof(LCEVC_PictureDesc, transferCharacteristics));
    ASSERT_EQ(offsetof(LdpPictureDesc, hdrStaticInfo), offsetof(LCEVC_PictureDesc, hdrStaticInfo));
    ASSERT_EQ(offsetof(LdpPictureDesc, sampleAspectRatioNum),
              offsetof(LCEVC_PictureDesc, sampleAspectRatioNum));
    ASSERT_EQ(offsetof(LdpPictureDesc, sampleAspectRatioDen),
              offsetof(LCEVC_PictureDesc, sampleAspectRatioDen));
    ASSERT_EQ(offsetof(LdpPictureDesc, cropTop), offsetof(LCEVC_PictureDesc, cropTop));
    ASSERT_EQ(offsetof(LdpPictureDesc, cropBottom), offsetof(LCEVC_PictureDesc, cropBottom));
    ASSERT_EQ(offsetof(LdpPictureDesc, cropLeft), offsetof(LCEVC_PictureDesc, cropLeft));
    ASSERT_EQ(offsetof(LdpPictureDesc, cropRight), offsetof(LCEVC_PictureDesc, cropRight));
}
