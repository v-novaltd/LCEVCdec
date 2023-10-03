/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "picture.h"

#include "accel_context.h"
#include "buffer_manager.h"
#include "interface.h"
#include "picture_lock.h"
#include "pool.h"
#include "uLog.h"
#include "uPictureCopy.h"

#include <LCEVC/PerseusDecoder.h>
#include <LCEVC/lcevc_dec.h>

// ------------------------------------------------------------------------------------------------

using namespace lcevc_dec::api_utility;

namespace lcevc_dec::decoder {

// ------------------------------------------------------------------------------------------------

// TODO: These will need to be specialised in the case of non-buffer pictures.

static bool copyNV12ToI420Picture(const Picture& src, Picture& dest)
{
    if (src.getNumPlanes() != 2 || dest.getNumPlanes() != 3) {
        VNLogError("CC %u, PTS %" PRId64 ": Wrong plane counts. Source has %u planes (should be 2) "
                   "and dest has %u (should be 3).\n",
                   timehandleGetCC(src.getTimehandle()), timehandleGetTimestamp(src.getTimehandle()),
                   src.getNumPlanes(), dest.getNumPlanes());
        return false;
    }

    const uint32_t height = std::min(src.getHeight(), dest.getHeight());
    const std::array<const uint8_t*, 2> srcBufs = {
        src.getPlaneFirstSample(0),
        src.getPlaneFirstSample(1),
    };
    const std::array<uint8_t*, 3> destBufs = {
        dest.getPlaneFirstSample(0),
        dest.getPlaneFirstSample(1),
        dest.getPlaneFirstSample(2),
    };
    const uint32_t srcYMemorySize = src.getPlaneMemorySize(0);
    const uint32_t destYMemorySize = dest.getPlaneMemorySize(0);
    const std::array<uint32_t, 2> srcPlaneByteStrides = {src.getPlaneByteStride(0),
                                                         src.getPlaneByteStride(1)};
    const std::array<uint32_t, 3> destPlaneByteStrides = {
        dest.getPlaneByteStride(0), dest.getPlaneByteStride(1), dest.getPlaneByteStride(2)};
    const std::array<uint32_t, 2> srcPlaneByteWidths = {src.getPlaneWidthBytes(0),
                                                        src.getPlaneWidthBytes(1)};
    const std::array<uint32_t, 3> destPlaneByteWidths = {
        dest.getPlaneWidthBytes(0), dest.getPlaneWidthBytes(1), dest.getPlaneWidthBytes(2)};

    copyNV12ToI420Buffers(srcBufs, srcPlaneByteStrides, srcPlaneByteWidths, srcYMemorySize, destBufs,
                          destPlaneByteStrides, destPlaneByteWidths, destYMemorySize, height);

    return true;
}

static bool copyPictureToPicture(const Picture& src, Picture& dest)
{
    // We copy 1 plane at a time, in case the images have matching formats but mismatching layouts.
    uint32_t numPlanes = std::min(src.getNumPlanes(), dest.getNumPlanes());
    for (uint32_t i = 0; i < numPlanes; ++i) {
        simpleCopyPlaneBuffer(src.getPlaneFirstSample(i), src.getPlaneByteStride(i),
                              src.getPlaneWidthBytes(i), src.getPlaneHeight(i),
                              src.getPlaneMemorySize(i), dest.getPlaneFirstSample(i),
                              dest.getPlaneByteStride(i), dest.getPlaneWidthBytes(i),
                              dest.getPlaneHeight(i), dest.getPlaneMemorySize(i));
    }
    return true;
}

// - Picture --------------------------------------------------------------------------------------

// Public functions:

Picture::~Picture()
{
    // Should have already unlocked (and un-bound) by now, in the child class.
    VNAssert(!isLocked());
}

bool Picture::copyMetadata(const Picture& source)
{
    // This copies all format information, as well as the timehandle (since the typical use case is
    // in passthrough mode). Other identifying information is not copied (since this is, after all,
    // meant to help uniquely identify a picture), and underlying data is not copied either (that's
    // copyData).
    if (!canModify()) {
        return false;
    }

    LCEVC_PictureDesc sourceDesc;
    source.getDesc(sourceDesc);
    if (!setDesc(sourceDesc)) {
        return false;
    }

    m_colorRange = source.m_colorRange;
    m_matrixCoefficients = source.m_matrixCoefficients;
    m_transferCharacteristics = source.m_transferCharacteristics;
    m_publicFlags = source.m_publicFlags;
    m_hdrStaticInfo = source.m_hdrStaticInfo;
    m_sampleAspectRatio = source.m_sampleAspectRatio;
    m_crop = source.m_crop;
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
    if ((source.m_desc.GetInterleaving() == PictureInterleaving::NV12) &&
        (m_desc.GetInterleaving() != PictureInterleaving::NV12) &&
        (PictureFormat::IsYUV(m_desc.GetFormat()))) {
        if (source.getNumPlanes() != 2 || getNumPlanes() != 3) {
            VNLogError("CC %u, PTS %" PRId64
                       ": Claim to be copying from NV12 to I420, but source picture has %u planes, "
                       "and this picture has %u planes\n",
                       timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                       source.getNumPlanes(), getNumPlanes());
            return false;
        }
        return copyNV12ToI420Picture(source, *this);
    }

    // No handling yet for I420->NV12
    if ((source.m_desc.GetInterleaving() != PictureInterleaving::NV12) &&
        (PictureFormat::IsYUV(source.m_desc.GetFormat())) &&
        (m_desc.GetInterleaving() == PictureInterleaving::NV12)) {
        VNLogError("CC %u, PTS %" PRId64
                   ":Cannot currently copy directly from non-NV12 to NV12 pictures\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()));
        return false;
    }

    if (source.m_desc.GetFormat() != m_desc.GetFormat()) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Cannot currently copy directly from format %u to format %u.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   source.m_desc.GetFormat(), m_desc.GetFormat());
        return false;
    }

    if (!copyPictureToPicture(source, *this)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to copy from <%s> to this picture, <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   source.getShortDbgString().c_str(), getShortDbgString().c_str());
        return false;
    }

    return true;
}

bool Picture::toCoreImage(perseus_image& dest)
{
    int32_t interleaving = 0;
    if (!toCoreInterleaving(m_desc.GetFormat(), m_desc.GetInterleaving(), interleaving)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to get interleaving from <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   toString().c_str());
        return false;
    }
    dest.ilv = static_cast<perseus_interleaving>(interleaving);

    int32_t bitdepth = PSS_DEPTH_8;
    if (!toCoreBitdepth(m_desc.GetBitDepth(), bitdepth)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to get interleaving from <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   toString().c_str());
        return false;
    }
    dest.depth = static_cast<perseus_bitdepth>(bitdepth);

    for (uint32_t i = 0; i < getNumPlanes(); i++) {
        dest.plane[i] = getPlaneFirstSample(i);
        dest.stride[i] = getPlaneSampleStride(i); // Core needs stride in samples
    }

    return true;
}

bool Picture::isValid() const
{
    if (m_desc.GetFormat() == PictureFormat::Invalid) {
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
        m_publicFlags = m_publicFlags | (0x1 << (flag - 1));
    } else {
        m_publicFlags = m_publicFlags & ~(0x1 << (flag - 1));
    }
}

bool Picture::getPublicFlag(uint8_t flag) const
{
    return ((m_publicFlags & (0x01 << (flag - 1))) != 0);
}

void Picture::getDesc(LCEVC_PictureDesc& desc) const
{
    desc.colorFormat = static_cast<LCEVC_ColorFormat>(
        toLCEVCDescColorFormat(m_desc.GetFormat(), m_desc.GetInterleaving()));

    desc.colorRange = static_cast<LCEVC_ColorRange>(toLCEVCColorRange(m_colorRange));
    desc.colorPrimaries =
        static_cast<LCEVC_ColorPrimaries>(toLCEVCColorPrimaries(m_desc.GetColorspace()));
    desc.matrixCoefficients =
        static_cast<LCEVC_MatrixCoefficients>(toLCEVCMatrixCoefficients(m_matrixCoefficients));
    desc.transferCharacteristics = static_cast<LCEVC_TransferCharacteristics>(
        toLCEVCTransferCharacteristics(m_transferCharacteristics));
    memcpy(&desc.hdrStaticInfo, &m_hdrStaticInfo, sizeof(HDRStaticInfo));
    desc.sampleAspectRatioDen = m_sampleAspectRatio.denominator;
    desc.sampleAspectRatioNum = m_sampleAspectRatio.numerator;
    desc.width = getWidth();
    desc.height = getHeight();
    desc.cropTop = m_crop.top;
    desc.cropBottom = m_crop.bottom;
    desc.cropLeft = m_crop.left;
    desc.cropRight = m_crop.right;
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

bool Picture::lock(Access access, Handle<PictureLock> newLock)
{
    if (isLocked()) {
        return false;
    }

    switch (access) {
        case Access::Unknown: return false;

        case Access::Read:
        case Access::Modify:
        case Access::Write: m_lock = newLock; break;
    }

    return true;
}

bool Picture::unlock()
{
    if (!isLocked()) {
        return false;
    }
    m_lock = kInvalidHandle;
    return true;
}

bool Picture::setDesc(const LCEVC_PictureDesc& newDesc) { return setDesc(newDesc, nullptr); }

// Private functions:

void Picture::setName(const std::string& name) { m_name = "Picture:" + name; }

bool Picture::setDesc(const LCEVC_PictureDesc& newDesc,
                      const uint32_t planeStridesBytes[PictureFormatDesc::kMaxNumPlanes])
{
    // This is either called via setDescExternal (in which case planeStridesBytes is set from the
    // plane descs, if provide), or else via the normal public setDesc function (in which case
    // planeStridesBytes is NEVER set).
    if (!initializeDesc(newDesc, planeStridesBytes)) {
        VNLogError("CC %u, PTS %" PRId64 ": Invalid new desc for Picture <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    return true;
}

std::string Picture::getShortDbgString() const
{
    std::string result;

    char tmp[512];
    snprintf(tmp, sizeof(tmp) - 1, "%s, %s, %p, fmt %d:%d, byteDepth=%d, bitDepthPP=%d, size=%d x %d.",
             m_name.c_str(), isManaged() ? "Managed" : "Unmanaged", this, m_desc.GetFormat(),
             m_desc.GetInterleaving(), m_desc.GetByteDepth(), m_desc.GetBitDepthPerPixel(),
             getWidth(), getHeight());
    return {tmp};
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

bool Picture::initializeDesc(const LCEVC_PictureDesc& desc,
                             const uint32_t planeStridesBytes[PictureFormatDesc::kMaxNumPlanes])
{
    // Note that error messages in this function just uses the name, rather than the full debug
    // string. This is because the debug string reports format data that isn't meaningful until
    // AFTER initializeDesc succeeds.

    if (!canModify()) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Picture cannot be modified, so cannot set desc. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    const PictureFormat::Enum format = fromLCEVCDescColorFormat(desc.colorFormat);
    if (format == PictureFormat::Invalid) {
        VNLogError("CC %u, PTS %" PRId64 ": Invalid format, cannot set desc. Picture: <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    const PictureInterleaving::Enum interleaving = fromLCEVCDescInterleaving(desc.colorFormat);
    if (interleaving == PictureInterleaving::Invalid) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Invalid interleaving, cannot set desc. Picture: <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    m_colorRange = fromLCEVCColorRange(desc.colorRange);
    m_matrixCoefficients = fromLCEVCMatrixCoefficients(desc.matrixCoefficients);
    m_transferCharacteristics = fromLCEVCTransferCharacteristics(desc.transferCharacteristics);
    memcpy(&m_hdrStaticInfo, &desc.hdrStaticInfo, sizeof(LCEVC_HDRStaticInfo));
    m_sampleAspectRatio = {desc.sampleAspectRatioNum, desc.sampleAspectRatioDen};

    const Colorspace::Enum colorspace = fromLCEVCColorPrimaries(desc.colorPrimaries);
    const uint32_t bitdepth = bitdepthFromLCEVCDescColorFormat(desc.colorFormat);
    if (!m_desc.Initialise(format, desc.width, desc.height, interleaving, colorspace, bitdepth,
                           planeStridesBytes)) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Couldn't initialise pictureFormatDesc with format %d:%d, size %dx%d.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   format, interleaving, desc.width, desc.height);
        return false;
    }

    if (((desc.cropLeft + desc.cropRight) > desc.width) ||
        ((desc.cropTop + desc.cropBottom) > desc.height)) {
        VNLogError("CC %u, PTS %" PRId64
                   ". Requested to crop out more than the whole picture. Requested crops are: left "
                   "%u, right %u, top %u, bottom %u. Size is %ux%u. Picture: <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   desc.cropLeft, desc.cropRight, desc.cropTop, desc.cropBottom, desc.width,
                   desc.height, m_name.c_str());
        return false;
    }
    m_crop = {desc.cropLeft, desc.cropTop, desc.cropRight, desc.cropBottom};

    return true;
}

uint32_t Picture::planeHorizontalShift(PictureFormat::Enum format, uint32_t planeIndex)
{
    return planeIndex > 0 ? ChromaSamplingType::GetHorizontalShift(format) : 0;
}

uint32_t Picture::planeVerticalShift(PictureFormat::Enum format, uint32_t planeIndex)
{
    return planeIndex > 0 ? ChromaSamplingType::GetVerticalShift(format) : 0;
}

bool Picture::bindMemory()
{
    if (!canModify()) {
        VNLogError("CC %u, PTS %" PRId64 ": Locked, cannot bind memory. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
        return false;
    }
    return true;
}

bool Picture::unbindMemory()
{
    VNLogVerbose("CC %u, PTS %" PRId64 ": UNBIND <%s>\n", timehandleGetCC(getTimehandle()),
                 timehandleGetTimestamp(getTimehandle()), toString().c_str());
    if (!canModify()) {
        VNLogError("CC %u, PTS %" PRId64 ": Locked, cannot unbind memory. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
        return false;
    }
    return true;
}

uint32_t Picture::getRequiredSize() const
{
    const uint32_t planesCount = m_desc.GetPlaneCount();
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < planesCount; i++) {
        totalSize += m_desc.GetPlaneMemorySize(i);
        VNLogVerbose("CC %u, PTS %" PRId64
                     ": [%d] S %dx%d size %d, Total Size: %d (plane loc: %p)\n",
                     timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()), i,
                     m_desc.GetPlaneWidth(i), m_desc.GetPlaneHeight(i),
                     m_desc.GetPlaneMemorySize(i), totalSize, getPlaneFirstSample(i));
    }
    return totalSize;
}

// - PictureExternal ------------------------------------------------------------------------------

PictureExternal::~PictureExternal()
{
    unlock();
    unbindMemory();
}

bool PictureExternal::descsMatch(const LCEVC_PictureDesc& newDesc,
                                 const LCEVC_PicturePlaneDesc* newPlaneDescArr,
                                 const LCEVC_PictureBufferDesc* newBufferDesc)
{
    LCEVC_PictureDesc curDesc;
    getDesc(curDesc);
    if (!equals(newDesc, curDesc)) {
        return false;
    }

    // If one's null and the other's not, they mismatch
    if ((m_bufferDesc == nullptr) != (newBufferDesc == nullptr)) {
        return false;
    }

    if (m_bufferDesc != nullptr) {
        // We're here so they're both NOT null
        LCEVC_PictureBufferDesc curBufferDesc = {};
        toLCEVCPictureBufferDesc(*m_bufferDesc, curBufferDesc);
        if (!equals(curBufferDesc, *newBufferDesc)) {
            return false;
        }
    }

    // If one's null and the other's not, they mismatch
    if ((m_planeDescs == nullptr) != (newPlaneDescArr == nullptr)) {
        return false;
    }

    if (m_planeDescs != nullptr) {
        LCEVC_PicturePlaneDesc reusablePlaneDesc = {};
        const PictureInterleaving::Enum ilv = fromLCEVCDescInterleaving(newDesc.colorFormat);
        const PictureFormat::Enum fmt = fromLCEVCDescColorFormat(newDesc.colorFormat);
        const uint32_t numPlanes = PictureFormat::NumPlanes(fmt, ilv);
        for (uint32_t planeIdx = 0; planeIdx < numPlanes; planeIdx++) {
            toLCEVCPicturePlaneDesc((*m_planeDescs)[planeIdx], reusablePlaneDesc);
            if (!equals(reusablePlaneDesc, newPlaneDescArr[planeIdx])) {
                return false;
            }
        }
    }

    return true;
}

bool PictureExternal::setDesc(const LCEVC_PictureDesc& newDesc,
                              const uint32_t planeStridesBytes[PictureFormatDesc::kMaxNumPlanes])
{
    if (!BaseClass::setDesc(newDesc, planeStridesBytes)) {
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
            "CC %u, PTS %" PRId64
            ": Did not allocate enough memory for the new desc. New desc is %ux%u, %u bits "
            "per sample, with a format of %d. Picture is <%s>\n",
            timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
            newDesc.width, newDesc.height, bitdepthFromLCEVCDescColorFormat(newDesc.colorFormat),
            newDesc.colorFormat, getShortDbgString().c_str());
        return false;
    }

    return true;
}

bool PictureExternal::setDesc(const LCEVC_PictureDesc& newDesc)
{
    // This is only used to RE-set an external picture's plane desc. So use the existing plane and
    // buffer descs and pass it on to the normal setDescExternal function (which checks for changes
    // before doing anything).

    std::unique_ptr<LCEVC_PictureBufferDesc> curBufferDesc = nullptr;
    if (m_bufferDesc != nullptr) {
        curBufferDesc = std::make_unique<LCEVC_PictureBufferDesc>();
        toLCEVCPictureBufferDesc(*m_bufferDesc, *curBufferDesc);
    }

    std::unique_ptr<LCEVC_PicturePlaneDesc[]> planeDescArr = nullptr;
    if (m_planeDescs != nullptr) {
        planeDescArr = std::make_unique<LCEVC_PicturePlaneDesc[]>(getNumPlanes());
        const PictureInterleaving::Enum ilv = fromLCEVCDescInterleaving(newDesc.colorFormat);
        const PictureFormat::Enum fmt = fromLCEVCDescColorFormat(newDesc.colorFormat);
        const uint32_t numPlanes = PictureFormat::NumPlanes(fmt, ilv);
        for (uint32_t planeIdx = 0; planeIdx < numPlanes; planeIdx++) {
            toLCEVCPicturePlaneDesc((*m_planeDescs)[planeIdx], planeDescArr[planeIdx]);
        }
    }

    return setDescExternal(newDesc, planeDescArr.get(), curBufferDesc.get());
}

bool PictureExternal::setDescExternal(const LCEVC_PictureDesc& newDesc,
                                      const LCEVC_PicturePlaneDesc* newPlaneDescArr,
                                      const LCEVC_PictureBufferDesc* newBufferDesc)
{
    // Check for changes, then bind, THEN set desc.

    if (descsMatch(newDesc, newPlaneDescArr, newBufferDesc)) {
        return true;
    }

    const PictureInterleaving::Enum ilv = fromLCEVCDescInterleaving(newDesc.colorFormat);
    const PictureFormat::Enum fmt = fromLCEVCDescColorFormat(newDesc.colorFormat);
    const uint32_t numPlanes = PictureFormat::NumPlanes(fmt, ilv);
    if (!bindMemoryBufferAndPlanes(numPlanes, newPlaneDescArr, newBufferDesc)) {
        VNLogError("Failed to bind memory for external picture at %p\n", this);
        return false;
    }

    // If there's a manual stride, set it up:
    std::unique_ptr<uint32_t[]> planeStridesBytes = nullptr;
    if (newPlaneDescArr != nullptr) {
        planeStridesBytes = std::make_unique<uint32_t[]>(PictureFormatDesc::kMaxNumPlanes);
        for (uint32_t planeIdx = 0; planeIdx < numPlanes; planeIdx++) {
            planeStridesBytes[planeIdx] = newPlaneDescArr[planeIdx].rowByteStride;
        }
    }

    return setDesc(newDesc, planeStridesBytes.get());
}

uint8_t* PictureExternal::internalGetPlaneFirstSample(uint32_t planeIdx) const
{
    if (m_planeDescs != nullptr) {
        return (*m_planeDescs)[planeIdx].firstSample;
    }

    return BaseClass::internalGetPlaneFirstSample(planeIdx);
}

bool PictureExternal::getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const
{
    if (m_bufferDesc == nullptr) {
        return false;
    }

    const PictureBufferDesc& desc = *m_bufferDesc;
    bufferDescOut = {desc.data, desc.byteSize, {desc.accelBuffer.handle}, static_cast<LCEVC_Access>(desc.access)};
    return true;
}

bool PictureExternal::getPlaneDescArr(PicturePlaneDesc planeDescArrOut[kMaxNumPlanes]) const
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

bool PictureExternal::bindMemoryBufferAndPlanes(uint32_t numPlanes,
                                                const LCEVC_PicturePlaneDesc* planeDescArr,
                                                const LCEVC_PictureBufferDesc* bufferDesc)
{
    if (!BaseClass::bindMemory()) {
        return false;
    }

    // This should have already been validated. Normally, non-null is communicated by references,
    // so we wouldn't need this assert
    VNAssert(bufferDesc != nullptr || planeDescArr != nullptr);

    // If we're rebinding, we need to reset our descs (for example, if we used to have a bufferDesc
    // but the client no longer wants us to know the bufferDesc).
    m_bufferDesc = nullptr;
    m_planeDescs = nullptr;

    if (bufferDesc != nullptr) {
        // Can't use make_unique (before C++20), it doesn't pass brace-initializers to structs.
        // NOLINTNEXTLINE(modernize-make-unique)
        m_bufferDesc = std::unique_ptr<PictureBufferDesc>(new PictureBufferDesc{
            bufferDesc->data, bufferDesc->byteSize, bufferDesc->accelBuffer.hdl, bufferDesc->access});
    }

    if (planeDescArr != nullptr) {
        m_planeDescs = std::make_unique<std::array<PicturePlaneDesc, kMaxNumPlanes>>();

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
    unlock();
    unbindMemory();
}

bool PictureManaged::getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const
{
    if (m_buffer == nullptr) {
        return false;
    }

    bufferDescOut.data = m_buffer->data();
    bufferDescOut.byteSize = static_cast<uint32_t>(m_buffer->size() * sizeof((*m_buffer)[0]));
    bufferDescOut.accelBuffer.hdl = kInvalidHandle;
    bufferDescOut.access = LCEVC_Access_Unknown;
    return true;
}

bool PictureManaged::getPlaneDescArr(PicturePlaneDesc planeDescArrOut[kMaxNumPlanes]) const
{
    for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
        planeDescArrOut[planeIdx].firstSample = internalGetPlaneFirstSample(planeIdx);
        planeDescArrOut[planeIdx].rowByteStride = getPlaneByteStride(planeIdx);
    }
    return true;
}

bool PictureManaged::descsMatch(const LCEVC_PictureDesc& newDesc)
{
    LCEVC_PictureDesc curDesc;
    getDesc(curDesc);
    return equals(newDesc, curDesc);
}

bool PictureManaged::setDesc(const LCEVC_PictureDesc& newDesc)
{
    // Check for changes, then set descs, THEN bind (based on new descs).
    if (descsMatch(newDesc)) {
        return true;
    }

    if (!BaseClass::setDesc(newDesc)) {
        return false;
    }

    if (!unbindMemory()) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Failed to unbind old memory for managed picture <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
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
        VNLogError("CC %u, PTS %" PRId64 ": Binding to nothing. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
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
    VNLogVerbose("CC %u, PTS %" PRId64 ": Allocated %" PRId64
                 " total bytes. Picture full description: <%s>\n",
                 timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                 requiredSize, toString().c_str());
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

} // namespace lcevc_dec::decoder
