# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

include("Sources.cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)

add_executable(lcevc_dec_sample ${SOURCES})
lcevc_set_properties(lcevc_dec_sample)

target_link_libraries(lcevc_dec_sample lcevc_dec::api lcevc_dec::utility fmt::fmt CLI11::CLI11)

add_executable(lcevc_dec::sample ALIAS lcevc_dec_sample)

install(TARGETS "lcevc_dec_sample" DESTINATION "bin")

if(VN_SDK_SAMPLE_SOURCE)
    install(FILES ${SOURCES} DESTINATION "sample_cpp")
    install(FILES "standalone/README.md" DESTINATION "sample_cpp")
    install(FILES "standalone/CMakeLists.txt" DESTINATION "sample_cpp")
    install(FILES "standalone/conanfile.py" DESTINATION "sample_cpp")
    install(FILES "standalone/requirements.txt" DESTINATION "sample_cpp")
    install(DIRECTORY "standalone/cmake" DESTINATION "sample_cpp")
endif()
