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

#include "LCEVC/utility/types_stream.h"

#include "LCEVC/utility/types_convert.h"

#include <fmt/core.h>

using namespace lcevc_dec::utility;

std::istream& operator>>(std::istream& in, LCEVC_ColorFormat& v)
{
    std::string str;
    in >> str;

    if (!fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid ColorFormat: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_ReturnCode& v)
{
    std::string str;
    in >> str;

    if (!fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid ReturnCode: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_ColorRange& v)
{
    std::string str;
    in >> str;

    if (!fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid ColorRange: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_ColorPrimaries& v)
{
    std::string str;
    in >> str;

    if (fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid ColorPrimaries: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_TransferCharacteristics& v)
{
    std::string str;
    in >> str;

    if (fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid ColorTransfer: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_PictureFlag& v)
{
    std::string str;
    in >> str;

    if (fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid PictureFlag: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_Access& v)
{
    std::string str;
    in >> str;

    if (fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid Access: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::istream& operator>>(std::istream& in, LCEVC_Event& v)
{
    std::string str;
    in >> str;

    if (fromString(str.c_str(), v)) {
        fmt::print(stderr, "Not a valid Event: '{}'\n", str);
        std::exit(EXIT_FAILURE);
    }

    return in;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_ReturnCode& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_ColorRange& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_ColorPrimaries& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_TransferCharacteristics& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_PictureFlag& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_ColorFormat& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_Access& v)
{
    out << toString(v);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LCEVC_Event& v)
{
    out << toString(v);
    return out;
}
