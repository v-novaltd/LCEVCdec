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

#ifndef VN_LCEVC_ENHANCEMENT_LOG_UTILITIES_H
#define VN_LCEVC_ENHANCEMENT_LOG_UTILITIES_H

#include "config_parser_types.h"

#include <LCEVC/enhancement/bitstream_types.h>

const char* chromaToString(LdeChroma type);

const char* bitdepthToString(LdeBitDepth type);

const char* pictureTypeToString(LdePictureType type);

const char* fieldTypeToString(LdeFieldType type);

const char* upscaleTypeToString(LdeUpscaleType type);

const char* ditherTypeToString(LdeDitherType type);

const char* planesTypeToString(LdePlanesType type);

const char* transformTypeToString(LdeTransformType type);

const char* loqIndexToString(LdeLOQIndex loq);

const char* quantMatrixModeToString(LdeQuantMatrixMode mode);

const char* scalingModeToString(LdeScalingMode mode);

const char* tileDimensionsToString(LdeTileDimensions type);

const char* userDataModeToString(LdeUserDataMode mode);

const char* sharpenTypeToString(LdeSharpenType type);

const char* dequantOffsetModeToString(LdeDequantOffsetMode mode);

const char* nalTypeToString(LdeNALType type);

const char* blockTypeToString(BlockType type);

const char* additionalInfoTypeToString(AdditionalInfoType type);

const char* seiPayloadTypeToString(SEIPayloadType type);

#endif // VN_LCEVC_ENHANCEMENT_LOG_UTILITIES_H
