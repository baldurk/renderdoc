Options Window
==============

.. _settings-window:

The options window contains various advanced or niche settings that configure the analysis UI.

Options
-------

Below each tab of the options window is detailed with explanations of the given settings.

Some settings may not be saved until the application is closed, although most will come into immediate effect.

The settings are contained in ``%APPDATA%\RenderDoc\UI.config`` if you wish to back up or reset these settings.

General options
---------------

  | ``Associate .rdc with RenderDoc`` Default: ``N/A``

This button will elevate RenderDoc to administrator privileges temporarily, and associate the ``.rdc`` file extension with RenderDoc. After doing this double clicking any ``.rdc`` file will open it in a new instance of RenderDoc.

---------------

  | ``Associate .cap with RenderDoc`` Default: ``N/A``

This button will elevate RenderDoc to administrator privileges temporarily, and associate the ``.cap`` file extension with RenderDoc. After doing this double clicking any ``.cap`` file will open a new instance of RenderDoc and show the capture dialog with the settings contained inside.

If the setting "Auto Start" is enabled, RenderDoc will then immediately trigger a capture of the target executable.

---------------

  | ``Minimum decimal places on float values`` Default: ``2``

Defines the smallest number of decimal places to display on a float, padding with 0s.

Examples:

* With a value of 2, ``0.1234f`` will be displayed as *0.1234*

* With a value of 2, ``1.0f`` will be displayed as *1.00*

* With a value of 6, ``0.1234f`` will be displayed as *0.123400*

* With a value of 6, ``1.0f`` will be displayed as *1.000000*

---------------

  | ``Maximum significant figures on decimals`` Default: ``5``

Defines the smallest number of decimal places to display on a float, padding with 0s.

Examples:

* With a value of 5, ``0.123456789f`` will be displayed as *0.12345*

* With a value of 5, ``1.0f`` will be displayed as *1.00*

* With a value of 10, ``0.123456789f`` will be displayed as *0.123456789*

* With a value of 10, ``1.0f`` will be displayed as *1.00*

---------------

  | ``Negative exponential cutoff value`` Default: ``5``

Any floating point numbers that are below *E-v* for this value *v* will be displayed in scientific notation rather than as a fixed point decimal.

Examples:

* With a value of 5, ``0.1234f`` will be displayed as *0.1234*

* With a value of 5, ``0.000001f`` will be displayed as *1.0E-6*

* With a value of 10, ``0.1234f`` will be displayed as *0.1234*

* With a value of 10, ``0.000001f`` will be displayed as *0.000001*

---------------

  | ``Positive exponential cutoff value`` Default: ``7``

Any floating point numbers that are larger *E+v* for this value *v* will be displayed in scientific notation rather than as a fixed point decimal.

Examples:

* With a value of 7, ``8000.5f`` will be displayed as *8005.5*

* With a value of 7, ``123456789.0f`` will be displayed as *1.2345E8*

* With a value of 2, ``8000.5f`` will be displayed as *8.0055E3*

* With a value of 2, ``123456789.0f`` will be displayed as *1.2345E8*

---------------

  | ``Directory for temporary capture files`` Default: ``%TEMP%``

This allows you to choose where on disk temporary capture files are stored between when the capture is made, and when it is either discarded or saved to a permanent location on disk. 

By default the system temporary directory is used, but if this lies on drive with limited space, large captures could become a problem so here you can redirect the storage elsewhere.
  
---------------

  | ``Default save directory for captures`` Default: ``Empty``

This allows you to choose which directory the save dialog will default to when prompting to save a capture file. By default the value is empty, which means that it follows the system behaviour (e.g. to begin on whichever folder was last browsed to in a file dialog).

The folder must exist, it will not be created when browsed to.

---------------

  | ``Allow global process hooking`` Default: ``Disabled``

This option enables the functionality allowing capturing of programs that aren't launched directly from RenderDoc, but are launched from somewhere else.

This option **can be dangerous** which is why you have to deliberately enable it here. Be careful when using this and only do so when necessary - more details can be found in the :ref:`global process hook <global-process-hook>` details.

---------------

  | ``Allow periodic anonymous update checks`` Default: ``Enabled``

Every couple of days RenderDoc will send a single web request to a server to see if a new version is available and let you know about it. The only information transmitted is the version of RenderDoc that is running.

If you would prefer RenderDoc does not ever contact an external server, disable this checkbox. If you do this it's recommended that you manually check for updates as new versions will be made available regularly with bugfixes.

---------------

  | ``Prefer monospaced fonts in UI`` Default: ``Disabled``

This option will use a monospaced font for every place in the UI where any data or output is displayed.

Changing this option will need the UI to be restarted before it takes effect.

---------------

  | ``Always replay logs locally`` Default: ``Disabled``

Normally, when RenderDoc begins to load a capture file that was created on a different type of machine, it will prompt you to ask if you really want to replay it locally (and perhaps get different results or even failed loading), or if you'd like to choose a :doc:`replay context <../how/how_network_capture_replay>` to replay it remotely on the type of machine it was recorded.

In that prompt you can choose to always replay logs locally, which enables this option. If enabled, RenderDoc will always just load the log locally.

Core options
------------

  | ``Shader debug search paths`` Default: ``Empty``

Here you can choose which locations to search in, and in which order, when looking up a relative path for unstripped debug info.

For more information you can consult :ref:`the FAQ entry about providing unstripped shader debug information <unstripped-shader-info>`.

Texture Viewer options
----------------------

  | ``Reset Range on changing selection`` Default: ``Disabled``

When changing texture from one to another, when this option is enabled the range control will reset itself to [0, 1]. This will happen between any two textures, and going back and forth between two textures will also reset the range.

---------------

  | ``Visible channels, mip/slice, and range saved per-texture`` Default: ``Enabled``

Settings including which channels are displayed (red, green, blue, alpha or depth/stencil), the mip or slice/cubemap face to display, or the visible min/max range values are remembered with the texture you were looking at. In other words if you display a render target with only the alpha channel visible, then switching to view another texture will default back to RGB - and switching back to that render target will view alpha again.

Shader Viewer options
---------------------

  | ``Rename disassembly registers`` Default: ``Enabled``

This option tries to make the disassembly of shaders easier to read by substituting variable names where available in for constant register names.

Event Browser options
---------------------

  | ``Time unit used for event browser timings`` Default: ``Microseconds``

This option allows you to select the unit that will be shown in the duration column in the event browser when you time individual drawcalls.

Seconds through to nanoseconds are supported.

---------------

  | ``Hide empty marker sections`` Default: ``Disabled``

Marker sections that contain no API calls or drawcalls will be completely removed. This also applies to the Timeline Bar.

This option only applies itself the next time you load a log.


---------------

  | ``Hide marker sections with only non-draw API calls`` Default: ``Disabled``

Marker sections that contain only miscellaneous non-draw API calls like queries or state setting will be completely removed. This also applies to the Timeline Bar.

This can be useful if you have markers around occlusion queries or where you have a minor state change, and you don't want them cluttering up the capture.

This option only applies itself the next time you load a log.


---------------

  | ``Apply marker colours`` Default: ``Enabled``

Some APIs can provide an RGBA colour alongside the marker name when setting or pushing a marker region. This option enables applying those colours in the UI. Usually you'd leave it on unless your code is passing garbage for the colours or something instead of 0s (which will then be ignored rather than coming out black).

This option only applies itself the next time you load a log.


---------------

  | ``Colourise whole row for marker regions`` Default: ``Enabled``

If the above option to apply colours is enabled, this will colourise the whole row in the event browser for any marker regions with colours, rather than just applying a strip of colour along the side of their children.

This option only applies itself the next time you load a log.

