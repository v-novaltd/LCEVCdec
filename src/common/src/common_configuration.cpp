/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include <LCEVC/common/common_configuration.h>
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/configure.hpp>
#include <LCEVC/common/configure_members.hpp>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>

namespace lcevc_dec::common {

class CommonConfiguration
{
public:
    bool setLogToStdout(const bool& val)
    {
        if (!m_logStdout && val) {
            ldcDiagnosticsHandlerPush(ldcDiagHandlerStdio, stdout);
            VNLogInfo("Logging to stdout");
        } else if (m_logStdout && !val) {
            ldcDiagnosticsHandlerPop(ldcDiagHandlerStdio, nullptr);
        }
        m_logStdout = val;
        return true;
    }

    bool setTraceFile(const std::string& val)
    {
        if (val == m_traceFile) {
            return true;
        }

        if (!m_traceFile.empty()) {
            ldcDiagTraceFileRelease();
            m_traceFile.clear();
        }

        if (!val.empty()) {
            if (!ldcDiagTraceFileInitialize(val.c_str())) {
                VNLogErrorF("Could not open trace file: %s", val.c_str());
            } else {
                m_traceFile = val;
            }
        }
        return true;
    }

    bool setDisableSIMD(const bool& val)
    {
        ldcAccelerationInitialize(!val);
        return true;
    }

    bool setLogLevel(const int32_t& val)
    {
        if (val < 0 || val >= LdcLogLevelCount) {
            return false;
        }
        ldcDiagnosticsLogLevel(static_cast<LdcLogLevel>(val));
        return true;
    }

    bool setLogLevels(const std::vector<int32_t>& arr)
    {
        VNLogError("Diagnostics does not support separate log levels.");
        return false;
    }

private:
    bool m_logStdout = false;
    std::string m_traceFile;
};

static const common::ConfigMemberMap<CommonConfiguration> kConfigMemberMap = {
    {"log_stdout", makeBinding(&CommonConfiguration::setLogToStdout)},
    {"disable_simd", makeBinding(&CommonConfiguration::setDisableSIMD)},
    {"log_level", makeBinding(&CommonConfiguration::setLogLevel)},
    {"trace_file", makeBinding(&CommonConfiguration::setTraceFile)},
    {"log_levels", makeBinding(&CommonConfiguration::setLogLevels)},
};
//
//
Configurable* getCommonConfiguration()
{
    static CommonConfiguration configuration;
    static ConfigurableMembers config(kConfigMemberMap, configuration);
    return &config;
}

} // namespace lcevc_dec::common
