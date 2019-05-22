How do I edit a shader?
=======================


.. |page_white_edit| image:: ../imgs/icons/page_white_edit.png

This page details how to edit shaders. This applies both to :doc:`custom visualisation shaders <how_custom_visualisation>` for the :doc:`../window/texture_viewer`, but can also be used to temporarily replace or edit a shader used in the actual scene.

How to edit a custom shader
---------------------------

:doc:`Custom visualisation shaders <how_custom_visualisation>` allow you to define your own transformation on any texture you're viewing before it is displayed. Mostly this is useful for decoding packed or custom-format data, or displaying some data in a more visually intuitive fashion.

These shaders live as files in the application storage directory ( ``%APPDATA%/qrenderdoc/`` on windows or ``~/.local/share/qrenderdoc`` elsewhere), and can be edited in your editor of choice, any changes saved will be reloaded. Note however that there is currently no way to see the compile warnings or errors produced when editing externally.

To edit a shader inside RenderDoc simply click the edit button |page_white_edit| when you have selected your custom shader for use. This will launch a new window with the custom shader and any changes you make to this shader will be saved to the file and compiled and reflected in the texture viewer as long as you have that custom shader selected.

How to edit a scene shader
--------------------------

RenderDoc allows you to edit a shader used in the capture and make changes to it and see the effects in real-time.

To launch the shader editor, go to the pipeline stage you wish to change in the :doc:`../window/pipeline_state` windows, and click on the edit button |page_white_edit| next to the shader. If there are multiple edit options, a drop-down menu will be available - see below for more information about shader processing tools.

Any changes to the shader will affect any drawcall using this shader, not just the currently-selected drawcall. The changes will persist until the edit window is closed.

Shader processing tools
-----------------------

.. _shader-processing-tools:

Each graphics API has a native shader format - for D3D11 and D3D12 this is DXBC bytecode, for Vulkan this is SPIR-V bytecode, and for OpenGL this is GLSL shader text. Additionally it's possible for the bytecode shaders to contain embedded debug information with the original source code and compilation settings.

When editing a shader RenderDoc will display the original source if available, but otherwise it will attempt to invoke a shader processing tool to decompile the bytecode into a usable form. Multiple tools can be configured :ref:`in the settings window <shader-processing-tools-config>` and several for SPIR-V processing come with RenderDoc by default. If no tool is available RenderDoc displays a generated stub or default disassembly as a starting point.

Each shader tool is defined to translate from some input to some output, so for example a compiler might take HLSL and output DXBC, and a decompiler might take SPIR-V and output HLSL.

Once the edit window for a shader tool is open, the most appropriate tool to compile from the source into the API's native format is selected, and the parameters used for compilation are populated from debug info if available. These can then be customised further.

Using the built-in shader editor
--------------------------------

When you have launched the shader editor, the main window will be filled with the source of your shader. In here you can make edits and changes with the basic controls and syntax highlighting available with the Scintilla editor.

To compile the shader and apply your changes, either click the refresh button in the toolbar or press :kbd:`F5`. This will compile the shader and apply it, any warnings and errors will be added to the errors panel below the main source.

You can choose the tool used to compile in the lower section, as well as configure any parameters to it. See above for more information about shader processing tools.

.. warning::

  If there are errors compiling a visualisation shader, it will be removed from the texture viewer and normal RGB display will be used until you fix the error.

  If there are errors compiling a shader-replacement shader, it will revert back to the original shader from the capture until the error is fixed.

In addition, when editing visualisation shaders a button will be available to insert several useful snippets for custom shaders with the pre-defined variables that can be bound. For more detail, see :doc:`how_custom_visualisation`
