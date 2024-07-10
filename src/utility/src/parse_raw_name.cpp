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

// Parse a raw video filename to extract any metadata (compatible with Vooya parsing)
//
#include "parse_raw_name.h"

#include "LCEVC/utility/check.h"
#include "LCEVC/utility/string_utils.h"

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
    {"p420", 14, LCEVC_I420_14_LE}, {"p420", 16, LCEVC_I420_16_LE}, {"p422", 8, LCEVC_I422_8},
    {"p422", 10, LCEVC_I422_10_LE}, {"p422", 12, LCEVC_I422_12_LE}, {"p422", 14, LCEVC_I422_14_LE},
    {"p422", 16, LCEVC_I422_16_LE}, {"p444", 8, LCEVC_I444_8},      {"p444", 10, LCEVC_I444_10_LE},
    {"p444", 12, LCEVC_I444_12_LE}, {"p444", 14, LCEVC_I444_14_LE}, {"p444", 16, LCEVC_I444_16_LE},
    {"y", 8, LCEVC_GRAY_8},         {"y", 10, LCEVC_GRAY_10_LE},    {"y", 12, LCEVC_GRAY_12_LE},
    {"y", 14, LCEVC_GRAY_14_LE},    {"y", 16, LCEVC_GRAY_16_LE},    {"nv12", 8, LCEVC_NV12_8},
    {"nv21", 8, LCEVC_NV21_8},      {"rgb", 8, LCEVC_RGB_8},        {"bgr", 8, LCEVC_BGR_8},
    {"rgba", 8, LCEVC_RGBA_8},      {"bgra", 8, LCEVC_BGRA_8},      {"argb", 8, LCEVC_ARGB_8},
    {"abgr", 8, LCEVC_ABGR_8},
};

// Parse filename for picture description
LCEVC_PictureDesc parseRawName(std::string_view name, float& rate)
{
    // Regular expression for parts of name
    static const std::regex kDimensionsRe("([0-9]+)x([0-9]+)(p[0-9]+)?"); // Size (sometimes followed by fps)
    static const std::regex kFpsRe("([0-9]+)(fps|hz)");                   // Hz
    static const std::regex kBitsRe("([0-9]+)(bits?|bpp)");               // Bit depth
    static const std::regex kFormatP420Re("(420|p420|420p|yuv)"); // YUV420 formats (420 is default yuv format)
    static const std::regex kFormatP422Re("(422|p422|422p)"); // YUV422 formats
    static const std::regex kFormatP444Re("(444|p444|444p)"); // YUV444 formats
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

        // Get color format if not yet deduced. That's colourspace, chroma subsampling, and
        // interleaving:
        if (!format.empty()) {
            continue;
        }

        if (regex_match(lp.c_str(), match, kFormatP420Re)) {
            format = "p420";
        }
        if (regex_match(lp.c_str(), match, kFormatP422Re)) {
            format = "p422";
        }
        if (regex_match(lp.c_str(), match, kFormatP444Re)) {
            format = "p444";
        }
        if (regex_match(lp.c_str(), match, kFormatOtherRe)) {
            format = match[1].str();
        }
    }

    // Convert format name and bitdepth to a specific LCEVC_ColorFormat
    LCEVC_ColorFormat colorFormat = LCEVC_ColorFormat_Unknown;
    for (const auto& pf : kPictureFormats) {
        if (format == pf.name && bits == pf.bits) {
            colorFormat = pf.format;
            break;
        }
    }

    LCEVC_PictureDesc pictureDescription{};
    if (colorFormat == LCEVC_ColorFormat_Unknown) {
        fmt::print("parse_raw_name: Couldn't deduce format from filename\n");
        return pictureDescription;
    }

    VN_LCEVC_CHECK(LCEVC_DefaultPictureDesc(&pictureDescription, colorFormat, width, height));

    return pictureDescription;
}

LCEVC_PictureDesc parseRawName(std::string_view name)
{
    float dummyRate = 0.0f;
    return parseRawName(name, dummyRate);
}

} // namespace lcevc_dec::utility
