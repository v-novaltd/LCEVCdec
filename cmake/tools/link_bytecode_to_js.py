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

import argparse
import subprocess
import os
import sys

log_tag_width = 20
log_tag_fmt = "%-" + str(log_tag_width) + "s| %s"


def log_msg(tag, msg):
    if tag is None:
        out = msg
    else:
        out = log_tag_fmt % (tag, msg)
    print(out)


def execute_cmdline(cmd, working_dir):
    p = subprocess.Popen(cmd, shell=True, cwd=working_dir)
    res = p.wait()
    (out, err) = p.communicate()

    if (res != 0):
        log_msg("Error", "Cmd execution failed")
        log_msg(None, "")
        log_msg("Error - Cmd", cmd)
        log_msg("Error - Res", "%d" % (res))
        log_msg("Error - Out", out)
        log_msg("Error - Err", err)

    return (res, "Empty" if (out is None) or (len(out) == 0) else out.rstrip(), "Empty" if (err is None) or (len(err) == 0) else err.rstrip())


def addBuildTypeEnv(build_type):
    build_args = []

    if build_type == "Debug":
        build_args += [
            '-g3',
            '-gsource-map',
            '-fdebug-compilation-dir=.',
            '-s', 'ASSERTIONS=2',
            '-s', 'SAFE_HEAP=1',
            '-s', 'STACK_OVERFLOW_CHECK=1',
        ]
    elif build_type == "Release":
        build_args += [
            '-O3',
            '--closure', '1'
        ]
    elif build_type == "MinSizeRel":
        build_args += [
            '-Oz',
            '--closure', '1'
        ]
    elif build_type == "RelWithDebInfo":
        build_args += [
            '-O2',
            '-g3',
            '-gsource-map',
            '-fdebug-compilation-dir=.',
            '--source-map-base', 'http://0.0.0.0:8000'
        ]
    else:
        print("Unexpected build type.")
        return 1, build_args

    return 0, build_args


def compile(input_file, output_file, functions, workerjs, prejs, licensejs,
            build_type, tracing, debug, libraries):
    sys.path.append(os.environ['EMSCRIPTEN_ROOT_PATH'])

    cur_path = os.getcwd()

    exported_functions = ''.join("EXPORTED_FUNCTIONS=\"['_free','_malloc',%s]\"" % (
        ",".join("'%s'" % t for t in functions)))

    res, build_args = addBuildTypeEnv(build_type)
    if res:
        return res

    if tracing:
        build_args += [
            '--memoryprofiler',
            '--tracing',
            '-s', 'EMSCRIPTEN_TRACING=1'
        ]

    emcc_args = build_args
    emcc_args += [
        '--extern-pre-js', prejs,
        '-s', 'MODULARIZE=1',
        '-s', 'EXPORT_NAME=libDPIModule',
        '-s', 'ENVIRONMENT=web,worker',
        '-s', f'DEMANGLE_SUPPORT={"0" if debug else "1"}',
        '-s', 'ALLOW_MEMORY_GROWTH=1',
        # '-s', 'SHARED_MEMORY=1',
        # '-pthread',
        # '-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency',
        '-msimd128',
        '-msse4.1'
    ]
    if debug:
        emcc_args += [
            '-O0',
        ]

    emcc_args.append(f'-L{os.path.dirname(input_file)}')

    if libraries:
        for x in libraries:
            emcc_args.append(f'-l{x}')

    print('emcc {} -> {}'.format(input_file, output_file))

    command = f"emcc {input_file} -s {exported_functions} {' '.join(emcc_args)} -o {output_file}"
    res = execute_cmdline(command, cur_path)
    return res[0]


def main():
    argParser = argparse.ArgumentParser(
        description="Compile emscripten byte-code into raw javascript")
    argParser.add_argument("-i", "--input", required=True, type=str)
    argParser.add_argument("-o", "--output", required=True, type=str)
    argParser.add_argument("-f", "--functions", required=True, type=str)
    argParser.add_argument("-p", "--prejs", required=True, type=str)
    argParser.add_argument("-w", "--workerjs", required=True, type=str)
    argParser.add_argument("-l", "--licensejs", required=True, type=str)
    argParser.add_argument("-t", "--build_type", required=True, type=str)
    argParser.add_argument("--tracing", required=False,
                           default=False, action="store_true")
    argParser.add_argument("--debug", action='store_true', default=False)
    argParser.add_argument("--link-library", action="append", default=None)

    args = argParser.parse_args()

    input_file = args.input
    output_file = args.output
    functions = args.functions
    prejs = args.prejs
    workerjs = args.workerjs
    licensejs = args.licensejs
    build_type = args.build_type
    tracing = args.tracing
    debug = args.debug
    libraries = args.link_library

    if len(functions) == 0:
        log_msg(
            "Error", "No functions specified, must be a comma-delimited list of functions you want exposing")
        return -1

    if len(workerjs) == 0:
        log_msg("Error", "No worker js files specified, must be a comma-delimited list of javascript for the worker")
        return -1

    functions = functions.split(";")
    workerjs = workerjs.split(";")

    log_msg(None, "")
    log_msg("Input file", input_file)
    log_msg("Output file", output_file)

    for f in functions:
        log_msg("Export function", f)

    for w in workerjs:
        log_msg("Worker js files", w)

    log_msg(None, "")

    if not os.path.exists(input_file):
        log_msg("Error", "Input file does not exist: {0}".format(input_file))
        return -1

    if os.path.exists(output_file):
        log_msg("Cleaning", "Deleting current file: {0}".format(output_file))
        os.remove(output_file)

    return compile(input_file, output_file, functions, workerjs, prejs, licensejs,
                   build_type, tracing, debug, libraries)


if __name__ == "__main__":
    sys.exit(main())
