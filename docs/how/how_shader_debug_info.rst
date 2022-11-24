How do I use shader debug information?
======================================

RenderDoc makes extensive use of shader reflection and debug information to provide a smoother debugging experience. This kind of information gives names to anything in the shader interface such as constants, resource bindings, interpolated inputs and outputs. It includes better type information that in many cases is not available, and it can also even include embedded copies of the shader source which is used for source-level debugging.

On most APIs this debug information adds up to a large amount of information which is not necessary for functionality, and so it can be optionally stripped out. Many shader compilation pipelines will do this automatically, so the information is lost by the time RenderDoc can intercept it at the API level. For this reason there are several ways to save either the unstripped shader blob or just the debug information separately at compile time and provide ways for RenderDoc to correlate the stripped blob it sees passed to the API with the debug information on disk.

If you did not deliberately strip out debug information and left it embedded in the blob, you do not need to do anything else and RenderDoc will find and use the information. If you used a compiler-based method of generating separate debug information you will at most need to specify search paths in the RenderDoc options but you do not need to follow any of these steps.

.. note::

  OpenGL is effectively excluded from this consideration because it has no separate debug information, everything is generated from the source uploaded to the API. If the source has been stripped of information or obfuscated, this will have to be handled by your application.

.. warning::

  Since this debug information is stored separately, it is *not* part of the capture and so if the debug information is moved or deleted RenderDoc will not be able to find it and the capture will show only what is possible with no debug information.

Specifying debug shader blobs
-----------------------------

Separated debug shader blobs can be specified using a path and an API-specific mechanism, if the separation has been done manually. The path itself can either be an absolute path, which will be used directly every time, or it can be a relative path which allows the blob identifier to be specified relative to customisable search folders. If using a relative path, it can be as simple as a filename.

The search folders for shader debug info are specified in the settings window, under the ``Core`` category. You can configure as many search directories as you wish, and they will be searched in the listed order.

If no match is found after all of the directories have been tried, the first subdirectory in the path specified will be removed and the directories will be tried again in order. This way an absolute path can match if there is a trailing subset that exists in one of the configured search paths. Similarly for a relative path which has a non-matching prefix to the path but a trailing subpath that does exist.

Using the D3D11 API you can specify the path at runtime:

.. highlight:: c++
.. code:: c++

    std::string pathName = "path/to/saved/blob.dxbc"; // path name is in UTF-8

    ID3D11VertexShader *shader = ...;

    // GUID value in renderdoc_app.h
    GUID RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

    // string parameter must be NULL-terminated, and in UTF-8
    shader->SetPrivateData(RENDERDOC_ShaderDebugMagicValue,
                           (UINT)pathName.length(), pathName.c_str());

You can also specify it using the Vulkan API:

.. highlight:: c++
.. code:: c++

    std::string pathName = "path/to/saved/blob.dxbc"; // path name is in UTF-8

    VkShaderModule shaderModule = ...;

    // Both EXT_debug_marker and EXT_debug_utils can be used, this example uses
    // EXT_debug_utils as EXT_debug_marker is deprecated
    VkDebugUtilsObjectTagInfoEXT tagInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT};
    tagInfo.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
    tagInfo.objectHandle = (uint64_t)shaderModule;
    // tag value in renderdoc_app.h
    tagInfo.tagName = RENDERDOC_ShaderDebugMagicValue_truncated;
    tagInfo.pTag = pathName.c_str();
    tagInfo.tagSize = pathName.length();

    vkSetDebugUtilsObjectTagEXT(device, &tagInfo);

D3D12 requires using a shader-compile-time specifier. This is done by passing ``/Fd`` to fxc or dxc. This switch requires a parameter which can either be a path to a file or a directory. If you specify a file, that path is then stored in the stripped blob as an absolute path. If you specify a directory the debug blob will be stored in that directory with a hash-based filename. The path stored in the stripped blob is then a *relative* path with just the filename.

See Also
--------

* :doc:`how_debug_shader`
