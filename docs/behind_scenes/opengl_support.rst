OpenGL & OpenGL ES Support
==========================

This page documents the support of OpenGL & OpenGL ES in RenderDoc. This gives an overview of what RenderDoc is capable of, and primarily lists information that is relevant. You might also be interested in the :doc:`full list of features <../getting_started/features>`.

The latest information and up-to-date support is always available on the `GitHub wiki <https://github.com/baldurk/renderdoc/wiki/OpenGL>`_.

Capture requirements
--------------------

RenderDoc only supports the core profile of OpenGL - from 3.2 up to 4.6 inclusive. This means any compatibility profile functionality will generally not be supported. There are a couple of concessions where it was easy to do so - like allowing the use of VAO 0, or luminance/intensity formats, but this in general will not happen.

.. note::

   To be more compatible with applications, RenderDoc will still attempt to capture on a compatibility context, but it will not replay successfully unless the given subset of functionality is used.

On OpenGL ES, any context version 2.0 and above is supported.

Replay requirements
-------------------

RenderDoc assumes a certain minimum feature set on replay. On desktop this means you must be able to create a 3.2 core context.

Also note that this is the *minimum* required functionality to replay, some analysis features will be disabled unless you have more capable hardware features such as GL_ARB_shader_image_load_store, GL_ARB_compute_shader and GL_ARB_gpu_shader5.

On OpenGL ES, you must be able to create a GLES 3 context to replay.

Multiple contexts & multithreading
----------------------------------

RenderDoc assumes that all GL commands (with the exception of perhaps a SwapBuffers call) for frames will come from a single thread. This means that e.g. if commands come from a second thread during loading, or some time during initialisation, this will be supported. However during frame capture all commands are serialised as if they come from a single thread, so interleaved rendering commands from multiple threads will not work.

Extension support
-----------------

RenderDoc supports many ARB, EXT and other vendor-agnostic extensions - primarily those that are either very widespread and commonly used but aren't in core, or are quite simple to support. In general RenderDoc won't support extensions unless they match one of these requirements, and this means most vendor extensions will not be supported.

OpenGL remaining work
---------------------

There are a couple of places where OpenGL is not yet at feature parity with other APIs.

* Shader debugging is not supported on any shader stage.
* Pixel history is not implemented.

Android
-------

OpenGL ES capture and replay on Android is natively supported. For more information on how to capture with Android see :doc:`../how/how_android_capture`.

OS X
----

OS X is not yet officially supported for OpenGL capture, however work is in progress on development builds.

See Also
--------

* :doc:`../getting_started/features`
