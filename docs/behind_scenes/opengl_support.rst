OpenGL Support
==============

This page documents the support of OpenGL in RenderDoc. This gives an overview of what RenderDoc is capable of, and primarily lists information that is relevant. You might also be interested in the :doc:`full list of features <../getting_started/features>`.

The latest information and up-to-date support is always available on the `GitHub wiki <https://github.com/baldurk/renderdoc/wiki/OpenGL>`_.

OpenGL requirements, and limitations
------------------------------------

RenderDoc only supports the core profile of OpenGL - from 3.2 up to 4.5 inclusive. This means any compatibility profile functionality will generally not be supported. There are a couple of concessions where it was easy to do so - like allowing the use of VAO 0, or luminance/intensity formats, but this in general will not happen. Note that to be more compatible with applications, RenderDoc will still attempt to capture on an older context, or on a compatibility context, but it will not replay successfully unless the given subset of functionality is used.

RenderDoc assumes your hardware/software configuration is able to create a core 4.3 context for replay, and also that ``EXT_direct_state_access`` and ``ARB_buffer_storage`` are available, both on replay and in capture.

Regarding multiple contexts and multithreading, RenderDoc assumes that all GL commands (with the exception of perhaps a SwapBuffers call) for frames will come from a single thread, and that all contexts are set up to share objects with each other. This means that e.g. if commands come from a second thread during loading, or some time during initialisation, this will be supported only if the second context shares with the primary context. During frame capture all commands are serialised as if they come from a single thread.

RenderDoc supports some ARB, EXT and other extensions - primarily those that are either very widespread and commonly used but aren't in core, or are quite simple to support. In general RenderDoc won't support extensions unless they match one of these requirements, and this means most vendor extensions will not be supported.

OpenGL remaining work
---------------------

There are several places where OpenGL is not yet at feature parity with D3D11.

* Full & complete support for multiple threads feeding GL simultaneously, or multiple contexts that don't share with each other (or only share within defined groups).
* Shader debugging is not supported on any shader stage.
* Pixel history is not implemented.


Linux and OS X
--------------

Linux and OS X support follows naturally when thinking about OpenGL support. Currently there is full support for capturing and replaying on linux, however the Qt UI is still heavily work in progress. The recommended method is to capture on linux, and then replay on windows. For more information on this see :doc:`../how/how_network_capture_replay`.

See Also
--------

* :doc:`../getting_started/features`
