/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#pragma once

#include "uTypes.h"

#include <assert.h>
#include <stdio.h>

#include <cstring>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define VNAlign(x) __declspec(align(x))
#define VNEmptyFile()      \
    namespace {            \
    char NoEmptyfileDummy; \
    }
#define VNThreadLocal __declspec(thread)
#else
#define VNAlign(x) __attribute__((aligned(x)))
#define VNEmptyFile()
#define VNThreadLocal __thread
#endif

#define VNUnused(x) (void)(x)

// Use VNStringify to convert a preprocessor defines value to a string.
#define VNStringify(a) VNStringifyHelper(a)
#define VNStringifyHelper(s) #s

// Use VNConcat to concatenate 2 values and make a new value, (i.e unique name generation based on file/line).
#define VNConcatHelper(a, b) a##b
#define VNConcat(a, b) VNConcatHelper(a, b)

// Use VNAssert to trap runtime assertions.
#define VNAssert(condition) assert((condition))
#define VNAssertf(condition, format, ...) \
    assert((condition))                   // @todo: Actually print this message on failure.
#define VNFail(format, ...) assert(false) // @todo: Actually print this message.

// Use VNStaticAssert for compile time assertions, templates a good place for this.
#if defined(VNPlatformSupportsStaticAssert)
#define VNStaticAssert(condition) static_assert((condition), #condition)
#else
// Emulate static assert by generating invalid code when the condition fails. This will result
// in a less obvious error message. Than the built in language support.
#ifdef __COUNTER__
#define VNStaticAssert(condition)                                                \
    ;                                                                            \
    enum                                                                         \
    {                                                                            \
        VNConcat(static_assert_counter_, __COUNTER__) = 1 / (int)(!!(condition)) \
    }
#else
#define VNStaticAssert(condition)                                          \
    ;                                                                      \
    enum                                                                   \
    {                                                                      \
        VNConcat(static_assert_line_, __LINE__) = 1 / (int)(!!(condition)) \
    }
#endif
#endif

// On MSVC < VS2015, snprintf doesn't exist
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf(DEST, COUNT, FORMAT, ...) sprintf((DEST), (FORMAT), __VA_ARGS__)
#endif

namespace lcevc_dec { namespace utility {
    namespace os {
        std::string GetAppPath();
        std::string GetCwd();
        void setThreadName(std::string name);
    } // namespace os

    namespace lib {
        std::wstring utf8ToUtf16(const std::string& utf8Str);
        std::string utf16ToUtf8(const std::wstring& utf16Str);

        void* Open(const std::string& name, const std::string& version = "", std::string* returnmsg = NULL);
        bool Close(void* handle);
        void* GetSymbol(void* handle, const std::string& name);
        std::string GetError();

        template <typename T>
        VNInline static bool GetFunction(void* libHandle, const char* fnName, T& out,
                                         bool bThrowOnFail = false)
        {
            out = (T)GetSymbol(libHandle, fnName);

            if (out == nullptr && bThrowOnFail) {
                throw std::runtime_error("Failed to find lib function\n");
            }

            return (out != nullptr);
        }
    } // namespace lib

    namespace file {
        uint64_t Tell(FILE* f);
        void Seek(FILE* f, long long offset, int32_t origin);
        uint64_t Size(FILE* f);
        FILE* OpenFileSearched(const std::string& filename, const char* mode);
        bool ReadContentsText(const std::string& filename, std::string& output);
        bool ReadContentsBinary(const std::string& filename, DataBuffer& output);
        bool Exists(const char* path);
        bool Exists(const std::string& path);
        uint64_t GetModifiedTime(const char* path);
        bool IsTerminal(FILE* f);
    } // namespace file
}}    // namespace lcevc_dec::utility
