from conans import ConanFile
import os
import shutil


class LCEVCDecHeadersConan(ConanFile):
    name = 'lcevc_dec_headers'
    url = 'git@gitlab.com:v-nova-group/development/lcevc_dec.git'
    description = 'V-Nova LCEVC Decoder SDK Headers'
    license = 'Copyright 2023 V-Nova Ltd'
    header_only = True

    def build_requirements(self):
        self.build_requires(f'lcevc_dec/{self.version}@')

    def build(self):
        lcevc_dec_includes_root = 'include'
        lcevc_dec_includes = self.deps_cpp_info['lcevc_dec'].includedirs
        for src in lcevc_dec_includes:
            full_src = os.path.join(
                self.deps_cpp_info['lcevc_dec'].rootpath, src)
            dst = os.path.relpath(src, lcevc_dec_includes_root)
            self.output.info(f'Copying {src} from LCEVC Decoder SDK headers to {dst}')
            shutil.copytree(
                full_src,
                os.path.join(self.build_folder, dst), dirs_exist_ok=True)

    def package(self):
        self.copy('*.h', dst='include', keep_path=True)
