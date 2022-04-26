import renderdoc as rd
import threading
import time

# This sample is intended as an example of how to do remote capture and replay
# as well as using device protocols to automatically enumerate remote targets.
#
# It is not complete since it requires filling in with custom logic to select
# the executable and trigger the capture at the desired time
raise RuntimeError("This sample should not be run directly, read the source")

rd.InitialiseReplay(rd.GlobalEnvironment(), [])

protocols = rd.GetSupportedDeviceProtocols()

print(f"Supported device protocols: {protocols}")

# Protocols are optional - they allow automatic detection and management of
# devices.
if protocol_to_use is not None:
    # the protocol must be supported
    if protocol_to_use not in protocols:
        raise RuntimeError(f"{protocol_to_use} protocol not supported")

    protocol = rd.GetDeviceProtocolController(protocol_to_use)

    devices = protocol.GetDevices()

    if len(devices) == 0:
        raise RuntimeError(f"no {protocol_to_use} devices connected")

    # Choose the first device
    dev = devices[0]
    name = protocol.GetFriendlyName(dev)

    print(f"Running test on {dev} - named {name}")

    URL = protocol.GetProtocolName() + "://" + dev

    # Protocols can enumerate devices which are not supported. Capture/replay
    # is not guaranteed to work on these devices
    if not protocol.IsSupported(URL):
        raise RuntimeError(f"{dev} doesn't support capture/replay - too old?")

    # Protocol devices may be single-use and not support multiple captured programs
    # If so, trying to execute a program for capture is an error
    if not protocol.SupportsMultiplePrograms(URL):
        # check to see if anything is running. Just use the URL
        ident = rd.EnumerateRemoteTargets(URL, 0)

        if ident != 0:
            raise RuntimeError(f"{name} already has a program running on {ident}")
else:
    # If you're not using a protocol then the URL can simply be a hostname.
    # The remote server must be running already - how that is done is up
    # to you. Everything else will work the same over a normal TCP connection
    protocol = None
    URL = hostname

# Let's try to connect
result,remote = rd.CreateRemoteServerConnection(URL)

if result == rd.ResultCode.NetworkIOFailed and protocol is not None:
    # If there's just no I/O, most likely the server is not running. If we have
    # a protocol, we can try to start the remote server
    print("Couldn't connect to remote server, trying to start it")

    result = protocol.StartRemoteServer(URL)

    if result != rd.ResultCode.Succeeded:
        raise RuntimeError(f"Couldn't launch remote server, got error {str(result)}")

    # Try to connect again!
    result,remote = rd.CreateRemoteServerConnection(URL)

if result != rd.ResultCode.Succeeded:
    raise RuntimeError(f"Couldn't connect to remote server, got error {str(result)}")

# We now have a remote connection. This works regardless of whether it's a device
# with a protocol or not. In fact we are done with the protocol at this point
protocol = None

print("Got connection to remote server")

# GetHomeFolder() gives you a good default path to start with.
# ListFolder() lists the contents of a folder and can recursively
# browse the remote filesystem.
home = remote.GetHomeFolder()
paths = remote.ListFolder(home)

print(f"Executables in home folder '{home}':")

for p in paths:
    print("  - " + p.filename)

# Select your executable, perhaps hardcoded or browsing using the above
# functions
exe,workingDir,cmdLine,env,opts = select_executable()

print(f"Running {exe}")

result = remote.ExecuteAndInject(exe, workingDir, cmdLine, env, opts)

if result.result != rd.ResultCode.Succeeded:
    remote.ShutdownServerAndConnection()
    raise RuntimeError(f"Couldn't launch {exe}, got error {str(result.result)}")

# Spin up a thread to keep the remote server connection alive while we make a capture,
# as it will time out after 5 seconds of inactivity
def ping_remote(remote, kill):
    success = True
    while success and not kill.is_set():
        success = remote.Ping()
        time.sleep(1)

kill = threading.Event()
ping_thread = threading.Thread(target=ping_remote, args=(remote,kill))
ping_thread.start()

# Create target control connection
target = rd.CreateTargetControl(URL, result.ident, 'remote_capture.py', True)

if target is None:
    kill.set()
    ping_thread.join()
    remote.ShutdownServerAndConnection()
    raise RuntimeError(f"Couldn't connect to target control for {exe}")

print("Connected - waiting for desired capture")

# Wait for the capture condition we want
capture_condition()

print("Triggering capture")

target.TriggerCapture(1)

# Pump messages, keep waiting until we get a capture message. Time out after 30 seconds
msg = None
start = time.clock()
while msg is None or msg.type != rd.TargetControlMessageType.NewCapture:
    msg = target.ReceiveMessage(None)

    if time.clock() - start > 30:
        break

# Close the target connection, we're done either way
target.Shutdown()
target = None

# Stop the background ping thread
kill.set()
ping_thread.join()

# If we didn't get a capture, error now
if msg.type != rd.TargetControlMessageType.NewCapture:
    remote.ShutdownServerAndConnection()
    raise RuntimeError("Didn't get new capture notification after triggering capture")

cap_path = msg.newCapture.path
cap_id = msg.newCapture.captureId

print(f"Got new capture at {cap_path} which is frame {msg.newCapture.frameNumber} with {msg.newCapture.api}")

# We could save the capture locally
# remote.CopyCaptureFromRemote(cap_path, local_path, None)


# Open a replay. It's recommended to set no proxy preference, but you could
# call remote.LocalProxies and choose an index.
#
# The path must be remote - if the capture isn't freshly created then you need
# to copy it with remote.CopyCaptureToRemote()
result,controller = remote.OpenCapture(rd.RemoteServer.NoPreference, cap_path, rd.ReplayOptions(), None)

if result != rd.ResultCode.Succeeded:
    remote.ShutdownServerAndConnection()
    raise RuntimeError(f"Couldn't open {cap_path}, got error {str(result)}")

# We can now use replay as normal.
#
# The replay is tunnelled over the remote connection, so you don't have to keep
# pinging the remote connection while using the controller. Use of the remote
# connection and controller can be interleaved though you should only access
# them from one thread at once. If they are both unused for 5 seconds though,
# the timeout will happen, so if the controller is idle it's advisable to ping
# the remote connection

sampleCode(controller)

print("Shutting down")

controller.Shutdown()

# We can still use remote here - e.g. capture again, replay something else,
# save the capture, etc

remote.ShutdownServerAndConnection()

rd.ShutdownReplay()
