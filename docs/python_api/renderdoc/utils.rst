Utilities
=========

.. contents::

.. currentmodule:: renderdoc

Maths
-----

.. autoclass:: FloatVector
  :members:

.. autofunction:: renderdoc.HalfToFloat
.. autofunction:: renderdoc.FloatToHalf

Logging & Versioning
--------------------

.. autofunction:: renderdoc.LogMessage
.. autofunction:: renderdoc.SetDebugLogFile
.. autofunction:: renderdoc.GetLogFile
.. autofunction:: renderdoc.GetCurrentProcessMemoryUsage
.. autofunction:: renderdoc.DumpObject

.. autoclass:: LogType
  :members:


Versioning
----------

.. autofunction:: renderdoc.GetVersionString
.. autofunction:: renderdoc.GetCommitHash
.. autofunction:: renderdoc.IsReleaseBuild

Settings
--------

.. autofunction:: renderdoc.GetConfigSetting
.. autofunction:: renderdoc.SetConfigSetting
.. autofunction:: renderdoc.SaveConfigSettings

Self-hosted captures
--------------------

.. autofunction:: renderdoc.StartSelfHostCapture
.. autofunction:: renderdoc.EndSelfHostCapture
