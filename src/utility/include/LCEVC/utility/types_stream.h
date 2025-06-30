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

// Declare operator>>() for the LCEVC types
//
// Useful for packages like CLI11 and cxxopts
//
#ifndef VN_LCEVC_UTILITY_TYPES_STREAM_H
#define VN_LCEVC_UTILITY_TYPES_STREAM_H

#include <LCEVC/lcevc_dec.h>

#include <istream>

// These have to be non-templated functions to avoid ambiguous template resolution

std::istream& operator>>(std::istream& in, LCEVC_ReturnCode& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorRange& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorPrimaries& v);
std::istream& operator>>(std::istream& in, LCEVC_TransferCharacteristics& v);
std::istream& operator>>(std::istream& in, LCEVC_PictureFlag& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorFormat& v);
std::istream& operator>>(std::istream& in, LCEVC_Access& v);
std::istream& operator>>(std::istream& in, LCEVC_Event& v);

std::ostream& operator<<(std::ostream& out, const LCEVC_ReturnCode& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorRange& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorPrimaries& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_TransferCharacteristics& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_PictureFlag& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorFormat& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_Access& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_Event& v);

#endif // VN_LCEVC_UTILITY_TYPES_STREAM_H
