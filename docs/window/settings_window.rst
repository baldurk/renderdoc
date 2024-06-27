Settings Window
===============

.. _settings-window:

The settings window contains various advanced or niche settings that configure the analysis UI.

Some settings may not be saved until the application is closed, although most will come into immediate effect.

The settings are saved in the application-specific settings directory (On windows that's ``%APPDATA%\qrenderdoc\UI.config`` and on linux that's a path like ``~/.local/share/qrenderdoc``) if you wish to back up or reset these settings.

General options
---------------

  | :guilabel:`Visual theme of the UI` Default: ``Light`` or ``Dark`` depending on native color theme.

This allows you to choose what theme to use in RenderDoc. By default when first starting either the light or dark native RenderDoc theme will be selected, but you can change that or switch to any other supported theme here.

.. note::

  The theme won't fully apply until the next restart, although most elements will update immediately to give you a preview.

---------------

  | :guilabel:`Global font scale` Default: ``100%``

This option will apply a global font scale to all fonts in the UI. This will only apply to text elements, and other UI elements will take their scale from the system's DPI scaling configuration.

Changing this option will need the UI to be restarted before it fully takes effect on all UI elements.

---------------

  | :guilabel:`Prefer monospaced fonts in UI` Default: ``Disabled``

This option will use a monospaced font for every place in the UI where any data or output is displayed.

Changing this option will need the UI to be restarted before it fully takes effect on all UI elements.

---------------

  | :guilabel:`Minimum decimal places on float values` Default: ``2``

Defines the smallest number of decimal places to display on a float, padding with 0s.

Examples:

* With a value of 2, ``0.1234`` will be displayed as *0.1234*

* With a value of 2, ``1.0`` will be displayed as *1.00*

* With a value of 6, ``0.1234`` will be displayed as *0.123400*

* With a value of 6, ``1.0`` will be displayed as *1.000000*

---------------

  | :guilabel:`Maximum decimal places on float values` Default: ``5``

Defines the largest number of decimal places to display on a float.

Examples:

* With a value of 5, ``0.123456789`` will be displayed as *0.12346*

* With a value of 5, ``1.0`` will be displayed as *1.00* depending on the above minimum

* With a value of 10, ``0.123456789`` will be displayed as *0.123456789*

* With a value of 10, ``1.0`` will be displayed as *1.00* depending on the above minimum

---------------

  | :guilabel:`Negative exponential cutoff value` Default: ``5``

Any floating point numbers that are below *E-v* for this value *v* will be displayed in scientific notation rather than as a fixed point decimal.

Examples:

* With a value of 5, ``0.1234`` will be displayed as *0.1234*

* With a value of 5, ``0.000001`` will be displayed as *1.0E-6*

* With a value of 10, ``0.1234`` will be displayed as *0.1234*

* With a value of 10, ``0.000001`` will be displayed as *0.000001*

---------------

  | :guilabel:`Positive exponential cutoff value` Default: ``7``

Any floating point numbers that are larger *E+v* for this value *v* will be displayed in scientific notation rather than as a fixed point decimal.

Examples:

* With a value of 7, ``8000.5`` will be displayed as *8005.5*

* With a value of 7, ``123456789.0`` will be displayed as *1.2345E8*

* With a value of 2, ``8000.5`` will be displayed as *8.0055E3*

* With a value of 2, ``123456789.0`` will be displayed as *1.2345E8*

---------------

  | :guilabel:`Offset or size fields format mode` Default: ``Auto``

Any fields which are displayed in the UI that represent byte offsets or sizes can be configured to display as either decimal, hexadecimal, or automatic.

In the default automatic mode the fields will be shown as decimal for small values up to a given threshold, and as hexadecimal for larger values. With this option they can be instead forced to always display as one or the other.

---------------

  | :guilabel:`Directory for temporary capture files` Default: ``%TEMP%``

This allows you to choose where on disk temporary capture files are stored between when the capture is made, and when it is either discarded or saved to a permanent location on disk.

By default the system temporary directory is used, but if this lies on drive with limited space, large captures could become a problem so here you can redirect the storage elsewhere.

---------------

  | :guilabel:`Default save directory for captures` Default: ``Empty``

This allows you to choose which directory the save dialog will default to when prompting to save a capture file. By default the value is empty, which means that it follows the system behaviour (e.g. to begin on whichever folder was last browsed to in a file dialog).

The folder must exist, it will not be created when browsed to.

---------------

  | :guilabel:`Allow global process hooking` Default: ``Disabled``

This option enables the functionality allowing capturing of programs that aren't launched directly from RenderDoc, but are launched from somewhere else.

This option **can be dangerous** which is why you have to deliberately enable it here. Be careful when using this and only do so when necessary - more details can be found in the :ref:`global process hook <global-process-hook>` details.

---------------

  | :guilabel:`Enable process injection (restart required)` Default: ``Disabled``

On windows only RenderDoc is able to inject into running processes. By default this is disabled since it is almost never the right thing to do and can easily break, so you are strongly recommended to instead launch your program from RenderDoc's launch process panel.

Injecting into processes can be unreliable and should only be used as a last resort when no other methods succeed, it should not be used as a primary method of launching applications.

---------------

  | :guilabel:`Allow periodic anonymous update checks` Default: ``Enabled``

Every couple of days RenderDoc will send a single web request to a secure server to see if a new version is available and let you know about it. The only information transmitted is the version of RenderDoc that is running.

If you would prefer RenderDoc does not ever contact an external server, disable this checkbox. If you do this it's recommended that you manually check for updates as new versions will be made available regularly with bugfixes.

---------------

  | :guilabel:`Always replay captures locally` Default: ``Disabled``

Normally, when RenderDoc begins to load a capture file that was created on a different type of machine, it will prompt you to ask if you really want to replay it locally (and perhaps get different results or even failed loading), or if you'd like to choose a different :doc:`replay context <../how/how_network_capture_replay>` to replay it remotely on the type of machine it was recorded.

In that prompt you can choose to always replay captures locally, which enables this option. If enabled, RenderDoc will always just load the capture locally.

---------------

  | :guilabel:`Anonymous Analytics`

When you first run a build of RenderDoc that's analytics-enabled, RenderDoc will prompt you for your preference.

You have three alternatives:

* *Gather anonymous low-detail statistics and submit automatically*. This will gather analytics in the background and submit the anonymous report automatically each month to RenderDoc's secure server.
* *Gather anonymous low-detail statistics, but manually verify before submitting*. This will gather analytics in the background but prompt the user each month to verify the contents of the report before submitting the anonymous report to RenderDoc's secure server.
* *Do not gather or submit any statistics*. This will disable all statistics gathering completely.

This option allows you to change modes at any time, although note that if you previously had statistics disabled the program must be restarted to enable gathering.

The complete details of the analytics can be found in the page about :doc:`../behind_scenes/analytics`.

Core options
------------

  | :guilabel:`Ignored DLLs for callstack symbol resolution` Default: ``Empty``

Here you can see a list of DLLs (on windows only) which have been permanently ignored for callstack symbol resolution. You can remove any or all of the items on the list and you will be prompted to locate them again the next time symbols are resolved which needs them.

For more information you can consult :doc:`../how/how_capture_callstack`.

  | :guilabel:`Shader debug search paths` Default: ``Empty``

Here you can choose which locations to search in, and in which order, when looking up a relative path for unstripped debug info.

For more information you can consult :doc:`../how/how_shader_debug_info`.

  | :guilabel:`Enable Radeon GPU Profiler integration` Default: ``Off``

Here you can choose to enable the RGP integration which is by default disabled.

For more information you can see :doc:`../how/how_rgp_profile`.

  | :guilabel:`Radeon GPU Profiler executable` Default: ``Empty``

Here you can choose where ``RadeonGPUProfiler`` executable is, for use with the RGP integration.

For more information you can see :doc:`../how/how_rgp_profile`.

Replay options
--------------

In this panel you can configure the default options used for replaying captures.

The specific options are documented along with the explanation of how replay options affect the capture in :doc:`../how/how_control_replay`.

Texture Viewer options
----------------------

  | :guilabel:`Reset Range on changing selection` Default: ``Disabled``

When changing texture from one to another, when this option is enabled the range control will reset itself to [0, 1]. This will happen between any two textures, and going back and forth between two textures will also reset the range.

---------------

  | :guilabel:`Visible channels, mip/slice, and range saved per-texture` Default: ``Enabled``

Settings including which channels are displayed (red, green, blue, alpha or depth/stencil), the mip or slice/cubemap face to display, or the visible min/max range values are remembered with the texture you were looking at. In other words if you display a render target with only the alpha channel visible, then switching to view another texture will default back to RGB - and switching back to that render target will view alpha again.

---------------

  | :guilabel:`Y-flipping state saved per-texture` Default: ``Disabled``

If the above setting is enabled, then also store the y-flip per texture. By default this is treated as a global toggle for all textures. With this setting enabled the flip will default to off for all textures, and then be saved per-texture.

  | :guilabel:`Custom shader directories` Default: ``Empty``

Here you can choose additional locations to search for custom visualisation shaders, and in which order in case of duplicates.

For more information see :doc:`../how/how_custom_visualisation`.

Shader Viewer options
---------------------

  | :guilabel:`Rename disassembly registers` Default: ``Enabled``

This option tries to make the disassembly of shaders easier to read by substituting variable names where available in for constant register names.

---------------

  | :guilabel:`Shader Processing Tools`

.. _shader-processing-tools-config:

Here you can configure external tools that convert between shader representations, including compilers from a high-level language like HLSL/GLSL to a bytecode, as well as disassemblers from bytecode back to high-level language.

Some built-in tools are supported such as SPIRV-Cross, glslang, spirv-dis and spirv-as. For these tools if they can be auto-detected they will already be present, and they may be distributed with RenderDoc builds in case a version isn't installed on the system.

Other custom tools can be configured, but for those the command line arguments must be configured. The command line arguments will have certain substitutions made to customise to the needs of the compile:

* ``{input_file}`` will be replaced by the input filename.
* ``{output_file}`` will be replaced by the output filename.
* ``{entry_point}`` will be replaced by the entry point name, only when compiling a shader.
* ``{glsl_stage4}`` will be replaced by the glsl stage short-hand, one of: vert, tesc, tese, geom, frag, or comp.
* ``{hlsl_stage2}`` will be replaced by the hlsl stage short-hand, one of: vs, hs, ds, gs, ps, or cs.
* ``{spirv_ver}`` will be replaced by the SPIR-V version in use, e.g. spirv1.2 or spirv1.6.
* ``{vulkan_ver}`` will be replaced by the Vulkan-identified SPIR-V version in use, e.g. vulkan1.0 or vulkan1.3. This value may be lossy, and will pick the next *lowest* version that compiles with a given SPIR-V version. E.g. SPIR-V 1.2 was not used by a vulkan version, so will be rounded down to vulkan1.0.

You must also select the input and output format of the tool, such as HLSL input and SPIR-V output. This will be used to match the tool against a given need at runtime with different types of shaders.

Custom parameters can also be added each time the tool is invoked on the shader editing panel. For full information about using shader processing tools to decompile or compile shaders, see :ref:`the section on editing shaders <shader-processing-tools>`.

Event Browser options
---------------------

  | :guilabel:`Time unit used for event browser timings` Default: ``Microseconds``

This option allows you to select the unit that will be shown in the duration column in the event browser when you time individual actions.

Seconds through to nanoseconds are supported.

---------------

  | :guilabel:`Add fake markers if none present` Default: ``Enable``

If a capture is found to contain no markers whatsoever, RenderDoc will generate some default markers based on grouping actions by the different output targets that they are drawing to. Roughly forming 'passes' of different types.

You can disable this option here if you want to view a pure list of actions with no annotations.

This option only applies itself the next time you load a capture.


---------------

  | :guilabel:`Apply marker colors` Default: ``Enabled``

Some APIs can provide an RGBA color alongside the marker name when setting or pushing a marker region. This option enables applying those colors in the UI. Usually you'd leave it on unless your code is passing garbage for the colors or something instead of 0s (which will then be ignored rather than coming out black).

This option only applies itself the next time you load a capture.


---------------

  | :guilabel:`Colorise whole row for marker regions` Default: ``Enabled``

If the above option to apply colors is enabled, this will colorise the whole row in the event browser for any marker regions with colors, rather than just applying a strip of color along the side of their children.

This option only applies itself the next time you load a capture.

Comments options
----------------

  | :guilabel:`Show capture commends on load` Default: ``Enabled``

If a capture is newly loaded (i.e. it is not in the recent captures list having been loaded before) and it contains a comments section, then the capture comments panel will be displayed and brought to the front to show the comments on load.

Newly created captures will not have any comments, they are only added through the UI, so this only applies to captures made somewhere else that have had comments added to them.

For more information, see :doc:`../how/how_annotate_capture`.

Android options
---------------

  | :guilabel:`Android SDK root path` Default: ``Empty``

RenderDoc requires some android tools from the android SDK to be able to function. In most cases it's able to locate the tools automatically without any configuration needed, but if not this option allows you to manually locate the SDK root.

By default it will try to auto-locate those tools by looking in different environment variables like ``ANDROID_HOME`` and ``ANDROID_SDK``, or else searching the default executable path. If it fails completely it will try to use the tools bundled with RenderDoc's installation.

This setting, if present, will override all other search paths and be looked in first.

---------------

  | :guilabel:`Java JDK root path` Default: ``Empty``

RenderDoc may require tools from the Java JDK in some rare circumstances. In most cases it's able to locate the tools automatically without any configuration needed, but if not this option allows you to manually locate the JDK root.

By default it will try to auto-locate the tools by looking in ``JAVA_HOME`` or else searching the default executable path.

This setting, if present, will override all other search paths and be looked in first.

---------------

  | :guilabel:`Max Connection Timeout` Default: ``30 seconds``

Some Android programs take a long time to start up before they begin rendering. This setting allows you to define a timeout before RenderDoc will consider the execution and connection to have failed.

This only applies to running Android programs.
