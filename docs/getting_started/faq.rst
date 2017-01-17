FAQ
===

Here is a list of commonly asked questions about RenderDoc. Feel free to `contact me <mailto:baldurk@baldurk.org?subject=RenderDoc%20question>`__ if you have another question that isn't covered here or in this document.

How do I do some particular task?
---------------------------------

Many specific tasks or functions are detailed in the "How Do I... ?" sections. These sections each take a feature or element of a workflow and explain how it fits into the program as a whole as well as any details of how it works.

If the task you have in mind isn't listed there you might find something similar, or you might find a related feature which can be used to do what you want. If the feature you want doesn't seem to exist at all you might want to check the :doc:`../behind_scenes/planned_features` to see if it's coming soon - if it's not on that list please feel free to `contact me <mailto:baldurk@baldurk.org?subject=RenderDoc%20request>`__ and request it! It has often been that simple features are very quick to implement and the prioritisation and scheduling of features is fairly fluid at this stage in development.

Why did you make RenderDoc?
---------------------------

Although several tools do already exist for graphics debugging, none of them quite suited the functionality I desired and I would often find myself wishing for a feature of one in another and vice versa.

In addition to this, although the functionality overlaps to some degree many of these tools were primarily designed around the profiling of applications rather than debugging. While being able to inspect the state and contents of resources does often suffice for debugging, it's not necessarily the ideal workflow and often it can become cumbersome.

In principle I didn't see any reason why I couldn't write a home-brew graphics debugger with some fairly simple operating principles. While there were a whole lot of caveats and little stumbling blocks along the way, the original design has pretty much stayed consistent since the project was started back in July 2012. If you're interested you might want to read about :doc:`../behind_scenes/how_works`.

Where did the name come from?
-----------------------------

All of the good names were taken :-(.

Who can I contact about bugs, feature requests, other queries?
--------------------------------------------------------------

At the moment there's just me at the wheel - feel free to `contact me <mailto:baldurk@baldurk.org?subject=RenderDoc%20feedback>`__ if you have anything you'd like to ask or suggest. I use a `GitHub repository <https://github.com/baldurk/renderdoc>`_ for tracking bugs and feature requests so that's the best place to file an issue.

I work on RenderDoc in my spare time but I am happy to help with anything and work with you if you have any issues that need attention.

How can I associate RenderDoc's file extensions with the program?
-----------------------------------------------------------------

If you installed RenderDoc via the installer package rather than the zip folder, the option is available there to associate RenderDoc's file extensions with the program. Otherwise you can set them up from the :doc:`../window/options_window`.

.. note::

  RenderDoc will elevate itself to set up these file associations, but otherwise will not hold on to administrator permissions.

RenderDoc associates .rdc and .cap with itself when desired. The .rdc files are the logfiles output when you capture inside an application. .cap files describe the set up of a particular capture, and can be used to quickly re-launch a capture preset.

If .rdc files are associated with RenderDoc a thumbnail handler is set up, so that in explorer you'll get thumbnail previews for your captures.

.. note::

    Note that if you move the directory that RenderDoc is you will need to re-configure the file associations as the registry entries contain absolute paths.

What APIs does RenderDoc support?
---------------------------------

Currently RenderDoc supports Vulkan, D3D11 (including D3D11.x), D3D12, and core profile OpenGL. Note OpenGL is a complex sprawling API, so see the details of what is supported in :doc:`its own page <../behind_scenes/opengl_support>`. Vulkan support has :doc:`a few notes <../behind_scenes/vulkan_support>`, as does :doc:`D3D12 <../behind_scenes/d3d12_support>`.

Future API support is at this point not clear, GLES, Metal and perhaps D3D9 all being possible. Higher priority is better operating system/platform support which is currently underway, as well as feature improvements for existing platforms and APIs.

How can I backup or restore my settings?
----------------------------------------

Everything RenderDoc relies upon is stored in ``%APPDATA%\RenderDoc``. You can back up and restore this directory at will as nothing stored in there is machine specific aside from things like recent file lists.

Deleting this folder will also reset RenderDoc to the defaults - if you uninstall RenderDoc this folder will not be deleted.

RenderDoc doesn't install any registry keys aside from those to set up file associations.

Which network ports does RenderDoc use?
---------------------------------------

RenderDoc uses TCP and UDP ports ``38920``-``38927`` consecutively for remote access and control (ie. capturing remotely) for each new program that is opened on a machine. Note that even if you initiate a capture locally these ports are still opened for listening. These are the ports that are probed on a remote host to see if a connection exists.

RenderDoc also uses TCP and UDP ports ``39920`` for remote replay connections, for when a remote host is used to replay and analyse the log.

Where can I get the source to RenderDoc?
----------------------------------------

RenderDoc is licensed under the MIT license and the source is available on `GitHub <https://github.com/baldurk/renderdoc>`_.

What are the requirements for RenderDoc?
----------------------------------------

Currently RenderDoc expects Feature Level 11.0 hardware and above for D3D11. Lower levels will capture successfully, but on replay RenderDoc will fall back to WARP software emulation which will run quite slowly.

For OpenGL RenderDoc will only capture core profile applications, in general, and expects at minimum to be able to create a core 3.2 context which includes a few key extensions. For more details see :doc:`../behind_scenes/opengl_support`.

With Vulkan, RenderDoc should fully support any Vulkan application. However replaying a Vulkan log may not work if the hardware used to capture it is different - portability of captures between hardware is not guaranteed.

Why does my capture say "Failed to capture frame: Uncapped command list"?
-------------------------------------------------------------------------

At the moment on some APIs like D3D9, RenderDoc only begins capturing deferred command lists at the point that you trigger a capture. If you replay command lists that were recorded before the captured frame, RenderDoc will fail to capture the frame and try again next frame (and eventually give up after a few retries).

To change this behaviour, enable the ``Capture all cmd lists`` option - see :doc:`../window/capture_log_attach` for more details. This will capture all command lists recorded from the start of the program, ready for when you decide to capture a frame. This currently has a fair amount of overhead.

Why does my capture say "Failed to capture frame: Uncapped Map()/Unmap()"?
--------------------------------------------------------------------------

If you start a ``Map()`` before a ``Present()`` call then call ``Unmap()`` after the ``Present()`` during the frame RenderDoc wants to capture, RenderDoc won't have intercepted this call and so will fail to capture this frame and try again next time. This usually only invalidates the first frame you try to capture, but if you ``Map()`` many resources, and ``Unmap()`` them one by one in subsequent frames, you could hit this failed capture scenario many times in a row.

Currently the only solution to this is to change the pattern of ``Map()``/``Unmap()`` such that they are contained within a frame.

.. _gamma-linear-display:

Gamma display of linear data, or "Why doesn't my texture look right?"
---------------------------------------------------------------------

Gamma/sRGB correctness is a rather painful subject. If we could all just agree to store everything in 32bit float data we could probably do away with it. Until that time we have to worry about displaying textures while making sure to respect sRGB.

For texture formats that explicitly specify that they contain sRGB data this isn't a problem and everything works smoothly. Note that RenderDoc shows picked texel values in linear float format, so if you pick a pixel that is 0.5, 0.5, 0.5, the actual bytes might be stored as say 186, 186, 186.

For other textures it's more difficult - for starters they may actually contain sRGB data but the correction is handled by shaders so there's no markup. Or indeed the app may not be gamma-correct so the data is sRGB but uncorrected. If we display these textures in a technically correct way, such that the data is not over or under gamma-corrected, the result often looks 'wrong' or unintuitively different from expected.

Nothing is actually wrong here except perhaps that when visualising linear data it is often more convenient to "overcorrect" such that the data is perceptually linear. A good example to use is a normal map: The classic deep blue of (127,127,255) flat normals is technically incorrect as everyone is used to visualising these textures in programs that display the data as if it were sRGB (which is the convention for normal images that do not represent vectors).

You can override this behaviour on any texture that isn't listed as explicitly sRGB with the gamma (γ) button - toggle this off and the overcorrection will be disabled.

RenderDoc makes my bug go away! Or causes new artifacts that weren't there
--------------------------------------------------------------------------

For various tedious reasons RenderDoc's replay isn't (and in most cases can't be) a perfect reproduction of what your code was executing in the application when captured, and it can change the circumstances while running.

During capture the main impact of having RenderDoc enabled is that timings will change, and more memory (sometimes much more) will be allocated. There are also slight differences to the interception of Map() calls as they go through an intermediate buffer to be captured. Generally the only problem this can expose is that when capturing a frame, if something is timing dependent RenderDoc causes one or two very slow frames, and can cause the bug to disappear.

The two primary causes of differences between the captured program and the replayed log (for better or for worse) are:

#. ``Map()`` s that use DISCARD are filled with a marker value, so any values that aren't written to the buffer will be different - in application you can get lucky and they can be previous values that were uploaded, but in replay they will be ``0xCCCCCCCC``.

#. RenderDoc as an optimisation will not save or restore the contents of render targets at the start of the frame if it believes they will be entirely overwritten in the frame. This detection is typically accurate but means targets are cleared to black or full depth rather than accumulating, even if that accumulation is not intentional it may be the cause of the bug.

  This behaviour can be overridden by enabling 'Save all initials' in the :doc:`capture options <../how/how_capture_log>`.

I can't launch my program for capture directly. Can I capture it anyway?
------------------------------------------------------------------------

There is an option for capturing programs using RenderDoc where you can't easily set up a direct launch of the process.

More details can be found in the :ref:`capture options page <global-process-hook>` which details how to use it, however you should take care to read the warnings! The global process hooking option isn't without its risks, so you need to be sure you know what you're doing before using it. It should always be used as a last resort when there is no other option.

.. _view-image-files:

I'd like to use RenderDoc's texture viewer for dds files, or other images. Can I?
---------------------------------------------------------------------------------

Yes you can!

Simply drag in an image file, or open it via file → open. RenderDoc will open the image if it is supported, and display it as if there were a log open with only one texture.

RenderDoc supports these formats: ``.dds``, ``.hdr``, ``.exr``, ``.bmp``, ``.jpg``, ``.png``, ``.tga``, ``.gif``, ``.psd``. For ``.dds`` files RenderDoc supports all DXGI formats, compressed formats, arrays and mips - all of which will display as expected.

Any modifications to the image while open in RenderDoc will be refreshed in the viewer. However if the image metadata changes (dimension, format, etc) then this will likely cause artifacts or incorrect rendering, and you'll have to re-open the image.

I think I might be overwriting Map() boundaries, can I check this?
------------------------------------------------------------------

Yes RenderDoc can be configured to insert a boundary marker at the end of the memory returned from a ``Map()`` call. If this marker gets overwritten during a captured frame then a message box will pop up alerting you, and clicking Yes will break into the program in the debugger so that you can investigate the callstack.

To enable this behaviour, select the ``Verify Map() Writes`` option when :doc:`capturing <../window/capture_log_attach>`.

Note this is only supported on D3D11 and OpenGL currently, since Vulkan and D3D12 are lower overhead and do not have the infrastructure to intercept map writes.

RenderDoc is complaining about my OpenGL app in the overlay - what gives?
-------------------------------------------------------------------------

The first thing to remember is that **RenderDoc only supports Core 3.2 and above OpenGL**. If your app is using features from before 3.2 it almost certainly won't work as most functionality is not supported. A couple of things like not creating a VAO (which are required in core profile) and luminance textures (which don't exist in core profile) are allowed, but none of the fixed function pipeline will work, etc etc.

If your app is not using the ``CreateContextAttribs`` API then RenderDoc will completely refuse to capture, and will display overlay text to this effect using the simplest fixed-function pipeline code, so it will run on any OpenGL app, even on a 1.4 context or similar.

If your app did use the ``CreateContextAttribs`` API, RenderDoc will allow you to capture, but compatibility profiles will have a warning displayed in the overlay - this is because you could easily use old functionality as it is all still available in the context.

Can I tell via the graphics APIs if RenderDoc is present at runtime?
--------------------------------------------------------------------

Yes indeed. Some APIs offer ways to do this already - ``D3DPERF_GetStatus()``, ``ID3DUserDefinedAnnotation::GetStatus()`` and ``ID3D11DeviceContext2::IsAnnotationEnabled()``.

In addition to those:

Querying an ``ID3D11Device`` for UUID ``{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}`` will return an ``IUnknown*`` and ``S_OK`` when RenderDoc is present.

`GL_EXT_debug_tool <https://renderdoc.org/debug_tool.txt>`_ is implemented on RenderDoc, which is an extension I've proposed for this purpose (identifying when and which tool is injected in your program). It allows you to query for the presence name and type of a debug tool that's currently hooked. At the time of writing only RenderDoc implements this as I've only just proposed the extension publicly, but in future you can use the queries described in that spec.

.. note::

    It's unlikely the extension will ever be 'made official', so these enumerants can be used:

    .. highlight:: c++
    .. code:: c++

        #define GL_DEBUG_TOOL_EXT                 0x6789
        #define GL_DEBUG_TOOL_NAME_EXT            0x678A
        #define GL_DEBUG_TOOL_PURPOSE_EXT         0x678B

A similar extension for Vulkan will be proposed after release.

.. _unstripped-shader-info:

My shaders have 'cbuffer0' and unnamed variables, how do I get proper debug info?
---------------------------------------------------------------------------------

If you get textures that are just named ``texture0`` and ``texture1`` or constant/uniform buffers named ``cbuffer2`` then this indicates that you have stripped optional reflection/debug information out of your shaders.

This optional information is generated by the compiler, but is not required for API correctness so some codebases will strip the information out after processing it offline, and so it will not be available for RenderDoc to fetch.

The simplest solution is just to avoid stripping the data when using RenderDoc, but that isn't always possible. Instead RenderDoc allows you to use API-specific methods to specify where the unstripped data can be found. This means you can save the unstripped shader to a debug location and then either store this location with the shader, or specify it at runtime. On replay RenderDoc will expect the data to be available at that location and it will load it up instead.

The path you specify (with the stripped shader, or at runtime) can be either absolute or relative. If it's relative, you must configure a shader search path in the :doc:`../window/options_window`.

The stripped shader file stored on disk can also be compressed with LZ4 to save space as often most of the size is made up for shader source text which compresses well. To do this, simply compress the contents of the file and prepend the pathname (either absolute or relative, specified in the shader blob or at runtime) with ``lz4#``.

For example code using this method, check out :doc:`tips_tricks`.

I want to debug a process that my program launches itself, how can I inject RenderDoc?
--------------------------------------------------------------------------------------

When launching a process in RenderDoc, by default only this process is debugged and any children it launches are not affected. This better ensures compatibility for the most common case where you are able to start the process to be debugged directly.

In the case where your program launches sub-processes that you would like to debug, you can enable the ``Hook into Children`` capture option, which causes RenderDoc to recursively inject itself into all children (and grand-children, and so on). When you open a capture connection, the child processes will be displayed and you can open a connection to each child to locate the process you wish to debug.

There are :ref:`more details available <child-process-hook>` in the documentation for the :doc:`../window/capture_log_attach` window.
