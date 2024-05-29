Fetch Shader details
====================

In this example we will fetch the disassembly for a shader and a set of constant values.

When disassembling a shader there may be more than one possible representation available, so we first enumerate the formats that are available using :py:meth:`~renderdoc.ReplayController.GetDisassemblyTargets` before selecting a target to disassemble to. The first target is always a reasonable default, and there will be at least one. :py:meth:`~renderdoc.ReplayController.GetDisassemblyTargets` takes a parameter indicating whether the pipeline state object will be available - some disassembly targets require the full pipeline and are not available when disassembling only a shader in isolation:

.. highlight:: python
.. code:: python

    print("Available disassembly formats:")

    targets = controller.GetDisassemblyTargets(True)

    for disasm in targets:
        print("  - " + disasm)

    target = targets[0]

Next we fetch any ancillary data that might be needed to disassemble - this varies by API depending on whether it supports multiple entry points per shader, or has a concept of pipeline state objects that are used together with a shader to disassemble.

For the purposes of this example we use the API abstraction :py:class:`~renderdoc.PipeState` so that this code works on a capture from any API, so we fetch the state bindings that we need. Finally we fetch the disassembled shader string with :py:meth:`~renderdoc.ReplayController.DisassembleShader` and print it:

.. highlight:: python
.. code:: python

	state = controller.GetPipelineState()

	# For some APIs, it might be relevant to set the PSO id or entry point name
	pipe = state.GetGraphicsPipelineObject()
	entry = state.GetShaderEntryPoint(rd.ShaderStage.Pixel)

	# Get the pixel shader's reflection object
	ps = state.GetShaderReflection(rd.ShaderStage.Pixel)

	cb = state.GetConstantBlock(rd.ShaderStage.Pixel, 0, 0)

    print("Pixel shader:")
    print(controller.DisassembleShader(pipe, ps.reflection, target))

Now we want to display the constants bound to this shader. Shader bindings is an area that diverges quite a lot between the APIs, and RenderDoc's abstraction over this is detailed in :ref:`more detail <descriptor-abstraction>`. For now, we'll simply select the first constant buffer in this shader and fetch the constants for it with :py:meth:`~renderdoc.ReplayController.GetCBufferVariableContents`.

.. highlight:: python
.. code:: python

    cbufferVars = controller.GetCBufferVariableContents(pipe, ps.resourceId, rd.ShaderStage.Pixel, entry, 0, cb.descriptor.resource, 0, 0)

Since constants can contain structs of other constants, we want to define a recursive function that will iterate over a constant and print it along with its value. We want to handle both vectors and matrices so we need to iterate over both rows and columns for each variable.

.. highlight:: python
.. code:: python

    def printVar(v, indent = ''):
        print(indent + v.name + ":")

        if len(v.members) == 0:
            valstr = ""
            for r in range(0, v.rows):
                valstr += indent + '  '

                for c in range(0, v.columns):
                    valstr += '%.3f ' % v.value.fv[r*v.columns + c]

                if r < v.rows-1:
                    valstr += "\n"

            print(valstr)

        for v in v.members:
            printVar(v, indent + '    ')

Finally, we iterate over the constants that we fetched earlier calling the function for each.

.. highlight:: python
.. code:: python

    for v in cbufferVars:
        printVar(v)

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <fetch_shader.py>`.

.. literalinclude:: fetch_shader.py

Sample output:

.. sourcecode:: text

    Available disassembly formats:
    - DXBC
    - AMD GCN ISA
    Pixel shader:
    Shader hash 9dd8337a-c75dd787-1fa0f07e-5f39f955

    ps_5_0
        dcl_globalFlags refactoringAllowed
        dcl_constantbuffer cb0[8], immediateIndexed
        dcl_sampler gDepthSam (s0), mode_default
        dcl_resource_texture2d (float,float,float,float) gDepthMap (t0)
        dcl_resource_texture2d (float,float,float,float) gGBufferMap (t1)
        dcl_input_ps linear v1.xyz
        dcl_input_ps linear v2.xyw
        dcl_output o0.xyzw
        dcl_output o1.xyzw
        dcl_temps 6

    0: nop
    1: mov r0.xyz, v2.xywx
    2: div r0.xy, r0.xyxx, r0.zzzz
    3: mul r0.xy, r0.xyxx, l(0.500000, -0.500000, 0.000000, 0.000000)
    4: add r0.xy, r0.xyxx, l(0.500000, 0.500000, 0.000000, 0.000000)
    5: mov r0.xy, r0.xyxx
    6: sample_indexable(texture2d)(float,float,float,float) r0.z, r0.xyxx, gDepthMap.yzxw, gDepthSam
    7: mov r0.z, r0.z
    8: nop
    9: mov r0.z, r0.z
    10: mov r0.z, -r0.z
    11: add r0.z, r0.z, l(1.001801)
    12: div r0.z, l(0.421448), r0.z
    13: mov r0.z, r0.z
    14: dp3 r0.w, v1.xyzx, v1.xyzx
    15: rsq r0.w, r0.w
    16: mul r1.xyz, r0.wwww, v1.xyzx
    17: div r0.z, r0.z, r1.z
    18: mul r2.xyz, r0.zzzz, r1.xyzx
    19: sample_indexable(texture2d)(float,float,float,float) r0.xyzw, r0.xyxx, gGBufferMap.xyzw, gDepthSam
    20: dp3 r1.w, r0.xyzx, r0.xyzx
    21: rsq r1.w, r1.w
    22: mul r0.xyz, r0.xyzx, r1.wwww
    23: mov r3.xyz, gLightPosV.xyzx
    24: mov r4.xyz, -r2.xyzx
    25: add r4.xyz, r3.xyzx, r4.xyzx
    26: dp3 r1.w, r4.xyzx, r4.xyzx
    27: rsq r1.w, r1.w
    28: mul r4.xyz, r1.wwww, r4.xyzx
    29: dp3 r1.w, r0.xyzx, r4.xyzx
    30: max r5.x, r1.w, l(0)
    31: nop
    32: mov r1.xyz, -r1.xyzx
    33: add r1.xyz, r1.xyzx, r4.xyzx
    34: dp3 r1.w, r1.xyzx, r1.xyzx
    35: rsq r1.w, r1.w
    36: mul r1.xyz, r1.wwww, r1.xyzx
    37: mov r0.xyz, r0.xyzx
    38: mul r0.w, r0.w, l(64.000000)
    39: dp3 r0.x, r1.xyzx, r0.xyzx
    40: max r0.x, r0.x, l(0)
    41: log r0.x, r0.x
    42: mul r0.x, r0.x, r0.w
    43: exp r5.y, r0.x
    44: mov r5.y, r5.y
    45: nop
    46: mov r3.xyz, r3.xyzx
    47: mov r2.xyz, r2.xyzx
    48: mov r0.xyz, gLight.att.xyzx
    49: mov r1.xyz, -r2.xyzx
    50: add r1.xyz, r1.xyzx, r3.xyzx
    51: dp3 r0.w, r1.xyzx, r1.xyzx
    52: sqrt r0.w, r0.w
    53: mul r0.y, r0.y, r0.w
    54: add r0.x, r0.y, r0.x
    55: mul r0.y, r0.w, r0.w
    56: mul r0.y, r0.z, r0.y
    57: add r0.x, r0.y, r0.x
    58: div r0.x, l(1.000000), r0.x
    59: mov r0.x, r0.x
    60: mul r0.xy, r5.xyxx, r0.xxxx
    61: max r0.xy, r0.xyxx, l(0, 0, 0, 0)
    62: mul o0.xyzw, r0.xxxx, gLight.diffuse.xyzw
    63: mul o1.xyzw, r0.yyyy, gLight.diffuse.xyzw
    64: ret

    gLight:
        pos:
        -2.022 2.000 -3.694 
        dir:
        0.000 0.000 0.000 
        ambient:
        0.300 0.300 0.300 1.000 
        diffuse:
        0.300 1.000 0.600 1.000 
        spec:
        0.500 0.500 0.500 1.000 
        att:
        0.000 0.200 0.100 
        spotPower:
        0.000 
        range:
        3.000 
    gLightPosV:
        -2.022 0.200 6.306 -107374176.000 
    gLigthDirES:
        -0.298 -0.596 -0.745 
    gWorldViewProj:
        1.567 0.000 0.000 0.000 
        0.000 2.414 0.000 0.000 
        0.000 0.000 1.002 1.000 
        -3.169 0.483 5.896 6.306 
    gWorldView:
        1.000 0.000 0.000 0.000 
        0.000 1.000 0.000 0.000 
        0.000 0.000 1.000 0.000 
        -2.022 0.200 6.306 1.000 
