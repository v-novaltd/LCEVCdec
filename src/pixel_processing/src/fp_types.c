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

#include "fp_types.h"

#include <LCEVC/common/limit.h>

FixedPointPromotionFunction fixedPointGetPromotionFunction(LdpFixedPoint unsignedFP)
{
    switch (unsignedFP) {
        case LdpFPU10: return &fpU10ToS10;
        case LdpFPU12: return &fpU12ToS12;
        case LdpFPU14: return &fpU14ToS14;
        default: break;
    }

    return NULL;
}

FixedPointDemotionFunction fixedPointGetDemotionFunction(LdpFixedPoint unsignedFP)
{
    switch (unsignedFP) {
        case LdpFPU10: return &fpS10ToU10;
        case LdpFPU12: return &fpS12ToU12;
        case LdpFPU14: return &fpS14ToU14;
        default: break;
    }

    return NULL;
}

int32_t fixedPointMaxValue(LdpFixedPoint fp)
{
    switch (fp) {
        case LdpFPU8: return 255;
        case LdpFPU10: return 1023;
        case LdpFPU12: return 4095;
        case LdpFPU14: return 16383;
        case LdpFPS8:
        case LdpFPS10:
        case LdpFPS12:
        case LdpFPS14: return 32767;
        default: break;
    }
    return 0;
}

int32_t fixedPointHighlightValue(LdpFixedPoint fp)
{
    switch (fp) {
        case LdpFPU8: return 255;
        case LdpFPU10: return 1023;
        case LdpFPU12: return 4095;
        case LdpFPU14: return 16383;
        case LdpFPS8: return fpU8ToS8(255);
        case LdpFPS10: return fpU10ToS10(1023);
        case LdpFPS12: return fpU12ToS12(4095);
        case LdpFPS14: return fpU14ToS14(16383);
        default: break;
    }
    return 0;
}

int32_t fixedPointOffset(LdpFixedPoint fp)
{
    switch (fp) {
        case LdpFPS8:
        case LdpFPS10:
        case LdpFPS12:
        case LdpFPS14: return 16384;
        default: break;
    }
    return 0;
}

bool fixedPointIsSigned(LdpFixedPoint type)
{
    assert(fixedPointIsValid(type));

    switch (type) {
        case LdpFPU8:
        case LdpFPU10:
        case LdpFPU12:
        case LdpFPU14: return false;
        case LdpFPS8:
        case LdpFPS10:
        case LdpFPS12:
        case LdpFPS14: return true;
        default: break;
    }

    return false;
}

const char* fixedPointToString(LdpFixedPoint type)
{
    switch (type) {
        case LdpFPU8: return "U8";
        case LdpFPU10: return "U10";
        case LdpFPU12: return "U12";
        case LdpFPU14: return "U14";
        case LdpFPS8: return "S8_7";
        case LdpFPS10: return "S10_5";
        case LdpFPS12: return "S12_3";
        case LdpFPS14: return "S14_1";
        default: break;
    }

    assert(false);
    return "unknown";
}

bool fixedPointIsValid(LdpFixedPoint type) { return (uint32_t)type < LdpFPCount; }

uint32_t bitdepthFromFixedPoint(LdpFixedPoint type)
{
    assert(fixedPointIsValid(type));

    switch (type) {
        case LdpFPU8:
        case LdpFPS8: return 8;
        case LdpFPU10:
        case LdpFPS10: return 10;
        case LdpFPU12:
        case LdpFPS12: return 12;
        case LdpFPU14:
        case LdpFPS14: return 14;
        default: break;
    }
    return 8;
}

uint32_t fixedPointByteSize(LdpFixedPoint type)
{
    assert(fixedPointIsValid(type));

    switch (type) {
        case LdpFPU8: return sizeof(uint8_t);
        case LdpFPU10:
        case LdpFPU12:
        case LdpFPU14:
        case LdpFPS8:
        case LdpFPS10:
        case LdpFPS12:
        case LdpFPS14: return sizeof(int16_t);
        default: break;
    }

    return 0;
}

LdpFixedPoint fixedPointLowPrecision(LdpFixedPoint type)
{
    switch (type) {
        case LdpFPS8: return LdpFPU8;
        case LdpFPS10: return LdpFPU10;
        case LdpFPS12: return LdpFPU12;
        case LdpFPS14: return LdpFPU14;
        default: break;
    }

    return type;
}

LdpFixedPoint fixedPointHighPrecision(LdpFixedPoint type)
{
    switch (type) {
        case LdpFPU8: return LdpFPS8;
        case LdpFPU10: return LdpFPS10;
        case LdpFPU12: return LdpFPS12;
        case LdpFPU14: return LdpFPS14;
        default: break;
    }
    return type;
}

bool isPow2(uint32_t value) { return (value & (value - 1)) == 0; }

uint32_t alignTruncU32(uint32_t value, uint32_t alignment)
{
    assert(alignment > 0);
    assert(isPow2(alignment));
    return value & ~(alignment - 1);
}
