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

// Diagnostic handler that writes log records to stdout
//
#include <assert.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/platform.h>
#include <stdbool.h>
#include <stdio.h>

static bool diagnosticHandlerTraceFile(void* user, const LdcDiagSite* site,
                                       const LdcDiagRecord* record, const LdcDiagValue* value)
{
    FILE* output = user;

    if (site->type != LdcDiagTypeTraceScoped && site->type != LdcDiagTypeTraceInstant &&
        site->type != LdcDiagTypeTraceAsyncBegin && site->type != LdcDiagTypeTraceAsyncEnd &&
        site->type != LdcDiagTypeTraceAsyncInstant && site->type != LdcDiagTypeMetric) {
        return false;
    }

    char buffer[4096]; // Could make thread local?
    ldcDiagnosticFormatJson(buffer, sizeof(buffer), site, record, VNGetProcessId());
    fputs(buffer, output);
    return true;
}

bool ldcDiagTraceFileInitialize(const char* filename)
{
    FILE* output = fopen(filename, "wb");
    fputs("[\n", output);

    if (!output) {
        VNLogError("Cannot open trace file");
        return false;
    }

    ldcDiagnosticsHandlerPush(diagnosticHandlerTraceFile, output);

    return true;
}

bool ldcDiagTraceFileRelease(void)
{
    void* userData = 0;
    if (!ldcDiagnosticsHandlerPop(diagnosticHandlerTraceFile, &userData)) {
        VNLogError("Cannot pop diagnostics handler");
        return false;
    }

    // Recover file handle
    assert(userData);
    FILE* output = (FILE*)userData;

    // Terminate the log and close
    if (fputs("]\n", output) < 0 || fclose(output) < 0) {
        VNLogError("Cannot close trace file");
        return false;
    }

    return true;
}
