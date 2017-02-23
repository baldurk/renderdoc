How do I debug a shader?
========================

This page goes into detail about how to set up your captures for debugging shaders, as well as how to debug a shader and what controls are available.

Including debug info in shaders
-------------------------------

For the most part at least some debug information is included with shaders unless it is being explicitly stripped out at some point. There is usually an option to also include additional debug information - such as original source code in a high-level language. The exact process varies by API, but for D3D11 the flag ``/Zi`` to fxc or the equivalent flag to ``D3DCompile()`` will include additional debugging information, and ``/Qstrip_debug`` and ``/Qstrip_reflection`` will remove reflection information that can be useful - such as the names of variables in constant buffers.

For more information on how to get this unstripped debug information to renderdoc, see :ref:`unstripped-shader-info`.

Debugging a vertex
------------------

Vertex debugging is invoked from the mesh viewer. With the mesh viewer open you can select the input vertex you wish to debug.

When a vertex is selected in the mesh data for the vertex input it will be highlighted along with the primitive it is part of in the mesh display, provided the display is in vertex input mode.

Either right click and choose debug vertex from the context menu, or click on the debug icon in the toolbar.

.. figure:: ../imgs/Screenshots/VertexDebug.png

	Vertex Debugging: Launching vertex debugging from the mesh viewer.

From here the debugger will open with the vertex shader in the shader debugger. The inputs are automatically filled in from the mesh data.

.. note::

	Geometry and tessellation shaders are not yet debuggable.

Debugging a Pixel
-----------------

Pixel debugging is launched from the texture viewer. For more details on selecting the pixel to debug see :doc:`how_inspect_pixel`.

When a given pixel is selected you can click the history button underneath the pixel context. This will launch the :ref:`pixel-history` window with the selected pixel showing every modification. You can then choose to debug any of the triangles that generated a change.

If you already have the current drawcall selected that you want to debug, you can click the debug button to skip the pixel history and jump straight to the debugger. The inputs to the pixel will be automatically filled in.

There are a couple of things to note while pixel debugging:

* If the drawcall selected doesn't write to the pixel you have highlighted, the pixel history window will open to let you choose which draw call to debug.
* If a drawcall overdraws the same pixel several times then the results of debugging will come from the last fragment that passed the depth test. If you wish to choose a particular fragment from the list then first launch the pixel history and choose which fragment to debug from the list there.

Debugging a Compute thread
--------------------------

To debug a compute thread simply go to the compute shader section of the pipeline state viewer and enter the group and thread ID of the thread you would like to debug. This thread will be debugged in isolation with no other threads in the group running.

This means there can be no synchronisation with any other compute thread running and the debugging will run from start to finish as if no other thread had run.

.. warning::

	This feature is **highly** experimental and is provided with no guarantees yet! It may work on simple shaders which is why it is available.

Debugging Controls
------------------

When debugging, at the moment the controls are fairly basic.

.. figure:: ../imgs/Screenshots/ShaderControls.png

	Shader controls: Controls for stepping through shaders.

.. |runfwd| image:: ../imgs/icons/runfwd.png
.. |runback| image:: ../imgs/icons/runback.png

The toolbar at the top gives controls for the program flow through the shader. |runfwd| Run and |runback| Run Backward simply run from the current position all the way through to the end or start of the program respectively.

The keyboard shortcuts for these controls are :kbd:`F5` and :kbd:`Shift-F5` respectively.

You can set a breakpoint by pressing :kbd:`F9` (this will also remove a breakpoint that is already there). When running in each direction or to cursor (see below) if execution hits a breakpoint it will stop.

.. |runsample| image:: ../imgs/icons/runsample.png

This button will run to the next texture load, gather or sample operation, and stop as if a breakpoint had been placed on that instruction.

.. |runnaninf| image:: ../imgs/icons/runnaninf.png

This button will run to the next operation that generates either a NaN or infinity value instead of a floating point value. This will not apply to operations that produce integer results which may be NaN/infinity when interpreted as float.

.. |stepnext| image:: ../imgs/icons/stepnext.png
.. |stepprev| image:: ../imgs/icons/stepprev.png

The other controls allow for single stepping and limited running. |stepnext| Step forward will execute the current instruction and continue to the next - this includes following any flow control statements such as jumps, loops, etc.

|stepprev| Step backwards will jump back to whichever instruction lead to the current instruction. This does not necessarily mean the previous instruction in the program as it could be the destination of a jump. Stepping forwards and stepping backwards will always reverse each other. The shortcuts for these commands are :kbd:`F10` and :kbd:`Shift-F10`

.. |runcursor| image:: ../imgs/icons/runcursor.png

The final control is to |runcursor| Run to the cursor. This will perform in a similar fashion to the "Run" command, but when it reaches the line that the cursor highlights it will stop and pause execution. It will also stop if it reaches the end of the shader.


.. note::

	The highlighted instruction at any given point indicates the *next* instruction to be executed - not the instruction that was just executed.

Hovering over a register in either the disassembly or in the view windows will open a tooltip showing the value in different interpretations.

There is also a toggle available to control the 'default' interpretation of temporary register values - float or int. Since registers are typeless typically they are interpreted as float values, but with this toggle you can toggle them to be interpreted as integers.

Debugging Displays
------------------

Currently there is only a very basic display when debugging shaders.


There are two windows that display different types of registers. The constants window will display input and constant buffer registers that are immutable throughout execution. This will also list registers for resources and samplers (with basic format information).

.. figure:: ../imgs/Screenshots/ShaderConsts.png

	Constants window: Constant, input and resource registers.

The other window will contain variable/mutable registers. These contain temporaries that are typically allocated up front and will update as you step through the execution of the shader. This window also contains the output registers.

.. figure:: ../imgs/Screenshots/ShaderRegs.png

	Variable window: Variable registers - temporaries and outputs.

The final window is initially empty but can be filled out as needed. This shows custom watch expressions and their values. Here you can write any expression involving an input, temporary or output register along with a swizzle and typecast.

Swizzles follow the standard hlsl rules - ``.[xyzw]`` or ``.[rgba]`` in any permutation or repetition will show those channels.

The custom typecast can be any of ``,x`` ``,i`` ``,d`` ``,f`` ``,u`` ``,b`` to display the register as hex, signed integer, double, float, unsigned, or bitwise respectively.

.. figure:: ../imgs/Screenshots/ShaderWatch.png

	Watch window: Watch window - custom register expressions evaluated.
