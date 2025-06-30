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

#define VNEnablePublicAPIExport

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/constants.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/types.h>
//
#include "decoder_context.h"
#include "event_dispatcher.h"
#include "handle.h"
#include "interface.h"
#include "pool.h"
//
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace lcevc_dec::decoder;
using namespace lcevc_dec;

// - API Functions --------------------------------------------------------------------------------

// Decoder lifetime

LCEVC_API LCEVC_ReturnCode LCEVC_CreateDecoder(LCEVC_DecoderHandle* decHandle,
                                               LCEVC_AccelContextHandle accelContext)
{
    if (decHandle == nullptr) {
        return LCEVC_InvalidParam;
    }

    ldcDiagnosticsInitialize(NULL);
    ldcAccelerationInitialize(true);

    // Make the new decoder context
    //
    std::unique_ptr<DecoderContext> context = std::make_unique<DecoderContext>();
    // Note: DecoderPool has threadsafe allocation, so handles are guaranteed to be sequential and
    // valid.
    Handle<DecoderContext> hdl = DecoderContext::decoderPoolAdd(std::move(context));

    decHandle->hdl = hdl.handle;

    {
        LockedDecoder lockedDecoder(hdl);
        lockedDecoder.context()->handleSet(*decHandle);
    }

    return LCEVC_Success;
}

LCEVC_API void LCEVC_DestroyDecoder(LCEVC_DecoderHandle decHandle)
{
    if (decHandle.hdl == kInvalidHandle) {
        return;
    }

    std::unique_ptr<DecoderContext> ptr = DecoderContext::decoderPoolRemove(decHandle.hdl);

    // Clear out pools
    ptr->releasePools();

    // Nobody should be able to get a pointer to this decoder from here on - destroy at our leisure
    ptr.reset();

    ldcDiagnosticsRelease();
}

// Picture
//

LCEVC_API LCEVC_ReturnCode LCEVC_DefaultPictureDesc(LCEVC_PictureDesc* pictureDesc, LCEVC_ColorFormat format,
                                                    uint32_t width, uint32_t height)
{
    if (pictureDesc == nullptr) {
        return LCEVC_InvalidParam;
    }

    return fromLdcReturnCode(ldpDefaultPictureDesc(toLdpPictureDescPtr(pictureDesc),
                                                   toLdpColorFormat(format), width, height));

    return LCEVC_Success;
}

LCEVC_API LCEVC_ReturnCode LCEVC_AllocPicture(LCEVC_DecoderHandle decHandle,
                                              const LCEVC_PictureDesc* pictureDesc,
                                              LCEVC_PictureHandle* picHandle)
{
    if (picHandle == nullptr || pictureDesc == nullptr) {
        return LCEVC_InvalidParam;
    }

    const auto* ldpPictureDesc = toLdpPictureDescPtr(pictureDesc);

    picHandle->hdl = 0;
    return withLockedDecoder(decHandle.hdl, [&ldpPictureDesc, &picHandle,
                                             functionName = __FUNCTION__](DecoderContext* context) {
        LdpPicture* picture = context->pipeline()->allocPictureManaged(*ldpPictureDesc);
        if (!picture) {
            VNUnused(functionName);
            VNLogError("Unable to create a managed Picture!");
            return LCEVC_Error;
        }

        Handle<LdpPicture> hdl = context->picturePool().add(picture);
        if (!hdl.isValid()) {
            return LCEVC_Error;
        }

        *picHandle = LCEVC_PictureHandle{hdl.handle};
        return LCEVC_Success;
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_AllocPictureExternal(LCEVC_DecoderHandle decHandle,
                                                      const LCEVC_PictureDesc* pictureDesc,
                                                      const LCEVC_PictureBufferDesc* pictureBufferDesc,
                                                      const LCEVC_PicturePlaneDesc* picturePlaneDescs,
                                                      LCEVC_PictureHandle* picHandle)
{
    if (picHandle == nullptr || pictureDesc == nullptr ||
        (pictureBufferDesc == nullptr && picturePlaneDescs == nullptr)) {
        return LCEVC_InvalidParam;
    }

    const auto* ldpPictureDesc = toLdpPictureDescPtr(pictureDesc);
    const auto* ldpPictureBufferDesc = toLdpPictureBufferDescPtr(pictureBufferDesc);
    const auto* ldpPicturePlaneDescs = toLdpPicturePlaneDescPtr(picturePlaneDescs);

    picHandle->hdl = 0;

    return withLockedDecoder(decHandle.hdl,
                             [&ldpPictureDesc, &ldpPictureBufferDesc, &ldpPicturePlaneDescs,
                              &picHandle, functionName = __FUNCTION__](DecoderContext* context) {
                                 LdpPicture* picture = context->pipeline()->allocPictureExternal(
                                     *ldpPictureDesc, ldpPicturePlaneDescs, ldpPictureBufferDesc);
                                 if (!picture) {
                                     VNUnused(functionName);
                                     VNLogError("Unable to create an external Picture!");
                                     return LCEVC_Error;
                                 }
                                 Handle<LdpPicture> hdl = context->picturePool().add(picture);
                                 if (!hdl.isValid()) {
                                     return LCEVC_Error;
                                 }

                                 *picHandle = LCEVC_PictureHandle{hdl.handle};
                                 return LCEVC_Success;
                             });
}

LCEVC_API LCEVC_ReturnCode LCEVC_FreePicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle)
{
    if (picHandle.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoder(decHandle.hdl, [&picHandle](DecoderContext* context) {
        LdpPicture* pic{context->picturePool().remove(picHandle.hdl)};
        if (!pic) {
            return LCEVC_Error;
        }
        context->pipeline()->freePicture(pic);
        return LCEVC_Success;
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_GetPictureBuffer(LCEVC_DecoderHandle decHandle,
                                                  LCEVC_PictureHandle picHandle,
                                                  LCEVC_PictureBufferDesc* bufferDesc)
{
    if ((picHandle.hdl == kInvalidHandle) || (bufferDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    auto* ldpPictureBufferDesc = toLdpPictureBufferDescPtr(bufferDesc);

    return withLockedDecoderAndPicture(
        decHandle.hdl, picHandle.hdl,
        [&ldpPictureBufferDesc](const DecoderContext*, const LdpPicture* picture) {
            return (ldpPictureGetBufferDesc(picture, ldpPictureBufferDesc) ? LCEVC_Success : LCEVC_Error);
        });
}

LCEVC_ReturnCode LCEVC_GetPicturePlaneCount(LCEVC_DecoderHandle decHandle,
                                            LCEVC_PictureHandle picHandle, uint32_t* planeCount)
{
    if ((picHandle.hdl == kInvalidHandle) || (planeCount == nullptr)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoderAndPicture(decHandle.hdl, picHandle.hdl,
                                       [&planeCount](const DecoderContext*, const LdpPicture* picture) {
                                           *planeCount = ldpPictureLayoutPlanes(&picture->layout);
                                           return LCEVC_Success;
                                       });
}

LCEVC_ReturnCode LCEVC_SetPictureUserData(LCEVC_DecoderHandle decHandle,
                                          LCEVC_PictureHandle picHandle, void* userData)
{
    if (picHandle.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoderAndPicture(decHandle.hdl, picHandle.hdl,
                                       [&userData](const DecoderContext*, LdpPicture* picture) {
                                           picture->userData = userData;
                                           return LCEVC_Success;
                                       });
}

LCEVC_ReturnCode LCEVC_GetPictureUserData(LCEVC_DecoderHandle decHandle,
                                          LCEVC_PictureHandle picHandle, void** userData)
{
    if ((picHandle.hdl == kInvalidHandle) || (userData == nullptr)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoderAndPicture(decHandle.hdl, picHandle.hdl,
                                       [&userData](const DecoderContext*, const LdpPicture* picture) {
                                           *userData = picture->userData;
                                           return LCEVC_Success;
                                       });
}

LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureFlag(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureFlag flag, bool value)
{
    if ((flag == LCEVC_PictureFlag_Unknown) || (picHandle.hdl == kInvalidHandle)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoderAndPicture(
        decHandle.hdl, picHandle.hdl, [&flag, &value](const DecoderContext*, LdpPicture* picture) {
            ldpPictureSetFlag(picture, static_cast<uint8_t>(flag), value);
            return LCEVC_Success;
        });
}

LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureFlag(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureFlag flag, bool* value)
{
    if ((picHandle.hdl == kInvalidHandle) || (flag == LCEVC_PictureFlag_Unknown) || (value == nullptr)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoderAndPicture(
        decHandle.hdl, picHandle.hdl, [&flag, &value](const DecoderContext*, const LdpPicture* picture) {
            *value = ldpPictureGetFlag(picture, static_cast<uint8_t>(flag));
            return LCEVC_Success;
        });
}

LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureDesc(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      const LCEVC_PictureDesc* pictureDesc)
{
    if ((picHandle.hdl == kInvalidHandle) || (pictureDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    const auto* ldpPictureDesc = toLdpPictureDescPtr(pictureDesc);

    return withLockedDecoderAndPicture(
        decHandle.hdl, picHandle.hdl, [&ldpPictureDesc](const DecoderContext*, LdpPicture* picture) {
            return (ldpPictureSetDesc(picture, ldpPictureDesc) ? LCEVC_Success : LCEVC_Error);
        });
}

LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureDesc(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle,
                                      LCEVC_PictureDesc* desc)
{
    if ((desc == nullptr) || (picHandle.hdl == kInvalidHandle)) {
        return LCEVC_InvalidParam;
    }

    auto* ldpPictureDesc = toLdpPictureDescPtr(desc);

    return withLockedDecoderAndPicture(decHandle.hdl, picHandle.hdl,
                                       [&ldpPictureDesc](const DecoderContext*, const LdpPicture* picture) {
                                           ldpPictureGetDesc(picture, ldpPictureDesc);
                                           return LCEVC_Success;
                                       });
}

LCEVC_API LCEVC_ReturnCode LCEVC_LockPicture(LCEVC_DecoderHandle decHandle,
                                             LCEVC_PictureHandle picHandle, LCEVC_Access access,
                                             LCEVC_PictureLockHandle* pictureLockHandle)
{
    if ((picHandle.hdl == kInvalidHandle) || (pictureLockHandle == nullptr) ||
        (access == LCEVC_Access_Unknown) || (access > LCEVC_Access_Write)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoderAndPicture(
        decHandle.hdl, picHandle.hdl,
        [&access, &pictureLockHandle](DecoderContext* context, LdpPicture* picture) {
            if (ldpPictureGetLock(picture) != nullptr) {
                VNLogError("Already have a lock for Picture <%p>.", static_cast<void*>(picture));
                return LCEVC_Error;
            }

            LdpPictureLock* pictureLock = nullptr;
            if (!ldpPictureLock(picture, toLdpAccess(access), &pictureLock)) {
                *pictureLockHandle = LCEVC_PictureLockHandle{kInvalidHandle};
                return LCEVC_Error;
            }

            // Map Buffer if present
            // Allows any hardware API to sort out memory view
            //
            if (picture->buffer) {
                ldpBufferMap(picture->buffer, &pictureLock->mapping, picture->byteOffset,
                             ldpPictureLayoutSize(&picture->layout), toLdpAccess(access));
            }

            *pictureLockHandle =
                LCEVC_PictureLockHandle{context->pictureLockPool().add(pictureLock).handle};
            return LCEVC_Success;
        });
}

LCEVC_API LCEVC_ReturnCode LCEVC_GetPictureLockBufferDesc(LCEVC_DecoderHandle decHandle,
                                                          LCEVC_PictureLockHandle pictureLockHandle,
                                                          LCEVC_PictureBufferDesc* pictureBufferDesc)
{
    if ((pictureLockHandle.hdl == kInvalidHandle) || (pictureBufferDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    auto* ldpPictureBufferDesc = toLdpPictureBufferDescPtr(pictureBufferDesc);

    return withLockedDecoder(decHandle.hdl, [&pictureLockHandle,
                                             &ldpPictureBufferDesc](DecoderContext* context) {
        const LdpPictureLock* const picLock = context->pictureLockPool().lookup(pictureLockHandle.hdl);
        if (picLock == nullptr) {
            return LCEVC_InvalidParam;
        }
        if (!ldpPictureLockGetBufferDesc(picLock, ldpPictureBufferDesc)) {
            return LCEVC_Error;
        }
        return LCEVC_Success;
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_GetPictureLockPlaneDesc(LCEVC_DecoderHandle decHandle,
                                                         LCEVC_PictureLockHandle pictureLockHandle,
                                                         uint32_t planeIndex,
                                                         LCEVC_PicturePlaneDesc* planeDesc)
{
    if ((pictureLockHandle.hdl == kInvalidHandle) || (planeDesc == nullptr)) {
        return LCEVC_InvalidParam;
    }

    auto* ldpPicturePlaneDesc = toLdpPicturePlaneDescPtr(planeDesc);

    return withLockedDecoder(decHandle.hdl, [&pictureLockHandle, &planeIndex,
                                             &ldpPicturePlaneDesc](DecoderContext* context) {
        const LdpPictureLock* const picLock = context->pictureLockPool().lookup(pictureLockHandle.hdl);
        if (picLock == nullptr) {
            return LCEVC_InvalidParam;
        }
        if (!ldpPictureLockGetPlaneDesc(picLock, planeIndex, ldpPicturePlaneDesc)) {
            return LCEVC_Error;
        }
        return LCEVC_Success;
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_UnlockPicture(LCEVC_DecoderHandle decHandle,
                                               LCEVC_PictureLockHandle pictureLockHandle)
{
    if (pictureLockHandle.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoder(decHandle.hdl, [&pictureLockHandle](DecoderContext* context) {
        LdpPictureLock* const picLock = context->pictureLockPool().remove(pictureLockHandle.hdl);
        if (picLock == nullptr) {
            return LCEVC_InvalidParam;
        }

        // Unmap the buffer if present
        //
        if (picLock->picture->buffer) {
            ldpBufferUnmap(picLock->picture->buffer, &picLock->mapping);
        }

        ldpPictureUnlock(picLock->picture, picLock);
        return LCEVC_Success;
    });
}

// Configuration

template <typename T>
static LCEVC_ReturnCode internalConfigure(LCEVC_DecoderHandle& decHandle, const char* name, const T& val)
{
    return withLockedUninitializedDecoder(decHandle.hdl, [&name, &val](DecoderContext* context) {
        return context->configure(name, val) ? LCEVC_Success : LCEVC_Error;
    });
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
    // Can't use internalConfigureArray directly: have to convert const char* to string, one at a time.
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
    return withLockedUninitializedDecoder(decHandle.hdl, [](DecoderContext* context) {
        return context->initialize() ? LCEVC_Success : LCEVC_Error;
    });
}

// Decoding
//
LCEVC_API LCEVC_ReturnCode LCEVC_SendDecoderEnhancementData(LCEVC_DecoderHandle decHandle, uint64_t timestamp,
                                                            const uint8_t* data, uint32_t byteSize)
{
    return withLockedDecoder(decHandle.hdl, [&timestamp, &data, &byteSize](DecoderContext* context) {
        return fromLdcReturnCode(context->pipeline()->sendEnhancementData(timestamp, data, byteSize));
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_SendDecoderBase(LCEVC_DecoderHandle decHandle, uint64_t timestamp,
                                                 LCEVC_PictureHandle base, uint32_t timeoutUs, void* userData)
{
    return withLockedDecoder(decHandle.hdl, [&timestamp, &base, &timeoutUs, &userData](DecoderContext* context) {
        LdpPicture* basePicture = context->picturePool().lookup(base.hdl);
        return fromLdcReturnCode(
            context->pipeline()->sendBasePicture(timestamp, basePicture, timeoutUs, userData));
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_ReceiveDecoderBase(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle* output)
{
    if (output == nullptr) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoder(decHandle.hdl, [&output](DecoderContext* context) {
        const LdpPicture* const finishedBase = context->pipeline()->receiveFinishedBasePicture();
        if (finishedBase == nullptr) {
            return LCEVC_Again;
        }
        output->hdl = context->picturePool().reverseLookup(finishedBase).handle;
        return LCEVC_Success;
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_SendDecoderPicture(LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle output)
{
    if (output.hdl == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoder(decHandle.hdl, [&output](DecoderContext* context) {
        LdpPicture* outputPicture = context->picturePool().lookup(output.hdl);
        return fromLdcReturnCode(context->pipeline()->sendOutputPicture(outputPicture));
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_ReceiveDecoderPicture(LCEVC_DecoderHandle decHandle,
                                                       LCEVC_PictureHandle* output,
                                                       LCEVC_DecodeInformation* decodeInformation)
{
    if ((output == nullptr) || (decodeInformation == nullptr)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoder(decHandle.hdl, [&output, &decodeInformation](DecoderContext* context) {
        LdpDecodeInformation di;
        const LdpPicture* const outputPicture = context->pipeline()->receiveOutputPicture(di);
        if (outputPicture == nullptr) {
            return LCEVC_Again;
        }

        // Copy DecodeInformation over to destination
        *toLdpDecodeInformationPtr(decodeInformation) = di;

        output->hdl = context->picturePool().reverseLookup(outputPicture).handle;
        return LCEVC_Success;
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_PeekDecoder(LCEVC_DecoderHandle decHandle, uint64_t timestamp,
                                             uint32_t* width, uint32_t* height)

{
    if ((width == nullptr) || (height == nullptr)) {
        return LCEVC_InvalidParam;
    }

    return withLockedDecoder(decHandle.hdl, [&timestamp, &width, &height](DecoderContext* context) {
        return fromLdcReturnCode(context->pipeline()->peek(timestamp, *width, *height));
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_SkipDecoder(LCEVC_DecoderHandle decHandle, uint64_t timestamp)
{
    return withLockedDecoder(decHandle.hdl, [&timestamp](DecoderContext* context) {
        return fromLdcReturnCode(context->pipeline()->skip(timestamp));
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_SynchronizeDecoder(LCEVC_DecoderHandle decHandle, bool dropPending)
{
    return withLockedDecoder(decHandle.hdl, [&dropPending](DecoderContext* context) {
        return fromLdcReturnCode(context->pipeline()->synchronize(dropPending));
    });
}

LCEVC_API LCEVC_ReturnCode LCEVC_FlushDecoder(LCEVC_DecoderHandle decHandle)
{
    return withLockedDecoder(decHandle.hdl, [](DecoderContext* context) {
        return fromLdcReturnCode(context->pipeline()->flush(kInvalidTimestamp));
    });
}

// Events
//
LCEVC_API
LCEVC_ReturnCode LCEVC_SetDecoderEventCallback(LCEVC_DecoderHandle decHandle,
                                               LCEVC_EventCallback callback, void* userData)
{
    return withLockedUninitializedDecoder(decHandle.hdl, [&callback, &userData](DecoderContext* context) {
        context->eventDispatcher()->setEventCallback(callback, userData);
        return LCEVC_Success;
    });
}
