Functions
=========

.. contents::

.. module:: renderdoc

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
.. autofunction:: renderdoc.GetDefaultRemoteServerPort
.. autofunction:: renderdoc.BecomeRemoteServer

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

Maths & Utilities
-----------------

.. autofunction:: renderdoc.CreateWin32WindowingData
.. autofunction:: renderdoc.CreateXlibWindowingData
.. autofunction:: renderdoc.CreateXCBWindowingData
.. autofunction:: renderdoc.CreateAndroidWindowingData
.. autofunction:: renderdoc.InitCamera
.. autofunction:: renderdoc.HalfToFloat
.. autofunction:: renderdoc.FloatToHalf
.. autofunction:: renderdoc.NumVerticesPerPrimitive
.. autofunction:: renderdoc.VertexOffset
.. autofunction:: renderdoc.PatchList_Count
.. autofunction:: renderdoc.PatchList_Topology
.. autofunction:: renderdoc.IsStrip
.. autofunction:: renderdoc.IsD3D
.. autofunction:: renderdoc.MaskForStage
.. autofunction:: renderdoc.StartSelfHostCapture
.. autofunction:: renderdoc.EndSelfHostCapture
