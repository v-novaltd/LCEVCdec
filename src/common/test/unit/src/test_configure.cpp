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

#include <LCEVC/common/configure.hpp>
#include <LCEVC/common/configure_members.hpp>
//
#include <fmt/core.h>
#include <gtest/gtest.h>
//
#include <array>

using namespace lcevc_dec::common;

// The configurable values that get passed from the build to the initialized pipeline
//
struct SomeConfiguration
{
    bool configBool = false;
    int32_t configInt = 0;
    float configFloat = 0.0f;
    std::string configString;

    std::array<bool, 4> configVecBool = {0};
    std::array<int32_t, 8> configVecInt = {0};
    std::array<float, 6> configVecFloat = {0};
    std::array<std::string, 3> configVecString = {"", "", ""};
};

class ConfigFixture : public testing::Test
{
public:
    ConfigFixture() = default;

    void SetUp() override {}

    SomeConfiguration config;
    ConfigMemberMap<SomeConfiguration> configMemberMap{{
        {"config_bool", makeBinding(&SomeConfiguration::configBool)},
        {"config_int", makeBinding(&SomeConfiguration::configInt)},
        {"config_float", makeBinding(&SomeConfiguration::configFloat)},
        {"config_string", makeBinding(&SomeConfiguration::configString)},
        {"config_bools", makeBindingArray(&SomeConfiguration::configVecBool)},
        {"config_ints", makeBindingArray(&SomeConfiguration::configVecInt)},
        {"config_floats", makeBindingArray(&SomeConfiguration::configVecFloat)},
        {"config_strings", makeBindingArray(&SomeConfiguration::configVecString)},
    }};
    ConfigurableMembers<SomeConfiguration> configurableMembers{configMemberMap, config};
    Configurable* configurable{&configurableMembers};
};

TEST_F(ConfigFixture, Defaults)
{
    EXPECT_EQ(config.configBool, false);
    EXPECT_EQ(config.configInt, 0);
    EXPECT_EQ(config.configFloat, 0.0f);
    EXPECT_EQ(config.configString, "");

    for (const auto& a : config.configVecBool) {
        EXPECT_EQ(a, false);
    }

    for (const auto& a : config.configVecInt) {
        EXPECT_EQ(a, 0);
    }

    for (const auto& a : config.configVecFloat) {
        EXPECT_EQ(a, 0.0f);
    }

    for (const auto& a : config.configVecString) {
        EXPECT_EQ(a, "");
    }
}

TEST_F(ConfigFixture, SetBool)
{
    EXPECT_EQ(configurable->configure("config_bool", true), true);
    EXPECT_EQ(config.configBool, true);

    EXPECT_EQ(configurable->configure("config_int", true), false);
    EXPECT_EQ(config.configBool, true);

    EXPECT_EQ(configurable->configure("config_float", true), false);
    EXPECT_EQ(config.configBool, true);

    EXPECT_EQ(configurable->configure("config_string", true), false);
    EXPECT_EQ(config.configBool, true);
}

TEST_F(ConfigFixture, SetInt)
{
    EXPECT_EQ(configurable->configure("config_int", 42), true);
    EXPECT_EQ(config.configInt, 42);

    EXPECT_EQ(configurable->configure("config_bool", 42), false);
    EXPECT_EQ(config.configInt, 42);

    EXPECT_EQ(configurable->configure("config_float", 42), false);
    EXPECT_EQ(config.configInt, 42);

    EXPECT_EQ(configurable->configure("config_string", 42), false);
    EXPECT_EQ(config.configInt, 42);
}

TEST_F(ConfigFixture, SetFloat)
{
    EXPECT_EQ(configurable->configure("config_float", 123456.789f), true);
    EXPECT_EQ(config.configFloat, 123456.789f);

    EXPECT_EQ(configurable->configure("config_bool", 123456.789f), false);
    EXPECT_EQ(config.configFloat, 123456.789f);

    EXPECT_EQ(configurable->configure("config_int", 123456.789f), false);
    EXPECT_EQ(config.configFloat, 123456.789f);

    EXPECT_EQ(configurable->configure("config_string", 123456.789f), false);
    EXPECT_EQ(config.configFloat, 123456.789f);
}

TEST_F(ConfigFixture, SetString)
{
    EXPECT_EQ(configurable->configure("config_string", std::string("hello world")), true);
    EXPECT_EQ(config.configString, "hello world");

    EXPECT_EQ(configurable->configure("config_bool", std::string("hello world")), false);
    EXPECT_EQ(config.configString, "hello world");

    EXPECT_EQ(configurable->configure("config_int", std::string("hello world")), false);
    EXPECT_EQ(config.configString, "hello world");

    EXPECT_EQ(configurable->configure("config_float", std::string("hello world")), false);
    EXPECT_EQ(config.configString, "hello world");
}

TEST_F(ConfigFixture, SetBoolArray)
{
    std::vector<bool> arr = {true, false, true, false};
    EXPECT_EQ(configurable->configure("config_bools", arr), true);

    EXPECT_EQ(arr.size(), config.configVecBool.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], config.configVecBool[i]);
    }
}

TEST_F(ConfigFixture, SetIntArray)
{
    std::vector<int32_t> arr = {234, 12123, 3, 987, 345345, 4, 667, 23};
    EXPECT_EQ(configurable->configure("config_ints", arr), true);

    EXPECT_EQ(arr.size(), config.configVecInt.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], config.configVecInt[i]);
    }
}

TEST_F(ConfigFixture, SetFloatArray)
{
    std::vector<float> arr = {34.5f, 5.2e10f, -100.0f, -34.79e-14f, 8970987.768f, 234.0f};
    EXPECT_EQ(configurable->configure("config_floats", arr), true);

    EXPECT_EQ(arr.size(), config.configVecFloat.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], config.configVecFloat[i]);
    }
}

TEST_F(ConfigFixture, SetStringArray)
{
    std::vector<std::string> arr = {"red", "lorry", "yellow"};
    EXPECT_EQ(configurable->configure("config_strings", arr), true);

    EXPECT_EQ(arr.size(), config.configVecString.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], config.configVecString[i]);
    }
}
