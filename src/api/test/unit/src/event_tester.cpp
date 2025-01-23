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

#include "event_tester.h"

#include "data.h"
#include "utils.h"

#include <gtest/gtest.h>
#include <interface.h>
#include <LCEVC/lcevc_dec.h>

#include <cctype>
#include <cstdint>
#include <limits>

// ------------------------------------------------------------------------------------------------

namespace {
void callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
              const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
              uint32_t dataSize, void* userData)
{
    ASSERT_NE(userData, nullptr);
    auto& tester = *static_cast<EventTester*>(userData);
    tester.callback(decHandle, event, picHandle, decodeInformation, data, dataSize);
}
} // namespace

// EventTester -------------------------------------------------------------------------------------

void EventTester::setup()
{
    const LCEVC_AccelContextHandle dummyHdl = {};
    ASSERT_EQ(LCEVC_CreateDecoder(&m_hdl, dummyHdl), LCEVC_Success);
    ASSERT_EQ(LCEVC_ConfigureDecoderIntArray(m_hdl, "events", static_cast<uint32_t>(kAllEvents.size()),
                                             kAllEvents.data()),
              LCEVC_Success);
    ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_hdl, "core_threads", 1), LCEVC_Success);
    ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_hdl, "loq_unprocessed_cap", 10), LCEVC_Success);
    ASSERT_EQ(LCEVC_SetDecoderEventCallback(m_hdl, ::callback, static_cast<void*>(this)), LCEVC_Success);
    LCEVC_DefaultPictureDesc(&m_inputDesc, LCEVC_I420_8, 960, 540);
    LCEVC_DefaultPictureDesc(&m_outputDesc, LCEVC_I420_8, 1920, 1080);

    ASSERT_EQ(LCEVC_InitializeDecoder(m_hdl), LCEVC_Success);
}

void EventTester::teardown(bool wasTimeout)
{
    if (wasTimeout) {
        // This blocks any new sends (in case we're tearing down after a timeout).
        m_basePtsToSend = m_enhancementPtsToSend = m_latestReceivedPts = m_afterTheEndPts;

        while (receiveOutput() != LCEVC_Again) {
            // Receive all pending outputs before destroying, or else you might set certain outputs
            // to nullptr while the decoder is writing to those destinations.
        }
    }

    LCEVC_DestroyDecoder(m_hdl);
    m_tornDown = true;
}

void EventTester::log(const uint8_t* data, uint32_t dataSize)
{
    ASSERT_NE(data, nullptr);

    // Not a lot we can do here except check that every character is printable...
    for (uint32_t idx = 0; idx < dataSize; idx++) {
        ASSERT_TRUE(isprint(data[idx]));
    }
}

void EventTester::exit() { ASSERT_EQ(m_eventCounts[LCEVC_Exit], 0); }

LCEVC_ReturnCode EventTester::sendBase(LCEVC_DecoderHandle decHandle)
{
    if (m_hdl.hdl != decHandle.hdl) {
        return LCEVC_Error;
    }
    if (m_afterTheEndPts == m_basePtsToSend || m_basesDone) {
        return LCEVC_Again;
    }
    if (m_basePtsToSend > m_afterTheEndPts) {
        return LCEVC_Error;
    }

    // Note that enhancements need to be sent before bases, so check if we've sent the necessary
    // enhancement yet
    if (m_enhancementPtsToSend <= m_basePtsToSend) {
        return LCEVC_Again;
    }

    if (m_bases.empty()) {
        LCEVC_PictureHandle newHandle = {};
        LCEVC_AllocPicture(decHandle, &m_inputDesc, &newHandle);
        m_bases.insert(newHandle.hdl);
    }

    auto baseIter = m_bases.begin();
    const LCEVC_PictureHandle base = {*baseIter};
    m_bases.erase(baseIter);

    // use "this" as arbitrary user data.
    const LCEVC_ReturnCode res = LCEVC_SendDecoderBase(decHandle, m_basePtsToSend, false, base,
                                                       std::numeric_limits<uint32_t>::max(), this);

    if (res == LCEVC_Success) {
        m_basePtsToSend++;
    }

    if (m_basePtsToSend == m_afterTheEndPts) {
        m_basesDone = true;
        return LCEVC_Again;
    }

    return res;
}

LCEVC_ReturnCode EventTester::sendOutput(LCEVC_DecoderHandle decHandle)
{
    if (m_hdl.hdl != decHandle.hdl) {
        return LCEVC_Error;
    }
    if (m_outputsDone) {
        return LCEVC_Again;
    }

    if (m_outputs.empty()) {
        LCEVC_PictureHandle newHandle = {};
        LCEVC_AllocPicture(decHandle, &m_outputDesc, &newHandle);
        m_outputs.insert(newHandle.hdl);
    }

    auto outputIter = m_outputs.begin();
    const LCEVC_PictureHandle output = {*outputIter};
    m_outputs.erase(outputIter);

    return LCEVC_SendDecoderPicture(decHandle, output);
}

LCEVC_ReturnCode EventTester::sendEnhancement(LCEVC_DecoderHandle decHandle)
{
    if (m_hdl.hdl != decHandle.hdl) {
        return LCEVC_Error;
    }
    if (m_enhancementPtsToSend == m_afterTheEndPts || m_enhancementsDone) {
        return LCEVC_Again;
    }
    if (m_enhancementPtsToSend > m_afterTheEndPts) {
        return LCEVC_Error;
    }

    const auto [data, size] = getEnhancement(m_enhancementPtsToSend, kValidEnhancements);
    const LCEVC_ReturnCode res =
        LCEVC_SendDecoderEnhancementData(decHandle, m_enhancementPtsToSend, false, data, size);

    if (res == LCEVC_Success) {
        m_enhancementPtsToSend++;
    }

    if (m_enhancementPtsToSend == m_afterTheEndPts) {
        m_enhancementsDone = true;
        return LCEVC_Again;
    }

    return res;
}

LCEVC_ReturnCode EventTester::receiveOutput()
{
    if (m_outputsDone) {
        return LCEVC_Again;
    }

    // The actual CONTENT of decodeInformation is already tested by receiveOneOutputFromFullDecoder
    // so this test just checks that (1) at least one receive claims to succeed, and (2) it is a
    // picture that matches what we sent.
    LCEVC_PictureHandle picHdl = {};
    LCEVC_DecodeInformation decodeInformation = {};
    const LCEVC_ReturnCode res = LCEVC_ReceiveDecoderPicture(m_hdl, &picHdl, &decodeInformation);
    if (res != LCEVC_Success) {
        // Return early if there's nothing to receive (in other words, a previous called triggered
        // us to call receive, and we did a bunch all at once, and got them all).
        EXPECT_EQ(res, LCEVC_Again);
        return res;
    }

    LCEVC_PictureDesc descReceived = {};
    EXPECT_EQ(LCEVC_GetPictureDesc(m_hdl, picHdl, &descReceived), LCEVC_Success);

    // The received desc probably WON'T match the initial desc (some defaults will be replaced
    // by the actual value from the stream). However, the initial desc SHOULD be the same as
    // what the received desc WOULD be, if all non-user-supplied parameters were defaults.
    LCEVC_PictureDesc equivalentDefaultDesc = {};
    LCEVC_DefaultPictureDesc(&equivalentDefaultDesc, descReceived.colorFormat, descReceived.width,
                             descReceived.height);
    EXPECT_TRUE(lcevc_dec::decoder::equals(equivalentDefaultDesc, m_outputDesc));

    EXPECT_GT(decodeInformation.timestamp, m_latestReceivedPts);

    m_latestReceivedPts = decodeInformation.timestamp;

    if (m_latestReceivedPts == (m_afterTheEndPts - 1)) {
        m_outputsDone = true;
        return LCEVC_Again;
    }

    return res;
}

void EventTester::checkDecInfo(const LCEVC_DecodeInformation& decodeInformation)
{
    EXPECT_LE(decodeInformation.timestamp, m_basePtsToSend);
    EXPECT_LE(decodeInformation.timestamp, m_enhancementPtsToSend);
    EXPECT_TRUE(decodeInformation.hasBase);
    EXPECT_TRUE(decodeInformation.hasEnhancement);
    EXPECT_FALSE(decodeInformation.skipped);
    EXPECT_TRUE(decodeInformation.enhanced);
    EXPECT_EQ(decodeInformation.baseBitdepth, 8);
    EXPECT_EQ(decodeInformation.baseHeight, m_inputDesc.height);
    EXPECT_EQ(decodeInformation.baseWidth, m_inputDesc.width);
    EXPECT_EQ(decodeInformation.baseUserData, this);
}

void EventTester::reuseOutput(LCEVC_PictureHandle picHandle)
{
    ASSERT_EQ(m_outputs.count(picHandle.hdl), 0);
    m_outputs.insert(picHandle.hdl);
}
