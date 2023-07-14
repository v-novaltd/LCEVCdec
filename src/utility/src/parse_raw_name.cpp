// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Parse a raw video filename to extract any metadata (compatible with Vooya parsing)
//
#include "parse_raw_name.h"

#include "LCEVC/utility/check.h"
#include "string_utils.h"

#include <fmt/core.h>

#include <regex>

namespace lcevc_dec::utility {

// Maps format names and bit depth to LCEVC_ColorFormat
static const struct
{
    const char* name;
    unsigned bits;
    LCEVC_ColorFormat format;
} kPictureFormats[] = {
    {"p420", 8, LCEVC_I420_8},      {"p420", 10, LCEVC_I420_10_LE}, {"p420", 12, LCEVC_I420_12_LE},
    {"p420", 14, LCEVC_I420_14_LE}, {"p420", 16, LCEVC_I420_16_LE}, {"y", 8, LCEVC_GRAY_8},
    {"y", 10, LCEVC_GRAY_10_LE},    {"y", 12, LCEVC_GRAY_12_LE},    {"y", 14, LCEVC_GRAY_14_LE},
    {"y", 16, LCEVC_GRAY_16_LE},    {"nv12", 8, LCEVC_NV12_8},      {"nv21", 8, LCEVC_NV21_8},
    {"rgb", 8, LCEVC_RGB_8},        {"bgr", 8, LCEVC_BGR_8},        {"rgba", 8, LCEVC_RGBA_8},
    {"bgra", 8, LCEVC_BGRA_8},      {"argb", 8, LCEVC_ARGB_8},      {"abgr", 8, LCEVC_ABGR_8},
};

// Parse filename for picture description
LCEVC_PictureDesc parseRawName(std::string_view name, float& rate)
{
    // Regular expression for parts of name
    static const std::regex kDimensionsRe("([0-9]+)x([0-9]+)");   // Size
    static const std::regex kFpsRe("([0-9]+)(fps|hz)");           // Hz
    static const std::regex kBitsRe("([0-9]+)(bits?|bpp)");       // Bit depth
    static const std::regex kFormatP420Re("(420|p420|420p|yuv)"); // YUV420 formats
    static const std::regex kFormatOtherRe("(y|yuyv|rgb|bgr|rgba|argb|abgr|bgra|nv12|nv21)"); // Other formats

    std::string format;
    unsigned bits = 8;
    unsigned width = 0;
    unsigned height = 0;

    for (auto const & part : split(name, ("-_."))) {
        std::cmatch match;
        const std::string lp = lowercase(part);

        if (regex_match(lp.c_str(), match, kDimensionsRe)) {
            width = stoi(match[1]);
            height = stoi(match[2]);
        }
        if (regex_match(lp.c_str(), match, kFpsRe)) {
            rate = stof(match[1].str());
        }
        if (regex_match(lp.c_str(), match, kBitsRe)) {
            bits = stoi(match[1].str());
        }
        if (regex_match(lp.c_str(), match, kFormatP420Re)) {
            if (format.empty()) {
                format = "p420";
            }
        }
        if (regex_match(lp.c_str(), match, kFormatOtherRe)) {
            if (format.empty()) {
                format = match[1].str();
            }
        }
    }

    // Convert format name and bitdepth to a specific LCEVC_ColorFormat
    LCEVC_ColorFormat colorFormat = LCEVC_ColorFormat_Unknown;
    for (const auto& pf : kPictureFormats) {
        if (format == pf.name && bits == pf.bits) {
            colorFormat = pf.format;
        }
    }

    LCEVC_PictureDesc pictureDescription{};
    VN_LCEVC_CHECK(LCEVC_DefaultPictureDesc(&pictureDescription, colorFormat, width, height));

    return pictureDescription;
}

LCEVC_PictureDesc parseRawName(std::string_view name)
{
    float dummyRate = 0.0f;
    return parseRawName(name, dummyRate);
}

} // namespace lcevc_dec::utility
