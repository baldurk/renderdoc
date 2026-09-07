Example: Pixel History & Shader Debug
=====================================

Two of the more complex and closely related analysis steps you can perform in RenderDoc is fetching the history of modifications to a given pixel in a texture, and debugging shaders to get a trace of what happened with its execution.

In this sample we will run a given pixel history, and then debug one of the pixel shaders that executed.

Choosing a pixel
----------------

To begin with we need to find a pixel, and the easiest way to do this is to let the user choose it naturally in the texture viewer. In addition to our :doc:`typical start <index>` that loads a capture we will also afterwards prompt the user to pick a pixel.

We do this by calling a function that will query the user if they are ready - if not the function will call itself back in 5 seconds via delayed callback to ask again.

Once the user has gone to the texture viewer and picked a pixel they like at the right event, they can click yes and we will proceed to do the actual work.

.. highlight:: python
.. code:: python

    def check_ready():
        choice = pyrenderdoc.Extensions().QuestionDialog(
            "Are you at an interesting event with pixel selected?",
            [qrenderdoc.DialogButton.Yes, qrenderdoc.DialogButton.No, qrenderdoc.DialogButton.Cancel],
            "Ready?",
        )
        
        if choice == qrenderdoc.DialogButton.Cancel:
            return

        if choice == qrenderdoc.DialogButton.No:
            pyrenderdoc.DelayedCallback(5000, check_ready)
            return

        prepare_history()

    check_ready()

Configuring Pixel History
-------------------------

From the :class:`~qrenderdoc.TextureViewer` class that we can obtain (:meth:`~qrenderdoc.CaptureContext.GetTextureViewer`), we can query for the selected texture (:meth:`~qrenderdoc.TextureViewer.GetCurrentResource`), subresource (:meth:`~qrenderdoc.TextureViewer.GetSelectedSubresource`), and pixel (:meth:`~qrenderdoc.TextureViewer.GetPickedLocation`).

.. highlight:: python
.. code:: python

    tex_view = pyrenderdoc.GetTextureViewer()

    # find the selected texture and location
    id = tex_view.GetCurrentResource()
    sub = tex_view.GetSelectedSubresource()
    x, y = tex_view.GetPickedLocation()

Once we've done this we can open a new pixel history viewer (:meth:`~qrenderdoc.CaptureContext.ViewPixelHistory`) in preparation. Because pixel history can be a long-running process it is a good user experience to load the viewer first and display it, then later fill it with data once obtained.

We will open it using the properties that we have queried, and then display it (:meth:`~qrenderdoc.CaptureContext.AddDockWindow`) on the right hand side of the python shell wherever it is currently docked.

.. highlight:: python
.. code:: python

    history_window = pyrenderdoc.ViewPixelHistory(id, x, y, sub.slice, disp)
    pyrenderdoc.AddDockWindow(
        history_window.Widget(),
        qrenderdoc.DockReference.RightOf,
        pyrenderdoc.GetPythonShell().Widget(),
    )

Running Pixel History
---------------------

As we mentioned above, pixel history can be a fairly long running task that takes many seconds to complete depending on the complexity of the capture. This is a good time to consider :ref:`pythreading` and how to arrange the work such that the UI does not become unresponsive.

If writing a UI extension, by default code will be running on the UI thread so you are recommended to use :meth:`~qrenderdoc.ReplayManager.AsyncInvoke` and :meth:`~qrenderdoc.CaptureContext.InvokeOntoUIThread`. First you call a callback on the replay thread to do the long-running analysis work, then call a callback on the UI thread for any updating of views or displaying information to the user.

Since we are running this in the python scripting panel, we can take advantage of that script running in a separate thread itself and instead use the convenience helper :meth:`~qrenderdoc.CaptureContext.GetBlockingController` to obtain a blocking version of the :class:`~renderdoc.ReplayController`. This will cause the python thread to stall while the pixel history works but the UI will remain responsive. Once the data is returned, we can pass it to the UI display.

.. highlight:: python
.. code:: python

    controller = pyrenderdoc.GetBlockingController()

    history = controller.PixelHistory(id, x, y, sub, disp.typeCast)

    history_window.SetHistory(history)

Examining Pixel History
-----------------------

The pixel history is returned as a list of :class:`~renderdoc.PixelModification` structures. Each of these structures corresponds to one instance of the pixel potentially being modified. We can examine this ourselves to process the data programmatically in addition to the UI panel we opened above.

.. note::

    It is possible to have multiple modifications in one event! This can happen if there is a draw with multiple polygons overlapping the same pixel.

    You can use :data:`~renderdoc.PixelModification.fragIndex` together with :data:`~renderdoc.PixelModification.eventId` to identify this case.

The modification structure itself contains as much information about the modification as possible, but depending on the API details or the particular event not all information may be available.

Each modification contains the value before modification (:data:`~renderdoc.PixelModification.preMod`), the value after modification (:data:`~renderdoc.PixelModification.postMod`), and the value output from the shader (:data:`~renderdoc.PixelModification.shaderOut`) which are all of type :class:`~renderdoc.ModificationValue`.

Not all of this information is always available, for example on events without shaders like clears. In other cases it may not be possible to obtain all information such as in secondary command buffers on vulkan. If a value is not available :meth:`~renderdoc.ModificationValue.IsValid` will return false.

Pixel history can give the most information for rasterized modifications but pixels in a texture can also be modified via direct writes from shaders. In this case :data:`~renderdoc.PixelModification.directShaderWrite` will be true indicating that the texture was bound for write at an event. When true, most other information apart from pre- and post- modification values will be unavailable as RenderDoc does not instrument shader writes to determine whether the texel was modified and if so by what.

A useful high level helper is :meth:`~renderdoc.PixelModification.Passed` which returns true if the pixel did not fail any known or detectable test. This does not *guarantee* that the pixel was modified especially in the case of direct shader writes. If a pixel did not pass, then there are bool properties in :class:`~renderdoc.PixelModification` for different fixed function tests or checks it may have failed.

Launching pixel debug
---------------------

Once we have our list of history events, we can try to debug the pixel shader in one of them. To do that we filter the list of events to those that have:

#. Passed all known tests
#. Have a pixel shader bound
#. Are an event that is a draw call

Picking the first of these, we can then :ref:`move to that event <currentevent>` and try to initiate a pixel debug (:meth:`~renderdoc.ReplayController.DebugPixel`). First we need to check the shader reflection data to make sure it is :data:`~renderdoc.ShaderDebugInfo.debuggable`. Not all shader constructs are currently supported, so we query and print an error if there is something that can't be debugged.

.. highlight:: python
.. code:: python

    if refl is None or not refl.debugInfo.debuggable:
        print("Shader can't be debugged:")
        print(refl.debugInfo.debugStatus)
        return

Launching the debug is done via :meth:`~renderdoc.ReplayController.DebugPixel` which takes the obvious x, y but also a small configuration struct (:class:`~renderdoc.DebugPixelInputs`) with other properties to narrow the candidate pixel to debug. To handle the case of multiple overlapping polygons we use the :data:`~renderdoc.PixelModification.primitiveID` we hopefully got from pixel history. If we didn't get a primitive ID the debugger will pick an arbitrary instance of the pixel shader at that co-ordinate to debug.

The return value will be a debug trace to be owned by python, which is ready for further processing. If the trace failed to initialise for any reason, the :data:`~renderdoc.ShaderDebugTrace.debugger` member will be ``None``.

.. highlight:: python
.. code:: python

    inputs = renderdoc.DebugPixelInputs()

    inputs.primitive = p.primitiveID
    inputs.sample = renderdoc.ReplayController.NoPreference
    inputs.view = renderdoc.ReplayController.NoPreference

    trace = controller.DebugPixel(x, y, inputs)

    if trace.debugger is None:
        print("Debug failed :(")
        controller.FreeTrace(trace)
        return

.. warning::
    It is important to note that the trace is owned by the caller, and must be freed with :data:`~renderdoc.ReplayController.FreeTrace` once you are done with it, to avoid a leak. For more information see :doc:`../in_depth/lifetimes`.

Simulating debug session
------------------------

The trace itself contains global information which is not specific to any particular step of the execution of the shader. For example it will contain bound resources (:data:`~renderdoc.ShaderDebugTrace.readOnlyResources`), and constant data (:data:`~renderdoc.ShaderDebugTrace.constantBlocks`), as well as the input values (:data:`~renderdoc.ShaderDebugTrace.inputs`) which will differ by shader stage.

We will get into other members below, but the important one to consider first is the :data:`~renderdoc.ShaderDebugTrace.debugger` member. This is an opaque handle to the debug engine which is now set up to begin simulating execution of a shader.

RenderDoc's debug engine simulates in small steps to allow incremental processing and display as needed. For simplicity and in the common case we will just repeatedly simulate until it is complete. This is done by calling :meth:`~renderdoc.ReplayController.ContinueDebug` which returns a list of :class:`~renderdoc.ShaderDebugState`, until it is complete when it returns an empty list.

.. highlight:: python
.. code:: python

    states: List[renderdoc.ShaderDebugState] = []

    # continually simulate the shader until it completes
    while True:
        more = controller.ContinueDebug(trace.debugger)
        if more == []:
            break
        states += more

Examining debug shader states
-----------------------------

The main trace of the shader's execution is now represented in this list of :class:`~renderdoc.ShaderDebugState`. Each state represents a single atomic execution - usually one instruction or indivisible set of instructions. At each step you can see the step index overall (which linearly increases from 0), the next instruction that will be executed, the callstack, and any changes that happened to debug variables.

The first state at :data:`~renderdoc.ShaderDebugState.stepIndex` 0 is the state immediately before any shader instructions have executed, this then linearly increases by one at each step so ``states[x].stepIndex == x`` in our case. The next state is the state immediately after the first instruction, and before the second.

Each state lists the :data:`~renderdoc.ShaderDebugState.nextInstruction` which will be executed. Each instruction has a unique index but these indices are not compactly numbered starting from 0 - different shader representations may vary in different APIs.

Each state also lists the :data:`~renderdoc.ShaderDebugState.changes` to debug variables. Debug variables are indexed by name and so may be arbitrary, but will generally follow normal identifier rules and should be grouped with ``foo.bar`` being considered to represent a member ``bar`` in a parent ``foo``, and similarly for ``foo[2]`` being index ``2`` in an array ``foo[]``.

Changes to these variables are given with the value they have before, and the value after. This means it is easy to step forwards or backwards across a state without needing to store significant amounts of data as the information is bi-directional. The first time a variable is seen, its :data:`~renderdoc.ShaderVariableChange.before` will be an uninitialised :class:`~renderdoc.ShaderVariable`, to indicate that it did not exist before. If a variable goes out of scope or otherwise exits its lifespan then instead its :data:`~renderdoc.ShaderVariableChange.after` will be an uninitialised :class:`~renderdoc.ShaderVariable`.

.. tip::
    This representation is convenient for walking back and forward through the steps, but it also means there is no single lookup of all variables at any given step. If you need this you will need to walk the steps forward from the first step and accumulate variables as necessary.

.. highlight:: python
.. code:: python

    # look at the mid-point state
    if states == []:
        print("Shader debug failed!")
    else:
        state = states[len(states) // 2]
        print(
            f"Examining step {state.stepIndex}, before instruction {state.nextInstruction}"
        )
        stack = "\n".join(state.callstack)
        print(f"Callstack:\n{stack}")

        print()

        print(f"{len(state.changes)} debug variable changes")
        for ch in state.changes:
            print(f" '{ch.before.name}' -> '{ch.after.name}'")

        print()

Source Variables and Instruction info
-------------------------------------

Debug variables as listed above are the variables directly simulated by the simulation such as registers or SSA values and are often hard to interpret. Most APIs provide optional debug information which can give information about how high-level source code and variables maps down to the variables and instructions being simulated.

This information is stored in the trace, in :data:`~renderdoc.ShaderDebugTrace.instInfo`. Each entry describes a single instruction as given by :data:`~renderdoc.InstructionSourceInfo.instruction`, which can be looked up by the instruction indices given in the debug steps. Not every instruction is guaranteed to have an entry, as for some cases the same debug info will apply to several adjacent instructions. If there is no direct match for an instruction, the closest previous match will apply.

.. warning::
    As mentioned above, not all APIs use a compact list of instructions from ``0..n``. You should **not** index into the :data:`~renderdoc.ShaderDebugTrace.instInfo` array with the instruction index, but instead search it for the closest matching instruction number.

.. highlight:: python
.. code:: python

    infos = [i for i in trace.instInfo if i.instruction <= state.nextInstruction]

    if infos == []:
        info = trace.instInfo[0]
    else:
        info = infos[-1]

Each instruction's information contains the line information for the instruction, giving both the line number in the default-generated disassembly (:meth:`~renderdoc.ReplayController.DisassembleShader`) as well as (if available) the line number and optionally column where the code mapped to in the original source (:data:`~renderdoc.ShaderDebugInfo.files`).

The information also gives information about high-level variables in the source that map to debug variables, possibly only partially available or mapped across different debug variables. At each instruction there is a list of :class:`~renderdoc.SourceVariableMapping` which provides the information about the type of the original variable as well as a component-wise list of debug variables (:class:`~renderdoc.DebugVariableReference`) that it maps to.

In our example we print out the disassembly and source line for the instruction, as well as a list of source variables.

.. highlight:: python
.. code:: python

    disasm = controller.DisassembleShader(
        pipe.GetGraphicsPipelineObject(),
        refl,
        "",
    )

    disline = disasm.splitlines()[info.lineInfo.disassemblyLine - 1]

    srcline = ""
    if info.lineInfo.fileIndex >= 0:
        src = refl.debugInfo.files[info.lineInfo.fileIndex].contents
        srcline = src.splitlines()[info.lineInfo.lineStart - 1]

    print(f"Examining instruction {info.instruction}.")
    print(f"  which has {len(info.sourceVars)} source vars:")
    if srcline != "":
        print(srcline)
    print(disline)
    print()
    for s in info.sourceVars:
        debugVars = ", ".join(
            [v.name + "." + ("xyzw"[v.component % 4]) for v in s.variables]
        )
        print(f"   {s.name} is {str(s.type)} stored in: {debugVars}")

.. note::
    If the shader being debugged does not have any debug information available, some or all of this may be unavailable.

Displaying debug session in the UI
----------------------------------

The results of a shader debug can be displayed in the UI, though not directly from the data we have obtained in python. Instead the UI shader viewer expects to be given ownership over the debug trace immediately after it is created so that it can perform the simulation steps with :meth:`~renderdoc.ReplayController.ContinueDebug` itself.

.. highlight:: python
.. code:: python

    trace = controller.DebugPixel(x, y, inputs)
    shad = pyrenderdoc.DebugShader(
        pipe.GetShaderReflection(renderdoc.ShaderStage.Pixel),
        pipe.GetGraphicsPipelineObject(),
        trace,
        "Debugged From Python",
    )

    pyrenderdoc.AddDockWindow(
        shad.Widget(),
        qrenderdoc.DockReference.BottomOf,
        history_window.Widget(),
    )

Sample Output
-------------

.. sourcecode:: text

    Analysing on "Main Color Buffer"@{'mip': '0', 'sample': '0', 'slice': '0'} at 502,533
    4 modifications to that pixel:
    at EID 637 changed from [0.93, 0.67, 0.81, 1.0] to [0.0, 0.0, 0.0, 1.0]
    at EID 812 modification failed
    at EID 823 modification failed
    at EID 831 changed from [0.0, 0.0, 0.0, 1.0] to [1.85, 0.49, 0.81, 1.0]
    Examining step 653, before instruction 157
    Callstack:
    main
    ApplyConeLight
    ApplyLightCommon

    1 debug variable changes
    'r8' -> 'r8'

    Examining instruction 157.
    which has 24 source vars:
        float3 halfVec = normalize(lightDir - viewDir);
    157:   dp3 r4.w, r8.xyzx, r8.xyzx

    vsOutput.position is VarType.Float stored in: v0.x, v0.y, v0.z, v0.w
    vsOutput.worldPos is VarType.Float stored in: v1.x, v1.y, v1.z
    vsOutput.uv is VarType.Float stored in: v2.x, v2.y
    vsOutput.viewDir is VarType.Float stored in: v3.x, v3.y, v3.z
    vsOutput.shadowCoord is VarType.Float stored in: v4.x, v4.y, v4.z
    vsOutput.normal is VarType.Float stored in: v5.x, v5.y, v5.z
    vsOutput.tangent is VarType.Float stored in: v6.x, v6.y, v6.z
    vsOutput.bitangent is VarType.Float stored in: v7.x, v7.y, v7.z
    <main return value> is VarType.Float stored in: o0.x, o0.y, o0.z
    diffuseAlbedo is VarType.Float stored in: r1.x, r1.y, r1.z
    normal is VarType.Float stored in: r3.x, r3.y, r3.z
    specularMask is VarType.Float stored in: r1.w
    viewDir is VarType.Float stored in: r4.x, r4.y, r4.z
    colorSum is VarType.Float stored in: r2.x, r2.y, r2.z
    tileLightCountConeShadowed is VarType.UInt stored in: r6.y
    tileLightCountCone is VarType.UInt stored in: r6.x
    tileLightLoadOffset is VarType.UInt stored in: r0.y
    lightData.radiusSq is VarType.Float stored in: r8.w
    lightData.color is VarType.Float stored in: r9.x, r9.y, r9.z
    lightData.coneDir is VarType.Float stored in: r10.x, r10.y, r10.z
    lightData.coneAngles is VarType.Float stored in: r10.w
    invLightDist is VarType.Float stored in: r4.w
    lightDir is VarType.Float stored in: r11.x, r11.y, r11.z
    distanceFalloff is VarType.Float stored in: r5.w

Example Source
--------------

This example can be found under the name "Pixel History & Shader Debug" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <history_debug.py>`.

.. literalinclude:: history_debug.py
