/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#define VNEnablePublicAPIExport

#include "accel_context.h"
#include "decoder.h"
#include "decoder_pool.h"
#include "handle.h"
#include "picture.h"
#include "picture_lock.h"
#include "pool.h"

#include <LCEVC/lcevc_dec.h>

#include <memory>
#include <vector>

// ------------------------------------------------------------------------------------------------

using namespace lcevc_dec::decoder;

// - Structs --------------------------------------------------------------------------------------

// These are Opaque wrappers for their Handled classes. We have to hide them in structs here
// because the header has to follow C syntax, so templates can't be exposed, nor can nested
// namespaces.

// - Helper functions -----------------------------------------------------------------------------

LCEVC_ReturnCode getLockAndCheckDecoder(bool shouldBeInitialised, const Handle<Decoder>& decHandle,
                                        Decoder*& decoderOut,
                                        std::unique_ptr<std::lock_guard<std::mutex>>& lockOut)
{
    if (decHandle == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    lockOut = std::make_unique<std::lock_guard<std::mutex>>(DecoderPool::get().lookupMutex(decHandle));
    decoderOut = DecoderPool::get().lookup(decHandle);
    if (decoderOut == nullptr) {
        // If it's null and we expect it to be initialized, report the error as "uninitialized",
        // although a more accurate error would really be "uncreated".
        return (shouldBeInitialised ? LCEVC_Uninitialized : LCEVC_InvalidParam);
    }

    if (decoderOut->isInitialized() != shouldBeInitialised) {
        return (shouldBeInitialised ? LCEVC_Uninitialized : LCEVC_Initialized);
    }

    return LCEVC_Success;
}

// - API Functions --------------------------------------------------------------------------------

// Picture

LCEVC_API LCEVC_ReturnCode LCEVC_DefaultPictureDesc(LCEVC_PictureDesc* pictureDesc, LCEVC_ColorFormat format,
                                                    uint32_t width, uint32_t height)
{
    if (pictureDesc == nullptr) {
        return LCEVC_InvalidParam;
    }

    pictureDesc->width = width;
    pictureDesc->height = height;
    pictureDesc->colorFormat = format;
    pictureDesc->colorRange = LCEVC_ColorRange_Unknown;
    pictureDesc->colorPrimaries = LCEVC_ColorPrimaries_Unspecified;
    pictureDesc->matrixCoefficients = LCEVC_MatrixCoefficients_Unspecified;
    pictureDesc->transferCharacteristics = LCEVC_TransferCharacteristics_Unspecified;
    pictureDesc->hdrStaticInfo = {};
    pictureDesc->sampleAspectRatioNum = 1;
    pictureDesc->sampleAspectRatioDen = 1;
    pictureDesc->cropTop = 0;
    pictureDesc->cropBottom = 0;
    pictureDesc->cropLeft = 0;
    pictureDesc->cropRight = 0;

    return LCEVC_Success;
}

LCEVC_API LCEVC_ReturnCode LCEVC_AllocPicture(LCEVC_DecoderHandle decHandle,
                                              const LCEVC_PictureDesc* pictureDesc,
                                              LCEVC_PictureHandle* picHandle)
{
    if (picHandle == nullptr) {
        return LCEVC_InvalidParam;
    }
    picHandle->hdl = 0;

    if (pictureDesc == nullptr) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return (decoder->allocPictureManaged(*pictureDesc, *picHandle) ? LCEVC_Success : LCEVC_Error);
}

LCEVC_API LCEVC_ReturnCode LCEVC_AllocPictureExternal(LCEVC_DecoderHandle decHandle,
                                                      const LCEVC_PictureDesc* pictureDesc,
                                                      const LCEVC_PictureBufferDesc* buffer,
                                                      const LCEVC_PicturePlaneDesc* planes,
                                                      LCEVC_PictureHandle* picHandle)
{
    if (picHandle == nullptr) {
        return LCEVC_InvalidParam;
    }
    picHandle->hdl = 0;
    if (pictureDesc == nullptr || (buffer == nullptr && planes == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return (decoder->allocPictureExternal(*pictureDesc, *picHandle, planes, buffer) ? LCEVC_Success
                                                                                    : LCEVC_Error);
}

LCEVC_API LCEVC_ReturnCode LCEVC_FreePicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle)
{
    if (picHandle.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return (decoder->releasePicture(picHandle.hdl) ? LCEVC_Success : LCEVC_Error);
}

LCEVC_API LCEVC_ReturnCode LCEVC_GetPictureBuffer(LCEVC_DecoderHandle decHandle,
                                                  LCEVC_PictureHandle picHandle,
                                                  LCEVC_PictureBufferDesc* bufferDesc)
{
    if ((picHandle.hdl == kInvalidHandle) || (bufferDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    const Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    return (pic->getBufferDesc(*bufferDesc) ? LCEVC_Success : LCEVC_Error);
}

LCEVC_ReturnCode LCEVC_GetPicturePlaneCount(LCEVC_DecoderHandle decHandle,
                                            LCEVC_PictureHandle picHandle, uint32_t* planeCount)
{
    if ((picHandle.hdl == kInvalidHandle) || (planeCount == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    const Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    *planeCount = pic->getNumPlanes();
    return LCEVC_Success;
}

LCEVC_ReturnCode LCEVC_SetPictureUserData(LCEVC_DecoderHandle decHandle,
                                          LCEVC_PictureHandle picHandle, void* userData)
{
    if (picHandle.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    pic->setUserData(userData);
    return LCEVC_Success;
}

LCEVC_ReturnCode LCEVC_GetPictureUserData(LCEVC_DecoderHandle decHandle,
                                          LCEVC_PictureHandle picHandle, void** userData)
{
    if ((picHandle.hdl == kInvalidHandle) || (userData == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    const Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    *userData = pic->getUserData();
    return LCEVC_Success;
}

LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureFlag(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureFlag flag, bool value)
{
    if ((flag == LCEVC_PictureFlag_Unknown) || (picHandle.hdl == kInvalidHandle)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    pic->setPublicFlag(flag, value);
    return LCEVC_Success;
}

LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureFlag(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureFlag flag, bool* value)
{
    if ((picHandle.hdl == kInvalidHandle) || (flag == LCEVC_PictureFlag_Unknown) || (value == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    const Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    *value = pic->getPublicFlag(flag);
    return LCEVC_Success;
}

LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureDesc(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      const LCEVC_PictureDesc* desc)
{
    if ((picHandle.hdl == kInvalidHandle) || (desc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    return (pic->setDesc(*desc) ? LCEVC_Success : LCEVC_Error);
}

LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureDesc(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureDesc* desc)
{
    if ((desc == nullptr) || (picHandle.hdl == kInvalidHandle)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    const Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    pic->getDesc(*desc);
    return LCEVC_Success;
}

LCEVC_API LCEVC_ReturnCode LCEVC_LockPicture(LCEVC_DecoderHandle decHandle,
                                             LCEVC_PictureHandle picHandle, LCEVC_Access access,
                                             LCEVC_PictureLockHandle* pictureLockHandle)
{
    if ((picHandle.hdl == kInvalidHandle) || (pictureLockHandle == nullptr) ||
        (access == LCEVC_Access_Unknown) || (access > LCEVC_Access_Write)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    Picture* pic = decoder->getPicture(picHandle.hdl);
    if (pic == nullptr) {
        return LCEVC_InvalidParam;
    }

    return (decoder->lockPicture(*pic, static_cast<Access>(access), *pictureLockHandle) ? LCEVC_Success
                                                                                        : LCEVC_Error);
}

LCEVC_API LCEVC_ReturnCode LCEVC_GetPictureLockBufferDesc(LCEVC_DecoderHandle decHandle,
                                                          LCEVC_PictureLockHandle pictureLockHandle,
                                                          LCEVC_PictureBufferDesc* bufferDesc)
{
    if ((pictureLockHandle.hdl == kInvalidHandle) || (bufferDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    PictureLock* picLock = decoder->getPictureLock(pictureLockHandle.hdl);
    if (picLock->getBufferDesc() == nullptr) {
        return LCEVC_Error;
    }

    toLCEVCPictureBufferDesc(*(picLock->getBufferDesc()), *bufferDesc);

    return LCEVC_Success;
}

LCEVC_API LCEVC_ReturnCode LCEVC_GetPictureLockPlaneDesc(LCEVC_DecoderHandle decHandle,
                                                         LCEVC_PictureLockHandle pictureLockHandle,
                                                         uint32_t planeIndex,
                                                         LCEVC_PicturePlaneDesc* planeDesc)
{
    if ((pictureLockHandle.hdl == kInvalidHandle) || (planeDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    PictureLock* picLock = decoder->getPictureLock(pictureLockHandle.hdl);
    if (picLock->getPlaneDescArr() == nullptr) {
        return LCEVC_Error;
    }

    toLCEVCPicturePlaneDesc((*picLock->getPlaneDescArr())[planeIndex], *planeDesc);

    return LCEVC_Success;
}

LCEVC_API LCEVC_ReturnCode LCEVC_UnlockPicture(LCEVC_DecoderHandle decHandle,
                                               LCEVC_PictureLockHandle pictureLockHandle)
{
    if (pictureLockHandle.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return (decoder->unlockPicture(pictureLockHandle.hdl) ? LCEVC_Success : LCEVC_Error);
}

// Decoder

LCEVC_API LCEVC_ReturnCode LCEVC_CreateDecoder(LCEVC_DecoderHandle* decHandle,
                                               LCEVC_AccelContextHandle accelContext)
{
    if (decHandle == nullptr) {
        return LCEVC_InvalidParam;
    }

    // Note: DecoderPool has threadsafe allocation, so handles are guaranteed to be sequential and
    // valid.
    std::unique_ptr<Decoder> newDecoder = std::make_unique<Decoder>(accelContext, *decHandle);
    decHandle->hdl = DecoderPool::get().allocate(std::move(newDecoder)).handle;

    return LCEVC_Success;
}

template <typename T>
static LCEVC_ReturnCode internalConfigure(LCEVC_DecoderHandle& decHandle, const char* name, const T& val)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(false, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return (decoder->setConfig(name, val) ? LCEVC_Success : LCEVC_Error);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderBool(LCEVC_DecoderHandle decHandle, const char* name, bool val)
{
    return internalConfigure(decHandle, name, val);
}
LCEVC_ReturnCode LCEVC_ConfigureDecoderInt(LCEVC_DecoderHandle decHandle, const char* name, int32_t val)
{
    return internalConfigure(decHandle, name, val);
}
LCEVC_ReturnCode LCEVC_ConfigureDecoderFloat(LCEVC_DecoderHandle decHandle, const char* name, float val)
{
    return internalConfigure(decHandle, name, val);
}
LCEVC_ReturnCode LCEVC_ConfigureDecoderString(LCEVC_DecoderHandle decHandle, const char* name,
                                              const char* val)
{
    return internalConfigure(decHandle, name, std::string(val));
}

template <typename T>
static LCEVC_ReturnCode internalConfigureArray(LCEVC_DecoderHandle& decHandle, const char* name,
                                               size_t count, const T* arr)
{
    if (arr == nullptr) {
        return LCEVC_InvalidParam;
    }
    std::vector<T> arrAsVec(arr, arr + count);
    return internalConfigure(decHandle, name, arrAsVec);
}

LCEVC_ReturnCode LCEVC_ConfigureDecoderBoolArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                 uint32_t count, const bool* arr)
{
    return internalConfigureArray(decHandle, name, count, arr);
}
LCEVC_ReturnCode LCEVC_ConfigureDecoderIntArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                uint32_t count, const int32_t* arr)
{
    return internalConfigureArray(decHandle, name, count, arr);
}
LCEVC_ReturnCode LCEVC_ConfigureDecoderFloatArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                  uint32_t count, const float* arr)
{
    return internalConfigureArray(decHandle, name, count, arr);
}
LCEVC_ReturnCode LCEVC_ConfigureDecoderStringArray(LCEVC_DecoderHandle decHandle, const char* name,
                                                   uint32_t count, const char* const * arr)
{
    // Can't use internalConfigureArray: have to convert const char* to string, one at a time.
    if (arr == nullptr) {
        return LCEVC_InvalidParam;
    }

    std::vector<std::string> stringArr(count, "");
    size_t idx = 0;
    for (const char* str = *arr; idx < count; idx++) {
        stringArr[idx] = std::string(str);
        str++;
    }
    return internalConfigure(decHandle, name, stringArr);
}

LCEVC_API LCEVC_ReturnCode LCEVC_InitializeDecoder(LCEVC_DecoderHandle decHandle)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(false, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return (decoder->initialize() ? LCEVC_Success : LCEVC_Error);
}

LCEVC_API void LCEVC_DestroyDecoder(LCEVC_DecoderHandle decHandle)
{
    if (decHandle.hdl == kInvalidHandle) {
        return;
    }
    DecoderPool::get().release(decHandle.hdl);
}

LCEVC_API LCEVC_ReturnCode LCEVC_SendDecoderEnhancementData(LCEVC_DecoderHandle decHandle,
                                                            int64_t timestamp, bool discontinuity,
                                                            const uint8_t* data, uint32_t byteSize)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->feedEnhancementData(timestamp, discontinuity, data, byteSize);
}

LCEVC_API LCEVC_ReturnCode LCEVC_SendDecoderBase(LCEVC_DecoderHandle decHandle, int64_t timestamp,
                                                 bool discontinuity, LCEVC_PictureHandle base,
                                                 uint32_t timeoutUs, void* userData)
{
    if (base.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->feedBase(timestamp, discontinuity, base.hdl, timeoutUs, userData);
}

LCEVC_API LCEVC_ReturnCode LCEVC_ReceiveDecoderBase(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle* output)
{
    if (output == nullptr) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->produceFinishedBase(*output);
}

LCEVC_API LCEVC_ReturnCode LCEVC_SendDecoderPicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle output)
{
    if (output.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->feedOutputPicture(output.hdl);
}

LCEVC_API LCEVC_ReturnCode LCEVC_ReceiveDecoderPicture(LCEVC_DecoderHandle decHandle,
                                                       LCEVC_PictureHandle* output,
                                                       LCEVC_DecodeInformation* decodeInformation)
{
    if ((output == nullptr) || (decodeInformation == nullptr)) {
        return LCEVC_InvalidParam;
    }

    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
    if (getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->produceOutputPicture(*output, *decodeInformation);
}

LCEVC_API LCEVC_ReturnCode LCEVC_SkipDecoder(LCEVC_DecoderHandle decHandle, int64_t timestamp)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    if (const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
        getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->skip(timestamp);
}

LCEVC_API LCEVC_ReturnCode LCEVC_SynchronizeDecoder(LCEVC_DecoderHandle decHandle, bool dropPending)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    if (const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
        getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->synchronize(dropPending);
}

LCEVC_API LCEVC_ReturnCode LCEVC_FlushDecoder(LCEVC_DecoderHandle decHandle)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    if (const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(true, decHandle.hdl, decoder, lock);
        getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    return decoder->flush();
}

LCEVC_API
LCEVC_ReturnCode LCEVC_SetDecoderEventCallback(LCEVC_DecoderHandle decHandle,
                                               LCEVC_EventCallback callback, void* userData)
{
    std::unique_ptr<std::lock_guard<std::mutex>> lock = nullptr;
    Decoder* decoder = nullptr;
    if (const LCEVC_ReturnCode getDecResult = getLockAndCheckDecoder(false, decHandle.hdl, decoder, lock);
        getDecResult != LCEVC_Success) {
        return getDecResult;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    decoder->setEventCallback(reinterpret_cast<EventCallback>(callback), userData);
    return LCEVC_Success;
}
