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

#ifndef VN_LCEVC_PIPELINE_PICTURE_H
#define VN_LCEVC_PIPELINE_PICTURE_H

#include <assert.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/types.h>
#include <stdint.h>
//
#include <LCEVC/pipeline/detail/pipeline_api.h>

// NOLINTBEGIN(modernize-use-using)

// Picture
//
typedef struct LdpPictureLock LdpPictureLock;
typedef struct LdpPicture LdpPicture;

typedef struct LdpPictureFunctions
{
    bool (*setDesc)(LdpPicture* picture, const LdpPictureDesc* desc);
    void (*getDesc)(const LdpPicture* picture, LdpPictureDesc* desc);

    bool (*getBufferDesc)(const LdpPicture* picture, LdpPictureBufferDesc* desc);
    bool (*setFlag)(LdpPicture* picture, uint8_t flag, bool value);
    bool (*getFlag)(const LdpPicture* picture, uint8_t flag);

    bool (*lock)(LdpPicture* picture, LdpAccess access, LdpPictureLock** pictureLock);
    void (*unlock)(LdpPicture* picture, LdpPictureLock* pictureLock);
    LdpPictureLock* (*getLock)(const LdpPicture* picture);

} LdpPictureFunctions;

typedef struct LdpPicture
{
    // Function pointer table
    const LdpPictureFunctions* functions;

    // The underlying memory
    LdpBuffer* buffer;

    // Offset within buffer
    uint32_t byteOffset;

    // Size (from offset) within buffer
    uint32_t byteSize;

    // Format: channels and dimensions
    LdpPictureLayout layout;

    LdpColorRange colorRange;
    LdpColorPrimaries colorPrimaries;
    LdpMatrixCoefficients matrixCoefficients;
    LdpTransferCharacteristics transferCharacteristics;
    LdpHDRStaticInfo hdrStaticInfo;
    LdpAspectRatio sampleAspectRatio;
    LdpPictureMargins margins;

    uint8_t publicFlags;

    void* userData;
} LdpPicture;

// PictureLock
//

typedef struct LdpPictureLockFunctions
{
    bool (*getBufferDesc)(const LdpPictureLock* pictureLock, LdpPictureBufferDesc* desc);
    bool (*getPlaneDesc)(const LdpPictureLock* pictureLock, uint32_t planeIndex,
                         LdpPicturePlaneDesc* planeDescOut);
} LdpPictureLockFunctions;

struct LdpPictureLock
{
    // Function pointer table
    const LdpPictureLockFunctions* functions;

    // Locked picture
    LdpPicture* picture;

    // Access for lock
    LdpAccess access;

    // buffer mapping
    LdpBufferMapping mapping;
};

static inline bool ldpPictureSetDesc(LdpPicture* picture, const LdpPictureDesc* desc)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->setDesc);
    return picture->functions->setDesc(picture, desc);
}

static inline void ldpPictureGetDesc(const LdpPicture* picture, LdpPictureDesc* desc)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->getDesc);
    picture->functions->getDesc(picture, desc);
}

static inline bool ldpPictureGetBufferDesc(const LdpPicture* picture, LdpPictureBufferDesc* desc)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->getBufferDesc);
    return picture->functions->getBufferDesc(picture, desc);
}

static inline bool ldpPictureSetFlag(LdpPicture* picture, uint8_t flag, bool value)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->setFlag);
    return picture->functions->setFlag(picture, flag, value);
}

static inline bool ldpPictureGetFlag(const LdpPicture* picture, uint8_t flag)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->getFlag);
    return picture->functions->getFlag(picture, flag);
}

static inline bool ldpPictureLock(LdpPicture* picture, LdpAccess access, LdpPictureLock** pictureLock)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->lock);
    return picture->functions->lock(picture, access, pictureLock);
}

static inline void ldpPictureUnlock(LdpPicture* picture, LdpPictureLock* pictureLock)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->unlock);
    picture->functions->unlock(picture, pictureLock);
}

static inline LdpPictureLock* ldpPictureGetLock(const LdpPicture* picture)
{
    assert(picture);
    assert(picture->functions);
    assert(picture->functions->getLock);
    return picture->functions->getLock(picture);
}

static inline bool ldpPictureLockGetBufferDesc(const LdpPictureLock* pictureLock, LdpPictureBufferDesc* desc)
{
    assert(pictureLock);
    assert(pictureLock->functions);
    assert(pictureLock->functions->getBufferDesc);
    return pictureLock->functions->getBufferDesc(pictureLock, desc);
}

static inline bool ldpPictureLockGetPlaneDesc(const LdpPictureLock* pictureLock,
                                              uint32_t planeIndex, LdpPicturePlaneDesc* planeDescOut)
{
    assert(pictureLock);
    assert(pictureLock->functions);
    assert(pictureLock->functions->getPlaneDesc);
    return pictureLock->functions->getPlaneDesc(pictureLock, planeIndex, planeDescOut);
}

// NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_PIPELINE_PICTURE_H
