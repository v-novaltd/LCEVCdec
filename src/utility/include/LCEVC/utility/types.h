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

// LCEVC type utilities:
//
// - fromString() and toString() functions
// - operator<<() and operator>>() for sue with streams and packages like CLI11 and cxxopts
// - {fmt} custom formatters
//
// For command line argument conversion via CLI11, include "LCEVC/utility/types_cli11.h"
//
#ifndef VN_LCEVC_UTILITY_TYPES_H
#define VN_LCEVC_UTILITY_TYPES_H

// Convert types to and from strings
//
#include <LCEVC/utility/types_convert.h>

// Implementations of operator<<() and operator>>() for use with streams and packages like CLI11 and cxxopts
//
#include <LCEVC/utility/types_stream.h>

// Implementations of {fmt} specific formatters for types
//
#include <LCEVC/utility/types_fmt.h>

#endif // VN_LCEVC_UTILITY_TYPES_H
