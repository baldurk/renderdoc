OpenGL & OpenGL ES Support
==========================

This page documents the support of OpenGL & OpenGL ES in RenderDoc. This gives an overview of what RenderDoc is capable of, and primarily lists information that is relevant. You might also be interested in the :doc:`full list of features <../getting_started/features>`.

The latest information and up-to-date support is always available on the `GitHub wiki <https://github.com/baldurk/renderdoc/wiki/OpenGL>`_.

Capture requirements
--------------------

RenderDoc only supports the core profile of OpenGL - from 3.2 up to 4.5 inclusive. This means any compatibility profile functionality will generally not be supported. There are a couple of concessions where it was easy to do so - like allowing the use of VAO 0, or luminance/intensity formats, but this in general will not happen.

.. note::

   that to be more compatible with applications, RenderDoc will still attempt to capture on a compatibility context, but it will not replay successfully unless the given subset of functionality is used.

On OpenGL ES, any context version 2.0 and above is supported.

Replay requirements
-------------------

RenderDoc assumes a certain minimum feature set on replay. On desktop this means you must be able to create a 3.2 context with the following extensions available:

* GL_ARB_vertex_attrib_binding
* GL_ARB_program_interface_query
* GL_ARB_shading_language_420pack
* GL_ARB_separate_shader_objects
* GL_ARB_explicit_attrib_location
* GL_ARB_sampler_objects

These extensions should not require newer hardware than the base 3.2 context, but they might need an updated driver to be listed as available. Also note that this is the *minimum* required extension set to replay, some analysis features will be disabled unless you have more capable hardware features such as GL_ARB_shader_image_load_store, GL_ARB_compute_shader and GL_ARB_gpu_shader5.

On OpenGL ES, you must be able to create a GLES 3 context to replay.

Multiple contexts & multithreading
----------------------------------

RenderDoc assumes that all GL commands (with the exception of perhaps a SwapBuffers call) for frames will come from a single thread, and that all contexts are set up to share objects with each other. This means that e.g. if commands come from a second thread during loading, or some time during initialisation, this will be supported only if the second context shares with the primary context. During frame capture all commands are serialised as if they come from a single thread.

Extension support
-----------------

RenderDoc supports many ARB, EXT and other vendor-agnostic extensions - primarily those that are either very widespread and commonly used but aren't in core, or are quite simple to support. In general RenderDoc won't support extensions unless they match one of these requirements, and this means most vendor extensions will not be supported.

OpenGL remaining work
---------------------

There are a couple of places where OpenGL is not yet at feature parity with other APIs.

* Full & complete support for multiple threads feeding GL simultaneously, or multiple contexts that don't share with each other (or only share within defined groups).
* Shader debugging is not supported on any shader stage.
* Pixel history is not implemented.

Android
-------

OpenGL ES capture and replay on Android is natively supported. For more information on how to capture with Android see :doc:`../how/how_android_capture`.

OS X
----

OS X is not yet supported for OpenGL capture, in part due to some of the required replay extensions above being missing, and also due to missing platform-specific hooking code.

See Also
--------

* :doc:`../getting_started/features`
