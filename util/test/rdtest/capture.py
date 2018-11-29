import os
import signal
import time
import renderdoc as rd
from . import util
from .logging import log


# Keep running until we get a capture
def run_until_capture(control):
    """Exits when the first capture is made"""
    return len(control.captures()) == 0


class TargetControl():
    def __init__(self, ident: int, host="localhost", username="testrunner", force=True, timeout=30, exit_kill=True):
        """
        Creates a target control manager for a given ident

        :param ident: The ident to connect to.
        :param host: The hostname.
        :param username: The username to use when connecting.
        :param force: Whether to force the connection.
        :param timeout: The timeout in seconds before aborting the run.
        :param exit_kill: Whether to kill the process when the control loop ends.
        """
        self._pid = 0
        self._captures = []
        self._children = []
        self.control = rd.CreateTargetControl(host, ident, username, force)
        self._timeout = timeout
        self._exit_kill = exit_kill

        if self.control is None:
            raise RuntimeError("Couldn't connect target control")

        self._pid = self.control.GetPID()

    def pid(self):
        """Return the PID of the connected application."""
        return self._pid

    def captures(self):
        """Return a list of renderdoc.NewCaptureData with captures made."""
        return self._captures

    def children(self):
        """Return a list of renderdoc.NewChildData with any child processes created."""
        return self._children

    def queue_capture(self, frame: int, num=1):
        """
        Queue a frame to make a capture of.

        :param frame: The frame number to capture.
        :param num: The number of frames
        """
        if self.control is not None:
            self.control.QueueCapture(frame, num)

    def run(self, keep_running=run_until_capture):
        """
        Runs a loop ticking the target control. The callback is called each time and
        can be used to determine if the loop should keep running. The default callback
        continues running until at least one capture has been made.

        Either way, if the target application closes and the target control connection
        is lost, the loop exits and the function returns.

        :param keep_running: A callback function to call each tick. Returns ``True`` if
          the loop should continue, or ``False`` otherwise.
        """
        if self.control is None:
            return

        start_time = time.time()

        while keep_running(self):
            msg: rd.TargetControlMessage = self.control.ReceiveMessage(None)

            if time.time() - start_time > self._timeout:
                log.error("Timed out")
                break

            # If we got a graceful or non-graceful shutdown, break out of the loop
            if (msg.type == rd.TargetControlMessageType.Disconnected or
                    not self.control.Connected()):
                break

            # If we got a new capture, add it to our list
            if msg.type == rd.TargetControlMessageType.NewCapture:
                self._captures.append(msg.newCapture)
                break

            # Similarly for a new child
            if msg.type == rd.TargetControlMessageType.NewChild:
                self._children.append(msg.newChild)
                break

        # Shut down the connection
        self.control.Shutdown()
        self.control = None

        # If we should make sure the application is killed when we exit, do that now
        if self._exit_kill:
            # Try 5 times to kill the application. This may fail if the application exited already
            for attempt in range(5):
                try:
                    os.kill(self._pid, signal.SIGTERM)
                    time.sleep(1)
                    return
                except Exception:
                    # Ignore errors killing the program
                    continue


def run_executable(exe: str, cmdline: str,
                   workdir="", envmods=None, cappath=None,
                   opts=rd.GetDefaultCaptureOptions()):
    """
    Runs an executable with RenderDoc injected, and returns the control ident.

    Throws a RuntimeError if the execution failed for any reason.

    :param exe: The executable to run.
    :param cmdline: The command line to pass.
    :param workdir: The working directory.
    :param envmods: Environment modifications to apply.
    :param cappath: The directory to output captures in.
    :param opts: An instance of renderdoc.CaptureOptions.
    :return:
    """
    if envmods is None:
        envmods = []
    if cappath is None:
        cappath = util.get_tmp_path('capture')

    wait_for_exit = False

    log.print("Running exe:'{}' cmd:'{}' in dir:'{}' with env:'{}'".format(exe, cmdline, workdir, envmods))

    # Execute the test program
    res = rd.ExecuteAndInject(exe, workdir, cmdline, envmods, cappath, opts, wait_for_exit)

    if res.status != rd.ReplayStatus.Succeeded:
        raise RuntimeError("Couldn't launch program: {}".format(str(res.status)))

    return res.ident


def run_and_capture(exe: str, cmdline: str, frame: int, capture_name=None, opts=rd.GetDefaultCaptureOptions()):
    """
    Helper function to run an executable with a command line, capture a particular frame, and exit.

    This will raise a RuntimeError if anything goes wrong, otherwise it will return the path of the
    capture that was generated.

    :param exe: The executable to run.
    :param cmdline: The command line to pass.
    :param frame: The frame to capture.
    :param capture_name: The name to use creating the captures
    :return: The path of the generated capture.
    :rtype: str
    """

    if capture_name is None:
        capture_name = 'capture'

    control = TargetControl(run_executable(exe, cmdline, cappath=util.get_tmp_path(capture_name), opts=opts))

    # Capture frame
    control.queue_capture(frame)

    # By default, runs until the first capture is made
    control.run()

    captures = control.captures()

    if len(captures) == 0:
        raise RuntimeError("No capture made")

    return captures[0].path