// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// The operator>>() declarations for enum types do not work with CLI11, as it's
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
