import re

from validation_shared import make_shader_basename, find_shader_path, remove_prefix


EXTENSION = "glsl"


def validate(shaders_metadata, all_shader_filepaths, header_filepaths):
    valid = True

    header_defines = set()
    for h_path in header_filepaths:
        with open(h_path, "r") as h_file:
            header_source = h_file.read()
        header_defines, dummy = get_defined_defines(header_source)

    for shader in shaders_metadata:
        # get defines used in the source code
        basename = make_shader_basename(shader["name"], EXTENSION)
        path = find_shader_path(basename, all_shader_filepaths)
        if not path:
            continue  # This happens if shader is excluded by a filter
        with open(path, "r") as f:
            shader_source = f.read()
        source_defines = get_externally_set_defines(
            shader_source, header_defines)

        # get defines checked by the metadata
        metadata_defines = []
        if "defines" in shader and shader["defines"]:
            metadata_defines += [d["name"] for d in shader["defines"]]
        if "debug_defines" in shader:
            metadata_defines += [d["name"] for d in shader["debug_defines"]]

        # print differences
        for source_define in source_defines:
            if source_define not in metadata_defines:
                print(shader["name"] + ": define '" + source_define + "' is used in the shader, but it's missing from shader_metadata.yaml. Please add it")
                valid = False
        for metadata_define in metadata_defines:
            if metadata_define not in source_defines:
                print(shader["name"] + ": define '" + metadata_define + "' is in shader_metadata.yaml, but the shader doesn't use it. Please remove it")
                valid = False
    return valid


def get_externally_set_defines(shader_file, header_defines):
    # All defines are one or more of: "used defines", "tested defines", and/or "defined defines".
    # Defines are also exactly one of: "internally set defines", "externally set defines", OR
    #   "header defines".
    #
    # A "tested define" is: Any identifier that is TESTED in a preprocessor directive. That is, an
    #   identifier whose value or definition status is checked by #if, #elif, #ifdef, or #ifndef.
    #   For example: "INTERLEAVE" in "#if (INTERLEAVE == 1)"
    # A "used define" is: Any identifier that is used directly in the body of the code, and
    #   contains no lower-case letters. For example: "COMP" in "oColour.COMP += dither()"
    # A "defined define" is: Any identifier which follows the "#define" preprocessor directive.
    #
    # An "internally set define" is: any defined define EXCEPT where: (1) that definition is
    #   subject to another preprocessor test AND (2) that test checks, at least in part, whether
    #   the identifier itself is NOT defined. Colloquially: it's defined outside of its own ifndef.
    # A "header define" is: any define that is defined in a header shader.
    # An "externally set define" is: any non-internally-set, non-header define. Anything which is:
    #   (1) tested, used, or defined, AND
    #   (2) NOT defined in the shader itself (except if it's defined inside its own "ifndef"), AND
    #   (3) NOT defined in a header
    #
    # Note that, sadly, we have to rely on formatting heuristics to distinguish "used" defines from
    # say, variables. That is because there is actually no difference: if I write
    # "oColour.U += magicNumber", you have no way of detecting, syntactically, whether magicNumber
    # is a variable that I forgot to declare, or a define that can be preprocessor-defined. You
    # simply infer that it is probably a variable because a define would be "MAGIC_NUMBER"

    cleaned_file = remove_comments(shader_file)

    tested_defines = get_all_tested_defines(cleaned_file)
    used_defines = get_all_used_defines(cleaned_file)
    int_defined_defines, ext_defined_defines = get_defined_defines(
        cleaned_file)

    external_defines = tested_defines | used_defines | ext_defined_defines
    external_defines -= int_defined_defines
    external_defines -= header_defines
    return external_defines


# Getting the regular expression right was hard, so I went to stack overflow. Function is courtesy
# of this answer: https://stackoverflow.com/questions/241327/remove-c-and-c-comments-using-python
def remove_comments(text):
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            return " "  # note: a space and not an empty string
        else:
            return s
    pattern = re.compile(
        r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    return re.sub(pattern, replacer, text)


# Define getters:

def get_all_tested_defines(shader_file):
    tested_defines = set()
    lines = shader_file.splitlines()
    # taken from the GL spec for ES2 and ES3:
    preproc_keywords = ["define", "undef", "if", "ifdef", "ifndef", "else", "elif",
                        "endif", "error", "pragma", "extension", "version", "line", "defined"]
    preproc_tests = ["if", "elif", "ifdef", "ifndef"]
    for line in lines:
        line = line.lstrip()
        is_preproc_test = False
        if line.startswith("#"):
            line = line.lstrip("#")
            line = line.lstrip()
            for test in preproc_tests:
                if line.startswith(test):
                    is_preproc_test = True
                    break

        if is_preproc_test:
            identifiers = split_into_identifiers(line)
            for idx in identifiers:
                if idx not in preproc_keywords:
                    tested_defines.add(idx)
    return tested_defines


def get_all_used_defines(shader_file):
    used_defines = set()
    for line in shader_file.splitlines():
        line = line.lstrip()
        if line.startswith("#") or len(line) == 0:
            continue
        identifiers = split_into_identifiers(line)
        for ident in identifiers:
            if has_no_lower_case(ident):
                used_defines.add(ident)
    return used_defines


def get_defined_defines(shader_file):
    internally_set_defines = set()
    externally_set_defines = set()

    preprocessor_tests_stack = []
    for line in shader_file.splitlines():
        line = line.lstrip()
        if line.startswith("#if") or line.startswith("#elif"):
            line = line.strip()
            preprocessor_tests_stack.append(line)
        elif line.startswith("#define"):
            line = remove_prefix(line, "#define")
            line = line.strip()
            identifiers = split_into_identifiers(line)
            if len(identifiers) == 0:
                print("#define needs to be followed by an identifier!")
            define = split_into_identifiers(line)[0]
            is_ifndeffed = False
            for test in preprocessor_tests_stack:
                if is_define_ifndeffed(define, test):
                    is_ifndeffed = True
                    break
            if not is_ifndeffed:
                internally_set_defines.add(define)
            else:
                externally_set_defines.add(define)
        elif line.startswith("#endif"):
            preprocessor_tests_stack.pop()

    return internally_set_defines, externally_set_defines


# Helper functions:

def split_into_identifiers(line):
    # Identifiers are defined by unicode. Python's "isidentifier" function and C's compiler (and
    # therefore GLSL's) both use the same unicode criteria. Luckily, all we really need to know is
    # that identifiers can simply be extracted "greedily" from left to right. In other words, to
    # get the identifiers, ignore non-identifier chars until you hit an identifier char. Then,
    # collect chars up until the cumulative string no longer meets the criteria for an identifier
    identifiers = []
    if len(line) == 0:
        return identifiers

    string_so_far = ""
    for char in line:
        candidate_identifier = string_so_far
        if candidate_identifier.isidentifier():
            candidate_identifier += char
            if not candidate_identifier.isidentifier():
                candidate_identifier = candidate_identifier[:-1]
                identifiers.append(candidate_identifier)
                string_so_far = ""
        else:
            string_so_far = ""
        string_so_far += char

    if string_so_far.isidentifier():
        identifiers.append(string_so_far)

    return identifiers


def get_all_not_defineds(line):
    notdef_terms_out = []
    list_of_strings = line.split("!")
    for string in list_of_strings:
        identifiers = split_into_identifiers(string)
        if "defined" in identifiers:
            notdef_terms_out.append(
                identifiers[1 + identifiers.index("defined")])
    return notdef_terms_out


def is_define_ifndeffed(define, current_preproc_test):
    if current_preproc_test.startswith("#ifndef"):
        line = remove_prefix(current_preproc_test, "#ifndef")
        current_ndef = split_into_identifiers(line)[0]
        return define == current_ndef
    elif current_preproc_test.startswith("#if"):
        return define in get_all_not_defineds(current_preproc_test)

    return False


def has_no_lower_case(string):
    for char in string:
        if char.islower():
            return False
    return True
