/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#pragma once

#include <string>

namespace lcevc_dec::api_utility::string {
bool IEquals(const std::string& a, const std::string& b);
const std::string& ToLower(std::string& str);
std::string ToLowerCopy(const std::string& str);
const std::string& ToUpper(std::string& str);
std::string ToUpperCopy(const std::string& str);
bool StartsWith(const std::string& str, const std::string& prefix);
bool EndsWith(const std::string& str, const std::string& suffix);
std::string PathDirectory(const std::string& path);
std::string PathFile(const std::string& path);
std::string PathExtension(const std::string& path);
std::string PathFileName(const std::string& path);
const std::string& PathNormalise(std::string& path, bool bDirectory);
std::string PathMakeFullPath(std::string const & path, std::string const & file);
std::string PathMakeFullFile(std::string const & name, std::string const & ext);
} // namespace lcevc_dec::api_utility::string