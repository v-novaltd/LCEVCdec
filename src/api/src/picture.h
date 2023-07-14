/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_PICTURE_H_
#define VN_API_PICTURE_H_

#include "handle.h"
#include "interface.h"
#include "uPictureFormatDesc.h"
#include "uTimestamps.h"
#include "uTypes.h"

#include <array>
#include <memory>
#include <string>

// ------------------------------------------------------------------------------------------------

struct LCEVC_PictureBufferDesc;
struct LCEVC_PictureDesc;
struct perseus_image;

namespace lcevc_dec::decoder {

class BufferManager;
class Decoder;
class Picture;
class PictureLock;

using PictureBuffer = lcevc_dec::utility::DataBuffer;

// - Picture --------------------------------------------------------------------------------------

class Picture
{
public:
    Picture() = default;
    virtual ~Picture() = 0;
    Picture& operator=(const Picture&) = delete;
    Picture& operator=(Picture&&) = delete;
    Picture(const Picture&) = delete;
    Picture(Picture&&) = delete;

    bool copyMetadata(const Picture& source);
    bool copyData(const Picture& source);
    bool toCoreImage(perseus_image& dest);

    bool isValid() const;

    virtual bool isManaged() const = 0;

    // Getters and setters.
    // clang-format off: Want inline getters, but they bloat the header if they take multiple lines

    std::string getShortDbgString() const;
    std::string toString() const;

    bool getPublicFlag(uint8_t flag) const;
    void setPublicFlag(uint8_t flag, bool value);

    // setDesc is lazy: does nothing if requested desc equals new desc
    void getDesc(LCEVC_PictureDesc& descOut) const;
    bool setDesc(const LCEVC_PictureDesc& newDesc);

    uint32_t getWidth() const { return m_desc.GetWidth() - (m_crop.left + m_crop.right); }
    uint32_t getHeight() const { return m_desc.GetHeight() - (m_crop.top + m_crop.bottom); }
    uint32_t getBitdepth() const { return m_desc.GetBitDepth(); }
    uint32_t getBytedepth() const { return m_desc.GetByteDepth(); }
    uint32_t getNumPlanes() const { return m_desc.GetPlaneCount(); }
    uint32_t getPlaneWidth(uint32_t planeIndex) const { return m_desc.GetPlaneWidth(planeIndex); }
    uint32_t getPlaneHeight(uint32_t planeIndex) const { return m_desc.GetPlaneHeight(planeIndex); }
    uint32_t getPlaneWidthBytes(uint32_t planeIndex) const
    {
        return m_desc.GetPlaneWidthBytes(planeIndex);
    }
    uint32_t getPlaneBytesPerPixel(uint32_t planeIndex) const
    {
        // Bytes per pixel, where "UVUVUV" is considered 3 pixels wide. So, that's samples per
        // pixel times bytes per sample.
        return m_desc.GetPlanePixelStride(planeIndex) * m_desc.GetByteDepth();
    }
    uint32_t getPlaneByteStride(uint32_t planeIndex) const
    {
        // Bytes per row for this plane (also called row byte stride).
        return m_desc.GetPlaneStrideBytes(planeIndex);
    }
    uint32_t getPlaneSampleStride(uint32_t planeIndex) const
    {
        // Samples per row for this plane (if not interleaved, this equals planePixelStride).
        // (bytes/row) / (bytes/sample) = samples/row
        // An equivalent formula would be (samples/pixel)*(pixels/row)=samples/row, like so:
        // return m_pictureDesc.GetPlaneStridePixels(i) * m_pictureDesc.GetPlanePixelStride(i);
        return m_desc.GetPlaneStrideBytes(planeIndex) / m_desc.GetByteDepth();
    }
    uint32_t getPlaneMemorySize(uint32_t planeIndex) const
    {
        return m_desc.GetPlaneMemorySize(planeIndex);
    }

    uint8_t* getPlaneFirstSample(uint32_t planeIdx);
    const uint8_t* getPlaneFirstSample(uint32_t planeIdx) const;

    virtual uint32_t getBufferCount() const = 0;
    virtual bool getBufferDesc(uint32_t bufferIndex, LCEVC_PictureBufferDesc& bufferDescOut) const = 0;

    void* getUserData() const { return m_userData; }
    void setUserData(void* userData) { m_userData = userData; }
    uint64_t getTimehandle() const { return m_timehandle; }
    void setTimehandle(uint64_t timehandle) { m_timehandle = timehandle; }
    // clang-format on

    // Access management
    bool lock(Access newAccess, Handle<PictureLock> newLock);
    Handle<PictureLock> getLock() const { return m_lock; }
    bool unlock();
    virtual bool canModify() const { return !isLocked(); }

    // Memory allocation

    virtual bool bindMemory();
    virtual bool unbindMemory();

protected:
    void setName(const std::string& name);

    bool isLocked() const { return m_lock != kInvalidHandle; }

    virtual bool setDesc(const LCEVC_PictureDesc& newDesc, bool& descChangedOut);

    // These are strictly internal, so it's fine to return non-const data of a const Picture.
    uint8_t* internalGetPlaneFirstSample(uint32_t planeIdx) const;
    virtual uint8_t* getBuffer(uint32_t bufferIdx = 0) const = 0;

    uint32_t getRequiredSize() const;

    lcevc_dec::utility::PictureFormatDesc m_desc = {};

private:
    bool initializeDesc(const LCEVC_PictureDesc& desc, bool& descChangedOut);

    // Identifying data
    uint64_t m_timehandle = lcevc_dec::utility::kInvalidTimehandle;
    void* m_userData = nullptr;
    std::string m_name = "unknown";

    // Format information
    ColorRange m_colorRange = ColorRange::Unknown;
    ColorTransfer m_colorTransfer = ColorTransfer::Unknown;
    uint8_t m_publicFlags = 0;
    HDRStaticInfo m_hdrStaticInfo = {};
    AspectRatio m_sampleAspectRatio = {1, 1};
    lcevc_dec::utility::Margins m_crop = {};

    // State
    Handle<PictureLock> m_lock = kInvalidHandle;
};

class PictureExternal : public Picture
{
    using BaseClass = Picture;

public:
    PictureExternal() = default;
    ~PictureExternal() override;
    PictureExternal& operator=(const PictureExternal&) = delete;
    PictureExternal& operator=(PictureExternal&&) = delete;
    PictureExternal(const PictureExternal&) = delete;
    PictureExternal(PictureExternal&&) = delete;

    bool isManaged() const final { return false; }
    bool setDesc(const LCEVC_PictureDesc& newDesc) { return BaseClass::setDesc(newDesc); }

    uint32_t getBufferCount() const final { return m_bufferCount; }
    bool getBufferDesc(uint32_t bufferIndex, LCEVC_PictureBufferDesc& bufferDescOut) const final;

    // It is technically possibly to set individual buffers as modifiable/unmodifiable, so we may
    // eventually want to allow this function to test on a per-buffer basis.
    bool canModify() const final;

    bool bindMemoryBuffers(uint32_t bufferCount, const LCEVC_PictureBufferDesc* bufferDescArr);

protected:
    bool setDesc(const LCEVC_PictureDesc& newDesc, bool& descChangedOut) final;

    uint8_t* getBuffer(uint32_t bufferIdx) const final { return m_bufferDescs[bufferIdx].data; }

    bool unbindMemory() final;

private:
    std::array<PictureBufferDesc, lcevc_dec::utility::PictureFormatDesc::kMaxNumPlanes> m_bufferDescs = {};
    uint32_t m_bufferCount = 0;
};

class PictureManaged : public Picture
{
    using BaseClass = Picture;

public:
    PictureManaged(BufferManager& bufferManagerHandle)
        : m_bufferManagerHandle(bufferManagerHandle)
    {}
    ~PictureManaged() override;
    PictureManaged& operator=(const PictureManaged&) = delete;
    PictureManaged& operator=(PictureManaged&&) = delete;
    PictureManaged(const PictureManaged&) = delete;
    PictureManaged(PictureManaged&&) = delete;

    bool isManaged() const final { return true; }
    bool setDesc(const LCEVC_PictureDesc& newDesc) { return BaseClass::setDesc(newDesc); }

    uint32_t getBufferCount() const final { return (m_buffer == nullptr ? 0 : 1); }
    bool getBufferDesc(uint32_t bufferIndex, LCEVC_PictureBufferDesc& bufferDescOut) const final;

protected:
    bool bindMemory() final; // hide this away (managed pictures bind memory in setDesc)
    bool unbindMemory() final;
    bool setDesc(const LCEVC_PictureDesc& newDesc, bool& descChangedOut) final;

    // BufferIdx unused because managed images only have 1 buffer.
    uint8_t* getBuffer(uint32_t) const final { return (m_buffer ? m_buffer->data() : nullptr); }

private:
    // This is a reference to the BufferManager which we want to manage our buffers for us.
    BufferManager& m_bufferManagerHandle;

    PictureBuffer* m_buffer = nullptr;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_PICTURE_H_
