Shaders
=======

.. contents::

.. currentmodule:: renderdoc

Descriptors
-----------

.. autoclass:: renderdoc.Descriptor
  :members:

.. autoclass:: renderdoc.SamplerDescriptor
  :members:

.. autoclass:: renderdoc.DescriptorFlags
  :members:
   
.. autoclass:: renderdoc.DescriptorCategory
  :members:
   
.. autoclass:: renderdoc.DescriptorType
  :members:
   
.. autofunction:: renderdoc.CategoryForDescriptorType
.. autofunction:: renderdoc.IsConstantBlockDescriptor
.. autofunction:: renderdoc.IsReadOnlyDescriptor
.. autofunction:: renderdoc.IsReadWriteDescriptor
.. autofunction:: renderdoc.IsSamplerDescriptor

.. autoclass:: renderdoc.DescriptorLogicalLocation
  :members:

.. autoclass:: renderdoc.DescriptorRange
  :members:

.. autoclass:: renderdoc.DescriptorAccess
  :members:

Reflection
----------

.. autoclass:: renderdoc.ShaderReflection
  :members:

.. autoclass:: renderdoc.ShaderStage
  :members:

.. autoclass:: renderdoc.ShaderStageMask
  :members:

.. autofunction:: renderdoc.MaskForStage
.. autofunction:: renderdoc.FirstStageForMask

.. autoclass:: renderdoc.SigParameter
  :members:

.. autoclass:: renderdoc.ShaderBuiltin
  :members:

.. autoclass:: renderdoc.ConstantBlock
  :members:

.. autoclass:: renderdoc.ShaderSampler
  :members:

.. autoclass:: renderdoc.ShaderResource
  :members:

Debug Info
----------

.. autoclass:: renderdoc.ShaderDebugInfo
  :members:

.. autoclass:: renderdoc.ShaderEncoding
  :members:
  
.. autoclass:: renderdoc.KnownShaderTool
  :members:
  
.. autofunction:: renderdoc.ToolExecutable
.. autofunction:: renderdoc.ToolInput
.. autofunction:: renderdoc.ToolOutput

.. autofunction:: renderdoc.IsTextRepresentation

.. autoclass:: renderdoc.ShaderEntryPoint
  :members:

.. autoclass:: renderdoc.ShaderSourceFile
  :members:

.. autoclass:: renderdoc.ShaderCompileFlags
  :members:

.. autoclass:: renderdoc.ShaderCompileFlag
  :members:

.. autoclass:: renderdoc.ShaderSourcePrefix
  :members:

Shader Constants
----------------

.. autoclass:: renderdoc.ShaderConstant
  :members:

.. autoclass:: renderdoc.ShaderConstantType
  :members:

.. autoclass:: renderdoc.ShaderVariableFlags
  :members:

.. autoclass:: renderdoc.VarType
  :members:

.. autofunction:: renderdoc.VarTypeByteSize
.. autofunction:: renderdoc.VarTypeCompType

Shader Debugging
----------------

.. autoclass:: renderdoc.ShaderDebugTrace
  :members:

.. autoclass:: renderdoc.ShaderDebugger
  :members:

.. autoclass:: renderdoc.SourceVariableMapping
  :members:

.. autoclass:: renderdoc.DebugVariableReference
  :members:

.. autoclass:: renderdoc.DebugVariableType
  :members:

.. autoclass:: renderdoc.LineColumnInfo
  :members:

.. autoclass:: renderdoc.InstructionSourceInfo
  :members:

.. autoclass:: renderdoc.ShaderDebugState
  :members:

.. autoclass:: renderdoc.ShaderEvents
  :members:

.. autoclass:: renderdoc.ShaderVariableChange
  :members:

Shader Variables
----------------
  
.. autoclass:: renderdoc.ShaderVariable
  :members:

.. autoclass:: renderdoc.ShaderValue
  :members:

.. autoclass:: renderdoc.PointerVal
  :members:

.. autoclass:: renderdoc.ShaderBindIndex
  :members:
