// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Parse a raw video filename to extract any metadata (compatible with Vooya parsing)
//
#ifndef VN_LCEVC_UTILITY_PARSE_RAW_NAME_H
#define VN_LCEVC_UTILITY_PARSE_RAW_NAME_H

#include "LCEVC/lcevc_dec.h"

#include <string_view>

namespace lcevc_dec::utility {

LCEVC_PictureDesc parseRawName(std::string_view name);
LCEVC_PictureDesc parseRawName(std::string_view name, float& rate);

} // namespace lcevc_dec::utility

#endif
