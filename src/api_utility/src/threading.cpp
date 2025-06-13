/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <LCEVC/api_utility/threading.h>
#include <sys/stat.h>

#include <string_view>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <processthreadsapi.h>
#include <windows.h>
#define stat _stat
#else
#ifdef __ANDROID__
#include <android/dlext.h>
#endif
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#endif

namespace lcevc_dec::decoder {

#ifdef __MINGW32__
bool setThreadName(const char* name)
{
    int bufsize = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
    WCHAR buf[bufsize];
    bufsize = MultiByteToWideChar(CP_UTF8, 0, name, -1, buf, bufsize);
    const HRESULT hr = SetThreadDescription(GetCurrentThread(), buf);
    if (FAILED(hr)) {
        return false;
    }
    return true;
}
#elif VN_COMPILER(MSVC)
bool setThreadName(std::wstring_view name)
{
    if (!name.empty()) {
        // This might not be available on Windows prior to Windows10
        const HRESULT hr = SetThreadDescription(GetCurrentThread(), name.data());
        if (FAILED(hr)) {
            return false;
        }
    }
    return true;
}
#else
bool setThreadName(std::string_view name)
{
    if (!name.empty()) {
#if defined(__ANDROID__) || defined(__linux__)
        const int res = pthread_setname_np(pthread_self(), name.data());
#else // i.e. Apple
        const int res = pthread_setname_np(name.data());
#endif
        if (res != 0) {
            return false;
        }
    }
    return true;
}
#endif

} // namespace lcevc_dec::decoder
