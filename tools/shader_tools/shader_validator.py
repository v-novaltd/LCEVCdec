import os
import shutil
import glob
import subprocess
import yaml
import itertools
import argparse

from enum import IntEnum
from braceexpand import braceexpand

import metadata_validator
from validation_shared import find_shader_path, make_shader_basename, combine_dicts


# Constants #######################################################################################

class CleanupOperation(IntEnum):
    # see https://docs.python.org/3/library/enum.html?highlight=enum#comparisons for why this is an IntEnum
    NONE = 0
    REMOVE_SUCCESSES = 1
    REMOVE_ALL = 2


VALIDATOR = "glslangValidator"


# Globals #########################################################################################

_directories = []
_header_files = []
_extension_files = []

_vertex_shaders = []
_fragment_shaders = []

_shader_filepaths = {}
_all_shader_filepaths = []

# Script parameters. TODO: delete shaders that are ignored by default, or find use for them.
_params = {
    "metadata": "shader_metadata.yaml",
    "variant_folder": "variants",
    "results": "results.yaml",
    "shader_filter": "*.glsl",
    "cleanup_mode": CleanupOperation.REMOVE_SUCCESSES,
    "ignore_list": ["frag_blit_zoom.glsl", "vert_zoom.glsl"]
}

# Preloaded source code
_header_file_sources = []
_extensions_file_sources = []

# Results
_failed_variants = []


# Functions #######################################################################################

def parse_args():
    global _params
    parser = argparse.ArgumentParser(
        description="Script to validate all variants of shaders from specified directories.")

    # DO NOT use the "default=" parameter for "add_argument". The defaults are already set.
    parser.add_argument("-m", "--metadata", help="Path to shader metadata YAML file.", required=False)
    parser.add_argument("-f", "--filter", help="Shader name filter to only generate variants that satisfy "
                        "this glob filter. Supports brace expansion.", required=False)
    parser.add_argument("-i", "--ignore", help="Shader file paths to ignore completely. Can have multiple values.",
                        nargs="+", required=False)
    parser.add_argument("-v", "--variantdir", help="Path to directory to write shader variant files.", required=False)
    parser.add_argument("-r", "--results", help="File path to output results YAML.", required=False)
    parser.add_argument("-c", "--cleanup", help="Shader file removal operation after variant generation has completed. "
                        "Options: 0 - No removal, 1 - Remove only successfully compiled variants, 2 - Remove all",
                        type=int, required=False)

    args = parser.parse_args()
    if args.metadata is not None:
        _params["metadata"] = args.metadata
    if args.variantdir is not None:
        _params["variant_folder"] = args.variantdir
    if args.results is not None:
        _params["results"] = args.results
    if args.filter is not None:
        _params["shader_filter"] = args.filter
    if args.cleanup is not None:
        _params["cleanup_mode"] = args.cleanup
    if args.ignore is not None:
        _params["ignore_list"] = args.ignore


def read_metadata():
    global _directories
    global _header_files
    global _extension_files
    global _vertex_shaders
    global _fragment_shaders

    shader_metadata_file = _params["metadata"]

    if not os.path.exists(shader_metadata_file):
        print(f"Path does not exist to metadata file ({shader_metadata_file})")
        return False

    with open(shader_metadata_file, "r") as f:
        metadata = yaml.safe_load(f)

    _directories = metadata["shader_directories"]
    _header_files = metadata["header_files"]
    _extension_files = metadata["extension_files"]
    _vertex_shaders = metadata["vertex_shaders"]
    _fragment_shaders = metadata["fragment_shaders"]
    return True


def make_shader_filenames():
    global _shader_filepaths

    extension = "glsl"
    for shaders in [_vertex_shaders, _fragment_shaders]:
        for shader in shaders:
            basename = make_shader_basename(
                shader["name"], extension)
            path = find_shader_path(
                basename, _all_shader_filepaths)

            if not path:
                continue

            _shader_filepaths[basename] = path


def search_dirs(file_basename):
    for d in _directories:
        path = os.path.join(d, str(file_basename))
        if os.path.exists(path):
            return True, path

    return False, ""


def verify_metadata():
    global _header_files
    global _extension_files

    # Find paths for header and extension files in all provided directories
    files_not_found = []
    for header in _header_files:
        found, path = search_dirs(header["path"])
        if found:
            header["path"] = path
        else:
            files_not_found.append(header["path"])

    for i in range(0, len(_extension_files)):
        found, path = search_dirs(_extension_files[i])
        if found:
            _extension_files[i] = path
        else:
            files_not_found.append(_extension_files[i])

    if files_not_found:
        print("Failed to find files: {}".format(", ".join(files_not_found)))
        return False

    # Check that all of the shader filenames (from shader_filepaths) are present
    # in this list. If they're on the ignore list, it's fine, otherwise fail.
    ignore_list_fullpaths = []
    for ignored in _params["ignore_list"]:
        found, path = search_dirs(ignored)
        if found:
            ignore_list_fullpaths.append(path)

    ignore_list = ignore_list_fullpaths + \
        [x["path"] for x in _header_files] + _extension_files
    shaders_without_metadata = [s for s in _all_shader_filepaths if os.path.basename(
        s) not in _shader_filepaths and s not in ignore_list]

    if shaders_without_metadata:
        failed_shaders_string = "\n".join(shaders_without_metadata)
        print(
            f"Failed verification. The following non-ignored shaders have no metadata: {failed_shaders_string}")
        return False

    return True


def init():
    global _all_shader_filepaths

    print("Starting Shader Validator...")

    print("Performing pre-cleaning...")
    pre_cleanup()

    # Read in the YAML file
    metadata_file = _params["metadata"]
    if not read_metadata():
        print(f"Could not read metadata file: {metadata_file}")
        return False

    # First get all shader file paths (using the filter). Brace expansion means that "*" is still
    # "all files", but "{*.frag,*.vert}" is "all files which end in .frag OR .vert". Blank items
    # such as the second element of "{frag_blit.glsl,}", are simply ignored.
    shader_name_filter = _params["shader_filter"]
    shader_filename_list = list(braceexpand(f"{shader_name_filter}"))
    for d in _directories:
        for shader_filename in shader_filename_list:
            if shader_filename:
                _all_shader_filepaths.extend(
                    glob.glob(os.path.join(d, shader_filename)))

    # Construct the shader paths
    make_shader_filenames()

    # Verify that all shader files that aren't ignored or filtered are present in the metadata
    if not verify_metadata():
        print(f"Could not verify metadata file: {metadata_file}")
        return False

    # Read in text from header files
    for h in _header_files:
        with open(h["path"], "r") as f:
            _header_file_sources.append(f.read())

    # Read in text from extensions files
    for e in _extension_files:
        with open(e, "r") as f:
            _extensions_file_sources.append(f.read())

    # Create a directory for the variants if it doesn't already exist
    if not os.path.isdir(_params["variant_folder"]):
        os.mkdir(_params["variant_folder"])

    return True


def pre_cleanup():
    variant_folder = _params["variant_folder"]
    if os.path.isdir(variant_folder):
        try:
            shutil.rmtree(variant_folder)
        except OSError as e:
            print(f"Error: could not remove tree {variant_folder} - {e}")
    else:
        print("No directory to pre-clean, moving on.")


def post_cleanup():
    if _params["cleanup_mode"] == CleanupOperation.NONE:
        # Do nothing
        print("Skipping cleanup.")
        return

    variant_folder = _params["variant_folder"]
    if os.path.isdir(variant_folder):
        files = []
        for type in ["vert", "frag"]:
            files.extend(glob.glob(os.path.join(variant_folder, f"*.{type}")))

        should_remove_dir = False

        if _params["cleanup_mode"] == CleanupOperation.REMOVE_ALL:
            print("Removing all variants")
            for f in files:
                try:
                    os.remove(f)
                except OSError as e:
                    print(f"Error: Could not remove {f} - {e.strerror}")

            should_remove_dir = True
        elif _params["cleanup_mode"] == CleanupOperation.REMOVE_SUCCESSES:
            print("Removing variants which passed validation")
            failed_variant_files = [x["file"] for x in _failed_variants]
            for f in files:
                if f not in failed_variant_files:
                    try:
                        os.remove(f)
                    except OSError as e:
                        print(f"Error: Could not remove {f} - {e.strerror}")

            should_remove_dir = len(_failed_variants) == 0
        else:
            print(
                f"Invalid cleanup mode. Options are {CleanupOperation.NONE}, {CleanupOperation.REMOVE_SUCCESSES}, {CleanupOperation.REMOVE_ALL}, but mode provided was {_params['cleanup_mode']}. Remember: int comparisons with enums always return false, unless they are IntEnums. The cmd line parameter must also be an int.")

        if should_remove_dir:
            os.rmdir(variant_folder)
    else:
        print(
            f"Variant folder appears to be invalid, or never got created. Folder name: {variant_folder}.")


def concatenate_shader(header_file_index, shader_defines, shader_source):
    header_source = _header_file_sources[header_file_index]
    extensions_source = "\n".join(_extensions_file_sources)

    define_pairs = [f"{name} {value}" for (
        name, value) in shader_defines.items()]
    defines_source = "".join([f"#define {pair}\n" for pair in define_pairs])

    header_file_path = _header_files[header_file_index]["path"]
    variant_defines_info = "".join(
        [f"//\t- {pair}\n" for pair in define_pairs]) if define_pairs else "//\t- (None)"
    variant_info = f"//--- Variant Info ---\n// - Header File: {header_file_path}\n// - Defines:\n{variant_defines_info}\n\n"

    return "\n".join([header_source, variant_info, extensions_source, defines_source, shader_source])


def generate_shader_variants():
    shader_extension = "glsl"
    variant_extensions = ["vert", "frag"]
    full_shader_list = [_vertex_shaders, _fragment_shaders]

    for i in range(0, len(full_shader_list)):
        shaders = full_shader_list[i]
        variant_extension = variant_extensions[i]
        for shader in shaders:
            # Create all combinations of shader definitions for this shader file
            define_variants_ES2_allowed, define_variants_ES2_not_allowed = generate_define_variants(
                shader)
            # if define_variants are empty, add a placeholder meaning "there's 1 variant, with no defines"
            if not define_variants_ES2_allowed and not define_variants_ES2_not_allowed:
                define_variants_ES2_allowed.append({})
                define_variants_ES2_not_allowed.append({})

            define_variants_ES2_allowed = remove_avoided_define_variants(
                define_variants_ES2_allowed, shader, True)
            define_variants_ES2_not_allowed = remove_avoided_define_variants(
                define_variants_ES2_not_allowed, shader, False)

            # Create a file for each variant type
            variant_count = 0
            basename = make_shader_basename(
                shader["name"], shader_extension)

            # this is fine: it happens when we use a filter.
            if basename not in _shader_filepaths:
                continue

            with open(_shader_filepaths[basename], "r") as f:
                shader_source = f.read()

            for h in range(0, len(_header_files)):
                if _header_files[h]["es2"]:
                    for define_variant in define_variants_ES2_allowed:
                        concatenate_and_write_shader(
                            h, define_variant, shader_source, shader, variant_count, variant_extension)
                        variant_count += 1
                else:
                    for define_variant in define_variants_ES2_not_allowed:
                        concatenate_and_write_shader(
                            h, define_variant, shader_source, shader, variant_count, variant_extension)
                        variant_count += 1


def variants_from_defines(defines):
    # This generates a list of every possible configuration, where a configuration is expressed as
    # a dictionary, mapping from each name to its precise value in that configuration.
    define_keys, define_values = zip(*defines.items())
    list_of_dicts = []
    for val_list in itertools.product(*define_values):
        list_of_tuples = []
        for i in range(0, len(val_list)):
            if val_list[i] is not None:
                list_of_tuples.append((define_keys[i], val_list[i]))
        list_of_dicts.append(dict(list_of_tuples))
    return list_of_dicts


def variants_from_debug_defines(debug_defines):
    # We assume that debug defines are typically independent from each other, so we don't need to
    # test ALL their combinations. It's enough to test that they work in "all on" or "all off"
    # configurations. More formally, this function produces a list of configurations, such that in
    # the Nth configuration, every define takes its Nth possible value. If a define lacks an Nth
    # possible value, it will be ignored (i.e. not defined to anything)
    define_keys, define_values = zip(*debug_defines.items())
    list_of_dicts = []
    for i in range(0, len(define_values[0])):
        list_of_dicts.append({key: value_list[i] for key in define_keys for value_list in define_values if i < len(
            value_list) and value_list[i] is not None})
    return list_of_dicts


def generate_define_variants(shader):
    define_variants_ES2, define_variants_NonES2 = [], []

    if not shader["defines"]:
        return define_variants_ES2, define_variants_NonES2

    defines_allowed_in_ES2 = {}
    defines_not_allowed_in_ES2 = {}
    for define_and_values in shader["defines"]:
        values_allowed_in_ES2 = []
        values_not_allowed_in_ES2 = []
        if "values" in define_and_values:
            values_allowed_in_ES2 += define_and_values["values"]
            values_not_allowed_in_ES2 += define_and_values["values"]
        if "values_ES2" in define_and_values:
            values_allowed_in_ES2 += define_and_values["values_ES2"]
        if "values_NonES2" in define_and_values:
            values_not_allowed_in_ES2 += define_and_values["values_NonES2"]
        defines_allowed_in_ES2[define_and_values["name"]
                               ] = values_allowed_in_ES2
        defines_not_allowed_in_ES2[define_and_values["name"]
                                   ] = values_not_allowed_in_ES2

    if defines_allowed_in_ES2:
        define_variants_ES2 = variants_from_defines(defines_allowed_in_ES2)
    if defines_not_allowed_in_ES2:
        define_variants_NonES2 = variants_from_defines(
            defines_not_allowed_in_ES2)

    # Get the debug defines, and product them with the other defines, then merge all dictionaries.
    # For example, defines might be: [{A:0,B:0}, {A:0,B:1}, {A:1,B:0}, {A:1,B;1}]
    # debug defines might be: [{X:0,Y:0}, {X:1,Y:1}].
    # The product is then: [({A:0,B:0}, {X:0,Y:0}), ({A:0,B:1}, {X:0,Y:0}), ({A:1,B:0}, {X:0,Y:0}), ({A:1,B:1}, {X:0,Y:0}),
    #                       ({A:0,B:0}, {X:1,Y:1}), ({A:0,B:1}, {X:1,Y:1}), ({A:1,B:0}, {X:1,Y:1}), ({A:1,B:1}, {X:1,Y:1})]
    # And then, each pair gets merged to 1 dictionary
    debug_defines = {}
    debug_define_variants = [{}]
    if "debug_defines" in shader:
        debug_defines = {d["name"]: d["values"]
                         for d in shader["debug_defines"] if "values" in d}
    if debug_defines:
        debug_define_variants = variants_from_debug_defines(debug_defines)

    full_def_vars_ES2 = itertools.product(
        define_variants_ES2, debug_define_variants)
    full_def_vars_NonES2 = itertools.product(
        define_variants_NonES2, debug_define_variants)
    define_variants_ES2 = [combine_dicts(pair[0], pair[1]) for pair in full_def_vars_ES2]
    define_variants_NonES2 = [combine_dicts(pair[0], pair[1]) for pair in full_def_vars_NonES2]

    return define_variants_ES2, define_variants_NonES2


def remove_avoided_define_variants(define_variants, shader, is_es2):
    if "avoid" not in shader:
        return define_variants

    bad_variant_idxs = set()
    for i in range(0, len(define_variants)):
        for avoided_variant in shader["avoid"]:
            is_full_match = True
            for avoided_define_key in avoided_variant:
                if avoided_define_key == "es2":
                    if is_es2 != avoided_variant[avoided_define_key]:
                        is_full_match = False
                elif define_variants[i][avoided_define_key] != avoided_variant[avoided_define_key]:
                    is_full_match = False
            if is_full_match:
                bad_variant_idxs.add(i)
                break

    return [define_variants[i] for i in range(0, len(define_variants)) if i not in bad_variant_idxs]


def concatenate_and_write_shader(header_idx, define_variant, shader_source, shader, variant_count, variant_extension):
    # Concatenate the headers, extensions, defines and shader source into one
    full_shader_source = concatenate_shader(
        header_idx, define_variant, shader_source)
    variant_name = shader["name"]
    variant_filename = f"{variant_name}_{variant_count}.{variant_extension}"
    with open(os.path.join(_params["variant_folder"], variant_filename), "w") as f:
        f.write(full_shader_source)


def get_errors(error_lines):
    errors = []
    error_beginning = "ERROR: 0:"

    # Ignore the first line as it's always the file name
    # Ignore the last error as it's always a summary
    # Ignore any that consist of "compilation terminated"

    ignored_errors = [
        "compilation terminated"
    ]

    for i in range(1, len(error_lines) - 1):
        line = error_lines[i]
        if line.startswith(error_beginning):
            substr = line[len(error_beginning):]
            next_colon_index = substr.find(":")
            line_number = substr[0:next_colon_index]
            substr = substr[next_colon_index + 1:]
            next_colon_index = substr.find(":")
            substr = substr[next_colon_index + 1:]
            error = substr.strip()
            if error not in ignored_errors:
                errors.append(f"Line {line_number}: {error}")

    return errors


def validate():
    global _failed_variants

    variant_folder = _params["variant_folder"]

    variants = []
    variants.extend(glob.glob(os.path.join(variant_folder, "*.vert")))
    variants.extend(glob.glob(os.path.join(variant_folder, "*.frag")))

    counter = 0
    total_variants = len(variants)
    update_rate = 50
    print("Validating {} shader variants...".format(total_variants))
    for sv in variants:
        sp = subprocess.Popen(
            [VALIDATOR, sv], stdout=subprocess.PIPE, stderr=None)
        output = sp.communicate()[0].decode("utf-8")
        if sp.returncode != 0:
            errors = get_errors(output.splitlines())
            _failed_variants.append({
                "file": sv,
                "errors": errors
            })

        if (counter % update_rate) == 0:
            print("...{:.2f}% complete".format(
                100 * (counter / total_variants)))

        counter += 1

    return len(variants), len(_failed_variants)


def write_results(num_variants, num_failures):
    results = {
        "Results": {
            "Total": num_variants,
            "Passed": (num_variants - num_failures),
            "Failed": num_failures
        },

        "Fails": _failed_variants
    }

    with open(_params["results"], "w") as f:
        yaml.dump(results, f, sort_keys=False)


def run():
    generate_shader_variants()

    num_variants, num_failures = validate()
    write_results(num_variants, num_failures)

    failed_variant_files = [x["file"] for x in _failed_variants]
    failed_variant_files_str = "\n\t\t".join(
        failed_variant_files) if failed_variant_files else "- (None)"

    print("Results:")
    print(
        f"\t- Validated: {num_variants}\n\t- Passed: {num_variants - num_failures}\n\t- Failed: {num_failures}")
    print(f"\t- Failed Variant Files: \n\t\t{failed_variant_files_str}")

    if num_failures > 0:
        return -1
    return 0


def main():
    # Check we have all necessary executables on path
    if shutil.which(VALIDATOR) is None:
        print("glslangValidator not found on system path. Exiting.")
        exit(1)

    # Parse command line arguments
    parse_args()

    # Change to source file's directory, so we don't have to specify full paths for "metadata.yaml" etc
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    if not init():
        print("Failed to initialise validator.")
        exit(-1)

    print("Validating metadata...")
    if not metadata_validator.validate(_vertex_shaders + _fragment_shaders, _all_shader_filepaths, [header["path"] for header in _header_files]):
        print("Failed Metadata validation. Ensure that all defines are being tested, and that no superfluous defines are being tested either.")
        exit(-1)

    print("Running Shader Validator...")
    result = run()

    print("Validation complete. Performing post-cleaning...")
    post_cleanup()
    print("Shader validator done.")
    exit(result)


if __name__ == "__main__":
    main()
