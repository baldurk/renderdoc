Common Pipeline State Abstraction
=================================

.. contents::

.. currentmodule:: renderdoc

.. autoclass:: PipeState
  :members:

General
-------

.. autoclass:: renderdoc.Offset
  :members:

Vertex Inputs
-------------

.. autoclass:: renderdoc.BoundVBuffer
  :members:

.. autoclass:: renderdoc.VertexInputAttribute
  :members:

.. autoclass:: renderdoc.Topology
  :members:
  
.. autofunction:: renderdoc.NumVerticesPerPrimitive
.. autofunction:: renderdoc.VertexOffset
.. autofunction:: renderdoc.PatchList_Count
.. autofunction:: renderdoc.PatchList_Topology
.. autofunction:: renderdoc.IsStrip

Shader Resource Bindings
------------------------

.. autoclass:: renderdoc.UsedDescriptor
  :members:

.. autoclass:: renderdoc.BindType
  :members:

.. autoclass:: renderdoc.TextureSwizzle
  :members:

.. autoclass:: renderdoc.TextureSwizzle4
  :members:

Samplers
--------

.. autoclass:: renderdoc.AddressMode
  :members:

.. autoclass:: renderdoc.TextureFilter
  :members:

.. autoclass:: renderdoc.FilterMode
  :members:
  
.. autoclass:: renderdoc.FilterFunction
  :members:

.. autoclass:: renderdoc.ChromaSampleLocation
  :members:

.. autoclass:: renderdoc.YcbcrConversion
  :members:

.. autoclass:: renderdoc.YcbcrRange
  :members:

Viewport and Scissor
--------------------

.. autoclass:: renderdoc.Viewport
  :members:

.. autoclass:: renderdoc.Scissor
  :members:

Rasterizer
----------

.. autoclass:: renderdoc.CullMode
  :members:

.. autoclass:: renderdoc.FillMode
  :members:

.. autoclass:: renderdoc.ConservativeRaster
  :members:

.. autoclass:: renderdoc.LineRaster
  :members:

.. autoclass:: renderdoc.ShadingRateCombiner
  :members:


Stencil
-------

.. autoclass:: renderdoc.StencilFace
  :members:

.. autoclass:: renderdoc.StencilOperation
  :members:

.. autoclass:: renderdoc.CompareFunction
  :members:

Blending
--------

.. autoclass:: renderdoc.ColorBlend
  :members:

.. autoclass:: renderdoc.BlendEquation
  :members:

.. autoclass:: renderdoc.BlendMultiplier
  :members:

.. autoclass:: renderdoc.BlendOperation
  :members:

.. autoclass:: renderdoc.LogicOperation
  :members:

Shader Messages
---------------

.. autoclass:: renderdoc.ShaderMessage
  :members:

.. autoclass:: renderdoc.ShaderMessageLocation
  :members:

.. autoclass:: renderdoc.ShaderMeshMessageLocation
  :members:

.. autoclass:: renderdoc.ShaderVertexMessageLocation
  :members:

.. autoclass:: renderdoc.ShaderPixelMessageLocation
  :members:

.. autoclass:: renderdoc.ShaderGeometryMessageLocation
  :members:

.. autoclass:: renderdoc.ShaderComputeMessageLocation
  :members:


* qrenderdoc.ShaderMessageViewer
