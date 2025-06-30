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

// Write diagnostics to an ostream - overrides other handlers
//
// This is principally aimed at testing, so does not include source coordinates
//
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/platform.h>

#include <cassert>
#include <ostream>

class DiagnosticsCapture
{};

extern "C" bool LCEVC_DiagHandlerOStream(void* user, const LdcDiagSite* site,
                                         const LdcDiagRecord* record, const LdcDiagValue* values)
{
    std::ostream* ostream = (std::ostream*)user;
    const char* levelName = "";
    char buffer[4096]; // Could make thread local?

    // Erase timestamps and pid/tid so that strings will compare OK
    LdcDiagRecord fixedRecord = {0};
    memcpy(&fixedRecord, record, sizeof(LdcDiagRecord));
    fixedRecord.timestamp = 0;
    fixedRecord.threadId = 2;

    switch (site->level) {
        case LdcLogLevelNone: levelName = "None"; break;
        case LdcLogLevelFatal: levelName = "Fatal"; break;
        case LdcLogLevelError: levelName = "Error"; break;
        case LdcLogLevelWarning: levelName = "Warning"; break;
        case LdcLogLevelInfo: levelName = "Info"; break;
        case LdcLogLevelDebug: levelName = "Debug"; break;
        case LdcLogLevelVerbose: levelName = "Verbose"; break;
        default: assert(0);
    }

    switch (site->type) {
        case LdcDiagTypeLog:
            ldcDiagnosticFormatLog(buffer, sizeof(buffer), site, record, values);
            fmt::print(*ostream, "{}: {}\n", levelName, buffer);
            break;

        case LdcDiagTypeLogFormatted:
            fmt::print(*ostream, "{} (formatted): {}\n", levelName, (const char*)values);
            break;

        case LdcDiagTypeTraceBegin:
        case LdcDiagTypeTraceEnd:
        case LdcDiagTypeTraceScoped:
        case LdcDiagTypeTraceInstant:
        case LdcDiagTypeTraceAsyncBegin:
        case LdcDiagTypeTraceAsyncEnd:
        case LdcDiagTypeTraceAsyncInstant:
            ldcDiagnosticFormatJson(buffer, sizeof(buffer), site, &fixedRecord, 1);
            fmt::print(*ostream, "Trace: {}\n", buffer);
            break;
        case LdcDiagTypeMetric:
            ldcDiagnosticFormatJson(buffer, sizeof(buffer), site, &fixedRecord, 1);
            fmt::print(*ostream, "Metric: {}\n", buffer);
            break;

        default: break;
    }

    return true;
}
