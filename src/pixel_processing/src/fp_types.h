/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_FP_TYPES_H
#define VN_LCEVC_PIXEL_PROCESSING_FP_TYPES_H

#include <LCEVC/pipeline/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef int16_t (*FixedPointPromotionFunction)(uint16_t);
typedef uint16_t (*FixedPointDemotionFunction)(int32_t);

FixedPointPromotionFunction fixedPointGetPromotionFunction(LdpFixedPoint unsignedFP);
FixedPointDemotionFunction fixedPointGetDemotionFunction(LdpFixedPoint unsignedFP);

int32_t fixedPointMaxValue(LdpFixedPoint fp);
int32_t fixedPointHighlightValue(LdpFixedPoint fp);
int32_t fixedPointOffset(LdpFixedPoint fp);
bool fixedPointIsSigned(LdpFixedPoint type);
const char* fixedPointToString(LdpFixedPoint type);
bool fixedPointIsValid(LdpFixedPoint type);
uint32_t bitdepthFromFixedPoint(LdpFixedPoint type);
uint32_t fixedPointByteSize(LdpFixedPoint type);
LdpFixedPoint fixedPointLowPrecision(LdpFixedPoint type);
LdpFixedPoint fixedPointHighPrecision(LdpFixedPoint type);
bool isPow2(uint32_t value);
uint32_t alignTruncU32(uint32_t value, uint32_t alignment);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_PIXEL_PROCESSING_FP_TYPES_H
