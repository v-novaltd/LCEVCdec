import re
import os

try:
    import numpy as np
except ImportError as err:
    print(f"Unable to import numpy, diff based tests won't work")
    np = None

from functools import reduce
from math import ceil
from .vooya_specifiers import resolution_from_filename, bitdepth_from_filename, yuv_format_from_filename

def read_yuv(filename, size='auto', yuv_format='auto', bitdepth='auto'):
    basename = os.path.basename(filename)

    if size == 'auto':
        width, height = resolution_from_filename(basename)
    else:
        width, height = size

    if bitdepth == 'auto':
        bitdepth = bitdepth_from_filename(basename)
        if bitdepth is None:
            bitdepth = 8

    if yuv_format == 'auto':
        yuv_format = yuv_format_from_filename(basename)
        if yuv_format is None:
            yuv_format = '420p'

    container_bitdepth = bitdepth
    container_type = np.uint8
    if bitdepth == 10 or bitdepth == 12 or bitdepth == 16:
        container_bitdepth = 16
        container_type = np.uint16

    # print('container_bitdepth={}'.format(container_bitdepth))

    plane_subsamplings = []
    if yuv_format == '420p':
        plane_subsamplings = [
            [1, 1],
            [2, 2],
            [2, 2],
        ]
    elif yuv_format == '422p':
        plane_subsamplings = [
            [1, 1],
            [1, 2],
            [1, 2],
        ]
    elif yuv_format == '444p':
        plane_subsamplings = [
            [1, 1],
            [1, 1],
            [1, 1],
        ]

    plane_shapes = [
        [ceil(width/plane_subsampling[0]), ceil(height/plane_subsampling[1])] for plane_subsampling in plane_subsamplings
    ]

    n_planes = len(plane_shapes)

    plane_sizes = []
    pixel_size = int(container_bitdepth / 8)
    # print('pixel size: {}'.format(pixel_size))

    plane_sizes = list(map(lambda plane: pixel_size * plane_shapes[plane][0] * plane_shapes[plane][1],
                           range(n_planes)))
    # print('plane sizes: {}'.format(plane_sizes))

    frame_size = reduce(lambda a, b: a + b, plane_sizes, 0)

    plane_shapes = [
        [ceil(width/plane_subsampling[0]), ceil(height/plane_subsampling[1])] for plane_subsampling in plane_subsamplings
    ]

    with open(filename, 'rb') as input_file:
        yuv_data = input_file.read()

    file_size = len(yuv_data)

    if file_size % frame_size != 0:
        print('Invalid file size: {} not multiple of {} (expected frame size)'.format(
            file_size, frame_size))
        return None

    n_frames = int(file_size / frame_size)

    frame_offsets = map(lambda frame: frame * frame_size, range(n_frames))

    plane_offsets = list(map(lambda prev_plane_sizes:
                             reduce(lambda a, b: a + b, prev_plane_sizes, 0),
                             map(lambda plane: plane_sizes[:plane], range(n_planes))))

    # print('plane_offsets: {}'.format(plane_offsets))

    frame_pixels = int(frame_size / pixel_size)
    yuv_frames = np.ndarray(shape=(n_frames, frame_pixels),
                            dtype=container_type, buffer=yuv_data)

    yuv_frame_planes = [
        yuv_frames[
            :,
            int(plane_offsets[plane]/pixel_size):int(plane_offsets[plane]/pixel_size) + int(plane_sizes[plane]/pixel_size)
        ].reshape(
            n_frames,
            plane_shapes[plane][0],
            plane_shapes[plane][1]
        )
        for plane in range(n_planes)
    ]

    return yuv_frame_planes
