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

#include "LCEVC/utility/configure.h"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <string_view>
#include <vector>

using json = nlohmann::json;

namespace {
// Configuration value types
enum class ValueType
{
    Unknown,
    Int,
    Float,
    String,
    Bool,
};

// Classify the type of a single json value
ValueType classifyValue(const json::value_t& value)
{
    switch (value) {
        default: return ValueType::Unknown;
        case json::value_t::boolean: return ValueType::Bool;
        case json::value_t::string: return ValueType::String;
        case json::value_t::number_integer:
        case json::value_t::number_unsigned: return ValueType::Int;
        case json::value_t::number_float: return ValueType::Float;
    }
}

// Classify type of a json array
ValueType classifyArray(const json::array_t& array)
{
    if (array.size() == 0) {
        return ValueType::Unknown;
    }

    ValueType type = classifyValue(array[0]);

    for (const auto& v : array) {
        if (classifyValue(v) != type) {
            return ValueType::Unknown;
        }
    }

    return type;
}
} // namespace

namespace lcevc_dec::utility {

LCEVC_ReturnCode configureDecoderFromJson(LCEVC_DecoderHandle decoderHandle, std::string_view jsonStr)
{
    if (jsonStr.empty()) {
        return LCEVC_Error;
    }

    //    Document configuration;

    json configuration;

    if (jsonStr[0] != '{') {
        // File
        std::ifstream jsonFile(jsonStr.data());
        if (!jsonFile) {
            return LCEVC_Error;
        }
        std::string str{std::istreambuf_iterator<char>(jsonFile), std::istreambuf_iterator<char>()};
        try {
            configuration = json::parse(str.c_str());
        } catch (const json::parse_error&) {
            return LCEVC_Error;
        }
    } else {
        // Raw json
        try {
            configuration = json::parse(jsonStr);
        } catch (const json::parse_error&) {
            return LCEVC_Error;
        }
    }

    // Expecting a valid json object
    if (configuration.is_null()) {
        return LCEVC_Error;
    }

    for (auto& item : configuration.items()) {
        LCEVC_ReturnCode ret = LCEVC_Error;

        if (item.value().type() != json::value_t::array) {
            // Scalar value
            switch (classifyValue(item.value())) {
                case ValueType::Int:
                    ret = LCEVC_ConfigureDecoderInt(decoderHandle, item.key().c_str(), item.value());
                    if (ret == LCEVC_Success) {
                        break;
                    }
                    // Fall through to float case if int configuration fails
                case ValueType::Float:
                    ret = LCEVC_ConfigureDecoderFloat(decoderHandle, item.key().c_str(), item.value());
                    break;
                case ValueType::String: {
                    std::string value = configuration[item.key()];
                    ret = LCEVC_ConfigureDecoderString(decoderHandle, item.key().c_str(), value.c_str());
                    break;
                }
                case ValueType::Bool:
                    ret = LCEVC_ConfigureDecoderBool(decoderHandle, item.key().c_str(), item.value());
                    break;
                default: break;
            }
        } else {
            // Array of values
            switch (classifyArray(item.value())) {
                case ValueType::Int: {
                    std::vector<int32_t> arr = item.value();
                    ret = LCEVC_ConfigureDecoderIntArray(decoderHandle, item.key().c_str(),
                                                         static_cast<uint32_t>(arr.size()), arr.data());
                    break;
                }
                case ValueType::Float: {
                    std::vector<float> arr = item.value();
                    ret = LCEVC_ConfigureDecoderFloatArray(decoderHandle, item.key().c_str(),
                                                           static_cast<uint32_t>(arr.size()), arr.data());
                    break;
                }
                case ValueType::String: {
                    const auto vec = item.value().get<std::vector<std::string>>();
                    std::vector<const char*> arr;
                    for (const auto& i : vec) {
                        arr.push_back(i.c_str());
                    }
                    ret = LCEVC_ConfigureDecoderStringArray(decoderHandle, item.key().c_str(),
                                                            static_cast<uint32_t>(item.value().size()),
                                                            arr.data());
                    break;
                }

                case ValueType::Bool: {
                    // std::vector<bool> is special - can't convert it to a raw array
                    bool* arr = static_cast<bool*>(alloca(item.value().size() * sizeof(bool)));
                    bool* ptr = arr;
                    for (const auto& v : item.value()) {
                        *ptr++ = v;
                    }
                    ret = LCEVC_ConfigureDecoderBoolArray(decoderHandle, item.key().c_str(),
                                                          static_cast<uint32_t>(item.value().size()), arr);
                    break;
                }
                default: break;
            }
        }

        if (ret != LCEVC_NotFound && ret != LCEVC_Success) {
            fmt::print("Unknown parameter '{}'\n", item.key());
            return ret;
        }
    }

    return LCEVC_Success;
}

} // namespace lcevc_dec::utility
