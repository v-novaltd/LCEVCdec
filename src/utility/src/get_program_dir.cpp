// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/get_program_dir.h"

#include "filesystem.h"

#include <climits>

namespace lcevc_dec::utility {

#if VN_OS(WINDOWS)
#include <Windows.h>
std::string getExecutablePath()
{
    // Get executable name
    //
    char tmp[MAX_PATH];
    int s = GetModuleFileName(NULL, tmp, sizeof(tmp));

    return filesystem::canonical(tmp).string();
}
#endif

#if VN_OS(LINUX) || VN_OS(ANDROID)
#include <unistd.h>

std::string getExecutablePath()
{
    char tmp[PATH_MAX] = "";

    // Use /proc & readlink() to get real executable path
    char link[PATH_MAX] = {};
    snprintf(link, PATH_MAX, "/proc/%d/exe", getpid());
    ssize_t size = readlink(link, tmp, sizeof(tmp));

    if (size < 0) {
        return "";
    }

    return filesystem::canonical(tmp);
}
#endif

#if VN_OS(MACOS)
#include <mach-o/dyld.h>
#include <sys/syslimits.h>

std::string getExecutablePath()
{
    char tmp[PATH_MAX];
    uint32_t bufsize = sizeof(tmp);

    _NSGetExecutablePath(tmp, &bufsize);

    return filesystem::canonical(tmp);
}
#endif

// Get directory part of current executable path - optionally add a filename
//
std::string getProgramDirectory(std::string_view file)
{
    filesystem::path path(getExecutablePath());

    if (file.empty()) {
        return path.remove_filename().string();
    }

    return (path.remove_filename() / file).string();
}

} // namespace lcevc_dec::utility