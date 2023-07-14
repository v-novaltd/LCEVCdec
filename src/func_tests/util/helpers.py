import hashlib
import itertools
import re
from collections import namedtuple
from typing import Any, Generator, List, Tuple, Union

from _pytest.fixtures import FixtureRequest
from _pytest.mark.structures import Mark, ParameterSet
from _pytest.python import Metafunc

Parameter = Union[str, ParameterSet]


def get_test_parameters(metafunc: Metafunc) -> Generator[Mark, None, None]:
    """ Gets all parametrize marks from a metafunc """

    if not hasattr(metafunc.function, "pytestmark"):
        return None

    for mark in metafunc.function.pytestmark:
        if mark.name == "parametrize":
            yield mark


def get_marked_parameters(parameters: List[Parameter], mark: Mark) -> Generator[ParameterSet, None, None]:
    """ Gets all paramters marked with the given mark """
    for parameter in parameters:
        if isinstance(parameter, ParameterSet):
            if mark.name in [mark.name for mark in parameter.marks]:
                yield parameter


def get_test_name(request: FixtureRequest) -> str:
    return request.node.callspec.metafunc.function.__name__


def get_unique_name(request: FixtureRequest, ignored_params: List[str] = []):
    values = [str(value) for param, value in request.node.callspec.params.items()
              if param not in ignored_params]

    return get_test_name(request) + "[" + "_".join(values) + "]"


def get_formatted_test_name(request: FixtureRequest, ignored_params: List[str] = None):
    funcname = get_test_name(request)
    params = request.node.callspec.params
    result = ""

    # Base
    if 'yuv' in params.keys():
        yuv = params['yuv']
        if 'rgb' in yuv:
            result += 'rgb'
        elif '420p' in yuv:
            result += '420p'
        elif 'nv12' in yuv:
            result += 'nv12'

    # depth
    if 'yuv' in params.keys():
        yuv = params['yuv']
        if '8bit' in yuv:
            result += '\t8bit'
        elif '10bit' in yuv:
            result += '\t10bit'

    # Mode
    if 'cpu' in funcname:
        result += '\tcpu'
    else:
        result += '\tgpu'

    # Base
    if 'base' in params.keys():
        base = params['base']
        if 'rgb' in base:
            result += '\trgb'
        elif '420p' in base:
            result += '\t420p'
        elif 'nv12' in base:
            result += '\tnv12'

    # Scaling
    if 'scaling_mode_level0' in params.keys():
        result += '\t'
        result += params['scaling_mode_level0']

    # Passthrough
    result += '\t'
    if 'passthrough' in params.keys():
        result += str(params['passthrough'])
    else:
        result += '0'

    return result

def generate_decodes(with_parallel_decode: bool = True) -> Generator[Tuple[bool, str], None, None]:
    parallel_decode_params =  list(set([False, with_parallel_decode]))
    parallel_decode_names = {False: "serial", True: "parallel"}
    for x in parallel_decode_params:
        yield x, parallel_decode_names[x]


def generate_sequences(inputs: List[Any], bases: List[Any], depths: List[Any],
                       with_scaling_modes: str = None) -> Generator[Tuple[str, int, namedtuple], None, None]:
    """
    Generates all valid combinations of input parameters. If scaling modes is enabled will generated
    both 2D and 1D bases, otherwise it will only generate 2D bases

    Returns a human readable name, bit depth and then a namedtuple of the sequence with the
    following members:
        input - the full res input yuv
        base - the half res (horizontally or both) base yuv
        scaling_mode_level0/1 - These are only present if with_scaling_modes == simple or complex

    Bit depth is separated as we don't actually need to pass this to the encoder/decoder but is
    useful for some doing some extra logic
    """

    scaling_modes = [("2D", "0D")]
    keys = ["input", "base"]

    if with_scaling_modes:
        assert with_scaling_modes == "simple" or with_scaling_modes == "complex", f"Invalid scaling modes specified: {with_scaling_modes}"
        scaling_modes.append(("1D", "0D"))
        keys.append("scaling_mode_level0")
        keys.append("scaling_mode_level1")

        if with_scaling_modes == "complex":
            scaling_modes.append(("2D", "2D"))
            scaling_modes.append(("2D", "1D"))
            scaling_modes.append(("1D", "2D"))
            scaling_modes.append(("1D", "1D"))
            scaling_modes.append(("0D", "2D"))
            scaling_modes.append(("0D", "1D"))
            scaling_modes.append(("0D", "0D"))

    Sequence = namedtuple("Sequence", keys)

    for high, base, depth, scaling_mode in itertools.product(inputs, bases, depths, scaling_modes):
        scaling_mode_level0 = scaling_mode[0]
        scaling_mode_level1 = scaling_mode[1]

        if depth not in high:
            continue
        else:
            high = high[depth]

        name = f"{depth}bit"
        valid_base = True

        for key in (depth, scaling_mode_level0, scaling_mode_level1):
            if key in base:
                base = base[key]
            else:
                valid_base = False
                break

        if not valid_base:
            continue

        if with_scaling_modes:
            if depth == 10 and scaling_mode_level0 != "2D":
                continue

            name += f"_{scaling_mode_level0}_{scaling_mode_level1}"
            sequence = Sequence(high, base, scaling_mode_level0, scaling_mode_level1)
        else:
            sequence = Sequence(high, base)

        yield name, depth, sequence


def bitdepth_from_filename(filename):
    matcher = re.compile(r".*_([\d]+)bit[\._]")
    match = matcher.match(filename)
    assert match is not None and len(match.groups()) == 1,\
        "Filename format is invalid, it must contain _{bitdepth}bit"
    return match.groups()[0]


def get_md5(file):
    BLOCK_SIZE = 65536

    file_hash = hashlib.md5()
    with open(file, 'rb') as f:
        fb = f.read(BLOCK_SIZE)
        while len(fb) > 0:
            file_hash.update(fb)
            fb = f.read(BLOCK_SIZE)

    md5 = file_hash.hexdigest()
    return md5
