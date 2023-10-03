import os


def make_shader_basename(name, extension):
    return f"{name}.{extension}"


def find_shader_path(basename, all_shader_filepaths):
    found_shader_paths = [
        shader for shader in all_shader_filepaths if os.path.basename(shader) == basename]
    if not found_shader_paths:
        # print(
        #     f"Could not find any file matching '{basename}' in shader directories. Skipping.")
        return None
    elif len(found_shader_paths) > 1:
        print(
            f"More than one file found matching '{basename}' in shader directories. Skipping.")
        return None

    return found_shader_paths[0]


# re-implementations of python 3.9 built-ins:

def remove_prefix(string, prefix):
    if prefix in string:
        return string[len(prefix):]
    return string


def combine_dicts(dict1, dict2):
    dict_out = {}
    for key in dict1:
        dict_out[key] = dict1[key]
    for key in dict2:
        dict_out[key] = dict2[key]
    return dict_out
