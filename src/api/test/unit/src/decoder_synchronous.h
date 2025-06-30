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

#ifndef VN_LCEVC_API_DECODER_SYNCHRONOUS_H
#define VN_LCEVC_API_DECODER_SYNCHRONOUS_H

#include "event_tester.h"

#include <LCEVC/lcevc_dec.h>

#include <cstdint>
#include <mutex>

class DecoderSynchronous : public EventTester
{
    using BaseClass = EventTester;
    using BaseClass::BaseClass;

public:
    void callback(LCEVC_DecoderHandle decHandle, LCEVC_Event event, LCEVC_PictureHandle picHandle,
                  const LCEVC_DecodeInformation* decodeInformation, const uint8_t* data,
                  uint32_t dataSize) override;

    bool atomicIsDone() override;

protected:
    LCEVC_ReturnCode sendBase(LCEVC_DecoderHandle decHandle) override;
    LCEVC_ReturnCode sendEnhancement(LCEVC_DecoderHandle decHandle) override;
    LCEVC_ReturnCode sendOutput(LCEVC_DecoderHandle decHandle) override;
    LCEVC_ReturnCode receiveOutput() override;

private:
    std::mutex m_doneFlagsMutex;
};

#endif // VN_LCEVC_API_DECODER_SYNCHRONOUS_H
