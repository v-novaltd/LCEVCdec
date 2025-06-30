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

#ifndef VN_LCEVC_COMMON_STRING_FORMAT_H
#define VN_LCEVC_COMMON_STRING_FORMAT_H

#include <LCEVC/common/diagnostics.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct LdcFormatParser LdcFormatParser;
typedef struct LdcFormatElement LdcFormatElement;

// Persistent state for a printf format parser
//
struct LdcFormatParser
{
    // The format string being parsed
    const char* format;
    // Current point in `format`
    const char* ptr;

    // The current argument index
    size_t argumentIndex;
};

// Describes an extracted element of the format string.
//
struct LdcFormatElement
{
    // Pointer to start of format string element
    // NB: Not null terminated
    const char* start;

    // Length of format string element
    size_t length;

    // Type of data expected by this element
    // `LdcDiagArgNone` is used to represent a section that does not need interpolation.
    LdcDiagArg type;

    // If argumentCount!=0, index of first incoming argument
    size_t argumentIndex;

    // Number of incoming arguments consumed by this element
    size_t argumentCount;
};

// Initialize a printf format parser
//
void ldcFormatParseInitialise(LdcFormatParser* parser, const char* format);

// Get next element from a printf format parser
//
// Returns true when there are no more valid elements.
bool ldcFormatParseNext(LdcFormatParser* parser, LdcFormatElement* element);

//
size_t ldcFormat(char* dst, uint32_t dstSize, const char* format, const LdcDiagArg* argumentTypes,
                 const LdcDiagValue* argumentvalues, size_t argumentCount);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_STRING_FORMAT_H
