Replay Control
==============

.. contents::

.. currentmodule:: renderdoc

Initialisation and Shutdown
---------------------------

.. autofunction:: renderdoc.InitialiseReplay
.. autofunction:: renderdoc.ShutdownReplay

.. autoclass:: renderdoc.GlobalEnvironment
  :members:

.. autoclass:: renderdoc.ResultCode
  :members:

.. autoclass:: renderdoc.ResultDetails
  :members:

Capture File Access
-------------------

.. autofunction:: renderdoc.OpenCaptureFile

.. autoclass:: renderdoc.CaptureAccess
  :members:

.. autoclass:: renderdoc.CaptureFile
  :members:

.. autoclass:: renderdoc.ReplaySupport
  :members:

.. autoclass:: renderdoc.CaptureFileFormat
  :members:

.. autoclass:: renderdoc.SectionProperties
  :members:

.. autoclass:: renderdoc.SectionType
  :members:

.. autoclass:: renderdoc.SectionFlags
  :members:

.. autoclass:: renderdoc.Thumbnail
  :members:

GPU Enumeration
---------------

.. autoclass:: renderdoc.GPUDevice
  :members:

.. autoclass:: renderdoc.GPUVendor
  :members:

.. autofunction:: renderdoc.GPUVendorFromPCIVendor

.. autoclass:: renderdoc.GraphicsAPI
  :members:

.. autofunction:: renderdoc.IsD3D

.. autofunction:: renderdoc.GetDriverInformation

.. autoclass:: renderdoc.DriverInformation
  :members:

Replay Controller
-----------------

.. autoclass:: renderdoc.ReplayController
  :members:

.. autoclass:: renderdoc.ReplayOptions
  :members:

.. autoclass:: renderdoc.ReplayOptimisationLevel
  :members:

.. autoclass:: renderdoc.APIProperties
  :members:

Device Protocols
----------------

.. autoclass:: renderdoc.DeviceProtocolController
  :members:

.. autofunction:: renderdoc.GetSupportedDeviceProtocols
.. autofunction:: renderdoc.GetDeviceProtocolController

Remote Servers
--------------

.. autoclass:: renderdoc.RemoteServer
  :members:

.. autofunction:: renderdoc.CreateRemoteServerConnection
.. autofunction:: renderdoc.CheckRemoteServerConnection
.. autofunction:: renderdoc.BecomeRemoteServer

.. autoclass:: renderdoc.PathEntry
  :members:

.. autoclass:: renderdoc.PathProperty
  :members:
