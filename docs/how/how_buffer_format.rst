How do I specify a buffer format?
=================================

This page documents how to format buffer data, in cases where the default reflected format is missing or you want to customise it.

The format string can contain C and C++ style comments freely, but a C pre-processor is not supported.

By default the final interpreted format is defined by the list of global variables in the layout string, however if no global variables are defined the final struct to be defined is used as-if there were a single variable instance of that struct.

.. code:: c++

  struct data
  {
    float a;
    int b;
  };

Is equivalent to

.. code:: c++

  struct data
  {
    float a;
    int b;
  };

  data d;

However note that if there are global variables, structs will **not** automatically be instantiated and must be declared as variables.

Basic types
-----------

Variables can be declared using familiar syntax from either GLSL or HLSL. A float would be declared as:

.. code:: c++

  float myvalue;

Most common basic types are available:

* ``bool`` - A 4-byte boolean.
* ``byte``, ``short``, ``int``, ``long`` - 1, 2, 4, and 8 byte signed integers respectively.
* ``ubyte``, ``ushort``, ``uint``, ``ulong`` - 1, 2, 4, and 8 byte unsigned integers respectively. It is also possible to prefix ``unsigned`` before a signed type.
* ``half``, ``float``, ``double`` - 2, 4, and 8 byte floating point values.

These can be declared as vectors by appending the vector width. Similarly matrices can be declared by appending the vector and matrix dimensions.

GLSL declarations for float, int, and uint vectors and matrices are supported too.

.. code:: c++

  float2 myvector2; // 2-vector of floats
  float4 myvector4;
  float2x2 mymat;   // 2x2 matrix
  mat2 myGLmat;     // equivalent to float2x2
  vec2 myGLvec;     // equivalent to float2

You can also declare any of these types as an array. Arrays can either have a fixed size, or an unbounded size - unbounded arrays are :ref:`detailed below in the discussion of AoS vs SoA data <aos-soa>`.

.. code:: c++

  float fixedArray[2];    // array of two floats
  float3 unboundedArray[]; // unbounded array of float3s

Structs
-------

Structs are declared similarly to in GLSL or HLSL, or in C.

.. code:: c++

  struct MyStructName {
    float a;
    int b;
  };

  MyStructName str; // single instance of the struct
  MyStructName arr[4]; // array of 4 instances

Structs can be nested freely, but cannot be forward declared so a struct can only reference structs defined before it.

Enums
-----

Enums can be defined but must be defined using a base integer type to declare their size. Enum values must be literal integers, either in decimal or hexadecimal.

Values must be explicitly given, and automatic numbering or expression-based values are not supported.

.. code:: c++

  enum MyEnum : uint {
    FirstValue = 5,
    HexValue = 0xf,
  };

  MyEnum e; // A uint will be read and interpreted as the above enum

Bitfields
---------

Integer values can be bit-packed together using C style bitfields.

.. code:: c++

  int first : 3;
  int second : 5;
  int third : 10;
  int : 6; // anonymous values can be used to skip bits without declaring anything
  int last : 8;

This declaration will read only a single 32-bit integer, and interpret the bits according to this packing.

Pointers
--------

On APIs where GPU pointers can reside within memory, such as Vulkan, pointers can be declared with a base struct type and these will be read and interpreted from a 64-bit address in the underlying buffer.

.. code:: c++

  struct MyStructName {
    float a;
    int b;
  };

  MyStructName *pointer;

Packing and layout rules
------------------------

Graphics APIs define different rules for how data should be packed into memory, and this sometimes depends on the usage of the buffer within the API.

RenderDoc will use the most sensible default where possible - e.g. for D3D buffers that are known to be bound as constant buffers the constant buffer packing will be used, similarly for OpenGL uniform buffers using std140. However the packing can be explicitly specified and any automatic reflection-based format will declare the packing explicitly.

Once a packing format is specified, RenderDoc will calculate the necessary alignment and padding for each element to comply with the rules while otherwise tightly packing, the same as a normal shader declaration would.

The format for a buffer can be specified using ``#pack(packing_format)``. This can only be specified at global scope, not inside a structure, and the packing rules will apply for all subsequent declarations.

The five packing formats supported are:

* ``cbuffer``, ``d3dcbuffer`` or ``cb`` - D3D constant buffer packing.
* ``structured``, ``d3duav``, or ``uav`` - D3D structured buffer packing (applies to buffer SRVs as well as UAVs).
* ``std140``, ``ubo``, or ``gl`` - OpenGL std140 uniform buffer packing.
* ``std430``, ``ssbo`` - OpenGL std430 storage buffer packing.
* ``scalar`` - Vulkan scalar buffer packing.

It is also possible to tweak particular packing properties with ``#pack()``. Each property can be enabled or disabled by ``#pack(prop)`` or ``#pack(no_prop)``. Each property will *relax* some restrictions, so the strictest possible packing is ``std140`` with all properties off, and the most lax packing is ``scalar`` with all properties on.

The available packing properties are:

* ``vector_align_component`` - If enabled, vectors are only aligned to their component. If disabled, 2-vectors are aligned to 2x their component, 3-vectors and 4-vectors are aligned to 4x their components. This is disabled only for ``std140`` and ``std430`` by default.
* ``vector_straddle_16b`` - If enabled, vectors are allowed to straddle 16-byte alignment boundaries. If disabled, vectors must be padded/aligned to not straddle. This is disabled only for ``std140``, ``std430``, and ``cbuffer`` by default.
* ``tight_arrays`` - If enabled, arrays elements are only aligned to the element size. If disabled, each array element is aligned to a 16-byte boundary. This is disabled for ``std140`` and ``cbuffer`` by default.
* ``trailing_overlap`` - If enabled, elements can be placed in trailing padding from a previous element such as an array or struct. If disabled, each element's padding is reserved and the next element must come after the padding. This disabled for ``std140``, ``std430``, and ``structured`` by default.

Some additional properties are available to go beyond the normal packing rules:

* ``tight_bitfield_packing`` - If enabled, bitfields consume bits tightly packed no matter what their base type or offset. This would allow a ``uint`` bitfield to be placed at 30 bits into a buffer and span more than 2 bits, crossing multiple uints. If disabled, padding is added to ensure each bitfield member remains within an instance of its base type. This is disabled by default, and is equivalent to ``#pragma pack(1)`` behaviour.

Annotations
-----------

The buffer format supports annotations on declarations to specify special properties. These use C++ ``[[annotation(parameter)]]`` syntax.

Struct definitions support the following annotations:

* ``[[size(number)]]`` or ``[[byte_size(number)]]`` - Forces the struct to be padded up to a given size even if the contents don't require it.
* ``[[single]]`` or ``[[fixed]]`` - Forces the struct to be considered as a fixed SoA definition, even if in context the buffer viewer may default to AoS. See :ref:`the below section <aos-soa>` for more details. Structs with this annotation **may not** be declared as a variable, and should instead be the implicit final struct in a definition.

Variable declarations support the following annotations:

* ``[[offset(number)]]`` or ``[[byte_offset(number)]]`` - Forces this member to be at a given offset **relative to its parent**. This cannot place the member any earlier than it would have according to tight packing with the current packing rules.
* ``[[pad]]`` or ``[[padding]]`` - Mark this member as padding, such that structure layout is calculated accounting for it but it is not displayed visibly.
* ``[[single]]`` or ``[[fixed]]`` - Forces this variable to be considered as a fixed SoA definition, even if in context the buffer viewer may default to AoS. See :ref:`the below section <aos-soa>` for more details. This must be a global variable, and it must be the only global variable in the format definition.
* ``[[row_major]]`` or ``[[col_major]]`` - Declares the memory order for a matrix.
* ``[[rgb]]`` - Will color the background of any repeated data by interpreting its contents as RGB color.
* ``[[hex]]`` or ``[[hexadecimal]]`` - Will show integer data as hexadecimal.
* ``[[bin]]`` or ``[[binary]]`` - Will show integer data as binary.
* ``[[unorm]]`` or ``[[snorm]]`` - On 1-byte or 2-byte integer variables, will interpret them as unsigned or signed normalised data respectively.
* ``[[packed(format)]]`` - Interprets a variable according to a standard bit-packed format. Supported formats are:

   * ``r11g11b10`` which must be used with a ``float3`` type.
   * ``r10g10b10a2`` or ``r10g10b10a2_uint`` which must be used with a ``uint4`` type. Can optionally be combined with ``[[unorm]]`` or ``[[snorm]]``.
   * ``r10g10b10a2_unorm`` which must be used with a ``uint4`` type.
   * ``r10g10b10a2_snorm`` which must be used with a ``int4`` type.

.. _aos-soa:

Array of Structs (AoS) vs Struct of Arrays (SoA)
------------------------------------------------

The :doc:`../window/buffer_viewer` is capable of displaying both repeating data of a single format (AoS) as well as fixed non-repeating data (called SoA). Typically AoS is used for large buffers, where a small struct is repeated many times to form the elemnts in the buffer. SoA is used most commonly for constant buffers with a fixed amount of data, but can be used in any context. On some APIs it is possible for a buffer to contain some fixed data before the repeating data and thus it contains both types.

RenderDoc tries to use context to interpret buffer formats correctly, defaulting to AoS interpretation in cases where it is likely intended. However this can be hinted or overridden as desired.

To specify AoS data explicitly you can declare an unbounded array:

.. code:: c++

  float3 unboundedArray[]; // unbounded array of float3s

When supported by the API, this can be preceeded by any fixed data in the buffer before the repeated AoS data. The buffer viewer will show both parts of the data separately, with a tree view for the fixed data and a table for the repeated data.

In the opposite direction, normally a loose collection of variables without any such unbounded array will be taken as the definition of a struct within an AoS view:

.. code:: c++

  struct data
  {
    float a;
    int2 b;
    float c;
  };

However if the desire is to display this as a single fixed element where the fixed tree view is more appropriate, the structure or an variable of it can be annotated as ``[[single]]`` or ``[[fixed]]``.

.. code:: c++

  [[single]]
  struct data
  {
    float a;
    int2 b;
    float c;
  };

.. code:: c++

  struct data
  {
    float a;
    int2 b;
    float c;
  };

  [[single]]
  data fixed_data;

This will force the struct to be displayed as a single instance, and not as a repeated AoS.

Saving and loading formats
--------------------------

Commonly used formats can be saved and these will be persisted from run to run.

.. |save| image:: ../imgs/icons/save.png

.. |goarrow| image:: ../imgs/icons/action_hover.png

To save the current format to an existing entry, select it and click on the |save| button.

To save to a new entry, either type the name directly into the ``New...`` at the bottom, double click on it to begin entering the name, or select it and click save then enter the name.

If the buffer view was opened with an automatically populated format, it will be available as a read-only ``<Auto-generated>`` entry.

Loading an entry can be accomplished by either double clicking on it, or selecting it and clicking |goarrow|. This will load the format and automatically apply it to the buffer view.

You can use undo/redo to undo the loading of a saved format, if you wish to go back to the previous format.

See Also
--------

* :doc:`../window/buffer_viewer`
