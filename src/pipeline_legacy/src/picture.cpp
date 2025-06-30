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

#define VNComponentCurrent LdcComponentPicture

#include "picture.h"
//
#include <LCEVC/common/log.h>
#include <LCEVC/legacy/PerseusDecoder.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/types.h>
//
#include "buffer_manager.h"
#include "core_interface.h"
#include "picture_copy.h"
#include "picture_lock.h"

#include <cassert>
#include <cstring>
#include <memory>

namespace lcevc_dec::decoder {

// - Picture --------------------------------------------------------------------------------------
namespace {
    extern const LdpPictureFunctions kPictureFunctions;
}

Picture::Picture()
    : LdpPicture{&kPictureFunctions}
{
    ldpPictureLayoutInitialize(&layout, LdpColorFormatUnknown, 0, 0, 0);

    // Fill in defaults
    colorRange = LdpColorRangeUnknown;
    colorPrimaries = LdpColorPrimariesUnspecified;
    matrixCoefficients = LdpMatrixCoefficientsUnspecified;
    transferCharacteristics = LdpTransferCharacteristicsUnspecified;
    hdrStaticInfo = {};
    publicFlags = 0;
    sampleAspectRatio = {1, 1};
    margins = {};
    userData = nullptr;
}

Picture::~Picture()
{
    // Should have already unlocked (and un-bound) by now, in the child class.
    assert(!isLocked());
}

bool Picture::copyMetadata(const Picture& source)
{
    // This copies all format information, as well as the timestamp (since the typical use case is
    // in passthrough mode). Other identifying information is not copied (since this is, after all,
    // meant to help uniquely identify a picture), and underlying data is not copied either (that's
    // copyData).
    if (!canModify()) {
        return false;
    }

    LdpPictureDesc sourceDesc;
    source.getDesc(sourceDesc);
    if (!setDesc(sourceDesc)) {
        return false;
    }

    colorRange = source.colorRange;
    matrixCoefficients = source.matrixCoefficients;
    transferCharacteristics = source.transferCharacteristics;
    publicFlags = source.publicFlags;
    hdrStaticInfo = source.hdrStaticInfo;
    sampleAspectRatio = source.sampleAspectRatio;
    margins = source.margins;
    return true;
}

bool Picture::copyData(const Picture& source)
{
    if (!canModify()) {
        return false;
    }
    if (!isValid() || !source.isValid()) {
        return false;
    }

    // NV12->I420
    if (ldpPictureLayoutIsInterleaved(&source.layout) && !ldpPictureLayoutIsInterleaved(&layout) &&
        ldpPictureLayoutColorSpace(&layout) == LdpColorSpaceYUV) {
        copyNV12ToI420Picture(source, *this);
        return true;
    }

    // No handling yet for I420->NV12
    if (!ldpPictureLayoutIsInterleaved(&source.layout) && ldpPictureLayoutIsInterleaved(&layout) &&
        ldpPictureLayoutColorSpace(&layout) == LdpColorSpaceYUV) {
        VNLogError("timestamp %" PRId64
                   ":Cannot currently copy directly from non-NV12 to NV12 pictures",
                   getTimestamp());
        return false;
    }

    if (ldpPictureLayoutFormat(&source.layout) != ldpPictureLayoutFormat(&layout)) {
        VNLogError("timestamp %" PRId64
                   ": Cannot currently copy directly from format %u to format %u.",
                   getTimestamp(), static_cast<uint32_t>(ldpPictureLayoutFormat(&source.layout)),
                   static_cast<uint32_t>(ldpPictureLayoutFormat(&layout)));
        return false;
    }

    copyPictureToPicture(source, *this);
    return true;
}

bool Picture::toCoreImage(perseus_image& dest)
{
    const uint32_t numPlanes = getNumPlanes();
#ifdef VN_SDK_LOG_ENABLE_ERROR
    const uint64_t timestamp = getTimestamp();
#endif

    if (numPlanes > VN_IMAGE_NUM_PLANES) {
        VNLogError("timestamp %" PRId64 ": image has too many planes: %d", timestamp, numPlanes);
        return false;
    }

    int32_t interleaving = 0;
    if (!toCoreInterleaving(ldpPictureLayoutFormat(&layout), ldpPictureLayoutIsInterleaved(&layout),
                            interleaving)) {
        VNLogError("timestamp %" PRId64 ": Failed to get interleaving from <%s>", timestamp,
                   toString().c_str());
        return false;
    }
    dest.ilv = static_cast<perseus_interleaving>(interleaving);

    int32_t bitdepth = PSS_DEPTH_8;
    if (!toCoreBitdepth(ldpPictureLayoutSampleBits(&layout), bitdepth)) {
        VNLogError("timestamp %" PRId64 ": Failed to get bit depth from <%s>", timestamp,
                   toString().c_str());
        return false;
    }
    dest.depth = static_cast<perseus_bitdepth>(bitdepth);

    for (uint32_t i = 0; i < std::min(numPlanes, static_cast<uint32_t>(VN_IMAGE_NUM_PLANES)); i++) {
        dest.plane[i] = getPlaneFirstSample(i);
        dest.stride[i] = getPlaneSampleStride(i); // Core needs stride in samples
    }

    return true;
}

bool Picture::isValid() const
{
    if (ldpPictureLayoutFormat(&layout) == LdpColorFormatUnknown) {
        return false;
    }
    if (getPlaneFirstSample(0) == nullptr) {
        return false;
    }
    return true;
}

void Picture::setPublicFlag(uint8_t flag, bool value)
{
    // flags start at 1 so, subtract 1 to make sure we use all 8 bits.
    if (value) {
        publicFlags = publicFlags | static_cast<uint8_t>((0x1 << (flag - 1)));
    } else {
        publicFlags = publicFlags & static_cast<uint8_t>(~(0x1 << (flag - 1)));
    }
}

bool Picture::getPublicFlag(uint8_t flag) const
{
    return ((publicFlags & (0x01 << (flag - 1))) != 0);
}

void Picture::getDesc(LdpPictureDesc& desc) const
{
    desc.colorFormat = ldpPictureLayoutFormat(&layout);
    desc.colorRange = colorRange;
    desc.colorPrimaries = colorPrimaries;
    desc.matrixCoefficients = matrixCoefficients;
    desc.transferCharacteristics = transferCharacteristics;
    desc.hdrStaticInfo = hdrStaticInfo;
    desc.sampleAspectRatioDen = sampleAspectRatio.denominator;
    desc.sampleAspectRatioNum = sampleAspectRatio.numerator;
    desc.width = getWidth();
    desc.height = getHeight();
    desc.cropTop = margins.top;
    desc.cropBottom = margins.bottom;
    desc.cropLeft = margins.left;
    desc.cropRight = margins.right;
}

uint8_t* Picture::internalGetPlaneFirstSample(uint32_t planeIdx) const
{
    // This is the default behaviour: pictures are presumed to have 1 buffer with at least 1 plane,
    // but child classes may override this behaviour.
    uint8_t* out = getBuffer();
    if (out == nullptr) {
        return nullptr;
    }

    for (uint32_t prevPlane = 0; prevPlane < planeIdx; prevPlane++) {
        out += getPlaneMemorySize(prevPlane);
    }
    return out;
}

bool Picture::lock(LdpAccess access, PictureLock*& lockOut)
{
    if (isLocked()) {
        return false;
    }

    if (access != LdpAccessRead && access != LdpAccessModify && access != LdpAccessWrite) {
        return false;
    }

    // Create lock object
    m_lock = std::make_unique<PictureLock>(this, access);

    lockOut = m_lock.get();
    return true;
}

bool Picture::unlock(const PictureLock* lock)
{
    if (!isLocked()) {
        return false;
    }

    assert(lock == m_lock.get());

    // Release the lock object
    m_lock.reset();
    return true;
}

bool Picture::setDesc(const LdpPictureDesc& newDesc) { return setDesc(newDesc, nullptr); }

// Private functions:

void Picture::setName(const std::string& name) { m_name = "Picture:" + name; }

bool Picture::setDesc(const LdpPictureDesc& newDesc, const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes])
{
    // This is either called via setDescExternal (in which case planeStridesBytes is set from the
    // plane descs, if provide), or else via the normal public setDesc function (in which case
    // rowStridesBytes is automatically set).
    if (!initializeDesc(newDesc, rowStridesBytes)) {
        VNLogError("timestamp %" PRId64 ": Invalid new desc for Picture <%s>.", getTimestamp(),
                   m_name.c_str());
        return false;
    }

    return true;
}

std::string Picture::getShortDbgString() const
{
    char tmp[512];
    uint32_t width = (ldpPictureLayoutPlanes(&layout) > 0) ? getWidth() : 0;
    uint32_t height = (ldpPictureLayoutPlanes(&layout) > 0) ? getHeight() : 0;
    snprintf(tmp, sizeof(tmp) - 1, "%s, %s, %p, fmt %d:%d, byteDepth=%d, bitDepthPP=%d, size=%dx%d.",
             m_name.c_str(), isManaged() ? "Managed" : "Unmanaged", this,
             ldpPictureLayoutFormat(&layout), ldpPictureLayoutIsInterleaved(&layout),
             ldpPictureLayoutSampleSize(&layout), ldpPictureLayoutSampleBits(&layout), width, height);
    return tmp;
}

std::string Picture::toString() const
{
    std::string result;

    result = getShortDbgString() + "\n";

    char tmp[256];
    for (uint32_t i = 0; i < getNumPlanes(); ++i) {
        snprintf(tmp, sizeof(tmp) - 1, "Plane %d/%d. sampleByteStride:%d, rowByteStride:%d. ", i,
                 getNumPlanes(), getPlaneBytesPerPixel(i), getPlaneByteStride(i));
        result += tmp;
        result += "\n";
    }
    return result;
}

bool Picture::initializeDesc(const LdpPictureDesc& desc,
                             const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes])
{
    // Note that error messages in this function just uses the name, rather than the full debug
    // string. This is because the debug string reports format data that isn't meaningful until
    // AFTER initializeDesc succeeds.

    if (!canModify()) {
        VNLogError("timestamp %" PRId64
                   ": Picture cannot be modified, so cannot set desc. Picture: <%s>",
                   getTimestamp(), m_name.c_str());
        return false;
    }

    if (desc.colorFormat == LdpColorFormatUnknown) {
        VNLogError("timestamp %" PRId64 ": Invalid format, cannot set desc. Picture: <%s>.",
                   getTimestamp(), m_name.c_str());
        return false;
    }

    colorRange = desc.colorRange;
    matrixCoefficients = desc.matrixCoefficients;
    transferCharacteristics = desc.transferCharacteristics;
    hdrStaticInfo = desc.hdrStaticInfo;
    sampleAspectRatio = {desc.sampleAspectRatioNum, desc.sampleAspectRatioDen};

    if (rowStridesBytes) {
        if (!ldpPictureDescCheckValidStrides(&desc, rowStridesBytes)) {
            VNLogError("Invalid strides given for %dx%d plane", desc.width, desc.height);
            return false;
        }
        ldpPictureLayoutInitializeDescStrides(&layout, &desc, rowStridesBytes);
    } else {
        ldpPictureLayoutInitializeDesc(&layout, &desc, 0);
    }

    if (((desc.cropLeft + desc.cropRight) > desc.width) ||
        ((desc.cropTop + desc.cropBottom) > desc.height)) {
        VNLogError("timestamp %" PRId64
                   ". Requested to crop out more than the whole picture. Requested crops are: left "
                   "%u, right %u, top %u, bottom %u. Size is %ux%u. Picture: <%s>.",
                   getTimestamp(), desc.cropLeft, desc.cropRight, desc.cropTop, desc.cropBottom,
                   desc.width, desc.height, m_name.c_str());
        return false;
    }
    margins = {desc.cropLeft, desc.cropTop, desc.cropRight, desc.cropBottom};

    return true;
}

bool Picture::bindMemory()
{
    if (!canModify()) {
        VNLogError("timestamp %" PRId64 ": Locked, cannot bind memory. Picture: <%s>",
                   getTimestamp(), getShortDbgString().c_str());
        return false;
    }
    return true;
}

bool Picture::unbindMemory()
{
    VNLogVerbose("timestamp %" PRId64 ": UNBIND <%s>", getTimestamp(), toString().c_str());
    if (!canModify()) {
        VNLogError("timestamp %" PRId64 ": Locked, cannot unbind memory. Picture: <%s>",
                   getTimestamp(), getShortDbgString().c_str());
        return false;
    }
    return true;
}

uint32_t Picture::getRequiredSize() const
{
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < ldpPictureLayoutPlanes(&layout); i++) {
        totalSize += ldpPictureLayoutPlaneSize(&layout, i);
        VNLogVerbose("timestamp %" PRId64 ": [%d] S %dx%d size %d, Total Size: %d (plane loc: %p)",
                     getTimestamp(), i, ldpPictureLayoutPlaneWidth(&layout, i),
                     ldpPictureLayoutPlaneHeight(&layout, i), ldpPictureLayoutPlaneSize(&layout, i),
                     totalSize, static_cast<const void*>(getPlaneFirstSample(i)));
    }
    return totalSize;
}

// - PictureExternal ------------------------------------------------------------------------------

PictureExternal::~PictureExternal()
{
    unlock(getLock());
    unbindMemory();
}

bool PictureExternal::descsMatch(const LdpPictureDesc& newDesc, const LdpPicturePlaneDesc* newPlaneDescArr,
                                 const LdpPictureBufferDesc* newBufferDesc)
{
    if (ldpPictureLayoutPlanes(&layout) == 0) {
        return false; // Picture isn't initialised so cannot match
    }
    LdpPictureDesc curDesc;
    getDesc(curDesc);
    if (newDesc != curDesc) {
        return false;
    }

    // If one's null and the other's not, they mismatch
    if ((m_bufferDesc == nullptr) != (newBufferDesc == nullptr)) {
        return false;
    }

    if (m_bufferDesc != nullptr) {
        // We're here so they're both NOT null
        if (*m_bufferDesc != *newBufferDesc) {
            return false;
        }
    }

    // If one's null and the other's not, they mismatch
    if ((m_planeDescs == nullptr) != (newPlaneDescArr == nullptr)) {
        return false;
    }

    if (m_planeDescs != nullptr) {
        LdpPictureLayout newLayout = {};
        ldpPictureLayoutInitializeDesc(&newLayout, &newDesc, 0);
        for (uint32_t planeIdx = 0; planeIdx < ldpPictureLayoutPlanes(&newLayout); planeIdx++) {
            if ((*m_planeDescs)[planeIdx] != newPlaneDescArr[planeIdx]) {
                return false;
            }
        }
    }

    return true;
}

bool PictureExternal::setDesc(const LdpPictureDesc& newDesc,
                              const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes])
{
    if (!BaseClass::setDesc(newDesc, rowStridesBytes)) {
        return false;
    }

    // When both are provided, bufferDesc is the authority on allocation size, while planeDesc is
    // authority on memory locations.
    uint32_t totalAllocatedBytes = 0;
    if (m_bufferDesc != nullptr) {
        totalAllocatedBytes = m_bufferDesc->byteSize;
    } else {
        for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
            totalAllocatedBytes += getPlaneMemorySize(planeIdx);
        }
    }

    // If planeDesc was provided, then it will dictate getRequiredSize. In other words, when both
    // are provided, this checks that planeDesc doesn't exceed bufferDesc (smaller is fine though).
    if (getRequiredSize() > totalAllocatedBytes) {
        VNLogWarning(
            "timestamp %" PRId64
            ": Did not allocate enough memory for the new desc. New desc is %ux%u, %u bits "
            "per sample, with a format of %d. Picture is <%s>",
            getTimestamp(), newDesc.width, newDesc.height, bitdepthFromLdpColorFormat(newDesc.colorFormat),
            static_cast<uint16_t>(newDesc.colorFormat), getShortDbgString().c_str());
        return false;
    }

    return true;
}

bool PictureExternal::setDesc(const LdpPictureDesc& newDesc)
{
    // This is only used to RE-set an external picture's plane desc. So use the existing plane and
    // buffer descs and pass it on to the normal setDescExternal function (which checks for changes
    // before doing anything).

    std::unique_ptr<LdpPictureBufferDesc> bufferDesc;
    if (m_bufferDesc) {
        bufferDesc = std::make_unique<LdpPictureBufferDesc>();
        *bufferDesc = *m_bufferDesc;
    }

    std::unique_ptr<LdpPicturePlaneDesc[]> planeDescArr = nullptr;
    if (m_planeDescs != nullptr) {
        planeDescArr = std::make_unique<LdpPicturePlaneDesc[]>(getNumPlanes());

        LdpPictureLayout newLayout = {};
        ldpPictureLayoutInitializeDesc(&newLayout, &newDesc, 0);

        for (uint32_t planeIdx = 0; planeIdx < ldpPictureLayoutPlanes(&newLayout); planeIdx++) {
            planeDescArr[planeIdx] = (*m_planeDescs)[planeIdx];
        }
    }

    return setDescExternal(newDesc, planeDescArr.get(), bufferDesc.get());
}

bool PictureExternal::setDescExternal(const LdpPictureDesc& newDesc,
                                      const LdpPicturePlaneDesc* newPlaneDescArr,
                                      const LdpPictureBufferDesc* newBufferDesc)
{
    // Check for changes, then bind, THEN set desc.

    if (descsMatch(newDesc, newPlaneDescArr, newBufferDesc)) {
        return true;
    }
    ldpPictureLayoutInitializeDesc(&layout, &newDesc, 0);
    if (!bindMemoryBufferAndPlanes(ldpPictureLayoutPlanes(&layout), newPlaneDescArr, newBufferDesc)) {
        VNLogError("Failed to bind memory for external picture at %p", static_cast<void*>(this));
        return false;
    }

    // If there's a manual stride, set it up:
    std::unique_ptr<uint32_t[]> rowStridesBytes = nullptr;
    if (newPlaneDescArr != nullptr) {
        rowStridesBytes = std::make_unique<uint32_t[]>(kLdpPictureMaxNumPlanes);
        for (uint32_t planeIdx = 0; planeIdx < ldpPictureLayoutPlanes(&layout); planeIdx++) {
            rowStridesBytes[planeIdx] = newPlaneDescArr[planeIdx].rowByteStride;
        }
    }

    return setDesc(newDesc, rowStridesBytes.get());
}

uint8_t* PictureExternal::internalGetPlaneFirstSample(uint32_t planeIdx) const
{
    if (m_planeDescs != nullptr) {
        return (*m_planeDescs)[planeIdx].firstSample;
    }

    return BaseClass::internalGetPlaneFirstSample(planeIdx);
}

bool PictureExternal::getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const
{
    if (m_bufferDesc == nullptr) {
        return false;
    }

    const LdpPictureBufferDesc& desc = *m_bufferDesc;
    bufferDescOut = {desc.data, desc.byteSize, nullptr, desc.access};
    return true;
}

bool PictureExternal::getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const
{
    if (m_planeDescs == nullptr) {
        for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
            planeDescArrOut[planeIdx].firstSample = internalGetPlaneFirstSample(planeIdx);
            planeDescArrOut[planeIdx].rowByteStride = getPlaneByteStride(planeIdx);
        }
        return true;
    }

    for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
        planeDescArrOut[planeIdx] = (*m_planeDescs)[planeIdx];
    }
    return true;
}

bool PictureExternal::bindMemoryBufferAndPlanes(uint32_t numPlanes, const LdpPicturePlaneDesc* planeDescArr,
                                                const LdpPictureBufferDesc* bufferDesc)
{
    if (!BaseClass::bindMemory()) {
        return false;
    }

    // This should have already been validated. Normally, non-null is communicated by references,
    // so we wouldn't need this assert
    assert(bufferDesc != nullptr || planeDescArr != nullptr);

    // If we're rebinding, we need to reset our descs (for example, if we used to have a bufferDesc
    // but the client no longer wants us to know the bufferDesc).
    m_bufferDesc = nullptr;
    m_planeDescs = nullptr;

    if (bufferDesc != nullptr) {
        // Can't use make_unique (before C++20), it doesn't pass brace-initializers to structs.
        // NOLINTNEXTLINE(modernize-make-unique)
        m_bufferDesc = std::make_unique<LdpPictureBufferDesc>();
        *m_bufferDesc = *bufferDesc;
    }

    if (planeDescArr != nullptr) {
        m_planeDescs = std::make_unique<std::array<LdpPicturePlaneDesc, kLdpPictureMaxNumPlanes>>();

        for (uint32_t planeIdx = 0; planeIdx < numPlanes; planeIdx++) {
            (*m_planeDescs)[planeIdx] = {planeDescArr[planeIdx].firstSample,
                                         planeDescArr[planeIdx].rowByteStride};
        }
    }

    return true;
}

bool PictureExternal::unbindMemory()
{
    if (!BaseClass::unbindMemory()) {
        return false;
    }

    m_bufferDesc = nullptr;
    m_planeDescs = nullptr;
    return true;
}

// - PictureManaged -------------------------------------------------------------------------------

PictureManaged::~PictureManaged()
{
    unlock(getLock());
    unbindMemory();
}

bool PictureManaged::getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const
{
    if (m_buffer == nullptr) {
        return false;
    }

    bufferDescOut.data = m_buffer->data();
    bufferDescOut.byteSize = static_cast<uint32_t>(m_buffer->size() * sizeof((*m_buffer)[0]));
    bufferDescOut.accelBuffer = nullptr;
    bufferDescOut.access = LdpAccessUnknown;
    return true;
}

bool PictureManaged::getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const
{
    for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
        planeDescArrOut[planeIdx].firstSample = internalGetPlaneFirstSample(planeIdx);
        planeDescArrOut[planeIdx].rowByteStride = getPlaneByteStride(planeIdx);
    }
    return true;
}

bool PictureManaged::descsMatch(const LdpPictureDesc& newDesc)
{
    if (ldpPictureLayoutPlanes(&layout) == 0) {
        return false; // Picture isn't initialised so cannot match
    }
    LdpPictureDesc curDesc;
    getDesc(curDesc);
    return newDesc == curDesc;
}

bool PictureManaged::setDesc(const LdpPictureDesc& newDesc)
{
    // Check for changes, then set descs, THEN bind (based on new descs).
    if (descsMatch(newDesc)) {
        return true;
    }

    if (!BaseClass::setDesc(newDesc)) {
        return false;
    }

    if (!unbindMemory()) {
        VNLogError("timestamp %" PRId64 ": Failed to unbind old memory for managed picture <%s>.",
                   getTimestamp(), getShortDbgString().c_str());
        return false;
    }
    bindMemory();
    return true;
}

bool PictureManaged::bindMemory()
{
    if (!BaseClass::bindMemory()) {
        return false;
    }

    const uint32_t requiredSize = getRequiredSize();
    if (requiredSize == 0) {
        VNLogError("timestamp %" PRId64 ": Binding to nothing. Picture: <%s>", getTimestamp(),
                   getShortDbgString().c_str());
    }

    // m_buffer might be set, if we are resizing.
    if (m_buffer == nullptr) {
        m_buffer = m_bufferManagerHandle.getBuffer(requiredSize);
    } else {
        m_buffer->clear();
        if (requiredSize > m_buffer->size()) {
            m_buffer->resize(requiredSize);
        }
    }
    VNLogVerbose("timestamp %" PRId64 ": Allocated %" PRId64
                 " total bytes. Picture full description: %s",
                 getTimestamp(), requiredSize, toString().c_str());
    return true;
}

bool PictureManaged::unbindMemory()
{
    if (!BaseClass::unbindMemory()) {
        return false;
    }

    bool ret = true;
    if (m_buffer != nullptr) {
        ret = m_bufferManagerHandle.releaseBuffer(m_buffer);
        m_buffer = nullptr;
    }
    return ret;
}

// C function table to connect to C++ class
//
namespace {
    bool setDesc(LdpPicture* picture, const LdpPictureDesc* desc)
    {
        return static_cast<Picture*>(picture)->setDesc(*desc);
    }
    void getDesc(const LdpPicture* picture, LdpPictureDesc* desc)
    {
        static_cast<const Picture*>(picture)->getDesc(*desc);
    }

    bool getBufferDesc(const LdpPicture* picture, LdpPictureBufferDesc* desc)
    {
        return static_cast<const Picture*>(picture)->getBufferDesc(*desc);
    }

    bool setFlag(LdpPicture* picture, uint8_t flag, bool value)
    {
        static_cast<Picture*>(picture)->setPublicFlag(flag, value);
        return true;
    }

    bool getFlag(const LdpPicture* picture, uint8_t flag)
    {
        return static_cast<const Picture*>(picture)->getPublicFlag(flag);
    }

    bool lock(LdpPicture* picture, LdpAccess access, LdpPictureLock** pictureLock)
    {
        PictureLock* ptr = nullptr;

        if (!static_cast<Picture*>(picture)->lock(access, ptr)) {
            return false;
        }

        *pictureLock = ptr;
        return true;
    }

    void unlock(LdpPicture* picture, LdpPictureLock* pictureLock)
    {
        const PictureLock* const ptr = static_cast<PictureLock*>(pictureLock);
        static_cast<Picture*>(picture)->unlock(ptr);
    }

    LdpPictureLock* getLock(const LdpPicture* picture)
    {
        return static_cast<const Picture*>(picture)->getLock();
    }

    const LdpPictureFunctions kPictureFunctions = {
        setDesc, getDesc, getBufferDesc, setFlag, getFlag, lock, unlock, getLock,
    };
} // namespace

} // namespace lcevc_dec::decoder
