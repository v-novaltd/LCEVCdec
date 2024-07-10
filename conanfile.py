# Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

import os

from conans import ConanFile, CMake, tools
from conans.tools import save


class LCEVCDecoderSDK(ConanFile):
    name = "lcevc_dec"
    license = "BSD-3-Clause-Clear License"
    description = "LCEVC Decoder SDK"
    url = "https://github.com/v-novaltd/LCEVCdec"

    settings = "os", "compiler", "libc", "build_type", "arch"

    options = {
        "shared": [True, False],
        "executables": [True, False],
        "unit_tests": [True, False],
        "utility": [True, False],
        "benchmark": [True, False],
        "simd": [True, False],
        "decoder_profiler": [True, False],
        "force_overlay": [True, False],
        "debug_syntax": [True, False],
        "api_layer": [True, False],
        "json_config": [True, False],
        "use_ghc_filesystem": [True, False],
        "base_decoder": ["ffmpeg-libs-minimal", "ffmpeg", "libav", "none"],
    }

    default_options = {
        "shared": True,
        "executables": True,
        "unit_tests": True,
        "utility": True,
        "benchmark": False,
        "simd": True,
        "decoder_profiler": False,
        "force_overlay": False,
        "debug_syntax": False,
        "api_layer": True,
        "json_config": True,

        "fmt:header_only": True,
    }

    generators = ["cmake_find_package_multi"]

    # CMake generator to use (default is Ninja)
    _generators = {
        "iOS": 'Unix Makefiles',
        "Macos": 'Unix Makefiles',
        'Linux': 'Unix Makefiles',
    }

    # NB: maybe switch to scm attribute when it is stable.
    no_copy_source = True
    exports_sources = ("CMakeLists.txt", "COPYING", "licenses/*", "LICENSE.md", "cmake/*",
                       "include/*", "src/*", "!*.pyc", "!src/utility/unit_tests",
                       "!src/core/emscripten", "!src/func_tests")

    def is_android_vndk(self):
        return (self.settings.os == "Android" and self.settings.compiler.libcxx != "c++_shared")

    def config_options(self):
        # Some targets don't support the sample and test harness exes
        if self.options.executables == None:
            self.options.executables = \
                not ((self.settings.os == 'iOS' or self.settings.os == 'tvOS')
                     or (self.settings.os == 'Windows' and self.settings.compiler.runtime == 'MD')
                     or (self.is_android_vndk()))

        if self.options.base_decoder == None:
            if ((self.settings.os == 'iOS' or self.settings.os == 'tvOS')
                    or (self.settings.os == 'Windows' and self.settings.compiler.runtime == 'MD')
                    or (self.is_android_vndk())):
                self.options.base_decoder = "none"
            else:
                self.options.base_decoder = "ffmpeg-libs-minimal"

        # Install test dependencies if installing in dev dir
        if self.options.unit_tests == None:
            self.options.unit_tests = not self.in_local_cache

        # Use std::filesystem replacement on Ubuntu 18, armv7, and iOS
        if self.options.use_ghc_filesystem == None:
            if (self.is_android_vndk()) or \
               (self.settings.os == 'Linux'
                    and (self.settings.os.distribution == 'ubuntu'
                         and self.settings.os.distribution.version == '18.04'
                         or self.settings.os.distribution == 'debian'
                         and self.settings.os.distribution.version == '10')):
                self.options.use_ghc_filesystem = True
            else:
                self.options.use_ghc_filesystem = False

        # Now set the default options for ffmpeg for android
        if self.options.base_decoder == "ffmpeg-libs-minimal" and self.settings.os == "Android":
            # TODO: only set the default when it's not provide on command line like this
            # -o ffmpeg:shared=True -o dav1d:shared=True -o libvvdec:shared=True
            self.options['ffmpeg'].add_option('shared', False)
            self.options['dav1d'].add_option('shared', False)
            self.options['libvvdec'].add_option('shared', False)
            self.output.info(f'Building for android: shared library ffmpeg {self.options["ffmpeg"].shared}, '
                             f'dav1d {self.options["dav1d"].shared}, libvvdec {self.options["libvvdec"].shared}')

    def build_requirements(self):
        """ Add any extra build requirements based on options and settings.
        """
        reqs = list()

        if self.options.benchmark:
            reqs.append('benchmark/1.5.5')

        if self.options.executables:
            reqs.extend(['xxhash/0.8.1', 'cli11/2.3.2'])

        if self.options.utility or self.options.executables:
            reqs.append('fmt/8.0.1')

        if self.options.unit_tests:
            reqs.extend(['gtest/1.12.1', 'range-v3/0.12.0'])

        if self.options.json_config:
            reqs.append('rapidjson/06d58b9')

        # This should be private, but that does not seem to work as documented
        if self.options.base_decoder == "ffmpeg-libs-minimal":
            reqs.append('ffmpeg-libs-minimal/n6.1-vnova-8')

        if self.options.base_decoder == "ffmpeg":
            reqs.append('ffmpeg/n6.1-vnova-7')

        if self.options.base_decoder == "libav":
            reqs.append('libav/12.3')

        if self.options.use_ghc_filesystem:
            reqs.append('ghc-filesystem/1.5.14@')

        for package in reqs:
            self.build_requires(package)

    def set_version(self):
        """ Extract version from git
        """
        self._get_git_info()
        self.version = self.git_short_version

    def _get_git_info(self):
        """Add git info to exported sources.
        Will be picked up by version file generation tools instead of running git.
        """
        if os.path.exists(os.path.join(self.recipe_folder, '.git')):
            git = tools.Git()
            # Only consider tags of form <digits>.<digits>.<digits>...
            self.git_hash = git.run(f"-C {self.recipe_folder} rev-parse --verify --short=8 HEAD")
            self.git_long_hash = git.run(
                f"-C {self.recipe_folder} rev-parse --verify --short=10 HEAD")
            self.git_date = git.run(
                f"-C {self.recipe_folder} log -1 --format=%cd --date=format:%Y-%m-%d")
            self.git_branch = git.run(f"-C {self.recipe_folder} rev-parse --abbrev-ref HEAD")
            self.git_version = git.run(
                f"-C {self.recipe_folder} describe --match \"[0-9].[0-9].[0-9]\" --abbrev=1 --dirty")
            self.git_short_version = git.run(
                f"-C {self.recipe_folder} describe --match \"[0-9].[0-9].[0-9]\" --abbrev=0")
        else:
            with open(os.path.join(self.recipe_folder, '.githash')) as f:
                self.git_hash = f.read()
            with open(os.path.join(self.recipe_folder, '.gitlonghash')) as f:
                self.git_long_hash = f.read()
            with open(os.path.join(self.recipe_folder, '.gitdate')) as f:
                self.git_date = f.read()
            with open(os.path.join(self.recipe_folder, '.gitbranch')) as f:
                self.git_branch = f.read()
            with open(os.path.join(self.recipe_folder, '.gitversion')) as f:
                self.git_version = f.read()
            with open(os.path.join(self.recipe_folder, '.gitshortversion')) as f:
                self.git_short_version = f.read()

    def export_sources(self):
        self._get_git_info()

        save(os.path.join(self.export_sources_folder, ".githash"), self.git_hash)
        save(os.path.join(self.export_sources_folder, ".gitlonghash"), self.git_long_hash)
        save(os.path.join(self.export_sources_folder, ".gitdate"), self.git_date)
        save(os.path.join(self.export_sources_folder, ".gitbranch"), self.git_branch)
        save(os.path.join(self.export_sources_folder, ".gitversion"), self.git_version)
        save(os.path.join(self.export_sources_folder, ".gitshortversion"), self.git_short_version)

    @staticmethod
    def _on_off(b):
        """Convert truth value to ON/OFF for CMake definitions."""
        if b:
            return "ON"
        else:
            return "OFF"

    @property
    def _enable_integration_workaround_no_gl_x11_libs(self):
        # Workaround for cross compiling to Ubuntu 18 aarch64 - multiarch x11 dev libs are broken
        return self.settings.arch == 'armv8' and self.settings.os == 'Linux' and \
            self.settings.os.distribution == 'ubuntu' and self.settings.os.distribution.version == '18.04'

    def _configure_cmake(self):
        cmake = CMake(self, generator=self._generators.get(str(self.settings.os), "Ninja"))
        cmake.definitions["TARGET_ARCH"] = self.settings.arch
        cmake.definitions["CMAKE_FIND_ROOT_PATH_MODE_PACKAGE"] = "BOTH"
        cmake.definitions["BUILD_SHARED_LIBS"] = self._on_off(self.options.shared)
        cmake.definitions["VN_SDK_SAMPLE_SOURCE"] = "OFF"
        cmake.definitions["VN_SDK_UNIT_TESTS"] = self._on_off(self.options.unit_tests)
        cmake.definitions["VN_SDK_EXECUTABLES"] = self._on_off(self.options.executables)
        cmake.definitions["VN_SDK_UTILITY"] = self._on_off(self.options.utility)
        cmake.definitions["VN_SDK_API_LAYER"] = self._on_off(self.options.api_layer)
        cmake.definitions["VN_SDK_USE_GHC_FILESYSTEM"] = self._on_off(
            self.options.use_ghc_filesystem)
        cmake.definitions["VN_SDK_FFMPEG_LIBS_PACKAGE"] = self.options.base_decoder if self.options.base_decoder else ""
        cmake.definitions["VN_CORE_BENCHMARK"] = self._on_off(self.options.benchmark)
        cmake.definitions["VN_CORE_SIMD"] = self._on_off(self.options.simd)
        cmake.definitions["VN_CORE_DECODER_PROFILER"] = self._on_off(self.options.decoder_profiler)
        cmake.definitions["VN_CORE_DEBUG_SYNTAX"] = self._on_off(self.options.debug_syntax)
        cmake.definitions["VN_CORE_FORCE_OVERLAY"] = self._on_off(self.options.force_overlay)

        if self.settings.os == 'Android' and self.settings.arch == 'armv7':
            cmake.definitions["CMAKE_ANDROID_ARM_NEON"] = self._on_off(self.options.simd)

        if self.settings.compiler == "Visual Studio":
            cmake.definitions["VN_MSVC_RUNTIME_STATIC"] = self._on_off(
                'MT' in self.settings.compiler.runtime)

        cmake.configure()
        return cmake

    def imports(self):
        # Copy shared libraries from dependencies
        self.copy("*.dll", src="@bindirs", dst="bin")
        self.copy("*.so*", src="@libdirs", dst="lib")
        self.copy("*.dylib*", src="@libdirs", dst="lib")

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()
        self.copy(pattern="licenses", dst="licenses")

    def package_info(self):

        # Core
        self.cpp_info.components["core"].libs = ["lcevc_dec_core"]
        if not self.options.shared:
            self.cpp_info.components["core"].libs.append("lcevc_dec_overlay_images")
        if self.settings.os == "Linux":
            self.cpp_info.components["core"].system_libs.append("pthread")
            self.cpp_info.components["core"].system_libs.append("dl")

        # Utilities
        if self.options.api_layer:
            self.cpp_info.components["utility"].libs = ["lcevc_dec_utility"]

        # API
        if self.options.api_layer:
            self.cpp_info.components["api"].libs = ["lcevc_dec_api"]
            if not self.options.shared:
                self.cpp_info.components["api"].requires = ["utility"]
