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

#include <LCEVC/build_config.h>
#include <LCEVC/common/acceleration.h>
//
#include <assert.h>

static LdcAcceleration defaultAcceleration = {0};
static const LdcAcceleration* currentAcceleration = &defaultAcceleration;

#if (VN_SDK_FEATURE(SSE) || VN_SDK_FEATURE(AVX2)) && !VN_ARCH(WASM)
#include <stdint.h>
#if !VN_COMPILER(MSVC)
#include <cpuid.h>
#endif

/*! \brief Helper function for loading CPUID. */
static void loadCPUInfo(int32_t cpuInfo[4], int32_t field)
{
#if VN_COMPILER(MSVC)
    __cpuid(cpuInfo, field);
#elif VN_OS(LINUX) || VN_OS(ANDROID)
    __cpuid_count(field, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#elif VN_OS(APPLE)
    __asm__ __volatile__("xchg %%ebx, %k[tempreg]\n\t"
                         "cpuid\n\t"
                         "xchg %%ebx, %k[tempreg]\n"
                         : "=a"(cpuInfo[0]), [tempreg] "=&r"(cpuInfo[1]), "=c"(cpuInfo[2]),
                           "=d"(cpuInfo[3])
                         : "a"(field), "c"(0));
#elif VN_COMPILER(CLANG) || VN_COMPILER(GCC)
    __cpuid_count(field, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
}

#if VN_SDK_FEATURE(SSE)
/*! \brief Returns true if SSE is detected at runtime. */
static bool detectSSE(void)
{
    int32_t cpuInfo[4];
    loadCPUInfo(cpuInfo, 0);
    if (cpuInfo[0] < 1) {
        return false;
    }
    loadCPUInfo(cpuInfo, 1);

    static const int32_t kSSEFlag = 1 << 20;
    return (cpuInfo[2] & kSSEFlag) == kSSEFlag;
}
#endif

#if VN_SDK_FEATURE(AVX2)
/*! \brief Returns true if AVX2 is detected at runtime. */
static bool detectAVX2(void)
{
    int32_t cpuInfo[4];
    loadCPUInfo(cpuInfo, 0);
    if (cpuInfo[0] < 7) {
        return false;
    }
    loadCPUInfo(cpuInfo, 1);

    // Note: clang has sse in version 8, but not xsave functions, so need >=9
#if VN_COMPILER(GCC) && (__GNUC__ >= 9) || VN_COMPILER(CLANG) && (__clang_major__ >= 9) || \
    VN_COMPILER(MSVC)
    static const int32_t kAVX2Flag = 1 << 5;
    static const int32_t kXSaveFlag = 1 << 27;

    /* Check for XSAVE support on the CPU. */
    if ((cpuInfo[2] & kXSaveFlag) != kXSaveFlag) {
        return false;
    }

    /* Load processor extended feature bits. */
    int32_t info[4];
    loadCPUInfo(info, 7);

    /* Check if CPU supports AVX2. */
    if ((info[1] & kAVX2Flag) != kAVX2Flag) {
        return false;
    }

    return true;
#else
    return false;
#endif
}
#endif
#elif VN_ARCH(WASM)
static bool detectSSE(void) { return true; }
static bool detectAVX2(void) { return false; }
#endif

void ldcAccelerationInitialize(bool enable)
{
    if (enable) {
#if VN_SDK_FEATURE(SSE)
        defaultAcceleration.SSE = detectSSE();
#else
        defaultAcceleration.SSE = false;
#endif
#if VN_SDK_FEATURE(AVX2)
        defaultAcceleration.AVX2 = detectAVX2();
#else
        defaultAcceleration.AVX2 = false;
#endif
#if VN_SDK_FEATURE(NEON)
        defaultAcceleration.NEON = true;
#else
        defaultAcceleration.NEON = false;
#endif
    } else {
        defaultAcceleration.SSE = false;
        defaultAcceleration.AVX2 = false;
        defaultAcceleration.NEON = false;
    }

    currentAcceleration = &defaultAcceleration;
}

void ldcAccelerationSet(const LdcAcceleration* acceleration)
{
    assert(acceleration);
    currentAcceleration = acceleration;
}

const LdcAcceleration* ldcAccelerationGet(void) { return currentAcceleration; }
