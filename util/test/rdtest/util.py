import sys
import os
import re
import time
import hashlib
import zipfile
from PIL import Image, ImageChops, ImageStat


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


def get_tmp_path(name: str):
    os.makedirs(os.path.join(_temp_dir, _test_name), exist_ok=True)
    return os.path.join(_temp_dir, _test_name, name)


def sanitise_filename(name: str):
    name = name.replace(_artifact_dir, '') \
               .replace(get_tmp_dir(), '') \
               .replace(get_root_dir(), '') \
               .replace('\\', '/')

    return re.sub('^/', '', name)


def image_compare(test_img: str, ref_img: str, tolerance: int = 2):
    try:
        out = Image.open(test_img)
    except Exception as ex:
        raise FileNotFoundError("Can't open {}".format(sanitise_filename(test_img)))

    try:
        ref = Image.open(ref_img)
    except Exception as ex:
        raise FileNotFoundError("Can't open {}".format(sanitise_filename(ref_img)))

    if out.mode != ref.mode or out.size != ref.size:
        return False

    # Generate the difference
    diff = ImageChops.difference(out, ref)

    # Subtract N from the difference, to allow for off-by-N errors that can be caused by rounding.
    # For example clearing to 0.5, 0.5, 0.5 has two valid representations: 127,127,127 and 128,128,128
    # which are equally far from true 0.5.
    diff = ImageChops.subtract(diff, Image.new(diff.mode, (diff.width, diff.height),
                                               (tolerance, tolerance, tolerance, tolerance)))

    # If the diff fails, dump the difference to a file
    diff_file = get_tmp_path('diff.png')

    if os.path.exists(diff_file):
        os.remove(diff_file)

    if sum(ImageStat.Stat(diff).sum) > 0:
        # this does (img1 + img2) / scale, so scale=0.5 means we multiply the image by 2/0.5 = 4
        diff = ImageChops.add(diff, diff, scale=0.5)
        diff.convert("RGB").save(diff_file)
        return False

    return True


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
