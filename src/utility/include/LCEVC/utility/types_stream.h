// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Declare operator>>() for the LCEVC types
//
// Useful for packages like CLI11 and cxxopts
//
#ifndef VN_LCEVC_UTILITY_TYPES_STREAM_H
#define VN_LCEVC_UTILITY_TYPES_STREAM_H

#include "LCEVC/lcevc_dec.h"

#include <istream>

// These have to be non-templated functions to avoid ambiguous template resolution

std::istream& operator>>(std::istream& in, LCEVC_ReturnCode& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorRange& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorStandard& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorTransfer& v);
std::istream& operator>>(std::istream& in, LCEVC_PictureFlag& v);
std::istream& operator>>(std::istream& in, LCEVC_ColorFormat& v);
std::istream& operator>>(std::istream& in, LCEVC_Access& v);
std::istream& operator>>(std::istream& in, LCEVC_Event& v);

std::ostream& operator<<(std::ostream& out, const LCEVC_ReturnCode& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorRange& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorStandard& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorTransfer& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_PictureFlag& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_ColorFormat& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_Access& v);
std::ostream& operator<<(std::ostream& out, const LCEVC_Event& v);

#endif
