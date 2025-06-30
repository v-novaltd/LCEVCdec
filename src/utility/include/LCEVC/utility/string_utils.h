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

// Several utility functions that any sane string library would provide.
//
#ifndef VN_LCEVC_UTILITY_STRING_UTILS_H
#define VN_LCEVC_UTILITY_STRING_UTILS_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::utility {

//! Case-insensitive equals operator (available in boost but not STL)
//!
bool iEquals(std::string_view a, std::string_view b);

//! Check if the string starts with prefix. Available from STL in C++20
//!
bool startsWith(std::string_view str, std::string_view prefix);

//! Check if the string ends with suffix. Available from STL in C++20
//!
bool endsWith(std::string_view str, std::string_view suffix);

//! Convert a string to lowercase
//!
inline std::string lowercase(std::string_view str)
{
    std::string r(str.size(), '\0');
    std::transform(str.begin(), str.end(), r.begin(), tolower);
    return r;
}

//! Convert a string to uppercase
//!
inline std::string uppercase(std::string_view str)
{
    std::string r(str.size(), '\0');
    std::transform(str.begin(), str.end(), r.begin(), toupper);
    return r;
}

//! Split a source string into strings where any of the delimiter characters appear.
//! Repeated delimiters are treated as a single delimiter.
//! A delimiter at start or end of source strings creates any empty string in the output vector.
//!
std::vector<std::string> split(std::string_view src, std::string_view separators);

//! Generate a hex dump from a block of memory
//!
std::string hexDump(const uint8_t* data, uint32_t size, uint32_t offset, bool humanReadable = true);

} // namespace lcevc_dec::utility
#endif // VN_LCEVC_UTILITY_STRING_UTILS_H
