// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Several utility functions that any sane string library would provide.
//
#include "string_utils.h"

#include <fmt/core.h>

namespace lcevc_dec::utility {

std::vector<std::string> split(std::string_view src, std::string_view seperators)
{
    std::vector<std::string> output;

    // Detect an empty input
    if (src.empty()) {
        return output;
    }

    std::string_view::const_iterator start = src.begin();
    bool inToken = true;

    for (std::string_view::const_iterator i = src.begin(); i != src.end(); ++i) {
        if (seperators.find_first_of(*i) == std::string::npos) {
            // Not a separator char
            inToken = true;
        } else {
            // A separator char
            if (inToken) {
                // End of token - push from start to here to output
                output.emplace_back(start, i);
            }
            start = i + 1;
            inToken = false;
        }
    }

    // Add last token
    output.emplace_back(start, src.end());

    return output;
}

std::string hexDump(const uint8_t* data, uint32_t size, uint32_t offset)
{
    const int bytesPerLine = 16;
    std::string result;

    for (uint64_t line = 0; line < size; line += bytesPerLine) {
        result += fmt::format("{:#06x} : ", offset + line);
        // Bytes
        for (uint64_t byte = 0; byte < bytesPerLine; ++byte) {
            if (line + byte < size) {
                result += fmt::format("{:02x} ", data[line + byte]);
            } else {
                result += "-- ";
            }
        }

        result += " : ";

        // Chars
        for (uint64_t byte = 0; byte < bytesPerLine; ++byte) {
            if (line + byte < size) {
                char chr = static_cast<char>(data[line + byte]);
                result.push_back(isprint(chr) ? chr : '.');
            }
        }

        result.push_back('\n');
    }

    return result;
}

} // namespace lcevc_dec::utility
