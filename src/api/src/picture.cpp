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

using namespace lcevc_dec::utility;

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
    uint32_t numBuffers = std::min(src.getBufferCount(), dest.getBufferCount());
    for (uint32_t i = 0; i < numBuffers; ++i) {
        LCEVC_PictureBufferDesc srcBufDesc;
        LCEVC_PictureBufferDesc destBufDesc;
        if (!src.getBufferDesc(i, srcBufDesc) || !dest.getBufferDesc(i, destBufDesc)) {
            VNLogError("CC %u, PTS %" PRId64
                       ": Failed to get buffer desc while copying one buffer to another\n",
                       timehandleGetCC(src.getTimehandle()),
                       timehandleGetTimestamp(src.getTimehandle()));
            return false;
        }

        simpleCopyPlaneBuffer(srcBufDesc.data, src.getPlaneByteStride(i), src.getPlaneWidthBytes(i),
                              src.getPlaneHeight(i), src.getPlaneMemorySize(i), destBufDesc.data,
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
    m_colorTransfer = source.m_colorTransfer;
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
    for (uint32_t bufferIdx = 0; bufferIdx < getBufferCount(); bufferIdx++) {
        if (getBuffer(bufferIdx) == nullptr) {
            return false;
        }
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
    desc.colorStandard = static_cast<LCEVC_ColorStandard>(toLCEVCColorStandard(m_desc.GetColorspace()));
    desc.colorTransfer = static_cast<LCEVC_ColorTransfer>(toLCEVCColorTransfer(m_colorTransfer));
    memcpy(&desc.hdrStaticInfo, &m_hdrStaticInfo, sizeof(HDRStaticInfo));
    desc.sampleAspectRatioDen = m_sampleAspectRatio.denominator;
    desc.sampleAspectRatioNum = m_sampleAspectRatio.numerator;
    desc.width = m_desc.GetWidth();
    desc.height = m_desc.GetHeight();
    desc.cropTop = m_crop.top;
    desc.cropBottom = m_crop.bottom;
    desc.cropLeft = m_crop.left;
    desc.cropRight = m_crop.right;
}

uint8_t* Picture::internalGetPlaneFirstSample(uint32_t planeIdx) const
{
    // This only works if planes are either all in one buffer, or all in different buffers.
    // TODO: Implement properly when PictureLayout struct is introduced
    uint32_t bufferIdx = 0;
    if (getBufferCount() > 1) {
        bufferIdx = std::min(planeIdx, getBufferCount() - 1);
    }
    uint8_t* out = getBuffer(bufferIdx);
    uint32_t firstPlaneInBuffer = bufferIdx; // just assume
    uint32_t numPrevPlanesInThisBuffer = planeIdx - firstPlaneInBuffer;
    if (numPrevPlanesInThisBuffer > 0) {
        for (uint32_t prevPlane = firstPlaneInBuffer; prevPlane < planeIdx; prevPlane++) {
            out += getPlaneMemorySize(prevPlane);
        }
    }
    return out;
}

uint8_t* Picture::getPlaneFirstSample(uint32_t planeIdx)
{
    return internalGetPlaneFirstSample(planeIdx);
}

const uint8_t* Picture::getPlaneFirstSample(uint32_t planeIdx) const
{
    return internalGetPlaneFirstSample(planeIdx);
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

bool Picture::setDesc(const LCEVC_PictureDesc& newDesc)
{
    bool dummyDescChanged = false;
    return setDesc(newDesc, dummyDescChanged);
}

bool Picture::setDesc(const LCEVC_PictureDesc& newDesc, bool& descChangedOut)
{
    if (!initializeDesc(newDesc, descChangedOut)) {
        VNLogError("CC %u, PTS %" PRId64 ": Invalid new desc for Picture <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    return true;
}

// Private functions:

void Picture::setName(const std::string& name) { m_name = "Picture:" + name; }

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

bool Picture::initializeDesc(const LCEVC_PictureDesc& desc, bool& descChangedOut)
{
    // Note that error messages in this function just uses the name, rather than the full debug
    // string. This is because the debug string reports format data that isn't meaningful until
    // AFTER initializeDesc succeeds.
    LCEVC_PictureDesc curDesc;
    getDesc(curDesc);
    if (equals(desc, curDesc)) {
        descChangedOut = false;
        return true;
    }
    descChangedOut = true;

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
    m_colorTransfer = fromLCEVCColorTransfer(desc.colorTransfer);
    memcpy(&m_hdrStaticInfo, &desc.hdrStaticInfo, sizeof(LCEVC_HDRStaticInfo));
    m_sampleAspectRatio = {desc.sampleAspectRatioNum, desc.sampleAspectRatioDen};

    const Colorspace::Enum colorspace = fromLCEVCColorStandard(desc.colorStandard);
    const uint32_t bitdepth = bitdepthFromLCEVCDescColorFormat(desc.colorFormat);
    if (!m_desc.Initialise(format, desc.width, desc.height, interleaving, colorspace, bitdepth)) {
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
                     ": [%d] S %dx%d size %d, Total Size: %d (buffer loc: %p)\n",
                     timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()), i,
                     m_desc.GetPlaneWidth(i), m_desc.GetPlaneHeight(i),
                     m_desc.GetPlaneMemorySize(i), totalSize, getBuffer());
    }
    return totalSize;
}

// - PictureExternal ------------------------------------------------------------------------------

PictureExternal::~PictureExternal()
{
    unlock();
    unbindMemory();
}

bool PictureExternal::canModify() const
{
    if (!BaseClass::canModify()) {
        return false;
    }

    for (uint32_t bufIdx = 0; bufIdx < m_bufferCount; bufIdx++) {
        if (m_bufferDescs[bufIdx].access == LCEVC_Access_Read) {
            return false;
        }
    }

    return true;
}

bool PictureExternal::setDesc(const LCEVC_PictureDesc& newDesc, bool& descChangedOut)
{
    if (!BaseClass::setDesc(newDesc, descChangedOut)) {
        return false;
    }
    if (!descChangedOut) {
        return true;
    }

    // We need to configure our picture desc to work with our buffer sizes. In particular, if the
    // buffer is SMALLER than the new desc, then we need to whine and fail. Meanwhile, if the
    // buffer is BIGGER than the new desc, we SHOULD keep track of that "padding" (although we
    // don't currently know the shape of the padding, so just emit an info).
    // TODO: track padding once PictureLayout struct is introduced
    uint32_t totalAllocatedBytes = 0;
    for (uint32_t bufIdx = 0; bufIdx < getBufferCount(); bufIdx++) {
        LCEVC_PictureBufferDesc bufDesc;
        getBufferDesc(bufIdx, bufDesc);
        totalAllocatedBytes += bufDesc.byteSize;
    }

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

    if (getRequiredSize() < totalAllocatedBytes) {
        VNLogInfo(
            "CC %u, PTS %" PRId64
            ": This picture has more space allocated than the description requires. Allocated %u "
            "but desc only requires %u. This will work, but padding hasn't yet been implemented: "
            "it is not currently possible to specify WHERE the unused memory is. Picture: <%s>.\n",
            timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
            totalAllocatedBytes, getRequiredSize(), getShortDbgString().c_str());
    }

    return true;
}

bool PictureExternal::getBufferDesc(uint32_t bufferIndex, LCEVC_PictureBufferDesc& bufferDescOut) const
{
    if (bufferIndex >= getBufferCount()) {
        return false;
    }

    const PictureBufferDesc& desc = m_bufferDescs[bufferIndex];
    bufferDescOut = {desc.data, desc.byteSize, {desc.accelBuffer.handle}, static_cast<LCEVC_Access>(desc.access)};
    return true;
}

bool PictureExternal::bindMemoryBuffers(uint32_t bufferCount, const LCEVC_PictureBufferDesc* bufferDescArr)
{
    if (!BaseClass::bindMemory()) {
        return false;
    }

    for (uint32_t idx = 0; idx < bufferCount; idx++) {
        const LCEVC_PictureBufferDesc& desc = bufferDescArr[idx];
        m_bufferDescs[idx] = {desc.data, desc.byteSize, {desc.accelBuffer.hdl}, desc.access};
    }
    m_bufferCount = bufferCount;

    return true;
}

bool PictureExternal::unbindMemory()
{
    if (!BaseClass::unbindMemory()) {
        return false;
    }

    for (uint32_t idx = 0; idx < m_bufferCount; idx++) {
        m_bufferDescs[idx] = {};
    }
    m_bufferCount = 0;
    return true;
}

// - PictureManaged -------------------------------------------------------------------------------

PictureManaged::~PictureManaged()
{
    unlock();
    unbindMemory();
}

bool PictureManaged::getBufferDesc(uint32_t bufferIndex, LCEVC_PictureBufferDesc& bufferDescOut) const
{
    if (bufferIndex >= getBufferCount()) {
        return false;
    }

    bufferDescOut.accelBuffer.hdl = kInvalidHandle;
    bufferDescOut.data = m_buffer->data();
    bufferDescOut.byteSize = static_cast<uint32_t>(m_buffer->size() * sizeof((*m_buffer)[0]));
    return true;
}

bool PictureManaged::setDesc(const LCEVC_PictureDesc& newDesc, bool& descChangedOut)
{
    if (!BaseClass::setDesc(newDesc, descChangedOut)) {
        return false;
    }
    if (!descChangedOut) {
        return true;
    }

    // Managed pictures also need to resize, i.e. unbind and rebind
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

    const uint32_t planesCount = m_desc.GetPlaneCount();
    for (uint32_t i = 0; i < planesCount; i++) {
        m_desc.SetPlaneStrides(i, getPlaneByteStride(i), getPlaneBytesPerPixel(i));
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
