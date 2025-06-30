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

#include "log_utilities.h"

const char* chromaToString(LdeChroma type)
{
    switch (type) {
        case CTMonochrome: return "monochrome";
        case CT420: return "yuv420p";
        case CT422: return "yuv422p";
        case CT444: return "yuv444p";
        case CTCount: break;
    }

    return "Unknown";
}

const char* bitdepthToString(LdeBitDepth type)
{
    switch (type) {
        case Depth8: return "8-bit";
        case Depth10: return "10-bit";
        case Depth12: return "12-bit";
        case Depth14: return "14-bit";
        case DepthCount: break;
    }

    return "Unknown";
}

const char* pictureTypeToString(LdePictureType type)
{
    switch (type) {
        case PTFrame: return "frame";
        case PTField: return "field";
    }

    return "Unknown";
}

const char* fieldTypeToString(LdeFieldType type)
{
    switch (type) {
        case FTTop: return "top";
        case FTBottom: return "bottom";
    }

    return "Unknown";
}

const char* upscaleTypeToString(LdeUpscaleType type)
{
    switch (type) {
        case USNearest: return "nearest";
        case USLinear: return "linear";
        case USCubic: return "cubic";
        case USModifiedCubic: return "modifiedcubic";
        case USAdaptiveCubic: return "adaptivecubic";
        case USReserved1: return "reserved1";
        case USReserved2: return "reserved2";
        case USUnspecified: return "unspecified";
        case USCubicPrediction: return "cubicprediction";
        case USMishus: return "mishus";
        case USLanczos: return "lanczos";
    }

    return "Unknown";
}

const char* ditherTypeToString(LdeDitherType type)
{
    switch (type) {
        case DTNone: return "none";
        case DTUniform: return "uniform";
    }

    return "Unknown";
}

const char* planesTypeToString(LdePlanesType type)
{
    switch (type) {
        case PlanesY: return "y";
        case PlanesYUV: return "yuv";
    }

    return "Unknown";
}

const char* transformTypeToString(LdeTransformType type)
{
    switch (type) {
        case TransformDD: return "DD";
        case TransformDDS: return "DDS";
        case TransformCount: break;
    }

    return "Unknown";
}

const char* loqIndexToString(LdeLOQIndex loq)
{
    switch (loq) {
        case LOQ0: return "LOQ-0";
        case LOQ1: return "LOQ-1";
        case LOQ2: return "LOQ-2";
        default:;
    }

    return "Unknown";
}

const char* quantMatrixModeToString(LdeQuantMatrixMode mode)
{
    switch (mode) {
        case QMMUsePrevious: return "use_previous";
        case QMMUseDefault: return "use_default";
        case QMMCustomBoth: return "custom_both_same";
        case QMMCustomLOQ0: return "custom_loq0";
        case QMMCustomLOQ1: return "custom_loq1";
        case QMMCustomBothUnique: return "custom_both_unique";
    }

    return "Unknown";
}

const char* scalingModeToString(LdeScalingMode mode)
{
    switch (mode) {
        case Scale0D: return "0D";
        case Scale1D: return "1D";
        case Scale2D: return "2D";
    }

    return "Unknown";
}

const char* tileDimensionsToString(LdeTileDimensions type)
{
    switch (type) {
        case TDTNone: return "none";
        case TDT512x256: return "512x256";
        case TDT1024x512: return "1024x512";
        case TDTCustom: return "custom";
    }

    return "Unknown";
}

const char* userDataModeToString(LdeUserDataMode mode)
{
    switch (mode) {
        case UDMNone: return "none";
        case UDMWith2Bits: return "2-bits";
        case UDMWith6Bits: return "6-bits";
    }

    return "Unknown";
}

const char* sharpenTypeToString(LdeSharpenType type)
{
    switch (type) {
        case STDisabled: return "disabled";
        case STInLoop: return "in_loop";
        case STOutOfLoop: return "out_of_loop";
    }

    return "Unknown";
}

const char* dequantOffsetModeToString(LdeDequantOffsetMode mode)
{
    switch (mode) {
        case DQMDefault: return "default";
        case DQMConstOffset: return "const_offset";
    }

    return "Unknown";
}

const char* nalTypeToString(LdeNALType type)
{
    switch (type) {
        case NTError: return "error";
        case NTIDR: return "IDR";
        case NTNonIDR: return "NonIDR";
    }

    return "unknown";
}

const char* blockTypeToString(BlockType type)
{
    switch (type) {
        case BT_SequenceConfig: return "sequence_config";
        case BT_GlobalConfig: return "global_config";
        case BT_PictureConfig: return "picture_config";
        case BT_EncodedData: return "encoded_data";
        case BT_EncodedDataTiled: return "encoded_data_tiled";
        case BT_AdditionalInfo: return "additional_info";
        case BT_Filler: return "filler";
        case BT_Count: break;
    }

    return "Unknown";
}

const char* additionalInfoTypeToString(AdditionalInfoType type)
{
    switch (type) {
        case AIT_SEI: return "sei";
        case AIT_VUI: return "vui";
        case AIT_SFilter: return "s_filter";
        case AIT_HDR: return "hdr";
    }

    return "Unknown";
}

const char* seiPayloadTypeToString(SEIPayloadType type)
{
    switch (type) {
        case SPT_MasteringDisplayColourVolume: return "mastering_display_colour_volume";
        case SPT_ContentLightLevelInfo: return "content_light_level_info";
        case SPT_UserDataRegistered: return "user_data_registered";
    }

    return "Unknown";
}
