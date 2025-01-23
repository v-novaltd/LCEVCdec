/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#ifndef VN_API_TEST_UNIT_DECODE_TESTER_H_
#define VN_API_TEST_UNIT_DECODE_TESTER_H_

#include "utils.h"

#include <LCEVC/lcevc_dec.h>

#include <cstddef>
#include <cstdint>
#include <unordered_set>

static const std::unordered_set<LCEVC_ReturnCode> kSuccesses = {LCEVC_Success, LCEVC_Again};

class EventTester
{
public:
    explicit EventTester(int64_t numFrames)
        : m_afterTheEndPts(numFrames)
    {}
    virtual ~EventTester() = default;

    void setup();

    void teardown(bool wasTimeout);

    // Virtual callback, must be defined by child classes.
    virtual void callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
                          const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
                          uint32_t dataSize) = 0;

    void increment(LCEVC_Event event) { m_eventCounts[event]++; }
    uint32_t getCount(LCEVC_Event event) const { return m_eventCounts[event]; }

    size_t getNumUnsentBases() const { return m_bases.size(); }

    virtual bool atomicIsDone() = 0;

protected:
    // Callback responses:
    static void log(const uint8_t* data, uint32_t dataSize);
    void exit();
    virtual LCEVC_ReturnCode sendBase(LCEVC_DecoderHandle decHandle);
    virtual LCEVC_ReturnCode sendOutput(LCEVC_DecoderHandle decHandle);
    virtual LCEVC_ReturnCode sendEnhancement(LCEVC_DecoderHandle decHandle);
    virtual LCEVC_ReturnCode receiveOutput();
    void checkDecInfo(const LCEVC_DecodeInformation& decodeInformation);
    void reuseBase(LCEVC_PictureHandle picHandle) { m_bases.insert(picHandle.hdl); }
    void reuseOutput(LCEVC_PictureHandle picHandle);

    bool getBasesDone() const { return m_basesDone; }
    bool getEnhancementsDone() const { return m_enhancementsDone; }
    bool getOutputsDone() const { return m_outputsDone; }

    LCEVC_DecoderHandle getDecHandle() const { return m_hdl; }

private:
    LCEVC_DecoderHandle m_hdl = {};
    LCEVC_PictureDesc m_inputDesc = {};
    LCEVC_PictureDesc m_outputDesc = {};
    EventCountArr m_eventCounts = {};

    std::unordered_set<uintptr_t> m_bases;
    std::unordered_set<uintptr_t> m_outputs;

    int64_t m_basePtsToSend = 0;
    int64_t m_enhancementPtsToSend = 0;
    int64_t m_latestReceivedPts = -1;

    bool m_basesDone = false;
    bool m_enhancementsDone = false;
    bool m_outputsDone = false;

    const int64_t m_afterTheEndPts;
    bool m_tornDown = false;
};

#endif // VN_API_TEST_UNIT_DECODE_TESTER_H_
