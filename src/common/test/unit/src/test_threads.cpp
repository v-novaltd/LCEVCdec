/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/threads.h>

#include <climits>

static intptr_t threadSet1(void* argument)
{
    int* ptr = (int*)argument;
    *ptr = 1;
    return 100 + *ptr;
}

static intptr_t threadSet2(void* argument)
{
    int* ptr = (int*)argument;
    *ptr = 2;
    return 100 + *ptr;
}
static intptr_t threadSet3(void* argument)
{
    int* ptr = (int*)argument;
    *ptr = 3;
    return 100 + *ptr;
}

TEST(ThreadsTest, CreateOne)
{
    Thread t;
    volatile int a = 0;
    EXPECT_EQ(threadCreate(&t, threadSet1, (void*)&a), ThreadResultSuccess);
    intptr_t res = 0;
    EXPECT_EQ(threadJoin(&t, &res), ThreadResultSuccess);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(res, 101);
}

TEST(ThreadsTest, CreateMany)
{
    const int kNumThreads = 100;
    struct
    {
        Thread thr;
        volatile int a;
    } threads[kNumThreads];

    for (int i = 0; i < kNumThreads; ++i) {
        const ThreadFunction fns[] = {threadSet1, threadSet2, threadSet3};
        threads[i].a = 0;
        EXPECT_EQ(threadCreate(&threads[i].thr, fns[i % 3], (void*)&threads[i].a), ThreadResultSuccess);
    }

    for (int i = 0; i < kNumThreads; ++i) {
        intptr_t res = 0;
        EXPECT_EQ(threadJoin(&threads[i].thr, &res), ThreadResultSuccess);

        EXPECT_EQ(threads[i].a, (i % 3) + 1);
        EXPECT_EQ(res, 100 + (i % 3) + 1);
    }
}

TEST(ThreadsTest, JoinAfterExit)
{
    Thread t;
    volatile int a = 0;
    EXPECT_EQ(threadCreate(&t, threadSet1, (void*)&a), ThreadResultSuccess);
    intptr_t res = 0;

    // Wait a while
    EXPECT_EQ(threadSleep(100), 0);

    EXPECT_EQ(threadJoin(&t, &res), ThreadResultSuccess);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(res, 101);
}

TEST(ThreadsTest, MutexCreate)
{
    ThreadMutex m;
    EXPECT_EQ(threadMutexInitialize(&m), ThreadResultSuccess);
    EXPECT_EQ(threadMutexDestroy(&m), ThreadResultSuccess);
}

TEST(ThreadsTest, MutexLock)
{
    ThreadMutex m;
    EXPECT_EQ(threadMutexInitialize(&m), ThreadResultSuccess);
    EXPECT_EQ(threadMutexLock(&m), ThreadResultSuccess);
    EXPECT_EQ(threadMutexUnlock(&m), ThreadResultSuccess);
    EXPECT_EQ(threadMutexDestroy(&m), ThreadResultSuccess);
}

TEST(ThreadsTest, MutexLockMany)
{
    ThreadMutex m;
    EXPECT_EQ(threadMutexInitialize(&m), ThreadResultSuccess);
    for (int i = 0; i < 10000000; ++i) {
        EXPECT_EQ(threadMutexLock(&m), ThreadResultSuccess);
        EXPECT_EQ(threadMutexUnlock(&m), ThreadResultSuccess);
    }
    EXPECT_EQ(threadMutexDestroy(&m), ThreadResultSuccess);
}

struct ThreadData
{
    ThreadMutex mutex;
    int count;
};

static intptr_t threadLockIncrement1000(void* argument)
{
    ThreadData* data = (ThreadData*)argument;

    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);
        data->count++;
        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

TEST(ThreadsTest, MutexLockThread)
{
    ThreadData data = {};
    EXPECT_EQ(threadMutexInitialize(&data.mutex), ThreadResultSuccess);
    data.count = 0;

    Thread t;
    EXPECT_EQ(threadCreate(&t, threadLockIncrement1000, (void*)&data), ThreadResultSuccess);
    EXPECT_EQ(threadJoin(&t, NULL), ThreadResultSuccess);

    EXPECT_EQ(data.count, 1000);

    EXPECT_EQ(threadMutexDestroy(&data.mutex), ThreadResultSuccess);
}

static intptr_t threadLockIncrement100(void* argument)
{
    ThreadData* data = (ThreadData*)argument;

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);
        data->count++;
        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

TEST(ThreadsTest, MutexLockThread100)
{
    const int kNumThreads = 1000;

    ThreadData data = {};
    EXPECT_EQ(threadMutexInitialize(&data.mutex), ThreadResultSuccess);
    data.count = 0;

    Thread threads[kNumThreads];

    for (int i = 0; i < kNumThreads; ++i) {
        EXPECT_EQ(threadCreate(&threads[i], threadLockIncrement100, (void*)&data), ThreadResultSuccess);
    }

    for (int i = 0; i < kNumThreads; ++i) {
        EXPECT_EQ(threadJoin(&threads[i], NULL), ThreadResultSuccess);
    }

    EXPECT_EQ(data.count, 100 * kNumThreads);

    EXPECT_EQ(threadMutexDestroy(&data.mutex), ThreadResultSuccess);
}

TEST(ThreadsTest, CondVarCreate)
{
    ThreadCondVar cv;
    EXPECT_EQ(threadCondVarInitialize(&cv), ThreadResultSuccess);
    threadCondVarDestroy(&cv);
}

struct ThreadDataCVProdCons
{
    ThreadMutex mutex;
    ThreadCondVar notEmpty;
    ThreadCondVar notFull;
    uint32_t produceCount; // How many each producer should produce
    uint32_t consumeCount; // How many each consumer should consume
    uint32_t pendingLimit; // Number of pending items before being considered 'full'

    uint32_t pending;       // Pending items
    uint32_t consumedTotal; // Total Consumed items
    uint32_t producersWaiting;
    uint32_t consumersWaiting;
};

// Producer - signals for every push
//
static intptr_t threadCVProducer(void* argument)
{
    ThreadDataCVProdCons* data = (ThreadDataCVProdCons*)argument;

    for (uint32_t i = 0; i < data->produceCount; ++i) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);

        while (data->pending >= data->pendingLimit) {
            data->producersWaiting++;
            EXPECT_EQ(threadCondVarWait(&data->notFull, &data->mutex), ThreadResultSuccess);
            data->producersWaiting--;
        }

        if (data->consumersWaiting > 0) {
            EXPECT_EQ(threadCondVarSignal(&data->notEmpty), ThreadResultSuccess);
        }

        data->pending++;

        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

// Producer - signals for every pull
//
static intptr_t threadCVConsumer(void* argument)
{
    ThreadDataCVProdCons* data = (ThreadDataCVProdCons*)argument;

    bool done = false;

    while (!done) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);

        // Wait for work
        while (data->pending == 0) {
            data->consumersWaiting++;
            EXPECT_EQ(threadCondVarWait(&data->notEmpty, &data->mutex), ThreadResultSuccess);
            data->consumersWaiting--;
        }

        if (data->producersWaiting > 0) {
            EXPECT_EQ(threadCondVarSignal(&data->notFull), ThreadResultSuccess);
        }

        data->pending--;
        data->consumedTotal++;

        if (data->consumedTotal == data->consumeCount) {
            done = true;
        }

        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

// Producer for a single consumer - only signal when going empty->not empty
//
static intptr_t threadCVProducerSC(void* argument)
{
    ThreadDataCVProdCons* data = (ThreadDataCVProdCons*)argument;

    for (uint32_t i = 0; i < data->produceCount; ++i) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);

        // Wait for capacity
        while (data->pending >= data->pendingLimit) {
            data->producersWaiting++;
            EXPECT_EQ(threadCondVarWait(&data->notFull, &data->mutex), ThreadResultSuccess);
            data->producersWaiting--;
        }

        if (data->pending == 0 && data->consumersWaiting > 0) {
            // Going from empty to not empty
            EXPECT_EQ(threadCondVarSignal(&data->notEmpty), ThreadResultSuccess);
        }
        data->pending++;

        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

// Consumer for a single producer - only signal when going full->not full
//
static intptr_t threadCVConsumerSP(void* argument)
{
    ThreadDataCVProdCons* data = (ThreadDataCVProdCons*)argument;

    bool done = false;

    while (!done) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);

        // Wait for work
        while (data->pending == 0) {
            data->consumersWaiting++;
            EXPECT_EQ(threadCondVarWait(&data->notEmpty, &data->mutex), ThreadResultSuccess);
            data->consumersWaiting--;
        }

        if (data->pending >= data->pendingLimit && data->producersWaiting > 0) {
            // Going from full to not full
            EXPECT_EQ(threadCondVarSignal(&data->notFull), ThreadResultSuccess);
        }

        data->pending--;
        data->consumedTotal++;

        if (data->consumedTotal == data->consumeCount) {
            done = true;
        }

        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

// Producer that broadcasts when going empty->not empty
//
static intptr_t threadCVProducerBroadcast(void* argument)
{
    ThreadDataCVProdCons* data = (ThreadDataCVProdCons*)argument;

    for (uint32_t i = 0; i < data->produceCount; ++i) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);

        // Wait for capacity
        while (data->pending >= data->pendingLimit) {
            data->producersWaiting++;
            EXPECT_EQ(threadCondVarWait(&data->notFull, &data->mutex), ThreadResultSuccess);
            data->producersWaiting--;
        }

        if (data->pending == 0 && data->consumersWaiting > 0) {
            EXPECT_EQ(threadCondVarBroadcast(&data->notEmpty), ThreadResultSuccess);
        }
        data->pending++;

        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

// Consumer that broadcasts when going fill to no full
//
static intptr_t threadCVConsumerBroadcast(void* argument)
{
    ThreadDataCVProdCons* data = (ThreadDataCVProdCons*)argument;

    bool done = false;

    while (!done) {
        EXPECT_EQ(threadMutexLock(&data->mutex), ThreadResultSuccess);

        // Wait for work
        while (data->pending == 0) {
            data->consumersWaiting++;
            EXPECT_EQ(threadCondVarWait(&data->notEmpty, &data->mutex), ThreadResultSuccess);
            data->consumersWaiting--;
        }

        if (data->pending >= data->pendingLimit && data->producersWaiting > 0) {
            EXPECT_EQ(threadCondVarBroadcast(&data->notFull), ThreadResultSuccess);
        }

        data->pending--;
        data->consumedTotal++;

        if (data->consumedTotal == data->consumeCount) {
            done = true;
        }

        EXPECT_EQ(threadMutexUnlock(&data->mutex), ThreadResultSuccess);
    }

    return 0;
}

TEST(ThreadsTest, CondVarProdCons)
{
    ThreadDataCVProdCons data = {};

    EXPECT_EQ(threadMutexInitialize(&data.mutex), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notEmpty), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notFull), ThreadResultSuccess);
    data.produceCount = 10000000;
    data.consumeCount = 10000000;
    data.pending = 0;
    data.pendingLimit = 2000;
    data.consumedTotal = 0;
    data.producersWaiting = 0;
    data.consumersWaiting = 0;

    // Start a single producer
    Thread threadProduce;
    EXPECT_EQ(threadCreate(&threadProduce, threadCVProducer, (void*)&data), ThreadResultSuccess);

    // Start a single consumer
    Thread threadConsume;
    EXPECT_EQ(threadCreate(&threadConsume, threadCVConsumer, (void*)&data), ThreadResultSuccess);

    EXPECT_EQ(threadJoin(&threadProduce, NULL), ThreadResultSuccess);
    EXPECT_EQ(threadJoin(&threadConsume, NULL), ThreadResultSuccess);

    EXPECT_EQ(data.produceCount, data.consumeCount);
    EXPECT_EQ(data.pending, 0);
}

TEST(ThreadsTest, CondVarProdConsSCSP)
{
    ThreadDataCVProdCons data = {};

    EXPECT_EQ(threadMutexInitialize(&data.mutex), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notEmpty), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notFull), ThreadResultSuccess);
    data.produceCount = 10000000;
    data.consumeCount = 10000000;
    data.pending = 0;
    data.pendingLimit = 2000;
    data.consumedTotal = 0;
    data.producersWaiting = 0;
    data.consumersWaiting = 0;

    // Start a single producer
    Thread threadProduce;
    EXPECT_EQ(threadCreate(&threadProduce, threadCVProducerSC, (void*)&data), ThreadResultSuccess);

    // Start a single consumer
    Thread threadConsume;
    EXPECT_EQ(threadCreate(&threadConsume, threadCVConsumerSP, (void*)&data), ThreadResultSuccess);

    EXPECT_EQ(threadJoin(&threadProduce, NULL), ThreadResultSuccess);
    EXPECT_EQ(threadJoin(&threadConsume, NULL), ThreadResultSuccess);

    EXPECT_EQ(data.produceCount, data.consumeCount);
    EXPECT_EQ(data.pending, 0);
}

// NB: helgrind get confused by this - it can handle pthread conditional variables perfectly.
//
// See: https://valgrind.org/docs/manual/hg-manual.html#hg-manual.effective-use section 3.
//
TEST(ThreadsTest, CondVarManyProdCons)
{
    const uint32_t kNumProducers = 40;

    ThreadDataCVProdCons data = {};

    EXPECT_EQ(threadMutexInitialize(&data.mutex), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notEmpty), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notFull), ThreadResultSuccess);
    data.produceCount = 10000;
    data.consumeCount = data.produceCount * kNumProducers;
    data.pending = 0;
    data.pendingLimit = 200;
    data.consumedTotal = 0;
    data.producersWaiting = 0;
    data.consumersWaiting = 0;

    // Start all the producers
    Thread threadProducers[kNumProducers];
    for (uint32_t i = 0; i < kNumProducers; ++i) {
        EXPECT_EQ(threadCreate(&threadProducers[i], threadCVProducerSC, (void*)&data), ThreadResultSuccess);
    }

    // Start a consumer
    Thread threadConsume;
    EXPECT_EQ(threadCreate(&threadConsume, threadCVConsumer, (void*)&data), ThreadResultSuccess);
    threadSetPriority(&threadConsume, ThreadPriorityIdle);

    for (uint32_t i = 0; i < kNumProducers; ++i) {
        EXPECT_EQ(threadJoin(&threadProducers[i], NULL), ThreadResultSuccess);
    }
    EXPECT_EQ(threadJoin(&threadConsume, NULL), ThreadResultSuccess);
}

// NB: helgrind get confused by this - it can handle pthread conditional variables perfectly.
//
// See: https://valgrind.org/docs/manual/hg-manual.html#hg-manual.effective-use section 3.
//
TEST(ThreadsTest, CondVarManyProdConsBroadcast)
{
    const uint32_t kNumProducers = 20;

    ThreadDataCVProdCons data = {};

    EXPECT_EQ(threadMutexInitialize(&data.mutex), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notEmpty), ThreadResultSuccess);
    EXPECT_EQ(threadCondVarInitialize(&data.notFull), ThreadResultSuccess);
    data.produceCount = 10000;
    data.consumeCount = data.produceCount * kNumProducers;
    data.pending = 0;
    data.pendingLimit = 200;
    data.consumedTotal = 0;
    data.producersWaiting = 0;
    data.consumersWaiting = 0;

    // Start all the producers
    Thread threadProducers[kNumProducers];
    for (uint32_t i = 0; i < kNumProducers; ++i) {
        EXPECT_EQ(threadCreate(&threadProducers[i], threadCVProducerBroadcast, (void*)&data),
                  ThreadResultSuccess);
    }

    // Start a consumer
    Thread threadConsume;
    EXPECT_EQ(threadCreate(&threadConsume, threadCVConsumerBroadcast, (void*)&data), ThreadResultSuccess);
    threadSetPriority(&threadConsume, ThreadPriorityIdle);

    for (uint32_t i = 0; i < kNumProducers; ++i) {
        EXPECT_EQ(threadJoin(&threadProducers[i], NULL), ThreadResultSuccess);
    }
    EXPECT_EQ(threadJoin(&threadConsume, NULL), ThreadResultSuccess);
}
