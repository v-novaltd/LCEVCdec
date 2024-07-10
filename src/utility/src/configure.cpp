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

#include "LCEVC/utility/configure.h"

#include "LCEVC/lcevc_dec.h"

#include <fmt/core.h>
#include <rapidjson/document.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string_view>
#include <vector>

using namespace rapidjson;

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

// Classify type of a single rapidjson value
ValueType classifyValue(const Value& value)
{
    switch (value.GetType()) {
        default: return ValueType::Unknown;
        case kFalseType:
        case kTrueType: return ValueType::Bool;
        case kStringType: return ValueType::String;
        case kNumberType:
            if (value.IsInt()) {
                return ValueType::Int;
            } else {
                return ValueType::Float;
            }
    }
}

// Classify type of a rapidjson array
ValueType classifyArray(const Value& array)
{
    if (array.Size() == 0) {
        return ValueType::Unknown;
    }

    ValueType type = classifyValue(array[0]);

    for (const auto& v : array.GetArray()) {
        if (classifyValue(v) != type) {
            return ValueType::Unknown;
        }
    }

    return type;
}
} // namespace

namespace lcevc_dec::utility {

LCEVC_ReturnCode configureDecoderFromJson(LCEVC_DecoderHandle decoderHandle, std::string_view json)
{
    if (json.empty()) {
        return LCEVC_Error;
    }

    Document configuration;

    if (json[0] != '{') {
        // File
        std::ifstream jsonFile(json.data());
        if (!jsonFile) {
            return LCEVC_Error;
        }
        std::string str{std::istreambuf_iterator<char>(jsonFile), std::istreambuf_iterator<char>()};
        configuration.Parse(str.c_str());
    } else {
        // Raw json
        configuration.Parse(json.data());
    }

    // Expecting a valid json object
    if (configuration.HasParseError() || !configuration.IsObject()) {
        return LCEVC_Error;
    }

    for (Value::ConstMemberIterator item = configuration.MemberBegin();
         item != configuration.MemberEnd(); ++item) {
        LCEVC_ReturnCode ret = LCEVC_Error;

        if (!item->value.IsArray()) {
            // Scalar value
            switch (classifyValue(item->value)) {
                case ValueType::Int:
                    ret = LCEVC_ConfigureDecoderInt(decoderHandle, item->name.GetString(),
                                                    item->value.GetInt());
                    if (ret == LCEVC_Success) {
                        break;
                    }
                    // Fall through to float case if int configuration fails
                case ValueType::Float:
                    ret = LCEVC_ConfigureDecoderFloat(decoderHandle, item->name.GetString(),
                                                      item->value.GetFloat());
                    break;
                case ValueType::String:
                    ret = LCEVC_ConfigureDecoderString(decoderHandle, item->name.GetString(),
                                                       item->value.GetString());
                    break;
                case ValueType::Bool:
                    ret = LCEVC_ConfigureDecoderBool(decoderHandle, item->name.GetString(),
                                                     item->value.GetBool());
                    break;
                default: break;
            }
        } else {
            // Array of values
            switch (classifyArray(item->value)) {
                case ValueType::Int: {
                    std::vector<int32_t> arr(item->value.Size());
                    std::transform(item->value.GetArray().begin(), item->value.GetArray().end(),
                                   arr.begin(), [](const Value& v) { return v.GetInt(); });
                    ret = LCEVC_ConfigureDecoderIntArray(decoderHandle, item->name.GetString(),
                                                         static_cast<uint32_t>(arr.size()), arr.data());
                    break;
                }
                case ValueType::Float: {
                    std::vector<float> arr(item->value.Size());
                    std::transform(item->value.GetArray().begin(), item->value.GetArray().end(),
                                   arr.begin(), [](const Value& v) { return v.GetFloat(); });
                    ret = LCEVC_ConfigureDecoderFloatArray(decoderHandle, item->name.GetString(),
                                                           static_cast<uint32_t>(arr.size()), arr.data());
                    break;
                }
                case ValueType::String: {
                    std::vector<const char*> arr(item->value.Size());
                    std::transform(item->value.GetArray().begin(), item->value.GetArray().end(),
                                   arr.begin(), [](const Value& v) { return v.GetString(); });
                    ret = LCEVC_ConfigureDecoderStringArray(decoderHandle, item->name.GetString(),
                                                            static_cast<uint32_t>(arr.size()),
                                                            arr.data());
                    break;
                }

                case ValueType::Bool: {
                    // std::vector<bool> is special - can't convert it to a raw array
                    bool* arr = static_cast<bool*>(alloca(item->value.Size() * sizeof(bool)));
                    bool* ptr = arr;
                    for (const auto& v : item->value.GetArray()) {
                        *ptr++ = v.GetBool();
                    }
                    ret = LCEVC_ConfigureDecoderBoolArray(decoderHandle, item->name.GetString(),
                                                          item->value.Size(), arr);
                    break;
                }
                default: break;
            }
        }

        if (ret != LCEVC_NotFound && ret != LCEVC_Success) {
            fmt::print("Unknown parameter '{}'\n", item->name.GetString());
            return ret;
        }
    }

    return LCEVC_Success;
}

} // namespace lcevc_dec::utility
