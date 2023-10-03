/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#include "uString.h"

#include <algorithm>
#include <cctype>

namespace lcevc_dec::api_utility::string {
namespace {
    static VNInline size_t PathLastSlash(const std::string& path)
    {
        size_t p = path.rfind('/');
#if WIN32
        size_t p1 = path.rfind('\\');
        if ((p1 != std::string::npos) && ((p1 > p) || (p == std::string::npos)))
            p = p1;
#endif
        return p;
    }
} // namespace

bool IEquals(const std::string& a, const std::string& b)
{
    if (a.length() != b.length()) {
        return false;
    }

    const char* aptr = a.c_str();
    const char* bptr = b.c_str();

    while (*aptr && *bptr) {
        if (std::toupper(*aptr) != std::toupper(*bptr)) {
            return false;
        }

        aptr++;
        bptr++;
    }

    return true;
}

const std::string& ToLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), (int (*)(int))std::tolower);
    return str;
}

std::string ToLowerCopy(const std::string& str)
{
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(), (int (*)(int))std::tolower);
    return res;
}

const std::string& ToUpper(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), (int (*)(int))std::toupper);
    return str;
}

std::string ToUpperCopy(const std::string& str)
{
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(), (int (*)(int))std::toupper);
    return res;
}

bool StartsWith(const std::string& str, const std::string& prefix)
{
    if (prefix.length() > str.length())
        return false;

    return std::equal(str.begin(), str.begin() + prefix.length(), prefix.begin());
}

bool EndsWith(const std::string& str, const std::string& suffix)
{
    if (suffix.length() > str.length())
        return false;

    return std::equal(str.rbegin(), str.rbegin() + suffix.length(), suffix.rbegin());
}

std::string PathDirectory(const std::string& path)
{
    size_t p = PathLastSlash(path);

    if (p != std::string::npos)
        return path.substr(0, p + 1);

    return "";
}

std::string PathFile(const std::string& path)
{
    size_t p = PathLastSlash(path);

    if (p == std::string::npos)
        return path;

    return path.substr(p + 1, path.length() - p);
}

std::string PathExtension(const std::string& path)
{
    size_t p = path.rfind('.');
    std::string res;

    if (p != std::string::npos) {
        res = path.substr(p + 1);
    }

    return res;
}

std::string PathFileName(const std::string& path)
{
    size_t p = path.rfind('.');
    return path.substr(0, p);
}

const std::string& PathNormalise(std::string& path, bool bDirectory)
{
    if (path.length() == 0)
        return path;

    // Convert backslashes to forward.
    std::replace(path.begin(), path.end(), '\\', '/');

    // Cap with a slash
    if (bDirectory) {
        if (path[path.length() - 1] != '/') {
            path.append("/");
        }
    }

    return path;
}

std::string PathMakeFullPath(std::string const & path, std::string const & file)
{
    std::string rVal = path;
    PathNormalise(rVal, true);
    rVal.append(file);
    return rVal;
}

std::string PathMakeFullFile(std::string const & name, std::string const & ext)
{
    static const char psep_char = '.';
    std::string pathSep = "";
    if (!name.empty() && (name.back() != psep_char)) {
        pathSep = std::string(1, psep_char);
    }
    return name + pathSep + ext;
}
} // namespace lcevc_dec::api_utility::string