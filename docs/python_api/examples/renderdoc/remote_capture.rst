Remote Capture and Replay
=========================

This example is a bit different since it's not ready-to-run. It provides a template for how you can capture and replay on a remote machine, instead of the local machine. It also shows how to use device protocols to automatically manage devices.

First we can enumerate which device protocols are currently supported.

.. highlight:: python
.. code:: python

	protocols = rd.GetSupportedDeviceProtocols()

Each string in the list corresponds to a protocol that can be used for managing devices. If we're using one we can call :py:func:`~renderdoc.GetDeviceProtocolController` passing the protocol name and retrieve the controller.

The controller provides a few methods for managing devices. First we can call :py:meth:`~renderdoc.DeviceProtocolController.GetDevices` to return a list of device IDs. The format of these device IDs is protocol-dependent but will be equivalent to a normal hostname. Devices may have human-readable names obtainable via :py:meth:`~renderdoc.DeviceProtocolController.GetFriendlyName`.

.. highlight:: python
.. code:: python

  protocol = rd.GetDeviceProtocolController(protocol_to_use)

  devices = protocol.GetDevices()

  if len(devices) == 0:
      raise RuntimeError(f"no {protocol_to_use} devices connected")

  # Choose the first device
  dev = devices[0]
  name = protocol.GetFriendlyName(dev)

  print(f"Running test on {dev} - named {name}")

  URL = protocol.GetProtocolName() + "://" + dev

The URL will be used the same as we would use a hostname, when connecting for target control or remote servers.

Note that protocols may have additional restrictions - be sure to check :py:meth:`~renderdoc.DeviceProtocolController.IsSupported` to check if the device is expected to function at all, and :py:meth:`~renderdoc.DeviceProtocolController.SupportsMultiplePrograms` to see if it supports launching multiple programs. If multiple programs are not supported, you should ensure all running capturable programs are closed before launching a new one.

To begin with we create a remote server connection using :py:func:`~renderdoc.CreateRemoteServerConnection`. The URL is as constructed above for protocol-based connections, or a simple hostname/IP if we're connecting directly to remote machine.

If the connection fails, normally we must fail but if we have a device protocol available we can attempt to launch the remote server automatically using :py:meth:`~renderdoc.DeviceProtocolController.StartRemoteServer`.

.. highlight:: python
.. code:: python

  if result == rd.ResultCode.NetworkIOFailed and protocol is not None:
    # If there's just no I/O, most likely the server is not running. If we have
    # a protocol, we can try to start the remote server
    print("Couldn't connect to remote server, trying to start it")

    result = protocol.StartRemoteServer(URL)

    if result != rd.ResultCode.Succeeded:
      raise RuntimeError(f"Couldn't launch remote server, got error {str(result)}")

    # Try to connect again!
    result,remote = rd.CreateRemoteServerConnection(URL)

.. note::

   The remote server connection has a default timeout of 5 seconds. If the connection is unused for 5 seconds, the other side will disconnect and subsequent use of the interface will fail.

Once we have a remote server connection, we can browse the remote filesystem for the executable we want to launch using :py:meth:`~renderdoc.RemoteServer.GetHomeFolder` and :py:meth:`~renderdoc.RemoteServer.ListFolder`.

Then once we've selected the executable, we can launch the remote program for capturing with :py:meth:`~renderdoc.RemoteServer.ExecuteAndInject`. This function is almost identical to the local :py:func:`~renderdoc.ExecuteAndInject` except that it is not possible to wait for the program to exit.

In our sample, we now place the remote server connection on a background thread that will ping it each second to keep the connection alive while we use a target control connection to trigger a capture in the application.

.. highlight:: python
.. code:: python

  def ping_remote(remote, kill):
    success = True
    while success and not kill.is_set():
      success = remote.Ping()
      time.sleep(1)

  kill = threading.Event()
  ping_thread = threading.Thread(target=ping_remote, args=(remote,kill))
  ping_thread.start()

To connect to and control an application we use :py:func:`~renderdoc.CreateTargetControl` with the URL as before and the ident returned from :py:meth:`~renderdoc.RemoteServer.ExecuteAndInject`.

.. highlight:: python
.. code:: python

  target = rd.CreateTargetControl(URL, result.ident, 'remote_capture.py', True)

  # Here we wait for whichever condition you want
  target.TriggerCapture(1)

There are a couple of ways to trigger a capture, both :py:meth:`~renderdoc.TargetControl.TriggerCapture` and :py:meth:`~renderdoc.TargetControl.QueueCapture` depending on whether you want a time-based or frame-based trigger. The application itself can also use the in-application API to trigger a capture.

The target control connection can be intermittently polled for messages using :py:meth:`~renderdoc.TargetControl.ReceiveMessage`, which keeps the connection alive and will return any new information such as the data for a new capture that has been created. A message of type :py:data:`~renderdoc.TargetControlMessageType.NewCapture` indicates a new capture has been created, and :py:data:`~renderdoc.TargetControlMessage.newCapture` contains the information including the path.

.. highlight:: python
.. code:: python

  msg = target.ReceiveMessage(None)

  # Once msg.type == rd.TargetControlMessageType.NewCapture has been retrieved 

  cap_path = msg.newCapture.path
  cap_id = msg.newCapture.captureId

Once the capture has been found we are finished with the target control connection so we can shut it down and stop the background thread that was keeping the remote server connection alive. Using the remote server connection we can copy the capture back to the local machine with :py:meth:`~renderdoc.RemoteServer.CopyCaptureFromRemote`. Similarly if we wanted to load a previously made capture that wasn't on the remote machine :py:meth:`~renderdoc.RemoteServer.CopyCaptureToRemote` would be useful to copy it ready to be opened.

Finally to open the capture we use, and that returns a :py:class:`~renderdoc.ReplayController` which can be used as normal and will tunnel over the remote server connection. It can be useful to intermittently ping the remote server connection to check that it's still valid, and remote server and controller calls can be interleaved as long as they don't overlap on multiple threads.

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <remote_capture.py>`.

.. literalinclude:: remote_capture.py

