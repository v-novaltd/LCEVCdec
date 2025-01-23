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

// Intermediate base class. The two bin readers inherit from this.
//
#ifndef VN_LCEVC_UTILITY_BASE_DECODER_BIN_H
#define VN_LCEVC_UTILITY_BASE_DECODER_BIN_H

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/base_decoder.h"
#include "LCEVC/utility/bin_reader.h"
#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/raw_reader.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace lcevc_dec::utility {
class BaseDecoderBin : public BaseDecoder
{
    using BaseClass = BaseDecoder;

protected:
    BaseDecoderBin() = default;

public:
    BaseDecoderBin(std::string_view rawFile, std::string_view binFile);

    bool isInitialised() const;

    const LCEVC_PictureDesc& description() const override { return m_pictureDesc; }
    const PictureLayout& layout() const override { return m_pictureLayout; }

    int maxReorder() const override { return 0; }

protected:
    bool getBinGood() const { return m_binGood; }
    void setBinGood(const bool binGood) { m_binGood = binGood; }
    bool getRawGood() const { return m_rawGood; }
    void setRawGood(const bool rawGood) { m_rawGood = rawGood; }

    bool binRead(int64_t& decodeIndex, int64_t& presentationIndex, std::vector<uint8_t>& payload)
    {
        return m_binReader->read(decodeIndex, presentationIndex, payload);
    }
    bool rawRead(std::vector<uint8_t>& memory) { return m_rawReader->read(memory); }

    int64_t getLastBaseTimestamp() const { return m_lastBaseTimestamp; }
    void incrementLastBaseTimestamp() { m_lastBaseTimestamp += m_timestampStep; }

private:
    bool probe(std::string_view binFile);

    LCEVC_PictureDesc m_pictureDesc = {};
    PictureLayout m_pictureLayout;

    // The readers for source files
    std::unique_ptr<RawReader> m_rawReader;
    std::unique_ptr<BinReader> m_binReader;

    // Probed values from presentation timestamp in BIN file
    int64_t m_timestampStart = 0;
    int64_t m_timestampStep = 0;

    // The timestamp of the last base that we read
    int64_t m_lastBaseTimestamp = 0;

    // True if the BIN file is still good
    bool m_binGood = true;
    // True if the RAW file is still good
    bool m_rawGood = true;
};

} // namespace lcevc_dec::utility

#endif // VN_LCEVC_UTILITY_BASE_DECODER_BIN_H
