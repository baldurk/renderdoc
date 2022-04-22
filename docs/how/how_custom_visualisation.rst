How do I use a custom visualisation shader?
===========================================

This page details how to set up a custom shader for visualisation. This can be used in the :doc:`../window/texture_viewer` to unpack or decode complex formats, or simply apply a custom complex transformation to the image beyond the default controls.

Introduction
------------

The basic process of setting up the custom shader involves writing a shader file that will be compiled and used by RenderDoc. Note that you can use any language that is either natively accepted by the graphics API used, or any language that can be compiled to an accepted shader.

For example on D3D11 or D3D12, hlsl is the only language usable by default. On OpenGL only glsl can be used, but on Vulkan you can use glsl or use hlsl as long as a compiler is available.

There are several special global helpers that can be declared and used, which will be implemented and return values by RenderDoc. In addition there are automatic macros that can be used to bind resources and still write shaders which work on different APIs with different bindings.

Your pixel shader defines an operation that transforms the raw value from the input texture into a value that will then be displayed by the texture viewer. The usual texture viewer controls for range adaption and channels will still be available on the resulting texture.

To set up your shader, it's recommended that you use the UI defined in the documentation for the :doc:`../window/texture_viewer`, but you can manually create a ``.hlsl`` or ``.glsl`` file in the application storage directory ( ``%APPDATA%/qrenderdoc/`` on windows or ``~/.local/share/qrenderdoc`` elsewhere). The file must contain an entry point ``main()`` that returns ``float4``, and uses any of the below inputs. These shaders are loaded when RenderDoc loads a capture, and RenderDoc watches for any changes to the files (either externally or in the shader editor in RenderDoc) and automatically reloads them.

.. note::

  Since ``.glsl`` is used for both Vulkan and OpenGL shaders, you can differentiate by the pre-defined macro ``VULKAN`` if you are writing a shader to be used for both.

.. warning::

  Previously the custom shaders allowed more direct binding without helper functions or binding macros. These shaders will continue to work as backwards compatibility is maintained, however be aware that these bindings are API specific and so e.g. a shader written for OpenGL in glsl will not work on Vulkan unless care has been taken, or an HLSL shader between Vulkan and D3D. If portability is desired please update and use the new helpers and binding macros, or else be careful only to use custom shaders with the API they were written for.

.. note::

  See the custom shader templates available in the  `contrib repository <https://github.com/baldurk/renderdoc-contrib/tree/main/baldurk/custom-shader-templates>`__ for complete examples.

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

There are several constant parameters available, each available via a helper function. They are detailed below with the values they contain.

Texture dimensions
~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

  uint4 RD_TexDim(); // hlsl
  uvec4 RD_TexDim(); // glsl

  uint4 RD_YUVDownsampleRate(); // hlsl
  uvec4 RD_YUVDownsampleRate(); // vulkan glsl only
  uint4 RD_YUVAChannels(); // hlsl
  uvec4 RD_YUVAChannels(); // vulkan glsl only


``RD_TexDim`` will return the following values:

* ``.x``  Width
* ``.y``  Height (if 2D or 3D)
* ``.z``  Depth if 3D or array size if an array
* ``.w``  Number of mip levels


``RD_YUVDownsampleRate`` will return the following values:

* ``.x``  Horizontal downsample rate. 1 for equal luma and chroma width, 2 for half rate.
* ``.y``  Vertical downsample rate. 1 for equal luma and chroma height, 2 for half rate.
* ``.z``  Number of planes in the input texture, 1 for packed, 2+ for planar
* ``.w``  Number of bits per component, e.g. 8, 10 or 16.


``RD_YUVAChannels`` will return an index indicating where each channel comes from in the source textures. The order is ``.x`` for ``Y``, ``.y`` for ``U``, ``.z`` for ``V`` and ``.w`` for ``A``.

The indices for channels in the first texture in the normal 2D slot are ``0, 1, 2, 3``. Indices from ``4`` to ``7`` indicate channels in the second texture, and so on.

If a channel is not present, e.g. alpha is commonly not available, it will be set to ``0xff == 255``.

Selected Mip level
~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

  uint RD_SelectedMip(); // hlsl or glsl


This will return the selected mip level in the UI.

Selected Slice/Face
~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

  uint RD_SelectedSliceFace(); // hlsl or glsl


This variable will be filled out with the selected texture array slice (or cubemap face) in the UI.

Selected Multisample sample
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

  int RD_SelectedSample(); // hlsl or glsl


This variable will be filled out with the selected multisample sample index as chosen in the UI. If the UI has 'average value' selected, this variable will be negative and with an absolute value equal to the number of samples.

So for example in a 4x MSAA texture, the valid values are ``0``, ``1``, ``2``, ``3`` to select a sample, or ``-4`` for 'average value'.


Selected RangeMin, RangeMax
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

  float2 RD_SelectedRange(); // hlsl
  vec2 RD_SelectedRange(); // glsl


This function will return a pair, with the current Minimum and Maximum values for the Range-selector in the Texture Viewer.


Current texture type
~~~~~~~~~~~~~~~~~~~~

.. highlight:: c++
.. code:: c++

  uint RD_TextureType(); // hlsl or glsl


This variable will be set to a given integer value, depending on the type of the current texture being displayed. This can be used to sample from the correct resource.

.. note::

  The value varies depending on the API this shader will be used for, as each has different resource bindings. You should use the defines below to check, which will be portable across APIs

D3D11 or D3D12
^^^^^^^^^^^^^^

* ``RD_TextureType_1D`` - 1D texture
* ``RD_TextureType_2D`` - 2D texture
* ``RD_TextureType_3D`` - 3D texture
* ``RD_TextureType_Depth`` - Depth
* ``RD_TextureType_DepthStencil`` - Depth + Stencil
* ``RD_TextureType_DepthMS`` - Depth (Multisampled)
* ``RD_TextureType_DepthStencilMS`` - Depth + Stencil (Multisampled)
* ``RD_TextureType_2DMS`` - 2D texture (Multisampled)

In all cases on D3D the bindings can be used for arrays or not, interchangeably.

OpenGL
^^^^^^

* ``RD_TextureType_1D`` - 1D texture
* ``RD_TextureType_2D`` - 2D texture
* ``RD_TextureType_3D`` - 3D texture
* ``RD_TextureType_Cube`` - Cubemap
* ``RD_TextureType_1D_Array`` - 1D array texture
* ``RD_TextureType_2D_Array`` - 2D array texture
* ``RD_TextureType_Cube_Array`` - Cube array texture
* ``RD_TextureType_Rect`` - Rectangle texture
* ``RD_TextureType_Buffer`` - Buffer texture
* ``RD_TextureType_2DMS`` - 2D texture (Multisampled)
* ``RD_TextureType_2DMS_Array`` - 2D array texture (Multisampled)

OpenGL has separate types and bindings for arrayed and non-arrayed textures.

Vulkan
^^^^^^

* ``RD_TextureType_1D`` - 1D texture
* ``RD_TextureType_2D`` - 2D texture
* ``RD_TextureType_3D`` - 3D texture
* ``RD_TextureType_2DMS`` - 2D texture (Multisampled)

In all cases on Vulkan the bindings can be used for arrays or not, interchangeably.

Samplers
~~~~~~~~

.. highlight:: c++
.. code:: c++


  /* HLSL */
  SamplerState pointSampler : register(RD_POINT_SAMPLER_BINDING);
  SamplerState linearSampler : register(RD_LINEAR_SAMPLER_BINDING);


  /* GLSL */
  #ifdef VULKAN

  layout(binding = RD_POINT_SAMPLER_BINDING) uniform sampler pointSampler;
  layout(binding = RD_LINEAR_SAMPLER_BINDING) uniform sampler linearSampler;

  #endif

These samplers are provided to allow you to sample from the resource as opposed to doing straight loads. Samplers are not available on OpenGL, so it is recommended to protect the glsl definitions with ``#ifdef VULKAN`` as shown.

Resources
~~~~~~~~~

HLSL
^^^^

.. highlight:: c++
.. code:: c++

  // Float Textures
  Texture1DArray<float4> texDisplayTex1DArray : register(RD_FLOAT_1D_ARRAY_BINDING);
  Texture2DArray<float4> texDisplayTex2DArray : register(RD_FLOAT_2D_ARRAY_BINDING);
  Texture3D<float4> texDisplayTex3D : register(RD_FLOAT_3D_BINDING);
  Texture2DMSArray<float4> texDisplayTex2DMSArray : register(RD_FLOAT_2DMS_ARRAY_BINDING);
  Texture2DArray<float4> texDisplayYUVArray : register(RD_FLOAT_YUV_ARRAY_BINDING);

  // only used on D3D
  Texture2DArray<float2> texDisplayTexDepthArray : register(RD_FLOAT_DEPTH_ARRAY_BINDING);
  Texture2DArray<uint2> texDisplayTexStencilArray : register(RD_FLOAT_STENCIL_ARRAY_BINDING);
  Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(RD_FLOAT_DEPTHMS_ARRAY_BINDING);
  Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(RD_FLOAT_STENCILMS_ARRAY_BINDING);

  // Int Textures
  Texture1DArray<int4> texDisplayIntTex1DArray : register(RD_INT_1D_ARRAY_BINDING);
  Texture2DArray<int4> texDisplayIntTex2DArray : register(RD_INT_2D_ARRAY_BINDING);
  Texture3D<int4> texDisplayIntTex3D : register(RD_INT_3D_BINDING);
  Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(RD_INT_2DMS_ARRAY_BINDING);

  // Unsigned int Textures
  Texture1DArray<uint4> texDisplayUIntTex1DArray : register(RD_UINT_1D_ARRAY_BINDING);
  Texture2DArray<uint4> texDisplayUIntTex2DArray : register(RD_UINT_2D_ARRAY_BINDING);
  Texture3D<uint4> texDisplayUIntTex3D : register(RD_UINT_3D_BINDING);
  Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(RD_UINT_2DMS_ARRAY_BINDING);

GLSL
^^^^

.. highlight:: c++
.. code:: c++

  // Float Textures
  layout (binding = RD_FLOAT_1D_ARRAY_BINDING) uniform sampler1DArray tex1DArray;
  layout (binding = RD_FLOAT_2D_ARRAY_BINDING) uniform sampler2DArray tex2DArray;
  layout (binding = RD_FLOAT_3D_BINDING) uniform sampler3D tex3D;
  layout (binding = RD_FLOAT_2DMS_ARRAY_BINDING) uniform sampler2DMSArray tex2DMSArray;

  // YUV textures only supported on vulkan
  #ifdef VULKAN
  layout(binding = RD_FLOAT_YUV_ARRAY_BINDING) uniform sampler2DArray texYUVArray[2];
  #endif

  // OpenGL has more texture types to match
  #ifndef VULKAN
  layout (binding = RD_FLOAT_1D_BINDING) uniform sampler1D tex1D;
  layout (binding = RD_FLOAT_2D_BINDING) uniform sampler2D tex2D;
  layout (binding = RD_FLOAT_CUBE_BINDING) uniform samplerCube texCube;
  layout (binding = RD_FLOAT_CUBE_ARRAY_BINDING) uniform samplerCubeArray texCubeArray;
  layout (binding = RD_FLOAT_RECT_BINDING) uniform sampler2DRect tex2DRect;
  layout (binding = RD_FLOAT_BUFFER_BINDING) uniform samplerBuffer texBuffer;
  layout (binding = RD_FLOAT_2DMS_BINDING) uniform sampler2DMS tex2DMS;
  #endif

  // Int Textures
  layout (binding = RD_INT_1D_ARRAY_BINDING) uniform isampler1DArray texSInt1DArray;
  layout (binding = RD_INT_2D_ARRAY_BINDING) uniform isampler2DArray texSInt2DArray;
  layout (binding = RD_INT_3D_BINDING) uniform isampler3D texSInt3D;
  layout (binding = RD_INT_2DMS_ARRAY_BINDING) uniform isampler2DMSArray texSInt2DMSArray;

  #ifndef VULKAN
  layout (binding = RD_INT_1D_BINDING) uniform isampler1D texSInt1D;
  layout (binding = RD_INT_2D_BINDING) uniform isampler2D texSInt2D;
  layout (binding = RD_INT_RECT_BINDING) uniform isampler2DRect texSInt2DRect;
  layout (binding = RD_INT_BUFFER_BINDING) uniform isamplerBuffer texSIntBuffer;
  layout (binding = RD_INT_2DMS_BINDING) uniform isampler2DMS texSInt2DMS;
  #endif

  // Unsigned int Textures
  layout (binding = RD_UINT_1D_ARRAY_BINDING) uniform usampler1DArray texUInt1DArray;
  layout (binding = RD_UINT_2D_ARRAY_BINDING) uniform usampler2DArray texUInt2DArray;
  layout (binding = RD_UINT_3D_BINDING) uniform usampler3D texUInt3D;
  layout (binding = RD_UINT_2DMS_ARRAY_BINDING) uniform usampler2DMSArray texUInt2DMSArray;

  #ifndef VULKAN
  layout (binding = RD_UINT_1D_BINDING) uniform usampler1D texUInt1D;
  layout (binding = RD_UINT_2D_BINDING) uniform usampler2D texUInt2D;
  layout (binding = RD_UINT_RECT_BINDING) uniform usampler2DRect texUInt2DRect;
  layout (binding = RD_UINT_BUFFER_BINDING) uniform usamplerBuffer texUIntBuffer;
  layout (binding = RD_UINT_2DMS_BINDING) uniform usampler2DMS texUInt2DMS;
  #endif


These resources are bound sparsely with the appropriate type for the current texture. With a couple of exceptions there will only be one texture bound at any one time. Different APIs have different texture type matching requirements, so e.g. OpenGL has separate bindings for array and non-array texures, which will be reflected in the different ``RD_TextureType`` return values.

When a cubemap texture is bound, it is bound both to the 2D Array as well as the Cube Array. If a depth-stencil texture has both components, the relevant depth and stencil resources will both be bound at once.

To determine which resource to sample from you can use the ``RENDERDOC_TexType`` variable above.

Usually the float textures are used, but for unsigned and signed integer formats, the relevant integer resources are used.

As with the samplers, these textures are bound by slot and not by name, so while you are free to name the variables as you wish, you must bind them explicitly to the slots listed here.

.. note::
  YUV textures may have additional planes bound as separate textures - for D3D this is ``texDisplayYUVArray`` and for Vulkan it's ``texYUVArray`` above. Whether to use these planes or not is specified in the texture dimension variables.

See Also
--------

* :doc:`../window/texture_viewer`
