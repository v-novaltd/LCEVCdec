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

// Link time replacement for LCEVC_DEC API - captures calls and arguments for some entry points
//
// This should be replaced by proper API logging
//
#include "LCEVC/build_config.h"
#include "LCEVC/lcevc_dec.h"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <cassert>
#include <vector>

// A stream to dump API logging to - test can then inspect
static std::ostream* ostream;

void mockApiSetStream(std::ostream* os) { ostream = os; }

// Windows linker does not like __declspec(dllimport) for statically linked symbols, so don't
// test this on Windows until we have proper API logging, or use linker .def file rather than
// attributes to specify imports.
//
#if !VN_OS(WINDOWS)

LCEVC_ReturnCode LCEVC_DefaultPictureDesc(LCEVC_PictureDesc* pictureDesc, LCEVC_ColorFormat format,
                                          uint32_t width, uint32_t height)
{
    pictureDesc->colorFormat = format;
    pictureDesc->width = width;
    pictureDesc->height = height;

    return LCEVC_Success;
}

LCEVC_ReturnCode LCEVC_AllocPicture(LCEVC_DecoderHandle decHandle,
                                    const LCEVC_PictureDesc* pictureDesc, LCEVC_PictureHandle* picture)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_AllocPictureExternal(LCEVC_DecoderHandle decHandle,
                                            const LCEVC_PictureDesc* pictureDesc, uint32_t bufferCount,
                                            const LCEVC_PictureBufferDesc* buffers,
                                            LCEVC_PictureHandle* picture)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_FreePicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SetPictureFlag(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureFlag flag, bool value)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureFlag(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureFlag flag, bool* value)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureDesc(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureDesc* desc)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SetPictureDesc(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      const LCEVC_PictureDesc* desc)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureBufferCount(LCEVC_DecoderHandle decHandle,
                                             LCEVC_PictureHandle picHandle, uint32_t* bufferCount)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureBuffer(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                        uint32_t bufferIndex, LCEVC_PictureBufferDesc* bufferDesc)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPicturePlaneCount(LCEVC_DecoderHandle decHandle,
                                            LCEVC_PictureHandle picHandle, uint32_t* planeCount)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SetPictureUserData(LCEVC_DecoderHandle decHandle,
                                          LCEVC_PictureHandle picHandle, void* userData)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureUserData(LCEVC_DecoderHandle decHandle,
                                          LCEVC_PictureHandle picHandle, void** userData)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_LockPicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                   LCEVC_Access access, LCEVC_PictureLockHandle* pictureLock)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureLockBufferDesc(LCEVC_DecoderHandle decHandle,
                                                LCEVC_PictureLockHandle pictureLock,
                                                uint32_t bufferIndex, LCEVC_PictureBufferDesc* bufferDesc)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_GetPictureLockPlaneDesc(LCEVC_DecoderHandle decHandle,
                                               LCEVC_PictureLockHandle pictureLock,
                                               uint32_t planeIndex, LCEVC_PicturePlaneDesc* planeDesc)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_UnlockPicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureLockHandle pictureLock)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_CreateDecoder(LCEVC_DecoderHandle* decHandle, LCEVC_AccelContextHandle accelContext)
{
    assert(0);
    return LCEVC_Error;
}

// Use configuration name to fake a return code
static LCEVC_ReturnCode mockReturnCode(const char* name)
{
    if (!strncmp(name, "error_", 6)) {
        return LCEVC_Error;
    }

    if (!strncmp(name, "notfound_", 9)) {
        return LCEVC_NotFound;
    }

    return LCEVC_Success;
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderBool(LCEVC_DecoderHandle decHandle, const char* name, bool val)
{
    fmt::print(*ostream, "LCEVC_ConfigureDecoderBool({}, \"{}\", {})\n", decHandle.hdl, name, val);
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderInt(LCEVC_DecoderHandle decHandle, const char* name, int32_t val)
{
    fmt::print(*ostream, "LCEVC_ConfigureDecoderInt({}, \"{}\", {})\n", decHandle.hdl, name, val);
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderFloat(LCEVC_DecoderHandle decHandle, const char* name, float val)
{
    fmt::print(*ostream, "LCEVC_ConfigureDecoderFloat({}, \"{}\", {})\n", decHandle.hdl, name, val);
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderString(LCEVC_DecoderHandle decHandle, const char* name,
                                              const char* val)
{
    fmt::print(*ostream, "LCEVC_ConfigureDecoderString({}, \"{}\", \"{}\")\n", decHandle.hdl, name, val);
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderBoolArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                 uint32_t count, const bool* arr)
{
    std::vector<bool> vec(arr, arr + count);
    fmt::print(*ostream, "LCEVC_ConfigureDecoderBoolArray({}, \"{}\", {})\n", decHandle.hdl, name,
               fmt::join(vec, ", "));
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderIntArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                uint32_t count, const int32_t* arr)
{
    std::vector<int32_t> vec(arr, arr + count);
    fmt::print(*ostream, "LCEVC_ConfigureDecoderIntArray({}, \"{}\", {})\n", decHandle.hdl, name,
               fmt::join(vec, ", "));
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderFloatArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                  uint32_t count, const float* arr)
{
    std::vector<float> vec(arr, arr + count);
    fmt::print(*ostream, "LCEVC_ConfigureDecoderFloatArray({}, \"{}\", {})\n", decHandle.hdl, name,
               fmt::join(vec, ", "));
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderStringArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                   uint32_t count, const char* const * arr)
{
    std::vector<std::string> vec(arr, arr + count);
    fmt::print(*ostream, "LCEVC_ConfigureDecoderStringArray({}, \"{}\", {})\n", decHandle.hdl, name,
               fmt::join(vec, ", "));
    return mockReturnCode(name);
}

LCEVC_ReturnCode LCEVC_InitializeDecoder(LCEVC_DecoderHandle decHandle)
{
    assert(0);
    return LCEVC_Error;
}

void LCEVC_DestroyDecoder(LCEVC_DecoderHandle decHandle) { assert(0); }

LCEVC_ReturnCode LCEVC_SendDecoderEnhancementData(LCEVC_DecoderHandle decHandle, int64_t timestamp,
                                                  bool discontinuity, const uint8_t* data, uint32_t byteSize)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SendDecoderBase(LCEVC_DecoderHandle decHandle, int64_t timestamp, bool discontinuity,
                                       LCEVC_PictureHandle base, uint32_t timeoutUs, void* userData)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_ReceiveDecoderBase(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle* output)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SendDecoderPicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle output)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_ReceiveDecoderPicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle* output,
                                             LCEVC_DecodeInformation* decodeInformation)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_PeekDecoder(LCEVC_DecoderHandle decHandle, int64_t timestamp,
                                   LCEVC_PictureDesc* pictureDesc, LCEVC_DecodeInformation* information)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SkipDecoder(LCEVC_DecoderHandle decHandle, int64_t timestamp)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SynchronizeDecoder(LCEVC_DecoderHandle decHandle, bool dropPending)
{
    assert(0);
    return LCEVC_Error;
}

LCEVC_ReturnCode LCEVC_SetDecoderEventCallback(LCEVC_DecoderHandle decHandle,
                                               LCEVC_EventCallback callback, void* userData)
{
    assert(0);
    return LCEVC_Error;
}
#endif
