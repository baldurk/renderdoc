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

Logging & Versioning 
--------------------

.. autofunction:: renderdoc.SetDebugLogFile
.. autofunction:: renderdoc.GetLogFile
.. autofunction:: renderdoc.GetVersionString

Maths & Utilities
-----------------

.. autofunction:: renderdoc.Maths_FloatToHalf
.. autofunction:: renderdoc.Maths_HalfToFloat
.. autofunction:: renderdoc.Topology_NumVerticesPerPrimitive
.. autofunction:: renderdoc.Topology_VertexOffset
.. autofunction:: renderdoc.PatchList_Count
.. autofunction:: renderdoc.PatchList_Topology
.. autofunction:: renderdoc.IsD3D
.. autofunction:: renderdoc.MaskForStage
.. autofunction:: renderdoc.StartSelfHostCapture
.. autofunction:: renderdoc.EndSelfHostCapture
