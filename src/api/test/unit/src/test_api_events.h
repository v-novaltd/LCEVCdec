/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_TEST_UNIT_TEST_API_EVENTS_H_
#define VN_API_TEST_UNIT_TEST_API_EVENTS_H_

#include "utils.h"

#include <LCEVC/lcevc_dec.h>

#include <atomic>
#include <unordered_set>

class DecodeTester
{
public:
    explicit DecodeTester(int64_t numFrames)
        : m_afterTheEndPts(numFrames)
    {}

    void setup();

    void teardown() const { LCEVC_DestroyDecoder(m_hdl); }

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
    void receiveOutput();
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
};

#endif // VN_API_TEST_UNIT_TEST_API_EVENTS_H_
