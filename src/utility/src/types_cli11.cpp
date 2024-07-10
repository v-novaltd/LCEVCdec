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

// The operator>>() declarations for enum types do not work with CLI11, as its
// own specialisation of lexical_cast for enum overrides them.
//
#include "LCEVC/utility/types_convert.h"
#include "LCEVC/utility/types_stream.h"

#include <fmt/core.h>

using namespace lcevc_dec::utility;

bool lexical_cast(const std::string& input, LCEVC_ColorFormat& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid ColorFormat: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_ReturnCode& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid ReturnCode: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_ColorRange& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid ColorRange: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_ColorPrimaries& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid ColorPrimaries: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_TransferCharacteristics& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid ColorTransfer: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_PictureFlag& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid PictureFlag: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_Access& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid ColorFormat: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

bool lexical_cast(const std::string& input, LCEVC_Event& v) // NOLINT
{
    if (!fromString(input, v)) {
        fmt::print(stderr, "Not a valid Event: '{}'\n", input);
        std::exit(EXIT_FAILURE);
    }

    return true;
}
