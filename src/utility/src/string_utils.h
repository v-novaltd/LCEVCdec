// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Several utility functions that any sane string library would provide.
//
#ifndef VN_LCEVC_UTILITY_STRING_UTILS_H
#define VN_LCEVC_UTILITY_STRING_UTILS_H

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace lcevc_dec::utility {

//! Convert a string to lowercase
//
static inline std::string lowercase(std::string_view str)
{
    std::string r(str.size(), '\0');
    std::transform(str.begin(), str.end(), r.begin(), tolower);
    return r;
}

//! Convert a string to uppercase
//
static inline std::string uppercase(std::string_view str)
{
    std::string r(str.size(), '\0');
    std::transform(str.begin(), str.end(), r.begin(), toupper);
    return r;
}

//! Split a source string into strings where any of the delimiter characters appear
//
// Repeated delimiters are treated as a single delimiter
// A delimiter at start or end of source strings creates any empty string in the output vector.
//
std::vector<std::string> split(std::string_view src, std::string_view seperators);

//! Generate a hex dump from a block of memory
//
std::string hexDump(const uint8_t* data, uint32_t size, uint32_t offset);

} // namespace lcevc_dec::utility
#endif
