// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Get paths relative to executable.
//
#ifndef VN_LCEVC_UTILITY_GET_PROGRAM_DIR_H
#define VN_LCEVC_UTILITY_GET_PROGRAM_DIR_H

#include <string>

namespace lcevc_dec::utility {

/*!
 * \brief Get path of current executable
 *
 * @return         			 Absolute path to current executable
 */

std::string getExecutablePath();

/*!
 * \brief Get directory part of current executable path - optionally add a filename
 *
 * @param[in]       file 	Optional string to append to the path.
 * @return          		Absolute path to current executable with any string appended
 */
std::string getProgramDirectory(std::string_view file = "");

} // namespace lcevc_dec::utility

#endif
