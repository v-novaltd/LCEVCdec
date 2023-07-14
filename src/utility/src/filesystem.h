// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Wrapper to select std::filesystem of ghc_filesystem
//
#ifndef VN_LCEVC_UTILITY_FILESYSTEM_H
#define VN_LCEVC_UTILITY_FILESYSTEM_H

#include "lcevc_config.h"

#if VN_SDK_FEATURE(USE_GHC_FILESYSTEM)
#include <ghc/filesystem.hpp>
namespace filesystem = ghc::filesystem;
#else
#include <filesystem>
namespace filesystem = std::filesystem;
#endif

#endif
