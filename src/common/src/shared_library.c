/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

//
#include <LCEVC/common/platform.h>
#include <LCEVC/common/shared_library.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define SHARED_SUFFIX "dll"

LdcSharedLibrary ldcSharedLibraryFind(const char* libraryName)
{
    // If exact name exists - just use that
    HMODULE hModule = LoadLibrary(libraryName);

    if (!hModule) {
        // Try <name>.dll
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s." SHARED_SUFFIX, libraryName);

        hModule = LoadLibrary(tmp);
    }

    return (LdcSharedLibrary)hModule;
}

void ldcSharedLibraryRelease(LdcSharedLibrary sharedLibrary)
{
    FreeLibrary((HMODULE)sharedLibrary);
}

void* ldcSharedLibrarySymbol(LdcSharedLibrary sharedLibrary, const char* symbolName)
{
    return GetProcAddress((HMODULE)sharedLibrary, symbolName);
}

#else
// Linux, Android, macOS
#include <dlfcn.h>

#if VN_OS(APPLE)
#define SHARED_SUFFIX "dylib"
#else
#define SHARED_SUFFIX "so"
#endif

LdcSharedLibrary ldcSharedLibraryFind(const char* libraryName)
{
    // If exact name exists - just use that
    void* handle = dlopen(libraryName, RTLD_LAZY);

    if (!handle) {
        // Try lib<name>.<suffix>
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "lib%s." SHARED_SUFFIX, libraryName);

        handle = dlopen(tmp, RTLD_LAZY);
    }

    return (LdcSharedLibrary)handle;
}

void ldcSharedLibraryRelease(LdcSharedLibrary sharedLibrary) { dlclose((void*)sharedLibrary); }

void* ldcSharedLibrarySymbol(LdcSharedLibrary sharedLibrary, const char* symbolName)
{
    return dlsym((void*)sharedLibrary, symbolName);
}
#endif
