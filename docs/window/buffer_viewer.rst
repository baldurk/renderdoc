Raw Buffer Viewer
=================

When opening a buffer as a raw display, sometimes a default layout will be specified e.g. if available from shader reflection data. If not, the layout will default to 4 32bit unsigned integers.

This format can be refined and customised by entering a structure-like definition into the text box at the bottom of the window. The given types are listed below, and can be combined in hlsl- or glsl-like fashion specifying n-wide vector elements.

In addition to this, you can specify a row offset which is useful in remaining at the same row while watching the change in a buffer between different events, as well as a byte offset to shift the data along from the start of the buffer (e.g. if what you are interested in starts only part-way through the buffer but is not aligned along the data stride you enter).

.. figure:: ../imgs/Screenshots/RawBuffer.png

	Buffer specification: Specifying a custom buffer format.

Below are listed the basic types. You can append a number to each of these to make an N-wide vector (e.g. ``ushort4`` or ``float3``, or ``uvec4``/``vec3``). You can also specify matrices as ``float3x4`` or ``mat3x4``. By default matrices are column major, but you can change this by prepending ``row_major`` as you would in hlsl.

* ``uint`` - unsigned 32bit integer
* ``bool`` - unsigned 32bit integer (this is the format for hlsl bools)
* ``int`` - signed 32bit integer
* ``ushort`` - unsigned 16bit integer
* ``short`` - signed 16bit integer
* ``ubyte`` - unsigned 8bit integer
* ``byte`` - signed 8bit integer
* ``double`` - 64bit floating point
* ``float`` - 32bit floating point
* ``half`` - 16bit floating point

There are also some non-hlsl types for displaying other formats which don't have a corresponding native hlsl type

* ``unormb`` - 8bit unsigned normalised value
* ``unormh`` - 16bit unsigned normalised value
* ``unormf`` - 32bit unsigned normalised value
* ``snormb`` - 8bit signed normalised value
* ``snormh`` - 16bit signed normalised value
* ``snormf`` - 32bit signed normalised value
* ``uintten`` - 4 component unsigned integer format, packed as 10:10:10:2
* ``unormten`` - 4 component unsigned normalised format, packed as 10:10:10:2
* ``floateleven`` - 3 component floating point format, packed as 11:11:10
* ``xint`` - hex-formatted 32bit integer
* ``xshort`` - hex-formatted 16bit integer
* ``xbyte`` - hex-formatted 8bit integer
