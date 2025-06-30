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

#include "constants.h"

#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/sequencer/lcevc_container.h>

#include <algorithm>
#include <chrono>
#include <unordered_set>

using MilliSecond = std::chrono::duration<int64_t, std::milli>;
using TimePoint = std::chrono::high_resolution_clock::time_point;

// - Helper functions -----------------------------------------------------------------------------
TimePoint GetTimePoint() { return std::chrono::high_resolution_clock::now(); }

template <typename DurationType>
typename DurationType::rep GetTime()
{
    TimePoint now = GetTimePoint();
    DurationType ticks = std::chrono::duration_cast<DurationType>(now.time_since_epoch());
    return ticks.count();
}

bool isEvenNumberedFrame(uint64_t th)
{
    // The timestamps in our test data happen to be (78 + 4n)*10,000,000 where n is the frame
    // number. Therefore, divide by 10,000,000, subtract 78, divide by 4, and you have the frame
    // number.
    th /= 10000000;
    th -= 78;
    th /= 4;
    return (th % 2 == 0);
}

// - Fixtures -------------------------------------------------------------------------------------

class UnencapsulationTestFixture : public testing::Test
{
protected:
    // Virtual functions
    std::vector<uint8_t> unencapsulated;
    size_t unencapsulatedSize = 0;
    bool isIdr = false;
};

// Basic LCEVCContainer with no starting data.
class LCEVCContainerTestFixture : public testing::Test
{
protected:
    // Virtual functions
    void SetUp() override { SetUp(kContainerDefaultCapacity, kEmptyArray); }
    virtual void SetUp(size_t capacity, const std::vector<uint64_t>& timestampList)
    {
        allocator = ldcMemoryAllocatorMalloc();
        m_capacity = capacity;
        m_lcevcContainer = lcevcContainerCreate(allocator, &allocation, m_capacity);
        lcevcContainerSetMaxNumReorderFrames(m_lcevcContainer, m_maxNumReorderFrames);
        populate(timestampList);
    }
    void TearDown() override { lcevcContainerDestroy(m_lcevcContainer); }

    // Non-virtual helper functions
    void populate(const std::vector<uint64_t>& timestampList)
    {
        size_t limit = m_capacity;
        if (limit == 0) {
            limit = std::numeric_limits<size_t>::max();
        } else if (limit == std::numeric_limits<size_t>::max()) {
            limit = 0;
        }

        for (size_t i = 0; i < limit && i < timestampList.size(); i++) {
            addArbitraryData(timestampList[i], i);
        }
    }

    bool addArbitraryData(uint64_t timestamp, size_t index)
    {
        index = index % kLenRandLengths;
        const uint32_t bufLen = static_cast<uint32_t>(kRandLengths[index]);
        uint8_t* randomData = kRandData[index];
        return lcevcContainerInsertNoCopy(m_lcevcContainer, randomData, bufLen, timestamp, false,
                                          getTimeSinceStart());
    }

    void testOnEasyData(size_t start, size_t end, bool finishExtraction,
                        const std::vector<uint64_t>& srcData, std::set<uint64_t>& timestampsNotFound)
    {
        size_t firstSuccess = srcData.size();
        for (size_t i = start; i < end; i++) {
            addArbitraryData(srcData[i], i);

            uint64_t dummyTh = 0;
            size_t dummyQueueLen = 0;
            StampedBuffer* nextBufferInOrder =
                lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);
            if (i < m_deltaRepeatCount) {
                EXPECT_EQ(nextBufferInOrder, nullptr);
            } else if (i < firstSuccess) {
                if (nextBufferInOrder != nullptr) {
                    firstSuccess = i;
                }
            } else {
                EXPECT_NE(nextBufferInOrder, nullptr);
            }

            if (nextBufferInOrder != nullptr) {
                // Extra test: expect them to come out in the right order, starting at 0.
                EXPECT_EQ(stampedBufferGetTimestamp(nextBufferInOrder),
                          kSortedTimestamps[i - firstSuccess]);
                timestampsNotFound.erase(stampedBufferGetTimestamp(nextBufferInOrder));
                stampedBufferRelease(&nextBufferInOrder);
            }
        }
        EXPECT_LT(firstSuccess, m_maxNumReorderFrames);

        if (finishExtraction) {
            forceUntilEnd(timestampsNotFound);
        }
    }

    void recoverFromBadPatch(size_t start, size_t end, bool finishExtraction, bool expectFewForces,
                             const std::vector<uint64_t>& srcData, std::set<uint64_t>& timestampsNotFound)
    {
        size_t numForced = 0;
        uint64_t prevTH = 0;
        for (size_t i = start; i < end; i++) {
            addArbitraryData(srcData[i], i);

            uint64_t dummyTh = 0;
            size_t dummyQueueLen = 0;
            StampedBuffer* nextBufferInOrder =
                lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);

            if (nextBufferInOrder == nullptr) {
                nextBufferInOrder =
                    lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &dummyTh, &dummyQueueLen);
                ASSERT_NE(nextBufferInOrder, nullptr);
                numForced++;
            }

            EXPECT_GT(stampedBufferGetTimestamp(nextBufferInOrder), prevTH);
            prevTH = stampedBufferGetTimestamp(nextBufferInOrder);
            timestampsNotFound.erase(prevTH);
            stampedBufferRelease(&nextBufferInOrder);
        }

        if (finishExtraction) {
            forceUntilEnd(timestampsNotFound);
        }

        if (expectFewForces) {
            EXPECT_LT(numForced, m_maxNumReorderFrames);
        }
    }

    void forceUntilEnd(std::set<uint64_t>& timestampsNotFound)
    {
        uint64_t dummyTh = 0;
        size_t dummyQueueLen = 0;
        StampedBuffer* nextBuffer = nullptr;
        while ((nextBuffer = lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &dummyTh,
                                                              &dummyQueueLen)) != nullptr) {
            timestampsNotFound.erase(stampedBufferGetTimestamp(nextBuffer));
        }
    }

    // Member variables
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation allocation = {0};
    LCEVCContainer* m_lcevcContainer = nullptr;
    size_t m_capacity = UINT32_MAX;
    const uint32_t m_maxNumReorderFrames = 16; // Todo: try streams with other values.
    const uint32_t m_deltaRepeatCount = m_maxNumReorderFrames / 2;
};

class LCEVCContainerFixtureWithParam
    : public LCEVCContainerTestFixture
    , public testing::WithParamInterface<std::vector<uint64_t>>
{};

// LCEVCContainer which takes the first kContainerDefaultCapacity elements from a given
// parameterised timestamp list.
class LCEVCContainerTestFixturePreFillSome : public LCEVCContainerFixtureWithParam
{
    using PARENT = LCEVCContainerTestFixture;

protected:
    void SetUp() override { PARENT::SetUp(kContainerDefaultCapacity, GetParam()); }
};

// LCEVCContainer which takes all of the elements from the given timestamp list.
class LCEVCContainerTestFixturePreFillAll : public LCEVCContainerFixtureWithParam
{
    using PARENT = LCEVCContainerTestFixture;

protected:
    void SetUp() override { PARENT::SetUp(GetParam().size(), GetParam()); }
};

// The next 3 fixtures test behaviour for various capacities.
class LCEVCContainerTestFixture0Capacity : public LCEVCContainerFixtureWithParam
{
    using PARENT = LCEVCContainerTestFixture;

protected:
    void SetUp() override { PARENT::SetUp(UINT32_MAX, kEmptyArray); }
};

class LCEVCContainerTestFixtureUIntMaxCapacity : public LCEVCContainerFixtureWithParam
{
    using PARENT = LCEVCContainerTestFixture;

protected:
    void SetUp() override { PARENT::SetUp(0, kEmptyArray); }
};

class LCEVCContainerTestFixtureMediumCapacity : public LCEVCContainerFixtureWithParam
{
    using PARENT = LCEVCContainerTestFixture;

protected:
    void SetUp() override { PARENT::SetUp(kContainerDefaultCapacity / 2, kEmptyArray); }
};

// Combines the "preFillAll" fixture with the "no-capacity" behaviour.
class LCEVCContainerTestFixturePreFillAllNoCap : public LCEVCContainerFixtureWithParam
{
    using PARENT = LCEVCContainerTestFixture;

protected:
    void SetUp() override { PARENT::SetUp(UINT32_MAX, GetParam()); }
};

// - Tests ----------------------------------------------------------------------------------------

// Tests to make sure we can actually use the fixtures and constants

TEST(SequencerTestConstants, sortedTimestampsAreSorted)
{
    EXPECT_TRUE(std::is_sorted(kSortedTimestamps.begin(), kSortedTimestamps.end()));
}

TEST(SequencerTestLCEVCContainer, validCreateLCEVCContainer)
{
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation allocation = {0};
    allocator = ldcMemoryAllocatorMalloc();
    LCEVCContainer* lcevcContainer =
        lcevcContainerCreate(allocator, &allocation, kContainerDefaultCapacity);
    ASSERT_NE(lcevcContainer, nullptr);
    EXPECT_EQ(lcevcContainerSize(lcevcContainer), 0);

    lcevcContainerDestroy(lcevcContainer);
}

TEST_F(UnencapsulationTestFixture, validUnencapsulation)
{
    unencapsulated.resize(kEncapsulatedLength);
    EXPECT_EQ(lcevcContainerUnencapsulate(kEncapsulatedRandData, kEncapsulatedLength,
                                          unencapsulated.data(), &unencapsulatedSize, &isIdr),
              true);

    // Compare results against kRandData2 as kEncapsulatedRandData is an encapsulated kRandData2
    EXPECT_EQ(unencapsulatedSize, kRandLengths[1]);
    for (uint8_t index = 0; index < unencapsulatedSize; index++) {
        EXPECT_EQ(unencapsulated.data()[index], kRandData2[index]);
    }
    EXPECT_TRUE(isIdr);
}

TEST_F(UnencapsulationTestFixture, invalidUnencapsulation)
{
    unencapsulated.resize(kRandLengths[1]);
    EXPECT_EQ(lcevcContainerUnencapsulate(kRandData2, kRandLengths[1], unencapsulated.data(),
                                          &unencapsulatedSize, &isIdr),
              false);
    EXPECT_EQ(unencapsulatedSize, 0);
    EXPECT_FALSE(isIdr);
}

// Basic fixture tests on an initially empty LCEVCContainer

TEST_F(LCEVCContainerTestFixture, insertAddsTimestampProvided)
{
    // size from 0 to kMaxBufSize, inclusive
    const uint64_t timestamp = kTimestamps1[0];
    const size_t bufSize = kRandLengths[0];
    uint8_t* randomData = kRandData[0];
    lcevcContainerInsertNoCopy(m_lcevcContainer, randomData, bufSize, timestamp, false,
                               getTimeSinceStart());

    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), 1);

    bool dummyIsAtHead = false;
    EXPECT_TRUE(lcevcContainerExists(m_lcevcContainer, timestamp, &dummyIsAtHead));
}

TEST_F(LCEVCContainerTestFixture, removeSubtractsWhatWasAdded)
{
    // size from 0 to kMaxBufSize, inclusive
    const uint64_t timestamp = kTimestamps1[1];
    const size_t bufSize = kRandLengths[1];
    uint8_t* randomData = kRandData[1];
    const uint64_t inputTime = getTimeSinceStart();
    lcevcContainerInsertNoCopy(m_lcevcContainer, randomData, bufSize, timestamp, false, inputTime);

    const size_t oldSize = lcevcContainerSize(m_lcevcContainer);

    uint64_t retrievedTimestamp = 0;
    size_t queueSize = 0;
    StampedBuffer* releaseThis =
        lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &retrievedTimestamp, &queueSize);
    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), oldSize - 1);
    EXPECT_EQ(timestamp, retrievedTimestamp);
    EXPECT_EQ(stampedBufferGetTimestamp(releaseThis), timestamp);
    EXPECT_EQ(stampedBufferGetBufSize(releaseThis), bufSize);
    EXPECT_EQ(stampedBufferGetInsertTime(releaseThis), inputTime);

    // Note that we used lcevcContainerInsertNoCopy, NOT lcevcContainerInsert. So, we expect
    // the pointers to be to the same memory.
    EXPECT_EQ(stampedBufferGetBuffer(releaseThis), randomData);

    stampedBufferRelease(&releaseThis);
}

TEST_F(LCEVCContainerTestFixture, unencapsulateRemoveSubtractsWhatWasAdded)
{
    // size from 0 to kMaxBufSize, inclusive
    const uint64_t timestamp = kTimestamps1[1];
    const size_t bufSize = kEncapsulatedLength;
    uint8_t* randomData = kEncapsulatedRandData;
    const uint64_t inputTime = getTimeSinceStart();
    lcevcContainerInsert(m_lcevcContainer, randomData, bufSize, timestamp, true, inputTime);

    const size_t oldSize = lcevcContainerSize(m_lcevcContainer);

    uint64_t retrievedTimestamp = 0;
    size_t queueSize = 0;
    StampedBuffer* releaseThis =
        lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &retrievedTimestamp, &queueSize);
    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), oldSize - 1);
    EXPECT_EQ(timestamp, retrievedTimestamp);
    EXPECT_EQ(stampedBufferGetTimestamp(releaseThis), timestamp);
    EXPECT_EQ(stampedBufferGetInsertTime(releaseThis), inputTime);
    EXPECT_EQ(stampedBufferGetIDR(releaseThis), true);

    // Note that we used lcevcContainerInsert, NOT lcevcContainerInsertNoCopy. So, we expect and
    // unencapsulated copy, i.e. the pointers should be to different places
    EXPECT_NE(stampedBufferGetBuffer(releaseThis), randomData);
    // Compare results against kRandData2 as kEncapsulatedRandData is an encapsulated kRandData2
    EXPECT_EQ(stampedBufferGetBufSize(releaseThis), kRandLengths[1]);
    for (uint8_t index = 0; index < kRandLengths[1]; index++) {
        EXPECT_EQ(stampedBufferGetBuffer(releaseThis)[index], kRandData2[index]);
    }

    stampedBufferRelease(&releaseThis);
}

// LCEVCContainerFixtureWithParam
// Main test to ensure that it can do the processing correctly

INSTANTIATE_TEST_SUITE_P(timestampLists, LCEVCContainerFixtureWithParam,
                         testing::Values(kTimestamps2, kTimestamps1, kSortedTimestamps));

TEST_P(LCEVCContainerFixtureWithParam, processRealWorldProcessing)
{
    const std::vector<uint64_t>& thList = GetParam();

    std::set<uint64_t> timestampsToFind;
    size_t addIdx = 0;
    int64_t lastAdd = 0;
    int64_t lastExtract = 0;

    // Need to find all the timestamps we added to the container
    while (timestampsToFind.size() < thList.size()) {
        if (addIdx < thList.size()) {
            addArbitraryData(thList[addIdx], addIdx);
            lastAdd = GetTime<MilliSecond>();
            addIdx++;
        }

        uint64_t th = 0;
        size_t queueLen = 0;
        StampedBuffer* nextBufferInOrder =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &th, &queueLen);
        if (nextBufferInOrder != NULL) {
            lastExtract = GetTime<MilliSecond>();
            timestampsToFind.insert(th);
        }
        // check to see if we should timeout, or we have collected everything
        int64_t tnow = GetTime<MilliSecond>();
        if (((tnow - lastAdd) > 2000) && ((tnow - lastExtract) > 2000)) {
            ADD_FAILURE() << "Timeout, it's been more than 2s since the last add extract";
            return;
        }
    }
    EXPECT_EQ(timestampsToFind.size(), thList.size());
}

// LCEVCContainerTestFixturePreFillSome

INSTANTIATE_TEST_SUITE_P(timestampLists, LCEVCContainerTestFixturePreFillSome,
                         testing::Values(kTimestamps1, kSortedTimestamps));

TEST_P(LCEVCContainerTestFixturePreFillSome, validateTestingData)
{
    // This is just to make sure the supplied vectors are actually valid (i.e. no duplicates).
    // std::unordered_set is used for quick "contains" testing.
    std::unordered_set<uint64_t> timestamps;
    for (uint64_t th : GetParam()) {
        EXPECT_TRUE(timestamps.count(th) == 0);
        timestamps.insert(th);
    }
}

TEST_P(LCEVCContainerTestFixturePreFillSome, extractIsSortedAfterInsertion)
{
    // Container has already been filled, so just check it's sorted now.
    uint64_t timestamp = 0;
    size_t queueSize = 0;
    StampedBuffer* curBuffer =
        lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &timestamp, &queueSize);
    while (lcevcContainerSize(m_lcevcContainer) > 0) {
        StampedBuffer* nextBuffer =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &timestamp, &queueSize);
        EXPECT_LT(stampedBufferGetTimestamp(curBuffer), stampedBufferGetTimestamp(nextBuffer));
        stampedBufferRelease(&curBuffer);
        curBuffer = nextBuffer;
    }
    stampedBufferRelease(&curBuffer);
}

TEST_P(LCEVCContainerTestFixturePreFillSome, extractGetsRightEntry)
{
    const std::vector<uint64_t>& thList = GetParam();
    const size_t idx = std::min(thList.size() - 1, static_cast<size_t>(83)); // arbitrary test index, 83
    const uint64_t th = thList[idx];
    bool dummyIsAtHeadOut = false;
    StampedBuffer* middleBuffer = lcevcContainerExtract(m_lcevcContainer, th, &dummyIsAtHeadOut);
    EXPECT_EQ(stampedBufferGetTimestamp(middleBuffer), th);
    stampedBufferRelease(&middleBuffer);
}

TEST_P(LCEVCContainerTestFixturePreFillSome, extractReturnsNullIfEntryIsMissingInMiddle)
{
    // Timestamps are never within +/-1 of each other, so add 1 to get a fake timestamp.
    const uint64_t fictionalMiddleTh = GetParam()[m_capacity / 2] + 1;
    bool dummyIsAtHeadOut = false;
    StampedBuffer* middleBuffer =
        lcevcContainerExtract(m_lcevcContainer, fictionalMiddleTh, &dummyIsAtHeadOut);
    EXPECT_EQ(middleBuffer, nullptr);
    EXPECT_NE(lcevcContainerSize(m_lcevcContainer), 0);

    // Just in case, don't want a memory leak in the test failure case.
    stampedBufferRelease(&middleBuffer);
}

TEST_P(LCEVCContainerTestFixturePreFillSome, extractReturnsNullAndDeletesAllIfEntryIsPastEnd)
{
    // This will probably be WAY more than the final timestamp, since it's beyond the final entry
    // in the source array (and not all of that is added to the container necessarily).
    const uint64_t laterThanLatestTh = kSortedTimestamps[kSortedTimestamps.size() - 1] + 1;
    bool dummyIsAtHeadOut = false;
    StampedBuffer* middleBuffer =
        lcevcContainerExtract(m_lcevcContainer, laterThanLatestTh, &dummyIsAtHeadOut);
    EXPECT_EQ(middleBuffer, nullptr);
    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), 0);

    // Just in case, don't want a memory leak in the test failure case.
    stampedBufferRelease(&middleBuffer);
}

// LCEVCContainerTestFixturePreFillAll

INSTANTIATE_TEST_SUITE_P(timestampLists, LCEVCContainerTestFixturePreFillAll,
                         testing::Values(kTimestamps1, kSortedTimestamps));

TEST_P(LCEVCContainerTestFixturePreFillAll, extractFromMiddleRemovesAllLower)
{
    // This test has complicated logic, because extract doesn't give us a list of removed entries,
    // but ultimately we're testing that:
    // After removing an entry with a middle-value timestamp
    // (1) The set of remaining entries is STRICTLY EQUAL to the set of higher timestamps, which
    //     we test by showing that
    //     (a) Every remaining entry is in the set of higher timestamps, and
    //     (b) Every entry in the set of higher timestamps is one of the remaining entries, AND
    // (2) The set of removed entries (excluding the requested entry) is STRICTLY EQUAL to the set
    //     of lower timestamps, which we test by showing that
    //     (a) The two sets have the same size, and
    //     (b) Every entry in the set of lower timestamps has been removed.
    //

    const std::vector<uint64_t>& thList = GetParam();

    // Get an entry from an arbitrary non-edge index (the middle index).
    ASSERT_TRUE(thList.size() > 2) << "Can't run this test on a list with 2 or fewer timestamps";
    const size_t remIdx = thList.size() / 2;
    const uint64_t th = thList[remIdx];

    // Gather the lower and higher timestamps in a pair of lists.
    std::vector<uint64_t> lowerThs;
    std::vector<uint64_t> higherThs;
    for (uint64_t i = 0; i < thList.size(); i++) {
        if (i == remIdx) {
            continue;
        }

        // We already validated that our lists contain no duplicate entries, so we don't need to
        // re-test that here: not-less-than means greater-than.
        if (thList[i] < th) {
            lowerThs.push_back(thList[i]);
        } else {
            higherThs.push_back(thList[i]);
        }
    }

    // Test part 2 above
    {
        // Start testing 2b (above) by showing that all the lower entries WERE present. Likewise,
        // start testing 2a by getting the oldSize to see how many were removed.
        const uint64_t oldSize = lcevcContainerSize(m_lcevcContainer);
        for (uint64_t lowTh : lowerThs) {
            bool dummyIsAtHeadOut = false;
            EXPECT_TRUE(lcevcContainerExists(m_lcevcContainer, lowTh, &dummyIsAtHeadOut));
        }

        bool isAtHead = false;
        StampedBuffer* extractedMiddle = lcevcContainerExtract(m_lcevcContainer, th, &isAtHead);
        stampedBufferRelease(&extractedMiddle);
        ASSERT_FALSE(isAtHead)
            << "Failed to choose a timestamp in the middle of the tested container";

        // Finish testing 2b by showing that the lower entries are no longer present, and finish
        // testing 2a by showing that the size has decreased by lowerThs.size() + 1 (the + 1 is for
        // extractedMiddle itself)
        for (uint64_t lowTh : lowerThs) {
            bool dummyIsAtHeadOut = false;
            EXPECT_FALSE(lcevcContainerExists(m_lcevcContainer, lowTh, &dummyIsAtHeadOut));
        }
        EXPECT_EQ(oldSize - lcevcContainerSize(m_lcevcContainer), lowerThs.size() + 1);
    }

    // Test part 1 above
    {
        // This makes sure that ONLY higher timestamps remain
        while (lcevcContainerSize(m_lcevcContainer) > 0) {
            uint64_t nextTh = UINT64_MAX;
            size_t dummyQueueSize = 0;
            StampedBuffer* nextOut =
                lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &nextTh, &dummyQueueSize);

            // Timestamp should be one of the higher ones. Note that we assert rather than
            // expecting, so that we don't hit an exception when we erase the entry.
            const auto highThIter = std::find(higherThs.begin(), higherThs.end(), nextTh);
            ASSERT_NE(highThIter, higherThs.end());

            // remove this th from the higherThs so we can confirm at the end that ALL were present.
            higherThs.erase(highThIter);
            stampedBufferRelease(&nextOut);
        }

        // This makes sure that ALL higher timestamps remain (or, used to remain, but have been
        // extracted now).
        EXPECT_EQ(higherThs.size(), 0);
    }
}

// Tests on empty LCEVCContainers of various capacities

TEST_F(LCEVCContainerTestFixtureMediumCapacity, insertSucceedsUntilCapacity)
{
    ASSERT_LT(m_capacity, kTimestamps1.size()) << "Capacity of fixture was set too low";

    for (size_t i = 0; i < kTimestamps1.size(); i++) {
        const bool insertionSucceeded = addArbitraryData(kTimestamps1[i], i);
        EXPECT_EQ(insertionSucceeded, i < m_capacity);
    }
}

TEST_F(LCEVCContainerTestFixture0Capacity, insertAlwaysSucceeds)
{
    // Assume that the whole vector is large enough to count as "always".
    size_t index = 0;
    for (uint64_t th : kTimestamps1) {
        const bool insertionSucceeded = addArbitraryData(th, index);
        EXPECT_TRUE(insertionSucceeded);
        index++;
    }
}

TEST_F(LCEVCContainerTestFixtureUIntMaxCapacity, insertAlwaysFails)
{
    // Assume that the whole vector is large enough to count as "always".
    size_t index = 0;
    for (uint64_t th : kTimestamps1) {
        const bool insertionSucceeded = addArbitraryData(th, index);
        EXPECT_FALSE(insertionSucceeded);
        index++;
    }
}

// Test the "reject duplicates" behaviour, now that we've confirmed the "insertAlwaysSucceeds"
// behaviour for no-capacity containers.

INSTANTIATE_TEST_SUITE_P(timestampLists, LCEVCContainerTestFixturePreFillAllNoCap,
                         testing::Values(kTimestamps1, kSortedTimestamps));

TEST_P(LCEVCContainerTestFixturePreFillAllNoCap, insertDuplicateNoEffect)
{
    const std::vector<uint64_t>& thList = GetParam();

    // Get an arbitrary entry (17) to duplicate the timestamp of.
    const size_t dupIdx = std::min(thList.size() - 1, static_cast<size_t>(17));
    const uint64_t dupTh = thList[dupIdx];

    const uint64_t oldSize = lcevcContainerSize(m_lcevcContainer);

    // In order to check that the new entry was ignored, we make sure that the new buffer is a
    // different length from all the others.
    const size_t replacementBufLen = kMaxBufSize + 1;
    uint8_t* replacementData = new uint8_t[replacementBufLen];
    lcevcContainerInsertNoCopy(m_lcevcContainer, replacementData, replacementBufLen, dupTh, false,
                               getTimeSinceStart());

    bool dummyIsAtHead = false;
    EXPECT_TRUE(lcevcContainerExists(m_lcevcContainer, dupTh, &dummyIsAtHead));
    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), oldSize);

    StampedBuffer* originalBuffer = lcevcContainerExtract(m_lcevcContainer, dupTh, &dummyIsAtHead);

    EXPECT_NE(stampedBufferGetBufSize(originalBuffer), replacementBufLen);
    EXPECT_NE(stampedBufferGetBuffer(originalBuffer), replacementData);

    stampedBufferRelease(&originalBuffer);
}

// Testing the timestampPredictor aspect (i.e. testing that extraction works with "force" false)

TEST_F(LCEVCContainerTestFixture, extractFailsOnlyAfterDeltaRepeatCountEntries)
{
    std::set<uint64_t> emptySet;
    std::set<uint64_t> timestampsNotYetFound;
    for (uint64_t th : kTimestamps1) {
        timestampsNotYetFound.insert(th);
    }
    testOnEasyData(0, kTimestamps1.size(), true, kTimestamps1, timestampsNotYetFound);
    EXPECT_EQ(timestampsNotYetFound, emptySet);
}

TEST_F(LCEVCContainerTestFixture, extractAlwaysFailsIfTimestampsIncreaseExponentially)
{
    for (size_t i = 0; i < kTimestampsIncreaseExponentially.size(); i++) {
        addArbitraryData(kTimestampsIncreaseExponentially[i], i);

        uint64_t dummyTh = 0;
        size_t dummyQueueLen = 0;
        StampedBuffer* nextBufferInOrder =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);
        EXPECT_EQ(nextBufferInOrder, nullptr);
        stampedBufferRelease(&nextBufferInOrder);
    }
}

TEST_F(LCEVCContainerTestFixture, extractAlwaysFailsIfTimestampsStrictlyDecrease)
{
    // i must be int so it can become -1 and fail loop condition.
    for (int64_t i = static_cast<int64_t>(kSortedTimestamps.size() - 1); i >= 0; i--) {
        addArbitraryData(kSortedTimestamps[i], i);

        uint64_t dummyTh = 0;
        size_t dummyQueueLen = 0;
        StampedBuffer* nextBufferInOrder =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);
        EXPECT_EQ(nextBufferInOrder, nullptr);

        stampedBufferRelease(&nextBufferInOrder);
    }
}

TEST_F(LCEVCContainerTestFixture, extractAlwaysFailsIfTimestampsApproximatelyDecrease)
{
    for (int64_t i = static_cast<int64_t>(kTimestamps1.size() - 1); i >= 0; i--) {
        addArbitraryData(kTimestamps1[i], i);

        uint64_t dummyTh = 0;
        size_t dummyQueueLen = 0;
        StampedBuffer* nextBufferInOrder =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);
        EXPECT_EQ(nextBufferInOrder, nullptr);
        stampedBufferRelease(&nextBufferInOrder);
    }
}

TEST_F(LCEVCContainerTestFixture, extractRecoversAfterEarlyDroppedFrame)
{
    std::set<uint64_t> timestampsToFind;
    for (uint64_t th : kTimestamps1) {
        timestampsToFind.insert(th);
    }

    // Suppose you fail to feed frame 4. We expect to be able to extract all frames except frame
    // 4, and we expect to recover eventually. Note that we expect every extraction to fail in the
    // "skipFrame" range, because it's too early.
    const size_t skipFrame = m_deltaRepeatCount / 2;
    for (size_t i = 0; i < skipFrame; i++) {
        addArbitraryData(kTimestamps1[i], i);

        uint64_t dummyTh = 0;
        size_t dummyQueueLen = 0;
        StampedBuffer* nextBufferInOrder =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);
        EXPECT_EQ(nextBufferInOrder, nullptr);
    }

    recoverFromBadPatch(skipFrame + 1, kTimestamps1.size(), true, true, kTimestamps1, timestampsToFind);

    const size_t expectedMissingTimestamp = kTimestamps1[skipFrame];
    EXPECT_EQ(timestampsToFind.size(), 1);
    EXPECT_NE(timestampsToFind.find(expectedMissingTimestamp), timestampsToFind.end());
}

TEST_F(LCEVCContainerTestFixture, extractRecoversAfterRepeatedLateDroppedFrames)
{
    // Check that, if you drop (say) 1 in every K frames, then you'll still get the rest of the
    // frames out. This test is for skipping AFTER the timestamp gap has been deduced, so set
    // K > m_deltaRepeatCount.

    std::set<uint64_t> timestampsNotYetFound;
    for (uint64_t th : kTimestamps1) {
        timestampsNotYetFound.insert(th);
    }

    std::set<uint64_t> timestampsNotExpectedToBeFound;
    const size_t skipPeriod = m_deltaRepeatCount + 3;
    size_t numForced = 0;
    size_t numAdded = 0;
    uint64_t lastFoundTH = 0;
    for (size_t i = 0; i < kTimestamps1.size(); i++) {
        if ((i % skipPeriod) == (skipPeriod - 1)) { // i.e. skip number 10,21,32,etc
            timestampsNotExpectedToBeFound.insert(kTimestamps1[i]);
            continue;
        }
        addArbitraryData(kTimestamps1[i], i);
        numAdded++;

        uint64_t dummyTh = 0;
        size_t dummyQueueLen = 0;
        StampedBuffer* nextBufferInOrder =
            lcevcContainerExtractNextInOrder(m_lcevcContainer, false, &dummyTh, &dummyQueueLen);
        if (nextBufferInOrder == nullptr && i >= m_deltaRepeatCount) {
            nextBufferInOrder =
                lcevcContainerExtractNextInOrder(m_lcevcContainer, true, &dummyTh, &dummyQueueLen);
            numForced++;
        }

        // Expect timestamps strictly increasing.
        if (nextBufferInOrder != nullptr) {
            EXPECT_GT(stampedBufferGetTimestamp(nextBufferInOrder), lastFoundTH);
            lastFoundTH = stampedBufferGetTimestamp(nextBufferInOrder);
            timestampsNotYetFound.erase(lastFoundTH);
            stampedBufferRelease(&nextBufferInOrder);
        }
    }

    // Force through the end to check that everything we've added is later removed.
    forceUntilEnd(timestampsNotYetFound);
    EXPECT_EQ(timestampsNotExpectedToBeFound, timestampsNotYetFound);

    // Unfortunately, since the data is constantly bad, it's really hard to set a strict limit to
    // the expected number of forced extractions. Experimentally, the number seems to be about 10%,
    // but it's hard to see why. So for now, simply expect that MOST extractions were not forced,
    // i.e. numForced < (numAdded / 2).
    EXPECT_LT(numForced, numAdded / 2);
}

TEST_F(LCEVCContainerTestFixture, extractRecoversAfterTimestampJump)
{
    // This test runs through the first quarter of the data, then skips ahead to the last quarter.
    // We expect the timestamp predictor to recover, with SOME failures (less than
    // m_maxNumReorderFrames). We can't guarantee that EVERY frame will be present, because
    // the source data is out of order, so some data in the middle 2 quarters might belong in the
    // final quarter. Therefore, we merely test that the timestamps come out in increasing order.

    const size_t firstZoneEnd = kTimestamps1.size() / 4;
    const size_t secondZoneStart = 3 * kTimestamps1.size() / 4;

    std::set<uint64_t> timestampsNotYetFound;
    std::set<uint64_t> timestampsNotExpectedToBeFound;
    for (size_t idx = 0; idx < kTimestamps1.size(); idx++) {
        timestampsNotYetFound.insert(kTimestamps1[idx]);
        if (idx >= firstZoneEnd && idx < secondZoneStart) {
            timestampsNotExpectedToBeFound.insert(kTimestamps1[idx]);
        }
    }

    // Go from 0% to 25%
    testOnEasyData(0, firstZoneEnd, false, kTimestamps1, timestampsNotYetFound);

    // Now jump to 75% and go to the end.
    recoverFromBadPatch(secondZoneStart, kTimestamps1.size(), true, true, kTimestamps1,
                        timestampsNotYetFound);

    EXPECT_EQ(timestampsNotExpectedToBeFound, timestampsNotYetFound);
}

TEST_F(LCEVCContainerTestFixture, extractRecoversAfterFPSChange)
{
    std::vector<uint64_t> halfFrameRate(kTimestamps1.size() / 2);
    for (size_t idx = 0; idx < kTimestamps1.size(); idx += 2) {
        halfFrameRate[idx / 2] = kTimestamps1[idx];
    }

    std::set<uint64_t> timestampsNotYetFound;
    for (uint64_t th : kTimestamps1) {
        timestampsNotYetFound.insert(th);
    }

    const size_t transition1 = kTimestamps1.size() / 4;
    const size_t transition2 = 3 * kTimestamps1.size() / 4;
    std::set<uint64_t> timestampsNotExpectedToBeFound;
    for (size_t idx = 0; idx < kTimestamps1.size(); idx++) {
        if ((idx > transition1) && (idx <= transition2) && (idx % 2 == 1)) {
            timestampsNotExpectedToBeFound.insert(kTimestamps1[idx]);
        }
    }

    // Start off with normal data (i.e. "high" fps).
    testOnEasyData(0, transition1, false, kTimestamps1, timestampsNotYetFound);

    // now try the half-frame-rate (it's half size so all indices are halved). Note that
    // realistically, this would come with an inputCC change, which would force the timestamp
    // predictor to reset its expected delta. However, since we're not using inputCCs in this test,
    // the delta may not get updated, resulting in excessive forced extractions. So, we set
    // "expectFewForces" to false.
    recoverFromBadPatch(transition1 / 2, transition2 / 2, false, false, halfFrameRate, timestampsNotYetFound);

    // now back to normal (still need to use the "recovery" behaviour though).
    recoverFromBadPatch(transition2, kTimestamps1.size(), true, true, kTimestamps1, timestampsNotYetFound);

    EXPECT_EQ(timestampsNotExpectedToBeFound, timestampsNotYetFound);
}

// Testing the "isAtHead" output behaviour

TEST_F(LCEVCContainerTestFixture, minExistsAtHead)
{
    populate(kSortedTimestamps);
    const uint64_t minTh = kSortedTimestamps[0];
    bool isAtHead = false;
    lcevcContainerExists(m_lcevcContainer, minTh, &isAtHead);
    EXPECT_TRUE(isAtHead);
}

TEST_F(LCEVCContainerTestFixture, minExtractsAtHead)
{
    populate(kSortedTimestamps);
    const uint64_t minTh = kSortedTimestamps[0];
    bool isAtHead = false;
    StampedBuffer* releaseThis = lcevcContainerExtract(m_lcevcContainer, minTh, &isAtHead);
    EXPECT_TRUE(isAtHead);
    stampedBufferRelease(&releaseThis);
}

// Testing flush and clear functions

TEST_F(LCEVCContainerTestFixture, flushRemovesIfTimestampIsPresent)
{
    populate(kTimestamps1);
    bool dummyIsAtHead = false;
    ASSERT_TRUE(lcevcContainerExists(m_lcevcContainer, kTimestamps1[0], &dummyIsAtHead))
        << "lcevcContainer is missing a timestamp that should have been added to it: "
        << kTimestamps1[0];
    ASSERT_EQ(lcevcContainerSize(m_lcevcContainer), m_capacity)
        << "Please use a fixture whose capacity is less than the size of the sample timestamp "
           "list";

    lcevcContainerFlush(m_lcevcContainer, kTimestamps1[0]);

    EXPECT_FALSE(lcevcContainerExists(m_lcevcContainer, kTimestamps1[0], &dummyIsAtHead));
    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), m_capacity - 1);
}

TEST_F(LCEVCContainerTestFixture, flushDoesNothingIfTimestampAbsent)
{
    populate(kTimestamps1);
    bool dummyIsAtHead = false;
    ASSERT_FALSE(lcevcContainerExists(m_lcevcContainer, kTimestamps1[m_capacity], &dummyIsAtHead))
        << "lcevcContainer contains a timestamp that shouldn't have been added to it: "
        << kTimestamps1[m_capacity];
    ASSERT_EQ(lcevcContainerSize(m_lcevcContainer), m_capacity)
        << "Please use a fixture whose capacity is less than the size of the sample timestamp "
           "list";

    lcevcContainerFlush(m_lcevcContainer, kTimestamps1[m_capacity]);

    EXPECT_FALSE(lcevcContainerExists(m_lcevcContainer, kTimestamps1[m_capacity], &dummyIsAtHead));
    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), m_capacity);
}

TEST_F(LCEVCContainerTestFixture, clearRemovesAll)
{
    populate(kTimestamps1);
    bool dummyIsAtHead = false;
    ASSERT_EQ(lcevcContainerSize(m_lcevcContainer), m_capacity)
        << "Please use a fixture whose capacity is less than the size of the sample timestamp "
           "list";

    lcevcContainerClear(m_lcevcContainer);

    EXPECT_EQ(lcevcContainerSize(m_lcevcContainer), 0);
    for (uint64_t th : kTimestamps1) {
        EXPECT_FALSE(lcevcContainerExists(m_lcevcContainer, th, &dummyIsAtHead));
    }
}
