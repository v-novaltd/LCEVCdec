/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_API_TEST_UNIT_TEST_API_EVENTS_H_
#define VN_API_TEST_UNIT_TEST_API_EVENTS_H_

#include "utils.h"

#include <LCEVC/lcevc_dec.h>

#include <array>
#include <atomic>
#include <unordered_set>

class DecodeTester
{
public:
    explicit DecodeTester(int64_t numFrames)
        : m_afterTheEndPts(numFrames)
    {}
    ~DecodeTester()
    {
        if (!m_tornDown) {
            teardown();
        }
    }

    void setup();

    void teardown();

    // Main callback:
    void callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
                  const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
                  uint32_t dataSize);

    void increment(LCEVC_Event event) { m_eventCounts[event]++; }
    uint32_t getCount(LCEVC_Event event) const { return m_eventCounts[event]; }

    void sendEnhancement() { sendEnhancement(m_hdl); }

    bool atomicIsDone() const { return m_atomicIsDone; }

private:
    // Callback responses:
    static void log(const uint8_t* data, uint32_t dataSize);
    void exit();
    void sendBase(LCEVC_DecoderHandle decHandle);
    void sendOutput(LCEVC_DecoderHandle decHandle);
    void sendEnhancement(LCEVC_DecoderHandle decHandle);
    void receiveOutput(bool expectOutput = true);
    void reuseBase(LCEVC_PictureHandle picHandle);
    void reuseOutput(LCEVC_PictureHandle picHandle, const LCEVC_DecodeInformation* decodeInformation);

    // Internal, for setting the externally-viewable atomic.
    bool isDone() const
    {
        return (m_basePtsToSend >= (m_afterTheEndPts - 1) &&
                m_enhancementPtsToSend >= (m_afterTheEndPts - 1) &&
                m_latestReceivedPts >= (m_afterTheEndPts - 1));
    }

    LCEVC_DecoderHandle m_hdl = {};
    LCEVC_PictureDesc m_inputDesc = {};
    LCEVC_PictureDesc m_outputDesc = {};
    EventCountArr m_eventCounts = {};

    std::unordered_set<uintptr_t> m_bases;
    std::unordered_set<uintptr_t> m_outputs;

    int64_t m_basePtsToSend = 0;
    int64_t m_enhancementPtsToSend = 0;
    int64_t m_latestReceivedPts = 0;

    const int64_t m_afterTheEndPts;
    std::atomic<bool> m_atomicIsDone = false;
    bool m_tornDown = false;
};

#endif // VN_API_TEST_UNIT_TEST_API_EVENTS_H_
