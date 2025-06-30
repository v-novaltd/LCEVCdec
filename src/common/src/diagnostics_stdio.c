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

// Diagnostic handler that writes log records to a stdio FILE*
//
#include <assert.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/platform.h>
#include <stdbool.h>
#include <stdio.h>

bool ldcDiagHandlerStdio(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                         const LdcDiagValue* values)
{
    FILE* output = user;
    char buffer[4096];
    const char* cp = 0;

    if (site->type == LdcDiagTypeLog) {
        ldcDiagnosticFormatLog(buffer, sizeof(buffer), site, record, values);
        cp = buffer;
    } else if (site->type == LdcDiagTypeLogFormatted) {
        if (values) {
            cp = (const char*)values;
        } else {
            cp = "<No Value>";
        }
    } else {
        return false;
    }

    switch (site->level) {
        case LdcLogLevelFatal:
            fprintf(output, "%s:%d Fatal: %s\n", site->file, site->line, cp);
            break;
        case LdcLogLevelError:
            fprintf(output, "%s:%d Error: %s\n", site->file, site->line, cp);
            break;
        case LdcLogLevelWarning: fprintf(output, "Warning: %s\n", cp); break;
        case LdcLogLevelInfo: fprintf(output, "Info: %s\n", cp); break;
        case LdcLogLevelDebug:
            fprintf(output, "%s:%d Debug: %s\n", site->file, site->line, cp);
            break;
        case LdcLogLevelVerbose:
            fprintf(output, "%s:%d Verbose: %s\n", site->file, site->line, cp);
            break;
        default: assert(0); break;
    }
    fflush(output);

    return true;
}
