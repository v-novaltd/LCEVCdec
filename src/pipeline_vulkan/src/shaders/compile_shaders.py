# Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

import argparse
import binascii
import os
import re
import subprocess


def get_glslang_validator_path(build_dir):
    conanbuildinfo_path = f"{build_dir}/conanbuildinfo.txt"

    try:
        with open(conanbuildinfo_path, 'r') as file:
            lines = file.readlines()

            for line_no, line in enumerate(lines):
                if line.strip() == '[bindirs_glslang]':
                    next_line = lines[line_no + 1].strip()
                    if not next_line:
                        break

                    exe_path = f"{next_line}/glslangValidator"
                    if os.name == 'nt':
                        exe_path += '.exe'
                    return exe_path

            print("Cound not find glslang package in conanbuildinfo.txt")
            return None

    except FileNotFoundError:
        print(f"Error: {conanbuildinfo_path} not found.")
        return None


def preprocess_shader(shader_path, shader_dir):

    with open(shader_path, 'r') as shader_file:
        shader_code = shader_file.read()

    # Find all #include directives and replace them with the content of the included file
    def include_replacer(match):
        include_file = match.group(1)
        include_path = os.path.join(shader_dir, include_file)
        if not os.path.isfile(include_path):
            raise FileNotFoundError(f"Included file '{include_path}' not found.")

        # Recursively preprocess the included file
        return preprocess_shader(include_path, shader_dir)

    # Regex to match #include directives
    include_pattern = re.compile(r'^\s*#include\s+"(.+)"\s*$', re.MULTILINE)

    # Replace all #include directives
    shader_code = include_pattern.sub(include_replacer, shader_code)

    return shader_code


def generate_header_file(spv_file, header_file):

    # Generate a .h file containing the binary data of the SPIR-V file
    with open(spv_file, 'rb') as f:
        spv_data = f.read()

    # Convert to hex string format suitable for a C header file
    spv_hex = binascii.hexlify(spv_data).decode('utf-8')

    with open(header_file, 'w') as f:
        # Write the header guard and array definition
        spv_name = os.path.splitext(os.path.basename(spv_file))[0]
        header_guard = f"VULKAN_SHADER_{spv_name}_HEADER_H"
        f.write(f"#ifndef {header_guard.upper()}\n")
        f.write(f"#define {header_guard.upper()}\n\n")

        # Write the array of bytes
        f.write(f'static const unsigned char {spv_name}_spv[] = {{\n')

        # Split the hex string into byte-sized chunks and format as a C array
        for i in range(0, len(spv_hex), 2):
            hex_byte = spv_hex[i:i + 2]
            f.write(f'    0x{hex_byte},\n')

        f.write('};\n\n')
        f.write(f"#endif // {header_guard.upper()}\n")


def build_shader(input_shader, glslangvalidator_path, shader_build_dir):

    # Input shader source file is pre-processed
    shader_dir = os.path.dirname(input_shader)
    preprocessed_code = preprocess_shader(input_shader, shader_dir)

    # Pre-processed source written out to file
    input_shader_name, input_shader_ext = os.path.splitext(os.path.basename(input_shader))
    processed_dir = os.path.join(shader_build_dir, shader_dir, 'processed')
    os.makedirs(processed_dir, exist_ok=True)
    input_shader_processed = os.path.join(
        processed_dir, f"{input_shader_name}_processed{input_shader_ext}")
    warning_comment = "// Auto generated file - Do not edit.\n\n"
    with open(input_shader_processed, 'w') as output_file:
        output_file.write(warning_comment + preprocessed_code)

    print(f"Preprocessed shader written to {input_shader_processed}")

    # Compile shader to SPIR-V
    spv_dir = os.path.join(shader_build_dir, shader_dir, 'spv')
    os.makedirs(spv_dir, exist_ok=True)
    output_shader = os.path.join(spv_dir, f"{input_shader_name}.spv")
    subprocess.run([glslangvalidator_path, '-V', input_shader_processed,
                   '-o', output_shader], check=True)
    print(f"SPIR-V shader written to {output_shader}")

    # Generate C++ header file
    hpp_dir = os.path.join(shader_build_dir, shader_dir, 'hpp')
    os.makedirs(hpp_dir, exist_ok=True)
    hpp_shader = os.path.join(hpp_dir, f"{input_shader_name}.h")
    generate_header_file(output_shader, hpp_shader)
    print(f"hpp shader written to {hpp_shader}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compile GLSL shaders")
    parser.add_argument('--cmakebuildinfodir', type=str, required=True,
                        help="The directory containing cmakebuildinfo.txt")
    args = parser.parse_args()
    glslang_validator_path = get_glslang_validator_path(args.cmakebuildinfodir)
    shader_build_dir = os.path.join(args.cmakebuildinfodir, 'src', 'pipeline_vulkan')

    if glslang_validator_path:
        print("Found glslangValidator path from conan:", glslang_validator_path)
    else:
        print("Could not find glslangValidator path from conan, defaulting to PATH")

    if glslang_validator_path is None:
        glslang_validator_path = 'glslangValidator'

    build_shader("src/upscale_vertical.comp", glslang_validator_path, shader_build_dir)
    build_shader("src/upscale_horizontal.comp", glslang_validator_path, shader_build_dir)
    build_shader("src/apply.comp", glslang_validator_path, shader_build_dir)
    build_shader("src/conversion.comp", glslang_validator_path, shader_build_dir)
    build_shader("src/blit.comp", glslang_validator_path, shader_build_dir)
