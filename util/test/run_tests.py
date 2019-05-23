import argparse
import os
import sys

script_dir = os.path.realpath(os.path.dirname(__file__))

parser = argparse.ArgumentParser()
parser.add_argument('-r', '--renderdoc',
                    help="The location of the renderdoc library to use", type=str)
parser.add_argument('-p', '--pyrenderdoc',
                    help="The location of the renderdoc python module to use", type=str)
parser.add_argument('-l', '--list',
                    help="Lists the tests available to run", action="store_true")
parser.add_argument('-t', '--test_include', default=".*",
                    help="The tests to include, as a regexp filter", type=str)
parser.add_argument('-x', '--test_exclude', default="",
                    help="The tests to exclude, as a regexp filter", type=str)
parser.add_argument('--in-process',
                    help="Lists the tests available to run", action="store_true")
parser.add_argument('--slow-tests',
                    help="Run potentially slow tests", action="store_true")
parser.add_argument('--data', default=os.path.join(script_dir, "data"),
                    help="The folder that reference data is in. Will not be modified.", type=str)
parser.add_argument('--demos-binary', default="",
                    help="The path to the built demos binary.", type=str)
parser.add_argument('--data-extra', default=os.path.join(script_dir, "data_extra"),
                    help="The folder that extra reference data is in (typically very large captures that aren't part "
                         "of the normal repo). Will not be modified.", type=str)
parser.add_argument('--artifacts', default=os.path.join(script_dir, "artifacts"),
                    help="The folder to put output artifacts in. Will be completely cleared.", type=str)
parser.add_argument('--temp', default=os.path.join(script_dir, "tmp"),
                    help="The folder to put temporary run data in. Will be completely cleared.", type=str)
parser.add_argument('--debugger',
                    help="Enable debugger mode, exceptions are not caught by the framework.", action="store_true")
# Internal command, when we fork out to run a test in a separate process
parser.add_argument('--internal_run_test', help=argparse.SUPPRESS, type=str, required=False)
# Internal command, when we re-run as admin to register vulkan layer
parser.add_argument('--internal_vulkan_register', help=argparse.SUPPRESS, action="store_true", required=False)
args = parser.parse_args()

if args.renderdoc is not None:
    if os.path.isfile(args.renderdoc):
        os.environ["PATH"] += os.pathsep + os.path.abspath(os.path.dirname(args.renderdoc))
    elif os.path.isdir(args.renderdoc):
        os.environ["PATH"] += os.pathsep + os.path.abspath(args.renderdoc)
    else:
        raise RuntimeError("'{}' is not a valid path to the renderdoc library".format(args.renderdoc))

custom_pyrenderdoc = None

if args.pyrenderdoc is not None:
    if os.path.isfile(args.pyrenderdoc):
        custom_pyrenderdoc = os.path.abspath(os.path.dirname(args.pyrenderdoc))
    elif os.path.isdir(args.pyrenderdoc):
        custom_pyrenderdoc = os.path.abspath(args.pyrenderdoc)
    else:
        raise RuntimeError("'{}' is not a valid path to the pyrenderdoc module".format(args.pyrenderdoc))

    sys.path.insert(0, custom_pyrenderdoc)

sys.path.insert(0, os.path.realpath(os.path.dirname(__file__)))

data_path = os.path.realpath(args.data)
data_extra_path = os.path.realpath(args.data_extra)
temp_path = os.path.realpath(args.temp)
demos_binary = args.demos_binary
if demos_binary != "":
    demos_binary = os.path.realpath(demos_binary)

os.chdir(sys.path[0])

artifacts_dir = os.path.realpath(args.artifacts)

try:
    import rdtest
except (ModuleNotFoundError, ImportError) as ex:
    # very simple output, to ensure we have *something*
    import shutil

    if os.path.exists(artifacts_dir):
        shutil.rmtree(artifacts_dir, ignore_errors=True)
    os.makedirs(artifacts_dir, exist_ok=True)

    with open(os.path.join(artifacts_dir, 'output.log.html'), "w") as f:
        f.write("<body><h1>Failed to import rdtest: {}</h1></body>".format(ex))

    print("Couldn't import renderdoc module. Try specifying path to python module with --pyrenderdoc " +
          "or the path to the native library with --renderdoc")
    print(ex)

    sys.exit(1)

from tests import *

if args.list:
    for test in rdtest.get_tests():
        print("Test: {}".format(test.__name__))
    sys.exit(0)

rdtest.set_root_dir(os.path.realpath(os.path.dirname(__file__)))
rdtest.set_artifact_dir(artifacts_dir)
rdtest.set_data_dir(data_path)
rdtest.set_data_extra_dir(data_extra_path)
rdtest.set_temp_dir(temp_path)
rdtest.set_demos_binary(demos_binary)

# debugger option implies in-process test running
if args.debugger:
    args.in_process = True

if args.internal_vulkan_register:
    rdtest.vulkan_register()
elif args.internal_run_test is not None:
    rdtest.internal_run_test(args.internal_run_test)
else:
    rdtest.run_tests(args.test_include, args.test_exclude, args.in_process, args.slow_tests, args.debugger)
