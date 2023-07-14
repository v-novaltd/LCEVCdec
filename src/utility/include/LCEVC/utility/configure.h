// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Generate LCEVC_DEC API LCEVC_Configure... calls from json
//
#ifndef VN_LCEVC_UTILITY_CONFIGURE_H
#define VN_LCEVC_UTILITY_CONFIGURE_H

#include "LCEVC/lcevc_dec.h"

#include <string_view>

namespace lcevc_dec::utility {

/*!
 * \brief Extract configuration from JSON string or file and apply it to a decoder.
 *
 * If 'json' starts with '[', it is treated as a JSON string, otherwise it is treated
 * as a filename.
 *
 * @param[in]       decoder 		Decoder to be configured
 * @param[in]       json            JSON string of filename
 * @return 					        LCEVC_Sucess
 *                  				LCEVC_NotFound
 *                  				LCEVC_Error
 */
LCEVC_ReturnCode configureDecoderFromJson(LCEVC_DecoderHandle decoder, std::string_view json);

} // namespace lcevc_dec::utility

#endif
