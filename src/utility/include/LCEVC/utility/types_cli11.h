/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Support for LCEVC_DEC types via CLI11.
//
// NB: This is separate from types.h to avoid forcing inclusion of <CLI/CLI.hpp>
//
#ifndef VN_LCEVC_UTILITY_TYPES_CLI11_H
#define VN_LCEVC_UTILITY_TYPES_CLI11_H

#include "LCEVC/lcevc_dec.h"

#include <CLI/CLI.hpp>

// NB: The operator>>() declarations for enum types do not work with CLI11, as its
// own specialisation of lexical_cast for enum overrides them, so this works at that level.
//
// These have to be non-templated functions to avoid ambiguous template resolution
//
bool lexical_cast(const std::string& input, LCEVC_ColorFormat& v);
bool lexical_cast(const std::string& input, LCEVC_ReturnCode& v);
bool lexical_cast(const std::string& input, LCEVC_ColorRange& v);
bool lexical_cast(const std::string& input, LCEVC_ColorPrimaries& v);
bool lexical_cast(const std::string& input, LCEVC_TransferCharacteristics& v);
bool lexical_cast(const std::string& input, LCEVC_PictureFlag& v);
bool lexical_cast(const std::string& input, LCEVC_Access& v);
bool lexical_cast(const std::string& input, LCEVC_Event& v);

namespace CLI::detail {
template <>
constexpr const char* type_name<LCEVC_ColorFormat>()
{
    return "COLORFORMAT";
}

template <>
constexpr const char* type_name<LCEVC_ReturnCode>()
{
    return "RETURNCODE";
}

template <>
constexpr const char* type_name<LCEVC_ColorRange>()
{
    return "COLORRANGE";
}

template <>
constexpr const char* type_name<LCEVC_ColorPrimaries>()
{
    return "COLORPRIMARIES";
}

template <>
constexpr const char* type_name<LCEVC_TransferCharacteristics>()
{
    return "COLORTRANSFER";
}

template <>
constexpr const char* type_name<LCEVC_PictureFlag>()
{
    return "PICTUREFLAG";
}

template <>
constexpr const char* type_name<LCEVC_Access>()
{
    return "ACCESS";
}

template <>
constexpr const char* type_name<LCEVC_Event>()
{
    return "EVENT";
}

} // namespace CLI::detail

#endif
