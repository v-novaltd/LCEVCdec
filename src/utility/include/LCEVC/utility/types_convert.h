// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// LCEVC type utilities:
//  - to/from strings
//
#ifndef VN_LCEVC_TYPES_CONVERT_H
#define VN_LCEVC_TYPES_CONVERT_H

#include "LCEVC/lcevc_dec.h"

#include <string_view>

namespace lcevc_dec::utility {

// Decoder API enums
//
std::string_view toString(LCEVC_ReturnCode returnCode);
bool fromString(std::string_view str, LCEVC_ReturnCode& out);

std::string_view toString(LCEVC_ColorRange colorRange);
bool fromString(std::string_view str, LCEVC_ColorRange& out);

std::string_view toString(LCEVC_ColorPrimaries colorPrimaries);
bool fromString(std::string_view str, LCEVC_ColorPrimaries& out);

std::string_view toString(LCEVC_TransferCharacteristics transferCharacteristics);
bool fromString(std::string_view str, LCEVC_TransferCharacteristics& out);

std::string_view toString(LCEVC_PictureFlag pictureFlag);
bool fromString(std::string_view str, LCEVC_PictureFlag& out);

std::string_view toString(LCEVC_ColorFormat colorFormat);
bool fromString(std::string_view str, LCEVC_ColorFormat& out);

std::string_view toString(LCEVC_Access access);
bool fromString(std::string_view str, LCEVC_Access& out);

std::string_view toString(LCEVC_Event event);
bool fromString(std::string_view str, LCEVC_Event& out);

} // namespace lcevc_dec::utility

#endif
