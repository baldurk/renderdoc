import os
import shutil
import ctypes
import sys
import re
import platform
import subprocess
import threading
import queue
import time
import renderdoc as rd
from . import util
from . import testcase
from .logging import log


def get_tests():
    testcases = []

    for m in sys.modules.values():
        for name in m.__dict__:
            obj = m.__dict__[name]
            if isinstance(obj, type) and issubclass(obj, testcase.TestCase) and obj != testcase.TestCase:
                testcases.append(obj)

    testcases.sort(key=lambda t: (t.slow_test,t.__name__))

    return testcases


RUNNER_TIMEOUT = 30    # Require output every 30 seconds
RUNNER_DEBUG = False   # Debug test runner running by printing messages to track it


def _enqueue_output(process: subprocess.Popen, out, q: queue.Queue):
    try:
        for line in iter(out.readline, b''):
            q.put(line)

            if process.returncode is not None:
                break
    except Exception:
        pass


def _run_test(testclass, failedcases: list):
    name = testclass.__name__

    # Fork the interpreter to run the test, in case it crashes we can catch it.
    # We can re-run with the same parameters
    args = sys.argv.copy()
    args.insert(0, sys.executable)

    # Add parameter to run the test itself
    args.append('--internal_run_test')
    args.append(name)

    test_run = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

    output_threads = []

    test_stdout = queue.Queue()
    t = threading.Thread(target=_enqueue_output, args=(test_run, test_run.stdout, test_stdout))
    t.daemon = True  # thread dies with the program
    t.start()

    output_threads.append(t)

    test_stderr = queue.Queue()
    t = threading.Thread(target=_enqueue_output, args=(test_run, test_run.stderr, test_stderr))
    t.daemon = True  # thread dies with the program
    t.start()

    output_threads.append(t)

    if RUNNER_DEBUG:
        print("Waiting for test runner to complete...")

    while test_run.poll() is None:
        out = err = ""

        if RUNNER_DEBUG:
            print("Checking runner output...")

        try:
            out = test_stdout.get(timeout=RUNNER_TIMEOUT)
            while not test_stdout.empty():
                out += test_stdout.get_nowait()

                if test_run.poll() is not None:
                    break
        except queue.Empty:
            out = None  # No output

        try:
            while not test_stderr.empty():
                err += test_stderr.get_nowait()

                if test_run.poll() is not None:
                    break
        except queue.Empty:
            err = None  # No output

        if RUNNER_DEBUG and out is not None:
            print("Test stdout: {}".format(out))

        if RUNNER_DEBUG and err is not None:
            print("Test stderr: {}".format(err))

        if out is None and err is None and test_run.poll() is None:
            log.error('Timed out, no output within {}s elapsed'.format(RUNNER_TIMEOUT))
            test_run.kill()
            test_run.communicate()
            raise subprocess.TimeoutExpired(' '.join(args), RUNNER_TIMEOUT)

    if RUNNER_DEBUG:
        print("Test runner has finished")

    # If we couldn't get the return code, something went wrong in the timeout above
    # and the program never exited. Try once more to kill it then bail
    if test_run.returncode is None:
        test_run.kill()
        test_run.communicate()
        raise RuntimeError('INTERNAL ERROR: Couldn\'t get test return code')

    for t in output_threads:
        t.join(10)

        if t.is_alive():
            raise RuntimeError('INTERNAL ERROR: Subprocess output thread couldn\'t be closed')

    # Return code of 0 means we exited cleanly, nothing to do
    if test_run.returncode == 0:
        pass
    # Return code of 1 means the test failed, but we have already logged the exception
    # so we just need to mark this test as failed
    elif test_run.returncode == 1:
        failedcases.append(testclass)
    else:
        raise RuntimeError('Test did not exit cleanly while running, possible crash. Exit code {}'
                           .format(test_run.returncode))


def fetch_tests():
    output = subprocess.run([util.get_demos_binary(), '--list-raw'], stdout=subprocess.PIPE).stdout

    # Skip the header, grab all the remaining lines
    tests = str(output, 'utf-8').splitlines()[1:]

    # Split the TSV values and store
    split_tests = [ test.split('\t') for test in tests ]

    return { x[0]: (x[1] == 'True', x[2]) for x in split_tests }


def run_tests(test_include: str, test_exclude: str, in_process: bool, slow_tests: bool, debugger: bool):
    start_time = time.time()

    rd.InitGlobalEnv(rd.GlobalEnvironment(), [])

    # On windows, disable error reporting
    if 'windll' in dir(ctypes):
        ctypes.windll.kernel32.SetErrorMode(1 | 2)  # SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX

    # clean up artifacts and temp folder
    if os.path.exists(util.get_artifact_dir()):
        shutil.rmtree(util.get_artifact_dir(), ignore_errors=True)

    if os.path.exists(util.get_tmp_dir()):
        shutil.rmtree(util.get_tmp_dir(), ignore_errors=True)

    log.add_output(util.get_artifact_path("output.log.html"))

    for file in ['testresults.css', 'testresults.js']:
        shutil.copyfile(os.path.join(os.path.dirname(__file__), file), util.get_artifact_path(file))

    log.rawprint('<meta charset="utf-8"><!-- header to prevent output from being processed as html -->' +
                 '<body><link rel="stylesheet" type="text/css" media="all" href="testresults.css">' +
                 '<script src="testresults.js"></script>' +
                 '<script id="logoutput" type="preformatted">\n\n\n', with_stdout=False)

    plat = os.name
    if plat == 'nt' or 'Windows' in platform.platform():
        plat = 'win32'

    log.header("Tests running for RenderDoc Version {} ({})".format(rd.GetVersionString(), rd.GetCommitHash()))
    log.header("On {}".format(platform.platform()))

    log.comment("plat={} git={}".format(platform.platform(), rd.GetCommitHash()))

    driver = ""

    for api in rd.GraphicsAPI:
        v = rd.GetDriverInformation(api)
        log.print("{} driver: {} {}".format(str(api), str(v.vendor), v.version))

        # Take the first version number we get, but prefer GL as it's universally available and
        # Produces a nice version number & device combination
        if (api == rd.GraphicsAPI.OpenGL or driver == "") and v.vendor != rd.GPUVendor.Unknown:
            driver = v.version

    log.comment("driver={}".format(driver))

    log.print("Demos running from {}".format(util.get_demos_binary()))

    layerInfo = rd.VulkanLayerRegistrationInfo()
    if rd.NeedVulkanLayerRegistration(layerInfo):
        log.print("Vulkan layer needs to be registered: {}".format(str(layerInfo.flags)))
        log.print("My JSONs: {}, Other JSONs: {}".format(layerInfo.myJSONs, layerInfo.otherJSONs))

        # Update the layer registration without doing anything special first - if running automated we might have
        # granted user-writable permissions to the system files needed to update. If possible we register at user
        # level.
        if layerInfo.flags & rd.VulkanLayerFlags.NeedElevation:
            rd.UpdateVulkanLayerRegistration(True)
        else:
            rd.UpdateVulkanLayerRegistration(False)

        # Check if it succeeded
        reg_needed = rd.NeedVulkanLayerRegistration(layerInfo)

        if reg_needed:
            if plat == 'win32':
                # On windows, try to elevate. This will mean a UAC prompt
                args = sys.argv.copy()
                args.append("--internal_vulkan_register")

                ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, ' '.join(args), None, 1)

                time.sleep(10)
            else:
                log.print("Couldn't register vulkan layer properly, might need admin rights")
                sys.exit(1)

        reg_needed = rd.NeedVulkanLayerRegistration(layerInfo)

        if reg_needed:
            log.print("Couldn't register vulkan layer properly, might need admin rights")
            sys.exit(1)

    os.environ['RENDERDOC_DEMOS_DATA'] = util.get_data_path('demos')

    testcase.TestCase.set_test_list(fetch_tests())

    testcases = get_tests()

    include_regexp = re.compile(test_include, re.IGNORECASE)
    exclude_regexp = None
    if test_exclude != '':
        exclude_regexp = re.compile(test_exclude, re.IGNORECASE)
        log.print("Running tests matching '{}' and not matching '{}'".format(test_include, test_exclude))
    else:
        log.print("Running tests matching '{}'".format(test_include))

    failedcases = []
    skippedcases = []

    ver = 0

    if plat == 'win32':
        try:
            ver = sys.getwindowsversion().major
            if ver == 6:
                ver = 7  # Windows 7 is 6.1
        except AttributeError:
            pass

    for testclass in testcases:
        name = testclass.__name__

        instance = testclass()

        supported,unsupported_reason = instance.check_support()

        if not supported:
            log.print("Skipping {} as {}".format(name, unsupported_reason))
            skippedcases.append(testclass)
            continue

        if not include_regexp.search(name):
            log.print("Skipping {} as it doesn't match '{}'".format(name, test_include))
            skippedcases.append(testclass)
            continue

        if exclude_regexp is not None and exclude_regexp.search(name):
            log.print("Skipping {} as it matches '{}'".format(name, test_exclude))
            skippedcases.append(testclass)
            continue

        if not slow_tests and testclass.slow_test:
            log.print("Skipping {} as it is a slow test, which are not enabled".format(name))
            skippedcases.append(testclass)
            continue

        # Print header (and footer) outside the exec so we know they will always be printed successfully
        log.begin_test(name)

        util.set_current_test(name)

        def do():
            if in_process:
                instance.invoketest()
            else:
                _run_test(testclass, failedcases)

        if debugger:
            do()
        else:
            try:
                do()
            except Exception as ex:
                log.failure(ex)
                failedcases.append(testclass)

        log.end_test(name)

    duration = time.time() - start_time

    hours = int(duration / 3600)
    minutes = int(duration / 60) % 60
    seconds = round(duration % 60)

    log.comment("total={} fail={} skip={} time={}".format(len(testcases), len(failedcases), len(skippedcases), duration))
    log.header("Tests complete summary: {} passed out of {} run from {} total in {}:{:02}:{:02}"
               .format(len(testcases)-len(skippedcases)-len(failedcases), len(testcases)-len(skippedcases), len(testcases), hours, minutes, seconds))
    if len(failedcases) > 0:
        log.print("Failed tests:")
    for testclass in failedcases:
        log.print("  - {}".format(testclass.__name__))

    # Print a proper footer if we got here
    log.rawprint('\n\n\n</script>', with_stdout=False)

    if len(failedcases) > 0:
        sys.exit(1)

    sys.exit(0)


def vulkan_register():
    rd.UpdateVulkanLayerRegistration(True)


def internal_run_test(test_name):
    testcases = get_tests()

    rd.InitGlobalEnv(rd.GlobalEnvironment(), [])

    log.add_output(util.get_artifact_path("output.log.html"))

    for testclass in testcases:
        if testclass.__name__ == test_name:
            log.begin_test(test_name, print_header=False)

            util.set_current_test(test_name)

            try:
                instance = testclass()
                instance.invoketest()
                suceeded = True
            except Exception as ex:
                log.failure(ex)
                suceeded = False

            log.end_test(test_name, print_footer=False)

            if suceeded:
                sys.exit(0)
            else:
                sys.exit(1)

    log.error("INTERNAL ERROR: Couldn't find '{}' test to run".format(test_name))
