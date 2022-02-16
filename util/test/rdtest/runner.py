import os
import shutil
import ctypes
import sys
import re
import platform
import subprocess
import threading
import queue
import datetime
import time
import renderdoc as rd
from . import util
from . import testcase
from .logging import log
from pathlib import Path


def get_tests():
    testcases = []

    for m in sys.modules.values():
        for name in m.__dict__:
            obj = m.__dict__[name]
            if isinstance(obj, type) and issubclass(obj, testcase.TestCase) and obj != testcase.TestCase and not obj.internal:
                testcases.append(obj)

    testcases.sort(key=lambda t: (t.slow_test,t.__name__))

    return testcases


RUNNER_TIMEOUT = 90    # Require output every X seconds
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

    out_pending = ""
    err_pending = ""

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
            err = None
            while not test_stderr.empty():
                if err is None:
                    err = ''
                err += test_stderr.get_nowait()

                if test_run.poll() is not None:
                    break
        except queue.Empty:
            err = None  # No output

        if RUNNER_DEBUG:
            if out is not None:
                print("Test stdout: {}".format(out))

            if err is not None:
                print("Test stderr: {}".format(err))
        else:
            if out is not None:
                out_pending += out
            if err is not None:
                err_pending += err

        while True:
            try:
                nl = out_pending.index('\n')
                line = out_pending[0:nl]
                out_pending = out_pending[nl+1:]
                line = line.replace('\r', '')
                sys.stdout.write(line + '\n')
                sys.stdout.flush()
            except:
                break

        while True:
            try:
                nl = err_pending.index('\n')
                line = err_pending[0:nl]
                err_pending = err_pending[nl+1:]
                line = line.replace('\r', '')
                sys.stderr.write(line + '\n')
                sys.stderr.flush()
            except:
                break

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
    start_time = datetime.datetime.now(datetime.timezone.utc)

    rd.InitialiseReplay(rd.GlobalEnvironment(), [])

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

                for i in range(len(args)):
                    if os.path.exists(args[i]):
                        args[i] = str(Path(args[i]).resolve())

                if 'renderdoccmd' in sys.executable:
                    args = ['vulkanlayer', '--register', '--system']

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
    runcases = []

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

        supported, unsupported_reason = instance.check_support()

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

        runcases.append((testclass, name, instance))

    for testclass, name, instance in runcases:
        # Print header (and footer) outside the exec so we know they will always be printed successfully
        log.begin_test(name)

        util.set_current_test(name)

        def do(debugMode):
            if in_process:
                instance.invoketest(debugMode)
            else:
                _run_test(testclass, failedcases)

        if debugger:
            do(True)
        else:
            try:
                do(False)
            except Exception as ex:
                log.failure(ex)
                failedcases.append(testclass)

        log.end_test(name)

    duration = datetime.datetime.now(datetime.timezone.utc) - start_time

    if len(failedcases) > 0:
        logfile = rd.GetLogFile()
        if os.path.exists(logfile):
            log.inline_file('RenderDoc log', logfile)

    log.comment("total={} fail={} skip={} time={}".format(len(testcases), len(failedcases), len(skippedcases), int(duration.total_seconds())))
    log.header("Tests complete summary: {} passed out of {} run from {} total in {}"
               .format(len(runcases)-len(failedcases), len(runcases), len(testcases), duration))
    if len(failedcases) > 0:
        log.print("Failed tests:")
    for testclass in failedcases:
        log.print("  - {}".format(testclass.__name__))

    # Print a proper footer if we got here
    log.rawprint('\n\n\n</script>', with_stdout=False)

    rd.ShutdownReplay()

    if len(failedcases) > 0:
        sys.exit(1)

    sys.exit(0)


def vulkan_register():
    rd.UpdateVulkanLayerRegistration(True)


def launch_remote_server():
    # Fork the interpreter to run the test, in case it crashes we can catch it.
    # We can re-run with the same parameters
    args = sys.argv.copy()
    args.insert(0, sys.executable)

    # Add parameter to run the remote server itself
    args.append('--internal_remote_server')

    # if we're running from renderdoccmd, invoke it properly
    if 'renderdoccmd' in sys.executable:
        # run_tests.py
        # --renderdoc
        # <renderdoc_path>
        # --pyrenderdoc
        # <pyrenderdoc_path>
        del args[1:6]
        args.insert(1, 'test')
        args.insert(2, 'functional')

    subprocess.Popen(args)
    return


def become_remote_server():
    rd.BecomeRemoteServer('localhost', 0, None, None)


def internal_run_test(test_name):
    testcases = get_tests()

    log.add_output(util.get_artifact_path("output.log.html"))

    for testclass in testcases:
        if testclass.__name__ == test_name:
            globalenv = rd.GlobalEnvironment()
            globalenv.enumerateGPUs = False
            rd.InitialiseReplay(globalenv, [])

            log.begin_test(test_name, print_header=False)

            util.set_current_test(test_name)

            try:
                instance = testclass()
                instance.invoketest(False)
                suceeded = True
            except Exception as ex:
                log.failure(ex)
                suceeded = False
                
            logfile = rd.GetLogFile()
            if os.path.exists(logfile):
                log.inline_file('RenderDoc log', logfile)

            log.end_test(test_name, print_footer=False)

            rd.ShutdownReplay()

            if suceeded:
                sys.exit(0)
            else:
                sys.exit(1)

    log.error("INTERNAL ERROR: Couldn't find '{}' test to run".format(test_name))
