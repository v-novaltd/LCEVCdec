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

#include "base_decoder_bin.h"

#include "LCEVC/utility/bin_reader.h"
#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/raw_reader.h"

#include <fmt/core.h>

#include <cstdint>
#include <iterator>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace lcevc_dec::utility {

bool BaseDecoderBin::isInitialised() const { return m_rawReader && m_binReader; }

// Work out starting PTS and PTS increment - by looking at up to first 100 frames
//
bool BaseDecoderBin::probe(std::string_view binFile)
{
    constexpr uint32_t kProbeFrameLimit = 100;

    auto binReader = createBinReader(binFile);
    if (!binReader) {
        return false;
    }

    std::set<int64_t> timestamps;

    uint32_t count = 0;
    for (; count < kProbeFrameLimit; ++count) {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;

        if (std::vector<uint8_t> payload; !binReader->read(decodeIndex, presentationIndex, payload)) {
            break;
        }
        timestamps.insert(presentationIndex);
    }

    if (timestamps.empty()) {
        fmt::print("base_decoder_bin: probe found empty BIN file.\n");
        return false;
    }

    if (timestamps.size() != count) {
        fmt::print("base_decoder_bin: probe found duplicate timestamps in BIN file.\n");
        return false;
    }

    m_timestampStart = *timestamps.begin();

    if (timestamps.size() == 1) {
        // If there is only one frame, guess '1' for step
        m_timestampStep = 1;
        return true;
    }

    m_timestampStep = *std::next(timestamps.begin()) - m_timestampStart;

    int64_t testTimestamp = m_timestampStart;
    for (const auto ts : timestamps) {
        if (ts != testTimestamp) {
            fmt::print("base_decoder_bin: warning: probe found missing timestamp {}.\n", ts);
            break;
        }
        testTimestamp += m_timestampStep;
    }

    return true;
}

BaseDecoderBin::BaseDecoderBin(std::string_view rawFile, std::string_view binFile)
{
    if (!probe(binFile)) {
        return;
    }

    auto binReader = createBinReader(binFile);
    if (!binReader) {
        return;
    }

    auto rawReader = createRawReader(rawFile);
    if (!rawReader) {
        return;
    }

    m_pictureDesc = rawReader->description();
    m_pictureLayout = PictureLayout(m_pictureDesc);
    m_rawReader = std::move(rawReader);
    m_binReader = std::move(binReader);

    m_lastBaseTimestamp = m_timestampStart;
}

} // namespace lcevc_dec::utility
