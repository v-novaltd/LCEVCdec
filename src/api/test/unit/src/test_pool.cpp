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

// This tests api/src/pool.h and api/src/decoder_pool.h

#include "utils.h"

#include <gtest/gtest.h>
#include <handle.h>
#include <LCEVC/lcevc_dec.h>
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
    uintptr_t handle = pool.add(std::move(ptr)).handle;
    EXPECT_TRUE(pool.isValid(handle));
    TestClass* objRet = pool.lookup(handle);
    EXPECT_EQ(kTestIdentifier, objRet->identifier);
}

TEST_F(PoolFixture, AllocInvalid)
{
    std::unique_ptr<TestClass> ptr = nullptr;
    uintptr_t handle = pool.add(std::move(ptr)).handle;
    EXPECT_EQ(pool.isValid(handle), false);
}

TEST_F(PoolFixture, DoubleAllocInvalid)
{
    std::unique_ptr<TestClass> ptr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
    uintptr_t handle = pool.add(std::move(ptr)).handle;
    EXPECT_TRUE(pool.isValid(handle));
    std::unique_ptr<TestClass> secondPtr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
    uintptr_t secondHandle = pool.add(std::move(secondPtr)).handle;
    EXPECT_FALSE(pool.isValid(secondHandle));
}

TEST(PoolTest, DeleteValid)
{
    std::vector<int> destroyedObjs;
    const int kTestIdentifier = 123;
    {
        Pool<TestClass> pool = Pool<TestClass>(1);
        std::unique_ptr<TestClass> ptr = std::make_unique<TestClass>(kTestIdentifier, destroyedObjs);
        TestClass* rawPtr = ptr.get();
        pool.add(std::move(ptr));
        EXPECT_EQ(destroyedObjs.size(), 0);

        std::unique_ptr<TestClass> rptr{pool.remove(pool.reverseLookup(rawPtr))};
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
    uintptr_t handle = pool.add(std::move(ptr)).handle;
    std::unique_ptr<TestClass> rptr{pool.remove(handle)};
    rptr.reset();
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
