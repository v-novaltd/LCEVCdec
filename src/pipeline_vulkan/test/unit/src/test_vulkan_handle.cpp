/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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

#include "vulkan_handle.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace lcevc_dec::pipeline_vulkan { namespace {

    struct CountingDeleter
    {
        void operator()(VkDevice, uint32_t handle) const noexcept
        {
            ++destroyCount;
            lastDestroyed = handle;
        }

        static uint32_t destroyCount;
        static uint32_t lastDestroyed;
    };

    uint32_t CountingDeleter::destroyCount{};
    uint32_t CountingDeleter::lastDestroyed{};

    using TestHandle = UniqueDeviceHandle<uint32_t, CountingDeleter>;

    static_assert(!std::is_copy_constructible_v<TestHandle>);
    static_assert(!std::is_copy_assignable_v<TestHandle>);
    static_assert(std::is_nothrow_move_constructible_v<TestHandle>);
    static_assert(std::is_nothrow_move_assignable_v<TestHandle>);

    class VulkanHandleTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            CountingDeleter::destroyCount = 0;
            CountingDeleter::lastDestroyed = 0;
        }
    };

    TEST_F(VulkanHandleTest, ResetDestroysOnce)
    {
        TestHandle handle;
        *handle.put(VkDevice{}) = 7;

        handle.reset();
        handle.reset();

        EXPECT_FALSE(handle);
        EXPECT_EQ(CountingDeleter::destroyCount, 1);
        EXPECT_EQ(CountingDeleter::lastDestroyed, 7);
    }

    TEST_F(VulkanHandleTest, MoveConstructionTransfersOwnership)
    {
        TestHandle source;
        *source.put(VkDevice{}) = 11;

        TestHandle destination(std::move(source));

        EXPECT_FALSE(source);
        EXPECT_TRUE(destination);
        EXPECT_EQ(destination.get(), 11);

        destination.reset();
        EXPECT_EQ(CountingDeleter::destroyCount, 1);
        EXPECT_EQ(CountingDeleter::lastDestroyed, 11);
    }

    TEST_F(VulkanHandleTest, MoveAssignmentReleasesPreviousObject)
    {
        TestHandle source;
        *source.put(VkDevice{}) = 13;
        TestHandle destination;
        *destination.put(VkDevice{}) = 17;

        destination = std::move(source);

        EXPECT_FALSE(source);
        EXPECT_TRUE(destination);
        EXPECT_EQ(destination.get(), 13);
        EXPECT_EQ(CountingDeleter::destroyCount, 1);
        EXPECT_EQ(CountingDeleter::lastDestroyed, 17);

        destination.reset();
        EXPECT_EQ(CountingDeleter::destroyCount, 2);
        EXPECT_EQ(CountingDeleter::lastDestroyed, 13);
    }

}} // namespace lcevc_dec::pipeline_vulkan
