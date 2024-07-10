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

// This tests api/include/LCEVC/lcevc_dec.h against event-based operation. This is a near-duplicate
// of `test_event_manager`, but with a focus on how the code might realistically get used, in case
// certain use cases throw up unique problems (for example, the event_manager tests wouldn't catch
// deadlocks caused by accessing the API from within a callback).

#include "test_api_events.h"

#include "data.h"
#include "interface.h"

#include <gtest/gtest.h>

#include <cctype>

// ------------------------------------------------------------------------------------------------

static void callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
                     const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
                     uint32_t dataSize, void* userData)
{
    ASSERT_NE(userData, nullptr);
    auto& tester = *(static_cast<DecodeTester*>(userData));
    tester.callback(decHandle, event, picHandle, decodeInformation, data, dataSize);
}

// DecodeTester -----------------------------------------------------------------------------------

void DecodeTester::setup()
{
    LCEVC_AccelContextHandle dummyHdl = {};
    ASSERT_EQ(LCEVC_CreateDecoder(&m_hdl, dummyHdl), LCEVC_Success);
    ASSERT_EQ(LCEVC_ConfigureDecoderIntArray(m_hdl, "events", static_cast<uint32_t>(kAllEvents.size()),
                                             kAllEvents.data()),
              LCEVC_Success);
    ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_hdl, "core_threads", 1), LCEVC_Success);
    ASSERT_EQ(LCEVC_SetDecoderEventCallback(m_hdl, ::callback, static_cast<void*>(this)), LCEVC_Success);
    LCEVC_DefaultPictureDesc(&m_inputDesc, LCEVC_I420_8, 960, 540);
    LCEVC_DefaultPictureDesc(&m_outputDesc, LCEVC_I420_8, 1920, 1080);

    ASSERT_EQ(LCEVC_InitializeDecoder(m_hdl), LCEVC_Success);
}

void DecodeTester::teardown()
{
    // This blocks any new sends (in case we're tearing down after a timeout).
    m_basePtsToSend = m_enhancementPtsToSend = m_latestReceivedPts = m_afterTheEndPts;

    // Receive any pending outputs before destroying, or else you might set certain outputs to NULL
    // while the decoder is writing to those destinations.
    receiveOutput(false);

    LCEVC_DestroyDecoder(m_hdl);
    m_tornDown = true;
}

void DecodeTester::callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
                            const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
                            uint32_t dataSize)
{
    switch (event) {
        case LCEVC_Log: log(data, dataSize); break;
        case LCEVC_Exit: exit(); break;
        case LCEVC_CanSendBase: sendBase(decHandle); break;
        case LCEVC_CanSendEnhancement: sendEnhancement(decHandle); break;
        case LCEVC_CanSendPicture: sendOutput(decHandle); break;
        case LCEVC_CanReceive: receiveOutput(); break;
        case LCEVC_BasePictureDone: reuseBase(picHandle); break;
        case LCEVC_OutputPictureDone: reuseOutput(picHandle, decodeInformation); break;

        case LCEVC_EventCount:
        case LCEVC_Event_ForceUInt8: FAIL() << "Invalid event type: " << event; break;
    }
    increment(event);
}

void DecodeTester::log(const uint8_t* data, uint32_t dataSize)
{
    ASSERT_NE(data, nullptr);

    // Not a lot we can do here except check that every character is printable...
    for (uint32_t idx = 0; idx < dataSize; idx++) {
        ASSERT_TRUE(isprint(data[idx]));
    }
}

void DecodeTester::exit() { ASSERT_EQ(m_eventCounts[LCEVC_Exit], 0); }

void DecodeTester::sendBase(LCEVC_DecoderHandle decHandle)
{
    ASSERT_EQ(m_hdl.hdl, decHandle.hdl);
    if (m_afterTheEndPts == m_basePtsToSend) {
        return;
    }
    ASSERT_LT(m_basePtsToSend, m_afterTheEndPts);

    if (m_bases.empty()) {
        LCEVC_PictureHandle newHandle = {};
        LCEVC_AllocPicture(decHandle, &m_inputDesc, &newHandle);
        m_bases.insert(newHandle.hdl);
    }

    auto baseIter = m_bases.begin();
    LCEVC_PictureHandle base = {*baseIter};
    m_bases.erase(baseIter);

    // use "this" as arbitrary user data.
    ASSERT_EQ(LCEVC_SendDecoderBase(decHandle, m_basePtsToSend, false, base,
                                    std::numeric_limits<uint32_t>::max(), this),
              LCEVC_Success);

    m_basePtsToSend++;
    if (isDone()) {
        m_atomicIsDone = true;
    }
}

void DecodeTester::sendOutput(LCEVC_DecoderHandle decHandle)
{
    ASSERT_EQ(m_hdl.hdl, decHandle.hdl);
    if (m_outputs.empty()) {
        LCEVC_PictureHandle newHandle = {};
        LCEVC_AllocPicture(decHandle, &m_outputDesc, &newHandle);
        m_outputs.insert(newHandle.hdl);
    }

    auto outputIter = m_outputs.begin();
    LCEVC_PictureHandle output = {*outputIter};
    m_outputs.erase(outputIter);

    ASSERT_EQ(LCEVC_SendDecoderPicture(decHandle, output), LCEVC_Success);
}

void DecodeTester::sendEnhancement(LCEVC_DecoderHandle decHandle)
{
    ASSERT_EQ(m_hdl.hdl, decHandle.hdl);
    if (m_afterTheEndPts == m_enhancementPtsToSend) {
        return;
    }
    ASSERT_LT(m_enhancementPtsToSend, m_afterTheEndPts);

    const auto [data, size] = getEnhancement(m_enhancementPtsToSend, kValidEnhancements);
    ASSERT_EQ(LCEVC_SendDecoderEnhancementData(decHandle, m_enhancementPtsToSend, false, data, size),
              LCEVC_Success);

    m_enhancementPtsToSend++;
    if (isDone()) {
        m_atomicIsDone = true;
    }
}

void DecodeTester::receiveOutput(bool expectOutput)
{
    LCEVC_PictureHandle picHdl = {};
    LCEVC_DecodeInformation decodeInformation = {};

    // The actual CONTENT of decodeInformation is already tested by receiveOneOutputFromFullDecoder
    // so this test just checks that (1) at least one receive claims to succeed, and (2) it is a
    // picture that matches what we sent.
    bool anySuccesses = false;
    while (LCEVC_ReceiveDecoderPicture(m_hdl, &picHdl, &decodeInformation) == LCEVC_Success) {
        anySuccesses = true;

        LCEVC_PictureDesc descReceived = {};
        EXPECT_EQ(LCEVC_GetPictureDesc(m_hdl, picHdl, &descReceived), LCEVC_Success);

        // The received desc probably WON'T match the initial desc (some defaults will be replaced
        // by the actual value from the stream). However, the initial desc SHOULD be the same as
        // what the received desc WOULD be, if all non-user-supplied parameters were defaults.
        LCEVC_PictureDesc equivalentDefaultDesc = {};
        LCEVC_DefaultPictureDesc(&equivalentDefaultDesc, descReceived.colorFormat,
                                 descReceived.width, descReceived.height);
        EXPECT_TRUE(lcevc_dec::decoder::equals(equivalentDefaultDesc, m_outputDesc));

        if (decodeInformation.timestamp > m_latestReceivedPts) {
            m_latestReceivedPts = decodeInformation.timestamp;
            if (isDone()) {
                m_atomicIsDone = true;
            }
        }
    }

    if (expectOutput) {
        EXPECT_TRUE(anySuccesses);
    }
}

void DecodeTester::reuseBase(LCEVC_PictureHandle picHandle)
{
    if (isDone()) {
        return;
    }

    ASSERT_EQ(m_bases.count(picHandle.hdl), 0);
    m_bases.insert(picHandle.hdl);

    // Note that enhancements must be sent before bases.
    sendEnhancement(m_hdl);
    sendBase(m_hdl);
}

void DecodeTester::reuseOutput(LCEVC_PictureHandle picHandle, const LCEVC_DecodeInformation* decodeInformation)
{
    if (isDone()) {
        return;
    }

    ASSERT_NE(decodeInformation, nullptr);
    EXPECT_LE(decodeInformation->timestamp, m_basePtsToSend);
    EXPECT_LE(decodeInformation->timestamp, m_enhancementPtsToSend);
    EXPECT_TRUE(decodeInformation->hasBase);
    EXPECT_TRUE(decodeInformation->hasEnhancement);
    EXPECT_FALSE(decodeInformation->skipped);
    EXPECT_TRUE(decodeInformation->enhanced);
    EXPECT_EQ(decodeInformation->baseBitdepth, 8);
    EXPECT_EQ(decodeInformation->baseHeight, m_inputDesc.height);
    EXPECT_EQ(decodeInformation->baseWidth, m_inputDesc.width);
    EXPECT_EQ(decodeInformation->baseUserData, this);

    ASSERT_EQ(m_outputs.count(picHandle.hdl), 0);
    m_outputs.insert(picHandle.hdl);
    sendOutput(m_hdl);
}

// Tests ------------------------------------------------------------------------------------------

TEST(APIEventReporting, Test)
{
    DecodeTester tester(100);

    // This will be fully automatic:
    // Test program                    | Decoder fn    | Decoder event
    // --------------------------------------------------------------
    // setup ->                        | initialise -> | "send" events ->
    // send callbacks ->               | decode ->     | "output/base done" events ->
    // "reuse" callbacks, new sends -> | decode ->     | More "done" events, repeat <--
    tester.setup();

    // Wait until tester is done, with a 15 second timeout for the entire 100-frame decode loop
    // (locally, this took about 500ms, and 15 seconds was sufficient even for coverage builds).
    bool wasTimeout = false;
    const std::chrono::milliseconds timeout(45000);
    ASSERT_TRUE(
        atomicWaitUntilTimeout(wasTimeout, timeout, [&tester]() { return tester.atomicIsDone(); }));
    EXPECT_FALSE(wasTimeout);

    tester.teardown();

    for (uint8_t eventType = 0; eventType < LCEVC_EventCount; eventType++) {
        if (eventType == LCEVC_Log) {
            // LCEVC_Log is currently the only unused event type. This is likely to change as part
            // of DEC-416
            continue;
        }

        EXPECT_NE(tester.getCount(static_cast<LCEVC_Event>(eventType)), 0);
    }
}
