/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
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

#include "decoder_asynchronous.h"

#include "utils.h"

#include <gtest/gtest.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/chrono.h>
#include <LCEVC/utility/threading.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>
#include <thread>

using namespace lcevc_dec::decoder;
using namespace lcevc_dec::utility;

// - Thread ---------------------------------------------------------------------------------------

#if defined(WIN32)
Thread::Thread(std::wstring_view name, uint8_t maxFinishedTasks)
#else
Thread::Thread(std::string_view name, uint8_t maxFinishedTasks)
#endif
    : m_maxFinishedTasks(maxFinishedTasks)
    , m_thread(&Thread::loop, this, name)
{}

Thread::~Thread()
{
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Thread::wake(const Task& task)
{
    const std::scoped_lock lock(m_mutex);
    m_tasks.push(task);
    m_cv.notify_all();
}

#if defined(WIN32)
void Thread::loop(std::wstring_view name)
#else
void Thread::loop(std::string_view name)
#endif
{
    setThreadName(name.data());

    while (true) {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this]() { return !m_tasks.empty(); });
        const Task& toDo = m_tasks.front();
        LCEVC_ReturnCode res = LCEVC_Success;
        uint8_t doneTasks = 0;
        while (res != LCEVC_Again) {
            res = toDo(doneTasks);
            ASSERT_NE(res, LCEVC_Error);
        }
        m_tasks.pop();
        m_doneTasks += doneTasks;

        if (isDoneInternal()) {
            return;
        }
    }
}

bool Thread::isDone()
{
    const std::scoped_lock lock(m_mutex);
    return isDoneInternal();
}

// - DecoderAsynchronous -------------------------------------------------------------------------

void DecoderAsynchronous::callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event,
                                   LCEVC_PictureHandle picHandle,
                                   const LCEVC_DecodeInformation* decodeInformation,
                                   const uint8_t* data, uint32_t dataSize)
{
    switch (event) {
        case LCEVC_Log: log(data, dataSize); break;
        case LCEVC_Exit: exit(); break;

        // Base
        case LCEVC_CanSendBase: {
            m_inputThread.wake([decHandle, this](uint8_t& doneTasks) {
                const bool basesWereDone = getBasesDone();
                const LCEVC_ReturnCode res = sendBase(decHandle);
                if (getBasesDone() && !basesWereDone) {
                    doneTasks++;
                }
                return res;
            });
            break;
        }
        case LCEVC_BasePictureDone: {
            m_inputThread.wake([this, picHandle](uint8_t& doneTasks) {
                reuseBase(picHandle);
                return sendBaseAndEnhancement(doneTasks);
            });
            break;
        }

        // Enhancement
        case LCEVC_CanSendEnhancement: {
            m_inputThread.wake([decHandle, this](uint8_t& doneTasks) {
                const bool enhancementsWereDone = getEnhancementsDone();
                const LCEVC_ReturnCode res = sendEnhancement(decHandle);
                if (getEnhancementsDone() && !enhancementsWereDone) {
                    doneTasks++;
                }
                return res;
            });
            break;
        }

        // Output
        case LCEVC_CanSendPicture: {
            m_outputThread.wake([decHandle, this](const uint8_t&) { return sendOutput(decHandle); });
            break;
        }
        case LCEVC_CanReceive: {
            m_outputThread.wake([this](uint8_t& doneTasks) {
                const bool outputsWereDone = getOutputsDone();
                const LCEVC_ReturnCode res = receiveOutput();
                if (getOutputsDone() && !outputsWereDone) {
                    doneTasks++;
                }
                return res;
            });
            break;
        }
        case LCEVC_OutputPictureDone: {
            ASSERT_NE(decodeInformation, nullptr);
            const LCEVC_DecodeInformation decInfoCopy(*decodeInformation);
            // Note that the output thread reuses the output (it has access to m_outputs), whereas
            // the input thread checks decodeInformation (it has access to m_basePtsToSend, etc).
            m_inputThread.wake([decInfoCopy, this](const uint8_t&) {
                if (getBasesDone() || getEnhancementsDone()) {
                    // Early-exit if with LCEVC_Again if we're already done (otherwise, we may re-
                    // enter the "while (true)" loop and never escape).
                    return LCEVC_Again;
                }
                checkDecInfo(decInfoCopy);
                return LCEVC_Success;
            });
            m_outputThread.wake([picHandle, decHandle, this](const uint8_t&) {
                reuseOutput(picHandle);
                return sendOutput(decHandle);
            });
            break;
        }

        case LCEVC_EventCount:
        case LCEVC_Event_ForceUInt8: FAIL() << "Invalid event type: " << event; break;
    }

    increment(event);
}

LCEVC_ReturnCode DecoderAsynchronous::sendBaseAndEnhancement(uint8_t& doneTasks)
{
    const bool enhancementsWereDone = getEnhancementsDone();
    const LCEVC_ReturnCode enhancementRes = sendEnhancement(getDecHandle());
    if (getEnhancementsDone() && !enhancementsWereDone) {
        doneTasks++;
    }

    const bool basesWereDone = getBasesDone();
    const LCEVC_ReturnCode baseRes = sendBase(getDecHandle());
    if (getBasesDone() && !basesWereDone) {
        doneTasks++;
    }

    if (enhancementRes == LCEVC_Again || baseRes == LCEVC_Again) {
        return LCEVC_Again;
    }
    if (enhancementRes != LCEVC_Success || baseRes != LCEVC_Success) {
        return LCEVC_Error;
    }
    return LCEVC_Success;
}
