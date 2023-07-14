if(VN_SDK_DOCS)  # OFF by default
    find_package(Doxygen REQUIRED)
    find_package(Sphinx REQUIRED)

    # Doxygen only consumes the lcevc_dec.h header right now
    set(DOXYGEN_INPUT ${PROJECT_SOURCE_DIR}/src/api/include/LCEVC/lcevc_dec.h)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
    set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # Configure the Doxyfile to allow cmake to add paths dynamically
    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
    add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
                       COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                       MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
                       DEPENDS ${DOXYGEN_INPUT}
                       COMMENT "Generating docs Doxygen XML")

    add_custom_target(doxygen
                      DEPENDS ${DOXYGEN_INDEX_FILE})

    set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
    set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/docs)

    add_custom_target(documentation ALL
                      DEPENDS doxygen
                      COMMAND ${SPHINX_EXECUTABLE} -b html -W -n -a -Dversion=${GIT_SHORT_VERSION} -Drelease=${GIT_VERSION} -Dbreathe_projects.LCEVC_DEC=${DOXYGEN_OUTPUT_DIR}/xml ${SPHINX_SOURCE} ${SPHINX_BUILD}
                      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                      COMMENT "Generating docs with Sphinx")

    ## Add targets fo building documentation samples
    ##
endif()
