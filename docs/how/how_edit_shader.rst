How do I edit a shader?
=======================


.. |page_white_edit| image:: ../imgs/icons/page_white_edit.png

This page details how to edit shaders. This applies both to :doc:`custom visualisation shaders <how_custom_visualisation>` for the :doc:`../window/texture_viewer`, but can also be used to temporarily replace or edit a shader used in the actual scene.

How to edit a custom shader
---------------------------

:doc:`Custom visualisation shaders <how_custom_visualisation>` allow you to define your own transformation on any texture you're viewing before it is displayed. Mostly this is useful for decoding packed or custom-format data, or displaying some data in a more visually intuitive fashion.

These shaders live as ``.hlsl`` or ``.glsl`` files in the application storage directory ( ``%APPDATA%/qrenderdoc/`` on windows or ``~/.local/share/qrenderdoc`` elsewhere), and can be edited in your editor of choice, any changes saved will be reloaded. Note however that there is currently no way to see the compile warnings or errors produced when editing externally.

To edit a shader inside RenderDoc simply click the edit button |page_white_edit| when you have selected your custom shader for use. This will launch a new window with the custom shader and any changes you make to this shader will be saved to the file and compiled and reflected in the texture viewer as long as you have that custom shader selected.

How to edit a scene shader
--------------------------

RenderDoc allows you to edit a shader used in the capture and make changes to it and see the effects in real-time.

To launch the shader editor, go to the pipeline stage you wish to change in the :doc:`../window/pipeline_state` windows, and click on the edit button |page_white_edit| next to the shader.

Any changes to the shader will affect any drawcall using this shader, not just the currently-selected drawcall. The changes will persist until the edit window is closed.

.. warning::

	One unfortunate artifact of how the shader debug info works, not all #included hlsl files will come along with the debug info, only those files that contain compiled code. RenderDoc automatically replaces any #includes to missing files with just an empty comment, but unfortunately this can lead to compile errors in unused code.

For SPIR-V shaders in Vulkan, if there is embedded source info it will be used for compilation, however in many cases the source is not available.

RenderDoc allows you to configure a SPIR-V disassembler in the :doc:`../window/settings_window`. By default if ``spirv-cross`` is available it will be pre-configured and invoked automatically to decompile the SPIR-V into compilable GLSL.

Using the built-in shader editor
--------------------------------

When you have launched the shader editor, the main window will be filled with the source of your shader. In here you can make edits and changes with the basic controls and syntax highlighting available with the Scintilla editor.

To compile the shader and apply your changes, either click the refresh button in the toolbar or press :kbd:`F5`. This will compile the shader and apply it, any warnings and errors will be added to the box below the main source.

Custom shaders are built with a simple set of flags, any shaders from the scene will be compiled with the flags that were originally passed to the compiler.

.. warning::

	If there are errors compiling a visualisation shader, it will be removed from the texture viewer and normal RGB display will be used until you fix the error.

  If there are errors compiling a shader-replacement shader, it will revert back to the original shader from the capture until the error is fixed.

In addition, when editing visualisation shaders a button will be available to insert several useful snippets for custom shaders with the pre-defined variables that can be bound. For more detail, see :doc:`how_custom_visualisation`
