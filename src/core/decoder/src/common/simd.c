/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

#include "common/simd.h"

#if VN_CORE_FEATURE(NEON)
#include "common/neon.h"
#elif VN_CORE_FEATURE(SSE)
#include "common/sse.h"
#endif

#include "common/types.h"
#include "lcevc_config.h"

#include <stdbool.h>
#include <stdint.h>

/* @todo(bob): Drive this not on compile time enable features flags, but target architecture (new repo will have this). */

/*-----------------------------------------------------------------------------*/

#if VN_ARCH(WASM)

static CPUAccelerationFeatures_t detectX86Features(void) { return CAFSSE; }

#elif (VN_CORE_FEATURE(SSE) || VN_CORE_FEATURE(AVX2))

#if (!VN_COMPILER(MSVC))
#include <cpuid.h>
#endif

/*! \brief Helper function for loading CPUID. */
static void loadCPUInfo(int32_t cpuInfo[4], int32_t field)
{
#if VN_OS(WINDOWS)
    __cpuid(cpuInfo, field);
#elif (VN_OS(LINUX) || VN_OS(ANDROID)) && !VN_OS(BROWSER)
    __cpuid_count(field, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#elif VN_OS(MACOS) || VN_OS(IOS) || VN_OS(TVOS)
    __asm__ __volatile__("xchg %%ebx, %k[tempreg]\n\t"
                         "cpuid\n\t"
                         "xchg %%ebx, %k[tempreg]\n"
                         : "=a"(cpuInfo[0]), [tempreg] "=&r"(cpuInfo[1]), "=c"(cpuInfo[2]),
                           "=d"(cpuInfo[3])
                         : "a"(field), "c"(0));
#elif !VN_OS(BROWSER)
#error "Unsupported platform for CPUID"
#endif
}

static CPUAccelerationFeatures_t detectSSEFeature(const int32_t cpuInfo[4])
{
    static const int32_t kSSEFlag = 1 << 20;
    return ((cpuInfo[2] & kSSEFlag) == kSSEFlag) ? CAFSSE : CAFNone;
}

static CPUAccelerationFeatures_t detectAVX2Feature(const int32_t cpuInfo[4])
{
    /* Note: clang has sse in version 8, but not xsave functions, so need >=9 .*/
#if VN_COMPILER(GCC) && (__GNUC__ >= 9) || VN_COMPILER(CLANG) && (__clang_major__ >= 9) || \
    VN_COMPILER(MSVC)
    static const int32_t kAVX2Flag = 1 << 5;
    static const int32_t kXSaveFlag = 1 << 27;

    bool hasXSaveSupport = false;

    /* Check for XSAVE support on the OS. */
    if ((cpuInfo[2] & kXSaveFlag) == kXSaveFlag) {
        static const uint32_t kOSXSaveMask = 6;
        static const uint32_t kControlRegister = 0; /* _XCR_XFEATURE_ENABLED_MASK */
        hasXSaveSupport = ((_xgetbv(kControlRegister) & kOSXSaveMask) == kOSXSaveMask);
    }

    /* Must have xsave feature and OS must support it. */
    if (!hasXSaveSupport) {
        return CAFNone;
    }

    /* Load processor extended feature bits. */
    int32_t info[4];
    loadCPUInfo(info, 7);

    return ((info[1] & kAVX2Flag) == kAVX2Flag) ? CAFAVX2 : CAFNone;
#else
    /* vndk does not define _xgetbv */
    return CAFNone;
#endif
}

static CPUAccelerationFeatures_t detectX86Features(void)
{
    int32_t cpuInfo[4];

    /* Function 0: Processor Info and Feature Bits */
    loadCPUInfo(cpuInfo, 0);

    const int32_t nids = cpuInfo[0];

    if (nids < 1) {
        return CAFNone;
    }

    /* Detect individual processor features. */
    loadCPUInfo(cpuInfo, 1);

    CPUAccelerationFeatures_t res = detectSSEFeature(cpuInfo);

    if (nids >= 7) {
        res |= detectAVX2Feature(cpuInfo);
    }

    return res;
}

#else

static CPUAccelerationFeatures_t detectX86Features(void) { return CAFNone; }

#endif /* VN_CORE_FEATURE(SSE) || VN_CORE_FEATURE(AVX2) */

#if VN_CORE_FEATURE(NEON)

static CPUAccelerationFeatures_t detectARMFeatures(void) { return CAFNEON; }

#else

static CPUAccelerationFeatures_t detectARMFeatures(void) { return CAFNone; }

#endif /* VN_CORE_FEATURE(NEON) */

/*-----------------------------------------------------------------------------*/

CPUAccelerationFeatures_t detectSupportedSIMDFeatures(void)
{
    return detectX86Features() | detectARMFeatures();
}

/*-----------------------------------------------------------------------------*/
