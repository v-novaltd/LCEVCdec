# Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
# software may be incorporated into a project under a compatible license provided the requirements
# of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
# licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
# ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
# THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

# Doxygen only consumes selected externally facing headers referenced by the docs
set(DOXYGEN_INPUTS
    "${PROJECT_SOURCE_DIR}/src/api/include/LCEVC/lcevc_dec.h"
    "${PROJECT_SOURCE_DIR}/src/enhancement/include/LCEVC/enhancement"
    "${PROJECT_SOURCE_DIR}/src/common/include/LCEVC/common")
string(REPLACE ";" " " DOXYGEN_INPUT "${DOXYGEN_INPUTS}")
set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# Configure the Doxyfile to allow cmake to add paths dynamically
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
add_custom_command(
    OUTPUT ${DOXYGEN_INDEX_FILE}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    MAIN_DEPENDENCY ${DOXYFILE_OUT}
    ${DOXYFILE_IN}
    DEPENDS ${DOXYGEN_INPUTS}
    COMMENT "Generating docs Doxygen XML")

add_custom_target(doxygen DEPENDS ${DOXYGEN_INDEX_FILE})

set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/docs)

add_custom_target(
    documentation ALL
    DEPENDS doxygen
    COMMAND
        ${SPHINX_EXECUTABLE} ${SPHINX_SOURCE} ${SPHINX_BUILD} -b html -W -n -a
        -Dversion=${GIT_SHORT_VERSION} -Drelease=${GIT_VERSION}
        -Dbreathe_projects.LCEVCdec=${DOXYGEN_OUTPUT_DIR}/xml
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating docs with Sphinx")
