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

#ifndef VN_LCEVC_API_DECODER_ASYNCHRONOUS_H
#define VN_LCEVC_API_DECODER_ASYNCHRONOUS_H

#include "event_tester.h"

#include <LCEVC/api_utility/threading.h>
#include <LCEVC/lcevc_dec.h>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string_view>
#include <thread>

// - Thread ---------------------------------------------------------------------------------------

using Task = std::function<LCEVC_ReturnCode(uint8_t&)>;

class Thread
{
public:
#if defined(WIN32)
    explicit Thread(std::wstring_view name, uint8_t maxFinishedTasks);
#else
    explicit Thread(std::string_view name, uint8_t maxFinishedTasks);
#endif
    ~Thread();

    void wake(const Task& task);

#if defined(WIN32)
    void loop(std::wstring_view name);
#else
    void loop(std::string_view name);
#endif

    // Safely check m_doneTasks behind an m_mutex lock
    bool isDone();

private:
    bool isDoneInternal() const { return m_doneTasks >= m_maxFinishedTasks; }

    std::queue<Task> m_tasks;
    uint8_t m_doneTasks = 0;
    const uint8_t m_maxFinishedTasks;

    // Thread must be last memvar, so that all the others are initialized before it starts.
    std::condition_variable m_cv;
    std::mutex m_mutex;
    std::thread m_thread;
};

// - DecoderAsynchronous -------------------------------------------------------------------------

class DecoderAsynchronous : public EventTester
{
    using BaseClass = EventTester;
    using BaseClass::BaseClass;

public:
    void callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
                  const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
                  uint32_t dataSize) override;

    bool atomicIsDone() override { return m_inputThread.isDone() && m_outputThread.isDone(); }

private:
    LCEVC_ReturnCode sendBaseAndEnhancement(uint8_t& doneTasks);

    // These threads each have a set of data which they can write to. Neither thread is allowed to
    // read the other's writable data (that's a data race).

    // Input thread goes until BOTH base and enhancements are done. Input thread can write to:
    // m_basePtsToSend
    // m_enhancementPtsToSend
    // m_basesDone
    // m_enhancementsDone
    Thread m_inputThread{VN_TO_THREAD_NAME("LCEVC_test_api_threaded_input"), 2};

    // Output goes until outputs are done. Output thread can write to:
    // m_outputs
    // m_outputsDone
    // m_latestReceivedPts
    Thread m_outputThread{VN_TO_THREAD_NAME("LCEVC_test_api_threaded_output"), 1};
};

#endif // VN_LCEVC_API_DECODER_ASYNCHRONOUS_H
