/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "LCEVC/utility/get_program_dir.h"

#include <climits>
#include <filesystem>

namespace filesystem = std::filesystem;
namespace lcevc_dec::utility {

#if VN_OS(WINDOWS)
#include <windows.h>
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
#if !VN_OS(IOS) && !VN_OS(TVOS)
std::string getProgramDirectory(std::string_view file)
{
    filesystem::path path(getExecutablePath());

    if (file.empty()) {
        return path.remove_filename().string();
    }

    return (path.remove_filename() / file).string();
}
#endif

} // namespace lcevc_dec::utility
