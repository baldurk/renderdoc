FAQ
===

Here is a list of commonly asked questions about RenderDoc. Feel free to `contact me <mailto:baldurk@baldurk.org?subject=RenderDoc%20question>`_ if you have another question that isn't covered here or in this document.

What are the details of RenderDoc's Anonymous Analytics?
--------------------------------------------------------

RenderDoc has some very light anonymous analytics to allow analysis of which features and platforms are used more, to prioritise and guide future development.

The complete details of the analytics can be found in the page about :doc:`../behind_scenes/analytics`, but the brief outline is that RenderDoc records data **only in the replay program** and does not record any data that is specific to any captured programs. The data recorded is primarily boolean flags indicating whether or not a given feature, API, or platform is used or not. You can see the precise list of data gathered on your current RenderDoc build in the settings menu under the :guilabel:`Anonymous Analytics` section.

The analytics data is summarised and transmitted securely and anonymously to RenderDoc's server. The aggregated statistics are available for anyone to see at `the analytics homepage <https://renderdoc.org/analytics>`_.

Enabling the analytics is greatly appreciated, if you have any concerns about the data gathered you can choose to manually verify each report before it's submitted.

How do I do some particular task?
---------------------------------

Many specific tasks or functions are detailed in the :doc:`"How Do I... ?" <../how/index>` sections. These sections each take a feature or element of a workflow and explain how it fits into the program as a whole as well as any details of how it works.

If the task you have in mind isn't listed there you might find something similar, or you might find a related feature which can be used to do what you want. If you have a workflow which isn't supported at all, feel free to open an issue on github to request a new feature. Make sure to describe clearly what you are trying to do and what workflow you want to support, not just the specific feature you want. That way the problem can be better understood.

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

I work on RenderDoc full time contracting for Valve Software, and I am happy to help with anything and work with you if you have any issues that need attention.

In particular I'm used to working with people who have strong NDA protection over their projects - as long as you are able to spend time to diagnose the issue by running builds and debugging by suggestion, it's not a requirement to send me a repro case - which may be impossible.

How can I associate RenderDoc's file extensions with the program?
-----------------------------------------------------------------

On Windows if you installed RenderDoc via the msi installer, the option is available there to associate RenderDoc's file extensions with the program.

On linux the binary tarball comes with files to place under ``/usr/share`` to associate RenderDoc with files. This obviously also requires ``qrenderdoc`` to be available in your ``PATH``.

RenderDoc can be associated with ``.rdc`` and ``.cap`` files. The ``.rdc`` files are the frame capture containers generated from your application. ``.cap`` files describe the set up of a particular capture, and can be used to quickly re-launch a capture preset.

There is also a thumbnail handler available for ``.rdc`` so that while browsing through files you will get a thumbnail preview of the capture where available.

What APIs does RenderDoc support?
---------------------------------

Currently RenderDoc supports Vulkan 1.3, D3D11 (up to D3D11.4), D3D12, OpenGL 3.2+, and OpenGL ES 2.0 - 3.2. Note that OpenGL (and similarly OpenGL ES) is a complex & sprawling API, so see the details of what is supported in :doc:`its own page <../behind_scenes/opengl_support>`. In particular on desktop only modern GL is supported - legacy GL that is only available via the compatibility profile in OpenGL 3.2 is not supported.

Vulkan support has :doc:`a few notes <../behind_scenes/vulkan_support>`, as does :doc:`D3D12 <../behind_scenes/d3d12_support>`.

Future API support is at this point not clear; Metal, WebGL, and perhaps D3D9/D3D10 all being possible. Support for new APIs will be balanced against all other work such as features for existing APIs, bugfixes. So if you care strongly about support for a new API make sure to `file an issue on GitHub <https://github.com/baldurk/renderdoc/issues>`_ or comment on an existing issue to register your interest.

How can I backup or restore my settings?
----------------------------------------

RenderDoc stores data in two folders:

The UI stores data in a ``qrenderdoc`` folder underneath your OS's user settings folder. On windows this is ``%APPDATA%`` and on linux this will be somewhere in your home directory, perhaps in ``~/.local/share``. Nothing in this data is machine specific aside from paths, so you can back up and restore this directory at will.

Deleting this folder will also reset the RenderDoc UI to the defaults - if you uninstall RenderDoc this folder will not be deleted.

The core code may save cached data in a ``renderdoc`` folder - either ``%APPDATA%/renderdoc`` or ``~/.renderdoc/`` but this doesn't contain settings, so is not important to back up.

RenderDoc doesn't install any registry keys on windows aside from those required to set up file associations.

Which network ports does RenderDoc use?
---------------------------------------

RenderDoc uses TCP and UDP ports ``38920-38927`` consecutively for remote access and control (i.e. capturing remotely) for each new program that is opened on a machine. Note that even if you initiate a capture locally these ports are still opened for listening. These are the ports that are probed on a remote host to see if a connection exists.

RenderDoc also uses TCP and UDP ports ``39920`` for remote replay connections, for when a remote host is used to replay and analyse the capture.

Where can I get the source to RenderDoc?
----------------------------------------

RenderDoc is licensed under the MIT license and the source is available on `GitHub <https://github.com/baldurk/renderdoc>`_.

What are the requirements for RenderDoc?
----------------------------------------

Currently RenderDoc expects Feature Level 11.0 hardware and above for D3D11 and D3D12. Lower levels will capture successfully, but on replay RenderDoc will fall back to WARP software emulation which will run quite slowly.

For OpenGL RenderDoc will only capture core profile applications, in general, and expects at minimum to be able to create a core 3.2 context which includes a few key extensions. For more details see :doc:`../behind_scenes/opengl_support`.

With Vulkan, RenderDoc should fully support any Vulkan application. However replaying a Vulkan capture may not work if the hardware used to capture it is different - portability of captures between hardware is not guaranteed.

Why does my capture say "Failed to capture frame: Uncapped command list"?
-------------------------------------------------------------------------

On D3D11, RenderDoc only begins capturing deferred command lists at the point that you trigger a capture. If you replay command lists that were recorded before the captured frame, RenderDoc will fail to capture the frame and try again next frame (and eventually give up after a few retries).

To change this behaviour, enable the ``Capture all cmd lists`` option - see :doc:`../window/capture_attach` for more details. This will capture all command lists recorded from the start of the program, ready for when you decide to capture a frame. This currently has a fair amount of overhead.

Why does my capture say "Failed to capture frame: Uncapped Map()/Unmap()"?
--------------------------------------------------------------------------

If you start a ``Map()`` before a ``Present()`` call then call ``Unmap()`` after the ``Present()`` during the frame RenderDoc wants to capture, RenderDoc won't have intercepted this call and so will fail to capture this frame and try again next time. This usually only invalidates the first frame you try to capture, but if you ``Map()`` many resources, and ``Unmap()`` them one by one in subsequent frames, you could hit this failed capture scenario many times in a row.

Currently the only solution to this is to change the pattern of ``Map()/Unmap()`` such that they are contained within a frame.

.. _gamma-linear-display:

Gamma display of linear data, or "Why doesn't my texture look right?"
---------------------------------------------------------------------

Gamma/sRGB correctness is a rather painful subject. If we could all just agree to store everything in 32bit float data we could probably do away with it. Until that time we have to worry about displaying textures while making sure to respect the color space it's stored in.

For texture formats that explicitly specify that they contain sRGB data this isn't a problem and everything works smoothly. Note that RenderDoc shows picked texel values in linear float format, so if you pick a pixel that is 0.5, 0.5, 0.5, the actual bytes might be stored as say 186, 186, 186.

For other textures it's more difficult - for starters they may actually contain sRGB data but the correction is handled by shaders so there's no markup. Or indeed the application may not be gamma-correct so the data is sRGB but uncorrected. If we display these textures in a technically correct way, such that the data is not over or under gamma-corrected, the result often looks 'wrong' or unintuitively different from expected.

Nothing is actually wrong here except perhaps that when visualising linear data it is often more convenient to "over-correct" such that the data is perceptually linear. A good example to use is a normal map: The classic deep blue of (127,127,255) flat normals is technically incorrect as everyone is used to visualising these textures in programs that display the data as if it were sRGB (which is the convention for normal images that do not represent vectors).

You can override this behaviour on any texture that isn't listed as explicitly sRGB with the gamma (γ) button - toggle this off and the over-correction will be disabled.

RenderDoc makes my bug go away! Or causes new artifacts that weren't there
--------------------------------------------------------------------------

For various tedious reasons RenderDoc's replay isn't (and in most cases can't be) a perfect reproduction of what your code was executing in the application when captured, and it can change the circumstances while running.

During capture the main impact of having RenderDoc enabled is that timings will change, and more memory (sometimes much more) will be allocated. There are also slight differences to the interception of Map() calls as they may go through an intermediate buffer to be captured. Generally the only problem this can expose is that when capturing a frame, if something is timing dependent RenderDoc causes one or two very slow frames, and can cause the bug to disappear.

RenderDoc also isn't intended to handle invalid API use - this is better caught by each API's built-in validation features. If your program is using the API in an invalid way it may break RenderDoc in the same way that it may break a driver.


I can't launch my program for capture directly. Can I capture it anyway?
------------------------------------------------------------------------

There is an option for capturing programs using RenderDoc where you can't easily set up a direct launch of the process.

More details can be found in the :ref:`capture options page <global-process-hook>` which details how to use it, however you should take care to read the warnings! The global process hooking option isn't without its risks, so you need to be sure you know what you're doing before using it. It should always be used as a last resort when there is no other option.

.. _view-image-files:

I'd like to use RenderDoc's texture viewer for dds files, or other images. Can I?
---------------------------------------------------------------------------------

Yes you can!

Simply drag in an image file, or open it via file → open. RenderDoc will open the image if it is supported, and display it as if there were a capture open with only one texture.

RenderDoc supports these formats: ``.dds``, ``.hdr``, ``.exr``, ``.bmp``, ``.jpg``, ``.png``, ``.tga``, ``.gif``, ``.psd``. For ``.dds`` files RenderDoc supports all DXGI formats, compressed formats, arrays and mips - all of which will display as expected.

Any modifications to the image while open in RenderDoc will be refreshed in the viewer. However if the image metadata changes (dimension, format, etc) then this will likely cause artifacts or incorrect rendering, and you'll have to re-open the image.

I think I might be overwriting Map() boundaries or relying on undefined buffer contents, can I check this?
----------------------------------------------------------------------------------------------------------

RenderDoc can be configured to insert a boundary marker at the end of the memory returned from a ``Map()`` call. If this marker gets overwritten during a captured frame then a message box will pop up alerting you, and clicking Yes will break into the program in the debugger so that you can investigate the callstack.

It will also fill buffers with undefined contents on creation with a marker value, to help catch the use of undefined contents that may be assumed to be zero.

To enable this behaviour, select the ``Verify Buffer Access`` option when :doc:`capturing <../window/capture_attach>`.

Note this is only supported on D3D11 and OpenGL currently, since Vulkan and D3D12 are lower overhead and do not have the infrastructure to intercept map writes.

RenderDoc is complaining about my OpenGL app in the overlay - what gives?
-------------------------------------------------------------------------

The first thing to remember is that **RenderDoc only supports Core Profile 3.2 and above OpenGL**. If your app is using deprecated compatibility profile features from before 3.2 it almost certainly won't work as most functionality is not supported. A couple of things like not creating a VAO (which is required in core profile) and using luminance textures (which don't exist in core profile) are allowed, but none of the fixed function pipeline will work, etc.

If your app is not using the ``CreateContextAttribs`` API then RenderDoc will assume your program uses legacy functionality and it will completely refuse to capture. The overlay will display text to this effect using the simplest fixed-function pipeline code, so it will run on any OpenGL app, even on a 1.4 context or similar.

If your app did use the ``CreateContextAttribs`` API, RenderDoc will allow you to capture, but compatibility profiles will have a warning displayed in the overlay - this is because you could easily use old functionality which is still available in the context.

Can I tell via the graphics APIs if RenderDoc is present at runtime?
--------------------------------------------------------------------

Yes indeed. Some APIs offer ways to do this already - ``D3DPERF_GetStatus()``, ``ID3DUserDefinedAnnotation::GetStatus()`` and ``ID3D11DeviceContext2::IsAnnotationEnabled()``.

In addition to those the simplest way is to see if the RenderDoc module is loaded, using ``GetModuleHandleA("renderdoc.dll") != NULL`` or ``dlopen("librenderdoc.so, RTLD_NOW | RTLD_NOLOAD) != NULL``. There are also API specific ways to query:

Querying an ``ID3D11Device`` or ``ID3D12Device`` for UUID ``{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}`` will return an ``IUnknown*`` and ``S_OK`` when RenderDoc is present.

`GL_EXT_debug_tool <https://renderdoc.org/debug_tool.txt>`_ is implemented on RenderDoc, which is an extension I've proposed for this purpose (identifying when and which tool is injected in your program). It allows you to query for the presence name and type of a debug tool that's currently hooked. At the time of writing only RenderDoc implements this as I've only just proposed the extension publicly, but in future you can use the queries described in that spec.

.. note::

    It's unlikely the extension will ever be 'made official', so these enumerants can be used:

    .. highlight:: c++
    .. code:: c++

        #define GL_DEBUG_TOOL_EXT                 0x6789
        #define GL_DEBUG_TOOL_NAME_EXT            0x678A
        #define GL_DEBUG_TOOL_PURPOSE_EXT         0x678B

On Vulkan `VK_EXT_tooling_info` will return an entry for RenderDoc. This extension will always be available when running under RenderDoc.

My shaders have 'cbuffer0' and unnamed variables, how do I get proper debug info?
---------------------------------------------------------------------------------

If you get textures that are just named ``texture0`` and ``texture1`` or constant/uniform buffers named ``cbuffer2`` then this indicates that you have stripped optional reflection/debug information out of your shaders.

This optional information is generated by the compiler, but is not required for API correctness so some codebases will strip the information out after processing it offline, and so it will not be available for RenderDoc to fetch. This also includes shader source for source-level shader debugging on APIs that support it.

The simplest solution is just to avoid stripping the data when using RenderDoc. If you can enable debugging information to be embedded when building your shaders that's all that is necessary. All APIs will allow this data in the shader blob, but the way it is generated varies by API and shader compiler.

It's not always simple to avoid that stripping, and in some cases it may not be desirable as debug information can be quite a storage and memory overhead, it may instead be better to separate out the debug information and store it in a disk cache, with only a path or identifier remaining in the stripped blob so that RenderDoc can identify the matching debug information later.

For more information on how to do this, see :doc:`../how/how_shader_debug_info`.

I want to debug a child process that my program launches, how can I inject RenderDoc?
-------------------------------------------------------------------------------------

When launching a process in RenderDoc, by default only this process is debugged and any children it launches are not affected. This better ensures compatibility for the most common case where you are able to start the process to be debugged directly.

In the case where your program launches sub-processes that you would like to debug, you can enable the ``Capture Child Processes`` capture option, which causes RenderDoc to recursively inject itself into all children (and grand-children, and so on). When you open a capture connection, the child processes will be displayed and you can open a connection to each child to locate the process you wish to debug.

There are :ref:`more details available <child-process-hook>` in the documentation for the :doc:`../window/capture_attach` window.

I'm debugging a program using an OpenGL ES emulator, how can I capture the underlying API?
------------------------------------------------------------------------------------------

Wherever possible on Windows RenderDoc will capture OpenGL ES natively and ignore any underlying API calls it makes. For libraries such as ANGLE that emulate GLES on windows using calls to D3D11, this means the GLES itself gets captured and debugged.

If you don't want this to happen and you'd prefer OpenGL ES to be ignored during capture you can set the ``RENDERDOC_HOOK_EGL`` environment variable to ``0``.

.. note::

    This toggle only has effect on windows. On other platforms GLES is always natively captured as it is expected to have system-level support rather than being emulated.

When I launch my application through RenderDoc, why can't I access the API validation?
--------------------------------------------------------------------------------------

API validation layers are controlled by RenderDoc when it's active. This means that enabling the API validation from your application will have no effect, it will be controlled by the ``Enable API Validation`` capture option. For more information see :ref:`capture-options`.

.. note::

    On D3D11 if you know what you are doing you can access the underlying ``ID3D11InfoQueue`` using the separate UUID ``{3FC4E618-3F70-452A-8B8F-A73ACCB58E3D}``. Be aware that accessing this interface is done at your own risk and may break the RenderDoc capture. If you need to use the API validation directly it's recommended that you do so without RenderDoc active.

If I have multiple GPUs available, which one is used by RenderDoc? Can I change that?
-------------------------------------------------------------------------------------

By default RenderDoc will try to use the closest matching GPU to the one used on capture, which is controlled by the application. For example if a system has both Nvidia and AMD GPUs, then if the capture was made on an AMD GPU then the AMD GPU will be used on replay.

If a compatible GPU cannot be found - e.g. if the capture was made on an Intel GPU then the default - then the system default will be used.

This selection process can be overridden using :ref:`the GPU selection replay option <gpu-selection-override>` on a per-capture or global basis.

.. _what-is-an-action:
.. _what-is-a-drawcall:

What is an Action?
------------------

RenderDoc uses 'action' as an umbrella term to cover events like draws, dispatches, copies, clears, resolves, and other calls that cause the GPU to do work or can affect memory and resources like textures and buffers. This is sometimes referred to as a drawcall, but the term action is used to be less ambiguous compared to actual rasterization drawing.

This means that when browsing in the event browser by default only actions will be shown, meaning you can only see the list of actions and user-defined markers.
