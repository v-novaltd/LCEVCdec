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

#include "trickplay.h"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>

using json = nlohmann::json;

std::vector<Trickplay> parseTrickplayJson(std::string_view jsonStr)
{
    json raw;
    std::vector<Trickplay> config;

    if (jsonStr[0] != '[') {
        // File
        std::ifstream jsonFile(jsonStr.data());
        if (!jsonFile) {
            fmt::print("Trickplay JSON file not found: {}\n", jsonStr.data());
            exit(EXIT_FAILURE);
        }
        std::string str{std::istreambuf_iterator<char>(jsonFile), std::istreambuf_iterator<char>()};
        try {
            raw = json::parse(str.c_str());
        } catch (const json::parse_error&) {
            fmt::print("Trickplay JSON error - invalid JSON\n");
            exit(EXIT_FAILURE);
        }
    } else {
        // Raw json
        try {
            raw = json::parse(jsonStr);
        } catch (const json::parse_error&) {
            fmt::print("Trickplay JSON error - invalid JSON\n");
            exit(EXIT_FAILURE);
        }
    }

    if (raw.type() == json::value_t::array) {
        for (const auto& item : raw) {
            if (!item.contains("action") || !item.contains("frame_num")) {
                fmt::print(
                    "Trickplay JSON error - action and frame_number params must be provided\n");
                exit(EXIT_FAILURE);
            }
            std::string action = item["action"];
            uint64_t frame_num = item["frame_num"];

            if (action == "peek") {
                config.push_back(Trickplay{Peek, frame_num, item["timestamp"], false, false});
            } else if (action == "skip") {
                config.push_back(Trickplay{Skip, frame_num, item["timestamp"], false, false});
            } else if (action == "synchronize") {
                config.push_back(Trickplay{Synchronize, frame_num, 0, item["drop_pending"], false});
            } else if (action == "flush") {
                config.push_back(Trickplay{Flush, frame_num, 0, false, false});
            } else {
                fmt::print("Trickplay JSON error - invalid action name '{}'\n", action);
                exit(EXIT_FAILURE);
            }
        }
    }

    return config;
}

void logTrickplay(Trickplay* trickplay)
{
    switch (trickplay->action) {
        case Peek:
            fmt::print("Trickplay: calling Peek API on timestamp {:#08x} after frame {} has been "
                       "sent\n",
                       trickplay->timestamp, trickplay->frameNum);
            break;
        case Skip:
            fmt::print("Trickplay: calling Skip API on timestamp {:#08x} after frame {} has been "
                       "sent\n",
                       trickplay->timestamp, trickplay->frameNum);
            break;
        case Synchronize:
            fmt::print("Trickplay: calling Synchronize API with dropPending: {} after frame {} has "
                       "been sent\n",
                       trickplay->dropPending, trickplay->frameNum);
            break;
        case Flush:
            fmt::print("Trickplay: calling Flush API after frame {} has been sent\n", trickplay->frameNum);
            break;
    }
}
