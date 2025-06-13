/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

// Get paths relative to executable.
//
#ifndef VN_LCEVC_UTILITY_GET_PROGRAM_DIR_H
#define VN_LCEVC_UTILITY_GET_PROGRAM_DIR_H

#include <LCEVC/build_config.h>

#include <string>

namespace lcevc_dec::utility {

/*!
 * \brief Get path of current executable
 *
 * @return         			 Absolute path to current executable
 */

std::string getExecutablePath();

/*!
 * \brief Get directory part of current executable path - optionally add a filename. Not currently
 *        available on iOS, due to the lack of filesystem::path on older versions.
 *
 * @param[in]       file    Optional string to append to the path.
 * @return          		Absolute path to current executable with any string appended
 */
#if !VN_OS(IOS) && !VN_OS(TVOS)
std::string getProgramDirectory(std::string_view file = "");
#endif

} // namespace lcevc_dec::utility

#endif
