// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// LCEVC type utilities:
//
// - fromString() and toString() finctions
// - operator<<() and operator>>() for sue with streams and packages like CLI11 and cxxopts
// - {fmt} custom formatters
//
// For command line argument conversion via CLI11, include "LCEVC/utility/types_cli11.h"
//
#ifndef VN_LCEVC_UTILITY_TYPES_H
#define VN_LCEVC_UTILITY_TYPES_H

// Convert types to and from strings
//
#include "LCEVC/utility/types_convert.h"

// Implementaitons of operator<<() and operator>>() for use with streams and packages like CLI11 and cxxopts
//
#include "LCEVC/utility/types_stream.h"

// Implementations of {fmt} specific formatters for types
//
#include "LCEVC/utility/types_fmt.h"

#endif
