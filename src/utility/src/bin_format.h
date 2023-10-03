// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// writer for V-Nova internal .bin format.
//
#ifndef VN_LCEVC_UTILITY_BIN_FORMAT_H
#define VN_LCEVC_UTILITY_BIN_FORMAT_H

#include <cstdint>

namespace lcevc_dec::utility {

// LCEVC BIN constants
//
static constexpr char kMagicBytes[8] = {'l', 'c', 'e', 'v', 'c', 'b', 'i', 'n'};
static constexpr uint32_t kVersion = 1;

enum class BlockTypes
{
    LCEVCPayload = 0,
    Extension = 65535,
    Unknown = 65536
};

} // namespace lcevc_dec::utility

#endif
