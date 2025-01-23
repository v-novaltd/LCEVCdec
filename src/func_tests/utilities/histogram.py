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

import os
import numpy as np


def calc_yuv_frame_size(width, height, subsampling_ratio, np_int_type, color_space):
    y, u, v = subsampling_ratio
    chroma_to_luma_ratio = 0 if color_space == 'gray' else (u + v) / y
    y_size = width * height
    uv_size = int((chroma_to_luma_ratio * y_size) / 2)
    if color_space == 'nv12':
        uv_size *= 2
    if np_int_type == np.int16:
        y_size *= 2
        uv_size *= 2

    return y_size, uv_size


def extract_yuv_data(input_yuv_file, width, height, subsampling_ratio, np_int_type, color_space):
    y_size, uv_size = calc_yuv_frame_size(
        width, height, subsampling_ratio, np_int_type, color_space)
    y_array = np.array([])
    u_array = np.array([])
    v_array = np.array([])
    y_data = input_yuv_file.read(y_size)
    y_new_array = np.frombuffer(y_data, dtype=np_int_type)
    y_array = np.concatenate((y_array, y_new_array), axis=None)

    u_data = input_yuv_file.read(uv_size)
    u_new_array = np.frombuffer(u_data, dtype=np_int_type)
    u_array = np.concatenate((u_array, u_new_array), axis=None)

    # for nv12, we read frames to the U (i.e. UV) plane and de-interleave after
    if not color_space == 'nv12':
        v_data = input_yuv_file.read(uv_size)
        v_new_array = np.frombuffer(v_data, dtype=np_int_type)
        v_array = np.concatenate((v_array, v_new_array), axis=None)

    if color_space == 'nv12':
        v_array = np.array(u_array[1::2])
        u_array = np.array(u_array[::2])
    return y_array, u_array, v_array


def histogram_helper(reference, sample, bin_range):
    histo = np.histogram(reference - sample, bins=bin_range, density=False)
    # Remove empty head and tail of histogram
    zh = list(zip(histo[1], histo[0]))
    while zh and zh[0][1] == 0:
        zh = zh[1:]
    while zh and zh[-1][1] == 0:
        zh.pop()

    total = sum(histo[0])
    ret = dict()
    for level, value in zh:
        percentage = value * 100.0 / total
        ret[level] = percentage
    return ret


def average_histograms(histos, frames):
    levels = set()
    [[levels.add(key) for key in histo.keys()] for histo in histos]
    averaged_histo = {level: sum(histo.get(level, 0) / frames for histo in histos)
                      for level in levels}

    return dict(sorted(averaged_histo.items()))


def pixel_deviation_histogram(reference_filepath, sample_filepath, bits, width, height, subsampling_ratio='420', color_space='yuv'):
    subsampling_ratio = [int(s) for s in subsampling_ratio]
    np_int_type = np.int8 if bits <= 8 else np.int16
    bin_range = range(int((-2 ** bits) / 2), int(((2 ** bits) / 2) - 1))
    if color_space == 'nv12':
        subsampling_ratio = 4, 2, 0

    assert os.path.getsize(reference_filepath) == os.path.getsize(
        sample_filepath), "YUVs are different sizes - cannot compare"

    y_size, uv_size = calc_yuv_frame_size(
        width, height, subsampling_ratio, np_int_type, color_space)
    frame_size = y_size + (uv_size * 2)
    frames = int(os.path.getsize(reference_filepath) // frame_size)
    y_histos, u_histos, v_histos = list(), list(), list()
    reference_file = open(reference_filepath, 'rb')
    sample_file = open(sample_filepath, 'rb')
    for _ in range(frames):
        reference_y, reference_u, reference_v = \
            extract_yuv_data(reference_file, width, height,
                             subsampling_ratio, np_int_type, color_space)
        sample_y, sample_u, sample_v = \
            extract_yuv_data(sample_file, width, height,
                             subsampling_ratio, np_int_type, color_space)

        y_histos.append(histogram_helper(reference_y, sample_y, bin_range))
        u_histos.append(histogram_helper(reference_u, sample_u, bin_range))
        v_histos.append(histogram_helper(reference_v, sample_v, bin_range))
    reference_file.close()
    sample_file.close()

    y_histo = average_histograms(y_histos, frames)
    u_histo = average_histograms(u_histos, frames)
    v_histo = average_histograms(v_histos, frames)

    return y_histo, u_histo, v_histo
