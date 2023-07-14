// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Figure out path to the test assets directory
//
#ifndef LCEVC_TEST_FIND_ASSETS_DIR_H
#define LCEVC_TEST_FIND_ASSETS_DIR_H

#include <string>
#include <string_view>

namespace lcevc_dec::utility {

std::string findAssetsDir(std::string_view pathFromRoot);

}

#endif
