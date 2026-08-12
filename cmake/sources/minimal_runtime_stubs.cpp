/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

/* Minimal C++ runtime stubs, compiled directly into each shared library only when
 * VN_SDK_MINIMUM_LIB_SIZE is set (see cmake/modules/CMakeSetup.cmake).
 *
 * The NDK's prebuilt libc++/libc++abi are built WITH exceptions and RTTI, so the
 * internal throwing paths of std:: types reference the C++ exception machinery and,
 * below it, the stack unwinder. This SDK is built -fno-exceptions -fno-rtti and never
 * catches anything, so an exception that is thrown is already fatal. Providing strong
 * definitions for the entry points below pre-empts the linker from extracting two
 * large, otherwise-dead chunks from every shared object:
 *
 *   1. cxa_demangle.o (~100 KB) - the symbol demangler, reached only by libc++abi's
 *      default terminate handler. __cxa_demangle below keeps it out.
 *
 *   2. libunwind.a (~20 KB) - the stack unwinder. Per the linker's --why-extract
 *      output, the only external reference into it is _Unwind_RaiseException (from
 *      cxa_exception.o); everything else in libunwind is referenced only internally.
 *      Defining the complete public _Unwind_* ABI keeps every libunwind member out.
 *      We deliberately do NOT redefine any __cxa_ or __gxx_ symbol: the small
 *      cxa_exception.o / cxa_personality.o stay linked (their catch-side symbols are
 *      co-located, so stubbing them piecemeal causes duplicate-symbol errors), and
 *      they simply resolve their _Unwind_* references to the stubs below.
 *
 * None of the _Unwind_* routines is ever executed: a throw reaches __cxa_throw ->
 * _Unwind_RaiseException, which aborts before any unwinding begins.
 *
 * NOTE: this file must be compiled as a direct object of each shared library (not via
 * a static archive), otherwise it would itself be subject to archive extraction and
 * would not pre-empt the prebuilt members.
 */

#include <cstddef>
#include <cstdlib>

// These intentionally define reserved C++ runtime / unwinder symbols by their exact
// ABI names.
// NOLINTBEGIN(bugprone-reserved-identifier, readability-identifier-naming)
extern "C"
{
/* -- Symbol demangler ----------------------------------------------------------- */

char* __cxa_demangle(const char* /*mangledName*/, char* /*buf*/, size_t* /*n*/, int* status)
{
    if (status != nullptr) {
        *status = -1; /* demangle failure */
    }
    return nullptr;
}

/* -- Unwinder (libunwind) public ABI -------------------------------------------- */
/* LP64 (arm64, x86_64) only. On 32-bit ARM the EHABI unwinder object (Unwind-EHABI.o)
 * is force-linked regardless of these definitions, so providing the _Unwind_* symbols
 * here just produces duplicate-symbol errors rather than keeping libunwind out - on that
 * target we leave the unwinder in place. The demangler stub above still applies on all
 * architectures. unsigned long == uintptr_t == _Unwind_Word on the LP64 targets below;
 * signatures only need to satisfy the linker (C linkage, never called). */
#if defined(__LP64__)

using UnwindReasonCode = int;

/* Routines that begin an unwind: reaching one means an exception was actually
 * thrown, which is already fatal under -fno-exceptions. */
UnwindReasonCode _Unwind_RaiseException(void*) { std::abort(); }
void _Unwind_Resume(void*) { std::abort(); }
UnwindReasonCode _Unwind_Resume_or_Rethrow(void*) { std::abort(); }
UnwindReasonCode _Unwind_ForcedUnwind(void*, void*, void*) { std::abort(); }
void _Unwind_DeleteException(void*) { std::abort(); }

/* Personality-routine helpers: only invoked while an unwind is in progress, which
 * never happens here. Abort defensively. */
unsigned long _Unwind_GetGR(void*, int) { std::abort(); }
void _Unwind_SetGR(void*, int, unsigned long) { std::abort(); }
unsigned long _Unwind_GetIP(void*) { std::abort(); }
unsigned long _Unwind_GetIPInfo(void*, int*) { std::abort(); }
void _Unwind_SetIP(void*, unsigned long) { std::abort(); }
unsigned long _Unwind_GetLanguageSpecificData(void*) { std::abort(); }
unsigned long _Unwind_GetRegionStart(void*) { std::abort(); }
unsigned long _Unwind_GetDataRelBase(void*) { std::abort(); }
unsigned long _Unwind_GetTextRelBase(void*) { std::abort(); }
unsigned long _Unwind_GetCFA(void*) { std::abort(); }

/* Utility queries that can legitimately be called outside of unwinding. Not
 * referenced in this build, but kept benign rather than aborting, just in case. */
UnwindReasonCode _Unwind_Backtrace(void*, void*) { return 5 /* _URC_END_OF_STACK */; }
void* _Unwind_FindEnclosingFunction(void* pc) { return pc; }
void* _Unwind_Find_FDE(void*, void*) { return nullptr; }
#endif /* __LP64__ */

} /* extern "C" */
// NOLINTEND(bugprone-reserved-identifier, readability-identifier-naming)
