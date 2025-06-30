# Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

import os

from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import save
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class LCEVCDecoderSDK(ConanFile):
    name = "lcevc_dec"
    license = "BSD-3-Clause-Clear License"
    description = "LCEVC Decoder SDK"
    url = "https://github.com/v-novaltd/LCEVCdec"

    settings = "os", "compiler", "build_type", "arch"

    options = {
        "shared": [True, False],
        "executables": [True, False],
        "unit_tests": [True, False],
        "vulkan": [True, False],
        "utility": [True, False],
        "benchmark": [True, False],
        "simd": [True, False],
        "force_overlay": [True, False],
        "debug_syntax": [True, False],
        "api_layer": [True, False],
        "json_config": [True, False],
        "base_decoder": ["ffmpeg", "libav", "manual", "none"],
    }

    default_options = {
        "shared": True,
        "executables": True,
        "unit_tests": True,
        "utility": True,
        "benchmark": False,
        "simd": True,
        "force_overlay": False,
        "debug_syntax": False,
        "api_layer": True,
        "json_config": True,
        "fmt:header_only": True,
    }

    generators = "cmake_find_package_multi"
    # CMake generator to use (default is Ninja)
    cmake_generator = {
        'iOS': 'Unix Makefiles',
        'Macos': 'Unix Makefiles',
        'Linux': 'Unix Makefiles',
    }

    # NB: maybe switch to scm attribute when it is stable.
    no_copy_source = True
    exports_sources = ("CMakeLists.txt", "COPYING", "licenses/*", "LICENSE.md", "cmake/*",
                       "include/*", "src/*", "!*.pyc", "!src/utility/unit_tests",
                       "!src/core/emscripten", "!src/func_tests")

    keep_imports = True

    def can_have_executables(self):
        return not ((self.settings.os == 'iOS' or self.settings.os == 'tvOS')
                    or (self.settings.os == 'Windows' and self.settings.compiler.runtime == 'MD'))

    def configure(self):
        # Some targets don't support the sample and test harness exes
        if self.options.executables == None:
            self.options.executables = self.can_have_executables()

        if self.options.base_decoder == None:
            if self.can_have_executables():
                self.options.base_decoder = "ffmpeg"
            else:
                self.options.base_decoder = "none"

        if self.options.vulkan == None:
            if self.settings.os in ["Windows", "Linux"]:
                self.options.vulkan = True
            elif self.settings.os == "Android" and int(self.settings.os.api_level.value) >= 28:
                self.options.vulkan = True
            else:
                self.options.vulkan = False

        # Install test dependencies if installing in dev dir
        if self.options.unit_tests == None:
            self.options.unit_tests = not self.in_local_cache and self.can_have_executables()

        if self.options.base_decoder == "ffmpeg":
            self.options['ffmpeg'].shared = True
            self.options['ffmpeg'].postproc = False
            if self.settings.os == 'Linux':
                self.options['ffmpeg'].with_libalsa = False
                self.options['ffmpeg'].with_pulse = False
                self.options['ffmpeg'].with_vaapi = False
                self.options['ffmpeg'].with_vdpau = False
                self.options['ffmpeg'].with_vulkan = False
                self.options['ffmpeg'].with_xcb = False
            if self.settings.os in ("Macos", "iOS", "tvOS"):
                self.options['ffmpeg'].with_coreimage = False
                self.options['ffmpeg'].with_audiotoolbox = False
                self.options['ffmpeg'].with_videotoolbox = False
            self.options['ffmpeg'].with_asm = False
            self.options['ffmpeg'].with_zlib = False
            self.options['ffmpeg'].with_bzip2 = False
            self.options['ffmpeg'].with_lzma = False
            self.options['ffmpeg'].with_libiconv = False
            self.options['ffmpeg'].with_freetype = False
            self.options['ffmpeg'].with_openjpeg = False
            self.options['ffmpeg'].with_openh264 = False
            self.options['ffmpeg'].with_opus = False
            self.options['ffmpeg'].with_vorbis = False
            self.options['ffmpeg'].with_zeromq = False
            self.options['ffmpeg'].with_sdl = False
            self.options['ffmpeg'].with_libx264 = False
            self.options['ffmpeg'].with_libx265 = False
            self.options['ffmpeg'].with_libvpx = False
            self.options['ffmpeg'].with_libmp3lame = False
            self.options['ffmpeg'].with_libfdk_aac = False
            self.options['ffmpeg'].with_libwebp = False
            self.options['ffmpeg'].with_ssl = False
            self.options['ffmpeg'].with_programs = False

    def requirements(self):
        # Add requirements based on options and settings
        reqs = list()

        if self.options.benchmark:
            reqs.append('benchmark/1.5.5')

        if self.options.json_config:
            reqs.append('nlohmann_json/3.11.3')

        if self.options.base_decoder == "ffmpeg":
            reqs.append('ffmpeg/7.1')

        if self.options.base_decoder == "libav":
            reqs.append('libav/12.3')

        if self.options.base_decoder != "none":
            if self.options.executables:
                reqs.extend(['xxhash/0.8.1', 'cli11/2.3.2', 'fmt/8.0.1'])

            if self.options.unit_tests:
                reqs.extend(['gtest/1.12.1', 'range-v3/0.12.0'])

        if self.options.vulkan and self.settings.os != "Android":
            reqs.extend(['vulkan-loader/1.3.255', 'vulkan-headers/1.3.255'])

        for package in reqs:
            self.requires(package)

    def build_requirements(self):
        if self.options.vulkan:
            self.tool_requires('glslang/11.7.0')

    def set_version(self):
        # Extract version from git
        self._get_git_info()
        self.version = self.git_short_version

    def _get_git_info(self):
        # Add git info to exported sources.
        # Will be picked up by version file generation tools instead of running git.
        if os.path.exists(os.path.join(self.recipe_folder, '.git')):
            git = Git(self)
            # Only consider tags of form [dev]<digits>.<digits>.<digits>...
            self.git_hash = git.run(f"-C {self.recipe_folder} rev-parse --verify --short=8 HEAD")
            self.git_long_hash = git.run(
                f"-C {self.recipe_folder} rev-parse --verify --short=10 HEAD")
            self.git_date = git.run(
                f"-C {self.recipe_folder} log -1 --format=%cd --date=format:%Y-%m-%d")
            self.git_branch = git.run(f"-C {self.recipe_folder} rev-parse --abbrev-ref HEAD")
            self.git_version = git.run(
                f"-C {self.recipe_folder} describe --match \"*.*.*\" --dirty")
            self.git_short_version = git.run(
                f"-C {self.recipe_folder} describe --match \"*.*.*\" --abbrev=0")
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

        save(self, os.path.join(self.export_sources_folder, ".githash"), self.git_hash)
        save(self, os.path.join(self.export_sources_folder, ".gitlonghash"), self.git_long_hash)
        save(self, os.path.join(self.export_sources_folder, ".gitdate"), self.git_date)
        save(self, os.path.join(self.export_sources_folder, ".gitbranch"), self.git_branch)
        save(self, os.path.join(self.export_sources_folder, ".gitversion"), self.git_version)
        save(self, os.path.join(self.export_sources_folder, ".gitshortversion"), self.git_short_version)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        if self.options.base_decoder == "none":
            self.output.warn("Base decoder disabled, not building unit tests or executables")
            self.options.unit_tests = False
            self.options.executables = False

        cmake = CMakeToolchain(self, generator=self.cmake_generator.get(
            str(self.settings.os), "Ninja"))
        cmake.variables["TARGET_ARCH"] = self.settings.arch
        cmake.variables["CMAKE_FIND_ROOT_PATH_MODE_PACKAGE"] = "BOTH"
        cmake.variables["BUILD_SHARED_LIBS"] = self.options.shared
        cmake.variables["VN_SDK_SAMPLE_SOURCE"] = False
        cmake.variables["VN_SDK_UNIT_TESTS"] = self.options.unit_tests
        cmake.variables["VN_SDK_EXECUTABLES"] = self.options.executables
        cmake.variables["VN_SDK_PIPELINE_VULKAN"] = self.options.vulkan
        cmake.variables["VN_SDK_API_LAYER"] = self.options.api_layer
        cmake.variables["VN_CORE_BENCHMARK"] = self.options.benchmark
        cmake.variables["VN_CORE_SIMD"] = self.options.simd
        cmake.variables["VN_CORE_FORCE_OVERLAY"] = self.options.force_overlay

        if self.settings.os == 'Android' and self.settings.arch == 'armv7':
            cmake.variables["CMAKE_ANDROID_ARM_NEON"] = self.options.simd

        if self.settings.compiler == "Visual Studio":
            cmake.variables["VN_MSVC_RUNTIME_STATIC"] = 'MT' in self.settings.compiler.runtime

        cmake.generate()
        return cmake

    def imports(self):
        # Copy shared libraries from dependencies
        self.copy("*.dll", src="@bindirs", dst=os.path.join(self.folders.build, "bin"))
        self.copy("*.so*", src="@libdirs", dst=os.path.join(self.folders.build, "lib"))
        self.copy("*.dylib*", src="@libdirs", dst=os.path.join(self.folders.build, "lib"))

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

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
            self.cpp_info.components["api_utility"].libs = ["lcevc_dec_api_utility"]

        # Extract
        if self.options.api_layer:
            self.cpp_info.components["extract"].libs = ["lcevc_dec_extract"]

        # API
        if self.options.api_layer:
            self.cpp_info.components["api"].libs = ["lcevc_dec_api"]
            if not self.options.shared:
                self.cpp_info.components["api"].requires = ["utility", "api_utility", "extract"]
