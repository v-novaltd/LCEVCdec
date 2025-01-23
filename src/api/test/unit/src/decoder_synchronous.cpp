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

#include "decoder_synchronous.h"

#include "event_tester.h"

#include <gtest/gtest.h>
#include <LCEVC/lcevc_dec.h>

#include <cstdint>
#include <mutex>

// DecoderSynchronous ----------------------------------------------------------------------------

void DecoderSynchronous::callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event,
                                  LCEVC_PictureHandle picHandle,
                                  const LCEVC_DecodeInformation* decodeInformation,
                                  const uint8_t* data, uint32_t dataSize)
{
    switch (event) {
        case LCEVC_Log: log(data, dataSize); break;
        case LCEVC_Exit: exit(); break;
        case LCEVC_CanSendBase: EXPECT_EQ(kSuccesses.count(sendBase(decHandle)), 1); break;
        case LCEVC_CanSendEnhancement:
            EXPECT_EQ(kSuccesses.count(sendEnhancement(decHandle)), 1);
            break;
        case LCEVC_CanSendPicture: EXPECT_EQ(kSuccesses.count(sendOutput(decHandle)), 1); break;
        case LCEVC_CanReceive: EXPECT_EQ(kSuccesses.count(receiveOutput()), 1); break;
        case LCEVC_BasePictureDone: {
            EXPECT_EQ(getNumUnsentBases(), 0);
            reuseBase(picHandle);
            EXPECT_EQ(kSuccesses.count(sendEnhancement(decHandle)), 1);
            EXPECT_EQ(kSuccesses.count(sendBase(decHandle)), 1);
            break;
        }
        case LCEVC_OutputPictureDone: {
            ASSERT_NE(decodeInformation, nullptr);
            checkDecInfo(*decodeInformation);
            reuseOutput(picHandle);
            EXPECT_EQ(kSuccesses.count(sendOutput(decHandle)), 1);
            break;
        }

        case LCEVC_EventCount:
        case LCEVC_Event_ForceUInt8: FAIL() << "Invalid event type: " << event; break;
    }
    increment(event);
}

LCEVC_ReturnCode DecoderSynchronous::sendBase(LCEVC_DecoderHandle hdl)
{
    const std::scoped_lock lock(m_doneFlagsMutex);
    return BaseClass::sendBase(hdl);
}

LCEVC_ReturnCode DecoderSynchronous::sendEnhancement(LCEVC_DecoderHandle hdl)
{
    const std::scoped_lock lock(m_doneFlagsMutex);
    return BaseClass::sendEnhancement(hdl);
}

LCEVC_ReturnCode DecoderSynchronous::sendOutput(LCEVC_DecoderHandle decHandle)
{
    const std::scoped_lock lock(m_doneFlagsMutex);
    return BaseClass::sendOutput(decHandle);
}

LCEVC_ReturnCode DecoderSynchronous::receiveOutput()
{
    const std::scoped_lock lock(m_doneFlagsMutex);
    return BaseClass::receiveOutput();
}

bool DecoderSynchronous::atomicIsDone()
{
    const std::scoped_lock lock(m_doneFlagsMutex);
    return getBasesDone() && getEnhancementsDone() && getOutputsDone();
}
