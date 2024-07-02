# Copyright 2023 - V-Nova Ltd.
#
include("Sources.cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)

if(VN_SDK_EXECUTABLES)
	add_executable(lcevc_api_sample_cpp ${SOURCES})
	lcevc_set_properties(lcevc_api_sample_cpp)

	target_link_libraries(lcevc_api_sample_cpp
		lcevc_dec::api
		lcevc_dec::utility
		fmt::fmt
		CLI11::CLI11
		)

	add_executable(lcevc_dec::api_sample_cpp ALIAS lcevc_api_sample_cpp)

	install(TARGETS "lcevc_api_sample_cpp"  DESTINATION "bin")
endif()

if(VN_SDK_SAMPLE_SOURCE)
	include(GNUInstallDirs)
	install(FILES ${SOURCES} DESTINATION "${CMAKE_INSTALL_DOCDIR}/sample_cpp")
	install(FILES "standalone/README.md"  DESTINATION "${CMAKE_INSTALL_DOCDIR}/sample_cpp")
	install(FILES "standalone/CMakeLists.txt"  DESTINATION "${CMAKE_INSTALL_DOCDIR}/sample_cpp")
	install(FILES "standalone/conanfile.py"  DESTINATION "${CMAKE_INSTALL_DOCDIR}/sample_cpp")
	install(FILES "standalone/requirements.txt"  DESTINATION "${CMAKE_INSTALL_DOCDIR}/sample_cpp")
	install(DIRECTORY "standalone/cmake"  DESTINATION "${CMAKE_INSTALL_DOCDIR}/sample_cpp")
endif()
