import os

from conans import ConanFile, CMake, tools
from conans.tools import save


class LCEVCDecoderSDK(ConanFile):
    name = "lcevc_dec"
    license = "XXX TBD XXX"
    description = "LCEVC Decoder SDK"
    url = "https://github.com/v-novaltd/LCEVCdec"

    settings = "os", "compiler", "libc", "build_type", "arch"

    options = {
        "shared": [True, False],
        "executables": [True, False],
        "tests": [True, False],
        "unit_tests": [True, False],
        "utility": [True, False],
        "opengl": [True, False],
        "webm": [True, False],
        "benchmark": [True, False],
        "simd": [True, False],
        "decoder_profiler": [True, False],
        "force_overlay": [True, False],
        "debug_syntax": [True, False],
        "integration_layer": [True, False],
        "api_layer": [True, False],
        "use_span_lite": [True, False],
        "use_ghc_filesystem": [True, False],
        "base_decoder": ["ffmpeg", "libav", "none"],
    }

    default_options = {
        "shared": True,
        "utility": True,
        "opengl": True,
        "webm": True,
        "benchmark": False,
        "simd": True,
        "decoder_profiler": False,
        "force_overlay": False,
        "debug_syntax": False,
        "integration_layer": True,
        "api_layer": True,
        "use_span_lite": True,

        "fmt:header_only": True,
    }

    generators = ["cmake_find_package_multi"]

    # Common requirements
    build_requires = [
        'rapidjson/06d58b9',
        'tclap/1.2.1',
        'xxhash/0.8.1',
        'concurrentqueue/1.0.3',
        'fmt/8.0.1',
        'cli11/2.3.2',
    ]

    # CMake generator to use (default is Ninja)
    _generators = {
        "iOS": 'Unix Makefiles',
        "Macos": 'Unix Makefiles',
        'Linux': 'Unix Makefiles',
    }

    # NB: maybe switch to scm attribute when it is stable.
    no_copy_source = True
    exports_sources = "CMakeLists.txt", "COPYING", "licenses/*", "cmake/*", "include/*", "src/*", \
        "!*.pyc", "!*.md", "!src/utility/unit_tests", "!src/core/emscripten", \
        "!src/func_tests"

    def config_options(self):
        # Some targets don't support the sample and test harness exes
        if self.options.executables == None:
            self.options.executables = \
                not ((self.settings.os == 'iOS' or self.settings.os == 'tvOS')
                     or (self.settings.os == 'Windows' and self.settings.compiler.runtime == 'MD')
                     or (self.settings.os == 'Android' and self.settings.compiler.libcxx != 'c++_shared'))

        if self.options.base_decoder == None:
            if ((self.settings.os == 'iOS' or self.settings.os == 'tvOS')
                    or (self.settings.os == 'Windows' and self.settings.compiler.runtime == 'MD')
                    or (self.settings.os == 'Android' and self.settings.compiler.libcxx != 'c++_shared')):
                self.options.base_decoder = "none"
            else:
                self.options.base_decoder = "libav"

        # Install test dependencies if installing in dev dir
        if self.options.tests == None:
            self.options.tests = not self.in_local_cache

        if self.options.unit_tests == None:
            self.options.unit_tests = not self.in_local_cache

        # Use std::filesystem replacement on Ubuntu 18
        if self.options.use_ghc_filesystem == None:
            if self.settings.os == 'Linux' \
                and (self.settings.os.distribution == 'ubuntu'
                     and self.settings.os.distribution.version == '18.04'
                     or self.settings.os.distribution == 'debian'
                     and self.settings.os.distribution.version == '10'):
                self.options.use_ghc_filesystem = True
            else:
                self.options.use_ghc_filesystem = False

    def build_requirements(self):
        """ Add any extra build requirements based on options and settings.
        """
        reqs = []

        if self.options.integration_layer:
            if self.settings.os in ['Linux', 'Windows', 'Android', 'Macos']:
                reqs.append('glad/0.1.33')

            if self._enable_integration_workaround_no_gl_x11_libs:
                # Do not force ubuntu18 ARM dependencies to link X11, since it's broken. We can get away
                # with this since we are linking the static version of glfw and X11/GL don't actually
                # appear in the link dependencies of our produced package. But still needed for build
                reqs.append('glfw/3.3.8')

        if self.options.benchmark:
            reqs.append('benchmark/1.5.5')

        if self.options.unit_tests:
            reqs.extend(['gtest/1.12.1', 'range-v3/0.12.0'])

        # This should be private, but that does not seem to work as documented
        if self.options.base_decoder == "libav":
            reqs.append('libav/12.3')
        elif self.options.base_decoder == "ffmpeg":
            reqs.append('ffmpeg/n6.0-vnova-26')

        if self.options.use_ghc_filesystem:
            reqs.append('ghc-filesystem/1.5.14@')

        if self.options.use_span_lite:
            reqs.append('span-lite/0.10.3')

        for p in reqs:
            self.build_requires(p)

    def requirements(self):
        """ Add any extra requirements based on options and settings.
        """
        reqs = []

        if self.options.integration_layer:
            if self.settings.os == 'Linux':
                if not self._enable_integration_workaround_no_gl_x11_libs:
                    reqs.append('glfw/3.3.8')
                else:
                    reqs.append('xorg/system')

            if self.settings.os in ['Windows', 'Macos']:
                reqs.append('glfw/3.3.8')

            if (self.settings.os in ['Linux', 'Windows', 'Android', 'Macos']) and not self.options.shared:
                reqs.append('glad/0.1.33')

        for p in reqs:
            self.requires(p)

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
            self.git_long_hash = git.run(f"-C {self.recipe_folder} rev-parse --verify --short=10 HEAD")
            self.git_date = git.run(f"-C {self.recipe_folder} log -1 --format=%cd --date=format:%Y-%m-%d")
            self.git_branch = git.run(f"-C {self.recipe_folder} rev-parse --abbrev-ref HEAD")
            self.git_version = git.run(f"-C {self.recipe_folder} describe --match \"[0-9].[0-9].[0-9]\" --abbrev=1 --dirty")
            self.git_short_version = git.run(f"-C {self.recipe_folder} describe --match \"[0-9].[0-9].[0-9]\" --abbrev=0")
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

        dil_properties_string = '''ext {{
    dilGitBranch = {0}
    dilGitDate = {1}
    dilGitHash = {2}
}}'''.format(self.git_branch, self.git_date, self.git_long_hash)

        save(os.path.join(self.export_sources_folder, "dil.properties"), dil_properties_string)

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
        cmake.definitions["VN_SDK_TESTS"] = self._on_off(self.options.tests)
        cmake.definitions["VN_SDK_UTILITY"] = self._on_off(self.options.utility)
        cmake.definitions["VN_SDK_INTEGRATION_LAYER"] = self._on_off(self.options.integration_layer)
        cmake.definitions["VN_SDK_API_LAYER"] = self._on_off(self.options.api_layer)
        cmake.definitions["VN_SDK_USE_GHC_FILESYSTEM"] = self._on_off(self.options.use_ghc_filesystem)
        cmake.definitions["VN_SDK_USE_SPAN_LITE"] = self._on_off(self.options.use_span_lite)
        cmake.definitions["VN_CORE_BENCHMARK"] = self._on_off(self.options.benchmark)
        cmake.definitions["VN_CORE_SIMD"] = self._on_off(self.options.simd)
        cmake.definitions["VN_CORE_DECODER_PROFILER"] = self._on_off(self.options.decoder_profiler)
        cmake.definitions["VN_CORE_DEBUG_SYNTAX"] = self._on_off(self.options.debug_syntax)
        cmake.definitions["VN_CORE_FORCE_OVERLAY"] = self._on_off(self.options.force_overlay)
        cmake.definitions["VN_INTEGRATION_OPENGL"] = self._on_off(self.options.opengl)
        cmake.definitions["VN_INTEGRATION_WEBM"] = self._on_off(self.options.webm)

        cmake.definitions["VN_SDK_FFMPEG_LIBS_PACKAGE"] = self.options.base_decoder if self.options.base_decoder else ""

        if self.settings.os == 'Android' and self.settings.arch == 'armv7':
            cmake.definitions["CMAKE_ANDROID_ARM_NEON"] = self._on_off(self.options.simd)

        if self.settings.compiler == "Visual Studio":
            cmake.definitions["VN_MSVC_RUNTIME_STATIC"] = self._on_off('MT' in self.settings.compiler.runtime)

        # Workaround for cross compiling to Ubuntu 18 aarch64 - multiarch x11 dev libs are broken
        if self._enable_integration_workaround_no_gl_x11_libs:
            cmake.definitions["VN_INTEGRATION_WORKAROUND_NO_GL_X11_LIBS"] = 'ON'

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

        # Core (DPI)s
        self.cpp_info.components["core"].libs = ["lcevc_dec_core"]
        if not self.options.shared:
            self.cpp_info.components["core"].libs.append("lcevc_dec_overlay_images")
        if self.settings.os == "Linux":
            self.cpp_info.components["core"].system_libs.append("pthread")
            self.cpp_info.components["core"].system_libs.append("dl")

        # Integration (DIL)
        if self.options.integration_layer:
            self.cpp_info.components["integration_utility"].libs = ["lcevc_dec_integration_utility"]
            self.cpp_info.components["integration_utility"].includedirs = ["include/LCEVC/integration/utility"]

            self.cpp_info.components["integration"].libs = ["lcevc_dec_integration"]
            if not self.options.shared:
                self.cpp_info.components["integration"].requires = ["integration_utility"]
            self.cpp_info.components["integration"].requires.append("core")
            if self.settings.os in ['Linux', 'Windows', 'Android', 'Macos'] and not self.options.shared:
                self.cpp_info.components["integration"].requires.append("glad::glad")
            if self.settings.os in ['Windows', 'Macos']:
                self.cpp_info.components["integration"].requires.append("glfw::glfw")

            # Android
            if self.settings.os == "Android":
                # Distinguishes VNDK vs. NDK
                if self.settings.compiler.libcxx == "c++_shared":
                    self.cpp_info.components["integration"].system_libs.append("android")
                else:
                    self.cpp_info.components["integration"].system_libs.append("nativewindow")
                self.cpp_info.components["integration"].system_libs.append("log")
                self.cpp_info.components["integration"].system_libs.append("EGL")
                self.cpp_info.components["integration"].system_libs.append("z")

        # API
        if self.options.api_layer:
            self.cpp_info.components["api"].libs = ["lcevc_dec_api"]
            if not self.options.shared:
                self.cpp_info.components["api"].requires = ["integration_utility"]
