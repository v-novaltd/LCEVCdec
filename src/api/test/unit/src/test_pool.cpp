/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

// This tests api/src/pool.h and api/src/decoder_pool.h

#include "utils.h"

#include <LCEVC/lcevc_dec.h>
#include <decoder.h>
#include <decoder_pool.h>
#include <gtest/gtest.h>
#include <handle.h>
#include <pool.h>

#include <future>

using namespace lcevc_dec::decoder;

class TestClass
{
public:
    TestClass() = delete;
    TestClass(int id, std::vector<int>& deleteListRef)
        : identifier(id)
        , deleteList(deleteListRef)
    {}

    ~TestClass()
    {
        // try-catch because destructors shouldn't throw exceptions (but we won't need it)
        try {
            deleteList.push_back(identifier);
        } catch (const std::bad_alloc&) {}
    };

    int identifier;
    std::vector<int>& deleteList;
};

class ChildClass : public TestClass
{};

class PoolFixture : public testing::Test
{
public:
    const int kPoolSize = 1;
    const int kTestIdentifier = 123;
    std::vector<int> destroyedObjs;
    Pool<TestClass> pool = Pool<TestClass>(kPoolSize);
};

TEST_F(PoolFixture, AllocValid)
{
    std::unique_ptr<TestClass> ptr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
    uintptr_t handle = pool.allocate(std::move(ptr)).handle;
    EXPECT_TRUE(pool.isValid(handle));
    TestClass* objRet = pool.lookup(handle);
    EXPECT_EQ(kTestIdentifier, objRet->identifier);
}

TEST_F(PoolFixture, AllocInvalid)
{
    std::unique_ptr<TestClass> ptr = nullptr;
    uintptr_t handle = pool.allocate(std::move(ptr)).handle;
    EXPECT_EQ(pool.isValid(handle), false);
}

TEST_F(PoolFixture, DoubleAllocInvalid)
{
    std::unique_ptr<TestClass> ptr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
    uintptr_t handle = pool.allocate(std::move(ptr)).handle;
    EXPECT_TRUE(pool.isValid(handle));
    std::unique_ptr<TestClass> secondPtr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
    uintptr_t secondHandle = pool.allocate(std::move(secondPtr)).handle;
    EXPECT_FALSE(pool.isValid(secondHandle));
}

TEST(PoolTest, DeleteValid)
{
    std::vector<int> destroyedObjs;
    const int kTestIdentifier = 123;
    {
        Pool<TestClass> pool = Pool<TestClass>(1);
        std::unique_ptr<TestClass> ptr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
        pool.allocate(std::move(ptr));
        EXPECT_EQ(destroyedObjs.size(), 0);
    }
    EXPECT_EQ(destroyedObjs.size(), 1);
    if (!destroyedObjs.empty()) {
        EXPECT_EQ(destroyedObjs[0], kTestIdentifier);
    }
}

using PoolFixtureDeathTest = PoolFixture;
TEST_F(PoolFixtureDeathTest, ReleaseValid)
{
    std::unique_ptr<TestClass> ptr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
    uintptr_t handle = pool.allocate(std::move(ptr)).handle;
    pool.release(handle);
    EXPECT_EQ(pool.isValid(handle), false);
    VN_EXPECT_DEATH(pool.lookup(handle), "Assertion .* failed", nullptr);
    EXPECT_EQ(destroyedObjs.size(), 1);
    if (!destroyedObjs.empty()) {
        EXPECT_EQ(destroyedObjs[0], kTestIdentifier);
    }
}

TEST(HandleTest, HandleValid)
{
    uintptr_t ptr = 0;
    Handle<TestClass> hdl(ptr);
    EXPECT_EQ(hdl, ptr);
    EXPECT_EQ(hdl.handle, ptr);
}

TEST(HandleTest, ConversionValid)
{
    uintptr_t ptr = 0;
    Handle<ChildClass> childHdl(ptr);
    Handle<TestClass> parentHdl(childHdl);
    EXPECT_EQ(parentHdl.handle, childHdl.handle);
}

TEST(HandleTest, AssignmentValid)
{
    uintptr_t ptr = 0;
    Handle<ChildClass> childHdl(ptr);
    const Handle<TestClass> parentHdl = childHdl;
    EXPECT_EQ(parentHdl.handle, childHdl.handle);
}

uintptr_t allocPool(std::chrono::high_resolution_clock::time_point resumeTime)
{
    LCEVC_DecoderHandle invalidDecoderHdl{kInvalidHandle};
    LCEVC_AccelContextHandle invalidAccelContextHdl{kInvalidHandle};
    std::unique_ptr<Decoder> ptr = std::make_unique<Decoder>(invalidAccelContextHdl, invalidDecoderHdl);
    std::this_thread::sleep_until(resumeTime);
    return DecoderPool::get().allocate(std::move(ptr)).handle;
}

TEST(DecoderPoolTest, DecoderPoolThreadedAlloc)
{
    const size_t numDecoders = 2;
    std::vector<uintptr_t> decoderHdls(numDecoders);
    std::vector<std::future<uintptr_t>> allocThreads(numDecoders);
    // Setting resumeTime to be 10ms in the future so that the two threads try and allocate in the
    // pool at exactly the same time to force the mutex to be exercised, not perfect but generally
    // runs the allocate()s within <100us of each other
    std::chrono::high_resolution_clock::time_point resumeTime =
        std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(10);

    for (uint32_t i = 0; i < numDecoders; i++) {
        allocThreads[i] = std::async(&allocPool, resumeTime);
    }
    for (uint32_t i = 0; i < numDecoders; i++) {
        decoderHdls[i] = allocThreads[i].get();
    }
    EXPECT_NE(decoderHdls[0], decoderHdls[1]);
}

Decoder* lookupPool(uintptr_t ptr, std::chrono::high_resolution_clock::time_point resumeTime)
{
    std::this_thread::sleep_until(resumeTime);
    return DecoderPool::get().lookup(ptr);
}

TEST(DecoderPoolTest, DecoderPoolThreadedLookup)
{
    LCEVC_DecoderHandle invalidDecoderHdl{kInvalidHandle};
    LCEVC_AccelContextHandle invalidAccelContextHdl{kInvalidHandle};
    const size_t numDecoders = 2;
    std::vector<uintptr_t> decoderHdls(numDecoders);
    std::vector<std::future<Decoder*>> lookupThreads(numDecoders);
    std::vector<Decoder*> returnedDecoders(numDecoders);
    // See DecoderPoolThreadedAlloc for resumeTime explanation
    std::chrono::high_resolution_clock::time_point resumeTime =
        std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(10);

    for (uint32_t i = 0; i < numDecoders; i++) {
        std::unique_ptr<Decoder> ptr =
            std::make_unique<Decoder>(invalidAccelContextHdl, invalidDecoderHdl);
        decoderHdls[i] = DecoderPool::get().allocate(std::move(ptr)).handle;
    }
    for (uint32_t i = 0; i < numDecoders; i++) {
        lookupThreads[i] = std::async(&lookupPool, decoderHdls[i], resumeTime);
    }
    for (uint32_t i = 0; i < numDecoders; i++) {
        returnedDecoders[i] = lookupThreads[i].get();
    }
    EXPECT_NE(returnedDecoders[0], returnedDecoders[1]);
}
