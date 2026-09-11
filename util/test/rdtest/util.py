from __future__ import annotations
import array
import os
import re
import math
import struct
import platform
import hashlib
import zipfile
import subprocess
from typing import Callable, Tuple, List, Union
from . import png
from rdtest.remoteserver import RemoteServer, AndroidRemoteServer 


# Android app IDs for the demos
ADRD_DEMO_APP32 = 'renderdoc.org.demos.arm32'
ADRD_DEMO_APP64 = 'renderdoc.org.demos.arm64'


# Thanks to https://stackoverflow.com/a/3431838 for this file definition
def _md5_file(fname: str):
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
_demos_timeout = None


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

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import qrenderdoc

def set_capture_context(ctx: qrenderdoc.CaptureContext):
    global _capture_context
    _capture_context = ctx


_capture_context: qrenderdoc.CaptureContext | None = None

def get_capture_context() -> qrenderdoc.CaptureContext | None:
    return _capture_context


def set_remote_server(server: RemoteServer | None):
    global _remote_server
    _remote_server = server


def get_remote_server() -> RemoteServer | None:
    return _remote_server


def create_adb_device(name: str):
    server = AndroidRemoteServer(name)
    set_remote_server(server)


def get_current_test_name():
    return _test_name


def set_demos_timeout(timeout: int):
    global _demos_timeout
    _demos_timeout = timeout


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


def get_demos_timeout():
    return _demos_timeout


def get_tmp_path(name: str, test = ""):
    if test == "":
        test = get_current_test_name()
    os.makedirs(os.path.join(_temp_dir, test), exist_ok=True)
    return os.path.join(_temp_dir, test, name)


def get_android_demo_app_name():
    if ADRD_DEMO_APP64 in get_demos_binary():
        return ADRD_DEMO_APP64

    return ADRD_DEMO_APP32


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
        raise FileNotFoundError(f"Can't open {sanitise_filename(out_path)} for write")

    writer = png.Writer(dimensions[0], dimensions[1], alpha=has_alpha, greyscale=False, compression=7)
    writer.write(f, rows)
    f.close()


def png_load_data(in_path: str):
    reader = png.Reader(filename=in_path)
    return list(reader.read()[2])


def png_compare(test_img: str, ref_img: str, tolerance: int = 2):
    test_reader = png.Reader(filename=test_img)
    ref_reader = png.Reader(filename=ref_img)

    test_w, test_h, test_data, test_info = test_reader.read()
    ref_w, ref_h, ref_data, ref_info = ref_reader.read()

    # lookup rgba data straight
    rgba_get: Callable[[bytearray | array.array[int], int], int] = lambda data, x: data[x]
    # lookup rgb data and return 255 for alpha
    rgb_get: Callable[[bytearray | array.array[int], int], int] = lambda data, x: data[ (x >> 2)*3 + (x % 4) ] if (x % 4) < 3 else 255

    test_get = (rgba_get if test_info['alpha'] else rgb_get)
    ref_get = (rgba_get if ref_info['alpha'] else rgb_get)

    if test_w != ref_w or test_h != test_h:
        return False

    is_same = True
    diff_data: List[bytes] = []

    for test_row, ref_row in zip(test_data, ref_data):

        diff = [min(255, abs(test_get(test_row, i) - ref_get(ref_row, i))*4) for i in range(0, test_w*4)]

        is_same = is_same and not any([d > tolerance*4 for d in diff])

        diff_data.append(bytes([255 if i % 4 == 3 else d for i, d in enumerate(diff)]))

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

    test_files: List[Tuple[str, int, str]] = []
    for file in test.infolist():
        hash_md5 = hashlib.md5()
        with test.open(file.filename) as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        test_files.append((file.filename, file.file_size, hash_md5.hexdigest()))

    ref_files: List[Tuple[str, int, str]] = []
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

# Python 3.8 doesn't support | for complex types even with future
VectorValue = Union[Tuple[int,...], List[int], Tuple[float,...], List[float]]
ScalarValue = Union[int, float]
ScalarOrVectorValue = Union[VectorValue, ScalarValue]

try:
    from typing import TypeGuard
except ImportError:
    pass

def is_scalar(x: object) -> TypeGuard[ScalarValue]:
    return isinstance(x, int) or isinstance(x, float)

def is_vector(x: object) -> TypeGuard[VectorValue]:
    return isinstance(x, list) or isinstance(x, tuple)

def _scalar_compare_diff(ref: int | float, data: int | float, eps=FLT_EPSILON) -> Tuple[bool, float]:
    # if the types are different this is probably 0.0 == 0 or something. Just compare straight by casting to floats
    if type(data) != type(data):
        return float(data) == float(ref), abs(float(data)-float(ref))

    # Special handling for NaNs - NaNs are always equal to NaNs, but NaN is never equal to any other value
    if math.isnan(ref) and math.isnan(data):
        return True, 0.0
    elif math.isnan(ref) != math.isnan(data):
        return False, 0.0

    # Same as above for infs, but check the sign
    if math.isinf(ref) and math.isinf(data):
        return math.copysign(1.0, ref) == math.copysign(1.0, data), 0.0
    elif math.isinf(ref) != math.isinf(data):
        return False, 0.0

    # Floats are equal if the absolute difference is less than epsilon times the largest.
    largest = max(abs(ref), abs(data))
    eps = largest * eps if largest > 1.0 else eps
    return abs(ref-data) <= eps, abs(ref-data)

def value_compare_diff(ref: ScalarOrVectorValue, data: ScalarOrVectorValue, eps=FLT_EPSILON) -> Tuple[bool, float]:
    if (type(data) == list or type(data) == tuple) and len(data) == 1 and type(data[0]) == type(ref):
        return value_compare_diff(ref, data[0], eps)

    if is_scalar(ref) and is_scalar(data):
        return _scalar_compare_diff(ref, data, eps)

    # if we're comparing scalar to a 1-length tuple or list, compare against the first element. We only expect this for
    # data where it's possibly autogenerated
    if is_scalar(ref):
        assert is_vector(data) and len(data) == 1
        return value_compare_diff(ref, data[0], eps)

    assert is_vector(ref) and is_vector(data)

    # Lists/tuples are not equal if they have different lengths
    if len(ref) != len(data):
        return False, 0.0

    ret = (True, 0.0)

    for i in range(len(ref)):
        is_eq, diff_amt = value_compare_diff(ref[i], data[i], eps)
        if not is_eq:
            ret = (False, max(ret[1], diff_amt))

    return ret


def value_compare(ref: ScalarOrVectorValue | str | None, data: ScalarOrVectorValue | str | None, eps=FLT_EPSILON):
    # some simple cases that don't need diff compares and we allow for uniformity
    if ref is None or data is None: return data is ref and data is None
    if isinstance(ref, str) or isinstance(data, str): return ref == data

    is_eq, diff_amt = value_compare_diff(ref, data, eps)
    return is_eq


def run_demo_blocking(args: List[str], timeout=100):
    """
    Executes the demo application with the given args and returns the stdout.

    :return: stdout, on Android this includes stripping of logcat prefixes
    """
    if get_remote_server() is None:
        raw = subprocess.run([get_demos_binary()] + args, stdout=subprocess.PIPE).stdout
        return str(raw, 'utf-8')

    return get_remote_server().run_demos(args, timeout)


def target_path_exists(path: str, timeout=10):
    if get_remote_server() is None:
        return os.path.exists(path)
    
    return get_remote_server().path_exists(path)
