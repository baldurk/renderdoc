Functions
=========

.. contents::

.. module:: renderdoc

Initialisation and Shutdown
---------------------------

.. autofunction:: renderdoc.InitialiseReplay
.. autofunction:: renderdoc.ShutdownReplay

Capture File Access
-------------------

.. autofunction:: renderdoc.OpenCaptureFile

Target Control
--------------

.. autofunction:: renderdoc.EnumerateRemoteTargets
.. autofunction:: renderdoc.CreateTargetControl

Remote Servers
--------------

.. autofunction:: renderdoc.CreateRemoteServerConnection
.. autofunction:: renderdoc.CheckRemoteServerConnection
.. autofunction:: renderdoc.BecomeRemoteServer

Device Protocols
----------------

.. autofunction:: renderdoc.GetSupportedDeviceProtocols
.. autofunction:: renderdoc.GetDeviceProtocolController

Local Execution & Injection
---------------------------

.. autofunction:: renderdoc.GetDefaultCaptureOptions
.. autofunction:: renderdoc.ExecuteAndInject
.. autofunction:: renderdoc.InjectIntoProcess
.. autofunction:: renderdoc.StartGlobalHook
.. autofunction:: renderdoc.StopGlobalHook
.. autofunction:: renderdoc.IsGlobalHookActive
.. autofunction:: renderdoc.CanGlobalHook

Logging & Versioning
--------------------

.. autofunction:: renderdoc.SetDebugLogFile
.. autofunction:: renderdoc.GetLogFile
.. autofunction:: renderdoc.GetVersionString
.. autofunction:: renderdoc.GetCommitHash
.. autofunction:: renderdoc.GetDriverInformation
.. autofunction:: renderdoc.IsReleaseBuild

Settings & Configuration
------------------------

.. autofunction:: renderdoc.GetConfigSetting
.. autofunction:: renderdoc.SetConfigSetting

Maths & Utilities
-----------------

.. autofunction:: renderdoc.CreateHeadlessWindowingData
.. autofunction:: renderdoc.CreateWin32WindowingData
.. autofunction:: renderdoc.CreateXlibWindowingData
.. autofunction:: renderdoc.CreateXCBWindowingData
.. autofunction:: renderdoc.CreateWaylandWindowingData
.. autofunction:: renderdoc.CreateGgpWindowingData
.. autofunction:: renderdoc.CreateAndroidWindowingData
.. autofunction:: renderdoc.CreateMacOSWindowingData
.. autofunction:: renderdoc.InitCamera
.. autofunction:: renderdoc.HalfToFloat
.. autofunction:: renderdoc.FloatToHalf
.. autofunction:: renderdoc.NumVerticesPerPrimitive
.. autofunction:: renderdoc.VertexOffset
.. autofunction:: renderdoc.PatchList_Count
.. autofunction:: renderdoc.PatchList_Topology
.. autofunction:: renderdoc.SupportsRestart
.. autofunction:: renderdoc.IsStrip
.. autofunction:: renderdoc.IsD3D
.. autofunction:: renderdoc.MaskForStage
.. autofunction:: renderdoc.StartSelfHostCapture
.. autofunction:: renderdoc.EndSelfHostCapture
.. autofunction:: renderdoc.GetCurrentProcessMemoryUsage
.. autofunction:: renderdoc.VarTypeByteSize
.. autofunction:: renderdoc.VarTypeCompType
