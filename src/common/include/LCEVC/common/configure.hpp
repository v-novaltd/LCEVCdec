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

#ifndef VN_LCEVC_COMMON_CONFIGURE_H
#define VN_LCEVC_COMMON_CONFIGURE_H

#include <LCEVC/common/class_utils.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lcevc_dec::common {

// Configurable
//
// Interface for something that can be configured with key,value pairs
//
class Configurable
{
protected:
    Configurable() = default;

public:
    virtual ~Configurable() = 0;

    // Default implementations return false
    virtual bool configure(std::string_view name, bool val);
    virtual bool configure(std::string_view name, int32_t val);
    virtual bool configure(std::string_view name, float val);
    virtual bool configure(std::string_view name, const std::string& val);

    virtual bool configure(std::string_view name, const std::vector<bool>& arr);
    virtual bool configure(std::string_view name, const std::vector<int32_t>& arr);
    virtual bool configure(std::string_view name, const std::vector<float>& arr);
    virtual bool configure(std::string_view name, const std::vector<std::string>& arr);

    VNNoCopyNoMove(Configurable);
};

} // namespace lcevc_dec::common

#endif // VN_LCEVC_COMMON_CONFIGURE_H
