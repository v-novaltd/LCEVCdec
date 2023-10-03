/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#include "uPlatform.h"

#include "uLog.h"
#include "uString.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#ifdef WIN32
#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <processthreadsapi.h>
#define stat _stat
#else
#ifdef __ANDROID__
#include <android/dlext.h>
#include <pthread.h>
#endif
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#endif

namespace lcevc_dec::api_utility {
namespace {
    static const uint32_t kBufferSize = 1024;
} // namespace

namespace os {
#if defined(WIN32)
    void setThreadName(const std::wstring& name)
    {
        if (!name.empty()) {
            // This might not be available on Windows prior to Windows10
            HRESULT hr = SetThreadDescription(GetCurrentThread(), name.c_str());
            VNLogVerbose("name <%S> failed %d, hr = 0x%X\n", name.c_str(), FAILED(hr), hr);
        }
    }
#elif defined(__ANDROID__) || defined(__linux__)
    void setThreadName(const std::string& name)
    {
        if (!name.empty()) {
            pthread_setname_np(pthread_self(), name.c_str());
        }
    }
#elif defined(__APPLE__)
    void setThreadName(const std::string& name)
    {
        if (!name.empty()) {
            pthread_setname_np(name.c_str());
        }
    }
#endif

    std::string GetAppPath()
    {
        std::string res;
#ifdef WIN32
        char buf[kBufferSize];
        if (GetModuleFileName(nullptr, buf, kBufferSize)) {
            res = buf;
            res = string::PathDirectory(res);
        }
#else
        char buf1[kBufferSize];
        char buf2[kBufferSize];

        snprintf(buf1, sizeof(buf1), "/proc/%d/exe", getpid());
        int r = static_cast<int>(readlink(buf1, buf2, kBufferSize));

        if (r != -1 && r < static_cast<int>(kBufferSize)) {
            buf2[r] = 0;
            res = buf2;
            res = string::PathDirectory(res);
        }
#endif
        return string::PathNormalise(res, true);
    }

    std::string GetCwd()
    {
        std::string res;
#ifdef WIN32
        char buf[kBufferSize];
        if (_getcwd(buf, kBufferSize)) {
            res = buf;
        }
#else
        char buf[kBufferSize];
        if (getcwd(buf, kBufferSize)) {
            res = buf;
        }
#endif
        return string::PathNormalise(res, true);
    }
} // namespace os

namespace lib {
#ifdef WIN32
    void* Open(const std::string& name, const std::string& version, std::string* returnmsg)
    {
        const std::string extension = ".dll";
        std::string filename = name;
        void* ret = NULL;

        if (!string::EndsWith(name, extension))
            filename += extension;

        ret = LoadLibraryA(filename.c_str());
        if (ret == NULL) {
            if (returnmsg != NULL) {
                *returnmsg = GetError();
            }

            // Try to open libfoo-ver.dll
            filename = name + "-" + version;
            if (!string::EndsWith(filename, extension))
                filename += extension;
            ret = LoadLibraryA(filename.c_str());
            if ((ret != NULL) && (returnmsg != NULL)) {
                // libfoo-ver.dll worked, clear error message
                *returnmsg = "";
            }
        }

        return ret;
    }

    bool Close(void* handle)
    {
        if (handle)
            return FreeLibrary(static_cast<HMODULE>(handle)) != 0;

        return true;
    }

    void* GetSymbol(void* handle, const std::string& name)
    {
        if (handle)
            return GetProcAddress(static_cast<HMODULE>(handle), name.c_str());

        return nullptr;
    }

    std::string GetError()
    {
        std::string err;
        DWORD dw = GetLastError();
        LPVOID lpMsgBuf;

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

        err.assign(static_cast<char*>(lpMsgBuf));
        LocalFree(lpMsgBuf);

        return err;
    }
#else
    void* Open(const std::string& name, const std::string& version, std::string* returnmsg)
    {
        std::string extension = ".so";
        if (!version.empty()) {
            extension += "." + version;
        }
        std::string filename = name;
        void* ret = NULL;

        if (!string::EndsWith(filename, extension))
            filename += extension;

#if defined(__ANDROID__) && (__ANDROID_API__ >= __ANDROID_API_L__)
        ret = android_dlopen_ext(filename.c_str(), RTLD_LAZY | RTLD_GLOBAL, nullptr);
#else
        ret = dlopen(filename.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#endif

        if ((ret == NULL) && (returnmsg != NULL)) {
            returnmsg->assign(dlerror());
        }
        return ret;
    }

    bool Close(void* handle)
    {
        if (handle)
            return dlclose(handle) == 0;

        return true;
    }

    void* GetSymbol(void* handle, const std::string& name)
    {
        // if(handle)
        return dlsym(handle, name.c_str());

        // return nullptr;
    }

    std::string GetError() { return dlerror(); }
#endif
} // namespace lib

namespace file {
#ifdef WIN32
    uint64_t Tell(FILE* f) { return static_cast<uint64_t>(_ftelli64(f)); }

    void Seek(FILE* f, long long offset, int32_t origin) { _fseeki64(f, offset, origin); }
#else
    uint64_t Tell(FILE* f) { return static_cast<uint64_t>(ftello(f)); }

    void Seek(FILE* f, long long offset, int32_t origin) { fseeko(f, offset, origin); }
#endif

    uint64_t Size(FILE* f)
    {
        uint64_t cur = Tell(f);
        Seek(f, 0, SEEK_END);
        uint64_t size = Tell(f);
        Seek(f, cur, SEEK_SET);
        return size;
    }

    FILE* OpenFileSearched(const std::string& filename, const char* mode)
    {
        FILE* res = nullptr;

        // Look in process path
        std::string path = os::GetAppPath() + filename;
        res = fopen(path.c_str(), mode);

        // Look in cwd
        if (!res) {
            path = os::GetCwd() + filename;
            res = fopen(path.c_str(), mode);
        }

        // Look on system env PATH.
        if (!res) {
            res = fopen(filename.c_str(), mode);
        }

        return res;
    }

    template <typename T>
    bool ReadContentsHelper(const std::string& filename, const char* mode, T& output)
    {
        FILE* f = OpenFileSearched(filename, mode);

        if (!f)
            return false;

        uint64_t s = Size(f);
        output.resize(s);
        size_t res = fread(&output[0], sizeof(uint8_t), s, f);
        bool result = ((res == s) || (feof(f) != 0));
        fclose(f);
        return result;
    }

    bool ReadContentsText(const std::string& filename, std::string& output)
    {
        return ReadContentsHelper(filename, "r", output);
    }

    bool ReadContentsBinary(const std::string& filename, DataBuffer& output)
    {
        return ReadContentsHelper(filename, "rb", output);
    }

    bool Exists(const char* path)
    {
        struct stat r;
        return (stat(path, &r) == 0);
    }

    bool Exists(const std::string& path) { return Exists(path.c_str()); }

    uint64_t GetModifiedTime(const char* path)
    {
        struct stat r;
        if (stat(path, &r) == 0) {
            return (uint64_t)r.st_mtime;
        }

        return 0;
    }

    bool IsTerminal(FILE* f)
    {
        if (!f) {
            return false;
        }

#ifdef WIN32
        return !!_isatty(_fileno(f));
#else
        return !!isatty(fileno(f));
#endif
    }
} // namespace file
} // namespace lcevc_dec::api_utility
