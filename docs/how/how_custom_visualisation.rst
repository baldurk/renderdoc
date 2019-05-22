How do I use a custom visualisation shader?
===========================================

This page details how to set up a custom shader for visualisation. This can be used in the :doc:`../window/texture_viewer` to unpack or decode complex formats, or simply apply a custom complex transformation to the image beyond the default controls.

Introduction
------------

The basic process of setting up the custom shader involves writing a shader file that will be compiled and used by RenderDoc. Note that you can use any language that is either natively accepted by the graphics API used, or any language that can be compiled to an accepted shader.

For example on D3D11 or D3D12, hlsl is the only language usable by default, but on Vulkan you can use hlsl or glsl as long as a compiler is available.

There are several special global variables that can be specified and will be filled in with values by RenderDoc.

Your pixel shader defines an operation that transforms the raw value from the input texture into a value that will then be displayed by the texture viewer. The usual texture viewer controls for range adaption and channels will still be available on the resulting texture.

To set up your shader, it's recommended that you use the UI defined in the documentation for the :doc:`../window/texture_viewer`, but you can manually create a ``.hlsl`` or ``.glsl`` file in the application storage directory ( ``%APPDATA%/qrenderdoc/`` on windows or ``~/.local/share/qrenderdoc`` elsewhere). The file must contain an entry point ``main()`` that returns ``float4``, and uses any of the below inputs. These shaders are loaded when RenderDoc loads a capture, and RenderDoc watches for any changes to the files (either externally or in the shader editor in RenderDoc) and automatically reloads them.

.. note::

	Since ``.glsl`` is used for both Vulkan and OpenGL shaders, you can differentiate by the pre-defined macro ``VULKAN`` if you are writing a shader to be used for both.

Predefined inputs
-----------------

There are several pre-defined inputs that can either be taken as parameters to the shader entry point, or defined as global variables with a particular name that will then be filled in. There are also definitions for the different type of input textures.

.. warning::

	Type and capitalisation is important for these variables, so ensure you use the right declaration!

The shader editor when using the UI can be used to insert these snippets for you, with the right type and spelling. For GLSL these snippets are inserted at the top of the file just after any ``#version`` statement.

UV co-ordinates
~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	/* HLSL */
	float4 main(float4 pos : SV_Position, float4 uv : TEXCOORD0) : SV_Target0
	{
	  return 1;
	}

	/* GLSL */
	layout (location = 0) in vec2 uv;

	void main()
	{
	  // ...
	}

This input is defined in HLSL as a second parameter to the shader entry point. The first defines the usual ``SV_Position`` system semantic, and the first two components of the second ``TEXCOORD0`` parameter gives UV co-ordinates from 0 to 1 in each dimension over the size of the texture (or texture slice).

In GLSL it is bound as a ``vec2`` input at location ``0``.

You can also use the auto-generated system co-ordinates - ``SV_Position`` or ``gl_FragCoord`` if you need co-ordinates in ``0`` to ``N,M`` for an ``NxM`` texture.

.. note::

	You must bind these parameters like this in this order to ensure the linkage with the vertex shader matches.

Constant Parameters
~~~~~~~~~~~~~~~~~~~

There are several constant parameters available, each detailed below with the values they contain. Where possible these are bound by name as globals for convenience, but in Vulkan all variables must be contained within a single uniform buffer. The parameters correspond with the GLSL documentation but are contained within a uniform buffer at binding 0, with a structure given as so:


.. highlight:: c++
.. code:: c++

	layout(binding = 0, std140) uniform RENDERDOC_Uniforms
	{
		uvec4 TexDim;
		uint SelectedMip;
		uint TextureType;
		uint SelectedSliceFace;
		int SelectedSample;
		uvec4 YUVDownsampleRate;
		uvec4 YUVAChannels;
	} RENDERDOC;

In this way you can access the properties as ``RENDERDOC.TexDim`` instead of ``RENDERDOC_TexDim``.

Texture dimensions
~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	uint4 RENDERDOC_TexDim; // hlsl
	uniform uvec4 RENDERDOC_TexDim; // glsl

	uint4 RENDERDOC_YUVDownsampleRate; // hlsl / vulkan glsl only
	uint4 RENDERDOC_YUVAChannels; // hlsl / vulkan glsl only


``RENDERDOC_TexDim`` will be filled out with the following values:

* ``.x``  Width
* ``.y``  Height (if 2D or 3D)
* ``.z``  Depth if 3D or array size if an array
* ``.w``  Number of mip levels


``RENDERDOC_YUVDownsampleRate`` will be filled out with the following values:

* ``.x``  Horizontal downsample rate. 1 for equal luma and chroma width, 2 for half rate.
* ``.y``  Vertical downsample rate. 1 for equal luma and chroma height, 2 for half rate.
* ``.z``  Number of planes in the input texture, 1 for packed, 2+ for planar
* ``.w``  Number of bits per component, e.g. 8, 10 or 16.


``RENDERDOC_YUVAChannels`` will be filled out an index indicating where each channel comes from in the source textures. The order is ``.x`` for ``Y``, ``.y`` for ``U``, ``.z`` for ``V`` and ``.w`` for ``A``.

The indices for channels in the first texture in the normal 2D slot are ``0, 1, 2, 3``. Indices from ``4`` to ``7`` indicate channels in the second texture, and so on.

If a channel is not present, e.g. alpha is commonly not available, it will be set to ``0xff == 255``.

Selected Mip level
~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	uint RENDERDOC_SelectedMip; // hlsl
	uniform uint RENDERDOC_SelectedMip; // glsl


This variable will be filled out with the selected mip level in the UI.

Selected Slice/Face
~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	uint RENDERDOC_SelectedSliceFace; // hlsl
	uniform uint RENDERDOC_SelectedSliceFace; // glsl


This variable will be filled out with the selected texture array slice (or cubemap face) in the UI.

Selected Multisample sample
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	int RENDERDOC_SelectedSample; // hlsl
	uniform int RENDERDOC_SelectedSample; // glsl


This variable will be filled out with the selected multisample sample index as chosen in the UI. If the UI has 'average value' selected, this variable will be negative and with an absolute value equal to the number of samples.

So for example in a 4x MSAA texture, the valid values are ``0``, ``1``, ``2``, ``3`` to select a sample, or ``-4`` for 'average value'.

Current texture type
~~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	uint RENDERDOC_TextureType; // hlsl
	uniform uint RENDERDOC_TextureType; // glsl


This variable will be set to a given integer value, depending on the type of the current texture being displayed. This can be used to sample from the correct resource.

.. note::

	The value varies depending on the API this shader will be used for, as each has different resource bindings.

D3D11 or D3D12 / HLSL
^^^^^^^^^^^^^^^^^^^^^

#. 1D texture
#. 2D texture
#. 3D texture
#. Depth
#. Depth + Stencil
#. Depth (Multisampled)
#. Depth + Stencil (Multisampled)
#. Legacy: used to be cubemap, removed as it's unused
#. 2D texture (Multisampled)

OpenGL / GLSL
^^^^^^^^^^^^^

#. 1D texture
#. 2D texture
#. 3D texture
#. Cubemap
#. 1D array texture
#. 2D array texture
#. Cubemap array
#. Rectangle
#. Buffer texture
#. 2D texture (Multisampled)

Vulkan / GLSL
^^^^^^^^^^^^^

#. 1D texture
#. 2D texture
#. 3D texture
#. 2D texture (Multisampled)

Samplers (D3D11/D3D12 only)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

	SamplerState pointSampler : register(s0);
	SamplerState linearSampler : register(s1);

These samplers are provided to allow you to sample from the resource as opposed to doing straight loads. They are bound by slot and not by variable name - so this means you can name them as you wish but you must specify the register binding explicitly.

Resources
~~~~~~~~~

D3D11 or D3D12 / HLSL
^^^^^^^^^^^^^^^^^^^^^

.. highlight:: c++
.. code:: c++

	Texture1DArray<float4> texDisplayTex1DArray : register(t1);
	Texture2DArray<float4> texDisplayTex2DArray : register(t2);
	Texture3D<float4> texDisplayTex3D : register(t3);
	Texture2DArray<float2> texDisplayTexDepthArray : register(t4);
	Texture2DArray<uint2> texDisplayTexStencilArray : register(t5);
	Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(t6);
	Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(t7);
	Texture2DMSArray<float4> texDisplayTex2DMSArray : register(t9);
	Texture2DArray<float4> texDisplayYUVArray : register(t10);

	Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);
	Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);
	Texture3D<uint4> texDisplayUIntTex3D : register(t13);
	Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(t19);

	Texture1DArray<int4> texDisplayIntTex1DArray : register(t21);
	Texture2DArray<int4> texDisplayIntTex2DArray : register(t22);
	Texture3D<int4> texDisplayIntTex3D : register(t23);
	Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(t29);

OpenGL / GLSL
^^^^^^^^^^^^^

.. highlight:: c++
.. code:: c++

	// Unsigned int samplers
	layout (binding = 1) uniform usampler1D texUInt1D;
	layout (binding = 2) uniform usampler2D texUInt2D;
	layout (binding = 3) uniform usampler3D texUInt3D;
	// skip cube = 4
	layout (binding = 5) uniform usampler1DArray texUInt1DArray;
	layout (binding = 6) uniform usampler2DArray texUInt2DArray;
	// skip cube array = 7
	layout (binding = 8) uniform usampler2DRect texUInt2DRect;
	layout (binding = 9) uniform usamplerBuffer texUIntBuffer;
	layout (binding = 10) uniform usampler2DMS texUInt2DMS;

	// Int samplers
	layout (binding = 1) uniform isampler1D texSInt1D;
	layout (binding = 2) uniform isampler2D texSInt2D;
	layout (binding = 3) uniform isampler3D texSInt3D;
	// skip cube = 4
	layout (binding = 5) uniform isampler1DArray texSInt1DArray;
	layout (binding = 6) uniform isampler2DArray texSInt2DArray;
	// skip cube array = 7
	layout (binding = 8) uniform isampler2DRect texSInt2DRect;
	layout (binding = 9) uniform isamplerBuffer texSIntBuffer;
	layout (binding = 10) uniform isampler2DMS texSInt2DMS;

	// Floating point samplers
	layout (binding = 1) uniform sampler1D tex1D;
	layout (binding = 2) uniform sampler2D tex2D;
	layout (binding = 3) uniform sampler3D tex3D;
	layout (binding = 4) uniform samplerCube texCube;
	layout (binding = 5) uniform sampler1DArray tex1DArray;
	layout (binding = 6) uniform sampler2DArray tex2DArray;
	layout (binding = 7) uniform samplerCubeArray texCubeArray;
	layout (binding = 8) uniform sampler2DRect tex2DRect;
	layout (binding = 9) uniform samplerBuffer texBuffer;
	layout (binding = 10) uniform sampler2DMS tex2DMS;

Vulkan / GLSL
^^^^^^^^^^^^^

.. highlight:: c++
.. code:: c++

	// Floating point samplers

	// binding = 5 + RENDERDOC_TextureType
	layout(binding = 6) uniform sampler1DArray tex1DArray;
	layout(binding = 7) uniform sampler2DArray tex2DArray;
	layout(binding = 8) uniform sampler3D tex3D;
	layout(binding = 9) uniform sampler2DMS tex2DMS;
	layout(binding = 10) uniform sampler2DArray texYUV;

	// Unsigned int samplers

	// binding = 10 + RENDERDOC_TextureType
	layout(binding = 11) uniform usampler1DArray texUInt1DArray;
	layout(binding = 12) uniform usampler2DArray texUInt2DArray;
	layout(binding = 13) uniform usampler3D texUInt3D;
	layout(binding = 14) uniform usampler2DMS texUInt2DMS;

	// Int samplers

	// binding = 15 + RENDERDOC_TextureType
	layout(binding = 16) uniform isampler1DArray texSInt1DArray;
	layout(binding = 17) uniform isampler2DArray texSInt2DArray;
	layout(binding = 18) uniform isampler3D texSInt3D;
	layout(binding = 19) uniform isampler2DMS texSInt2DMS;


These resources are bound sparsely with the appropriate type for the current texture. With a couple of exceptions there will only be one texture bound at any one time.

When a cubemap texture is bound, it is bound both to the 2D Array as well as the Cube Array. If a depth-stencil texture has both components, the relevant depth and stencil resources will both be bound at once.

To determine which resource to sample from you can use the ``RENDERDOC_TexType`` variable above.

Usually the float textures are used, but for unsigned and signed integer formats, the relevant integer resources are used.

As with the samplers, these textures are bound by slot and not by name, so while you are free to name the variables as you wish, you must bind them explicitly to the slots listed here.

.. note::
  YUV textures may have additional planes bound as separate textures - for D3D this is ``texDisplayYUVArray`` and for Vulkan it's ``texYUV`` above. Whether to use these planes or not is specified in the texture dimension variables.

See Also
--------

* :doc:`../window/texture_viewer`
