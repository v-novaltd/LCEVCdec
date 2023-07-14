/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

// This tests api/src/picture.h

#include "utils.h"

// Define this so that we can reach both the internal headers (which are NOT in a dll) AND some
// handy functions from the actual interface of the API (which normally WOULD be in a dll).
#define VNDisablePublicAPI

#include <LCEVC/PerseusDecoder.h>
#include <LCEVC/lcevc_dec.h>
#include <buffer_manager.h>
#include <gtest/gtest.h>
#include <interface.h>
#include <picture.h>
#include <uPictureFormatDesc.h>

#include <algorithm>

// - Usings and consts ----------------------------------------------------------------------------

using namespace lcevc_dec::decoder;
using namespace lcevc_dec::utility;

static const uint32_t kBigRes[2] = {1920, 1080};
static const uint32_t kSmallRes[2] = {960, 540};
// Expected size for I420_8:
static const uint32_t kBigByteSize = kBigRes[0] * kBigRes[1] * 3 / 2;

static const LCEVC_HDRStaticInfo kNonsenseHDRInfo = {
    4, 120, 34, 81, 104, 29, 9323, 1085, 245, 102, 62, 101,
};

static const uint32_t kBytesIn12Bits = 2;

static const uint8_t kYUVValues[kI420NumPlanes] = {'V', '-', 'N'};

// - Helper functions -----------------------------------------------------------------------------

static bool initPic(Picture& pic, std::vector<SmartBuffer>& buffersOut, LCEVC_ColorFormat format,
                    uint32_t width, uint32_t height, LCEVC_AccelBufferHandle accelBufferHandle,
                    LCEVC_Access access)
{
    if (!pic.isManaged()) {
        if (!initPictureExternalAndBuffers(static_cast<PictureExternal&>(pic), buffersOut, format,
                                           width, height, accelBufferHandle, access)) {
            return false;
        }
    }

    LCEVC_PictureDesc desc = {};
    LCEVC_DefaultPictureDesc(&desc, format, width, height);
    if (!pic.setDesc(desc)) {
        return false;
    }

    return true;
}

static void bind(Picture& pic, uint32_t numBuffers, const LCEVC_PictureBufferDesc* buffers)
{
    if (!pic.isManaged()) {
        static_cast<PictureExternal&>(pic).bindMemoryBuffers(numBuffers, buffers);
    }
}

// - Fixtures -------------------------------------------------------------------------------------

template <typename PicType>
class PictureFixture : public testing::Test
{
public:
    PictureFixture();
    void bindIfNecessary() { bind(static_cast<Picture&>(m_pic), kI420NumPlanes, m_bufferDescs); }

    void SetUp() override
    {
        // Can't use initMaybeExternalPic, since we want to track the buffer descs too, to test
        // against later.
        setupBuffers(m_externalBuffers, LCEVC_I420_8, kBigRes[0], kBigRes[1]);
        setupBufferDesc(m_bufferDescs, m_externalBuffers, LCEVC_AccelBufferHandle{kInvalidHandle},
                        LCEVC_Access_Modify);
        bindIfNecessary();
    }

    bool setDesc()
    {
        LCEVC_PictureDesc defaultDesc;
        LCEVC_DefaultPictureDesc(&defaultDesc, LCEVC_I420_8, kBigRes[0], kBigRes[1]);
        return m_pic.setDesc(defaultDesc);
    }

    VNInline PicType constructPic();

protected:
    // for managed pics:
    BufferManager m_BufMan;

    // for external pics:
    std::vector<SmartBuffer> m_externalBuffers;
    LCEVC_PictureBufferDesc m_bufferDescs[kMaxNumPlanes] = {};

    PicType m_pic;
};

template <>
PictureFixture<PictureManaged>::PictureFixture()
    : m_pic(m_BufMan)
{}
template <>
PictureFixture<PictureExternal>::PictureFixture() = default;

template <>
VNInline PictureManaged PictureFixture<PictureManaged>::constructPic()
{
    return PictureManaged(m_BufMan);
}

template <>
VNInline PictureExternal PictureFixture<PictureExternal>::constructPic()
{
    return PictureExternal();
}

using PicTypes = ::testing::Types<PictureManaged, PictureExternal>;
TYPED_TEST_SUITE(PictureFixture, PicTypes);

using PicManFixture = PictureFixture<PictureManaged>;
using PicExtFixture = PictureFixture<PictureExternal>;

// - Tests ----------------------------------------------------------------------------------------

// - PictureExternal ----------------------------

// TODO: introduce tests for padding, once the Picture class has a way to take padding/stride data

TEST(pictureExternal, isManaged)
{
    PictureExternal pic;
    EXPECT_FALSE(pic.isManaged());
}

TEST_F(PicExtFixture, bindMemory)
{
    // Does it say that it succeeds?
    EXPECT_TRUE(m_pic.bindMemoryBuffers(static_cast<uint32_t>(m_externalBuffers.size()), m_bufferDescs));

    // Did it ACTUALLY succeed? This only checks that the picture has the right number of
    // 0-initialised pixels. A more thorough test (using arbitrary pixel values) is part of the
    // copyData test.
    for (uint32_t plane = 0; plane < m_pic.getNumPlanes(); plane++) {
        EXPECT_EQ(m_pic.getPlaneFirstSample(plane), m_bufferDescs[plane].data);
        EXPECT_EQ(memcmp(m_pic.getPlaneFirstSample(plane), m_bufferDescs[plane].data,
                         m_bufferDescs[plane].byteSize),
                  0);
    }
}

TEST_F(PicExtFixture, validSetDesc)
{
    m_pic.bindMemoryBuffers(static_cast<uint32_t>(m_externalBuffers.size()), m_bufferDescs);
    // Succeed if desc is equal or smaller, even if it's a mismatched type (e.g. switching from
    // a I420 at high-res to an NV12 at low-res)
    LCEVC_PictureDesc descToSet = {};
    LCEVC_PictureDesc descToGet = {};
    ASSERT_EQ(LCEVC_DefaultPictureDesc(&descToSet, LCEVC_NV12_8, kSmallRes[0], kSmallRes[1]), LCEVC_Success);
    EXPECT_TRUE(m_pic.setDesc(descToSet));
    m_pic.getDesc(descToGet);
    EXPECT_TRUE(equals(descToGet, descToSet));
}

TEST_F(PicExtFixture, invalidSetDesc)
{
    m_pic.bindMemoryBuffers(static_cast<uint32_t>(m_externalBuffers.size()), m_bufferDescs);

    // Fail if desc is bigger than bound memory (e.g. same res, larger bitdepth)
    LCEVC_PictureDesc defaultDesc = {};
    ASSERT_EQ(LCEVC_DefaultPictureDesc(&defaultDesc, LCEVC_I420_10_LE, kBigRes[0], kBigRes[1]),
              LCEVC_Success);
    EXPECT_FALSE(m_pic.setDesc(defaultDesc));
}

TEST_F(PicExtFixture, getBuffers)
{
    m_pic.bindMemoryBuffers(static_cast<uint32_t>(m_externalBuffers.size()), m_bufferDescs);
    setDesc();

    EXPECT_EQ(m_pic.getBufferCount(), m_externalBuffers.size());
    for (uint32_t buffer = 0; buffer < m_pic.getBufferCount(); buffer++) {
        LCEVC_PictureBufferDesc desc;
        EXPECT_TRUE(m_pic.getBufferDesc(buffer, desc));
        EXPECT_EQ(desc.accelBuffer.hdl, m_bufferDescs[buffer].accelBuffer.hdl);
        EXPECT_EQ(desc.access, m_bufferDescs[buffer].access);
        EXPECT_EQ(desc.byteSize, m_bufferDescs[buffer].byteSize);
        EXPECT_EQ(desc.data, m_bufferDescs[buffer].data);
    }
}

// - PictureManaged -----------------------------

TEST(pictureManaged, isManaged)
{
    BufferManager arbitraryBufferManager;
    PictureManaged pic(arbitraryBufferManager);
    EXPECT_TRUE(pic.isManaged());
}

// Note: no invalidSetDesc here (though there are some in the general PictureFixture). This is
// because Managed Pictures can just bind extra memory if you give them a too-large PictureDesc.
TEST_F(PicManFixture, validSetDesc)
{
    LCEVC_PictureDesc descToSet = {};
    LCEVC_PictureDesc descToGet = {};

    ASSERT_EQ(LCEVC_DefaultPictureDesc(&descToSet, LCEVC_NV12_8, kSmallRes[0], kSmallRes[1]), LCEVC_Success);
    EXPECT_TRUE(m_pic.setDesc(descToSet));
    m_pic.getDesc(descToGet);
    EXPECT_TRUE(equals(descToGet, descToSet));

    ASSERT_EQ(LCEVC_DefaultPictureDesc(&descToSet, LCEVC_I420_10_LE, kBigRes[0], kBigRes[1]), LCEVC_Success);
    EXPECT_TRUE(m_pic.setDesc(descToSet));
    m_pic.getDesc(descToGet);
    EXPECT_TRUE(equals(descToGet, descToSet));
}

TEST_F(PicManFixture, getBuffers)
{
    setDesc();

    // Managed pictures currently store all planes in one buffer.
    EXPECT_EQ(m_pic.getBufferCount(), 1);
    EXPECT_EQ(m_pic.getNumPlanes(), kI420NumPlanes);
    for (uint32_t buffer = 0; buffer < m_pic.getBufferCount(); buffer++) {
        LCEVC_PictureBufferDesc desc;
        EXPECT_TRUE(m_pic.getBufferDesc(buffer, desc));
        EXPECT_EQ(desc.byteSize, kBigByteSize);
    }
}

TEST(pictureManaged, bufferManagersDontOverlap)
{
    // Test that pictures with different buffer managers don't get buffers from each other's
    // manager. This can be done by creating two pictures, then releasing all buffers from one
    // buffer manager. The picture with the released buffer manager should have no buffer, while
    // the other should have a buffer as usual.
    BufferManager bufMan1;
    BufferManager bufMan2;
    PictureManaged pic1(bufMan1);
    PictureManaged pic2(bufMan2);

    LCEVC_PictureDesc defaultDesc;
    LCEVC_DefaultPictureDesc(&defaultDesc, LCEVC_I420_8, kBigRes[0], kBigRes[1]);
    pic1.setDesc(defaultDesc);
    pic2.setDesc(defaultDesc);

    bufMan1.release();

    LCEVC_PictureDesc altDesc;
    LCEVC_DefaultPictureDesc(&altDesc, LCEVC_NV12_8, kSmallRes[0], kSmallRes[1]);
    // Set desc should fail because it fails to unbind memory (because buffer is already gone).
    EXPECT_FALSE(pic1.setDesc(altDesc));
    // Set desc should succeed because it can unbind memory (because buffer still exists).
    EXPECT_TRUE(pic2.setDesc(altDesc));
}

// - Picture (i.e. managed and external) --------

TYPED_TEST(PictureFixture, setDescMatchesGet)
{
    // Provide a bunch of values that are definitely not the default values
    LCEVC_PictureDesc crazyDesc = {};
    crazyDesc.colorFormat = LCEVC_I420_12_LE;
    crazyDesc.colorRange = LCEVC_ColorRange_Limited;
    crazyDesc.colorStandard = LCEVC_ColorStandard_BT601_NTSC;
    crazyDesc.colorTransfer = LCEVC_ColorTransfer_ST2084;
    crazyDesc.cropBottom = 23;
    crazyDesc.cropLeft = 15;
    crazyDesc.cropRight = 97;
    crazyDesc.cropTop = 143;
    crazyDesc.hdrStaticInfo = kNonsenseHDRInfo;
    crazyDesc.height = 999 + crazyDesc.cropTop + crazyDesc.cropBottom;
    crazyDesc.sampleAspectRatioDen = 2;
    crazyDesc.sampleAspectRatioNum = 3;
    crazyDesc.width = 10 + crazyDesc.cropLeft + crazyDesc.cropRight;

    const uint32_t expectedHeight = crazyDesc.height - (crazyDesc.cropTop + crazyDesc.cropBottom);
    const uint32_t expectedWidth = crazyDesc.width - (crazyDesc.cropLeft + crazyDesc.cropRight);

    EXPECT_TRUE(this->m_pic.setDesc(crazyDesc));
    EXPECT_EQ(this->m_pic.getWidth(), expectedWidth);
    EXPECT_EQ(this->m_pic.getHeight(), expectedHeight);
    EXPECT_EQ(this->m_pic.getBitdepth(), 12); // LCEVC_I420_12_LE
    EXPECT_EQ(this->m_pic.getBytedepth(), kBytesIn12Bits);
    EXPECT_EQ(this->m_pic.getNumPlanes(), kI420NumPlanes);
    for (uint32_t planeIdx = 0; planeIdx < this->m_pic.getNumPlanes(); planeIdx++) {
        // I420 so chroma planes are half width and half height (rounded up). Note that these DON'T
        // take cropping into account (only the getWidth and getHeight functions do).
        const uint32_t expectedSampleStride =
            (planeIdx == 0 ? crazyDesc.width : (crazyDesc.width + 1) / 2);
        const uint32_t expectedByteStride = kBytesIn12Bits * expectedSampleStride;
        uint32_t expectedPlaneHeight = (planeIdx == 0 ? crazyDesc.height : (crazyDesc.height + 1) / 2);
        EXPECT_EQ(this->m_pic.getPlaneHeight(planeIdx), expectedPlaneHeight);
        EXPECT_EQ(this->m_pic.getPlaneBytesPerPixel(planeIdx),
                  kBytesIn12Bits); // would be double for nv12's chroma plane though.

        // Byte stride and width-in-bytes will be the same because we don't have any padding
        // (padding hasn't been implemented yet, see PictureExternal::setDesc).
        EXPECT_EQ(this->m_pic.getPlaneByteStride(planeIdx), expectedByteStride);
        EXPECT_EQ(this->m_pic.getPlaneWidthBytes(planeIdx), expectedByteStride);
        EXPECT_EQ(this->m_pic.getPlaneSampleStride(planeIdx), expectedSampleStride);

        EXPECT_EQ(this->m_pic.getPlaneMemorySize(planeIdx), expectedByteStride * expectedPlaneHeight);
    }
}

TYPED_TEST(PictureFixture, settersMatchGetters)
{
    this->m_pic.setTimehandle(123123);
    EXPECT_EQ(this->m_pic.getTimehandle(), 123123);
    this->m_pic.setUserData(this);
    EXPECT_EQ(this->m_pic.getUserData(), this);
    this->m_pic.setPublicFlag(LCEVC_PictureFlag_Interlaced, true);
    EXPECT_TRUE(this->m_pic.getPublicFlag(LCEVC_PictureFlag_Interlaced));
    EXPECT_FALSE(this->m_pic.getPublicFlag(LCEVC_PictureFlag_IDR));
}

TYPED_TEST(PictureFixture, invalidSetDesc)
{
    // Invalid crop
    LCEVC_PictureDesc defaultDesc = {};
    ASSERT_EQ(LCEVC_DefaultPictureDesc(&defaultDesc, LCEVC_I420_8, kBigRes[0], kBigRes[1]), LCEVC_Success);
    defaultDesc.cropBottom = kBigRes[1] * 2 / 3;
    defaultDesc.cropTop = kBigRes[1] * 2 / 3;
    EXPECT_FALSE(this->m_pic.setDesc(defaultDesc));

    // Invalid enum
    LCEVC_DefaultPictureDesc(&defaultDesc, LCEVC_I420_8, kBigRes[0], kBigRes[1]);
    defaultDesc.colorFormat = LCEVC_ColorFormat_Unknown;
    EXPECT_FALSE(this->m_pic.setDesc(defaultDesc));
}

TYPED_TEST(PictureFixture, copyData)
{
    // This is a little tough to test. What we want to do here is copy from one picture to another,
    // and check that the contents are the same. To simplify this, we set 3 magic numbers, which
    // are different for each plane, and unlikely to occur as junk memory.
    this->setDesc();

    // Init the source pic (make more challenging using NV12)
    TypeParam pic = this->constructPic();
    std::vector<SmartBuffer> nv12Buffers;
    initPic(pic, nv12Buffers, LCEVC_NV12_8, kBigRes[0], kBigRes[1],
            LCEVC_AccelBufferHandle{kInvalidHandle}, LCEVC_Access_Modify);

    // Fill the picture with data. We do cheeky memory mangling here to manually interleave
    {
        memset(pic.getPlaneFirstSample(0), kYUVValues[0], pic.getPlaneMemorySize(0));

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* ptrToPlane = reinterpret_cast<uint16_t*>(pic.getPlaneFirstSample(1));
        const uint16_t val = kYUVValues[2] << 8 | kYUVValues[1];
        const size_t count = pic.getPlaneMemorySize(1) / pic.getPlaneBytesPerPixel(1);
        std::fill_n(ptrToPlane, count, val);
    }

    // Now the actual copy, and check that it succeeded:
    EXPECT_TRUE(this->m_pic.copyData(pic));

    for (uint32_t plane = 0; plane < this->m_pic.getNumPlanes(); plane++) {
        const uint8_t* ptrToNV12 = pic.getPlaneFirstSample(std::min(plane, 1U));
        const uint8_t* ptrToI420 = this->m_pic.getPlaneFirstSample(plane);

        if (plane == 0) {
            EXPECT_EQ(memcmp(ptrToNV12, ptrToI420, this->m_pic.getPlaneMemorySize(plane) - 1), 0);
            continue;
        }

        // Can't really hack a memcmp thing so just manually iterate
        const uint8_t* const end = ptrToNV12 + pic.getPlaneMemorySize(plane);
        if (plane == 2) {
            ptrToNV12++;
        }

        while (ptrToNV12 < end) {
            EXPECT_EQ(*ptrToNV12, *ptrToI420);
            ptrToNV12 += 2;
            ptrToI420++;
        }
    }
}

TYPED_TEST(PictureFixture, toCoreImage)
{
    this->setDesc();
    perseus_image coreImg = {};
    EXPECT_TRUE(this->m_pic.toCoreImage(coreImg));

    // bitdepth
    uint8_t bitdepth = 0;
    EXPECT_TRUE(fromCoreBitdepth(coreImg.depth, bitdepth));
    EXPECT_EQ(bitdepth, this->m_pic.getBitdepth());

    // interleaving
    LCEVC_PictureDesc desc;
    this->m_pic.getDesc(desc);
    PictureFormat::Enum colorFormat = fromLCEVCDescColorFormat(desc.colorFormat);
    if (PictureFormat::IsRGB(colorFormat)) {
        EXPECT_TRUE(coreImg.ilv == PSS_ILV_RGB || coreImg.ilv == PSS_ILV_RGBA);
    } else {
        PictureInterleaving::Enum interleaving = fromLCEVCDescInterleaving(desc.colorFormat);
        EXPECT_EQ(interleaving == PictureInterleaving::NV12, coreImg.ilv == PSS_ILV_NV12);
    }

    // stride and contents
    for (uint32_t planeIdx = 0; planeIdx < this->m_pic.getNumPlanes(); planeIdx++) {
        EXPECT_EQ(memcmp(this->m_pic.getPlaneFirstSample(planeIdx), coreImg.plane[planeIdx],
                         this->m_pic.getPlaneMemorySize(planeIdx)),
                  0);
        EXPECT_EQ(this->m_pic.getPlaneSampleStride(planeIdx), coreImg.stride[planeIdx]);
    }
}

TYPED_TEST(PictureFixture, lock)
{
    // Sanity check that it's modifiable before locking
    EXPECT_TRUE(this->setDesc());

    // Handle doesn't actually matter here (generation of handles is tested elsewhere, in a test
    // called pictureLockPoolInterface). Just has to be something valid, and 0 is valid.
    Handle<PictureLock> lockHandle = 0;
    this->m_pic.lock(Access::Read, lockHandle);

    // Expect all modification to fail now, but it should still be possible to set it to the SAME
    // desc (trivial success)
    LCEVC_PictureDesc newNV12Desc;
    LCEVC_DefaultPictureDesc(&newNV12Desc, LCEVC_NV12_8, 540, 960);
    EXPECT_FALSE(this->m_pic.setDesc(newNV12Desc));
    EXPECT_TRUE(this->setDesc());

    this->m_pic.unlock();
    EXPECT_TRUE(this->m_pic.setDesc(newNV12Desc));
}
