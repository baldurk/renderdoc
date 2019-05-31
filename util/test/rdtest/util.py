import sys
import os
import re
import time
import math
import struct
import platform
import hashlib
import zipfile
from typing import Tuple, List
from . import png


def _timestr():
    return time.strftime("%Y%m%d_%H_%M_%S", time.gmtime()) + "_" + str(round(time.time() % 1000))


# Thanks to https://stackoverflow.com/a/3431838 for this file definition
def _md5_file(fname):
    hash_md5 = hashlib.md5()
    with open(fname, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()


_root_dir = os.getcwd()
_artifact_dir = os.path.realpath('artifacts')
_data_dir = os.path.realpath('data')
_data_extra_dir = os.path.realpath('data_extra')
_temp_dir = os.path.realpath('tmp')
_test_name = 'Unknown_Test'
_demos_bin = os.path.realpath('demos_x64')


def set_root_dir(path: str):
    global _root_dir
    _root_dir = path


def set_data_dir(path: str):
    global _data_dir
    _data_dir = os.path.abspath(path)


def set_data_extra_dir(path: str):
    global _data_extra_dir
    _data_extra_dir = os.path.abspath(path)


def set_artifact_dir(path: str):
    global _artifact_dir
    _artifact_dir = os.path.abspath(path)


def set_temp_dir(path: str):
    global _temp_dir
    _temp_dir = os.path.abspath(path)


def set_demos_binary(path: str):
    global _demos_bin
    if path == "":
        # Try to guess where the demos binary might be, but if we can't find it fall back to just trying it in PATH
        if struct.calcsize("P") == 8:
            _demos_bin = 'demos_x64'
        else:
            _demos_bin = 'demos_x86'

        if platform.system() == 'Windows':
            _demos_bin += '.exe'

        # Try a few common locations
        attempts = [
            os.path.join(get_root_dir(), _demos_bin),                    # in the root (default for Windows)
            os.path.join(get_root_dir(), 'build', _demos_bin),           # in a build folder (potential for linux)
            os.path.join(get_root_dir(), 'demos', _demos_bin),           # in the demos folder (ditto)
            os.path.join(get_root_dir(), 'demos', 'build', _demos_bin),  # in build in the demos folder (ditto)
        ]

        for attempt in attempts:
            if os.path.exists(attempt):
                _demos_bin = os.path.abspath(attempt)
                break
    else:
        _demos_bin = os.path.abspath(path)


def set_current_test(name: str):
    global _test_name
    _test_name = name


def get_root_dir():
    return _root_dir


def get_data_dir():
    return _data_dir


def get_data_path(name: str):
    return os.path.join(_data_dir, name)


def get_data_extra_dir():
    return _data_extra_dir


def get_data_extra_path(name: str):
    return os.path.join(_data_extra_dir, name)


def get_artifact_dir():
    return _artifact_dir


def get_artifact_path(name: str):
    return os.path.join(_artifact_dir, name)


def get_tmp_dir():
    return _temp_dir


def get_demos_binary():
    return _demos_bin


def get_tmp_path(name: str):
    os.makedirs(os.path.join(_temp_dir, _test_name), exist_ok=True)
    return os.path.join(_temp_dir, _test_name, name)


def sanitise_filename(name: str):
    name = name.replace(_artifact_dir, '') \
               .replace(get_tmp_dir(), '') \
               .replace(get_root_dir(), '') \
               .replace('\\', '/')

    return re.sub('^/', '', name)


def png_save(out_path: str, rows: List[bytes], dimensions: Tuple[int, int], has_alpha: bool):
    try:
        f = open(out_path, 'wb')
    except Exception as ex:
        raise FileNotFoundError("Can't open {} for write".format(sanitise_filename(out_path)))

    writer = png.Writer(dimensions[0], dimensions[1], alpha=has_alpha, greyscale=False, compression=7)
    writer.write(f, rows)
    f.close()


def png_load_data(in_path: str):
    reader = png.Reader(filename=in_path)
    return list(reader.read()[2])


def png_load_dimensions(in_path: str):
    reader = png.Reader(filename=in_path)
    info = reader.read()
    return (info[0], info[1])


def png_compare(test_img: str, ref_img: str, tolerance: int = 2):
    test_reader = png.Reader(filename=test_img)
    ref_reader = png.Reader(filename=ref_img)

    test_w, test_h, test_data, test_info = test_reader.read()
    ref_w, ref_h, ref_data, ref_info = ref_reader.read()

    # lookup rgba data straight
    rgba_get = lambda data, x: data[x]
    # lookup rgb data and return 255 for alpha
    rgb_get = lambda data, x: data[ (x >> 2)*3 + (x % 4) ] if (x % 4) < 3 else 255

    test_get = (rgba_get if test_info['alpha'] else rgb_get)
    ref_get = (rgba_get if ref_info['alpha'] else rgb_get)

    if test_w != ref_w or test_h != test_h:
        return False

    is_same = True
    diff_data = []

    for test_row, ref_row in zip(test_data, ref_data):

        diff = [min(255, abs(test_get(test_row, i) - ref_get(ref_row, i))*4) for i in range(0, test_w*4)]

        is_same = is_same and not any([d > tolerance*4 for d in diff])

        diff_data.append([255 if i % 4 == 3 else d for i, d in enumerate(diff)])

    if is_same:
        return True

    # If the diff fails, dump the difference to a file
    diff_file = get_tmp_path('diff.png')

    if os.path.exists(diff_file):
        os.remove(diff_file)

    png_save(diff_file, diff_data, (test_w, test_h), True)

    return False


def md5_compare(test_file: str, ref_file: str):
    return _md5_file(test_file) == _md5_file(ref_file)


def zip_compare(test_file: str, ref_file: str):
    test = zipfile.ZipFile(test_file)
    ref = zipfile.ZipFile(ref_file)

    test_files = []
    for file in test.infolist():
        hash_md5 = hashlib.md5()
        with test.open(file.filename) as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        test_files.append((file.filename, file.file_size, hash_md5.hexdigest()))

    ref_files = []
    for file in ref.infolist():
        hash_md5 = hashlib.md5()
        with test.open(file.filename) as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        ref_files.append((file.filename, file.file_size, hash_md5.hexdigest()))

    test.close()
    ref.close()

    return test_files == ref_files


# Use the 32-bit float epsilon, not sys.float_info.epsilon which is for double floats
FLT_EPSILON = 2.0*1.19209290E-07


def value_compare(ref, data):
    if type(ref) == float:
        if type(data) != float:
            return False

        # Special handling for NaNs - NaNs are always equal to NaNs, but NaN is never equal to any other value
        if math.isnan(ref) and math.isnan(data):
            return True
        elif math.isnan(ref) != math.isnan(data):
            return False

        # Same as above for infs, but check the sign
        if math.isinf(ref) and math.isinf(data):
            return math.copysign(1.0, ref) == math.copysign(1.0, data)
        elif math.isinf(ref) != math.isinf(data):
            return False

        # Floats are equal if the absolute difference is less than epsilon times the largest.
        largest = max(abs(ref), abs(data))
        eps = largest * FLT_EPSILON if largest > 1.0 else FLT_EPSILON
        return abs(ref-data) < eps
    elif type(ref) == list or type(ref) == tuple:
        # tuples and lists can be treated interchangeably
        if type(data) != list and type(data) != tuple:
            return False

        # Lists are equal if they have the same length and all members have value_compare(i, j) == True
        if len(ref) != len(data):
            return False

        for i in range(len(ref)):
            if not value_compare(ref[i], data[i]):
                return False

        return True
    elif type(ref) == dict:
        if type(data) != dict:
            return False

        # Similarly, dicts are equal if both have the same set of keys and
        # corresponding values are value_compare(i, j) == True
        if ref.keys() != data.keys():
            return False

        for i in ref.keys():
            if not value_compare(ref[i], data[i]):
                return False

        return True
    else:
        # For other types, just use normal comparison
        return ref == data
