# Copyright 2022 - V-Nova Ltd.
#
# Use NDK provided tioolchain with LCEVC default settings.
#

set(ANDROID_PLATFORM 21 CACHE STRING "Android API Level")
set(ANDROID_ABI armeabi-v7a CACHE STRING "Android ABI")
set(ANDROID_ARM_NEON TRUE)

# Look for packages in the build directory as well
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE "BOTH")

include($ENV{ANDROID_NDK}/build/cmake/android.toolchain.cmake)
