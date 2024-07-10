/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "threading.h"

#include "log.h"

#include <sys/stat.h>

#include <string_view>

#ifdef WIN32
#include <direct.h>
#include <io.h>
#include <processthreadsapi.h>
#include <Windows.h>
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

static const LogComponent kComp = LogComponent::Threading;

#if defined(WIN32)
void setThreadName(std::wstring_view name)
{
    if (!name.empty()) {
        // This might not be available on Windows prior to Windows10
        const HRESULT hr = SetThreadDescription(GetCurrentThread(), name.data());
        VNLogTrace("name <%S>, failed? = %d, hr = 0x%X\n", name.data(), FAILED(hr), hr);
    }
}
#else
void setThreadName(std::string_view name)
{
    if (!name.empty()) {
#if defined(__ANDROID__) || defined(__linux__)
        const int res = pthread_setname_np(pthread_self(), name.data());
#else // i.e. Apple
        const int res = pthread_setname_np(name.data());
#endif
        VNLogTrace("name <%S>, result = %d\n", name.data(), res);
    }
}
#endif

} // namespace lcevc_dec::decoder
