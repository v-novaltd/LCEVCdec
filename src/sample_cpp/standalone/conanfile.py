# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

from conans import ConanFile


class LCEVCDecoderSDKSample(ConanFile):
    name = "LCEVCdecSample"
    description = "LCEVC Decoder C++ Sample"
    settings = "os", "compiler", "arch", "build_type"
    generators = [
        "cmake_find_package_multi"
    ]

    requires = [
        "fmt/8.0.1",
        "cli11/2.3.2",
        "ffmpeg/5.1",
    ]

    def configure(self):
        self.options["fmt"].header_only = True

        self.options["ffmpeg"].with_programs = False

        self.options["ffmpeg"].with_zlib = False
        self.options["ffmpeg"].with_bzip2 = False
        self.options["ffmpeg"].with_lzma = False
        self.options["ffmpeg"].with_libiconv = False
        self.options["ffmpeg"].with_freetype = False
        self.options["ffmpeg"].with_openjpeg = False
        self.options["ffmpeg"].with_openh264 = False
        self.options["ffmpeg"].with_opus = False
        self.options["ffmpeg"].with_vorbis = False
        self.options["ffmpeg"].with_zeromq = False
        self.options["ffmpeg"].with_sdl = False
        self.options["ffmpeg"].with_libx264 = False
        self.options["ffmpeg"].with_libx265 = False
        self.options["ffmpeg"].with_libvpx = False
        self.options["ffmpeg"].with_libmp3lame = False
        self.options["ffmpeg"].with_libfdk_aac = False
        self.options["ffmpeg"].with_libwebp = False
        self.options["ffmpeg"].with_ssl = False

        if self.settings.os == "Linux":
            self.options["ffmpeg"].with_libalsa = False
            self.options["ffmpeg"].with_pulse = False
            self.options["ffmpeg"].with_vaapi = False
            self.options["ffmpeg"].with_vdpau = False
            self.options["ffmpeg"].with_vulkan = False
            self.options["ffmpeg"].with_xcb = False
