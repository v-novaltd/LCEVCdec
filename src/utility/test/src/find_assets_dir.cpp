// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Figure out path to the test assets directory
//
#include "find_assets_dir.h"


#include "LCEVC/utility/get_program_dir.h"

#include <fmt/core.h>

#include "filesystem.h"

#include <string>

namespace lcevc_dec::utility {

constexpr unsigned kMaxLevels = 4;

std::string findAssetsDir(std::string_view pathFromRoot)
{
    // Try path as specified
    if (filesystem::is_directory(pathFromRoot)) {
        return filesystem::canonical(pathFromRoot).string();
    }

    // Try relative to program directory .. prepend '..' up to four times
    const filesystem::path programDir = getProgramDirectory();
    filesystem::path dir(pathFromRoot);

    for (unsigned i = 0; i < kMaxLevels; ++i) {
        if (filesystem::is_directory(programDir / dir)) {
            return (programDir / dir).string();
        }
        dir = ".." / dir;
    }

    fmt::print(stderr, "Could not find asset directory {}\n", pathFromRoot);
    std::exit(EXIT_FAILURE);
}

} // namespace lcevc_dec::utility
