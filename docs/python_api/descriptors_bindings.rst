Descriptors and Bindings
========================

.. _descriptor-abstraction:

Access to descriptors or fixed resource bindings is an area of graphics APIs that varies significantly between each API. RenderDoc provides an abstracted interface which allows most code to be written in an API-agnostic way to determine what resources are used and their properties.

Some allowances are provided for looking up API-specific information or interpreting the bindings with an API-specific lens if the code knows which API it is being used with.

Overview
--------

RenderDoc's abstraction is generally speaking oriented around a modern API structure, with older APIs with fixed bindings mapped onto it with some fake/virtualised concepts. For this document we largely consider all APIs to have 'descriptors' though on OpenGL and D3D11 these may be fixed binding points.

Generally speaking the principle is as follows:

#. Get the list of accessed descriptors.
#. Get the contents of the referenced descriptors. This can be done in a batch call or individually.
#. For each accessed descriptor look up the shader binding it refers to and correlate to the contents of the descriptor.

There are helper functions available in the common pipeline state abstraction for doing most of this processing for you. If you only need to look at a list of resources and don't need to know their specific binding points, this is the recommended solution. See for example :py:meth:`renderdoc.PipeState.GetReadOnlyResources`, :py:meth:`renderdoc.PipeState.GetSamplers`, etc. These are queried per shader stage. You still look up the matching shader binding to get reflection data but the accessed descriptor gives you a direct reference to the reflection binding.

Accessed Descriptors
--------------------

At a given event, calling :py:meth:`renderdoc.ReplayController.GetDescriptorAccess` returns a list of all descriptors accessed at that event. These descriptors may be statically known to be accessed or they may be dynamically accessed, that information is not exposed.

.. note:: 
    On OpenGL, shader reflection may list items which end up statically unused. These are reflected for informational purposes but the descriptor access will list them as statically unused.

The returned structure :py:class:`renderdoc.DescriptorAccess` gives information about which shader binding accessed which physical descriptor.

The shader binding is identified by a shader stage, a descriptor type, an index, and an array element. The descriptor type can be categorised as sampler, constant block, read-only or read-write resource according to helper function :py:func:`renderdoc.CategoryForDescriptorType` or per-type by :py:func:`renderdoc.IsConstantBlockDescriptor` etc.

For example if a texture "DiffuseTexture" was element ``[2]`` in the shader reflection's :py:data:`renderdoc.ShaderReflection.readOnlyResources` list, the descriptor access would contain something like:

.. highlight:: python
.. code:: python

    access.stage == renderdoc.ShaderStage.Pixel
    access.descriptorType == renderdoc.DescriptorType.Image
    access.index == 2
    access.arrayElement == 0

In this way the access can be correlated to a particular shader binding in the reflection list for display or analysis, or the accesses within a particular binding can be identified.

.. note:: 
    On D3D12, it is possible for descriptor accesses to come directly from shader code with no binding or reflection information declared at all. In this case the :py:data:`renderdoc.DescriptorAccess.index` member will be set to :py:data:`renderdoc.DescriptorAccess.NoShaderBinding`.

The access also identifies the particular descriptor that was accessed. It does this by identifying the resource which contains the descriptor, the byte offset within that resource's descriptor storage, and the byte size of the descriptor. This uniquely identifies a particular descriptor stored within a resource.

For example, if the above diffuse texture accessed a descriptor in a descriptor set on Vulkan, the resulting properties may look like:

.. highlight:: python
.. code:: python

    access.descriptorStore == renderdoc.ResourceId(...) # ResourceId of a descriptor set
    access.byteOffset == ... # byte offset of descriptor
    access.byteSize == ... # byte size of descriptor

Your code should not make assumptions about the resource referred to by these accesses, nor the offset or size. These values should only be processed as opaque references when looking up descriptor contents.

.. note:: 
    For Vulkan and D3D12 where descriptors are physical objects these will commonly identify descriptor sets or descriptor heaps as the resource. However for older APIs and for non-memory-backed descriptors on Vulkan and D3D12 the resource may be a virtual resource created by RenderDoc or may be stored within another object such as a pipeline. Your code does not have to do anything different to query descriptor contents out of these, but you should take care not to make assumptions about the resources referred to in this way.

Descriptor Contents
-------------------

Given the information above, any given descriptor access can be identified as referencing a given 'physical' descriptor within a descriptor storage object. To query the contents of that descriptor you can use the functions  :py:meth:`renderdoc.ReplayController.GetDescriptors` and :py:meth:`renderdoc.ReplayController.GetSamplerDescriptors`.

These functions accept a single descriptor store to query from, and a series of ranges to allow potentially querying many descriptors at once. The returned descriptors are in a linear list corresponding to all ranges appended.

RenderDoc defines two different descriptor structures - :py:class:`renderdoc.Descriptor` for most types of resource binding, and :py:class:`renderdoc.SamplerDescriptor` for samplers. On APIs that provide a combined sampler/image descriptor type the same descriptor can be queried in both fashions to obtain both pieces of information.

Querying a pure sampler descriptor for normal contents, or vice-versa a buffer descriptor for sampler contents, is safe in either direction. The returned descriptor struct will be uninitialised/invalid.

These structures contain a number of properties, only some of which are relevant given different API capabilities and descriptor types. It is expected that the user will look at the type of descriptor listed to determine which properties are relevant. Any API specific information which is not present or available will be initialised to a sensible default value.

Location and binding information
--------------------------------

With the above process you can determine which bindings are used, which descriptors they reference, and the contents of those descriptors. However on most APIs there is additional API-specific binding or location information associated either with a binding or a descriptor which can be helpful to display or filter by.

In the shader reflection, each binding contains two additional values: ``fixedBindSetOrSpace`` and ``fixedBindNumber``. These values are entirely arbitrary and they serve no purpose within RenderDoc's general APIs for accessing descriptors, as their interpretation is API-specific. On some APIs these values may not be set at all. They are provided for informational purposes for uses which may want to look up resources in a way only for their target API.

Similarly, descriptors in a descriptor store may have locations associated. In the same way that you can query descriptor contents with :py:meth:`renderdoc.ReplayController.GetDescriptors` you can query locations with :py:meth:`renderdoc.ReplayController.GetDescriptorLocations` which returns a list of :py:class:`renderdoc.DescriptorLogicalLocation`.

Again this information is API-specific and is not used for any lookups or processing, only for user display or API-specific details.

The logical location contains a ``fixedBindNumber`` value, which depending on the API may match the binding in a shader reflection resource but is not guaranteed to. It also contains a mask of shader stages which can legally access it, the category of shader binding it may contain (if known), and a string which can be used for user display of this particular descriptor.

Updating from old code
----------------------

Old code that accessed bindings via the :py:class:`renderdoc.PipeState` should have minimal changes needed, only updating any references to members from the old ``BoundResource`` and ``BoundResourceArray`` classes to the new list of descriptors, as well as updating handling to process a flat list of descriptors rather than a two-level array.

For porting code that accessed bindings directly from API-specific pipelines it is highly recommended to use the pipeline abstraction to query for used descriptors, and instead do API-specific processing via locations if necessary.

To process bindings entirely from scratch without the abstraction you will need to determine the used descriptors and fetch the descriptor contents then use the locations to do any specific processing as needed.

API-specific information
------------------------

This section provides information about API-specific details and how they are surfaced. This may change in future but generally is expected to be stable.

D3D11
^^^^^

Descriptor access is determined at load time based on shader reflection, all resources are assumed to be used. The shader reflection ``fixedBindNumber`` gives the register number for each resource.

A single fake descriptor storage object is used for all current bindings, with the descriptor offset identifying the binding. All descriptors are identically sized - this size is available in :py:data:`renderdoc.DescriptorStoreDescription.descriptorByteSize` for the descriptor store.

The descriptor location information gives the stage and category based on the binding, and the string name is an encoded ``t0`` or ``b5`` register declaration corresponding to the HLSL declarations.

This means it is possible to iterate over all descriptors in a store without any access, and identify them according to the D3D11 binding spots. However if you do this note that UAVs have a descriptor per stage for ease of access, but as per D3D11's binding model all non-compute stages share the same bindings so these will be duplicated for every stage.

OpenGL
^^^^^^

Descriptor access is determined per-event based on a combination between shader reflection and querying current uniform values. Resources which are unused will be marked with :py:data:`renderdoc.DescriptorAccess.staticallyUnused` being set to ``True``. The shader reflection ``fixedBindNumber`` will be set to 0 as the binding number is not necessarily fixed and could vary per-event.

A single fake descriptor storage object is used for all current bindings, with the descriptor offset identifying the binding. All descriptors are identically sized - this size is available in :py:data:`renderdoc.DescriptorStoreDescription.descriptorByteSize` for the descriptor store.

The descriptor location information gives the stage and category based on the binding, and the string name will be a type and index something akin to ``Tex2D 3`` or ``SSBO 5``.

This means it is possible to iterate over all descriptors in a store without any access, and identify them according to the name given. The descriptor contents will also reflect this as unbound textures will still have the correct texture type when queried for their contents.

D3D12
^^^^^

Descriptor access is combined from non-arrayed access being calculated statically from reflection, and arrayed or direct-heap SM6.6 access being fetched at runtime per event. The shader reflection ``fixedBindNumber`` and ``fixedBindSetOrSpace`` gives the register number and register space for each resource.

SM6.6 direct-heap access will be identified with a descriptor access with :py:data:`renderdoc.DescriptorAccess.index` set to :py:data:`renderdoc.DescriptorAccess.NoShaderBinding`.

Descriptor storage is primarily in descriptor heap objects, however root constants, root descriptors, and static samplers will be stored elsewhere. The exact objects used as 'virtual' storage of these descriptor for querying should not be relied upon. Similarly the descriptor size is RenderDoc-defined and will not necessarily match the descriptor size used in D3D12 during capture.

It is possible to query all descriptors for a descriptor heap without a descriptor access, however care should be taken to ensure that valid descriptor offsets and sizes are used. The parameters for these are available in :py:data:`renderdoc.DescriptorStoreDescription.descriptorByteSize` for the descriptor store.

Descriptor locations have their index listed as the ``fixedBindNumber`` and the string name is the SM6.6 indexed ``ResourceDescriptorHeap[]`` or ``SamplerDescriptorHeap[]``. As descriptors are implicitly untyped and fully visible, there is no type or shader stage information in a descriptor's location.

Vulkan
^^^^^^

Descriptor access is combined from non-arrayed access being calculated statically from reflection, and arrayed access being fetched at runtime per event. The shader reflection ``fixedBindNumber`` and ``fixedBindSetOrSpace`` gives the binding number and set number for each resource.

Descriptor storage is primarily in descriptor set objects, however push constants, specialisation constants, and immutable samplers will be stored elsewhere. The exact objects used as 'virtual' storage of these descriptor for querying should not be relied upon.

It is possible to query all descriptors for a descriptor set without a descriptor access, however care should be taken to ensure that valid descriptor offsets and sizes are used. The parameters for these are available in :py:data:`renderdoc.DescriptorStoreDescription.descriptorByteSize` for the descriptor store.

Descriptor locations have their index listed the binding number within the set, and the string name will be the ``bind[arrayIndex]`` flattened value. The type will only reflect the most recently written descriptor data and may be undefined for unwritten descriptors, and the visible shader mask will be determined by the descriptor set layout visibility flags.
