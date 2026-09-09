from __future__ import annotations

import sys
import subprocess
from typing import Callable, List, Optional, Tuple
import renderdoc as rd
from . import util
from .logging import log
import os
import threading
from time import sleep
from abc import ABC, abstractmethod


class RemoteServer(ABC):
    def __init__(self) -> None:
        super().__init__()
        self.device = None
        self.remote: rd.RemoteServer | None = None

    @abstractmethod
    def init(self, in_process: bool):
        pass

    @abstractmethod
    def connect(self) -> rd.RemoteServer:
        pass

    @abstractmethod
    def disconnect(self):
        pass

    @abstractmethod
    def shutdown(self):
        pass

    @abstractmethod
    def is_connected(self) -> bool:
        pass

    @abstractmethod
    def get_temp_path(self, name="", timeout=20) -> str:
        pass

    @abstractmethod
    def get_renderdoc_path(self) -> str:
        pass

    @abstractmethod
    def path_exists(self, path: str, timeout=10) -> bool:
        pass

    @abstractmethod
    def run_demos(self, args: List[str], timeout=10) -> str:
        pass

    @abstractmethod
    def inject_and_run_exe(self, cmdline: str, envmods: List[rd.EnvironmentModification], opts: rd.CaptureOptions) -> rd.ExecuteResult:
        pass

    @abstractmethod
    def get_demos_exe(self) -> str:
        pass

    @abstractmethod
    def get_hostname(self) -> str:
        pass

    @abstractmethod
    def get_username(self) -> str:
        pass

    @abstractmethod
    def retrieve_latest_test_log(self, dst: str, timeout=10) -> str | None:
        pass

    @abstractmethod
    def retrieve_latest_server_log(self, dst: str, timeout=10) -> str | None:
        pass

    @abstractmethod
    def retrieve_comms_log(self, timeout=10) -> str | None:
        pass

    @abstractmethod
    def Ping(self) -> rd.ResultDetails:
        pass
        
    @abstractmethod
    def OpenCapture(self, proxyid: int, logfile: str, replayOptions: rd.ReplayOptions, progressCallback: Callable[[float], None] | None) -> Tuple[rd.ResultDetails, rd.ReplayController]:
        pass
    
    @abstractmethod
    def CloseCapture(self, controller: rd.ReplayController):
        pass

    @abstractmethod
    def ExecuteAndInject(self, app: str, workingDir: str, cmdLine: str, env: List[rd.EnvironmentModification], captureOptions: rd.CaptureOptions) -> rd.ExecuteResult:
        pass

    @abstractmethod
    def CopyCaptureFromRemote(self, src: str, dst: str, progress_callback: Callable[[float], None] | None):
        pass

class AndroidRemoteServer(RemoteServer):
    # Android app IDs for the server
    ADRD_SERVER_APP64 = 'org.renderdoc.renderdoccmd.arm64'
    CONNECTION_RETRY_COUNT = 3

    def __init__(self, device: Optional[str]):
        super().__init__()
        if device is None:
            raise RuntimeError('Android target specified, but no device given')
        self.device = device
        self.remote = None
        self._base_path = ''

    def init(self, in_process):        
        # Remove any existing Vulkan layers
        subprocess.run(['adb', '-s', self.device,
                        'shell', 'settings', 'delete', 'global', 'gpu_debug_layers'], check=False)

        remote = self.connect()
        log.print(f'Supported drivers: {remote.RemoteSupportedReplays()}')

        # Install the demo APK
        subprocess.run(['adb', '-s', self.device,
                        'install', '-g', util.get_demos_binary()], check=True)

        # Remove any stale logs and captures from previous runs
        rm_glob = self._base_path + '/*'
        subprocess.run(['adb', '-s', self.device, 'shell',
                       'rm', '-rf', rm_glob], check=True)

        # Close the connection if the tests are forked as each test will create their own
        # connection
        if not in_process:
            self.disconnect()

    def connect(self):
        log.print("Connecting to remote server...")

        protocols = rd.GetSupportedDeviceProtocols()
        if not 'adb' in protocols:
            log.print('ADB requested but not an available device protocol')
            sys.exit(1)

        protocol = rd.GetDeviceProtocolController('adb')
        if not self.device in protocol.GetDevices():
            log.print(f'ADB device {self.device} requested but not discovered')
            sys.exit(1)

        if not protocol.IsSupported(self.device):
            log.print(f'ADB device {self.device} is not supported')
            sys.exit(1)

        url = f'{protocol.GetProtocolName()}://{self.device}'
        result, remote = rd.CreateRemoteServerConnection(url)
        if result.code == rd.ResultCode.NetworkIOFailed and protocol is not None:
            log.print("Couldn't connect to remote server, trying to start it")

            result = protocol.StartRemoteServer(url)
            if not result:
                log.print(
                    f"Couldn't launch remote server, got error {str(result)}")
                sys.exit(1)

            # Try to connect again!
            result, remote = rd.CreateRemoteServerConnection(url)

        # Retry a few times
        if not result:
            for i in range(1, self.CONNECTION_RETRY_COUNT + 1):
                result, remote = rd.CreateRemoteServerConnection(url)
                if result:
                    break
                
                log.print(
                    f"Couldn't connect to remote server on attempt #{i}, got error {str(result)}")
                if i == self.CONNECTION_RETRY_COUNT:
                    sys.exit(1)

        # Calculate the remote base path
        base = '/sdcard/Android/'
        output = subprocess.run(['adb', '-s', self.device, 'shell', 'getprop', 'ro.build.version.sdk'],
                                stdout=subprocess.PIPE, timeout=10, check=True).stdout
        api_version = int(str(output, 'utf-8').strip())
        if api_version >= 30:
            base += 'media/'
        else:
            base += 'data/'

        self._data_path = base
        self._base_path = base + util.get_android_demo_app_name() + '/files/'
        self.remote = remote

        log.print("Connected!")
        
        # spawn a thread to keep connection alive
        self.mutex = threading.Lock()
        self.pingThread = threading.Thread(target=self.PingThread)
        self.pingThread.start()
        
        return remote

    def disconnect(self):
        self.remote.ShutdownConnection()
        self.remote = None
        
        # wait for thread completion
        self.pingThread.join()

    def shutdown(self):
        # If we running over ADB, close down the server.  This will involve first establishing a
        # connection if the tests have been running out-of-process
        log.print('Shutting down server...')

        # Kill the demo app first
        subprocess.run(['adb', '-s', self.device, 'shell', 'am', 'force-stop',
                        util.get_android_demo_app_name()])

        if self.remote is None:
            protocol = rd.GetDeviceProtocolController('adb')
            url = f'{protocol.GetProtocolName()}://{self.device}'
            result, self.remote = rd.CreateRemoteServerConnection(url)
            if not result:
                log.print(
                    f"Couldn't connect to remote server for shutdown, got error {str(result)}")
                return

        self.remote.ShutdownServerAndConnection()
        self.remote = None

    def is_connected(self):
        return self.remote is not None

    def get_temp_path(self, name="", timeout=20):
        subprocess.run(['adb', '-s', self.device, 'shell', 'mkdir', '-p',
                        self._base_path + '/' + util.get_current_test()],
                       timeout=timeout,
                       check=True)
        return self._base_path + util.get_current_test() + '/' + name

    def get_renderdoc_path(self):
        return self._data_path + '/' + AndroidRemoteServer.ADRD_SERVER_APP64 + '/files/RenderDoc/'

    def run_demos(self, args: List[str], timeout=10):
        raw = subprocess.run(['adb', '-s', self.device, 'shell', 'echo', '$EPOCHREALTIME'],
                             check=True, stdout=subprocess.PIPE, timeout=timeout).stdout
        ts = str(raw, 'utf-8').strip()
        # Run the command, blocking
        proc = subprocess.run(['adb', '-s', self.device,
                               'shell', 'am', 'start', '-W', '-n', f'{util.get_android_demo_app_name()}/.Loader',
                               '-e', 'demos', 'RenderDoc', '-e', 'rd_demos'] + args,
                              check=True, stdout=subprocess.DEVNULL)
        # Extract the log data
        raw = subprocess.run(['adb', '-s', self.device, 'shell',
                              'logcat', '-b', 'main', '-v', 'brief', '-T', ts, '-d', 'rd_demos:I', '*:S'],
                             check=True, stdout=subprocess.PIPE, timeout=timeout).stdout
        output = str(raw, 'utf-8')
        # Remove the per-line prefix
        output = output.splitlines()
        result = ""
        for line in output:
            pos = line.find('): ')
            if not line.startswith('I/rd_demos(') or pos == -1:
                continue

            result += line[pos+3:] + '\n'

        return result

    def path_exists(self, path, timeout=10):
        raw = subprocess.run(['adb', '-s', self.device, 'shell', f'ls {path} >> /dev/null'],
                             stderr=subprocess.PIPE, timeout=timeout).stderr
        raw = str(raw, 'utf-8').strip()

        return len(raw) == 0

    def inject_and_run_exe(self, cmdline, envmods, opts):
        package_and_activity = f"{util.get_android_demo_app_name()}/.Loader"
        args = "-e demos RenderDoc -e rd_demos \'\"" + cmdline + "\"\'"

        log.print(f"Running package:'{package_and_activity}' cmd:'{cmdline}' with env:'{envmods}'")
        res = util.get_remote_server().ExecuteAndInject(
            package_and_activity, "", args, envmods, opts)

        if not res.result:
            raise RuntimeError(
                "Couldn't launch program: {}".format(str(res.result)))

        return res

    def get_demos_exe(self):
        return util.get_android_demo_app_name()

    def get_hostname(self):
        return f"adb://{self.device}"

    def get_username(self):
        return "testrunner"

    def retrieve_latest_test_log(self, dst, timeout=10):
        if not self.is_connected():
            return None

        src = self._base_path + '/RenderDoc'
        raw = subprocess.run(['adb', '-s', self.device, 'shell', f'cd {src}; ls -t RenderDoc_* | head -1'],
                             check=True, stdout=subprocess.PIPE, timeout=timeout).stdout
        latestlog = str(raw, 'utf-8').strip()
        if not latestlog:
            log.print(f"Cannot find latest remote log from '{src}'")
            return None

        os.makedirs(dst, exist_ok=True)

        dst = os.path.join(dst, latestlog)
        src = os.path.join(src, latestlog)
        log.print(f"Copying remote test log from '{src}' to '{dst}'")
        self.remote.CopyCaptureFromRemote(src, dst, None)

        return dst

    def retrieve_latest_server_log(self, dst, timeout=10):
        if not self.is_connected():
            return None

        src = self.get_renderdoc_path()
        raw = subprocess.run(['adb', '-s', self.device, 'shell', f'cd {src}; ls -t RenderDoc_* | head -1'],
                             check=True, stdout=subprocess.PIPE, timeout=timeout).stdout
        latestlog = str(raw, 'utf-8').strip()
        if not latestlog:
            log.print(f"Cannot find latest remote log from '{src}'")
            return None

        os.makedirs(dst, exist_ok=True)

        dst = os.path.join(dst, latestlog)
        src = os.path.join(src, latestlog)
        log.print(f"Copying remote server log from '{src}' to '{dst}'")
        self.CopyCaptureFromRemote(src, dst, None)

        return dst

    def retrieve_comms_log(self, timeout=10):
        if not self.is_connected():
            return None

        src = self.get_renderdoc_path() + "RemoteServer_Server.log"
        if not self.path_exists(src):
            log.print(f"Cannot find server comms log '{src}'")
            return None

        os.makedirs(util.get_tmp_dir(), exist_ok=True)

        dst = os.path.join(util.get_tmp_dir(), 'RenderDoc_Server.log')
        log.print(f"Copying remote server comms log from '{src}' to '{dst}'")
        self.CopyCaptureFromRemote(src, dst, None)

        return dst
    
    def PingThread(self):
        while self.remote is not None:
            self.Ping()
            sleep(0.175)

    def Ping(self):
        with self.mutex:
            return self.remote.Ping()
        
    def OpenCapture(self, proxyid, logfile, replayOptions, progressCallback):
        with self.mutex:
            return self.remote.OpenCapture(proxyid, logfile, replayOptions, progressCallback)
   
    def CloseCapture(self, controller):
        with self.mutex:
            return self.remote.CloseCapture(controller)

    def ExecuteAndInject(self, app, workingDir, cmdLine, env, captureOptions):
        with self.mutex:
            return self.remote.ExecuteAndInject(app, workingDir, cmdLine, env, captureOptions) 

    def CopyCaptureFromRemote(self, src, dst, progress_callback):
        with self.mutex:
            return self.remote.CopyCaptureFromRemote(src, dst, progress_callback)

