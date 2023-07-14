from util.read_yuv import read_yuv

try:
    import numpy as np
except ImportError as err:
    print(f"Unable to import numpy, yuv reading won't work")
    np = None


class DiffError(RuntimeError):
    def __init__(self, msg):
        super(DiffError, self).__init__(msg)


def diff_custom(lhs_surface, rhs_surface, test):
    lhs_yuv = read_yuv(lhs_surface)
    rhs_yuv = read_yuv(rhs_surface)

    n_planes = 3
    if len(lhs_yuv) != n_planes:
        raise DiffError(
            f'Expected {n_planes} planes in downsampled surface {lhs_surface}, got {len(lhs_yuv)}')
    if len(rhs_yuv) != n_planes:
        raise DiffError(
            f'Expected {n_planes} planes in downsampled surface {rhs_surface}, got {len(rhs_yuv)}')

    return test(lhs_yuv, rhs_yuv)


def test_diff(lhs_yuv, rhs_yuv, max_diff):
    planes_minmax = []
    n_planes = len(lhs_yuv)

    for plane in range(n_planes):
        lhs_plane = lhs_yuv[plane].astype(np.int16)
        rhs_plane = rhs_yuv[plane].astype(np.int16)

        if len(lhs_plane) != len(rhs_plane):
            raise DiffError("Planes are different sizes")

        if lhs_plane.shape != rhs_plane.shape:
            raise DiffError("Planes are different shapes")

        diff_plane = lhs_plane - rhs_plane
        min_value = np.amin(diff_plane)
        max_value = np.amax(diff_plane)

        planes_minmax.append((min_value, max_value))

    if any(map(lambda minmax: minmax[0] < -max_diff or minmax[1] > max_diff, planes_minmax)):
        raise DiffError(
            f"failed difference test for max_diff={max_diff}, planes diff minmax={planes_minmax}")


def diff_exact(lhs_surface, rhs_surface):
    return diff_custom(lhs_surface, rhs_surface, lambda lhs, rhs: test_diff(lhs, rhs, 0))


def diff_one_off(lhs_surface, rhs_surface):
    return diff_custom(lhs_surface, rhs_surface, lambda lhs, rhs: test_diff(lhs, rhs, 1))


def files_equal(a, b, chunk_size=16384):
    with open(a, 'rb') as file_a:
        with open(b, 'rb') as file_b:
            while True:
                chunk_a = file_a.read(chunk_size)
                chunk_b = file_b.read(chunk_size)

                if not chunk_a and not chunk_b:
                    break

                if chunk_a != chunk_b:
                    return False

    return True
